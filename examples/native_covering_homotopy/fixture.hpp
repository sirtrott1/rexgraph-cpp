#pragma once
#include "rfc/inference/native_covering_homotopy.hpp"
#include "../native_relational_homotopy/fixture.hpp"

namespace native_covering_homotopy_example {
using namespace rfc;
using native_relational_homotopy_example::Q;
using native_relational_homotopy_example::Guard;
using native_relational_homotopy_example::Values;
using native_relational_homotopy_example::grade;
using native_relational_homotopy_example::source_map;
using native_relational_homotopy_example::identity;
using native_relational_homotopy_example::native_chain;
using native_relational_homotopy_example::zero;
using Endpoint = topology::CylinderEndpoint;
using Plan = inference::PreparedNativeCoveringHomotopy;
using Availability = inference::NativeCoveringHomotopyAvailability;
using Map = algebra::RetainedSourceMap;
struct CoverSupply { core::RelationalComplex source; Map projection; std::vector<Map> sheets; };
// A finite fixture constructor, not a runtime identity or signature service.
// A declared zero/inactive twist can obstruct the full structural cylinder lift.
inline CoverSupply copies(const core::RelationalComplex& c, std::size_t sheets = 2,
    std::optional<std::pair<core::Grade, std::size_t>> twist = {}) {
    core::RelationalComplexBuilder b(c.signature(), c.semantic_scale()); b.declare_grade(c.max_grade());
    Map projection; projection.cells.resize(c.grade_count()); projection.occurrences.resize(c.grade_count());
    std::vector<Map> inclusions(sheets);
    for (auto& m : inclusions) { m.cells.resize(c.grade_count()); m.occurrences.resize(c.grade_count()); }
    for (std::size_t k = 0; k < c.grade_count(); ++k) {
        const auto g = topology::owned_cylinder_detail::grade(k); const auto& a = c.grade(g);
        for (std::size_t sheet = 0; sheet < sheets; ++sheet) for (std::size_t i = 0; i < a.cell_count(); ++i) {
            const auto key = std::uint64_t{50000} + sheet * 10000 + k * 1000 + i;
            std::uint32_t index{};
            if (!k) index = b.declare_witness_identity(key);
            else {
                std::vector<core::BoundaryOccurrenceInput> low; std::vector<core::HigherIncidenceInput> high;
                const auto [first, last] = a.incidence_range(i);
                for (auto p = first; p < last; ++p) {
                    const auto lower_sheet = twist && twist->first == g && twist->second == p ? (sheet + 1) % sheets : sheet;
                    const auto lower = inclusions[lower_sheet].cells[k-1].at(a.lower_index[p]);
                    const auto q = c.incidence_coefficient(g, p); const auto magnitude = q < 0 ? -q : q;
                    std::vector<core::AspectValue> aspects;
                    for (std::size_t j = 0; j < c.signature().aspects().size(); ++j)
                        if (j != c.signature().existence_aspect() && j != c.signature().orientation_aspect())
                            aspects.push_back({static_cast<composite::AspectId>(j), a.aspects.get(static_cast<composite::AspectId>(j), p)});
                    const auto occurrence = core::OccurrenceKey{600000 + sheet * 10000 + k * 1000 + p};
                    const bool exists = a.aspects.get(c.signature().existence_aspect(), p), negative = a.aspects.get(c.signature().orientation_aspect(), p);
                    if (k == 1) low.emplace_back(50000 + lower_sheet * 10000 + a.lower_index[p], exists, negative, magnitude, aspects, occurrence);
                    else high.emplace_back(lower, exists, negative, magnitude, aspects, occurrence);
                    inclusions[sheet].occurrences[k].push_back(projection.occurrences[k].size()); projection.occurrences[k].push_back(p);
                }
                index = (k == 1 ? b.add_grade_one(core::RelationKey{key}, a.relation_types[i], low)
                                : b.add_higher(g, core::RelationKey{key}, a.relation_types[i], high)).index;
            }
            inclusions[sheet].cells[k].push_back(index); projection.cells[k].push_back(static_cast<std::uint32_t>(i));
        }
    }
    return {b.freeze(), std::move(projection), std::move(inclusions)};
}
inline core::RelationalComplex cycle(std::size_t n, std::uint64_t seed) {
    core::RelationalComplexBuilder b(native_relational_homotopy_example::signature());
    for (std::size_t i = 0; i < n; ++i) b.declare_witness_identity(seed + i);
    for (std::size_t i = 0; i < n; ++i) b.add_grade_one(core::RelationKey{seed + 100 + i}, 0,
        {{seed + i, true, true, Q{1}, {}, core::OccurrenceKey{seed + 1000 + 2*i}},
         {seed + (i+1)%n, true, false, Q{1}, {}, core::OccurrenceKey{seed + 1001 + 2*i}}});
    return b.freeze();
}
inline inference::PreparedNativeRelationalHomotopy homotopy(unsigned kind) {
    if (kind < 6) return native_relational_homotopy_example::prepare(kind);
    core::RelationalComplexBuilder b(native_relational_homotopy_example::signature()); b.declare_witness_identity(10);
    const auto cylinder = native_relational_homotopy_example::cylinder(b.freeze());
    auto request = native_relational_homotopy_example::request(cylinder); const auto target = cycle(3,9000);
    storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(target,19),1104);
    request.target = native_cylinder_example::anchor(directory);
    Map h{{{2,0},{2}},{{},{4,5}}}, first{{{2}},{{}}}, second{{{0}},{{}}};
    request.cylinder_target = source_map(request.carrier.endpoint()->source, target,h,1601,73,74);
    request.first = source_map(cylinder.factor(false).endpoint()->source,target,first,1602,71,74);
    request.second = source_map(cylinder.factor(false).endpoint()->source,target,second,1603,71,74);
    return inference::PreparedNativeRelationalHomotopy::prepare(cylinder,request,native_relational_homotopy_example::available);
}
struct Fixture {
    inference::PreparedNativeRelationalHomotopy original;
    inference::PreparedNativeCovering cover;
    inference::NativeCoveringHomotopyRequest request;
    Availability available;
};
inline Fixture fixture(unsigned kind = 0, std::size_t sheet = 0,
    std::optional<std::pair<core::Grade,std::size_t>> twist = {}, std::size_t sheet_count = 2) {
    const auto original = homotopy(kind == 7 || kind == 8 ? 3u : kind == 9 ? 0u : kind); const auto numeric = original.numeric_plan();
    auto base = numeric.target(); auto bridge = identity(numeric.target());
    Guard guard;
    if (kind == 7) {
        const auto target = homotopy(5); const auto target_numeric = target.numeric_plan(); const auto into = target_numeric.carrier();
        base = target_numeric.target(); const auto from = numeric.carrier();
        const auto image = [](algebra::ProductCell id) { if (id.left.grade.value) id.left.index = 0; return id; };
        for (const auto& cell : from.declaration().cells) bridge.cells[cell.from.grade().value][cell.to] = into.image(image(cell.from));
        for (const auto& flag : from.declaration().occurrences)
            bridge.occurrences[flag.from.upper.grade().value][flag.to] = into.image(topology::CylinderFlag{image(flag.from.upper),flag.from.side,flag.from.occurrence});
    }
    if (kind == 9) {
        const auto carrier = numeric.carrier();
        for (const auto& flag : carrier.declaration().occurrences) if (flag.from.side == algebra::ProductBoundarySide::Left &&
            (flag.from.occurrence.value == 105 || flag.from.occurrence.value == 106)) {
            auto other = flag.from; other.occurrence = core::OccurrenceKey{flag.from.occurrence.value == 105 ? 106u : 105u};
            bridge.occurrences[flag.from.upper.grade().value][flag.to] = carrier.image(other);
        }
    }
    auto supplied = copies(base,sheet_count,twist);
    auto first = topology::covering_homotopy_detail::compose(numeric.declaration().first,bridge,guard);
    Map initial = topology::covering_homotopy_detail::compose(first,supplied.sheets.at(sheet),guard);
    if (kind == 8) initial.cells[1][1] = supplied.sheets[(sheet+1)%sheet_count].cells[1][first.cells[1][1]];
    if (kind == 6) {
        supplied.source = cycle(6,50000); supplied.projection = {{{0,1,2,0,1,2},{0,1,2,0,1,2}},{{},{0,1,2,3,4,5,0,1,2,3,4,5}}};
        initial = {{{static_cast<std::uint32_t>(2 + 3*sheet)}},{{}}};
    }
    storage::NativeAccessionDirectory ca(storage::TemporalEndpoint(supplied.source,23),1201),ba(storage::TemporalEndpoint(base,29),1202);
    inference::NativeCoveringRequest cr{native_cylinder_example::anchor(ca),native_cylinder_example::anchor(ba),1700,2,
        source_map(supplied.source,base,supplied.projection,1701,81,82),128};
    const inference::NativeCoveringAvailability cv{{1201,23},{1202,29}};
    const auto cover = inference::PreparedNativeCovering::prepare(cr,cv);
    inference::NativeCoveringHomotopyRequest request{1800,1,
        source_map(numeric.target(),base,bridge,1801,74,82),
        source_map(original.cylinder().factor(false).endpoint()->source,supplied.source,initial,1802,71,81)};
    return {original,cover,std::move(request),{original.preparation_availability(),cv}};
}
inline Plan prepare(unsigned kind = 0, std::size_t sheet = 0) {
    const auto f = fixture(kind,sheet); return Plan::prepare(f.original,f.cover,f.request,f.available);
}
} // namespace native_covering_homotopy_example
