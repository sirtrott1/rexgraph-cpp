#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/local/positive_rules.hpp"
#include <array>

namespace positive_fixture {
using Cell = rfc::core::CellId;
inline Cell f(unsigned i) { return {{0}, i}; }
inline Cell r(unsigned i) { return {{1}, i}; }
inline std::shared_ptr<const rfc::core::RelationalComplex> source(unsigned extra = 0) {
    rfc::core::RelationSignature sig;
    const auto type = sig.add_type("retained positive rule", rfc::core::BoundaryPolicy::DeclaredExact, 0, 8);
    rfc::core::RelationalComplexBuilder b(sig, {7});
    for (unsigned i = 0; i < 9; ++i)
        b.add_grade_one(rfc::core::RelationKey{10000 + i}, type,
            {{1001,true,true,1},{9009,true,false,1},{77777,true,false,0},
             {1001,true,false,1},{1001,true,true,1},{77777,false,false,0}});
    for (unsigned i = 0; i < extra; ++i)
        b.add_grade_one(rfc::core::RelationKey{20000 + i}, type, {{1000000 + i,true,false,0}});
    return std::make_shared<const rfc::core::RelationalComplex>(b.freeze());
}
inline std::vector<Cell> universe() { return {f(0), f(1), f(2)}; }
inline constexpr std::array<unsigned,9> bodies{1,2,3,4,1,0,1,6,4};
inline constexpr std::array<unsigned,9> heads{2,1,4,1,1,4,2,1,2};
inline std::vector<rfc::local::PositiveRule> rules(unsigned mask) {
    std::vector<rfc::local::PositiveRule> out;
    for (unsigned i = 0; i < 9; ++i) if ((mask >> i) & 1u) {
        std::vector<Cell> ps;
        for (unsigned k = 0; k < 3; ++k) if ((bodies[i] >> k) & 1u) ps.push_back(f(k));
        if (i == 6) ps.push_back(f(0)); // duplicate premise occurrence, not two requirements
        unsigned h = heads[i] == 1 ? 0 : heads[i] == 2 ? 1 : 2;
        out.push_back({r(i), std::move(ps), f(h)});
    }
    return out;
}
inline std::vector<Cell> seeds(unsigned mask) {
    std::vector<Cell> out; for (unsigned i = 0; i < 3; ++i) if ((mask >> i)&1u) out.push_back(f(i)); return out;
}
inline unsigned expected(unsigned mask, unsigned seed) {
    // Independent exhaustive model intersection, not a copy of the worklist.
    unsigned intersection = 7;
    for (unsigned candidate = 0; candidate < 8; ++candidate) {
        if ((candidate & seed) != seed) continue;
        bool valid = true;
        for (unsigned i = 0; i < 9; ++i)
            if (((mask >> i)&1u) && ((candidate & bodies[i]) == bodies[i]) && !(candidate & heads[i])) valid = false;
        if (valid) intersection &= candidate;
    }
    return intersection;
}
}
