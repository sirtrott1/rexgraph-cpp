#pragma once

#include "rfc/learning/relational_available_finite_field.hpp"
#include "rfc/learning/relational_literal_finite_field_codec.hpp"

namespace rfc::learning {
struct RelationalLiteralFiniteFieldOrigins {
    std::vector<std::vector<std::uint64_t>> inputs;
    std::uint64_t interface{};
};
namespace available_literal_detail {
using namespace available_finite_detail;
inline constexpr std::array<std::byte, 8> interface_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Y'}, std::byte{'I'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> model_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Y'}, std::byte{'M'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Y'}, std::byte{'B'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> choice_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Y'}, std::byte{'C'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> completion_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Y'}, std::byte{'V'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline storage::PreparedBytePacket interface_payload(const Bytes& base,
    const storage::PreparedBytePacket& reader, const std::vector<std::vector<std::uint64_t>>& keys,
    const std::vector<Origin>& inputs, std::size_t max_bytes, const std::function<void()>& check) {
    storage::BytePacketBuilder w(interface_magic, max_bytes); w.blob(base); w.blob(reader); w.u64(keys.size());
    for (const auto& row : keys) {
        if (check) check();
        w.u64(row.size()); for (const auto key : row) w.u64(key);
    }
    w.u64(inputs.size()); for (const auto& origin : inputs) w.receipt(origin);
    return std::move(w).finish(check);
}
inline std::vector<Origin> input_origins(const PreparedAvailableRelationalFiniteFieldMap& base,
    const PreparedRelationalLiteralFiniteFieldMap& reader, const std::vector<std::vector<std::uint64_t>>& keys,
    const Context& context, std::size_t max_bytes, const std::function<void()>& check) {
    using inference::field_program_detail::require;
    base.admit(context, check); const auto map = reader.map(); const auto actual = base.map();
    require(reader.source().same_owner(base.source().finite()), "literal available map needs the same actual finite source");
    require(map.targets().size() == actual.targets().size(), "literal available target count differs");
    for (std::size_t i = 0; i < map.targets().size(); ++i) {
        const auto& target = map.targets()[i]; const auto& expected = actual.targets()[i];
        require(target.index() == expected.index(), "literal available target kind differs");
        std::visit([&](const auto& t) {
            require(t.same_owner(std::get<std::decay_t<decltype(t)>>(expected)), "literal available target has a foreign actual owner");
        }, target);
    }
    canonical(encode_relational_finite_field_map(map), encode_relational_finite_field_map(actual));
    const auto& rules = reader.declaration().rules;
    require(keys.size() == rules.size(), "literal available origin rows omit rules");
    std::vector<Origin> out;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        require(keys[i].size() == rules[i].inputs.size(), "literal available origins omit input recipes");
        for (std::size_t j = 0; j < keys[i].size(); ++j) {
            if (check) check();
            const auto packet = inference::prepare_relational_literal_input(rules[i].inputs[j], max_bytes, check);
            out.push_back(context.require_bytes(keys[i][j], Role::Interface, packet.bytes(check)));
        }
    }
    return out;
}
} // namespace available_literal_detail

inline storage::PreparedBytePacket prepare_relational_literal_finite_interface_payload(
    const PreparedAvailableRelationalFiniteFieldMap& base, const PreparedRelationalLiteralFiniteFieldMap& reader,
    const std::vector<std::vector<std::uint64_t>>& origins, const inference::RelationalAvailabilityContext& context,
    std::size_t max_bytes, std::function<void()> check = {}) {
    const auto b = base; const auto r = reader; const auto keys = origins; const auto authority = context;
    const auto inputs = available_literal_detail::input_origins(b, r, keys, authority, max_bytes, check);
    return available_literal_detail::interface_payload(available_finite_detail::model_bytes(b),
        prepare_relational_literal_finite_field_map(r, max_bytes, check), keys, inputs, max_bytes, check);
}
inline std::vector<std::byte> relational_literal_finite_interface_payload(
    const PreparedAvailableRelationalFiniteFieldMap& base, const PreparedRelationalLiteralFiniteFieldMap& reader,
    const std::vector<std::vector<std::uint64_t>>& origins, const inference::RelationalAvailabilityContext& context,
    std::size_t max_bytes, std::function<void()> check = {}) {
    return prepare_relational_literal_finite_interface_payload(base, reader, origins, context, max_bytes, check).bytes(check);
}

