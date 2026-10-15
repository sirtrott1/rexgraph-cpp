#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_family_gluing.hpp"
#include <numeric>

namespace rfc_example::transport_family {
using Fit=rfc::learning::PreparedGradedTransportFit;
using Guard=rfc::numeric::ExactActionGuard;
using Matrix=rfc::numeric::ExactMatrix;

inline rfc::core::RelationalComplex triangle() {
    using namespace rfc::core;
    RelationSignature signature;
    const auto type=signature.add_type("declared boundary",BoundaryPolicy::DeclaredExact,0,3);
    RelationalComplexBuilder builder(signature);
    builder.add_grade_one(RelationKey{100},type,{{10,true,true,1},{20,true,false,1}});
    builder.add_grade_one(RelationKey{200},type,{{20,true,true,1},{30,true,false,1}});
    builder.add_grade_one(RelationKey{300},type,{{10,true,true,1},{30,true,false,1}});
    builder.add_higher(Grade{2},RelationKey{400},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    return builder.freeze();
}

// The selected grade may be one or two. All other maps are fixed, and every
// variable is attached to an actual occurrence in this declared source.
inline Fit family(std::size_t selected,Guard& guard) {
    using namespace rfc;
    const auto source=triangle();
    local::LinearStalkBundle stalks(source,1);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> variables;
    for(std::size_t k=1;k<source.grade_count();++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        for(std::size_t p=0;p<source.grade(grade).incidence_count();++p) {
            Matrix map(1,1);
            if(k==selected)variables.push_back({p,0,0});
            else map.set(0,0,source.incidence_coefficient(grade,p));
            fibers.set_transport(grade,p,std::move(map));
        }
    }
    auto observations=learning::PreparedTransportObservations::at_grade(
        local::OwnedLinearFiberSystem(fibers),core::Grade{static_cast<std::uint16_t>(selected)},
        {},variables,guard);
    return Fit(std::move(observations),{},guard);
}

// This fixture explicitly identifies the same retained triangle in both
// members. Other sources must supply their actual cell/occurrence maps and
// stalk views; the gluing API does not infer them from matrix coordinates.
inline rfc::learning::TransportFamilyInterface triangle_interface(
        std::size_t left,std::size_t right,std::uint64_t key=1) {
    using namespace rfc;
    learning::TransportFamilyInterface out{key,1,left,right,triangle(),{},{},{},{}};
    const auto count=out.common.grade_count();
    out.common_left.cells.resize(count);out.common_left.occurrences.resize(count);
    out.common_right.cells.resize(count);out.common_right.occurrences.resize(count);
    out.left_views.resize(count);out.right_views.resize(count);
    for(std::size_t k=0;k<count;++k) {
        const auto& arena=out.common.grade(core::Grade{static_cast<std::uint16_t>(k)});
        auto& cells=out.common_left.cells[k];cells.resize(arena.cell_count());
        auto& flags=out.common_left.occurrences[k];flags.resize(arena.incidence_count());
        std::iota(cells.begin(),cells.end(),0);std::iota(flags.begin(),flags.end(),0);
        out.common_right.cells[k]=cells;out.common_right.occurrences[k]=flags;
        for(std::size_t i=0;i<arena.cell_count();++i) {
            const learning::TransportContextView view{1+100*k+i,Matrix::identity(1),Matrix::identity(1)};
            out.left_views[k].push_back(view);out.right_views[k].push_back(view);
        }
    }
    return out;
}
} // namespace rfc_example::transport_family
