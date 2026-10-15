#pragma once

#include "rfc/inference/relational_available_tensor_square.hpp"
#include "rfc/inference/relational_initial_input_lineage.hpp"
#include "rfc/learning/relational_available_evidence.hpp"

namespace rfc::learning {
struct RelationalAvailableTensorSquareObservation {
    std::uint64_t key{};
    inference::AvailableRelationalTensorSquareResult input;
    inference::OwnedRelationalInputUseAsset target_asset;
    std::uint64_t target_use{};
    numeric::SparseRationalTensor target;
    std::vector<core::CellId> observed;
    RelationalTargetOrigins origins;
    std::uint64_t interface_origin{}, evidence_origin{};
    std::optional<inference::AvailableRelationalInitialInputLineage> lineage{};
};
inline std::vector<std::byte> relational_tensor_square_observation_payload(
    std::uint64_t key, const available_evidence_detail::Origin& root,
    const std::vector<available_evidence_detail::Origin>& input,
    const available_evidence_detail::Origin& target_asset, const available_evidence_detail::Origin& target_value,
    std::uint64_t use, const std::vector<core::CellId>& observed) {
    using namespace inference::field_program_codec_detail;
    constexpr std::array<std::byte, 8> magic{
        std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'O'}, std::byte{1}, std::byte{0}, std::byte{0}};
    auto w = envelope(magic); w.u64(key);
    const auto reference = [&](const available_evidence_detail::Origin& origin) {
        const auto publication = origin.publication(); w.u64(origin.key()); w.u8(static_cast<std::uint8_t>(origin.role()));
        w.u64(publication.source_namespace()); w.u64(publication.cutoff());
    };
    reference(root); w.u64(input.size()); for (const auto& origin : input) reference(origin);
    reference(target_asset); reference(target_value); w.u64(use); w.u64(observed.size());
    for (const auto cell : observed) { w.u16(cell.grade.value); w.u32(cell.index); }
    return finish(std::move(w));
}
class AvailableRelationalTensorSquareAffineResult;

// Compose the existing one-grade whole-field fitter with complete product
// owners. No tensor arithmetic, alternate fitting engine or origin publisher
// is introduced here. Keep this owner rather than detaching base().
class PreparedAvailableRelationalTensorSquareEvidence {
    struct State {
        PreparedAvailableRelationalEvidence base;
        std::shared_ptr<const State> parent;
        std::vector<RelationalAvailableTensorSquareObservation> suffix;
        std::vector<available_evidence_detail::Origin> obligations;
    };
public:
    PreparedAvailableRelationalTensorSquareEvidence(inference::PreparedAvailableRelationalProgram program,
        RelationalFieldParameterDeclaration declaration, std::uint64_t origin,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) {
        const PreparedAvailableRelationalEvidence base(std::move(program), std::move(declaration), origin, context, limits, std::move(check));
        state_ = std::make_shared<const State>(State{base, {}, {}, base.obligations()});
    }
    PreparedAvailableRelationalEvidence base() const { return state_->base; }
    PreparedRelationalFieldEvidence evidence() const { return state_->base.evidence(); }
    inference::PreparedAvailableRelationalProgram program() const { return state_->base.program(); }
    numeric::ExactActionLimits limits() const { return state_->base.limits(); }
    const std::vector<RelationalAvailableTensorSquareObservation>& suffix() const & { return state_->suffix; }
    const std::vector<RelationalAvailableTensorSquareObservation>& suffix() const && = delete;
    const std::vector<available_evidence_detail::Origin>& obligations() const & { return state_->obligations; }
    const std::vector<available_evidence_detail::Origin>& obligations() const && = delete;
    std::optional<PreparedAvailableRelationalTensorSquareEvidence> parent() const {
        if (state_->parent) return PreparedAvailableRelationalTensorSquareEvidence(state_->parent);
        return {};
    }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        available_evidence_detail::recheck(pin->obligations, authority, callback);
    }
    PreparedAvailableRelationalTensorSquareEvidence append(std::vector<RelationalAvailableTensorSquareObservation> suffix,
        const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        using namespace available_evidence_detail;
        const auto pin = state_; const auto frozen = std::move(suffix); const auto authority = context; const auto callback = std::move(check);
        recheck(pin->obligations, authority, callback); auto obligations = pin->obligations;
        const auto prepared = pin->base.program(); const auto program_bytes = inference::encode_relational_field_program(prepared.program());
        // Preflight the entire suffix, including targets, before the first
        // downstream action or fitting step. Original product parents remain
        // mandatory even if every derived value is independently available.
        for (const auto& o : frozen) {
            o.input.admit(authority, callback);
            const auto compound = o.input.input();
            auto input_dependencies = compound.obligations();
            if (o.lineage) {
                o.lineage->admit(authority, callback);
                const auto lineage = o.lineage->lineage(); const auto native = compound.native_input();
                const auto square = native.square();
                inference::field_program_detail::require(lineage.initial().same_owner(square.parent().accession().asset()),
                    "initial lineage belongs to a foreign actual product input asset");
                input_dependencies.insert(input_dependencies.end(), o.lineage->obligations().begin(), o.lineage->obligations().end());
            }
            const auto asset = inference::encode_relational_input_use_asset(o.target_asset);
            const auto payload = relational_target_value_payload(program_bytes, pin->base.root_declaration().node, asset, o.target_use, o.target);
            const auto target_asset = authority.require(o.origins.asset, Role::TargetAsset, asset);
            const auto target_value = authority.require(o.origins.value, Role::TargetValue, payload);
            const auto declaration = relational_tensor_square_observation_payload(o.key, pin->base.root_origin(),
                input_dependencies, target_asset, target_value, o.target_use, o.observed);
            const auto interface = authority.require(o.interface_origin, Role::Interface, declaration);
            const auto evidence = authority.require(o.evidence_origin, Role::Evidence, declaration);
            obligations.insert(obligations.end(), input_dependencies.begin(), input_dependencies.end());
            obligations.push_back(target_asset); obligations.push_back(target_value);
            obligations.push_back(interface); obligations.push_back(evidence);
        }
        std::vector<RelationalAvailableTrainingObservation> observations;
        for (const auto& o : frozen) {
            const auto compound = o.input.input(); const auto native = compound.native_input();
            auto downstream = prepared.execute({{native.input(), compound.origins().input}}, authority, pin->base.limits(), callback);
            inference::field_program_codec_detail::canonical(inference::encode_accession_field_result(o.input.result()),
                inference::encode_accession_field_result(downstream.result()));
            observations.push_back({o.key, std::move(downstream), o.target_asset, o.target_use, o.target, o.observed, o.origins});
        }
        auto child = pin->base.append(std::move(observations), authority, callback);
        return PreparedAvailableRelationalTensorSquareEvidence(std::make_shared<const State>(State{
            std::move(child), pin, frozen, std::move(obligations)}));
    }
    AvailableRelationalTensorSquareAffineResult execute(inference::AvailableRelationalTensorSquareResult input,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}, std::optional<inference::AvailableRelationalInitialInputLineage> lineage = {}) const;
