#pragma once

#include "rfc/learning/relational_finite_field_codec.hpp"
#include "rfc/learning/relational_available_evidence_codec.hpp"

namespace rfc::learning {
enum class RelationalFiniteRelationshipStatus : std::uint8_t { Observed, Prospective };
struct RelationalFiniteSourceOrigins { std::uint64_t asset{}, constraints{}; };
struct RelationalFiniteMapOrigins { std::uint64_t maps{}, relationship{}; };
using AvailableRelationalFiniteFieldTarget = std::variant<inference::PreparedAvailableRelationalProgram,
    PreparedAvailableRelationalEvidence>;
struct RelationalAvailableFiniteFieldRule {
    std::uint64_t key{};
    std::vector<std::uint64_t> bindings, symbols;
    std::optional<std::size_t> target;
    std::vector<inference::RelationalAvailableInputSubmission> inputs;
    std::string unsupported;
};
struct RelationalAvailableFiniteFieldMapDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalAvailableFiniteFieldRule> rules;
};
namespace available_finite_detail {
using Bytes = std::vector<std::byte>;
using Origin = inference::OwnedRelationalValueOrigin;
using Context = inference::RelationalAvailabilityContext;
using Role = inference::RelationalDependencyRole;
using Status = RelationalFiniteRelationshipStatus;
using namespace inference::relational_finite_source_codec_detail;
inline constexpr std::array<std::byte, 8> source_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'S'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> sections_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'T'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> model_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'M'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'B'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> choice_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'C'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> constraint_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'P'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> interpretation_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'I'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> completion_magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Z'}, std::byte{'V'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline Role relationship_role(Status status) {
    inference::field_program_detail::require(status == Status::Observed || status == Status::Prospective, "finite relationship status");
    return status == Status::Observed ? Role::Evidence : Role::Prospective;
}
inline void recheck(const std::vector<Origin>& origins, const Context& context, const std::function<void()>& check) {
    for (const auto& origin : origins) { if (check) { check(); } context.recheck(origin); }
}
inline inference::PreparedAvailableRelationalProgram program(const AvailableRelationalFiniteFieldTarget& target) {
    return std::visit([](const auto& t) -> inference::PreparedAvailableRelationalProgram {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::PreparedAvailableRelationalProgram>) return t;
        else return t.program();
    }, target);
}
inline RelationalFiniteFieldTarget raw_target(const AvailableRelationalFiniteFieldTarget& target) {
    return std::visit([](const auto& t) -> RelationalFiniteFieldTarget {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::PreparedAvailableRelationalProgram>) return t.program();
        else return t.evidence();
    }, target);
}
inline Bytes target_bytes(const AvailableRelationalFiniteFieldTarget& target) {
    return std::visit([](const auto& t) -> Bytes {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, inference::PreparedAvailableRelationalProgram>) return inference::encode_available_relational_program(t);
        else return encode_available_relational_evidence(t);
    }, target);
}
inline std::vector<Origin> target_origins(const AvailableRelationalFiniteFieldTarget& target,
    const Context& context, const std::function<void()>& check) {
    return std::visit([&](const auto& t) { t.admit(context, check); return t.obligations(); }, target);
}
struct PassiveInput {
    Bytes asset;
    inference::RelationalInputAccessionDeclaration declaration;
    std::uint64_t use{};
    numeric::SparseRationalTensor field;
    inference::RelationalInputOrigins keys;
};
struct PassiveRule {
    std::uint64_t key{};
    std::vector<std::uint64_t> bindings, symbols;
    std::optional<std::size_t> target;
    std::vector<PassiveInput> inputs;
    std::string unsupported;
};
struct PassiveDeclaration { std::uint64_t key{}, revision{}; std::vector<PassiveRule> rules; };
inline PassiveDeclaration passive(const RelationalAvailableFiniteFieldMapDeclaration& d) {
    PassiveDeclaration out{d.key, d.revision, {}};
    for (const auto& r : d.rules) {
        PassiveRule row{r.key, r.bindings, r.symbols, r.target, {}, r.unsupported};
        for (const auto& input : r.inputs) row.inputs.push_back({inference::encode_relational_input_use_asset(input.input.asset),
            input.input.declaration, input.input.use, input.input.source_field, input.origins});
        out.rules.push_back(std::move(row));
    }
    return out;
}
inline void schema(Writer& w, const PassiveDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.rules.size());
    for (const auto& r : d.rules) {
        w.u64(r.key); finite_keys(w, r.bindings); finite_keys(w, r.symbols); w.u8(r.target ? 1 : 0); if (r.target) w.u64(*r.target);
        storage::temporal_codec_detail::string(w, r.unsupported); w.u64(r.inputs.size());
        for (const auto& input : r.inputs) {
            blob(w, input.asset); accession(w, input.declaration); w.u64(input.use); storage::temporal_codec_detail::tensor(w, input.field);
            for (const auto key : {input.keys.asset, input.keys.value, input.keys.accession, input.keys.reconstruction}) w.u64(key);
        }
    }
}
inline PassiveDeclaration schema(Reader& r) {
    PassiveDeclaration out; out.key = r.u64(); out.revision = r.u64(); const auto count = r.count(41);
    for (std::size_t i = 0; i < count; ++i) {
        PassiveRule row; row.key = r.u64(); row.bindings = finite_keys(r); row.symbols = finite_keys(r);
        if (r.flag()) row.target = size(r);
        row.unsupported = r.string(); const auto inputs = r.count(110);
        for (std::size_t j = 0; j < inputs; ++j) {
            const auto asset = r.blob(); auto d = accession(r); const auto use = r.u64(); auto field = storage::temporal_codec_detail::tensor(r);
            const inference::RelationalInputOrigins keys{r.u64(), r.u64(), r.u64(), r.u64()};
            row.inputs.push_back({Bytes(asset.begin(), asset.end()), std::move(d), use, std::move(field), keys});
        }
        out.rules.push_back(std::move(row));
    }
    return out;
}
inline std::vector<Origin> input_origins(const PassiveDeclaration& d, const Bytes& asset,
    const std::vector<Bytes>& programs, const Context& context, const std::function<void()>& check) {
    std::vector<Origin> out;
    for (const auto& r : d.rules) {
        inference::field_program_detail::require(r.target || r.inputs.empty(), "unsupported finite rule has numerical inputs");
        inference::field_program_detail::require(!r.target || *r.target < programs.size(), "finite target index");
        for (const auto& input : r.inputs) {
            if (check) { check(); } canonical(input.asset, asset);
            inference::value_admission_detail::input_origins(out, input.keys, input.asset,
                inference::relational_input_value_payload(input.asset, input.declaration, input.use, input.field),
                inference::relational_input_accession_payload(programs[*r.target], input.asset, input.declaration, input.use, input.field), context);
        }
    }
    return out;
}
inline Bytes interpretation_payload(const Bytes& source, const std::vector<std::pair<std::uint8_t, Bytes>>& targets,
    const PassiveDeclaration& d, Status status, const std::vector<Origin>& inputs) {
    (void)relationship_role(status); auto w = envelope(interpretation_magic); blob(w, source); w.u64(targets.size());
    for (const auto& [kind, bytes] : targets) { w.u8(kind); blob(w, bytes); }
    schema(w, d); w.u8(static_cast<std::uint8_t>(status)); w.u64(inputs.size());
    for (const auto& origin : inputs) { blob(w, origin.receipt()); } return finish(std::move(w));
}
} // namespace available_finite_detail
inline available_finite_detail::Bytes relational_finite_constraint_payload(std::span<const std::byte> asset,
    const inference::RelationalFiniteSourceDeclaration& declaration, RelationalFiniteRelationshipStatus status) {
    using namespace available_finite_detail; (void)relationship_role(status); auto w = envelope(constraint_magic);
    blob(w, asset); inference::relational_finite_source_codec_detail::declaration(w, declaration);
    w.u8(static_cast<std::uint8_t>(status)); return finish(std::move(w));
}
class AvailableRelationalFiniteSections;
class PreparedAvailableRelationalFiniteSource {
    struct State {
        inference::OwnedRelationalFiniteSource native;
        RelationalFiniteRelationshipStatus status;
        RelationalFiniteSourceOrigins keys;
        std::vector<available_finite_detail::Origin> obligations;
    };
public:
    PreparedAvailableRelationalFiniteSource(inference::OwnedRelationalInputUseAsset asset,
        inference::RelationalFiniteSourceDeclaration declaration, RelationalFiniteRelationshipStatus status,
        RelationalFiniteSourceOrigins origins, const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace available_finite_detail; const auto pin = asset; const auto d = declaration; const auto authority = context;
        const auto bytes = inference::encode_relational_input_use_asset(pin); std::vector<Origin> required;
        required.push_back(authority.require(origins.asset, Role::InputAsset, bytes));
        required.push_back(authority.require(origins.constraints, relationship_role(status), relational_finite_constraint_payload(bytes, d, status)));
        if (check) { check(); } inference::OwnedRelationalFiniteSource native(pin, d, limits, check);
        state_ = std::make_shared<const State>(State{std::move(native), status, origins, std::move(required)});
    }
    inference::OwnedRelationalFiniteSource finite() const { return state_->native; }
    RelationalFiniteRelationshipStatus status() const noexcept { return state_->status; }
    RelationalFiniteSourceOrigins origins() const noexcept { return state_->keys; }
    const std::vector<available_finite_detail::Origin>& obligations() const & { return state_->obligations; }
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    bool same_owner(const PreparedAvailableRelationalFiniteSource& other) const noexcept { return state_ == other.state_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; available_finite_detail::recheck(pin->obligations, authority, check);
    }
    AvailableRelationalFiniteSections sections(const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
namespace available_finite_detail {
inline Bytes source_bytes(const PreparedAvailableRelationalFiniteSource& source) {
    const auto pin = source; auto w = envelope(source_magic); blob(w, inference::encode_relational_finite_source(pin.finite()));
    w.u8(static_cast<std::uint8_t>(pin.status())); const auto keys = pin.origins(); w.u64(keys.asset); w.u64(keys.constraints);
    for (const auto& origin : pin.obligations()) { blob(w, origin.receipt()); } return finish(std::move(w));
}
inline std::vector<Origin> preflight_live(const PreparedAvailableRelationalFiniteSource& source,
    const std::vector<AvailableRelationalFiniteFieldTarget>& targets, const RelationalAvailableFiniteFieldMapDeclaration& d,
    const Context& context, const std::function<void()>& check) {
    source.admit(context, check); auto out = source.obligations(); std::vector<Bytes> programs;
    for (const auto& target : targets) {
        auto required = target_origins(target, context, check); out.insert(out.end(), required.begin(), required.end());
        programs.push_back(inference::encode_relational_field_program(program(target).program()));
    }
    const auto finite = source.finite(); const auto asset = finite.asset();
    for (const auto& rule : d.rules) for (const auto& input : rule.inputs)
        inference::field_program_detail::require(input.input.asset.same_owner(asset), "finite rule refers to a foreign actual source asset owner");
    auto inputs = input_origins(passive(d), inference::encode_relational_input_use_asset(asset), programs, context, check);
    out.insert(out.end(), inputs.begin(), inputs.end()); return out;
}
inline Bytes live_payload(const PreparedAvailableRelationalFiniteSource& source,
    const std::vector<AvailableRelationalFiniteFieldTarget>& targets, const RelationalAvailableFiniteFieldMapDeclaration& d,
    Status status, const Context& context, const std::function<void()>& check) {
    (void)preflight_live(source, targets, d, context, check); std::vector<std::pair<std::uint8_t, Bytes>> encoded; std::vector<Bytes> programs;
    for (const auto& target : targets) { encoded.push_back({static_cast<std::uint8_t>(target.index()), target_bytes(target)}); programs.push_back(inference::encode_relational_field_program(program(target).program())); }
    const auto finite = source.finite(); const auto inputs = input_origins(passive(d), inference::encode_relational_input_use_asset(finite.asset()), programs, context, check);
    return interpretation_payload(source_bytes(source), encoded, passive(d), status, inputs);
}
} // namespace available_finite_detail
inline available_finite_detail::Bytes relational_finite_interpretation_payload(const PreparedAvailableRelationalFiniteSource& source,
    const std::vector<AvailableRelationalFiniteFieldTarget>& targets, const RelationalAvailableFiniteFieldMapDeclaration& declaration,
    RelationalFiniteRelationshipStatus status, const inference::RelationalAvailabilityContext& context) {
    const auto source_pin = source; const auto frozen_targets = targets; const auto frozen = declaration; const auto authority = context;
    return available_finite_detail::live_payload(source_pin, frozen_targets, frozen, status, authority, {});
}
class AvailableRelationalFiniteSection;
class AvailableRelationalFiniteSections {
public:
    PreparedAvailableRelationalFiniteSource source() const { return source_; }
    inference::RelationalFiniteSections sections() const { return sections_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { const auto pin = *this; pin.source_.admit(context, std::move(check)); }
    std::vector<AvailableRelationalFiniteSection> select(std::size_t begin, std::size_t count,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedAvailableRelationalFiniteSource;
    AvailableRelationalFiniteSections(PreparedAvailableRelationalFiniteSource source, inference::RelationalFiniteSections sections)
        : source_(std::move(source)), sections_(std::move(sections)) {}
    PreparedAvailableRelationalFiniteSource source_; inference::RelationalFiniteSections sections_;
};
class AvailableRelationalFiniteSection {
public:
    AvailableRelationalFiniteSections parent() const { return parent_; }
    std::size_t ordinal() const noexcept { return ordinal_; }
    const std::vector<std::uint64_t>& assignment() const & { return sections_.assignment(ordinal_); }
    const std::vector<std::uint64_t>& assignment() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { parent_.admit(context, std::move(check)); }
private:
    friend class AvailableRelationalFiniteSections;
    AvailableRelationalFiniteSection(AvailableRelationalFiniteSections parent, std::size_t ordinal)
        : parent_(std::move(parent)), sections_(parent_.sections()), ordinal_(ordinal) {}
    AvailableRelationalFiniteSections parent_; inference::RelationalFiniteSections sections_; std::size_t ordinal_;
};
inline AvailableRelationalFiniteSections PreparedAvailableRelationalFiniteSource::sections(const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto owner = *this; const auto authority = context; owner.admit(authority, check); return {owner, owner.finite().sections(limits, check)};
}
inline std::vector<AvailableRelationalFiniteSection> AvailableRelationalFiniteSections::select(std::size_t begin, std::size_t count,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace inference::field_program_detail; const auto owner = *this; const auto authority = context; owner.admit(authority, check);
    Guard guard(limits, check); require(add(begin, count) <= owner.sections_.size(), "available finite section selection bounds"); guard.entries(count);
    std::vector<AvailableRelationalFiniteSection> out; for (auto i = begin; i < add(begin, count); ++i) { guard.step(); out.push_back(AvailableRelationalFiniteSection(owner, i)); } return out;
}
class AvailableRelationalFiniteFieldResult;
class PreparedAvailableRelationalFiniteFieldMap {
    struct State {
        PreparedAvailableRelationalFiniteSource source;
        std::vector<AvailableRelationalFiniteFieldTarget> targets;
        RelationalAvailableFiniteFieldMapDeclaration declaration;
        RelationalFiniteRelationshipStatus status;
        RelationalFiniteMapOrigins keys;
        std::vector<available_finite_detail::Origin> obligations;
        PreparedRelationalFiniteFieldMap native;
    };
public:
    PreparedAvailableRelationalFiniteFieldMap(PreparedAvailableRelationalFiniteSource source,
        std::vector<AvailableRelationalFiniteFieldTarget> targets, RelationalAvailableFiniteFieldMapDeclaration declaration,
        RelationalFiniteRelationshipStatus status, RelationalFiniteMapOrigins origins, const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace available_finite_detail; const auto source_pin = source; const auto pins = targets; const auto d = declaration; const auto authority = context;
        auto required = preflight_live(source_pin, pins, d, authority, check); const auto payload = live_payload(source_pin, pins, d, status, authority, check);
        required.push_back(authority.require(origins.maps, Role::Maps, payload)); required.push_back(authority.require(origins.relationship, relationship_role(status), payload));
        inference::field_program_detail::Guard guard(limits, check); guard.entries(d.rules.size()); guard.dimension(pins.size());
        RelationalFiniteFieldMapDeclaration raw{d.key, d.revision, {}}; std::vector<RelationalFiniteFieldTarget> raw_targets;
        for (const auto& target : pins) raw_targets.push_back(raw_target(target));
        for (const auto& rule : d.rules) {
            guard.step(); RelationalFiniteFieldRule row{rule.key, rule.bindings, rule.symbols, rule.target, {}, rule.unsupported};
            for (const auto& input : rule.inputs) row.inputs.push_back(program(pins.at(*rule.target)).admit_input(input.input, input.origins, authority, limits, check).input);
            raw.rules.push_back(std::move(row));
        }
        PreparedRelationalFiniteFieldMap native(source_pin.finite(), std::move(raw_targets), std::move(raw), limits, check);
        guard.step(); state_ = std::make_shared<const State>(State{source_pin, pins, d, status, origins, std::move(required), std::move(native)});
    }
    PreparedAvailableRelationalFiniteSource source() const { return state_->source; }
    const std::vector<AvailableRelationalFiniteFieldTarget>& targets() const & { return state_->targets; }
    const std::vector<AvailableRelationalFiniteFieldTarget>& targets() const && = delete;
    const RelationalAvailableFiniteFieldMapDeclaration& declaration() const & { return state_->declaration; }
    const RelationalAvailableFiniteFieldMapDeclaration& declaration() const && = delete;
    RelationalFiniteRelationshipStatus status() const noexcept { return state_->status; }
    RelationalFiniteMapOrigins origins() const noexcept { return state_->keys; }
    const std::vector<available_finite_detail::Origin>& obligations() const & { return state_->obligations; }
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    PreparedRelationalFiniteFieldMap map() const { return state_->native; }
    bool same_owner(const PreparedAvailableRelationalFiniteFieldMap& other) const noexcept { return state_ == other.state_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { const auto pin = state_; const auto authority = context; available_finite_detail::recheck(pin->obligations, authority, check); }
    AvailableRelationalFiniteFieldResult execute(const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class AvailableRelationalFiniteFieldBranch;
class AvailableRelationalFiniteFieldEvaluation;
class AvailableRelationalFiniteFieldResult {
    struct State { PreparedAvailableRelationalFiniteFieldMap model; RelationalFiniteFieldResult native; };
public:
    PreparedAvailableRelationalFiniteFieldMap model() const { return state_->model; }
    RelationalFiniteFieldResult result() const { return state_->native; }
    std::size_t size() const noexcept { return state_->native.size(); }
    const std::vector<available_finite_detail::Origin>& obligations() const & { return state_->model.obligations(); }
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { const auto pin = state_; pin->model.admit(context, std::move(check)); }
    std::vector<AvailableRelationalFiniteFieldBranch> select(std::size_t begin, std::size_t count,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    AvailableRelationalFiniteFieldEvaluation evaluate(std::size_t ordinal, numeric::RationalVector free,
        std::uint64_t parameter_setting, std::uint64_t choice_origin, const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedAvailableRelationalFiniteFieldMap;
    explicit AvailableRelationalFiniteFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
class AvailableRelationalFiniteFieldBranch {
public:
    AvailableRelationalFiniteFieldResult parent() const { return parent_; }
    std::size_t ordinal() const noexcept { return branch_.ordinal; }
    const RelationalFiniteFieldBranch& branch() const & { return branch_; }
    const RelationalFiniteFieldBranch& branch() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { parent_.admit(context, std::move(check)); }
private:
    friend class AvailableRelationalFiniteFieldResult;
    AvailableRelationalFiniteFieldBranch(AvailableRelationalFiniteFieldResult parent, RelationalFiniteFieldBranch branch)
        : parent_(std::move(parent)), branch_(std::move(branch)) {}
    AvailableRelationalFiniteFieldResult parent_; RelationalFiniteFieldBranch branch_;
};
inline AvailableRelationalFiniteFieldResult PreparedAvailableRelationalFiniteFieldMap::execute(const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto owner = *this; const auto authority = context; owner.admit(authority, check); const auto result = owner.map().execute(limits, check);
    if (check) { check(); } return AvailableRelationalFiniteFieldResult({owner, result});
}
inline std::vector<AvailableRelationalFiniteFieldBranch> AvailableRelationalFiniteFieldResult::select(std::size_t begin, std::size_t count,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace inference::field_program_detail; const auto owner = *this; const auto authority = context; owner.admit(authority, check);
    Guard guard(limits, check); require(add(begin, count) <= owner.size(), "available finite branch selection bounds"); guard.entries(count);
    std::vector<AvailableRelationalFiniteFieldBranch> out; for (auto i = begin; i < add(begin, count); ++i) { guard.step(); out.push_back(AvailableRelationalFiniteFieldBranch(owner, owner.state_->native.branch(i))); } return out;
}
namespace available_finite_detail {
inline Bytes model_bytes(const PreparedAvailableRelationalFiniteFieldMap& model) {
    const auto pin = model; auto w = envelope(model_magic); blob(w, source_bytes(pin.source())); w.u64(pin.targets().size());
    for (const auto& t : pin.targets()) { w.u8(static_cast<std::uint8_t>(t.index())); blob(w, target_bytes(t)); }
    schema(w, passive(pin.declaration())); w.u8(static_cast<std::uint8_t>(pin.status())); const auto keys = pin.origins(); w.u64(keys.maps); w.u64(keys.relationship);
    w.u64(pin.obligations().size()); for (const auto& origin : pin.obligations()) blob(w, origin.receipt());
    blob(w, encode_relational_finite_field_map(pin.map())); return finish(std::move(w));
}
inline Bytes result_bytes(const AvailableRelationalFiniteFieldResult& result) {
    const auto pin = result; auto w = envelope(result_magic); blob(w, model_bytes(pin.model())); blob(w, encode_relational_finite_field_result(pin.result())); return finish(std::move(w));
}
inline std::optional<Origin> setting(const AvailableRelationalFiniteFieldResult& family, std::size_t ordinal,
    const numeric::RationalVector& free, std::uint64_t key, const Context& context) {
    const auto raw = family.result(); const auto& branch = raw.branch(ordinal); const auto model = family.model();
    inference::field_program_detail::require(branch.status == RelationalFiniteFieldStatus::Interpreted && branch.rule && branch.value, "finite choice needs an interpreted branch");
    const auto& rules = model.declaration().rules; const auto rule = std::find_if(rules.begin(), rules.end(), [&](const auto& r) { return r.key == *branch.rule; });
    inference::field_program_detail::require(rule != rules.end() && rule->target, "finite choice loses declared target"); const auto& target = model.targets().at(*rule->target);
    if (const auto* evidence = std::get_if<PreparedAvailableRelationalEvidence>(&target)) {
        const auto native = evidence->evidence(); inference::field_program_detail::require(free.size() == native.kernel().cols(), "finite choice needs every actual common free axis");
        return context.require(key, Role::Prospective, relational_parameter_setting_payload(*evidence, free));
    }
    inference::field_program_detail::require(!key && free.empty(), "fixed finite choice has no global parameter setting"); return {};
}
inline Bytes completion_payload(const Bytes& family, std::size_t ordinal, const numeric::RationalVector& free,
    const std::optional<Origin>& setting) {
    auto w = envelope(completion_magic); blob(w, family); w.u64(ordinal); relational_affine_codec_detail::vector(w, free); w.u8(setting ? 1 : 0);
    if (setting) { blob(w, setting->receipt()); } return finish(std::move(w));
}
} // namespace available_finite_detail
inline available_finite_detail::Bytes relational_finite_completion_payload(const AvailableRelationalFiniteFieldResult& family,
    std::size_t ordinal, const numeric::RationalVector& free, std::uint64_t parameter_setting,
    const inference::RelationalAvailabilityContext& context) {
    const auto owner = family; const auto frozen = free; const auto authority = context; owner.admit(authority);
    return available_finite_detail::completion_payload(available_finite_detail::result_bytes(owner), ordinal, frozen,
        available_finite_detail::setting(owner, ordinal, frozen, parameter_setting, authority));
}
class AvailableRelationalFiniteFieldEvaluation {
public:
    AvailableRelationalFiniteFieldResult family() const { return family_; }
    RelationalFiniteFieldEvaluation result() const { return native_; }
    std::size_t ordinal() const noexcept { return native_.ordinal(); }
    std::optional<available_finite_detail::Origin> parameter_setting() const { return setting_; }
    available_finite_detail::Origin origin() const { return origin_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = *this; const auto authority = context; pin.family_.admit(authority, check);
        if (pin.setting_) { if (check) { check(); } authority.recheck(*pin.setting_); }
        if (check) { check(); } authority.recheck(pin.origin_);
    }
private:
    friend class AvailableRelationalFiniteFieldResult;
    AvailableRelationalFiniteFieldEvaluation(AvailableRelationalFiniteFieldResult family, RelationalFiniteFieldEvaluation native,
        std::optional<available_finite_detail::Origin> setting, available_finite_detail::Origin origin)
        : family_(std::move(family)), native_(std::move(native)), setting_(std::move(setting)), origin_(std::move(origin)) {}
    AvailableRelationalFiniteFieldResult family_; RelationalFiniteFieldEvaluation native_;
    std::optional<available_finite_detail::Origin> setting_; available_finite_detail::Origin origin_;
};
inline AvailableRelationalFiniteFieldEvaluation AvailableRelationalFiniteFieldResult::evaluate(std::size_t ordinal,
    numeric::RationalVector free, std::uint64_t parameter_setting, std::uint64_t choice_origin,
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto owner = *this; const auto frozen = free; const auto authority = context; owner.admit(authority, check);
    const auto setting = available_finite_detail::setting(owner, ordinal, frozen, parameter_setting, authority);
    const auto payload = available_finite_detail::completion_payload(available_finite_detail::result_bytes(owner), ordinal, frozen, setting);
    const auto actual = authority.require(choice_origin, available_finite_detail::Role::Prospective, payload); if (check) { check(); }
    return {owner, owner.result().evaluate(ordinal, frozen, limits, check), setting, actual};
}
} // namespace rfc::learning
