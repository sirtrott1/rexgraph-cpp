#include "rfc/geometry/chain_map.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/learning/semantic_network.hpp"
#include "rfc/learning/tower_network.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include <iostream>
#include <vector>

static rfc::core::RelationalComplex make_triangle() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    builder.add_grade_one(edge, {{0,true,true,false,1},{2,true,false,false,1}});
    builder.add_higher(rfc::core::Grade{2}, face, {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return builder.freeze();
}

int main() {
    using rfc::learning::TowerOperatorChannelKind;
    const auto complex = make_triangle();
    const std::vector<TowerOperatorChannelKind> candidates{
        TowerOperatorChannelKind::Identity,
        TowerOperatorChannelKind::Lowering,
        TowerOperatorChannelKind::Raising,
        TowerOperatorChannelKind::Dirac
    };
    const auto plan = rfc::learning::compile_integer_tower_layer(complex, candidates, {2,3,-1,4});
    std::cout << "tower_candidates=" << candidates.size() << " independent=" << plan.basis.rank() << '\n';
    for (std::size_t j = 0; j < plan.basis.candidates.size(); ++j) {
        std::cout << rfc::learning::tower_operator_channel_name(plan.basis.candidates[j]) << " =";
        for (std::size_t i = 0; i < plan.basis.rank(); ++i) {
            std::cout << ' ' << plan.basis.coordinates[j][i].get_str()
                      << '*' << rfc::learning::tower_operator_channel_name(plan.basis.basis_channels()[i]);
        }
        std::cout << '\n';
    }
    rfc::field::MachineLatticeTowerField tower(complex, {{1,2,3},{2,-1,3},{4}});
    const auto out = plan.layer.forward(tower);
    for (std::size_t k = 0; k < out.grade_count(); ++k) {
        std::cout << "grade" << k << ':';
        for (auto value : out.grade_values(rfc::core::Grade{static_cast<std::uint16_t>(k)})) std::cout << ' ' << value;
        std::cout << '\n';
    }
    return 0;
}
