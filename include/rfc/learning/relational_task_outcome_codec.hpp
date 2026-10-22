#pragma once

#include "rfc/learning/relational_task_outcome.hpp"

namespace rfc::learning {
namespace task_outcome_codec_detail {
using namespace task_outcome_detail;
using namespace inference::field_interface_codec_detail;
inline constexpr auto magic = [] { auto m = goal_magic; m[4] = std::byte{'O'}; return m; }();
inline RelationalTaskPostconditionDeclaration task(Reader& r) {
    RelationalTaskPostconditionDeclaration d;
    d.key = r.u64(); d.revision = r.u64(); d.use = r.u64(); d.basis = r.u64();
    const auto n = r.count(8); require(n <= r.limits().max_dimension, "task direction read bound");
    for (std::size_t i = 0; i < n; ++i) d.directions.push_back(r.u64());
    d.metric = storage::temporal_codec_detail::matrix(r); d.target = storage::temporal_codec_detail::tensor(r);
    return d;
}
inline RelationalWorkerBindingDeclaration binding(Reader& r) {
    RelationalWorkerBindingDeclaration d;
    d.key = r.u64(); d.revision = r.u64(); d.worker = r.u64(); d.machine = r.u64(); d.operation = r.u64();
    d.interface.key = r.u64(); d.interface.branch = r.u64(); const auto n = r.count(14);
    require(n <= r.limits().max_dimension, "task observation patch read bound");
    for (std::size_t i = 0; i < n; ++i) {
        const auto node = r.u64(); d.interface.cells.push_back({node, local_cell(r)});
    }
    d.view = inference::field_interface_codec_detail::view(r); return d;
}
inline Bytes proof(const RelationalTaskPostconditionCertificate& p) {
    auto w = envelope(magic); w.u8(static_cast<std::uint8_t>(p.status)); w.u64(p.rank);
    storage::temporal_codec_detail::tensor(w, p.residual.particular);
    storage::temporal_codec_detail::matrix(w, p.residual.directions);
    relational_affine_codec_detail::vector(w, p.feasible_shift);
    storage::temporal_codec_detail::matrix(w, p.feasible_kernel);
    relational_affine_codec_detail::vector(w, p.contradiction); return finish(std::move(w));
}
inline storage::PreparedBytePacket prepare(const OwnedRelationalTaskOutcome& outcome, const Context& context,
    relational_affine_codec_detail::PacketContext& packets) {
    const auto pin = outcome; pin.admit(context, packets.check);
    const auto& t = pin.task(); const auto& b = pin.binding(); const auto l = pin.limits();
    storage::BytePacketBuilder w(magic, packets.max_bytes, packets.byte_guard.get());
    w.u64(l.dimension); w.u64(l.entries); w.u64(l.steps); w.u64(l.integer_bits);
    w.write([&](auto& raw) { task_outcome_detail::task(raw, t); task_outcome_detail::binding(raw, b); });
    w.blob(packets.asset(pin.contract_asset()));
    const auto response = pin.response();
    const auto goal = context.require(t.origin, Role::Prospective, relational_task_goal_payload(t, pin.contract_asset(), response));
    const auto provider = context.require(b.origin, Role::Maps, relational_worker_binding_payload(b, response));
    w.u64(t.origin); w.receipt(goal); w.u64(b.origin); w.receipt(provider);
    w.u8(static_cast<std::uint8_t>(response.index()));
    std::visit([&](const auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, AvailableRelationalAffineResult>)
            w.blob(available_evidence_codec_detail::prepare_family(value, packets));
        else w.blob(inference::value_admission_codec_detail::prepare_result(value, packets));
    }, response);
    w.blob(proof(pin.certificate(context))); return std::move(w).finish(packets.check);
}
inline std::vector<Bytes> input_payloads(const inference::value_admission_codec_detail::PassiveAvailableResult& input) {
    std::vector<Bytes> out;
    for (const auto& v : input.inputs) out.push_back(inference::relational_input_accession_payload(
        input.program, v.asset, v.declaration, v.use, v.field));
    return out;
}
} // namespace task_outcome_codec_detail

