#pragma once
#include "rfc/inference/native_homotopy_composition.hpp"
#include "../native_relational_homotopy/fixture.hpp"

namespace native_homotopy_composition_example {
using namespace rfc;
using Q = numeric::Rational;
using Guard = numeric::ExactActionGuard;
using Cylinder = inference::PreparedNativeCylinder;
using Plan = inference::PreparedNativeHomotopyComposition;
using Values = Cylinder::Values;
using Endpoint = topology::CylinderEndpoint;
using native_relational_homotopy_example::grade;
using native_relational_homotopy_example::source;
using native_relational_homotopy_example::identity;
using native_relational_homotopy_example::source_map;
using native_relational_homotopy_example::supply;
using native_relational_homotopy_example::native_chain;
using native_relational_homotopy_example::zero;
inline constexpr inference::NativeHomotopyCompositionAvailability available{
    native_relational_homotopy_example::available, {{1201,23},{1202,29}}, {1203,31}, {1204,37}};
inline Cylinder cylinder(core::RelationalComplex c) {
    topology::RelationalInterval interval;
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(std::move(c),23),1201), b(storage::TemporalEndpoint(interval.complex(),29),1202);
    auto r = native_cylinder_example::request(a,b,true); r.factor_dimension_limit = 128;
    return Cylinder::prepare(std::move(r),available.domain);
}
struct Setup {
    inference::PreparedNativeRelationalHomotopy original;
    Cylinder domain;
    inference::NativeHomotopyCompositionRequest request;
};
inline Setup setup(unsigned kind = 0) {
    const unsigned original_kind = kind == 6 ? 5u : kind == 7 ? 3u : kind == 8 ? 0u : kind;
    auto original = native_relational_homotopy_example::prepare(original_kind);
    auto domain = cylinder(source(kind == 6 ? 3u : original_kind)); const auto supplied = supply(domain,15000);
    const auto target_cylinder = kind == 7 ? native_relational_homotopy_example::cylinder(source(5)) : original.cylinder();
    const auto supplied_target = supply(target_cylinder,19000); Guard guard;
    const auto target_carrier = topology::OwnedCylinderCarrier::prepare(target_cylinder.numeric_plan(), supplied_target.source, supplied_target.declaration, guard);
    storage::NativeAccessionDirectory ca(storage::TemporalEndpoint(supplied.source,31),1203), za(storage::TemporalEndpoint(supplied_target.source,37),1204);
    inference::NativeHomotopyCompositionRequest r{1700,6,83,84,native_cylinder_example::anchor(ca),native_cylinder_example::anchor(za),
        {supplied.declaration.key,supplied.declaration.revision,supplied.declaration.types,{},{}},{},{},{}};
    const auto product = domain.tensor_product();
    for (const auto& x : supplied.declaration.cells) r.realization.cells.push_back({product.cell(x.from).source_identity,storage::stable_cell_key(supplied.source,{x.from.grade(),x.to})});
    for (const auto& x : supplied.declaration.occurrences) r.realization.occurrences.push_back({product.cell(x.from.upper).source_identity,x.from.side,x.from.occurrence,
        {x.from.upper.grade(),supplied.source.grade(x.from.upper.grade()).occurrence_identities[x.to]}});
    auto pre = identity(domain.factor(false).endpoint()->source);
    if (kind == 6) for (auto& i : pre.cells[1]) i = 0;
    if (kind == 8) std::swap(pre.occurrences[1][4],pre.occurrences[1][5]);
    r.before = source_map(domain.factor(false).endpoint()->source,original.cylinder().factor(false).endpoint()->source,pre,1701,81,71);
    r.interval = source_map(domain.factor(true).endpoint()->source,original.cylinder().factor(true).endpoint()->source,identity(domain.factor(true).endpoint()->source),1702,82,72);
    auto post = identity(original.declaration().target.endpoint()->source);
    // Original Y and supplied Z use different stable keys. These explicit product
    // receipts determine the whole post-map, including coalesced empty assertions.
    const auto old_target = supply(original.cylinder(),9000);
    for (const auto& x : old_target.declaration.cells) {
        auto id = x.from; if (kind == 7 && id.left.grade.value == 1) id.left.index = 0;
        post.cells[id.grade().value][x.to] = target_carrier.image(id);
    }
    for (const auto& x : old_target.declaration.occurrences) {
        auto id = x.from; if (kind == 7 && id.upper.left.grade.value == 1) id.upper.left.index = 0;
        post.occurrences[id.upper.grade().value][x.to] = target_carrier.image(id);
    }
    r.after = source_map(original.declaration().target.endpoint()->source,supplied_target.source,post,1703,74,84);
    return {std::move(original),std::move(domain),std::move(r)};
}
inline Plan prepare(unsigned kind = 0) {
    auto s = setup(kind); return Plan::prepare(std::move(s.original),std::move(s.domain),std::move(s.request),available);
}
} // namespace native_homotopy_composition_example
