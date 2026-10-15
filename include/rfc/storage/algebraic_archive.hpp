#pragma once
#include "rfc/field/algebraic_metric_field.hpp"
#include "rfc/storage/rfcx.hpp"
#include "rfc/local/algebraic_reconstruction.hpp"
#include <optional>
#include <variant>

namespace rfc::storage {
using AlgebraicSelectedEmbedding=std::variant<geometry::SelectedRealAlgebraicEmbedding,geometry::SelectedComplexAlgebraicEmbedding>;
using AlgebraicArchiveValue=std::variant<local::AlgebraicModuleVector,local::AlgebraicCovector,
    local::AlgebraicModuleMap,field::AlgebraicMeasurementForm,field::AlgebraicModuleMetric>;
struct AlgebraicArchiveRecord { std::uint64_t key; AlgebraicArchiveValue value; };
struct AlgebraicArchiveReconstruction { std::uint64_t key,accession,reconstruction; };
struct AlgebraicArchiveContents {
    std::optional<numeric::AlgebraicInvolution> involution;
    std::optional<AlgebraicSelectedEmbedding> embedding;
    std::optional<field::AlgebraicMetricDomain> metric_domain;
    std::optional<local::AlgebraicRelationalFiberSystem> fibers;
    std::vector<AlgebraicArchiveRecord> records;
    std::vector<AlgebraicArchiveReconstruction> reconstructions;
};
// Reader/operation admission policy, not a persisted mathematical declaration.
struct AlgebraicArchiveLimits {
    std::size_t max_file_bytes{16u<<20},max_records{4096},max_rational_coordinates{1000000},max_integer_bytes{2048};
    numeric::AlgebraicFieldLimits field{};
    local::AlgebraicModuleLimits modules{};
    local::AlgebraicFiberLimits fibers{};
    geometry::RealRootLimits real_root{};
    geometry::ComplexRootLimits complex_root{};
    field::AlgebraicMetricLimits metric{};
};

// One explicitly owned field/bundle. Names label typed records; names and
// canonical bytes never establish equality of live field or carrier owners.
class AlgebraicArchive {
    struct State { local::AlgebraicStalkBundle bundle; AlgebraicArchiveContents contents; };
public:
    static AlgebraicArchive declare(const local::AlgebraicStalkBundle& bundle,AlgebraicArchiveContents contents={}) {
        if (contents.metric_domain) {
            const auto& domain=*contents.metric_domain;
            if (!domain.field().same_owner(bundle.field())) throw std::invalid_argument("archive metric field owner differs");
            if (contents.involution && !contents.involution->same_owner(domain.involution()))
                throw std::invalid_argument("archive metric involution owner differs");
            if (contents.embedding) {
                const bool same=std::visit([&](const auto& embedding) {
                    using T=std::decay_t<decltype(embedding)>;
                    const auto* selected=std::get_if<T>(&domain.embedding());
                    return selected && selected->same_owner(embedding);
                },*contents.embedding);
                if (!same) throw std::invalid_argument("archive metric embedding owner differs");
            }
            contents.involution=domain.involution(); contents.embedding=domain.embedding();
        }
        if (contents.involution && !contents.involution->field().same_owner(bundle.field()))
            throw std::invalid_argument("archive involution field owner differs");
        if (contents.embedding && !std::visit([&](const auto& root){return root.field().same_owner(bundle.field());},*contents.embedding))
            throw std::invalid_argument("archive selected embedding field owner differs");
        if (contents.fibers && !contents.fibers->bundle().same_owner(bundle))
            throw std::invalid_argument("archive fiber bundle owner differs");
        auto carrier=[&](const local::AlgebraicModule& module) {
            if (!bundle.stalk(module.cell()).same_owner(module)) throw std::invalid_argument("archive record carrier owner differs");
        };
        std::sort(contents.records.begin(),contents.records.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        std::uint64_t last{};
        for (const auto& record:contents.records) {
            if (!record.key || record.key==last) throw std::invalid_argument("archive record keys must be nonzero and unique");
            last=record.key;
            std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T,local::AlgebraicModuleMap>) { carrier(value.source()); carrier(value.target()); }
                else {
                    carrier(value.carrier());
                    if constexpr (std::same_as<T,field::AlgebraicMeasurementForm> || std::same_as<T,field::AlgebraicModuleMetric>)
                        if (!contents.metric_domain || !contents.metric_domain->same_owner(value.domain()))
                            throw std::invalid_argument("archive form metric domain owner differs");
                }
            },record.value);
        }
        auto named_map=[&](std::uint64_t key) -> const local::AlgebraicModuleMap& {
            const auto found=std::lower_bound(contents.records.begin(),contents.records.end(),key,[](const auto& r,auto k){return r.key<k;});
            if (found==contents.records.end() || found->key!=key) throw std::invalid_argument("archive reconstruction map name absent");
            const auto* map=std::get_if<local::AlgebraicModuleMap>(&found->value);
            if (!map) throw std::invalid_argument("archive reconstruction requires typed map records");
            return *map;
        };
        std::sort(contents.reconstructions.begin(),contents.reconstructions.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        last=0;
        for (const auto& pair:contents.reconstructions) {
            if (!pair.key || pair.key==last) throw std::invalid_argument("archive reconstruction names must be nonzero and unique");
            last=pair.key;
            (void)local::AlgebraicReconstruction::declare(local::AlgebraicCarrierComparison::from_map(named_map(pair.accession)),
                local::AlgebraicCarrierComparison::from_map(named_map(pair.reconstruction)));
        }
        return AlgebraicArchive(std::make_shared<const State>(State{bundle,std::move(contents)}));
    }
    const local::AlgebraicStalkBundle& bundle() const & noexcept { return state_->bundle; }
    const AlgebraicArchiveContents& contents() const & noexcept { return state_->contents; }
    const local::AlgebraicStalkBundle& bundle() const && = delete;
    const AlgebraicArchiveContents& contents() const && = delete;
    bool same_owner(const AlgebraicArchive& other) const noexcept { return state_==other.state_; }
    const AlgebraicArchiveValue& at(std::uint64_t key) const & {
        const auto& records=state_->contents.records;
        const auto found=std::lower_bound(records.begin(),records.end(),key,[](const auto& r,auto k){return r.key<k;});
        if (found==records.end() || found->key!=key) throw std::out_of_range("algebraic archive record absent");
        return found->value;
    }
    const AlgebraicArchiveValue& at(std::uint64_t) const && = delete;
    local::AlgebraicReconstruction reconstruction(std::uint64_t key) const {
        const auto& pairs=state_->contents.reconstructions;
        const auto found=std::lower_bound(pairs.begin(),pairs.end(),key,[](const auto& r,auto k){return r.key<k;});
        if (found==pairs.end() || found->key!=key) throw std::out_of_range("archive reconstruction absent");
        return local::AlgebraicReconstruction::declare(local::AlgebraicCarrierComparison::from_map(std::get<local::AlgebraicModuleMap>(at(found->accession))),
            local::AlgebraicCarrierComparison::from_map(std::get<local::AlgebraicModuleMap>(at(found->reconstruction))));
    }
private:
    explicit AlgebraicArchive(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::storage
