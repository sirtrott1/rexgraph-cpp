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
};

// A deterministic, serial exact gradient profile on the original ordered
// metric channels. The caller declares 1/2 Q_M(prediction-target), step sizes,
// sample order, time coordinates and label availability. There is no PRNG,
// momentum, Boolean derivative, parameter pooling or automatic assertion.
class NativeExactTrainer {
  public:
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
        next->history.append(storage::make_temporal_segment(old.endpoint(), endpoint, std::move(maps)));
        lease.check_cancelled();
        return NativeExactTrainer(std::move(next));
    }
    storage::NativeTemporalSeries history() const { return state_->history; }
    numeric::RationalVector parameters() const { return state_->parameters; }
    std::optional<numeric::Rational> last_loss() const { return state_->last_loss; }
    static NativeExactTrainer resume(NativeExactTrainer initial, std::span<const std::byte> archive,
                                     const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) {
        using namespace inference::native_field_detail;
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
        for (std::uint64_t step = 1; step <= history.last_sequence(); ++step) {
            const auto endpoint = history.at(step);
            const auto tensor = storage::temporal_endpoint_detail::tensors(endpoint.endpoint())
                                    .at(initial.state_->contract.metadata_tensor);
            const auto bytes = metadata_bytes(*tensor);
            storage::rfcx_detail::Reader reader(bytes);
            const auto payload = reader.slice(static_cast<std::size_t>(reader.u64()));
            storage::rfcx_detail::Reader metadata(payload);
            const auto prefix = metadata.string();
            require(prefix == "native-exact-gradient-step-v1", "unknown training resume profile");
            const auto saved_step = metadata.u64();
            const auto rate = storage::rfcx_detail::read_rational(metadata);
            const auto events = metadata.u64(), knowledge = metadata.u64();
            require(saved_step == step, "training step order mismatch");
            initial = initial.advance(rate, events, knowledge, lease);
            const auto produced = initial.state_->history.tail();
            require(storage::encode_temporal_endpoint(produced.endpoint()) ==
                        storage::encode_temporal_endpoint(endpoint.endpoint()),
                    "training resume update or complete optimizer/data-order state mismatch");
        }
        require(initial.state_->history.same_history(history),
                "training resume changed ordered native operation history");
        return initial;
    }

  private:
    struct State {
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
        for (auto &tensor : endpoint.tensors)
            if (tensor.key == key) {
                std::vector<numeric::SparseRationalEntry> entries;
                for (std::size_t i = 0; i < values.size(); ++i)
                    if (values[i] != 0)
                        entries.push_back({i, values[i]});
                tensor.tensor = {numeric::IndexDomain::linear(values.size()),
                                 numeric::StructuralSupport::full(values.size()),
                                 numeric::SparseRationalVector(values.size(), std::move(entries))};
                return;
            }
        throw std::invalid_argument("training parameter tensor absent");
    }
    static std::vector<std::byte> metadata_bytes(const storage::TemporalTensor &tensor) {
        std::vector<std::byte> out;
        out.reserve(4096);
        if (tensor.tensor.domain() != numeric::IndexDomain::linear(4096) ||
            tensor.tensor.support().count() != 4096)
            throw std::invalid_argument("training metadata carrier mismatch");
        for (std::size_t i = 0; i < 4096; ++i) {
            const auto value = *tensor.tensor.state(i);
            if (value.get_den() != 1 || value < 0 || value > 255)
                throw std::invalid_argument("training metadata byte mismatch");
            out.push_back(static_cast<std::byte>(value.get_num().get_ui()));
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
        if (writer.size() > 4088)
            throw std::length_error("training complete metadata byte limit");
        storage::rfcx_detail::Writer bytes;
        bytes.u64(writer.size());
        bytes.raw(writer.bytes());
        numeric::RationalVector values(4096, numeric::Rational{0});
        for (std::size_t i = 0; i < bytes.size(); ++i)
            values[i] = static_cast<unsigned>(bytes.bytes()[i]);
        update_field(endpoint, state.contract.metadata_tensor, std::move(values));
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
                                     numeric::IndexDomain::linear(4096)});
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
