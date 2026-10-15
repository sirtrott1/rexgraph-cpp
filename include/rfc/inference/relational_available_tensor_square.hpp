#pragma once

#include "rfc/inference/relational_known_tensor_square.hpp"
#include "rfc/inference/relational_value_admission.hpp"

namespace rfc::inference {
struct RelationalTensorSquareOrigins {
    RelationalProgramOrigins parent_program;
    RelationalInputOrigins parent_input;
    std::uint64_t declaration{};
    RelationalProgramOrigins program;
    RelationalInputOrigins input;
};
inline std::vector<std::byte> relational_tensor_square_dependency_payload(
    std::span<const std::byte> parent_program, const RelationalKnownTensorSquareDeclaration& d) {
    using namespace field_program_codec_detail;
    constexpr std::array<std::byte, 8> magic{
        std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'K'}, std::byte{'D'}, std::byte{1}, std::byte{0}, std::byte{0}};
    auto w = envelope(magic); blob(w, parent_program);
    w.u64(d.key); w.u64(d.revision); w.u64(d.lifted_basis);
    w.u16(d.context.grade.value); w.u32(d.context.index);
    w.u64(d.pair_directions.size()); for (const auto key : d.pair_directions) w.u64(key);
    return finish(std::move(w));
}
inline std::vector<std::byte> relational_tensor_square_dependency_payload(
    const OwnedRelationalFieldProgram& parent_program, const RelationalKnownTensorSquareDeclaration& d) {
    return relational_tensor_square_dependency_payload(encode_relational_field_program(parent_program), d);
}
namespace available_tensor_square_detail {
inline void recheck(const std::vector<OwnedRelationalValueOrigin>& obligations,
    const RelationalAvailabilityContext& context, const std::function<void()>& check) {
    for (const auto& origin : obligations) { if (check) check(); context.recheck(origin); }
}
inline std::vector<OwnedRelationalValueOrigin> input_origins(const PreparedAvailableRelationalProgram& program,
    const RelationalInputSubmission& submission, RelationalInputOrigins keys,
    const RelationalAvailabilityContext& context) {
    const auto asset = encode_relational_input_use_asset(submission.asset);
    const auto value = relational_input_value_payload(asset, submission.declaration, submission.use, submission.source_field);
    const auto accession = relational_input_accession_payload(encode_relational_field_program(program.program()), asset,
        submission.declaration, submission.use, submission.source_field);
    auto obligations = program.obligations();
    value_admission_detail::input_origins(obligations, keys, asset, value, accession, context);
    return obligations;
}
} // namespace available_tensor_square_detail

class AvailableRelationalTensorSquare;
class AvailableRelationalTensorSquareInput;
class AvailableRelationalTensorSquareResult;
AvailableRelationalTensorSquareInput admit_available_relational_tensor_square_input(
    RelationalKnownTensorSquareInput input, RelationalTensorSquareOrigins origins,
    const RelationalAvailabilityContext& context, std::function<void()> check = {});

// The declaration is an actual Interface publication. This prepares no product
// and never creates authority. The producer must admit the original input before
// its first multiplication, even when the original raw bytes are older.
class PreparedAvailableRelationalTensorSquare {
    struct State {
        PreparedAvailableRelationalProgram program;
        RelationalKnownTensorSquareDeclaration declaration;
        OwnedRelationalValueOrigin origin;
    };
public:
    PreparedAvailableRelationalTensorSquare(PreparedAvailableRelationalProgram program,
        RelationalKnownTensorSquareDeclaration declaration, std::uint64_t origin,
        const RelationalAvailabilityContext& context) {
        const auto authority = context; program.admit(authority);
        auto actual = authority.require(origin, RelationalDependencyRole::Interface,
            relational_tensor_square_dependency_payload(program.program(), declaration));
        state_ = std::make_shared<const State>(State{std::move(program), std::move(declaration), std::move(actual)});
    }
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        pin->program.admit(authority, callback); if (callback) callback(); authority.recheck(pin->origin);
    }
    AvailableRelationalTensorSquare lift(RelationalInputSubmission submission, RelationalInputOrigins origins,
        const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};

