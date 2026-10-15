#pragma once
#include "rfc/storage/algebraic_archive_graph.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"
#include "rfc/algebra/morphism.hpp"

namespace rfc::storage {
enum class AlgebraicGraphSourceKind : std::uint8_t { Independent, AnchorMorphism };
struct AlgebraicGraphSourceBinding {
    std::uint64_t bundle;
    AlgebraicGraphSourceKind kind{AlgebraicGraphSourceKind::Independent};
    std::vector<std::vector<std::uint32_t>> cells;
    std::vector<std::vector<std::size_t>> occurrences;
};
struct AlgebraicGraphArchiveBinding { std::uint64_t archive,bundle; };
struct AlgebraicGraphSnapshotLimits {
    AlgebraicArchiveGraphLimits graph{};
    std::size_t max_file_bytes{64u<<20},max_source_bindings{1024},max_archive_bindings{1024};
    std::size_t max_source_coordinates{1000000},max_history_revisions{1024};
};

namespace algebraic_graph_snapshot_detail {
inline AlgebraicGraphSourceBinding identity_binding(std::uint64_t key,const core::RelationalComplex& source,
    std::size_t maximum=1000000) {
    std::size_t count{};
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto& grade=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        algebraic_archive_detail::bound(grade.cell_count(),std::numeric_limits<std::uint32_t>::max(),"graph anchor cell index budget");
        count=machine::checked_size_add(count,grade.cell_count());
        if (k) count=machine::checked_size_add(count,grade.incidence_count());
    }
    algebraic_archive_detail::bound(count,maximum,"graph source identity coordinate budget");
    AlgebraicGraphSourceBinding result{key,AlgebraicGraphSourceKind::AnchorMorphism,{},{}};
    result.cells.resize(source.grade_count());result.occurrences.resize(source.grade_count());
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto& grade=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        for (std::size_t i=0;i<grade.cell_count();++i) result.cells[k].push_back(static_cast<std::uint32_t>(i));
        if (k) for (std::size_t i=0;i<grade.incidence_count();++i) result.occurrences[k].push_back(i);
    }
    return result;
}
inline void validate_binding(const AlgebraicArchiveGraph& graph,std::uint64_t anchor,
    const AlgebraicGraphSourceBinding& binding,std::size_t& remaining) {
    using algebraic_archive_detail::require;using algebraic_archive_detail::bound;
    const auto& source=graph.bundle(binding.bundle).source();const auto& target=graph.bundle(anchor).source();
    if (binding.kind==AlgebraicGraphSourceKind::Independent) {
        require(binding.bundle!=anchor && binding.cells.empty() && binding.occurrences.empty(),
            "independent graph source cannot declare an anchor map");return;
    }
    require(binding.kind==AlgebraicGraphSourceKind::AnchorMorphism,"unknown graph source binding kind");
    require(source.semantic_scale()==target.semantic_scale(),"graph source map requires its declared semantic scale");
    require(binding.cells.size()==source.grade_count() && binding.occurrences.size()==source.grade_count(),
        "graph source map must retain every grade");
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        require(binding.cells[k].size()==arena.cell_count() && binding.occurrences[k].size()==(k?arena.incidence_count():0),
            "graph source map must retain all cells and occurrences");
        const auto count=machine::checked_size_add(binding.cells[k].size(),binding.occurrences[k].size());
        bound(count,remaining,"graph source map coordinate budget");remaining-=count;
    }
    const algebra::RelationalMorphism map(source,target,binding.cells);
    require(map.occurrence_maps()==binding.occurrences,"graph source occurrence map differs from the declared strict morphism");
    if (binding.bundle==anchor) {
        for (const auto& row:binding.cells) for (std::size_t i=0;i<row.size();++i)
            require(row[i]==i,"native graph anchor cell map must be its complete identity");
        for (const auto& row:binding.occurrences) for (std::size_t i=0;i<row.size();++i)
            require(row[i]==i,"native graph anchor occurrence map must be its complete identity");
    }
}
} // namespace algebraic_graph_snapshot_detail

