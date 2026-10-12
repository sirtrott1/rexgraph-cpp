#pragma once

#include "rfc/learning/relational_finite_field.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include "rfc/learning/relational_affine_program_codec.hpp"

namespace rfc::learning {
namespace relational_finite_field_codec_detail {
using namespace inference::relational_finite_source_codec_detail;
inline constexpr std::array<std::byte, 8> map_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Q'}, std::byte{'M'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> result_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'Q'}, std::byte{'B'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline std::vector<std::byte> target(const RelationalFiniteFieldTarget& t) {
    return std::visit([](const auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, inference::OwnedRelationalFieldProgram>) return inference::encode_relational_field_program(value);
        else return encode_relational_field_evidence(value);
    }, t);
}
inline std::vector<std::byte> value(const RelationalFiniteFieldValue& v) {
    return std::visit([](const auto& result) {
        if constexpr (std::is_same_v<std::decay_t<decltype(result)>, inference::OwnedAccessionFieldResult>) return inference::encode_accession_field_result(result);
        else return encode_relational_affine_result(result);
    }, v);
}
inline std::vector<std::byte> witness(const RelationalFiniteFieldResult& result) {
    Writer w; const auto sections = result.sections(); w.u64(sections.candidate_count()); w.u64(sections.size());
    for (std::size_t i = 0; i < sections.size(); ++i) finite_keys(w, sections.assignment(i));
    w.u64(result.size()); for (std::size_t i = 0; i < result.size(); ++i) {
        const auto& b = result.branch(i); w.u64(b.ordinal); w.u64(b.section); w.u8(b.rule ? 1 : 0); if (b.rule) w.u64(*b.rule);
        w.u8(static_cast<std::uint8_t>(b.status)); w.u8(b.value ? 1 : 0);
        if (b.value) { w.u8(static_cast<std::uint8_t>(b.value->index())); blob(w, value(*b.value)); }
    }
    return std::move(w).take_bytes();
}
} // namespace relational_finite_field_codec_detail
inline std::vector<std::byte> encode_relational_finite_field_map(const PreparedRelationalFiniteFieldMap& map) {
    using namespace relational_finite_field_codec_detail; const auto pin = map; auto w = envelope(map_magic);
    blob(w, inference::encode_relational_finite_source(pin.source())); w.u64(pin.targets().size());
    for (const auto& t : pin.targets()) { w.u8(static_cast<std::uint8_t>(t.index())); blob(w, target(t)); }
    const auto& d = pin.declaration(); w.u64(d.key); w.u64(d.revision); w.u64(d.rules.size());
    for (const auto& r : d.rules) {
        w.u64(r.key); finite_keys(w, r.bindings); finite_keys(w, r.symbols); w.u8(r.target ? 1 : 0); if (r.target) w.u64(*r.target);
        storage::temporal_codec_detail::string(w, r.unsupported); w.u64(r.inputs.size()); for (const auto& input : r.inputs) {
            const auto a = input.accession(); accession(w, a.declaration()); w.u64(input.use()); storage::temporal_codec_detail::tensor(w, input.source_field());
        }
    }
    return finish(std::move(w));
}
inline PreparedRelationalFiniteFieldMap decode_relational_finite_field_map(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace relational_finite_field_codec_detail; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); bytes = image; Reader r(bytes, read_policy); header(r, bytes, map_magic);
    auto source = inference::decode_relational_finite_source(r.blob(), read_policy, limits, check); std::vector<RelationalFiniteFieldTarget> targets;
    const auto count = r.count(9); for (std::size_t i = 0; i < count; ++i) {
        if (check) check();
        const auto kind = r.u8(); const auto data = r.blob();
        inference::field_program_detail::require(kind < 2, "unknown finite target discriminant");
        if (!kind) targets.push_back(inference::decode_relational_field_program(data, read_policy, limits));
        else targets.push_back(decode_relational_field_evidence(data, read_policy, limits));
    }
    RelationalFiniteFieldMapDeclaration d; d.key = r.u64(); d.revision = r.u64(); const auto rules = r.count(41);
    for (std::size_t i = 0; i < rules; ++i) {
        if (check) check();
        RelationalFiniteFieldRule rule; rule.key = r.u64(); rule.bindings = finite_keys(r); rule.symbols = finite_keys(r);
        if (r.flag()) rule.target = size(r);
        rule.unsupported = r.string(); const auto inputs = r.count(70);
        inference::field_program_detail::require(!rule.target || *rule.target < targets.size(), "serialized finite target index");
        inference::field_program_detail::require(rule.target || !inputs, "unsupported finite rule has serialized numerical input");
        for (std::size_t j = 0; j < inputs; ++j) {
            auto a = accession(r); const auto use = r.u64(); auto field = storage::temporal_codec_detail::tensor(r);
            const inference::OwnedRelationalInputAccession adopted(source.asset(), relational_finite_field_detail::program(targets[*rule.target]), std::move(a), limits, check);
            rule.inputs.push_back(adopted.admit(use, std::move(field), limits, check));
        }
        d.rules.push_back(std::move(rule));
    }
    inference::field_program_detail::require(!r.remaining(), "trailing finite map bytes");
    PreparedRelationalFiniteFieldMap out(std::move(source), std::move(targets), std::move(d), limits, check);
    canonical(bytes, encode_relational_finite_field_map(out)); return out;
}
inline std::vector<std::byte> encode_relational_finite_field_result(const RelationalFiniteFieldResult& result) {
    using namespace relational_finite_field_codec_detail; const auto pin = result; auto w = envelope(result_magic);
    blob(w, encode_relational_finite_field_map(pin.map())); blob(w, witness(pin)); return finish(std::move(w));
}
inline RelationalFiniteFieldResult decode_relational_finite_field_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace relational_finite_field_codec_detail; const auto read_policy = policy;
    const auto image = frozen_bytes(bytes, read_policy); bytes = image; Reader r(bytes, read_policy); header(r, bytes, result_magic);
    const auto map = decode_relational_finite_field_map(r.blob(), read_policy, limits, check); const auto expected = r.blob();
    inference::field_program_detail::require(!r.remaining(), "trailing finite branch result bytes");
    const auto out = map.execute(limits, check); canonical(expected, witness(out)); canonical(bytes, encode_relational_finite_field_result(out)); return out;
}
} // namespace rfc::learning