class AvailableRelationalTensorSquare {
    struct State {
        OwnedRelationalKnownTensorSquare square;
        RelationalProgramOrigins program;
        RelationalInputOrigins input;
        std::uint64_t declaration;
        std::vector<OwnedRelationalValueOrigin> obligations;
    };
public:
    OwnedRelationalKnownTensorSquare square() const { return state_->square; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const & { return state_->obligations; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const && = delete;
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        available_tensor_square_detail::recheck(pin->obligations, authority, callback);
    }
    AvailableRelationalTensorSquareInput admit_input(PreparedAvailableRelationalProgram program,
        RelationalInputAccessionDeclaration declaration, RelationalInputOrigins origins,
        const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedAvailableRelationalTensorSquare;
    explicit AvailableRelationalTensorSquare(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};

// This is the executable compound owner. Keep it through training and serving;
// native_input() is a mathematical inspection accessor, not a replacement for
// admission. Both parents' original dependencies remain required at every use.
class AvailableRelationalTensorSquareInput {
    struct State {
        RelationalKnownTensorSquareInput input;
        RelationalTensorSquareOrigins origins;
        PreparedAvailableRelationalProgram program;
        std::vector<OwnedRelationalValueOrigin> obligations;
    };
public:
    RelationalKnownTensorSquareInput native_input() const { return state_->input; }
    RelationalTensorSquareOrigins origins() const { return state_->origins; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const & { return state_->obligations; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const && = delete;
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        available_tensor_square_detail::recheck(pin->obligations, authority, callback);
    }
    AvailableRelationalTensorSquareResult execute(const RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class AvailableRelationalTensorSquare;
    friend AvailableRelationalTensorSquareInput admit_available_relational_tensor_square_input(
        RelationalKnownTensorSquareInput, RelationalTensorSquareOrigins, const RelationalAvailabilityContext&, std::function<void()>);
    explicit AvailableRelationalTensorSquareInput(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
class AvailableRelationalTensorSquareResult {
public:
    AvailableRelationalTensorSquareInput input() const { return input_; }
    OwnedAccessionFieldResult result() const { return result_.result(); }
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = *this; const auto authority = context; const auto callback = std::move(check);
        pin.input_.admit(authority, callback); pin.result_.admit(authority, callback);
    }
private:
    friend class AvailableRelationalTensorSquareInput;
    AvailableRelationalTensorSquareResult(AvailableRelationalTensorSquareInput input, AvailableRelationalFieldResult result)
        : input_(std::move(input)), result_(std::move(result)) {}
    AvailableRelationalTensorSquareInput input_;
    AvailableRelationalFieldResult result_;
};

inline AvailableRelationalTensorSquare PreparedAvailableRelationalTensorSquare::lift(
    RelationalInputSubmission submission, RelationalInputOrigins origins, const RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = state_; const auto authority = context; const auto frozen = std::move(submission);
    const auto callback = std::move(check);
    pin->program.admit(authority, callback); if (callback) callback(); authority.recheck(pin->origin);
    // All passive original payloads are checked before A/R construction or the
    // tensor product. Retain the actual authorities, rather than a saved Boolean.
    pin->program.check_inputs({{frozen, origins}}, authority, callback);
    auto obligations = available_tensor_square_detail::input_origins(pin->program, frozen, origins, authority);
    obligations.push_back(pin->origin);
    const auto original = pin->program.admit_input(frozen, origins, authority, limits, callback);
    OwnedRelationalKnownTensorSquare square(original.input, pin->declaration, limits, callback);
    return AvailableRelationalTensorSquare({std::move(square), pin->program.program_origins(), origins,
        pin->origin.key(), std::move(obligations)});
}
inline AvailableRelationalTensorSquareInput AvailableRelationalTensorSquare::admit_input(
    PreparedAvailableRelationalProgram program, RelationalInputAccessionDeclaration declaration, RelationalInputOrigins origins,
    const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = state_; const auto target = std::move(program); const auto d = std::move(declaration);
    const auto authority = context; const auto callback = std::move(check);
    available_tensor_square_detail::recheck(pin->obligations, authority, callback);
    const auto original = pin->square.parent();
    const RelationalInputSubmission submission{original.accession().asset(), d, original.use(), pin->square.lifted_field()};
    target.check_inputs({{submission, origins}}, authority, callback);
    auto obligations = pin->obligations;
    const auto downstream = available_tensor_square_detail::input_origins(target, submission, origins, authority);
    obligations.insert(obligations.end(), downstream.begin(), downstream.end());
    const OwnedRelationalInputAccession accession(submission.asset, target.program(), d, limits, callback);
    auto input = pin->square.admit(accession, limits, callback);
    return AvailableRelationalTensorSquareInput({std::move(input), {pin->program, pin->input, pin->declaration,
        target.program_origins(), origins}, target, std::move(obligations)});
}
inline AvailableRelationalTensorSquareInput admit_available_relational_tensor_square_input(
    RelationalKnownTensorSquareInput input, RelationalTensorSquareOrigins origins,
    const RelationalAvailabilityContext& context, std::function<void()> check) {
    const auto frozen = std::move(input); const auto keys = origins; const auto authority = context; const auto callback = std::move(check);
    const auto square = frozen.square(); const auto original = square.parent(); const auto next = frozen.input();
    const PreparedAvailableRelationalProgram parent(original.accession().program(), keys.parent_program, authority);
    const PreparedAvailableRelationalProgram target(next.accession().program(), keys.program, authority);
    const auto a = original.accession();
    const RelationalInputSubmission submission{a.asset(), a.declaration(), original.use(), original.source_field()};
    parent.check_inputs({{submission, keys.parent_input}}, authority, callback);
    auto obligations = available_tensor_square_detail::input_origins(parent, submission, keys.parent_input, authority);
    obligations.push_back(authority.require(keys.declaration, RelationalDependencyRole::Interface,
        relational_tensor_square_dependency_payload(parent.program(), square.declaration())));
    const auto b = next.accession();
    const RelationalInputSubmission derived{b.asset(), b.declaration(), next.use(), next.source_field()};
    target.check_inputs({{derived, keys.input}}, authority, callback);
    const auto downstream = available_tensor_square_detail::input_origins(target, derived, keys.input, authority);
    obligations.insert(obligations.end(), downstream.begin(), downstream.end());
    return AvailableRelationalTensorSquareInput({frozen, keys, target, std::move(obligations)});
}
inline AvailableRelationalTensorSquareResult AvailableRelationalTensorSquareInput::execute(
    const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this; const auto authority = context; const auto callback = std::move(check);
    pin.admit(authority, callback);
    auto result = pin.state_->program.execute({{pin.state_->input.input(), pin.state_->origins.input}}, authority, limits, callback);
    return {pin, std::move(result)};
}
} // namespace rfc::inference