// A named anchor and complete binding table declare how graph sources relate to
// the native publication. Equal RFCX bytes neither share owners nor supply maps.
// A source morphism does not change coefficients, stalk bases or metric domains.
class AlgebraicGraphSnapshot {
    struct State {
        AlgebraicArchiveGraph graph;
        std::uint64_t anchor;
        std::vector<AlgebraicGraphSourceBinding> sources;
        std::vector<AlgebraicGraphArchiveBinding> archives;
    };
public:
    static AlgebraicGraphSnapshot declare(AlgebraicArchiveGraph graph,std::uint64_t anchor,
        std::vector<AlgebraicGraphSourceBinding> sources,std::vector<AlgebraicGraphArchiveBinding> archives,
        const AlgebraicGraphSnapshotLimits& limits={}) {
        using algebraic_archive_detail::require;using algebraic_archive_detail::bound;
        const auto& c=graph.contents();algebraic_graph_detail::admit_counts(c,limits.graph);
        (void)graph.bundle(anchor);bound(sources.size(),limits.max_source_bindings,"graph source binding count budget");
        bound(archives.size(),limits.max_archive_bindings,"graph archive binding count budget");
        require(sources.size()==c.bundles.size() && archives.size()==c.archives.size(),"native graph bindings must cover every registered source and archive");
        std::sort(sources.begin(),sources.end(),[](const auto& a,const auto& b){return a.bundle<b.bundle;});
        std::sort(archives.begin(),archives.end(),[](const auto& a,const auto& b){return a.archive<b.archive;});
        auto remaining=limits.max_source_coordinates;
        for (std::size_t i=0;i<sources.size();++i) {
            require(sources[i].bundle==c.bundles[i].key,"native graph source binding names differ");
            algebraic_graph_snapshot_detail::validate_binding(graph,anchor,sources[i],remaining);
        }
        for (std::size_t i=0;i<archives.size();++i) {
            require(archives[i].archive==c.archives[i].key &&
                archives[i].bundle==algebraic_graph_detail::owner_key(c.bundles,c.archives[i].value.bundle()),
                "native graph archive binding must name its actual registered bundle owner");
        }
        return AlgebraicGraphSnapshot(std::make_shared<const State>(State{std::move(graph),anchor,std::move(sources),std::move(archives)}));
    }
    // Explicit anchor-only declaration: every other source stays independent,
    // including sources whose complete values happen to equal the anchor.
    static AlgebraicGraphSnapshot anchor_only(AlgebraicArchiveGraph graph,std::uint64_t anchor,
        const AlgebraicGraphSnapshotLimits& limits={}) {
        const auto& c=graph.contents();(void)graph.bundle(anchor);
        algebraic_archive_detail::bound(c.bundles.size(),limits.max_source_bindings,"graph source binding count budget");
        algebraic_archive_detail::bound(c.archives.size(),limits.max_archive_bindings,"graph archive binding count budget");
        std::vector<AlgebraicGraphSourceBinding> sources;std::vector<AlgebraicGraphArchiveBinding> archives;
        for (const auto& row:c.bundles) sources.push_back(row.key==anchor?
            algebraic_graph_snapshot_detail::identity_binding(anchor,row.value.source(),limits.max_source_coordinates):
            AlgebraicGraphSourceBinding{row.key,AlgebraicGraphSourceKind::Independent,{},{}});
        for (const auto& row:c.archives) archives.push_back({row.key,algebraic_graph_detail::owner_key(c.bundles,row.value.bundle())});
        return declare(std::move(graph),anchor,std::move(sources),std::move(archives),limits);
    }
    AlgebraicGraphSnapshot with_source_morphism(std::uint64_t bundle,std::vector<std::vector<std::uint32_t>> cells,
        const AlgebraicGraphSnapshotLimits& limits={}) const {
        const auto& source=graph().bundle(bundle).source();
        // Charge caller map extents before the strict morphism allocates its
        // occurrence table. The declared cell map supplies the source relation.
        std::size_t count{};
        for (const auto& row:cells) count=machine::checked_size_add(count,row.size());
        for (std::size_t k=1;k<source.grade_count();++k)
            count=machine::checked_size_add(count,source.grade(core::Grade{static_cast<std::uint16_t>(k)}).incidence_count());
        algebraic_archive_detail::bound(count,limits.max_source_coordinates,"graph source map coordinate budget");
        const algebra::RelationalMorphism map(source,this->source(),cells);
        auto sources=source_bindings();
        auto found=std::find_if(sources.begin(),sources.end(),[&](const auto& row){return row.bundle==bundle;});
        algebraic_archive_detail::require(found!=sources.end(),"graph source binding absent");
        *found={bundle,AlgebraicGraphSourceKind::AnchorMorphism,std::move(cells),map.occurrence_maps()};
        return declare(graph(),anchor_bundle(),std::move(sources),archive_bindings(),limits);
    }
    const AlgebraicArchiveGraph& graph() const & {valid();return state_->graph;}
    const AlgebraicArchiveGraph& graph() const &&=delete;
    std::uint64_t anchor_bundle() const {valid();return state_->anchor;}
    const core::RelationalComplex& source() const & {return graph().bundle(anchor_bundle()).source();}
    const core::RelationalComplex& source() const &&=delete;
    const std::vector<AlgebraicGraphSourceBinding>& source_bindings() const & {valid();return state_->sources;}
    const std::vector<AlgebraicGraphSourceBinding>& source_bindings() const &&=delete;
    const std::vector<AlgebraicGraphArchiveBinding>& archive_bindings() const & {valid();return state_->archives;}
    const std::vector<AlgebraicGraphArchiveBinding>& archive_bindings() const &&=delete;
    bool same_owner(const AlgebraicGraphSnapshot& other) const noexcept {return state_ && state_==other.state_;}
    numeric::RationalVector push_source_field(std::uint64_t bundle,core::Grade grade,const numeric::RationalVector& values) const {
        const auto& rows=source_bindings();
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.bundle==bundle;});
        algebraic_archive_detail::require(found!=rows.end() && found->kind==AlgebraicGraphSourceKind::AnchorMorphism,
            "independent graph source has no declared anchor map");
        return algebra::RelationalMorphism(graph().bundle(bundle).source(),source(),found->cells).push_field(grade,values);
    }
private:
    void valid() const {algebraic_archive_detail::require(static_cast<bool>(state_),"moved native graph snapshot");}
    explicit AlgebraicGraphSnapshot(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::storage
