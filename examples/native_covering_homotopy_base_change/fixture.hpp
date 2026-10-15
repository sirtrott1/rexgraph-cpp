#pragma once
#include "rfc/inference/native_covering_homotopy_base_change.hpp"
#include "../native_covering_homotopy/fixture.hpp"

namespace native_covering_base_change_example {
using namespace native_covering_homotopy_example;
using BaseChange = inference::PreparedNativeCoveringHomotopyBaseChange;
using BaseAvailability = inference::NativeCoveringHomotopyBaseChangeAvailability;
struct PullbackSupply {
    core::RelationalComplex source;
    Map into, projection;
    std::vector<std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t>> pairs;
};
// Fixture materialization of the complete compatible-pair carrier, including
// original occurrence pairs. Production requires this carrier to be supplied.
inline PullbackSupply pairs(const core::RelationalComplex& e, const core::RelationalComplex& d, const Map& p, const Map& bmap) {
    core::RelationalComplexBuilder builder(e.signature(), e.semantic_scale()); builder.declare_grade(e.max_grade());
    Map into, q; into.cells.resize(e.grade_count()); into.occurrences.resize(e.grade_count()); q = into;
    std::vector<std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t>> cells(e.grade_count());
    for (std::size_t k = 0; k < e.grade_count(); ++k) {
        const auto g = topology::owned_cylinder_detail::grade(k); const auto& x = e.grade(g); const auto& y = d.grade(g);
        for (std::uint32_t i = 0; i < x.cell_count(); ++i) for (std::uint32_t j = 0; j < y.cell_count(); ++j) {
            if (p.cells[k][i] != bmap.cells[k][j]) continue;
            const auto index = static_cast<std::uint32_t>(into.cells[k].size()); const auto key = std::uint64_t{900000} + k*10000 + index;
            if (!k) builder.declare_witness_identity(key);
            else {
                std::vector<core::BoundaryOccurrenceInput> low; std::vector<core::HigherIncidenceInput> high;
                const auto [first, last] = x.incidence_range(i); const auto [a, z] = y.incidence_range(j);
                for (auto f = first; f < last; ++f) for (auto h = a; h < z; ++h) {
                    if (p.occurrences[k][f] != bmap.occurrences[k][h]) continue;
                    const auto lower = cells[k-1].at({x.lower_index[f], y.lower_index[h]});
                    const auto coefficient = e.incidence_coefficient(g, f); const auto magnitude = coefficient < 0 ? -coefficient : coefficient;
                    std::vector<core::AspectValue> aspects;
                    for (std::size_t r = 0; r < e.signature().aspects().size(); ++r)
                        if (r != e.signature().existence_aspect() && r != e.signature().orientation_aspect())
                            aspects.push_back({static_cast<composite::AspectId>(r), x.aspects.get(static_cast<composite::AspectId>(r), f)});
                    const bool exists = x.aspects.get(e.signature().existence_aspect(), f), negative = x.aspects.get(e.signature().orientation_aspect(), f);
                    const core::OccurrenceKey occurrence{1900000 + k*10000 + into.occurrences[k].size()};
                    if (k == 1) low.emplace_back(900000 + lower, exists, negative, magnitude, aspects, occurrence);
                    else high.emplace_back(lower, exists, negative, magnitude, aspects, occurrence);
                    into.occurrences[k].push_back(f); q.occurrences[k].push_back(h);
                }
                if (k == 1) builder.add_grade_one(core::RelationKey{key}, x.relation_types[i], low);
                else builder.add_higher(g, core::RelationKey{key}, x.relation_types[i], high);
            }
            cells[k].emplace(std::pair{i, j}, index); into.cells[k].push_back(i); q.cells[k].push_back(j);
        }
    }
    return {builder.freeze(), std::move(into), std::move(q), std::move(cells)};
}
struct Fixture {
    Plan original, changed;
    inference::PreparedNativeCoveringPullback pullback;
    inference::NativeCoveringHomotopyBaseChangeRequest request;
    BaseAvailability available;
};
inline Fixture fixture(unsigned kind = 0, std::size_t sheet = 0, unsigned mode = 0, bool wrong_seed = false) {
    const auto f = native_covering_homotopy_example::fixture(kind, sheet);
    const auto original = Plan::prepare(f.original, f.cover, f.request, f.available); const auto lift = original.numeric_plan();
    const auto cover = f.cover.numeric_plan(); const auto base = cover.source(local::CoveringChannel::Base);
    auto supply = copies(base, mode ? 2u : 1u); const auto d = supply.source; const auto bmap = supply.projection;
    Guard guard;
    auto comparison = topology::covering_homotopy_detail::compose(lift.declaration().base_map, supply.sheets[mode ? 1 : 0], guard);
    const auto h = f.original.numeric_plan(); auto z = d; auto gmap = identity(d);
    if (mode == 2) { z = h.target(); gmap = comparison; comparison = identity(z); }
    auto ps = pairs(cover.source(), d, cover.projection(), bmap);
    storage::NativeAccessionDirectory pd(storage::TemporalEndpoint(ps.source,31),1301), dd(storage::TemporalEndpoint(d,37),1302);
    const auto ppin = native_cylinder_example::anchor(pd), dpin = native_cylinder_example::anchor(dd);
    const auto pulled = inference::PreparedNativeCovering::prepare({ppin, dpin,1900,1,
        source_map(ps.source,d,ps.projection,1901,91,92),128}, {{1301,31},{1302,37}});
    const auto square = inference::PreparedNativeCoveringPullback::prepare(f.cover,pulled,{1950,1,
        source_map(ps.source,cover.source(),ps.into,1951,91,81),source_map(d,base,bmap,1952,92,82)},
        {f.available.covering,pulled.preparation_availability()});
    auto hr = f.original.declaration(); hr.key = 2000; hr.revision = 1; hr.target_source = mode == 2 ? 94 : 92;
    if (mode == 2) {
        storage::NativeAccessionDirectory zd(storage::TemporalEndpoint(z,41),1303); hr.target = native_cylinder_example::anchor(zd);
    } else hr.target = dpin;
    hr.cylinder_target = source_map(hr.carrier.endpoint()->source,z,topology::covering_homotopy_detail::compose(h.declaration().cylinder_target,comparison,guard),2001,73,hr.target_source);
    hr.first = source_map(f.original.cylinder().factor(false).endpoint()->source,z,topology::covering_homotopy_detail::compose(h.declaration().first,comparison,guard),2002,71,hr.target_source);
    hr.second = source_map(f.original.cylinder().factor(false).endpoint()->source,z,topology::covering_homotopy_detail::compose(h.declaration().second,comparison,guard),2003,71,hr.target_source);
    auto ha = f.original.preparation_availability(); ha.target = {hr.target.source_namespace(),hr.target.sequence()};
    const auto changed_h = inference::PreparedNativeRelationalHomotopy::prepare(f.original.cylinder(),hr,ha);
    const auto ln = lift.numeric_plan(); auto initial = ln.declaration().first;
    const auto changed_numeric = changed_h.numeric_plan(); const auto df = topology::covering_homotopy_detail::compose(changed_numeric.declaration().first,gmap,guard);
    for (std::size_t k = 0; k < initial.cells.size(); ++k) for (std::size_t i = 0; i < initial.cells[k].size(); ++i) {
        auto image = initial.cells[k][i];
        if (wrong_seed && k == 0) image = static_cast<std::uint32_t>((image + cover.source().grade(grade(0)).cell_count()/2) % cover.source().grade(grade(0)).cell_count());
        initial.cells[k][i] = ps.pairs[k].at({image,df.cells[k][i]});
    }
    // Unique flag pairs determined by both full cone maps.
    for (std::size_t k = 0; k < initial.occurrences.size(); ++k) for (std::size_t i = 0; i < initial.occurrences[k].size(); ++i) {
        const auto eflag = initial.occurrences[k][i]; std::size_t slot{};
        while (ps.into.occurrences[k].at(slot) != eflag || ps.projection.occurrences[k][slot] != df.occurrences[k][i]) ++slot;
        initial.occurrences[k][i] = slot;
    }
    const Availability ca{ha,pulled.preparation_availability()};
    const auto changed = Plan::prepare(changed_h,pulled,{2100,1,
        source_map(z,d,gmap,2101,hr.target_source,92),source_map(f.original.cylinder().factor(false).endpoint()->source,ps.source,initial,2102,71,91)},ca);
    inference::NativeCoveringHomotopyBaseChangeRequest request{2200,1,source_map(h.target(),z,comparison,2201,74,hr.target_source),{2202,2203,2204,2205,2206,2207}};
    return {original,changed,square,std::move(request),{f.available,ca,square.preparation_availability()}};
}
inline BaseChange prepare(unsigned kind = 0, std::size_t sheet = 0, unsigned mode = 0) {
    const auto f = fixture(kind,sheet,mode); return BaseChange::prepare(f.original,f.changed,f.pullback,f.request,f.available);
}
} // namespace native_covering_base_change_example
