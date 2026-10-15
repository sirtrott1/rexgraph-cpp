#pragma once
#include "rfc/storage/algebraic_archive.hpp"
#include "rfc/field/algebraic_metric_comparison.hpp"

namespace rfc::storage {
template<class T> struct AlgebraicGraphEntry { std::uint64_t key; T value; };
struct AlgebraicGraphReconstruction { std::uint64_t key,accession,reconstruction; };
struct AlgebraicGraphMeasurement {
    std::uint64_t key,domains,carrier,source_archive,source_record,target_archive,target_record;
};
struct AlgebraicArchiveGraphContents {
    std::vector<AlgebraicGraphEntry<numeric::CertifiedAlgebraicField>> fields;
    std::vector<AlgebraicGraphEntry<numeric::AlgebraicInvolution>> involutions;
    std::vector<AlgebraicGraphEntry<AlgebraicSelectedEmbedding>> embeddings;
    std::vector<AlgebraicGraphEntry<field::AlgebraicMetricDomain>> domains;
    std::vector<AlgebraicGraphEntry<local::AlgebraicStalkBundle>> bundles;
    std::vector<AlgebraicGraphEntry<local::AlgebraicRelationalFiberSystem>> fibers;
    std::vector<AlgebraicGraphEntry<AlgebraicArchive>> archives;
    std::vector<AlgebraicGraphEntry<numeric::AlgebraicFieldEmbedding>> scalars;
    std::vector<AlgebraicGraphEntry<local::AlgebraicCarrierComparison>> carriers;
    std::vector<AlgebraicGraphEntry<field::AlgebraicMetricDomainComparison>> metric_comparisons;
    std::vector<AlgebraicGraphReconstruction> reconstructions;
    std::vector<AlgebraicGraphMeasurement> measurements;
};
struct AlgebraicArchiveGraphLimits {
    std::size_t max_file_bytes{64u<<20},max_owner_records{1024},max_edges{4096},max_rational_coordinates{4000000};
    AlgebraicArchiveLimits archive{};
    field::AlgebraicMeasurementComparisonLimits measurement{};
};
namespace algebraic_graph_detail {
template<class T> bool same_owner(const T& a,const T& b) { return a.same_owner(b); }
inline bool same_owner(const AlgebraicSelectedEmbedding& a,const AlgebraicSelectedEmbedding& b) {
    return std::visit([&](const auto& root) {
        using T=std::decay_t<decltype(root)>;const auto* other=std::get_if<T>(&b);
        return other && root.same_owner(*other);
    },a);
}
template<class T> const T& at(const std::vector<AlgebraicGraphEntry<T>>& entries,std::uint64_t key) {
    const auto found=std::lower_bound(entries.begin(),entries.end(),key,[](const auto& row,auto k){return row.key<k;});
    if (found==entries.end() || found->key!=key) throw std::out_of_range("algebraic graph reference absent");
    return found->value;
}
template<class T> std::uint64_t owner_key(const std::vector<AlgebraicGraphEntry<T>>& entries,const T& value) {
    for (const auto& entry:entries) if (same_owner(entry.value,value)) return entry.key;
    throw std::invalid_argument("algebraic graph actual owner is not registered");
}
inline std::uint64_t bundle_key(const AlgebraicArchiveGraphContents& c,const local::AlgebraicModule& module) {
    const auto cell=module.cell();
    for (const auto& entry:c.bundles) {
        const auto& source=entry.value.source();
        if (cell.grade.value<source.grade_count() && cell.index<source.grade(cell.grade).cell_count() &&
            module.same_owner(entry.value.stalk(cell))) return entry.key;
    }
    throw std::invalid_argument("algebraic graph actual carrier is not registered");
}
template<class T> void sort_keys(std::vector<T>& rows) {
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    std::uint64_t last{};
    for (const auto& row:rows) {
        if (!row.key || row.key==last) throw std::invalid_argument("algebraic graph keys must be nonzero and unique");
        last=row.key;
    }
}
template<class T> void sort_owners(std::vector<AlgebraicGraphEntry<T>>& rows) {
    sort_keys(rows);
    for (std::size_t i=0;i<rows.size();++i) for (std::size_t j=0;j<i;++j)
        if (same_owner(rows[i].value,rows[j].value))
            throw std::invalid_argument("one graph owner requires one name; reuse that reference");
}
inline const field::AlgebraicMeasurementForm& measurement(const AlgebraicArchiveValue& value) {
    if (const auto* form=std::get_if<field::AlgebraicMeasurementForm>(&value)) return *form;
    if (const auto* metric=std::get_if<field::AlgebraicModuleMetric>(&value)) return metric->measurement();
    throw std::invalid_argument("graph measurement requires a typed form or metric record");
}
inline void admit_counts(const AlgebraicArchiveGraphContents& c,const AlgebraicArchiveGraphLimits& limits) {
    std::size_t owners{},edges{};
    for (const auto n:{c.fields.size(),c.involutions.size(),c.embeddings.size(),c.domains.size(),c.bundles.size(),
        c.fibers.size(),c.archives.size()}) owners=machine::checked_size_add(owners,n);
    for (const auto n:{c.scalars.size(),c.carriers.size(),c.metric_comparisons.size(),c.reconstructions.size(),c.measurements.size()})
        edges=machine::checked_size_add(edges,n);
    if (owners>limits.max_owner_records || edges>limits.max_edges) throw std::length_error("algebraic graph record budget");
}
} // namespace algebraic_graph_detail

// Explicit tables preserve shared registered owners and independent equal
// declarations. Names are scoped references, never content-derived identities.
class AlgebraicArchiveGraph {
    struct State {
        AlgebraicArchiveGraphContents contents;
        std::vector<AlgebraicGraphEntry<local::AlgebraicReconstruction>> reconstructions;
        std::vector<AlgebraicGraphEntry<field::AlgebraicMeasurementComparison>> measurements;
    };
public:
    static AlgebraicArchiveGraph declare(AlgebraicArchiveGraphContents c,const AlgebraicArchiveGraphLimits& limits={}) {
        using namespace algebraic_graph_detail;
        admit_counts(c,limits);
        sort_owners(c.fields);sort_owners(c.involutions);sort_owners(c.embeddings);sort_owners(c.domains);
        sort_owners(c.bundles);sort_owners(c.fibers);sort_owners(c.archives);sort_owners(c.scalars);
        sort_owners(c.carriers);sort_owners(c.metric_comparisons);sort_keys(c.reconstructions);sort_keys(c.measurements);
        for (const auto& row:c.involutions) (void)owner_key(c.fields,row.value.field());
        for (const auto& row:c.embeddings)
            std::visit([&](const auto& root){(void)owner_key(c.fields,root.field());},row.value);
        for (const auto& row:c.domains) {
            (void)owner_key(c.involutions,row.value.involution());(void)owner_key(c.embeddings,row.value.embedding());
        }
        for (const auto& row:c.bundles) (void)owner_key(c.fields,row.value.field());
        for (const auto& row:c.fibers) (void)owner_key(c.bundles,row.value.bundle());
        for (const auto& row:c.archives) {
            (void)owner_key(c.bundles,row.value.bundle());const auto& declarations=row.value.contents();
            if (declarations.involution) (void)owner_key(c.involutions,*declarations.involution);
            if (declarations.embedding) (void)owner_key(c.embeddings,*declarations.embedding);
            if (declarations.metric_domain) (void)owner_key(c.domains,*declarations.metric_domain);
            if (declarations.fibers) (void)owner_key(c.fibers,*declarations.fibers);
        }
        for (const auto& row:c.scalars) {
            (void)owner_key(c.fields,row.value.source());(void)owner_key(c.fields,row.value.target());
        }
        for (const auto& row:c.carriers) {
            (void)owner_key(c.scalars,row.value.scalar_comparison());
            (void)bundle_key(c,row.value.source());(void)bundle_key(c,row.value.target());
        }
        for (const auto& row:c.metric_comparisons) {
            (void)owner_key(c.scalars,row.value.scalar_comparison());
            (void)owner_key(c.domains,row.value.source());(void)owner_key(c.domains,row.value.target());
        }
        std::vector<AlgebraicGraphEntry<local::AlgebraicReconstruction>> reconstructions;reconstructions.reserve(c.reconstructions.size());
        for (const auto& row:c.reconstructions)
            reconstructions.push_back({row.key,local::AlgebraicReconstruction::declare(at(c.carriers,row.accession),at(c.carriers,row.reconstruction))});
        std::vector<AlgebraicGraphEntry<field::AlgebraicMeasurementComparison>> measurements;measurements.reserve(c.measurements.size());
        for (const auto& row:c.measurements)
            measurements.push_back({row.key,field::AlgebraicMeasurementComparison::declare(at(c.metric_comparisons,row.domains),
                at(c.carriers,row.carrier),algebraic_graph_detail::measurement(at(c.archives,row.source_archive).at(row.source_record)),
                algebraic_graph_detail::measurement(at(c.archives,row.target_archive).at(row.target_record)),limits.measurement)});
        return AlgebraicArchiveGraph(std::make_shared<const State>(State{std::move(c),std::move(reconstructions),std::move(measurements)}));
    }
    const AlgebraicArchiveGraphContents& contents() const & noexcept { return state_->contents; }
    const AlgebraicArchiveGraphContents& contents() const && = delete;
    bool same_owner(const AlgebraicArchiveGraph& other) const noexcept { return state_==other.state_; }
    const numeric::CertifiedAlgebraicField& field(std::uint64_t key) const & { return algebraic_graph_detail::at(contents().fields,key); }
    const numeric::CertifiedAlgebraicField& field(std::uint64_t) const && = delete;
    const local::AlgebraicStalkBundle& bundle(std::uint64_t key) const & { return algebraic_graph_detail::at(contents().bundles,key); }
    const local::AlgebraicStalkBundle& bundle(std::uint64_t) const && = delete;
    const AlgebraicArchive& archive(std::uint64_t key) const & { return algebraic_graph_detail::at(contents().archives,key); }
    const AlgebraicArchive& archive(std::uint64_t) const && = delete;
    const numeric::AlgebraicFieldEmbedding& scalars(std::uint64_t key) const & { return algebraic_graph_detail::at(contents().scalars,key); }
    const numeric::AlgebraicFieldEmbedding& scalars(std::uint64_t) const && = delete;
    const local::AlgebraicCarrierComparison& carrier(std::uint64_t key) const & { return algebraic_graph_detail::at(contents().carriers,key); }
    const local::AlgebraicCarrierComparison& carrier(std::uint64_t) const && = delete;
    const local::AlgebraicReconstruction& reconstruction(std::uint64_t key) const & { return algebraic_graph_detail::at(state_->reconstructions,key); }
    const local::AlgebraicReconstruction& reconstruction(std::uint64_t) const && = delete;
    const field::AlgebraicMeasurementComparison& measurement(std::uint64_t key) const & { return algebraic_graph_detail::at(state_->measurements,key); }
    const field::AlgebraicMeasurementComparison& measurement(std::uint64_t) const && = delete;
private:
    explicit AlgebraicArchiveGraph(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::storage
