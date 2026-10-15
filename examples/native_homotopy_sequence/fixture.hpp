#pragma once
#include "rfc/inference/native_homotopy_sequence.hpp"
#include "../native_homotopy_composition/fixture.hpp"

namespace native_homotopy_sequence_example {
using namespace rfc;
using Plan = inference::PreparedNativeHomotopySequence;
using Cylinder = inference::PreparedNativeCylinder;
using Guard = numeric::ExactActionGuard;
using Q = numeric::Rational;
using Values = Cylinder::Values;
using Endpoint = topology::CylinderEndpoint;
using Availability = inference::NativeHomotopySequenceAvailability;
using StageAvailability = inference::NativeHomotopyStageAvailability;
using native_homotopy_composition_example::grade;
using native_homotopy_composition_example::identity;
using native_homotopy_composition_example::source_map;
using native_homotopy_composition_example::supply;
using native_homotopy_composition_example::native_chain;
using native_homotopy_composition_example::zero;
inline Availability initial() { return {native_relational_homotopy_example::available,{}}; }
inline core::RelationalComplex three_zero_flags() {
    const auto c = native_relational_homotopy_example::source(0); core::RelationalComplexBuilder b(c.signature(),c.semantic_scale()); b.declare_grade(c.max_grade());
    for (const auto key : c.grade(grade(0)).identities) b.declare_witness_identity(key);
    const auto& a = c.grade(grade(1)); std::vector<core::BoundaryOccurrenceInput> flags;
    for (std::size_t p = 0; p < a.incidence_count(); ++p) {
        const auto q = c.incidence_coefficient(grade(1),p);
        flags.emplace_back(c.grade(grade(0)).identities[a.lower_index[p]],a.aspects.get(c.signature().existence_aspect(),p),
            a.aspects.get(c.signature().orientation_aspect(),p),q < 0 ? -q : q,std::vector<core::AspectValue>{},a.occurrence_identities[p]);
    }
    flags.emplace_back(c.grade(grade(0)).identities[a.lower_index[4]],true,false,Q{0},std::vector<core::AspectValue>{},core::OccurrenceKey{999});
    b.add_grade_one(a.relation_identities[0],a.relation_types[0],flags); return b.freeze();
}
inline Plan seed(unsigned kind = 0, std::size_t bound = 6) {
    if (kind == 9) {
        const auto c = native_relational_homotopy_example::cylinder(three_zero_flags());
        return Plan::prepare(inference::PreparedNativeRelationalHomotopy::prepare(c,native_relational_homotopy_example::request(c),initial().root),{1900,1,bound},initial());
    }
    return Plan::prepare(native_homotopy_composition_example::setup(kind).original,{1900,1,bound},initial());
}
struct Append { Cylinder domain; inference::NativeHomotopyCompositionRequest request; Availability availability; };
inline Append next(const Plan& plan, std::size_t ordinal, unsigned permutation = 0) {
    const auto old = plan.domain(); const auto base = std::uint64_t{1300} + 4*ordinal, id = std::uint64_t{100} + 4*ordinal;
    const auto seq = std::uint64_t{41} + 4*ordinal, key = std::uint64_t{2100} + 10*ordinal;
    topology::RelationalInterval interval;
    storage::NativeAccessionDirectory x(storage::TemporalEndpoint(old.factor(false).endpoint()->source,seq),base),
        i(storage::TemporalEndpoint(interval.complex(),seq+1),base+1);
    auto request = native_cylinder_example::request(x,i,true); request.left_factor=id; request.right_factor=id+1;
    request.declaration=native_cylinder_example::declaration(x.pin_snapshot()->source,i.pin_snapshot()->source,900+100*ordinal); request.factor_dimension_limit=128;
    const StageAvailability a{{{base,seq},{base+1,seq+1}},{base+2,seq+2},{base+3,seq+3}};
    auto domain=Cylinder::prepare(request,a.domain); const auto supplied=supply(domain,30000+1000*ordinal);
    const auto root=plan.root();
    const auto target=plan.stage_count() ? plan.stage_declaration(plan.stage_count()-1).target : root.declaration().target;
    const auto target_id=plan.stage_count() ? plan.stage_declaration(plan.stage_count()-1).target_source : root.declaration().target_source;
    storage::NativeAccessionDirectory c(storage::TemporalEndpoint(supplied.source,seq+2),base+2), z(storage::TemporalEndpoint(target.endpoint()->source,seq+3),base+3);
    inference::NativeHomotopyCompositionRequest r{key,1,id+2,id+3,native_cylinder_example::anchor(c),native_cylinder_example::anchor(z),
        {supplied.declaration.key,supplied.declaration.revision,supplied.declaration.types,{},{}},{},{},{}};
    const auto product=domain.tensor_product();
    for (const auto& cell:supplied.declaration.cells) r.realization.cells.push_back({product.cell(cell.from).source_identity,storage::stable_cell_key(supplied.source,{cell.from.grade(),cell.to})});
    for (const auto& flag:supplied.declaration.occurrences) r.realization.occurrences.push_back({product.cell(flag.from.upper).source_identity,flag.from.side,flag.from.occurrence,
        {flag.from.upper.grade(),supplied.source.grade(flag.from.upper.grade()).occurrence_identities[flag.to]}});
    auto before=identity(domain.factor(false).endpoint()->source);
    if(permutation)std::swap(before.occurrences[1][permutation==1 ? 4u:5u],before.occurrences[1][permutation==1 ? 5u:6u]);
    r.before=source_map(domain.factor(false).endpoint()->source,old.factor(false).endpoint()->source,before,key+1,id,old.declaration().left_factor);
    r.interval=source_map(domain.factor(true).endpoint()->source,old.factor(true).endpoint()->source,identity(domain.factor(true).endpoint()->source),key+2,id+1,old.declaration().right_factor);
    r.after=source_map(target.endpoint()->source,z.pin_snapshot()->source,identity(target.endpoint()->source),key+3,target_id,id+3);
    auto availability=plan.preparation_availability(); availability.stages.push_back(a);
    return {std::move(domain),std::move(r),std::move(availability)};
}
inline Plan append(const Plan& plan, std::size_t ordinal, unsigned permutation = 0) {
    auto n=next(plan,ordinal,permutation); return plan.append(std::move(n.domain),std::move(n.request),std::move(n.availability));
}
inline Plan prepare(unsigned kind=0,std::size_t depth=3) {
    auto p=seed(kind); if(!depth)return p;
    if(kind!=9) {
        auto s=native_homotopy_composition_example::setup(kind);
        // Reuse this root's actual owner, rather than an equal independently prepared seed.
        p=Plan::prepare(s.original,{1900,1,6},initial());
        const auto a=native_homotopy_composition_example::available;
        p=p.append(s.domain,s.request,{a.original,{{a.domain,a.carrier,a.target}}});
    } else p=append(p,1,1);
    for(std::size_t j=2;j<=depth;++j)p=append(p,j,kind==9 ? (j%2 ? 1u:2u):0u);
    return p;
}
} // namespace native_homotopy_sequence_example