class AvailableRelationalLiteralFiniteFieldResult;
// This owner adds reader obligations to the existing available finite map.
// Extracted raw maps/results retain their narrower raw contract.
class PreparedAvailableRelationalLiteralFiniteFieldMap {
    struct State {
        PreparedAvailableRelationalFiniteFieldMap base;
        PreparedRelationalLiteralFiniteFieldMap reader;
        RelationalLiteralFiniteFieldOrigins keys;
        std::vector<available_finite_detail::Origin> obligations;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedAvailableRelationalLiteralFiniteFieldMap(PreparedAvailableRelationalFiniteFieldMap base,
        PreparedRelationalLiteralFiniteFieldMap reader, RelationalLiteralFiniteFieldOrigins keys,
        const inference::RelationalAvailabilityContext& context, std::size_t max_bytes, std::function<void()> check = {}) {
        const auto b = base; const auto r = reader; const auto frozen = keys; const auto authority = context;
        auto inputs = available_literal_detail::input_origins(b, r, frozen.inputs, authority, max_bytes, check);
        const auto payload = available_literal_detail::interface_payload(available_finite_detail::model_bytes(b),
            prepare_relational_literal_finite_field_map(r, max_bytes, check), frozen.inputs, inputs, max_bytes, check);
        auto required = b.obligations(); required.insert(required.end(), inputs.begin(), inputs.end());
        required.push_back(authority.require_bytes(frozen.interface, inference::RelationalDependencyRole::Interface, payload.bytes(check)));
        state_ = std::make_shared<const State>(State{b, r, frozen, std::move(required)});
    }
    PreparedAvailableRelationalFiniteFieldMap base() const { return state_->base; }
    PreparedRelationalLiteralFiniteFieldMap reader() const { return state_->reader; }
    const RelationalLiteralFiniteFieldOrigins& origins() const & { return state_->keys; }
    const RelationalLiteralFiniteFieldOrigins& origins() const && = delete;
    const std::vector<available_finite_detail::Origin>& obligations() const & { return state_->obligations; }
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    bool same_owner(const PreparedAvailableRelationalLiteralFiniteFieldMap& other) const noexcept { return state_ == other.state_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context;
        available_finite_detail::recheck(pin->obligations, authority, check);
    }
    AvailableRelationalLiteralFiniteFieldResult execute(const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
};
class AvailableRelationalLiteralFiniteFieldBranch;
class AvailableRelationalLiteralFiniteFieldEvaluation;
class AvailableRelationalLiteralFiniteFieldResult {
    struct State { PreparedAvailableRelationalLiteralFiniteFieldMap model; AvailableRelationalFiniteFieldResult family; };
    std::shared_ptr<const State> state_;
    friend class PreparedAvailableRelationalLiteralFiniteFieldMap;
    explicit AvailableRelationalLiteralFiniteFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
public:
    PreparedAvailableRelationalLiteralFiniteFieldMap model() const { return state_->model; }
    RelationalFiniteFieldResult raw_result() const { return state_->family.result(); }
    std::size_t size() const noexcept { return state_->family.size(); }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { model().admit(context, std::move(check)); }
    std::vector<AvailableRelationalLiteralFiniteFieldBranch> select(std::size_t begin, std::size_t count,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    AvailableRelationalLiteralFiniteFieldEvaluation evaluate(std::size_t ordinal, numeric::RationalVector free,
        std::uint64_t parameter_setting, std::uint64_t choice, const inference::RelationalAvailabilityContext& context,
        std::size_t max_bytes, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    std::optional<available_finite_detail::Origin> setting(std::size_t ordinal, const numeric::RationalVector& free,
        std::uint64_t key, const inference::RelationalAvailabilityContext& context) const {
        admit(context); return available_finite_detail::setting(state_->family, ordinal, free, key, context);
    }
};
class AvailableRelationalLiteralFiniteFieldBranch {
    AvailableRelationalLiteralFiniteFieldResult parent_; RelationalFiniteFieldBranch branch_;
    friend class AvailableRelationalLiteralFiniteFieldResult;
    AvailableRelationalLiteralFiniteFieldBranch(AvailableRelationalLiteralFiniteFieldResult p, RelationalFiniteFieldBranch b)
        : parent_(std::move(p)), branch_(std::move(b)) {}
public:
    AvailableRelationalLiteralFiniteFieldResult parent() const { return parent_; }
    const RelationalFiniteFieldBranch& branch() const & { return branch_; }
    const RelationalFiniteFieldBranch& branch() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { parent_.admit(context, std::move(check)); }
};
inline AvailableRelationalLiteralFiniteFieldResult PreparedAvailableRelationalLiteralFiniteFieldMap::execute(
    const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto owner = *this; const auto authority = context; owner.admit(authority, check);
    return AvailableRelationalLiteralFiniteFieldResult({owner, owner.base().execute(authority, limits, std::move(check))});
}
inline std::vector<AvailableRelationalLiteralFiniteFieldBranch> AvailableRelationalLiteralFiniteFieldResult::select(
    std::size_t begin, std::size_t count, const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace inference::field_program_detail;
    const auto owner = *this; const auto authority = context; owner.admit(authority, check);
    Guard guard(limits, check); require(add(begin, count) <= owner.size(), "available literal branch selection bounds"); guard.entries(count);
    std::vector<AvailableRelationalLiteralFiniteFieldBranch> out; const auto raw = owner.raw_result();
    for (auto i = begin; i < add(begin, count); ++i) { guard.step(); out.push_back({owner, raw.branch(i)}); }
    return out;
}
namespace available_literal_detail {
inline storage::PreparedBytePacket model_bytes(const PreparedAvailableRelationalLiteralFiniteFieldMap& model,
    std::size_t max_bytes, const std::function<void()>& check) {
    const auto pin = model; storage::BytePacketBuilder w(model_magic, max_bytes);
    w.blob(available_finite_detail::model_bytes(pin.base())); w.blob(prepare_relational_literal_finite_field_map(pin.reader(), max_bytes, check));
    const auto& keys = pin.origins(); w.u64(keys.inputs.size());
    for (const auto& row : keys.inputs) { w.u64(row.size()); for (const auto key : row) w.u64(key); }
    w.u64(keys.interface); const auto base = pin.base(); const auto base_count = base.obligations().size();
    const auto& all = pin.obligations(); w.u64(all.size() - base_count);
    for (auto i = base_count; i < all.size(); ++i) w.receipt(all[i]);
    return std::move(w).finish(check);
}
inline storage::PreparedBytePacket result_bytes(const AvailableRelationalLiteralFiniteFieldResult& result,
    std::size_t max_bytes, const std::function<void()>& check) {
    const auto pin = result; storage::BytePacketBuilder w(result_magic, max_bytes);
    w.blob(model_bytes(pin.model(), max_bytes, check)); w.blob(encode_relational_finite_field_result(pin.raw_result()));
    return std::move(w).finish(check);
}
inline storage::PreparedBytePacket completion_payload(const AvailableRelationalLiteralFiniteFieldResult& family,
    std::size_t ordinal, const numeric::RationalVector& free, const std::optional<Origin>& setting,
    std::size_t max_bytes, const std::function<void()>& check) {
    storage::BytePacketBuilder w(completion_magic, max_bytes); w.blob(result_bytes(family, max_bytes, check)); w.u64(ordinal);
    w.write([&](auto& writer) { relational_affine_codec_detail::vector(writer, free); }); w.u8(setting ? 1 : 0);
    if (setting) w.receipt(*setting);
    return std::move(w).finish(check);
}
} // namespace available_literal_detail
inline std::vector<std::byte> relational_literal_finite_completion_payload(const AvailableRelationalLiteralFiniteFieldResult& family,
    std::size_t ordinal, const numeric::RationalVector& free, std::uint64_t parameter_setting,
    const inference::RelationalAvailabilityContext& context, std::size_t max_bytes, std::function<void()> check = {}) {
    const auto pin = family; const auto xi = free; const auto authority = context; pin.admit(authority, check);
    return available_literal_detail::completion_payload(pin, ordinal, xi, pin.setting(ordinal, xi, parameter_setting, authority), max_bytes, check).bytes(check);
}
class AvailableRelationalLiteralFiniteFieldEvaluation {
    AvailableRelationalLiteralFiniteFieldResult family_; RelationalFiniteFieldEvaluation native_;
    std::optional<available_finite_detail::Origin> setting_; available_finite_detail::Origin origin_;
    friend class AvailableRelationalLiteralFiniteFieldResult;
    AvailableRelationalLiteralFiniteFieldEvaluation(AvailableRelationalLiteralFiniteFieldResult f, RelationalFiniteFieldEvaluation n,
        std::optional<available_finite_detail::Origin> s, available_finite_detail::Origin o)
        : family_(std::move(f)), native_(std::move(n)), setting_(std::move(s)), origin_(std::move(o)) {}
public:
    AvailableRelationalLiteralFiniteFieldResult family() const { return family_; }
    RelationalFiniteFieldEvaluation raw_result() const { return native_; }
    std::optional<available_finite_detail::Origin> parameter_setting() const { return setting_; }
    available_finite_detail::Origin origin() const { return origin_; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = *this; const auto authority = context; pin.family_.admit(authority, check);
        if (pin.setting_) { if (check) check(); authority.recheck(*pin.setting_); }
        if (check) check();
        authority.recheck(pin.origin_);
    }
};
inline AvailableRelationalLiteralFiniteFieldEvaluation AvailableRelationalLiteralFiniteFieldResult::evaluate(
    std::size_t ordinal, numeric::RationalVector free, std::uint64_t parameter_setting, std::uint64_t choice,
    const inference::RelationalAvailabilityContext& context, std::size_t max_bytes,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto owner = *this; const auto xi = free; const auto authority = context; owner.admit(authority, check);
    const auto origin = owner.setting(ordinal, xi, parameter_setting, authority);
    const auto payload = available_literal_detail::completion_payload(owner, ordinal, xi, origin, max_bytes, check);
    const auto decision = authority.require_bytes(choice, inference::RelationalDependencyRole::Prospective, payload.bytes(check));
    return {owner, owner.raw_result().evaluate(ordinal, xi, limits, check), origin, decision};
}
} // namespace rfc::learning
