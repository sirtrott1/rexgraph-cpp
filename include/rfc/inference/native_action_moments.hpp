#pragma once
#include "rfc/inference/native_moments.hpp"
#include <set>

namespace rfc::inference {

// Bounds on prepared metadata, separate from the invocation's ExactActionGuard.
struct NativeActionMomentLimits {
    std::size_t components{4096}, interactions{65536}, label_bytes{1048576};
};
struct NativeActionInteractionRequest {
    std::size_t left{}, right{}; // unique unordered pairs, left < right
};
struct NativeActionMomentComponent {
    std::string label;
    NativeFieldValue field;
    numeric::Rational self, with_previous, increment, prefix_quadrance;
};
struct NativeActionInteraction {
    std::size_t left{}, right{};
    numeric::Rational pairing, doubled;
};
struct NativeActionMomentResult {
    NativeFieldValue input, aggregate;
    std::vector<NativeActionMomentComponent> components;
    std::vector<NativeActionInteraction> interactions;
    numeric::Rational diagonal_sum, cross_sum, total, unreported_cross_sum;
    std::optional<NativeFieldValue> reference, residual;
    std::optional<bool> matches_reference;
    std::shared_ptr<const void> plan_owner;
    std::size_t execution_steps{}, retained_sparse_entries{};
};

// Additive action channels on ONE actual input carrier and ONE paired target.
// This does not glue duplicate section readings by adding them. Interface descent,
// reconstruction weights and operation attribution must already be declared in
// the supplied actions. An independent reference is checked on the supplied field,
// not on every field in its carrier. Component order defines conditional increments,
// not causal ownership or a physical time ordering.
class PreparedNativeActionMoments {
    struct State {
        std::vector<NativeMomentChannel> channels;
        NativeBilinearForm metric;
        std::optional<NativeFactoredAction> reference;
        NativeActionMomentLimits limits;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedNativeActionMoments(std::vector<NativeMomentChannel> channels,
        NativeBilinearForm metric, std::optional<NativeFactoredAction> reference = {},
        NativeActionMomentLimits limits = {}) {
        if (channels.empty() || channels.size() > limits.components)
            throw std::length_error("action moments need a nonempty bounded component list");
        if (!metric.is_positive_metric() || !metric.left().same(metric.right()))
            throw std::invalid_argument("action quadrance decomposition requires one positive target metric");
        std::set<std::string> labels;
        std::size_t bytes = 0;
        const auto& input = channels.front().operation.input();
        for (const auto& channel : channels) {
            bytes = machine::checked_size_add(bytes, channel.label.size());
            if (bytes > limits.label_bytes) throw std::length_error("action moment label budget");
            if (channel.label.empty() || !labels.insert(channel.label).second)
                throw std::invalid_argument("action moment component labels must be nonempty and unique");
            if (!input.same(channel.operation.input()) || !metric.left().same(channel.operation.output()))
                throw std::invalid_argument("action moment components require actual shared input/output carriers");
        }
        if (reference && (!input.same(reference->input()) || !metric.left().same(reference->output())))
            throw std::invalid_argument("action moment reference has different carriers");
        state_ = std::make_shared<const State>(State{std::move(channels), std::move(metric),
                                                   std::move(reference), limits});
    }
    const std::vector<NativeMomentChannel>& channels() const & { return state_->channels; }
    const std::vector<NativeMomentChannel>& channels() const && = delete;
    const NativeBilinearForm& metric() const & { return state_->metric; }
    const NativeBilinearForm& metric() const && = delete;