private:
    explicit PreparedAvailableRelationalTensorSquareEvidence(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
class AvailableRelationalTensorSquareAffineResult {
    struct State {
        PreparedAvailableRelationalTensorSquareEvidence evidence;
        inference::AvailableRelationalTensorSquareResult input;
        AvailableRelationalAffineResult base;
        std::optional<inference::AvailableRelationalInitialInputLineage> lineage;
    };
public:
    PreparedAvailableRelationalTensorSquareEvidence evidence() const { return state_->evidence; }
    inference::AvailableRelationalTensorSquareResult input() const { return state_->input; }
    RelationalAffineProgramResult result() const { return state_->base.result(); }
    std::optional<inference::AvailableRelationalInitialInputLineage> lineage() const { return state_->lineage; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        pin->evidence.admit(authority, callback); pin->input.admit(authority, callback);
        if (pin->lineage) pin->lineage->admit(authority, callback);
    }
private:
    friend class PreparedAvailableRelationalTensorSquareEvidence;
    explicit AvailableRelationalTensorSquareAffineResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline AvailableRelationalTensorSquareAffineResult PreparedAvailableRelationalTensorSquareEvidence::execute(
    inference::AvailableRelationalTensorSquareResult input, const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check,
    std::optional<inference::AvailableRelationalInitialInputLineage> lineage) const {
    const auto pin = *this; const auto frozen = std::move(input); const auto authority = context; const auto callback = std::move(check);
    const auto original = std::move(lineage);
    pin.admit(authority, callback); frozen.admit(authority, callback);
    const auto compound = frozen.input(); const auto native = compound.native_input(); const auto prepared = pin.program();
    if (original) {
        original->admit(authority, callback); const auto owner = original->lineage(); const auto square = native.square();
        inference::field_program_detail::require(owner.initial().same_owner(square.parent().accession().asset()),
            "held initial lineage belongs to a foreign actual product input asset");
    }
    auto downstream = prepared.execute({{native.input(), compound.origins().input}}, authority, limits, callback);
    inference::field_program_codec_detail::canonical(inference::encode_accession_field_result(frozen.result()),
        inference::encode_accession_field_result(downstream.result()));
    auto family = pin.base().execute(std::move(downstream), authority, limits, callback);
    return AvailableRelationalTensorSquareAffineResult({pin, frozen, std::move(family), original});
}
} // namespace rfc::learning
