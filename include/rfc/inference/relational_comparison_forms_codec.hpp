#pragma once

#include "rfc/inference/relational_comparison_forms.hpp"
#include "rfc/inference/relational_field_program_codec.hpp"

namespace rfc::inference {
namespace relational_comparison_codec_detail {
using namespace field_program_codec_detail;
inline constexpr std::array<std::byte, 8> forms_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'C'}, std::byte{'F'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> comparison_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'C'}, std::byte{'R'}, std::byte{1}, std::byte{0}, std::byte{0}};
} // namespace relational_comparison_codec_detail
inline std::vector<std::byte> encode_relational_comparison_forms(const PreparedRelationalComparisonForms& forms) {
    using namespace relational_comparison_codec_detail;
    const auto pin = forms; auto w = envelope(forms_magic); blob(w, encode_relational_field_program(pin.program()));
    const auto& d = pin.declaration(); w.u64(d.key); w.u64(d.revision); w.u16(d.grade.value); w.u8(static_cast<std::uint8_t>(d.participation));
    w.u64(d.weights.size());
    for (const auto& weight : d.weights) {
        w.u16(weight.cell.grade.value); w.u32(weight.cell.index); storage::temporal_codec_detail::rational(w, weight.value);
    }
    return finish(std::move(w));
}
inline PreparedRelationalComparisonForms decode_relational_comparison_forms(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace relational_comparison_codec_detail;
    Reader r(bytes, policy); header(r, bytes, forms_magic); auto program = decode_relational_field_program(r.blob(), policy, limits);
    RelationalComparisonDeclaration d; d.key = r.u64(); d.revision = r.u64(); d.grade = {r.u16()};
    d.participation = static_cast<RelationalComparisonParticipation>(r.u8()); const auto count = r.count(16);
    for (std::size_t i = 0; i < count; ++i) {
        const auto g = r.u16(); const auto index = r.u32(); d.weights.push_back({{core::Grade{g}, index}, r.rational()});
    }
    field_program_detail::require(r.remaining() == 0, "trailing relational comparison form bytes");
    PreparedRelationalComparisonForms forms(std::move(program), std::move(d), limits);
    canonical(bytes, encode_relational_comparison_forms(forms)); return forms;
}
inline std::vector<std::byte> encode_relational_comparison_result(const RelationalComparisonResult& result) {
    using namespace relational_comparison_codec_detail;
    const auto pin = result; auto w = envelope(comparison_magic); blob(w, encode_relational_comparison_forms(pin.forms()));
    w.u8(static_cast<std::uint8_t>(pin.channel())); storage::temporal_codec_detail::tensor(w, pin.input());
    storage::temporal_codec_detail::tensor(w, pin.output()); storage::temporal_codec_detail::tensor(w, pin.signed_boundary_field());
    storage::temporal_codec_detail::tensor(w, pin.unsigned_boundary_field()); w.u64(pin.occurrence_fields().size());
    for (const auto& field : pin.occurrence_fields()) storage::temporal_codec_detail::tensor(w, field);
    storage::temporal_codec_detail::rational(w, pin.quadrance()); return finish(std::move(w));
}
inline RelationalComparisonResult decode_relational_comparison_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy, numeric::ExactActionLimits limits = {}) {
    using namespace relational_comparison_codec_detail;
    Reader r(bytes, policy); header(r, bytes, comparison_magic); const auto forms = decode_relational_comparison_forms(r.blob(), policy, limits);
    const auto channel = static_cast<RelationalComparisonChannel>(r.u8()); auto input = storage::temporal_codec_detail::tensor(r);
    auto result = forms.apply(channel, std::move(input), limits);
    field_program_detail::require(same_field(storage::temporal_codec_detail::tensor(r), result.output()), "cold whole comparison action differs");
    field_program_detail::require(same_field(storage::temporal_codec_detail::tensor(r), result.signed_boundary_field()), "cold weighted signed boundary differs");
    field_program_detail::require(same_field(storage::temporal_codec_detail::tensor(r), result.unsigned_boundary_field()), "cold weighted unsigned boundary differs");
    const auto count = r.count(25); field_program_detail::require(count == result.occurrence_fields().size(), "cold comparison loses original flags");
    for (const auto& expected : result.occurrence_fields())
        field_program_detail::require(same_field(storage::temporal_codec_detail::tensor(r), expected), "cold complete original flag comparison field differs");
    field_program_detail::require(r.rational() == result.quadrance(), "cold comparison quadrance differs");
    field_program_detail::require(r.remaining() == 0, "trailing relational comparison result bytes");
    canonical(bytes, encode_relational_comparison_result(result)); return result;
}
} // namespace rfc::inference