template<class Consume> inline void stream_relational_task_outcome(const OwnedRelationalTaskOutcome& outcome,
    const inference::RelationalAvailabilityContext& context, Consume&& consume,
    std::size_t max_bytes = 256 * 1024 * 1024, std::function<void()> check = {}) {
    const auto authority = context;
    relational_affine_codec_detail::PacketContext packets(max_bytes, std::move(check));
    task_outcome_codec_detail::prepare(outcome, authority, packets).stream(std::forward<Consume>(consume), packets.check);
}
inline std::vector<std::byte> encode_relational_task_outcome(const OwnedRelationalTaskOutcome& outcome,
    const inference::RelationalAvailabilityContext& context, std::size_t max_bytes = 256 * 1024 * 1024,
    std::function<void()> check = {}) {
    const auto authority = context;
    relational_affine_codec_detail::PacketContext packets(max_bytes, std::move(check));
    return task_outcome_codec_detail::prepare(outcome, authority, packets).bytes();
}
inline OwnedRelationalTaskOutcome decode_relational_task_outcome(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context, const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap = {}, std::function<void()> check = {}) {
    const auto authority = context; const auto read_policy = policy; const auto callback = std::move(check);
    using namespace task_outcome_codec_detail;
    inference::value_admission_detail::bound(bytes.size(), read_policy.max_file_bytes);
    const Bytes frozen(bytes.begin(), bytes.end()); Reader r(frozen, read_policy); header(r, frozen, magic);
    const numeric::ExactActionLimits limits{size(r), size(r), size(r), size(r)};
    require(limits.dimension <= cap.dimension && limits.entries <= cap.entries && limits.steps <= cap.steps &&
                limits.integer_bits <= cap.integer_bits, "task outcome action policy exceeds caller cap");
    auto t = task(r); auto b = binding(r); const auto asset = r.blob();
    t.origin = r.u64(); const auto goal_receipt = r.blob();
    b.origin = r.u64(); const auto binding_receipt = r.blob();
    const auto kind = r.u8(); require(kind <= 1, "task response kind"); const auto child = r.blob();
    (void)r.blob(); require(!r.remaining(), "trailing task outcome bytes");
    using Passive = std::variant<inference::value_admission_codec_detail::PassiveAvailableResult,
                                 available_evidence_codec_detail::PassiveFamily>;
    const auto parsed = kind == 0
        ? Passive(inference::value_admission_codec_detail::preflight_result(child, authority, read_policy, callback))
        : Passive(available_evidence_codec_detail::preflight_family(child, authority, read_policy, limits, callback));
    const auto* input = std::visit([](const auto& p) -> const inference::value_admission_codec_detail::PassiveAvailableResult* {
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, available_evidence_codec_detail::PassiveFamily>) return &p.input;
        else return &p;
    }, parsed);
    const auto goal = authority.require(t.origin, Role::Prospective, goal_payload(t, asset, task_outcome_codec_detail::input_payloads(*input)));
    const auto provider = authority.require(b.origin, Role::Maps, binding_payload(b, input->program));
    canonical(goal_receipt, goal.receipt()); canonical(binding_receipt, provider.receipt());
    // All original origins, task instructions and provider bindings are checked
    // before source restoration, fitting or residual arithmetic.
    auto original = inference::decode_relational_input_use_asset(asset, read_policy, limits);
    auto response = std::visit([&](const auto& p) -> RelationalTaskResponse {
        if constexpr (std::is_same_v<std::decay_t<decltype(p)>, available_evidence_codec_detail::PassiveFamily>)
            return available_evidence_codec_detail::replay_family(p, authority, read_policy, limits, callback);
        else return inference::value_admission_codec_detail::replay_result(p, authority, read_policy, limits, callback);
    }, parsed);
    OwnedRelationalTaskOutcome out(t, std::move(original), b, std::move(response), authority, limits, callback);
    canonical(frozen, encode_relational_task_outcome(out, authority, read_policy.max_file_bytes, callback)); return out;
}
} // namespace rfc::learning
