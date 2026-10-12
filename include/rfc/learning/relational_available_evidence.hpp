#pragma once

#include "rfc/inference/relational_value_admission_codec.hpp"
#include "rfc/learning/relational_affine_program_codec.hpp"

namespace rfc::learning {
namespace available_evidence_detail {
using Bytes = std::vector<std::byte>;
using Origin = inference::OwnedRelationalValueOrigin;
using Context = inference::RelationalAvailabilityContext;
using Role = inference::RelationalDependencyRole;
using namespace inference::field_program_codec_detail;
inline constexpr std::array<std::byte, 8> root_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'D'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> target_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'T'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> setting_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'X'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline void recheck(const std::vector<Origin>& origins, const Context& context, const std::function<void()>& check) {
    for (const auto& origin : origins) { if (check) check(); context.recheck(origin); }
}
} // namespace available_evidence_detail

// Passive coefficient serialization, before a root fit or measurement action.
inline available_evidence_detail::Bytes relational_evidence_root_payload(std::span<const std::byte> program,
    const RelationalFieldParameterDeclaration& declaration, numeric::ExactActionLimits limits) {
    using namespace available_evidence_detail;
    auto w = envelope(root_magic); blob(w, program); relational_affine_codec_detail::parameters(w, declaration);
    w.u64(limits.dimension); w.u64(limits.entries); w.u64(limits.steps); w.u64(limits.integer_bits);
    return finish(std::move(w));
}
inline available_evidence_detail::Bytes relational_target_value_payload(std::span<const std::byte> program,
    std::uint64_t node, std::span<const std::byte> asset, std::uint64_t use, const numeric::SparseRationalTensor& target) {
    using namespace available_evidence_detail;
    auto w = envelope(target_magic); blob(w, program); w.u64(node); blob(w, asset); w.u64(use);
    storage::temporal_codec_detail::tensor(w, target); return finish(std::move(w));
}
struct RelationalTargetOrigins { std::uint64_t asset{}, value{}; };
struct RelationalAvailableTrainingObservation {
    std::uint64_t key{};
    inference::AvailableRelationalFieldResult input;
    inference::OwnedRelationalInputUseAsset target_asset;
    std::uint64_t target_use{};
    numeric::SparseRationalTensor target;
    std::vector<core::CellId> observed;
    RelationalTargetOrigins origins;
};
struct RelationalAvailableObservationSubmission {
    std::uint64_t key{};
    std::vector<inference::RelationalAvailableInputSubmission> inputs;
    inference::OwnedRelationalInputUseAsset target_asset;
    std::uint64_t target_use{};
    numeric::SparseRationalTensor target;
    std::vector<core::CellId> observed;
    RelationalTargetOrigins origins;
};
class AvailableRelationalAffineResult;
class AvailableRelationalAffineEvaluation;
class PreparedAvailableRelationalEvidence {
    struct Root {
        inference::PreparedAvailableRelationalProgram program;
        RelationalFieldParameterDeclaration declaration;
        available_evidence_detail::Origin origin;
        numeric::ExactActionLimits limits;
    };
    struct State {
        std::shared_ptr<const Root> root;
        PreparedRelationalFieldEvidence evidence;
        std::shared_ptr<const State> parent;
        std::vector<RelationalAvailableTrainingObservation> suffix;
        std::vector<available_evidence_detail::Origin> obligations;
    };
public:
    PreparedAvailableRelationalEvidence(inference::PreparedAvailableRelationalProgram program,
        RelationalFieldParameterDeclaration declaration, std::uint64_t origin,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) {
        using namespace available_evidence_detail;
        const auto pin = program; const auto frozen = declaration; const auto authority = context; const auto callback = std::move(check);
        pin.admit(authority, callback);
        const auto bytes = relational_evidence_root_payload(inference::encode_relational_field_program(pin.program()), frozen, limits);
        const auto actual = authority.require(origin, Role::Evidence, bytes);
        if (callback) callback();
        auto native = PreparedRelationalFieldEvidence(pin.program(), frozen, limits, callback);
        auto root = std::make_shared<const Root>(Root{pin, frozen, actual, limits});
        auto obligations = pin.obligations(); obligations.push_back(actual);
        state_ = std::make_shared<const State>(State{std::move(root), std::move(native), {}, {}, std::move(obligations)});
    }
    PreparedRelationalFieldEvidence evidence() const { return state_->evidence; }
    inference::PreparedAvailableRelationalProgram program() const { return state_->root->program; }
    const RelationalFieldParameterDeclaration& root_declaration() const & { return state_->root->declaration; }
    const RelationalFieldParameterDeclaration& root_declaration() const && = delete;
    available_evidence_detail::Origin root_origin() const { return state_->root->origin; }
    numeric::ExactActionLimits limits() const { return state_->root->limits; }
    const std::vector<RelationalAvailableTrainingObservation>& suffix() const & { return state_->suffix; }
    const std::vector<RelationalAvailableTrainingObservation>& suffix() const && = delete;
    const std::vector<available_evidence_detail::Origin>& obligations() const & { return state_->obligations; }
    const std::vector<available_evidence_detail::Origin>& obligations() const && = delete;
    std::optional<PreparedAvailableRelationalEvidence> parent() const {
        if (state_->parent) return PreparedAvailableRelationalEvidence(state_->parent);
        return {};
    }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        available_evidence_detail::recheck(pin->obligations, authority, callback);
    }
    PreparedAvailableRelationalEvidence append(std::vector<RelationalAvailableTrainingObservation> suffix,
        const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        using namespace available_evidence_detail;
        const auto pin = state_; const auto frozen = std::move(suffix); const auto authority = context; const auto callback = std::move(check);
        recheck(pin->obligations, authority, callback); auto obligations = pin->obligations;
        std::vector<RelationalFieldTrainingObservation> observations;
        const auto program_bytes = inference::encode_relational_field_program(pin->root->program.program());
        // Check the entire suffix before entering the native observation fitter.
        for (const auto& o : frozen) {
            o.input.admit(authority, callback);
            const auto asset = inference::encode_relational_input_use_asset(o.target_asset);
            const auto value = relational_target_value_payload(program_bytes, pin->root->declaration.node, asset, o.target_use, o.target);
            const auto target_asset = authority.require(o.origins.asset, Role::TargetAsset, asset);
            const auto target_value = authority.require(o.origins.value, Role::TargetValue, value);
            obligations.insert(obligations.end(), o.input.obligations().begin(), o.input.obligations().end());
            obligations.push_back(target_asset); obligations.push_back(target_value);
            observations.push_back({o.key, o.input.result(), o.target_asset, o.target_use, o.target, o.observed});
        }
        if (callback) callback();
        auto child = pin->evidence.append(std::move(observations), pin->root->limits, callback);
        return PreparedAvailableRelationalEvidence(std::make_shared<const State>(State{pin->root, std::move(child), pin, frozen, std::move(obligations)}));
    }
    PreparedAvailableRelationalEvidence append_inputs(std::vector<RelationalAvailableObservationSubmission> suffix,
        const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        using namespace available_evidence_detail;
        const auto pin = *this; const auto frozen = std::move(suffix); const auto authority = context; const auto callback = std::move(check);
        pin.admit(authority, callback); const auto prepared = pin.program();
        const auto program_bytes = inference::encode_relational_field_program(prepared.program());
        // Entire raw suffix is checked before even the first input A/R action.
        for (const auto& o : frozen) {
            prepared.check_inputs(o.inputs, authority, callback);
            const auto asset = inference::encode_relational_input_use_asset(o.target_asset);
            const auto value = relational_target_value_payload(program_bytes, pin.root_declaration().node, asset, o.target_use, o.target);
            (void)authority.require(o.origins.asset, Role::TargetAsset, asset);
            (void)authority.require(o.origins.value, Role::TargetValue, value);
        }
        std::vector<RelationalAvailableTrainingObservation> observations;
        for (const auto& o : frozen) {
            auto input = prepared.execute_inputs(o.inputs, authority, pin.limits(), callback);
            observations.push_back({o.key, std::move(input), o.target_asset, o.target_use, o.target, o.observed, o.origins});
        }
        return pin.append(std::move(observations), authority, callback);
    }
    AvailableRelationalAffineResult execute(inference::AvailableRelationalFieldResult input,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const;
    AvailableRelationalAffineResult execute_inputs(std::vector<inference::RelationalAvailableInputSubmission> inputs,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const;
private:
    explicit PreparedAvailableRelationalEvidence(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

inline available_evidence_detail::Bytes relational_parameter_setting_payload(std::span<const std::byte> native_evidence,
    const std::vector<available_evidence_detail::Origin>& obligations, const numeric::RationalVector& free) {
    using namespace available_evidence_detail;
    auto w = envelope(setting_magic); blob(w, native_evidence); w.u64(obligations.size());
    for (const auto& origin : obligations) blob(w, origin.receipt());
    relational_affine_codec_detail::vector(w, free); return finish(std::move(w));
}
inline available_evidence_detail::Bytes relational_parameter_setting_payload(const PreparedAvailableRelationalEvidence& evidence,
    const numeric::RationalVector& free) {
    return relational_parameter_setting_payload(encode_relational_field_evidence(evidence.evidence()), evidence.obligations(), free);
}
class AvailableRelationalAffineResult {
    struct State {
        PreparedAvailableRelationalEvidence evidence;
        inference::AvailableRelationalFieldResult input;
        RelationalAffineProgramResult result;
    };
public:
    PreparedAvailableRelationalEvidence evidence() const { return state_->evidence; }
    inference::AvailableRelationalFieldResult input() const { return state_->input; }
    RelationalAffineProgramResult result() const { return state_->result; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        pin->evidence.admit(authority, callback); pin->input.admit(authority, callback);
    }
    AvailableRelationalAffineEvaluation evaluate(numeric::RationalVector free, std::uint64_t setting,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const;
private:
    friend class PreparedAvailableRelationalEvidence;
    explicit AvailableRelationalAffineResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline AvailableRelationalAffineResult PreparedAvailableRelationalEvidence::execute(inference::AvailableRelationalFieldResult input,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto frozen = std::move(input); const auto authority = context; const auto callback = std::move(check);
    pin.admit(authority, callback); frozen.admit(authority, callback);
    auto result = pin.evidence().execute(frozen.result(), limits, callback);
    return AvailableRelationalAffineResult({pin, frozen, std::move(result)});
}
inline AvailableRelationalAffineResult PreparedAvailableRelationalEvidence::execute_inputs(std::vector<inference::RelationalAvailableInputSubmission> inputs,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto frozen = std::move(inputs); const auto authority = context; const auto callback = std::move(check);
    pin.admit(authority, callback);
    auto original = pin.program().execute_inputs(frozen, authority, limits, callback);
    return pin.execute(std::move(original), authority, limits, callback);
}
class AvailableRelationalAffineEvaluation {
public:
    AvailableRelationalAffineResult family() const { return family_; }
    RelationalAffineEvaluation result() const { return result_; }
    available_evidence_detail::Origin setting() const { return setting_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = *this; const auto authority = context; const auto callback = std::move(check);
        pin.family_.admit(authority, callback); if (callback) callback(); authority.recheck(pin.setting_);
    }
private:
    friend class AvailableRelationalAffineResult;
    AvailableRelationalAffineEvaluation(AvailableRelationalAffineResult family, RelationalAffineEvaluation result,
        available_evidence_detail::Origin setting) : family_(std::move(family)), result_(std::move(result)), setting_(std::move(setting)) {}
    AvailableRelationalAffineResult family_;
    RelationalAffineEvaluation result_;
    available_evidence_detail::Origin setting_;
};
inline AvailableRelationalAffineEvaluation AvailableRelationalAffineResult::evaluate(numeric::RationalVector free, std::uint64_t setting,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto frozen = std::move(free); const auto authority = context; const auto callback = std::move(check);
    pin.admit(authority, callback);
    const auto payload = relational_parameter_setting_payload(pin.evidence(), frozen);
    const auto actual = authority.require(setting, available_evidence_detail::Role::Prospective, payload);
    if (callback) callback();
    auto value = pin.result().evaluate(frozen, limits, callback); return {pin, std::move(value), actual};
}
} // namespace rfc::learning
