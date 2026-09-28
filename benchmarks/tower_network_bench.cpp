#include "rfc/core/builder.hpp"
#include "rfc/learning/tower_network.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

static rfc::core::RelationalComplex make_path(std::size_t edges) {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < edges; ++i) {
        builder.add_grade_one(edge, {{i,true,true,false,1},{i+1u,true,false,false,1}});
    }
    return builder.freeze();
}

int main() {
    using rfc::learning::TowerOperatorChannelKind;
    constexpr std::size_t edges = 10000;
    constexpr std::size_t repeats = 30;
    const auto complex = make_path(edges);
    const std::vector<TowerOperatorChannelKind> candidates{
        TowerOperatorChannelKind::Identity,
        TowerOperatorChannelKind::Lowering,
        TowerOperatorChannelKind::Raising,
        TowerOperatorChannelKind::Dirac
    };
    const auto compile_start = std::chrono::steady_clock::now();
    const auto plan = rfc::learning::compile_integer_tower_layer(complex, candidates, {1,2,3,4});
    const auto compile_end = std::chrono::steady_clock::now();

    std::vector<std::int64_t> vertices(edges + 1u, 1);
    std::vector<std::int64_t> edge_values(edges, 1);
    rfc::field::MachineLatticeTowerField input(complex, {vertices, edge_values});
    std::int64_t checksum_compiled = 0;
    const auto run_start = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < repeats; ++r) {
        const auto out = plan.layer.forward(input);
        checksum_compiled += out.grade_values(rfc::core::Grade{0})[r % (edges + 1u)];
    }
    const auto run_end = std::chrono::steady_clock::now();

    std::int64_t checksum_direct = 0;
    const auto direct_start = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < repeats; ++r) {
        rfc::field::MachineLatticeTowerField out(complex);
        const std::vector<std::int64_t> gains{1,2,3,4};
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            const auto response = rfc::learning::apply_tower_operator_channel(candidates[c], input);
            for (std::size_t k = 0; k < out.grade_count(); ++k) {
                rfc::field::add_scaled_tower_grade(out.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}),
                                                  response.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)}), gains[c]);
            }
        }
        checksum_direct += out.grade_values(rfc::core::Grade{0})[r % (edges + 1u)];
    }
    const auto direct_end = std::chrono::steady_clock::now();

    std::cout << "edges=" << edges << " candidates=" << candidates.size() << " rank=" << plan.basis.rank() << '\n';
    std::cout << "compile_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(compile_end-compile_start).count() << '\n';
    std::cout << "compiled_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(run_end-run_start).count() << '\n';
    std::cout << "redundant_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(direct_end-direct_start).count() << '\n';
    std::cout << "checksum_equal=" << (checksum_compiled == checksum_direct ? 1 : 0) << '\n';
    return checksum_compiled == checksum_direct ? 0 : 1;
}
