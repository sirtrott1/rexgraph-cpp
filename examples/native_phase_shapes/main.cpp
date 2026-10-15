#include "rfc/algebra/polygon.hpp"
#include "rfc/geometry/rational_rotation.hpp"
#include "rfc/topology/cyclic_attachment.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <iostream>

int main() {
    using namespace rfc;
    using Q = numeric::Rational;
    const auto phase = numeric::RationalPhase::cayley(Q{1,2});
    std::cout << "{\"phase\":[\"" << phase.real() << "\",\"" << phase.imaginary()
              << "\"],\"finite_phase_order\":" << numeric::CyclicPhase(7,1).element_order() << "}\n";
    for (const auto filling : {algebra::PolygonFilling::BoundaryOnly,algebra::PolygonFilling::Disk,
                              algebra::PolygonFilling::DoubledDiskSphere}) {
        const auto source = algebra::oriented_polygon({101,203,307,409},filling);
        std::cout << "{\"filling\":" << static_cast<unsigned>(filling) << ",\"betti\":[";
        for (std::uint16_t grade = 0; grade < 3; ++grade) {
            if (grade != 0) std::cout << ',';
            std::cout << topology::integral_homology(source,core::Grade{grade}).free_rank;
        }
        std::cout << "]}\n";
    }
    core::RelationSignature signature;
    const auto loop = signature.add_type("meridian",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face = signature.add_attaching_loop_type("trefoil_relator",6,6);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(loop,{{99,true,true,false,1},{99,true,false,false,1}});
    builder.add_grade_one(loop,{{99,true,true,false,1},{99,true,false,false,1}});
    builder.add_higher(core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},
        {0,true,false,false,1},{1,true,true,false,1},{0,true,true,false,1},{1,true,true,false,1}});
    const topology::PreparedCyclicAttachingRealization prepared(builder.freeze());
    const auto generic = prepared.specialize_common_transport(numeric::ExactMatrix::from_rows({{2}}));
    const auto exceptional = prepared.specialize_common_transport(numeric::ExactMatrix::from_rows({{0,-1},{1,1}}));
    std::cout << "{\"trefoil\":{\"formal_flat\":" << (prepared.formal_flat() ? "true" : "false")
              << ",\"generic_h1\":" << generic.first_homology_dimension()
              << ",\"exceptional_rational_h1\":" << exceptional.first_homology_dimension() << "}}\n";
}
