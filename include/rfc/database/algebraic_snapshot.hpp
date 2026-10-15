#pragma once
#include "rfc/storage/algebraic_archive_history.hpp"
#include "rfc/storage/algebraic_archive_graph_history.hpp"
#include "rfc/runtime/work_graph.hpp"
#include "rfc/numeric/index_domain.hpp"

namespace rfc::database {
enum class AlgebraicRecordKind { Vector, Covector, Map, MeasurementForm, Metric };
struct AlgebraicGraphArchiveOrigin {
    storage::AlgebraicGraphRevision revision;
    std::uint64_t archive_key,bundle_key;
};
namespace algebraic_database_detail {
inline AlgebraicRecordKind kind(const storage::AlgebraicArchiveValue& value) {
    return static_cast<AlgebraicRecordKind>(value.index());
}
inline const std::vector<numeric::AlgebraicElement>& coefficients(const storage::AlgebraicArchiveValue& value) {
    return std::visit([](const auto& v) -> const std::vector<numeric::AlgebraicElement>& {
        using T=std::decay_t<decltype(v)>;
        if constexpr (std::same_as<T,local::AlgebraicModuleVector>) return v.coordinates();
        else if constexpr (std::same_as<T,field::AlgebraicMeasurementForm>) return v.coefficient_form().coefficients();
        else if constexpr (std::same_as<T,field::AlgebraicModuleMetric>) return v.measurement().coefficient_form().coefficients();
        else return v.coefficients();
    },value);
}
inline local::AlgebraicModule source(const storage::AlgebraicArchiveValue& value) {
    return std::visit([](const auto& v) {
        if constexpr (std::same_as<std::decay_t<decltype(v)>,local::AlgebraicModuleMap>) return v.source();
        else return v.carrier();
    },value);
}
inline local::AlgebraicModule target(const storage::AlgebraicArchiveValue& value) {
    return std::visit([](const auto& v) {
        if constexpr (std::same_as<std::decay_t<decltype(v)>,local::AlgebraicModuleMap>) return v.target();
        else return v.carrier();
    },value);
}
inline numeric::IndexDomain shape(const storage::AlgebraicArchiveValue& value) {
    const auto s=source(value),t=target(value);
    if (kind(value)==AlgebraicRecordKind::Vector || kind(value)==AlgebraicRecordKind::Covector)
        return numeric::IndexDomain({s.rank()});
    return numeric::IndexDomain({t.rank(),s.rank()});
}
inline std::size_t bits(const numeric::AlgebraicElement& value) {
    std::size_t out=1;
    for (const auto& q:value.coordinates()) out=std::max(out,std::max(
        composite::declaration_detail::integer_bits(q.get_num()),composite::declaration_detail::integer_bits(q.get_den())));
    return out;
}
} // namespace algebraic_database_detail

// Aliasing pins retain the archive, native source, field and actual carriers.
// All declared record coordinates are supported, including exact zero entries.
class AlgebraicRecordPin {
public:
    std::uint64_t key() const noexcept { return key_; }
    AlgebraicRecordKind kind() const { return algebraic_database_detail::kind(value()); }
    const storage::AlgebraicArchiveValue& value() const & { if (!value_) throw std::invalid_argument("moved algebraic record pin");return *value_; }
    const storage::AlgebraicArchiveValue& value() const && = delete;
    std::shared_ptr<const storage::AlgebraicArchiveValue> value_pin() const { return value_; }
    local::AlgebraicModule source_carrier() const { return algebraic_database_detail::source(value()); }
    local::AlgebraicModule target_carrier() const { return algebraic_database_detail::target(value()); }
    numeric::IndexDomain domain() const { return algebraic_database_detail::shape(value()); }
    const std::vector<numeric::AlgebraicElement>& coefficients() const & { return algebraic_database_detail::coefficients(value()); }
    const std::vector<numeric::AlgebraicElement>& coefficients() const && = delete;
    std::shared_ptr<const std::vector<numeric::AlgebraicElement>> coefficients_pin() const { return {value_,&coefficients()}; }
    numeric::AlgebraicElement coordinate(const std::vector<std::size_t>& indices) const { return coefficients().at(domain().flatten(indices)); }
    std::uint64_t source_namespace() const noexcept { return namespace_; }
    std::uint64_t sequence() const noexcept { return sequence_; }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return endpoint_; }
    std::shared_ptr<const AlgebraicGraphArchiveOrigin> graph_origin() const { return graph_; }
private:
    AlgebraicRecordPin(std::uint64_t key,std::shared_ptr<const storage::AlgebraicArchiveValue> value,
        std::uint64_t ns,std::uint64_t seq,std::shared_ptr<const storage::TemporalEndpoint> endpoint,
        std::shared_ptr<const AlgebraicGraphArchiveOrigin> graph)
        :key_(key),value_(std::move(value)),namespace_(ns),sequence_(seq),endpoint_(std::move(endpoint)),graph_(std::move(graph)) {}
    std::uint64_t key_;
    std::shared_ptr<const storage::AlgebraicArchiveValue> value_;
    std::uint64_t namespace_,sequence_;
    std::shared_ptr<const storage::TemporalEndpoint> endpoint_;
    std::shared_ptr<const AlgebraicGraphArchiveOrigin> graph_;
    friend class AlgebraicSnapshot;
};
struct AlgebraicOccurrenceView {
    core::Grade grade;
    std::size_t slot;
    core::OccurrenceKey key;
    bool active;
    numeric::Rational coefficient;
    local::AlgebraicModuleMap underlying;
    local::AlgebraicModuleMap weighted_map() const { return underlying.scale(underlying.source().field().scalar(coefficient)); }
    local::AlgebraicModuleMap contribution_map() const { return underlying.scale(underlying.source().field().scalar(active?coefficient:numeric::Rational{0})); }
};
class AlgebraicSnapshot {
    struct State {
        std::shared_ptr<const storage::AlgebraicArchive> archive;
        std::uint64_t name_space,sequence;
        std::shared_ptr<const storage::TemporalEndpoint> endpoint;
        std::shared_ptr<const AlgebraicGraphArchiveOrigin> graph;
    };
public:
    // Names/sequences label this publication; they do not identify carrier owners
    // or claim event-time/producer availability semantics.
    static AlgebraicSnapshot publish(const storage::AlgebraicArchive& archive,std::uint64_t source_namespace,std::uint64_t sequence) {
        if (!source_namespace) throw std::invalid_argument("algebraic publication namespace required");
        return AlgebraicSnapshot(std::make_shared<const State>(State{
            std::make_shared<const storage::AlgebraicArchive>(archive),source_namespace,sequence,{},{}}));
    }
    static AlgebraicSnapshot restore(const storage::NativeSeriesEndpoint& publication,storage::AlgebraicArchiveChannel channel,
        const storage::AlgebraicArchiveLimits& limits={}) {
        auto endpoint=publication.pin();
        auto archive=storage::read_algebraic_archive(*endpoint,channel,limits);
        return AlgebraicSnapshot(std::make_shared<const State>(State{
            std::make_shared<const storage::AlgebraicArchive>(std::move(archive)),publication.source_namespace(),publication.sequence(),std::move(endpoint),{}}));
    }
    // Select the actual registered archive without serializing it into a new
    // field/bundle owner. The alias and origin retain only this graph revision.
    static AlgebraicSnapshot select_graph_archive(const storage::AlgebraicGraphRevision& revision,std::uint64_t key,
        runtime::WorkAvailability availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(revision.source_namespace(),revision.sequence(),availability);
        runtime::ExecutionRequirements required;required.records=2;runtime::admit_execution(required,lease.policy());
        const auto& bindings=revision.snapshot().archive_bindings();
        const auto found=std::lower_bound(bindings.begin(),bindings.end(),key,[](const auto& row,auto value){return row.archive<value;});
        if (found==bindings.end() || found->archive!=key) throw std::out_of_range("graph archive binding absent");
        auto origin=std::make_shared<const AlgebraicGraphArchiveOrigin>(AlgebraicGraphArchiveOrigin{revision,key,found->bundle});
        auto result=AlgebraicSnapshot(std::make_shared<const State>(State{
            revision.archive_pin(key),revision.source_namespace(),revision.sequence(),revision.source_publication(),std::move(origin)}));
        lease.check_cancelled();return result;
    }
    const storage::AlgebraicArchive& archive() const & { return *state().archive; }
    const storage::AlgebraicArchive& archive() const && = delete;
    std::uint64_t source_namespace() const { return state().name_space; }
    std::uint64_t sequence() const { return state().sequence; }
    bool same_publication(const AlgebraicSnapshot& other) const noexcept { return state_ && state_==other.state_; }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return state().endpoint; }
    std::shared_ptr<const AlgebraicGraphArchiveOrigin> graph_origin() const { return state().graph; }
    AlgebraicRecordPin record(std::uint64_t key) const {
        return {key,{state_,&archive().at(key)},source_namespace(),sequence(),source_publication(),graph_origin()};
    }
    AlgebraicOccurrenceView occurrence(core::Grade grade,std::size_t slot) const {
        const auto& source=archive().bundle().source();
        const auto& fibers=archive().contents().fibers;
        if (!fibers) throw std::invalid_argument("archive has no declared fiber system");
        const auto& arena=source.grade(grade);
        (void)arena.lower_index.at(slot);
        return {grade,slot,arena.occurrence_identities.at(slot),arena.aspects.get(source.signature().existence_aspect(),slot),source.incidence_coefficient(grade,slot),fibers->occurrence_map(grade,slot)};
    }
