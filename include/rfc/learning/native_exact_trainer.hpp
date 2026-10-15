#pragma once
#include "rfc/learning/native_metric_layer.hpp"
#include "rfc/storage/native_series_codec.hpp"

namespace rfc::learning {
enum class NativeTrainingObjective { Unspecified, HalfMetricSquaredError };
enum class NativeSamplePartition { Training, Evaluation };
struct NativeExactTrainingSample {
    storage::PinnedNativeAccession input, target;
    NativeSamplePartition partition{NativeSamplePartition::Training};
    std::uint64_t event_time{}, knowledge_time{}, label_available_time{};
};
struct NativeExactTrainingContract {
    NativeTrainingObjective objective{NativeTrainingObjective::Unspecified};
    NativeMetricLayerBinding model;
    std::uint64_t history_namespace{}, metadata_carrier{}, metadata_tensor{}, time_coordinates{};
    std::uint64_t initial_parameter_knowledge_time{};
    std::uint64_t publication_cutoff{};
    std::uint64_t metadata_coordinates{};
    std::size_t metadata_capacity{4096};
};

// A deterministic, serial exact gradient profile on the original ordered
// metric channels. The caller declares 1/2 Q_M(prediction-target), step sizes,
// sample order, time coordinates and label availability. There is no PRNG,
// momentum, Boolean derivative, parameter pooling or automatic assertion.
class NativeExactTrainer {
    struct State;
    struct Identity {};
    struct Statistics {
        std::size_t sample{}, bits{};
        numeric::RationalVector values;
    };
  public:
    // Exclusive physical cache for fixed source responses and metric pairings.
    // Duplicate responses share storage. Hodge uses its down and up responses
    // when both are declared. Gradients retain the original parameter coordinates.
    class TrainingWorkspace {
      public:
        std::size_t capacity() const noexcept { return entries_.size(); }
        std::size_t cached_samples() const noexcept {
            return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
                                                        [](const auto& entry) { return bool(entry); }));
        }
        std::size_t retained_slots() const {
            std::size_t slots = 0;
            for (const auto& entry : entries_)
                if (entry) slots = machine::checked_size_add(slots, entry->values.size());
            return slots;
        }
        std::size_t hits() const noexcept { return hits_; }
        std::size_t misses() const noexcept { return misses_; }
        std::size_t response_count() const noexcept { return channels_.size() + 1; }
        const std::vector<ExactMetricChannel>& response_channels() const noexcept { return channels_; }
        const std::vector<std::vector<std::size_t>>& parameter_coordinates() const noexcept { return coordinates_; }
        const runtime::ExecutionRequirements& last_requirements() const noexcept { return last_; }
      private:
        friend class NativeExactTrainer;
        TrainingWorkspace(std::shared_ptr<const State> owner, std::size_t capacity)
            : owner_(std::move(owner)), entries_(capacity) {
            const auto contains = [&](ExactMetricChannelKind kind) {
                return std::any_of(owner_->channels.begin(), owner_->channels.end(),
                                   [&](const auto& channel) { return channel.kind == kind; });
            };
            const bool split_hodge = contains(ExactMetricChannelKind::HodgeDown) &&
                                     contains(ExactMetricChannelKind::HodgeUp);
            for (const auto kind : {ExactMetricChannelKind::Identity, ExactMetricChannelKind::HodgeDown,
                                    ExactMetricChannelKind::HodgeUp, ExactMetricChannelKind::Hodge})
                if (contains(kind) && !(split_hodge && kind == ExactMetricChannelKind::Hodge))
                    channels_.push_back({kind, numeric::Rational{0}});
            const auto index = [&](ExactMetricChannelKind kind) {
                const auto found = std::find_if(channels_.begin(), channels_.end(),
                                               [&](const auto& channel) { return channel.kind == kind; });
                if (found == channels_.end()) throw std::logic_error("training response coordinate absent");
                return static_cast<std::size_t>(found - channels_.begin());
            };
            std::vector<std::size_t> terms(response_count(), 0);
            for (const auto& channel : owner_->channels) {
                if (split_hodge && channel.kind == ExactMetricChannelKind::Hodge)
                    coordinates_.push_back({index(ExactMetricChannelKind::HodgeDown),
                                            index(ExactMetricChannelKind::HodgeUp)});
                else coordinates_.push_back({index(channel.kind)});
                for (const auto coordinate : coordinates_.back()) ++terms[coordinate];
            }
            coordinates_.push_back({channels_.size()});
            terms.back() = 1;
            max_fold_terms_ = *std::max_element(terms.begin(), terms.end());
        }
        std::shared_ptr<const State> owner_;
        std::vector<std::unique_ptr<Statistics>> entries_;
        std::vector<ExactMetricChannel> channels_;
        std::vector<std::vector<std::size_t>> coordinates_;
        std::size_t max_fold_terms_{};
        std::size_t hits_{}, misses_{};
        runtime::ExecutionRequirements last_{};
    };
    TrainingWorkspace make_training_workspace(std::size_t capacity,
            const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        using namespace inference::native_field_detail;
        const auto pin = state_;
        require(pin->tower.profile() == field::PreparedExactProfile::FiniteActions,
                "prepared training requires the finite metric profile");
        require(capacity && capacity <= pin->samples.size(), "training workspace capacity mismatch");
        lease.check_cancelled();
        TrainingWorkspace workspace(pin, capacity);
        auto required = pin->tower.requirements(1, field::OperatorKind::Hodge);
        required.records = add(required.records, capacity);
        required.records = add(required.records, mul(pin->parameters.size(), 3));
        required.rational_slots = add(required.rational_slots, mul(capacity, statistics_slots(workspace.response_count())));
        required.scalar_work = 0;
        runtime::admit_execution(required, lease.policy());
        return workspace;
    }
    static NativeExactTrainer
    prepare(inference::PreparedNativeExactTower tower, core::Grade grade,
            std::vector<ExactMetricChannel> channels, storage::PinnedNativeAccession gains,
            storage::PinnedNativeAccession bias, std::vector<NativeExactTrainingSample> samples,
            NativeExactTrainingContract contract, const runtime::ExecutionHostPolicy &policy = {}) {
        using namespace inference::native_field_detail;
        require(contract.objective == NativeTrainingObjective::HalfMetricSquaredError,
                "unknown exact training objective");
        require(contract.history_namespace &&
                    contract.history_namespace != tower.publication().source_namespace() &&
                    contract.metadata_carrier && contract.metadata_tensor && contract.time_coordinates &&
                    contract.metadata_coordinates &&
                    contract.metadata_coordinates != contract.time_coordinates,
                "training requires separate declared history, time and checkpoint byte coordinates");
        require(!samples.empty() && samples.size() <= 1024, "exact training sample count limit");
        require(contract.metadata_capacity >= 8 && contract.metadata_capacity <= 65536,
                "training checkpoint capacity must be between 8 and 65536 bytes");
        require(tower.publication().sequence() <= contract.publication_cutoff,
                "future source publication is unavailable for training");
        // Reuse the canonical parameter and ordered-channel binding checks.
        (void)PreparedNativeMetricLayer(tower, grade, channels, gains, bias, contract.model, policy);
        for (const auto &sample : samples) {
            require(sample.partition == NativeSamplePartition::Training ||
                        sample.partition == NativeSamplePartition::Evaluation,
                    "unknown training partition");
            tower.validate_input(sample.input, grade);
            tower.validate_input(sample.target, grade);
        }
        auto state =
            std::make_shared<State>(std::move(tower), grade, std::move(channels), std::move(gains),
                                    std::move(bias), std::move(samples), std::move(contract), policy);
        return NativeExactTrainer(std::move(state));
    }
    // Functional publication: failed/cancelled updates leave this owner and all
    // previous checkpoints unchanged. Independent callers own independent tails.
    NativeExactTrainer advance(const numeric::Rational &rate, std::uint64_t event_cutoff,
                               std::uint64_t knowledge_cutoff,
                               const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) const {
        using namespace inference::native_field_detail;
        validate_learning_rational(rate);
        require(rate > 0, "exact training step size must be positive");
        lease.check_cancelled();
        const auto pin = state_;
        const auto step = pin->history.last_sequence();
        if (step >= 1024)
            throw std::length_error("exact training retained step limit");
        const auto &sample = pin->samples[static_cast<std::size_t>(step % pin->samples.size())];
        require(sample.partition == NativeSamplePartition::Training,
                "evaluation labels cannot update fitted parameters");
        require(sample.event_time <= event_cutoff && sample.knowledge_time <= knowledge_cutoff &&
                    sample.label_available_time <= knowledge_cutoff &&
                    pin->parameter_knowledge_time <= knowledge_cutoff,
                "future event, knowledge, label or fitted parameter is unavailable");
        auto required = pin->tower.requirements(
            std::max(tensor_bits(*sample.input.tensor()), tensor_bits(*sample.target.tensor())),
            field::OperatorKind::Hodge);
        required.rational_slots = add(required.rational_slots,
                                      mul(pin->tower.grade_size(pin->grade), add(pin->channels.size(), 12)));
        required.scalar_work = mul(required.scalar_work, add(pin->channels.size(), 16));
        std::size_t parameter_bits = integer_bits(rate);
        for (const auto &q : pin->parameters)
            parameter_bits = std::max(parameter_bits, integer_bits(q));
        required.integer_bits = sum_bound(mul(add(required.integer_bits, parameter_bits), 4),
                                          add(pin->channels.size(), pin->tower.grade_size(pin->grade)));
        runtime::admit_execution(required, lease.policy());
        if (required.scalar_work > pin->tower.publication().limits().max_action_work)
            throw std::length_error("native training accession work limit");
        const auto x = pin->tower.input_values(sample.input, pin->grade),
                   target = pin->tower.input_values(sample.target, pin->grade);
        std::vector<numeric::RationalVector> responses;
        responses.reserve(pin->channels.size());
        numeric::RationalVector prediction(x.size(), pin->parameters.back());
        for (std::size_t c = 0; c < pin->channels.size(); ++c) {
            lease.check_cancelled();
            const auto response =
                pin->channels[c].kind == ExactMetricChannelKind::Identity
                    ? x
                    : pin->tower.tower().apply(
                          {operation(pin->channels[c]), pin->grade, pin->channels[c].parameter}, x);
            for (std::size_t i = 0; i < x.size(); ++i)
                prediction[i] += pin->parameters[c] * response[i];
            responses.push_back(response);
        }
        const auto error = numeric::sub(prediction, target);
        const auto &metric = pin->tower.tower().metrics().at(pin->grade.value);
        const auto measured = metric.apply(error);
        const numeric::Rational loss = numeric::dot(error, measured) / 2;
        auto parameters = pin->parameters;
        for (std::size_t c = 0; c < responses.size(); ++c)
            parameters[c] -= rate * numeric::dot(responses[c], measured);
        numeric::Rational bias_gradient = 0;
        for (const auto &q : measured)
            bias_gradient += q;
        parameters.back() -= rate * bias_gradient;
        for (const auto &q : parameters)
            if (integer_bits(q) > required.integer_bits)
                throw std::runtime_error("exact training integer envelope violated");
        return publish_step(pin, std::move(parameters), loss, rate, event_cutoff, knowledge_cutoff, lease);
    }
    NativeExactTrainer advance_prepared(const numeric::Rational& rate, std::uint64_t event_cutoff,
            std::uint64_t knowledge_cutoff, TrainingWorkspace& workspace,
            const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        using namespace inference::native_field_detail;
        validate_learning_rational(rate);
        require(rate > 0, "exact training step size must be positive");
        lease.check_cancelled();
        const auto pin = state_;
        require(workspace.owner_ && workspace.capacity() && workspace.owner_->identity == pin->identity,
                "training workspace belongs to another lineage");
        const auto step = pin->history.last_sequence();
        if (step >= 1024) throw std::length_error("exact training retained step limit");
        const auto sample_index = static_cast<std::size_t>(step % pin->samples.size());
        const auto& sample = pin->samples[sample_index];
        require(sample.partition == NativeSamplePartition::Training, "evaluation labels cannot update fitted parameters");
        require(sample.event_time <= event_cutoff && sample.knowledge_time <= knowledge_cutoff &&
                    sample.label_available_time <= knowledge_cutoff && pin->parameter_knowledge_time <= knowledge_cutoff,
                "future event, knowledge, label or fitted parameter is unavailable");
        const auto slot = sample_index % workspace.capacity();
        const bool hit = workspace.entries_[slot] && workspace.entries_[slot]->sample == sample_index;
        std::unique_ptr<Statistics> candidate;
        std::size_t cold_work = 0, cold_slots = 0, cold_bits = 0;
        if (!hit) {
            const auto cold = statistics_requirements(*pin, sample, workspace);
            admit_training(cold, *pin, lease);
            candidate = make_statistics(*pin, sample, sample_index, workspace, cold.integer_bits, lease);
            cold_work = cold.scalar_work;
            cold_slots = cold.rational_slots;
            cold_bits = cold.integer_bits;
        }
        const auto& statistics = hit ? *workspace.entries_[slot] : *candidate;
        const auto d = pin->parameters.size();
        const auto r = workspace.response_count();
        std::size_t parameter_bits = 1;
        for (const auto& value : pin->parameters) parameter_bits = std::max(parameter_bits, integer_bits(value));
        const auto folded_bits = workspace.max_fold_terms_ == 1 ? parameter_bits :
                                 sum_bound(parameter_bits, workspace.max_fold_terms_);
        const auto row_bits = sum_bound(add(statistics.bits, folded_bits), r);
        const auto gradient_bits = sum_bound(add(add(row_bits, statistics.bits), 1), 2);
        const auto update_bits = add(add(add(parameter_bits, integer_bits(rate)), gradient_bits), 1);
        const auto loss_bits = add(add(sum_bound(add(folded_bits, row_bits), r),
                                       sum_bound(add(folded_bits, statistics.bits), r)), add(statistics.bits, 4));
        auto required = pin->tower.requirements(1, field::OperatorKind::Hodge);
        required.records = add(required.records, add(workspace.capacity(), mul(d, 3)));
        required.rational_slots = add(required.rational_slots,
            add(workspace.retained_slots(), add(hit ? 0 : statistics.values.size(), add(add(d, mul(r, 2)), 6))));
        required.rational_slots = std::max(required.rational_slots, cold_slots);
        required.scalar_work = add(cold_work, add(mul(mul(r, r), 4), add(add(mul(r, 16), mul(d, 8)), 8)));
        required.integer_bits = std::max({required.integer_bits, cold_bits, folded_bits, row_bits, gradient_bits, update_bits, loss_bits});
        admit_training(required, *pin, lease);
        numeric::RationalVector folded(r, numeric::Rational{0}), gradients(r);
        for (std::size_t c = 0; c < d; ++c)
            for (const auto coordinate : workspace.coordinates_[c]) folded[coordinate] += pin->parameters[c];
        for (const auto& value : folded)
            if (integer_bits(value) > folded_bits) throw std::runtime_error("training folded parameter envelope violated");
        auto parameters = pin->parameters;
        numeric::Rational quadratic = 0, cross = 0;
        for (std::size_t c = 0; c < r; ++c) {
            lease.check_cancelled();
            numeric::Rational row = 0;
            for (std::size_t j = 0; j < r; ++j) row += statistics.values[c*r+j] * folded[j];
            const auto& linear = statistics.values[r*r+c];
            quadratic += folded[c] * row;
            cross += folded[c] * linear;
            gradients[c] = row - linear;
        }
        for (std::size_t c = 0; c < d; ++c) {
            numeric::Rational gradient = 0;
            for (const auto coordinate : workspace.coordinates_[c]) gradient += gradients[coordinate];
            if (integer_bits(gradient) > gradient_bits) throw std::runtime_error("training gradient envelope violated");
            parameters[c] -= rate * gradient;
        }
        const numeric::Rational loss = (quadratic - 2*cross + statistics.values.back()) / 2;
        require(loss >= 0, "prepared metric loss is negative");
        if (integer_bits(loss) > required.integer_bits)
            throw std::runtime_error("prepared training loss envelope violated");
        for (const auto& value : parameters)
            if (integer_bits(value) > required.integer_bits)
                throw std::runtime_error("prepared training parameter envelope violated");
        auto result = publish_step(pin, std::move(parameters), loss, rate, event_cutoff, knowledge_cutoff, lease);
        if (hit) ++workspace.hits_;
        else { workspace.entries_[slot] = std::move(candidate); ++workspace.misses_; }
        workspace.last_ = required;
        return result;
    }
    storage::NativeTemporalSeries history() const { return state_->history; }
    numeric::RationalVector parameters() const { return state_->parameters; }
    std::uint64_t parameter_knowledge_time() const { return state_->parameter_knowledge_time; }
    std::optional<numeric::Rational> last_loss() const { return state_->last_loss; }

  private:
    static NativeExactTrainer publish_step(const std::shared_ptr<const State>& pin,
            numeric::RationalVector parameters, const numeric::Rational& loss, const numeric::Rational& rate,
            std::uint64_t event_cutoff, std::uint64_t knowledge_cutoff, const runtime::ExecutionLease& lease) {
        const auto step = pin->history.last_sequence();
        auto next = std::make_shared<State>(*pin);
        next->parameters = std::move(parameters);
        next->parameter_knowledge_time = knowledge_cutoff;
        next->last_loss = loss;
        const auto old = pin->history.tail();
        auto endpoint = old.endpoint();
        endpoint.sequence = step + 1;
        update_field(endpoint, pin->gains.tensor_declaration()->key,
                     {next->parameters.begin(), next->parameters.end() - 1});
        update_field(endpoint, pin->bias.tensor_declaration()->key, {next->parameters.back()});
        update_metadata(endpoint, *next, step + 1, rate, event_cutoff, knowledge_cutoff);
        std::vector<storage::TemporalCorrespondence> maps;
        for (const auto &carrier : endpoint.carriers)
            maps.push_back({carrier.key, numeric::ExactMatrix::identity(carrier.domain.logical_size())});
        next->history.append_target(endpoint, std::move(maps));
        lease.check_cancelled();
        return NativeExactTrainer(std::move(next));
    }
  public:
    static NativeExactTrainer resume(NativeExactTrainer initial, std::span<const std::byte> archive,
                                     const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) {
        return resume_impl(std::move(initial), archive, nullptr, lease);
    }
    static NativeExactTrainer resume_prepared(NativeExactTrainer initial, std::span<const std::byte> archive,
            TrainingWorkspace& workspace, const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) {
        return resume_impl(std::move(initial), archive, &workspace, lease);
    }

  private:
    static NativeExactTrainer resume_impl(NativeExactTrainer initial, std::span<const std::byte> archive,
            TrainingWorkspace* workspace, const runtime::ExecutionLease& lease) {
        using namespace inference::native_field_detail;
        lease.check_cancelled();
        if (workspace) require(workspace->owner_ && workspace->capacity() &&
            workspace->owner_->identity == initial.state_->identity, "training workspace belongs to another lineage");
        require(initial.state_->history.last_sequence() == 0,
                "resume requires the declared initial parameter state");
        const auto history = storage::decode_native_series(archive);
        require(history.source_namespace() == initial.state_->contract.history_namespace &&
                    history.first_sequence() == 0 && history.last_sequence() <= 1024,
                "training resume namespace/step mismatch");
        const auto first = history.at(0), expected = initial.state_->history.at(0);
        require(
            storage::encode_temporal_endpoint(first.endpoint()) ==
                storage::encode_temporal_endpoint(expected.endpoint()),
            "training resume changed source, original parameters, channel order, samples or declarations");
        history.visit_endpoints([&](const storage::NativeSeriesEndpoint& endpoint) {
            lease.check_cancelled();
            const auto step = endpoint.sequence();
            if (step == 0) return;
            const auto tensor = storage::temporal_endpoint_detail::tensors(endpoint.endpoint())
                                    .at(initial.state_->contract.metadata_tensor);
            const auto bytes = metadata_bytes(*tensor, initial.state_->contract.metadata_capacity);
            storage::rfcx_detail::Reader reader(bytes);
            const auto payload = reader.slice(static_cast<std::size_t>(reader.u64()));
            storage::rfcx_detail::Reader metadata(payload);
            const auto prefix = metadata.string();
            require(prefix == "native-exact-gradient-step-v1", "unknown training resume profile");
            const auto saved_step = metadata.u64();
            const auto rate = storage::rfcx_detail::read_rational(metadata);
            const auto events = metadata.u64(), knowledge = metadata.u64();
            require(saved_step == step, "training step order mismatch");
            initial = workspace ? initial.advance_prepared(rate, events, knowledge, *workspace, lease)
                                : initial.advance(rate, events, knowledge, lease);
            const auto produced = initial.state_->history.tail();
            require(storage::encode_temporal_endpoint(produced.endpoint()) ==
                        storage::encode_temporal_endpoint(endpoint.endpoint()),
                    "training resume update or complete optimizer/data-order state mismatch");
        });
        require(initial.state_->history.same_history(history),
                "training resume changed ordered native operation history");
        return initial;
    }

  private:
    struct State {
        std::shared_ptr<const Identity> identity{std::make_shared<const Identity>()};
        inference::PreparedNativeExactTower tower;
        core::Grade grade;
        std::vector<ExactMetricChannel> channels;
        storage::PinnedNativeAccession gains, bias;
        std::vector<NativeExactTrainingSample> samples;
        NativeExactTrainingContract contract;
        runtime::ExecutionHostPolicy policy;
        numeric::RationalVector parameters;
        std::uint64_t parameter_knowledge_time;
        std::optional<numeric::Rational> last_loss;
        storage::NativeTemporalSeries history;
        State(inference::PreparedNativeExactTower t, core::Grade g, std::vector<ExactMetricChannel> c,
              storage::PinnedNativeAccession a, storage::PinnedNativeAccession b,
              std::vector<NativeExactTrainingSample> data, NativeExactTrainingContract declaration,
              runtime::ExecutionHostPolicy p)
            : tower(std::move(t)), grade(g), channels(std::move(c)), gains(std::move(a)), bias(std::move(b)),
              samples(std::move(data)), contract(std::move(declaration)), policy(std::move(p)),
              parameters(inference::native_field_detail::dense(*gains.tensor())),
              parameter_knowledge_time(contract.initial_parameter_knowledge_time),
              history(make_base(*this), contract.history_namespace) {}
    };
    static std::size_t statistics_slots(std::size_t parameters) {
        using namespace inference::native_field_detail;
        return add(mul(parameters, parameters), add(parameters, 1));
    }
    static void admit_training(const runtime::ExecutionRequirements& required, const State& state,
                                const runtime::ExecutionLease& lease) {
        lease.check_cancelled();
        runtime::admit_execution(required, lease.policy());
        if (required.scalar_work > state.tower.publication().limits().max_action_work)
            throw std::length_error("native training accession work limit");
    }
    static runtime::ExecutionRequirements statistics_requirements(const State& state,
            const NativeExactTrainingSample& sample, const TrainingWorkspace& workspace) {
        using namespace inference::native_field_detail;
        auto required = state.tower.requirements(std::max(tensor_bits(*sample.input.tensor()),
            tensor_bits(*sample.target.tensor())), field::OperatorKind::Hodge);
        const auto n = state.tower.grade_size(state.grade), d = workspace.response_count();
        required.records = add(required.records, add(workspace.capacity(), mul(state.parameters.size(), 3)));
        required.rational_slots = add(required.rational_slots, add(workspace.retained_slots(),
            add(statistics_slots(d), mul(n, add(d, 4)))));
        required.scalar_work = add(mul(required.scalar_work, add(workspace.channels_.size(), 16)),
            mul(n, add(mul(mul(d, d), 4), mul(d, 8))));
        required.integer_bits = sum_bound(mul(required.integer_bits, 3), n);
        return required;
    }
    static std::unique_ptr<Statistics> make_statistics(const State& state,
            const NativeExactTrainingSample& sample, std::size_t sample_index,
            const TrainingWorkspace& workspace, std::size_t bit_limit,
            const runtime::ExecutionLease& lease) {
        using namespace inference::native_field_detail;
        const auto x = state.tower.input_values(sample.input, state.grade);
        const auto target = state.tower.input_values(sample.target, state.grade);
        const auto d = workspace.response_count();
        std::vector<numeric::RationalVector> responses;
        responses.reserve(d);
        for (const auto& channel : workspace.channels_) {
            lease.check_cancelled();
            responses.push_back(channel.kind == ExactMetricChannelKind::Identity ? x :
                state.tower.tower().apply({operation(channel), state.grade, channel.parameter}, x));
        }
        responses.emplace_back(x.size(), numeric::Rational{1});
        const auto& metric = state.tower.tower().metrics().at(state.grade.value);
        const auto measured_target = metric.apply(target);
        auto statistics = std::make_unique<Statistics>();
        statistics->sample = sample_index;
        statistics->values.resize(statistics_slots(d));
        for (std::size_t c = 0; c < d; ++c) {
            lease.check_cancelled();
            const auto measured = metric.apply(responses[c]);
            for (std::size_t j = 0; j < d; ++j)
                statistics->values[j*d+c] = numeric::dot(responses[j], measured);
            statistics->values[d*d+c] = numeric::dot(responses[c], measured_target);
        }
        statistics->values.back() = numeric::dot(target, measured_target);
        statistics->bits = 1;
        for (const auto& value : statistics->values) statistics->bits = std::max(statistics->bits, integer_bits(value));
        if (statistics->bits > bit_limit) throw std::runtime_error("training statistics integer envelope violated");
        lease.check_cancelled();
        return statistics;
    }
    static field::OperatorKind operation(const ExactMetricChannel &channel) {
        switch (channel.kind) {
        case ExactMetricChannelKind::HodgeDown:
            return field::OperatorKind::HodgeDown;
        case ExactMetricChannelKind::HodgeUp:
            return field::OperatorKind::HodgeUp;
        case ExactMetricChannelKind::Hodge:
            return field::OperatorKind::Hodge;
        default:
            throw std::invalid_argument("channel outside declared exact gradient profile");
        }
    }
    static void update_field(storage::TemporalEndpoint &endpoint, std::uint64_t key,
                             numeric::RationalVector values) {
        std::vector<numeric::SparseRationalEntry> entries;
        entries.reserve(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
            if (values[i] != 0) entries.push_back({i, values[i]});
        update_sparse_field(endpoint, key, values.size(), std::move(entries));
    }
    static void update_sparse_field(storage::TemporalEndpoint &endpoint, std::uint64_t key,
                                   std::size_t size, std::vector<numeric::SparseRationalEntry> entries) {
        for (auto &tensor : endpoint.tensors)
            if (tensor.key == key) {
                tensor.tensor = {numeric::IndexDomain::linear(size),
                                 numeric::StructuralSupport::full(size),
                                 numeric::SparseRationalVector(size, std::move(entries))};
                return;
            }
        throw std::invalid_argument("training parameter tensor absent");
    }
    static std::vector<std::byte> metadata_bytes(const storage::TemporalTensor &tensor, std::size_t capacity) {
        if (tensor.tensor.domain() != numeric::IndexDomain::linear(capacity) ||
            tensor.tensor.support().count() != capacity)
            throw std::invalid_argument("training metadata carrier mismatch");
        std::vector<std::byte> out(capacity, std::byte{0});
        for (const auto& entry : tensor.tensor.values().entries()) {
            if (entry.value.get_den() != 1 || entry.value < 0 || entry.value > 255)
                throw std::invalid_argument("training metadata byte mismatch");
            out[entry.index] = static_cast<std::byte>(entry.value.get_num().get_ui());
        }
        return out;
    }
    static void update_metadata(storage::TemporalEndpoint &endpoint, const State &state, std::uint64_t step,
                                const numeric::Rational &rate, std::uint64_t events,
                                std::uint64_t knowledge) {
        storage::rfcx_detail::Writer writer;
        writer.string("native-exact-gradient-step-v1");
        writer.u64(step);
        storage::rfcx_detail::write_rational(writer, rate);
        writer.u64(events);
        writer.u64(knowledge);
        writer.u64(state.parameter_knowledge_time);
        writer.u64(state.tower.publication().source_namespace());
        writer.u64(state.tower.publication().sequence());
        writer.u16(state.grade.value);
        writer.string(state.contract.model.model);
        writer.u64(state.contract.model.channel_coordinates);
        writer.u64(state.contract.model.bias_coordinates);
        writer.u64(state.contract.time_coordinates);
        writer.u64(state.contract.publication_cutoff);
        writer.u64(state.contract.metadata_coordinates);
        writer.u8(static_cast<std::uint8_t>(state.tower.profile()));
        writer.u8(static_cast<std::uint8_t>(state.tower.metric_selection()));
        writer.u64(state.gains.tensor_declaration()->key);
        writer.u64(state.bias.tensor_declaration()->key);
        writer.string(state.policy.name);
        writer.u64(state.channels.size());
        for (const auto &channel : state.channels) {
            writer.u8(static_cast<std::uint8_t>(channel.kind));
            storage::rfcx_detail::write_rational(writer, channel.parameter);
        }
        writer.u64(state.samples.size());
        for (const auto &sample : state.samples) {
            writer.u64(sample.input.tensor_declaration()->key);
            writer.u64(sample.target.tensor_declaration()->key);
            writer.u8(static_cast<std::uint8_t>(sample.partition));
            writer.u64(sample.event_time);
            writer.u64(sample.knowledge_time);
            writer.u64(sample.label_available_time);
        }
        writer.string("half-metric-squared-error/original-parameters/fixed-order/no-momentum/no-random-v1");
        const auto capacity = state.contract.metadata_capacity;
        if (writer.size() > capacity - 8)
            throw std::length_error("training complete metadata byte limit");
        storage::rfcx_detail::Writer bytes;
        bytes.u64(writer.size());
        bytes.raw(writer.bytes());
        std::vector<numeric::SparseRationalEntry> values;
        values.reserve(bytes.size());
        for (std::size_t i = 0; i < bytes.size(); ++i)
            if (bytes.bytes()[i] != std::byte{0})
                values.push_back({i, numeric::Rational{static_cast<unsigned>(bytes.bytes()[i])}});
        update_sparse_field(endpoint, state.contract.metadata_tensor, capacity, std::move(values));
    }
    static storage::TemporalEndpoint make_base(State &state) {
        using namespace inference::native_field_detail;
        state.parameters.push_back(*state.bias.tensor()->state(0));
        auto endpoint = *state.tower.publication().endpoint();
        endpoint.sequence = 0;
        const auto carriers = storage::temporal_endpoint_detail::carriers(endpoint);
        const auto tensors = storage::temporal_endpoint_detail::tensors(endpoint);
        require(!carriers.contains(state.contract.metadata_carrier) &&
                    !tensors.contains(state.contract.metadata_tensor),
                "training metadata identity collision");
        endpoint.carriers.push_back({state.contract.metadata_carrier,
                                     storage::TemporalCarrierKind::Explicit,
                                     core::Grade{0},
                                     {},
                                     state.contract.metadata_coordinates,
                                     numeric::IndexDomain::linear(state.contract.metadata_capacity)});
        storage::TemporalTensor metadata;
        metadata.key = state.contract.metadata_tensor;
        metadata.role = storage::TemporalTensorRole::Field;
        metadata.output = state.contract.metadata_carrier;
        metadata.interpretation = "declared exact training checkpoint bytes; prospective model state";
        endpoint.tensors.push_back(std::move(metadata));
        update_metadata(endpoint, state, 0, numeric::Rational{0}, 0, state.parameter_knowledge_time);
        return endpoint;
    }
    explicit NativeExactTrainer(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::learning