    // Each component is realized once. Individual and prefix moments require O(n)
    // pairings; only explicitly requested off-diagonal pairs are materialized.
    // No ambient kernel, operator power matrix or all-pairs Gram is constructed.
    NativeActionMomentResult execute(NativeFieldValue input,
        const NativeActionAvailability& availability,
        std::span<const NativeActionInteractionRequest> selected = {},
        const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        const auto pin = state_;
        lease.check_cancelled();
        if (!input.carrier().same(pin->channels.front().operation.input()))
            throw std::invalid_argument("action moment input has a foreign source/carrier");
        if (selected.size() > pin->limits.interactions)
            throw std::length_error("selected action interaction budget");
        const auto add = machine::checked_size_add;
        // One input, one sum, optional direct/residual, plus component/pair records.
        const auto records = add(add(pin->channels.size(), selected.size()), 8);
        const auto scalars = add(add(machine::checked_size_mul(pin->channels.size(), 4),
                                    machine::checked_size_mul(selected.size(), 2)), 8);
        runtime::admit_execution({records, scalars, 0, 0, 0}, lease.policy());
        std::set<std::pair<std::size_t,std::size_t>> seen;
        for (const auto& pair : selected) {
            lease.check_cancelled();
            if (pair.left >= pair.right || pair.right >= pin->channels.size() ||
                !seen.emplace(pair.left, pair.right).second)
                throw std::invalid_argument("interaction requests must be unique in-range pairs left < right");
        }
        auto pins = input.publications();
        auto dimension = pin->metric.dimension_limit();
        const auto include = [&](const NativeFactoredAction& action) {
            dimension = std::min(dimension, action.dimension_limit());
            for (const auto& p : action.publications()) native_action_detail::append_pin(pins, p);
        };
        for (const auto& channel : pin->channels) include(channel.operation);
        if (pin->reference) include(*pin->reference);
        for (const auto& p : pin->metric.publications()) native_action_detail::append_pin(pins, p);
        for (const auto& p : pins) { lease.check_cancelled(); native_action_detail::available(p, availability); }
        const auto record_count = add(records, pins.size());
        runtime::admit_execution({record_count, scalars, 0, 0, 0}, lease.policy());
        numeric::ExactActionGuard guard(native_action_detail::limits(lease, pins, dimension),
                                        [&lease]{ lease.check_cancelled(); });
        guard.sparse_vector(input.sparse_values());
        std::size_t held = input.sparse_values().nonzeros();
        const auto admit = [&](std::size_t extra) {
            const auto n = add(held, extra);
            runtime::admit_execution({record_count, add(scalars,n), n, 0, 0}, lease.policy());
        };
        admit(0);
        std::vector<NativeActionMomentComponent> components;
        components.reserve(pin->channels.size());
        std::optional<NativeFieldValue> sum;
        numeric::Rational diagonal = 0, prefix = 0;
        for (const auto& channel : pin->channels) {
            lease.check_cancelled();
            admit(channel.operation.retained_entries());
            auto y = channel.operation.execute_with_guard(input, availability, lease, guard);
            const auto count = y.sparse_values().nonzeros();
            admit(add(channel.operation.retained_entries(), count));
            held = add(held, count);
            const auto self = pin->metric.pair(y, y, guard);
            numeric::Rational cross = 0;
            if (sum) {
                cross = guard.mul(2, pin->metric.pair(*sum, y, guard));
                // Account for a new sum before replacing the previous sum.
                admit(add(sum->sparse_values().nonzeros(), count));
                auto next = sum->plus(y, guard);
                held -= sum->sparse_values().nonzeros();
                held = add(held, next.sparse_values().nonzeros());
                sum = std::move(next);
            } else {
                sum = y; // immutable shared value; counts conservatively include both handles
                held = add(held, count);
            }
            diagonal = guard.add(diagonal, self);
            const auto increment = guard.add(self, cross);
            prefix = guard.add(prefix, increment);
            components.push_back({channel.label, std::move(y), self, cross, increment, prefix});
            admit(0);
        }
        const auto total = pin->metric.pair(*sum, *sum, guard);
        if (total != prefix) throw std::logic_error("action moment prefix identity failed");
        const auto cross_sum = guard.sub(total, diagonal);
        numeric::Rational reported = 0;
        std::vector<NativeActionInteraction> interactions;
        interactions.reserve(selected.size());
        for (const auto& pair : selected) {
            const auto value = pin->metric.pair(components[pair.left].field, components[pair.right].field, guard);
            const auto twice = guard.mul(2, value);
            reported = guard.add(reported, twice);
            interactions.push_back({pair.left, pair.right, value, twice});
        }
        std::optional<NativeFieldValue> direct, residual;
        std::optional<bool> matches;
        if (pin->reference) {
            admit(pin->reference->retained_entries());
            direct = pin->reference->execute_with_guard(input, availability, lease, guard);
            held = add(held, direct->sparse_values().nonzeros());
            admit(add(sum->sparse_values().nonzeros(),
                      machine::checked_size_mul(direct->sparse_values().nonzeros(),2)));
            residual = sum->plus(direct->scaled(-1, guard), guard);
            held = add(held, residual->sparse_values().nonzeros());
            matches = residual->sparse_values().nonzeros() == 0;
        }
        admit(0);
        const auto unreported = guard.sub(cross_sum, reported);
        lease.check_cancelled();
        return {std::move(input), std::move(*sum), std::move(components), std::move(interactions),
                diagonal, cross_sum, total, unreported, std::move(direct), std::move(residual),
                matches, pin, guard.steps(), held};
    }
};
} // namespace rfc::inference
