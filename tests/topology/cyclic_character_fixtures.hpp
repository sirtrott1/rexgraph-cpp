#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/topology/cyclic_characters.hpp"

namespace cyclic_character_test {
using namespace rfc;
using Q = numeric::Rational;
using Guard = numeric::ExactActionGuard;
using Census = topology::PreparedCyclicCharacterDecomposition;
using Basis = topology::EquivariantCoefficientBasis;
struct Declaration {
    core::RelationalComplex source;
    std::vector<std::vector<std::uint64_t>> voltages;
};
inline topology::PreparedCyclicEquivariantChain prepare(Declaration input, std::uint64_t n, Guard& guard,
                                                       Basis basis = Basis::NativeRational) {
    return topology::PreparedCyclicEquivariantChain::prepare(std::move(input.source), n,
        std::move(input.voltages), basis, 101, 1, guard);
}
inline Declaration cycle(std::uint64_t n) {
    core::RelationSignature sig;
    const auto edge = sig.add_type("cycle-edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge, {{0,true,true,1},{1,true,false,1}});
    b.add_grade_one(edge, {{1,true,true,1},{2,true,false,1}});
    b.add_grade_one(edge, {{2,true,true,1},{0,true,false,1}});
    return {b.freeze(), {{},{0,0,0,0,0,1%n}}};
}
// Actual ordered attaching walks, with transport accumulated occurrence by
// occurrence (including a backwards edge's inverse transport).
inline Declaration polygon(const std::vector<std::uint64_t>& nu,
                           const std::vector<std::pair<std::uint32_t,int>>& walk, std::uint64_t n) {
    core::RelationSignature sig;
    const auto edge = sig.add_type("loop", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = sig.add_attaching_loop_type("polygon", walk.size(), walk.size());
    core::RelationalComplexBuilder b(sig);
    std::vector<std::vector<std::uint64_t>> voltages(3);
    for(const auto v : nu) {
        b.add_grade_one(edge, {{0,true,true,1},{0,true,false,1}});
        voltages[1].insert(voltages[1].end(), {0,v});
    }
    std::uint64_t acc = 0;
    std::vector<core::HigherIncidenceInput> occurrences;
    for(const auto& [e, sign] : walk) {
        if(sign < 0) acc = (acc+n-nu.at(e))%n;
        occurrences.emplace_back(e, true, sign < 0, Q{1});
        voltages[2].push_back(acc);
        if(sign > 0) acc = (acc+nu.at(e))%n;
    }
    if(acc) throw std::invalid_argument("test polygon voltage is not flat");
    b.add_higher(core::Grade{2}, face, occurrences);
    return {b.freeze(), std::move(voltages)};
}
inline Declaration lens(std::uint32_t n, std::size_t components = 1, bool top = true, bool decorated = false) {
    core::RelationSignature sig;
    const auto edge = sig.add_type("loop", core::BoundaryPolicy::DeclaredExact, 2, 6);
    const auto face = sig.add_attaching_loop_type("disk", n, n);
    const auto solid = sig.add_type("solid", core::BoundaryPolicy::DeclaredExact, 2, 3);
    core::RelationalComplexBuilder b(sig);
    std::vector<std::vector<std::uint64_t>> voltages(top ? 4 : 3);
    for(std::size_t c = 0; c < components; ++c) {
        const auto v = static_cast<std::uint32_t>(c);
        std::vector<core::BoundaryOccurrenceInput> occ{{v,true,true,1},{v,true,false,1}};
        voltages[1].insert(voltages[1].end(), {0,1});
        if(decorated) {
            occ.insert(occ.end(), {{v,true,false,2},{v,true,true,2},{v,true,false,0},{v,false,false,17}});
            voltages[1].insert(voltages[1].end(), {n-1,n-1,n-1,n-1});
        }
        b.add_grade_one(edge, occ);
    }
    for(std::size_t c = 0; c < components; ++c) {
        std::vector<core::HigherIncidenceInput> occ;
        for(std::uint32_t j = 0; j < n; ++j) {
            occ.emplace_back(static_cast<std::uint32_t>(c), true, false, Q{1});
            voltages[2].push_back(j);
        }
        b.add_higher(core::Grade{2}, face, occ);
    }
    if(top) for(std::size_t c = 0; c < components; ++c) {
        b.add_higher(core::Grade{3}, solid, {{static_cast<std::uint32_t>(c),true,true,1},
                                         {static_cast<std::uint32_t>(c),true,false,1}});
        voltages[3].insert(voltages[3].end(), {0,1});
    }
    return {b.freeze(), std::move(voltages)};
}
inline Declaration fractional() {
    core::RelationSignature sig;
    const auto edge = sig.add_type("fractional-loop", core::BoundaryPolicy::DeclaredExact, 3, 3);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge, {{0,true,true,1},{0,true,false,Q{1,3}},{0,true,false,Q{2,3}}});
    b.declare_grade(core::Grade{4});
    return {b.freeze(), {{},{0,1,2},{},{},{}}};
}
inline Declaration empty() {
    core::RelationalComplexBuilder b(core::RelationSignature{});
    b.declare_grade(core::Grade{3});
    return {b.freeze(), {{},{},{},{}}};
}
} // namespace cyclic_character_test