private:
    explicit AlgebraicSnapshot(std::shared_ptr<const State> state):state_(std::move(state)) {}
    const State& state() const { if (!state_) throw std::invalid_argument("moved algebraic snapshot"); return *state_; }
    std::shared_ptr<const State> state_;
};

// Decode exactly the selected prefix endpoints. No suffix archive is examined;
// independent restores deliberately have independent coefficient/carrier owners.
class AlgebraicHistoryWindow {
public:
    static AlgebraicHistoryWindow select(const storage::NativeTemporalSeries& series,std::uint64_t first,std::uint64_t last,
        runtime::WorkAvailability cutoff,storage::AlgebraicArchiveChannel channel,
        const storage::AlgebraicArchiveLimits& limits={},std::size_t max_snapshots=1024) {
        runtime::admit_work_availability(series.source_namespace(),last,cutoff);
        if (first>last || first<series.first_sequence() || last>series.last_sequence()) throw std::out_of_range("algebraic history bounds");
        if (last-first>=max_snapshots) throw std::length_error("algebraic history snapshot budget");
        std::vector<AlgebraicSnapshot> selected;
        selected.reserve(static_cast<std::size_t>(last-first)+1);
        for (auto sequence=first;;++sequence) {
            selected.push_back(AlgebraicSnapshot::restore(series.at(sequence),channel,limits));
            if (sequence==last) break;
        }
        return AlgebraicHistoryWindow(std::move(selected),cutoff);
    }
    const std::vector<AlgebraicSnapshot>& snapshots() const & { return snapshots_; }
    const std::vector<AlgebraicSnapshot>& snapshots() const && = delete;
    runtime::WorkAvailability availability() const noexcept { return cutoff_; }
private:
    AlgebraicHistoryWindow(std::vector<AlgebraicSnapshot> snapshots,runtime::WorkAvailability cutoff)
        :snapshots_(std::move(snapshots)),cutoff_(cutoff) {}
    std::vector<AlgebraicSnapshot> snapshots_;
    runtime::WorkAvailability cutoff_;
};
} // namespace rfc::database
