#pragma once

#include "rfc/storage/temporal_seed.hpp"
#include "rfc/storage/persistent_append_index.hpp"
#include "rfc/temporal/multitime.hpp"
#include <memory>
#include <tuple>

namespace rfc::storage {

// Sequence numbers denote retained endpoints, not wall-clock time. No inverse
// correspondence or interpolation is invented between these endpoints.
struct NativeSeriesLimits {
    std::size_t max_segments{100000};
    std::size_t max_checkpoints{128};
    std::size_t max_archive_bytes{256u * 1024u * 1024u};
    std::size_t max_analysis_steps{1024};
    std::size_t max_analysis_dimension{4096};
    std::size_t max_analysis_scalars{4000000};
    std::size_t max_analysis_work{100000000};
    TemporalReadLimits temporal{};
};
struct NativeSeriesReplayStats {
    std::uint64_t starting_sequence{};
    std::size_t replayed_segments{};
    bool used_checkpoint{};
};
struct NativeSeriesStorageStats {
    std::size_t frame_index_pages{}, last_append_pages{}, last_append_pointer_copies{}, checkpoint_count{};
    unsigned frame_index_levels{};
};
enum class NativeSeriesComponentKind {BaseSource,BaseChannels,Segment,CheckpointSource,CheckpointChannels};
struct NativeSeriesComponent {
    NativeSeriesComponentKind kind;
    std::uint64_t source_namespace{},sequence{};
    std::shared_ptr<const std::vector<std::byte>> bytes;
};

namespace native_series_detail {
using Bytes = std::vector<std::byte>;
using temporal_source_detail::require;
inline void bound(std::size_t n, std::size_t limit, const char* message) {
    if (n > limit) throw std::length_error(message);
}
inline TemporalReadLimits policy(const NativeSeriesLimits& limits) {
    auto p = limits.temporal;
    p.max_file_bytes = std::min(p.max_file_bytes, limits.max_archive_bytes);
    p.require_canonical = true; // a series never weakens its embedded contracts
    return p;
}
struct Snapshot {
    std::shared_ptr<const TemporalEndpoint> endpoint;
    Bytes source;
    std::shared_ptr<const Bytes> channels;
};
inline std::shared_ptr<const Snapshot> snapshot(const TemporalEndpoint& e, const NativeSeriesLimits& limits) {
    auto source = encode_rfcx(e.source).bytes;
    auto channels = encode_temporal_endpoint(e);
    const auto p = policy(limits);
    bound(source.size(), p.max_file_bytes, "series source byte budget");
    bound(channels.size(), p.max_file_bytes, "series channel byte budget");
    // Decode into fresh storage rather than moving caller-owned vectors: old
    // mutable references must not be able to change an immutable publication.
    MappedRfcx mapped{std::span<const std::byte>(source)};
    auto decoded = temporal_seed_detail::decode_owned(mapped.materialize(), std::move(channels), p);
    auto retained = decoded.retained_channels ? std::move(decoded.retained_channels)
        : std::make_shared<const Bytes>(std::move(channels));
    return std::make_shared<const Snapshot>(Snapshot{
        std::make_shared<const TemporalEndpoint>(std::move(decoded.endpoint)), std::move(source), std::move(retained)});
}
struct Frame {
    TemporalSegment segment;
    Bytes bytes;
};
struct State {
    std::uint64_t name_space{};
    NativeSeriesLimits limits;
    std::shared_ptr<const Snapshot> base;
    std::shared_ptr<const TemporalEndpoint> tail;
    AppendIndex<Frame> frames;
    using Checkpoints = std::map<std::uint64_t, std::shared_ptr<const Snapshot>>;
    std::shared_ptr<const Checkpoints> checkpoints{std::make_shared<const Checkpoints>()};
    std::size_t archive_bytes{}, frame_bytes{}, checkpoint_bytes{};
};
inline std::size_t add_blob(std::size_t total, std::size_t n) {
    return machine::checked_size_add(total, machine::checked_size_add(8, n));
}
inline std::size_t archive_size(const State& s) {
    std::size_t n = 32 + 8 + 8 + 8; // envelope, namespace, frame/checkpoint counts
    n = add_blob(n, s.base->source.size()); n = add_blob(n, s.base->channels->size());
    n = machine::checked_size_add(n, s.frame_bytes);
    n = machine::checked_size_add(n, s.checkpoint_bytes);
    bound(n, s.limits.max_archive_bytes, "series aggregate archive byte budget");
    return n;
}
inline std::size_t offset(const State& s, std::uint64_t sequence) {
    const auto begin = s.base->endpoint->sequence;
    require(sequence >= begin && sequence <= s.tail->sequence, "sequence outside retained series");
    const auto n = sequence - begin;
    require(n <= s.frames.size(), "series sequence/index invariant");
    return static_cast<std::size_t>(n);
}
inline std::shared_ptr<const TemporalEndpoint> reconstruct(
    const State& s, std::uint64_t sequence, NativeSeriesReplayStats& stats, bool from_base = false) {
    (void)offset(s, sequence);
    auto current = s.base->endpoint;
    stats = {current->sequence, 0, false};
    if (!from_base) {
        auto cp = s.checkpoints->upper_bound(sequence);
        if (cp != s.checkpoints->begin()) {
            --cp; current = cp->second->endpoint;
            stats = {current->sequence, 0, true};
        }
    }
    for (auto i = offset(s, current->sequence); current->sequence < sequence; ++i) {
        current = std::make_shared<const TemporalEndpoint>(replay_temporal_segment(
            *current, s.frames.at(i)->segment, s.limits.temporal.source_declarations));
        ++stats.replayed_segments;
    }
    return current;
}
inline numeric::RationalVector dense(const numeric::SparseRationalVector& x) {
    numeric::RationalVector out(x.size(), numeric::Rational{0});
    for (const auto& e : x.entries()) out[e.index] = e.value;
    return out;
}
// Conservative work/dense-shape preflights for reference temporal analysis.
// They are not a process-wide allocation quota. Exact coefficient growth is
// checked separately; native GMP operations may allocate before that check.
struct AnalysisBudget {
    const NativeSeriesLimits& limits;
    std::size_t scalars{}, work{};
    void shape(std::size_t rows, std::size_t cols = 1) {
        bound(rows, limits.max_analysis_dimension, "series analysis row dimension");
        bound(cols, limits.max_analysis_dimension, "series analysis column dimension");
        scalars = machine::checked_size_add(scalars, machine::checked_size_mul(rows, cols));
        bound(scalars, limits.max_analysis_scalars, "series analysis scalar budget");
    }
    void multiplication(std::size_t rows, std::size_t inner, std::size_t cols) {
        shape(rows, cols);
        work = machine::checked_size_add(work,
            machine::checked_size_mul(machine::checked_size_mul(rows, inner), cols));
        bound(work, limits.max_analysis_work, "series analysis work budget");
    }
    void rational(const numeric::Rational& q) const {
        for (const auto& z : {q.get_num(), q.get_den()}) {
            const auto bits = mpz_sizeinbase(z.get_mpz_t(), 2);
            bound(bits / 8 + (bits % 8 != 0 ? 1u : 0u), limits.temporal.max_integer_bytes,
                  "series analysis coefficient budget");
        }
    }
    void matrix(const numeric::ExactMatrix& m) const { for (const auto& [ij, q] : m.entries()) { (void)ij; rational(q); } }
    void vector(const numeric::RationalVector& v) const { for (const auto& q : v) rational(q); }
};
} // namespace native_series_detail

class NativeSeriesEndpoint {
public:
    const TemporalEndpoint& endpoint() const & { valid(); return *endpoint_; }
    const TemporalEndpoint& endpoint() const && = delete;
    std::shared_ptr<const TemporalEndpoint> pin() const { valid(); return endpoint_; }
    std::uint64_t sequence() const { valid(); return endpoint_->sequence; }
    std::uint64_t source_namespace() const { valid(); return namespace_; }
    NativeSeriesReplayStats stats() const { valid(); return stats_; }
private:
    void valid() const { native_series_detail::require(static_cast<bool>(endpoint_), "moved-from series endpoint pin"); }
    friend class NativeTemporalSeries;
    friend class NativeSeriesDifference;
    NativeSeriesEndpoint(std::shared_ptr<const TemporalEndpoint> p, std::uint64_t ns, NativeSeriesReplayStats stats)
        : endpoint_(std::move(p)), namespace_(ns), stats_(stats) {}
    std::shared_ptr<const TemporalEndpoint> endpoint_;
    std::uint64_t namespace_{};
    NativeSeriesReplayStats stats_;
};

struct NativeSeriesFieldEvolution {
    std::uint64_t field{}, carrier{}, first_sequence{}, last_sequence{};
    temporal::MultiTimeAccumulation accumulation;
};
struct NativeSeriesMomentRequest {
    std::uint64_t field{};
    // Optional declared map at the terminal endpoint, from the field's carrier
    // into the metric's carrier. It is not an implicit metric identification.
    std::optional<std::uint64_t> target_map;
};
struct NativeSeriesSampleRequest {
    std::uint64_t sequence{}, field{};
    std::optional<std::uint64_t> target_map;
};
enum class NativeSeriesChannelKind : std::uint8_t { InitialField, Innovation, SampledField };
struct NativeSeriesMomentChannel {
    std::uint64_t field{}, sequence{};
    NativeSeriesChannelKind kind{NativeSeriesChannelKind::InitialField};
    std::optional<std::uint64_t> target_map;
};
struct NativeSeriesMoments {
    std::uint64_t carrier{}, metric{}, terminal_sequence{};
    std::vector<NativeSeriesMomentChannel> provenance;
    std::vector<temporal::MultiTimeChannel> channels;
    numeric::ExactMatrix gram;
};

// An interval retains its exact ordered word, NOT a synthetic consecutive
// segment. Endpoint patches are expressly lossy summaries of that history.
class NativeSeriesDifference {
public:
    NativeSeriesEndpoint before() const { return get(first_); }
    NativeSeriesEndpoint after() const { return get(last_); }
    std::size_t segment_count() const { valid(); return static_cast<std::size_t>(last_ - first_); }
    std::span<const std::byte> segment_bytes(std::size_t i) const & {
        native_series_detail::require(i < segment_count(), "difference frame index");
        return state_->frames.at(native_series_detail::offset(*state_, first_) + i)->bytes;
    }
    std::span<const std::byte> segment_bytes(std::size_t) const && = delete;
    // Only same-signature source patches are admitted by this summary API.
    // Signature migrations remain available in the complete ordered frames.
    TemporalSourcePatch source_patch() const {
        auto a = before(), b = after();
        return make_temporal_source_patch(a.endpoint().source, b.endpoint().source);
    }
    numeric::ExactMatrix correspondence(std::uint64_t carrier) const {
        valid(); native_series_detail::AnalysisBudget budget{state_->limits};
        return correspondence_impl(carrier, budget);
    }
private:
    numeric::ExactMatrix correspondence_impl(std::uint64_t carrier, native_series_detail::AnalysisBudget& budget) const {
        using namespace native_series_detail;
        valid(); bound(segment_count(), state_->limits.max_analysis_steps, "series analysis interval budget");
        auto current = before().pin();
        const auto cs = temporal_endpoint_detail::carriers(*current);
        require(cs.contains(carrier), "composed correspondence requires persistent carrier");
        const auto cols = cs.at(carrier)->domain.logical_size();
        budget.shape(cols, cols);
        auto product = numeric::ExactMatrix::identity(cols);
        walk(current, [&](const TemporalEndpoint& a, const TemporalEndpoint& b, const TemporalSegment& s) {
            const auto bs = temporal_endpoint_detail::carriers(b);
            require(bs.contains(carrier), "carrier removal/rebirth has no persistent correspondence");
            const auto rows = bs.at(carrier)->domain.logical_size();
            budget.shape(rows, product.rows()); budget.multiplication(rows, product.rows(), cols);
            auto J = temporal_carrier_correspondence(a, b, carrier, s.correspondences);
            product = J.multiply(product); budget.matrix(product);
        });
        return product;
    }
public:
    NativeSeriesFieldEvolution field_evolution(std::uint64_t field) const {
        valid(); native_series_detail::AnalysisBudget budget{state_->limits};
        return field_evolution_impl(field, budget);
    }
private:
    NativeSeriesFieldEvolution field_evolution_impl(std::uint64_t field, native_series_detail::AnalysisBudget& budget) const {
        using namespace native_series_detail;
        valid(); bound(segment_count(), state_->limits.max_analysis_steps, "series field interval budget");
        auto current = before().pin();
        auto tensors = temporal_endpoint_detail::tensors(*current);
        require(tensors.contains(field) && tensors.at(field)->role == TemporalTensorRole::Field,
                "multi-time query needs an initial declared field");
        const auto carrier = tensors.at(field)->output.value();
        budget.shape(tensors.at(field)->tensor.domain().logical_size());
        auto initial = dense(tensors.at(field)->tensor.values());
        std::vector<numeric::ExactMatrix> maps;
        std::vector<numeric::RationalVector> innovations;
        walk(current, [&](const TemporalEndpoint& a, const TemporalEndpoint& b, const TemporalSegment& s) {
            const auto as = temporal_endpoint_detail::tensors(a), bs = temporal_endpoint_detail::tensors(b);
            require(bs.contains(field) && bs.at(field)->role == TemporalTensorRole::Field &&
                    bs.at(field)->output == carrier && as.at(field)->output == carrier,
                    "multi-time field identity/carrier did not persist");
            const auto cols = as.at(field)->tensor.domain().logical_size();
            const auto rows = bs.at(field)->tensor.domain().logical_size();
            budget.shape(rows, cols); budget.shape(rows); budget.multiplication(rows, cols, 1);
            auto J = temporal_carrier_correspondence(a, b, carrier, s.correspondences);
            auto eta = temporal_endpoint_detail::sparse_sum(bs.at(field)->tensor.values(),
                temporal_endpoint_detail::sparse_apply(J, as.at(field)->tensor.values()), -1);
            maps.push_back(std::move(J)); innovations.push_back(dense(eta)); budget.vector(innovations.back());
        });
        const auto terminal_size = temporal_endpoint_detail::tensors(*current).at(field)->tensor.domain().logical_size();
        budget.shape(terminal_size, terminal_size);
        for (const auto& J : maps) {
            budget.multiplication(terminal_size, J.rows(), J.cols());
            budget.multiplication(terminal_size, J.rows(), 1);
        }
        budget.multiplication(terminal_size, initial.size(), 1);
        auto accumulated = temporal::accumulate_innovations(initial, maps, innovations);
        budget.vector(accumulated.transported_initial); budget.vector(accumulated.terminal);
        for (const auto& v : accumulated.propagated_innovations) budget.vector(v);
        require(accumulated.terminal == dense(temporal_endpoint_detail::tensors(*current).at(field)->tensor.values()),
                "multi-time accumulation differs from reconstructed endpoint");
        return {field, carrier, first_, last_, std::move(accumulated)};
    }
public:
    NativeSeriesMoments moments(const std::vector<NativeSeriesMomentRequest>& requests,
                               std::uint64_t metric_key) const {
        using namespace native_series_detail;
        require(!requests.empty(), "multi-time moments need selected fields");
        valid(); bound(segment_count(), state_->limits.max_analysis_steps, "moment interval budget");
        const auto channel_count = machine::checked_size_mul(requests.size(), segment_count() + 1);
        auto target = after(); const auto ts = temporal_endpoint_detail::tensors(target.endpoint());
        require(ts.contains(metric_key) && ts.at(metric_key)->role == TemporalTensorRole::Metric,
                "multi-time moment requires a target endpoint metric");
        const auto& metric = *ts.at(metric_key); const auto carrier = metric.output.value();
        const auto dim = temporal_endpoint_detail::carriers(target.endpoint()).at(carrier)->domain.logical_size();
        AnalysisBudget budget{state_->limits}; budget.shape(channel_count, channel_count);
        budget.shape(channel_count, dim); budget.shape(dim, dim);
        budget.multiplication(channel_count, machine::checked_size_mul(dim, dim), channel_count);
        std::set<std::pair<std::uint64_t, std::optional<std::uint64_t>>> seen;
        NativeSeriesMoments out; out.carrier = carrier; out.metric = metric_key; out.terminal_sequence = last_;
        for (const auto& request : requests) {
            require(seen.emplace(request.field, request.target_map).second, "duplicate moment channel selection");
            auto evo = field_evolution_impl(request.field, budget);
            std::optional<numeric::ExactMatrix> map;
            if (request.target_map) {
                require(ts.contains(*request.target_map), "moment target map absent");
                const auto& t = *ts.at(*request.target_map);
                const bool valid_role = t.role == TemporalTensorRole::Accession || t.role == TemporalTensorRole::Reconstruction ||
                    t.role == TemporalTensorRole::PhysicalRealization || t.role == TemporalTensorRole::SourceRealization ||
                    t.role == TemporalTensorRole::Observation || t.role == TemporalTensorRole::SemanticScaleMap;
                require(valid_role && t.input == evo.carrier && t.output == carrier,
                        "moment map role or typed carriers mismatch");
                map = temporal_tensor_matrix(t);
            } else require(evo.carrier == carrier, "moment needs an explicit target carrier map");
            const auto add = [&](numeric::RationalVector value, bool initial, std::uint64_t sequence) {
                if (map) { budget.multiplication(map->rows(), map->cols(), 1); value = map->apply(value); }
                budget.vector(value);
                out.provenance.push_back({request.field, sequence, initial ? NativeSeriesChannelKind::InitialField : NativeSeriesChannelKind::Innovation, request.target_map});
                out.channels.push_back({std::to_string(request.field) + (initial ? ":initial@" : ":innovation@") +
                                        std::to_string(sequence), std::move(value)});
            };
            add(std::move(evo.accumulation.transported_initial), true, first_);
            for (std::size_t i = 0; i < evo.accumulation.propagated_innovations.size(); ++i)
                add(std::move(evo.accumulation.propagated_innovations[i]), false, first_ + i + 1);
        }
        out.gram = temporal::multi_time_channel_gram(out.channels, field::ExactMetric(temporal_tensor_matrix(metric)));
        budget.matrix(out.gram); return out;
    }
    // Source samples are transported to one declared terminal carrier before
    // pairing. Symmetric Gram entries do not assert reversible evolution or
    // statistical covariance. Sample channels remain distinct from innovations.
    NativeSeriesMoments sample_moments(const std::vector<NativeSeriesSampleRequest>& requests,
                                      std::uint64_t metric_key) const {
        using namespace native_series_detail;
        valid(); require(!requests.empty(), "sample moments need explicit channels");
        bound(segment_count(), state_->limits.max_analysis_steps, "sample moment interval budget");
        auto target = after(); const auto ts = temporal_endpoint_detail::tensors(target.endpoint());
        require(ts.contains(metric_key) && ts.at(metric_key)->role == TemporalTensorRole::Metric,
                "sample moment target metric absent");
        const auto& metric = *ts.at(metric_key); const auto carrier = metric.output.value();
        const auto dim = temporal_endpoint_detail::carriers(target.endpoint()).at(carrier)->domain.logical_size();
        AnalysisBudget budget{state_->limits}; budget.shape(requests.size(), requests.size());
        budget.shape(requests.size(), dim); budget.shape(dim, dim);
        budget.multiplication(requests.size(), machine::checked_size_mul(dim, dim), requests.size());
        std::set<std::tuple<std::uint64_t, std::uint64_t, std::optional<std::uint64_t>>> seen;
        NativeSeriesMoments out; out.carrier = carrier; out.metric = metric_key; out.terminal_sequence = last_;
        for (const auto& request : requests) {
            require(request.sequence >= first_ && request.sequence <= last_, "sample outside requested interval");
            require(seen.emplace(request.sequence, request.field, request.target_map).second, "duplicate sampled moment channel");
            auto source = get(request.sequence); const auto fields = temporal_endpoint_detail::tensors(source.endpoint());
            require(fields.contains(request.field) && fields.at(request.field)->role == TemporalTensorRole::Field,
                    "sample requires a declared field at its source sequence");
            const auto& sample = *fields.at(request.field); const auto source_carrier = sample.output.value();
            const auto J = NativeSeriesDifference(state_, request.sequence, last_).correspondence_impl(source_carrier, budget);
            budget.multiplication(J.rows(), J.cols(), 1);
            auto value = J.apply(dense(sample.tensor.values()));
            if (request.target_map) {
                require(ts.contains(*request.target_map), "sample target map absent");
                const auto& t = *ts.at(*request.target_map);
                const bool valid_role = t.role == TemporalTensorRole::Accession || t.role == TemporalTensorRole::Reconstruction ||
                    t.role == TemporalTensorRole::PhysicalRealization || t.role == TemporalTensorRole::SourceRealization ||
                    t.role == TemporalTensorRole::Observation || t.role == TemporalTensorRole::SemanticScaleMap;
                require(valid_role && t.input == source_carrier && t.output == carrier,
                        "sample map role or typed carriers mismatch");
                budget.multiplication(dim, value.size(), 1);
                value = temporal_tensor_matrix(t).apply(value);
            } else require(source_carrier == carrier, "sample needs an explicit terminal carrier map");
            budget.vector(value);
            out.provenance.push_back({request.field, request.sequence, NativeSeriesChannelKind::SampledField, request.target_map});
            out.channels.push_back({std::to_string(request.field) + ":sample@" + std::to_string(request.sequence), std::move(value)});
        }
        out.gram = temporal::multi_time_channel_gram(out.channels, field::ExactMetric(temporal_tensor_matrix(metric)));
        budget.matrix(out.gram); return out;
    }
private:
    friend class NativeTemporalSeries;
    NativeSeriesDifference(std::shared_ptr<const native_series_detail::State> s, std::uint64_t first, std::uint64_t last)
        : state_(std::move(s)), first_(first), last_(last) {}
    void valid() const { native_series_detail::require(static_cast<bool>(state_), "moved-from series interval"); }
    NativeSeriesEndpoint get(std::uint64_t sequence) const {
        valid(); NativeSeriesReplayStats stats;
        auto e = native_series_detail::reconstruct(*state_, sequence, stats);
        return {std::move(e), state_->name_space, stats};
    }
    template<class F> void walk(std::shared_ptr<const TemporalEndpoint>& current, F&& f) const {
        for (auto i = native_series_detail::offset(*state_, first_); current->sequence < last_; ++i) {
            const auto& segment = state_->frames.at(i)->segment;
            auto next = std::make_shared<const TemporalEndpoint>(replay_temporal_segment(
                *current, segment, state_->limits.temporal.source_declarations));
            f(*current, *next, segment); current = std::move(next);
        }
    }
    std::shared_ptr<const native_series_detail::State> state_;
    std::uint64_t first_{}, last_{};
};

class NativeTemporalSeries {
public:
    explicit NativeTemporalSeries(const TemporalEndpoint& base, std::uint64_t source_namespace,
                                  NativeSeriesLimits limits = {}) {
        native_series_detail::require(source_namespace != 0, "series namespace required");
        auto s = std::make_shared<native_series_detail::State>(); s->name_space = source_namespace; s->limits = limits;
        s->base = native_series_detail::snapshot(base, limits); s->tail = s->base->endpoint;
        s->archive_bytes = native_series_detail::archive_size(*s); state_ = std::move(s);
    }
    // Copies are cheap immutable history forks. Mutation publishes a new state.
    std::uint64_t first_sequence() const { return state().base->endpoint->sequence; }
    std::uint64_t last_sequence() const { return state().tail->sequence; }
    std::uint64_t source_namespace() const { return state().name_space; }
    std::size_t segment_count() const { return state().frames.size(); }
    std::size_t archive_bytes() const { return state().archive_bytes; }
    NativeSeriesLimits limits() const { return state().limits; }
    // Visit the base and each retained endpoint in order, replaying each frame
    // once. Pins and the selected history survive callback publication changes.
    template<class Function> void visit_endpoints(Function&& function) const {
        (void)state(); const auto owner = state_;
        auto current = owner->base->endpoint;
        function(NativeSeriesEndpoint{current, owner->name_space, {current->sequence, 0, false}});
        owner->frames.for_each([&](const auto& frame) {
            const auto previous = current->sequence;
            current = std::make_shared<const TemporalEndpoint>(replay_temporal_segment(
                *current, frame->segment, owner->limits.temporal.source_declarations));
            function(NativeSeriesEndpoint{current, owner->name_space, {previous, 1, false}});
        });
    }
    // Components retain their immutable owner independently of the series and
    // callback. Their sequence/kind identity is separate from a physical digest.
    template<class Function> void visit_components(Function&& function) const {
        (void)state();const auto owner=state_;
        const auto base=owner->base;
        function(NativeSeriesComponent{NativeSeriesComponentKind::BaseSource,owner->name_space,base->endpoint->sequence,{base,&base->source}});
        function(NativeSeriesComponent{NativeSeriesComponentKind::BaseChannels,owner->name_space,base->endpoint->sequence,base->channels});
        auto sequence=base->endpoint->sequence;
        owner->frames.for_each([&](const auto& frame) {
            function(NativeSeriesComponent{NativeSeriesComponentKind::Segment,owner->name_space,++sequence,{frame,&frame->bytes}});
        });
        for(const auto& [seq,checkpoint]:*owner->checkpoints) {
            function(NativeSeriesComponent{NativeSeriesComponentKind::CheckpointSource,owner->name_space,seq,{checkpoint,&checkpoint->source}});
            function(NativeSeriesComponent{NativeSeriesComponentKind::CheckpointChannels,owner->name_space,seq,checkpoint->channels});
        }
    }
    NativeSeriesStorageStats storage_stats() const {
        const auto stats = state().frames.stats();
        return {stats.reachable_pages, stats.last_append_pages, stats.last_append_pointer_copies,
                state().checkpoints->size(), stats.levels};
    }
    bool same_history(const NativeTemporalSeries& other) const {
        const auto& a = state(); const auto& b = other.state();
        if (a.name_space != b.name_space || a.base->source != b.base->source || *a.base->channels != *b.base->channels ||
            a.frames.size() != b.frames.size()) return false;
        for (std::size_t i = 0; i < a.frames.size(); ++i) if (a.frames.at(i)->bytes != b.frames.at(i)->bytes) return false;
        return true; // checkpoint schedules are physical, not temporal identity
    }
    NativeSeriesEndpoint tail() const { return {state().tail, state().name_space, {last_sequence(), 0, false}}; }
    NativeSeriesEndpoint at(std::uint64_t sequence, bool from_base = false) const {
        NativeSeriesReplayStats stats;
        auto p = native_series_detail::reconstruct(state(), sequence, stats, from_base);
        return {std::move(p), state().name_space, stats};
    }
    std::vector<std::uint64_t> checkpoint_sequences() const {
        std::vector<std::uint64_t> out;
        for (const auto& [seq, cp] : *state().checkpoints) { (void)cp; out.push_back(seq); }
        return out;
    }
    // Canonical frames remain individually addressable after checkpoint repacking.
    std::span<const std::byte> segment_bytes(std::size_t index) const & { return state().frames.at(index)->bytes; }
    std::span<const std::byte> segment_bytes(std::size_t) const && = delete;
    NativeSeriesDifference difference(std::uint64_t first, std::uint64_t last) const {
        (void)native_series_detail::offset(state(), first); (void)native_series_detail::offset(state(), last);
        native_series_detail::require(first <= last, "reverse difference requires declared inverse history");
        return {state_, first, last};
    }
    void append(const TemporalSegment& segment) {
        // Only this fresh encoder result can transfer its allocation. Public
        // borrowed frames retain their defensive copy after full admission.
        auto bytes=encode_temporal_segment(segment);
        const auto view=std::span<const std::byte>(bytes);
        append_frame(view,std::move(bytes));
    }
    // Release the consumed segment before canonical decode and replay.
    void append(TemporalSegment&& segment) {
        auto bytes=[&] {
            auto owned=std::move(segment);
            return encode_temporal_segment(owned);
        }();
        const auto view=std::span<const std::byte>(bytes);
        append_frame(view,std::move(bytes));
    }
    // Build against the owned tail, decode the canonical frame and replay it
    // before publication. A failed append retains the complete previous state.
    void append_target(const TemporalEndpoint& target,
            std::vector<TemporalCorrespondence> correspondences = {},
            std::vector<RelationalCodeTransport> code_transports = {},
            RelationalCodeEncoding code_encoding = RelationalCodeEncoding::Automatic,
            std::optional<SourceDeclarationMigration> source_migration = {},
            std::optional<TemporalDeclarationUpdate> declaration_update = {}) {
        const auto& old = state();
        native_series_detail::bound(machine::checked_size_add(old.frames.size(), 1),
                                   old.limits.max_segments, "series segment count budget");
        auto segment = temporal_segment_detail::build_temporal_segment(*old.tail, target,
            std::move(correspondences), std::move(code_transports), code_encoding,
            std::move(source_migration), std::move(declaration_update), old.limits.temporal.source_declarations);
        append(std::move(segment));
    }
    void append_encoded(std::span<const std::byte> frame) { append_frame(frame,{}); }
private:
    void append_frame(std::span<const std::byte> frame,std::optional<native_series_detail::Bytes> owned) {
        using namespace native_series_detail;
        require(!owned || (owned->data()==frame.data() && owned->size()==frame.size()),
                "fresh frame owner differs from its complete component");
        const auto& old = state();
        bound(machine::checked_size_add(old.frames.size(), 1), old.limits.max_segments, "series segment count budget");
        bound(add_blob(old.archive_bytes, frame.size()), old.limits.max_archive_bytes, "series append byte budget");
        auto decoded = decode_temporal_segment(frame, policy(old.limits));
        auto next_endpoint = std::make_shared<const TemporalEndpoint>(replay_temporal_segment(
            *old.tail, decoded, old.limits.temporal.source_declarations));
        auto next = std::make_shared<State>(old);
        next->frames = old.frames.appended(std::make_shared<const Frame>(Frame{std::move(decoded), owned?std::move(*owned):Bytes(frame.begin(), frame.end())}));
        next->frame_bytes = add_blob(old.frame_bytes, frame.size());
        next->tail = std::move(next_endpoint); next->archive_bytes = archive_size(*next);
        state_ = std::move(next);
    }
    // Validate an entire concatenated RFCXT stream before publishing any prefix.
public:
    void append_stream(std::span<const std::byte> bytes, std::size_t chunk_bytes = 65536) {
        using namespace native_series_detail;
        require(chunk_bytes != 0, "series stream chunk size must be positive");
        bound(bytes.size(), state().limits.max_archive_bytes, "series input stream byte budget");
        NativeTemporalSeries candidate = *this;
        TemporalSegmentStreamDecoder decoder(policy(state().limits));
        for (std::size_t pos = 0; pos < bytes.size();) {
            const auto n = std::min(chunk_bytes, bytes.size() - pos);
            decoder.feed(bytes.subspan(pos, n), [&](const TemporalSegment&, std::span<const std::byte> frame) { candidate.append_encoded(frame); });
            pos += n;
        }
        decoder.finish(); state_ = std::move(candidate.state_);
    }
    void checkpoint(std::uint64_t sequence) {
        auto selected = checkpoint_sequences(); selected.push_back(sequence);
        std::sort(selected.begin(), selected.end()); selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
        repack_checkpoints(selected);
    }
    // Physical compaction only: replace the acceleration schedule, retaining all
    // original source/channel bytes and EVERY complete ordered segment unchanged.
    void repack_checkpoints(const std::vector<std::uint64_t>& sequences) {
        using namespace native_series_detail;
        const auto& old = state(); bound(sequences.size(), old.limits.max_checkpoints, "series checkpoint count budget");
        std::set<std::uint64_t> selected;
        for (auto seq : sequences) {
            (void)offset(old, seq); require(seq != first_sequence(), "base is already the initial checkpoint");
            require(selected.insert(seq).second, "duplicate checkpoint sequence");
        }
        auto next = std::make_shared<State>(old);
        auto checkpoints = std::make_shared<State::Checkpoints>(); next->checkpoint_bytes = 0;
        for (auto seq : selected) {
            if (const auto it = old.checkpoints->find(seq); it != old.checkpoints->end()) checkpoints->emplace(seq, it->second);
            else {
                auto endpoint = at(seq);
                checkpoints->emplace(seq, snapshot(endpoint.endpoint(), old.limits));
            }
            const auto& cp = *checkpoints->at(seq);
            next->checkpoint_bytes = machine::checked_size_add(next->checkpoint_bytes, 8);
            next->checkpoint_bytes = add_blob(next->checkpoint_bytes, cp.source.size());
            next->checkpoint_bytes = add_blob(next->checkpoint_bytes, cp.channels->size());
            next->archive_bytes = archive_size(*next);
        }
        next->checkpoints = std::move(checkpoints);
        next->archive_bytes = archive_size(*next); state_ = std::move(next);
    }
private:
    friend class NativeSeriesPrefix;
    friend std::vector<std::byte> encode_native_series(const NativeTemporalSeries&);
    friend NativeTemporalSeries decode_native_series(std::span<const std::byte>, const NativeSeriesLimits&);
    // Only the canonical reader adopts storage it freshly decoded and owns.
    // Public host endpoints still pass through the independently freezing path.
    explicit NativeTemporalSeries(std::shared_ptr<const native_series_detail::State> state)
        : state_(std::move(state)) {}
    const native_series_detail::State& state() const {
        native_series_detail::require(static_cast<bool>(state_), "moved-from temporal series"); return *state_;
    }
    std::shared_ptr<const native_series_detail::State> state_;
};

} // namespace rfc::storage
