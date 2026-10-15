#pragma once

#include "rfc/learning/relational_four_use_moment_basis.hpp"
#include "rfc/learning/relational_available_tensor_square_evidence_codec.hpp"

namespace rfc::learning {
namespace four_use_grammar_codec_detail {
using Context=inference::RelationalAvailabilityContext;
using Policy=storage::TemporalReadLimits;
using Limits=numeric::ExactActionLimits;
using Check=std::function<void()>;
inline void image(const inference::available_tensor_square_codec_detail::PassiveInput& parsed,const Check& check={}) {
    inference::field_program_detail::require(parsed.parent_field.has_value(),"original teacher grammar field required");
    four_use_grammar_detail::require_image(parsed.parent_declaration,*parsed.parent_field,check);
}
inline void image(const tensor_square_evidence_codec_detail::PassiveEvidence& parsed,const Check& check={}) {
    for(const auto& update:parsed.updates)for(const auto& observation:update){
        inference::field_program_detail::require(observation.lineage.has_value(),"teacher grammar model requires complete original/initial lineage");
        image(observation.input.input,check);
    }
}
inline void image(const tensor_square_evidence_codec_detail::PassiveFamily& parsed,const Check& check={}) {
    image(parsed.evidence,check);
    inference::field_program_detail::require(parsed.lineage.has_value(),"teacher grammar query requires original/initial lineage");
    image(parsed.input.input,check);
}
inline void model(const tensor_square_evidence_codec_detail::PassiveEvidence& parsed,std::span<const std::byte> expected) {
    if(expected.empty())return;
    const auto actual=relational_evidence_root_payload(parsed.parsed.program.native,parsed.parsed.declaration,parsed.parsed.limits);
    inference::field_program_detail::require(expected.size()==actual.size()&&std::equal(expected.begin(),expected.end(),actual.begin()),
        "declared four-use root program/parameters/limits contract differs");
}
} // namespace four_use_grammar_codec_detail

struct AvailableRelationalFourUseQuery {
    inference::AvailableRelationalTensorSquareResult input;
    inference::AvailableRelationalInitialInputLineage lineage;
};
namespace four_use_grammar_codec_detail {
inline AvailableRelationalFourUseQuery decode_frozen_query(std::span<const std::byte> input_bytes,std::span<const std::byte> lineage_bytes,
    const Context& context,const Policy& policy,Limits limits={},Check check={},std::optional<inference::OwnedRelationalFieldProgram> target={}) {
    const auto authority=context;const auto callback=std::move(check);if(callback){callback();}
    const auto input=tensor_square_evidence_codec_detail::preflight_input(input_bytes,authority,policy,callback);
    const auto initial=inference::initial_input_lineage_codec_detail::preflight(lineage_bytes,authority,policy,callback);
    inference::field_program_detail::require(initial.initial.size()==input.input.parent_asset.size()&&
        std::equal(initial.initial.begin(),initial.initial.end(),input.input.parent_asset.begin()),"paired query initial source differs from actual product parent");
    image(input.input,callback);
    // Validate the complete old-to-initial declaration before known product.
    const auto checked=inference::decode_available_relational_initial_input_lineage(lineage_bytes,authority,policy,limits,callback);
    auto result=inference::available_tensor_square_codec_detail::decode_frozen_result(input_bytes,authority,policy,limits,callback,std::move(target));
    const auto compound=result.input();const auto native=compound.native_input();const auto square=native.square();const auto old=checked.lineage();
    // Explicit cold adoption: original owner retained, initial owner belongs
    // to this actual restored product. Equal live owners are never coerced.
    const inference::OwnedRelationalInitialInputLineage joined(old.original(),square.parent().accession().asset(),old.declaration(),limits,callback);
    return {std::move(result),inference::AvailableRelationalInitialInputLineage(joined,checked.origins(),authority)};
}
} // namespace four_use_grammar_codec_detail
inline AvailableRelationalFourUseQuery decode_available_relational_four_use_query(std::span<const std::byte> input_bytes,
    std::span<const std::byte> lineage_bytes,const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={},std::optional<inference::OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;const auto input=inference::known_tensor_square_codec_detail::freeze(input_bytes,policy);
    const auto lineage=inference::known_tensor_square_codec_detail::freeze(lineage_bytes,policy);const auto authority=context;
    return four_use_grammar_codec_detail::decode_frozen_query(input,lineage,authority,policy,limits,std::move(check),std::move(target));
}
inline AvailableRelationalFourUseQuery decode_shared_available_relational_four_use_query(std::span<const std::byte> input_bytes,
    std::span<const std::byte> lineage_bytes,const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},storage::SharedPacketLimits storage_limits={},std::function<void()> check={},
    std::optional<inference::OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;const auto lineage=inference::known_tensor_square_codec_detail::freeze(lineage_bytes,policy);
    const auto authority=context;const auto callback=std::move(check);storage_limits.max_expanded_bytes=std::min(storage_limits.max_expanded_bytes,policy.max_file_bytes);
    const auto input=storage::decode_shared_packet(input_bytes,storage_limits,callback);
    return four_use_grammar_codec_detail::decode_frozen_query(input,lineage,authority,policy,limits,callback,std::move(target));
}

// Explicit passive profile inspections. They publish no execution authority.
// Atomic profiled decoders below pin the same bytes from check through replay.
inline void preflight_available_relational_four_use_tensor_square_input(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,std::function<void()> check={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    if(callback){callback();}const auto parsed=inference::available_tensor_square_codec_detail::preflight(frozen,authority,policy,callback);
    four_use_grammar_codec_detail::image(parsed,callback);
}
inline void preflight_available_relational_four_use_tensor_square_result(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,std::function<void()> check={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    if(callback){callback();}const auto parsed=tensor_square_evidence_codec_detail::preflight_input(frozen,authority,policy,callback);
    four_use_grammar_codec_detail::image(parsed.input,callback);
}
inline void preflight_available_relational_four_use_tensor_square_evidence(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    if(callback){callback();}const auto parsed=tensor_square_evidence_codec_detail::preflight_evidence(frozen,authority,policy,limits,callback);
    four_use_grammar_codec_detail::model(parsed,expected);
    four_use_grammar_codec_detail::image(parsed,callback);
}
inline void preflight_available_relational_four_use_tensor_square_affine_result(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    if(callback){callback();}const auto parsed=tensor_square_evidence_codec_detail::preflight_family(frozen,authority,policy,limits,callback);
    four_use_grammar_codec_detail::model(parsed.evidence,expected);
    four_use_grammar_codec_detail::image(parsed,callback);
}
inline inference::AvailableRelationalTensorSquareInput decode_available_relational_four_use_tensor_square_input(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    std::optional<inference::OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    return inference::available_tensor_square_codec_detail::decode_frozen_input(frozen,authority,policy,limits,callback,std::move(target),
        [&](const auto& parsed){four_use_grammar_codec_detail::image(parsed,callback);});
}
inline inference::AvailableRelationalTensorSquareResult decode_available_relational_four_use_tensor_square_result(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},
    std::optional<inference::OwnedRelationalFieldProgram> target={}) {
    const auto policy=caller_policy;
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    return inference::available_tensor_square_codec_detail::decode_frozen_result(frozen,authority,policy,limits,callback,std::move(target),
        [&](const auto& parsed){four_use_grammar_codec_detail::image(parsed,callback);});
}
inline PreparedAvailableRelationalTensorSquareEvidence decode_available_relational_four_use_tensor_square_evidence(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    return tensor_square_evidence_codec_detail::decode_frozen_evidence(frozen,authority,policy,limits,callback,
        [&](const auto& parsed){four_use_grammar_codec_detail::model(parsed,expected);four_use_grammar_codec_detail::image(parsed,callback);});
}
inline AvailableRelationalTensorSquareAffineResult decode_available_relational_four_use_tensor_square_affine_result(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto frozen=inference::known_tensor_square_codec_detail::freeze(bytes,policy);const auto authority=context;const auto callback=std::move(check);
    return tensor_square_evidence_codec_detail::decode_frozen_family(frozen,authority,policy,limits,callback,
        [&](const auto& parsed){four_use_grammar_codec_detail::model(parsed.evidence,expected);four_use_grammar_codec_detail::image(parsed,callback);});
}
inline PreparedAvailableRelationalTensorSquareEvidence decode_shared_available_relational_four_use_tensor_square_evidence(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits storage_limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto authority=context;const auto callback=std::move(check);storage_limits.max_expanded_bytes=std::min(storage_limits.max_expanded_bytes,policy.max_file_bytes);
    const auto frozen=storage::view_shared_packet(bytes,storage_limits,callback);
    return tensor_square_evidence_codec_detail::decode_image_evidence(frozen,authority,policy,limits,callback,
        [&](const auto& parsed){four_use_grammar_codec_detail::model(parsed,expected);four_use_grammar_codec_detail::image(parsed,callback);});
}
inline AvailableRelationalTensorSquareAffineResult decode_shared_available_relational_four_use_tensor_square_affine_result(std::span<const std::byte> bytes,
    const four_use_grammar_codec_detail::Context& context,const storage::TemporalReadLimits& caller_policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits storage_limits={},std::function<void()> check={},std::span<const std::byte> expected_root_payload={}) {
    const auto policy=caller_policy;
    const auto expected=inference::known_tensor_square_codec_detail::freeze(expected_root_payload,policy);
    const auto authority=context;const auto callback=std::move(check);storage_limits.max_expanded_bytes=std::min(storage_limits.max_expanded_bytes,policy.max_file_bytes);
    const auto frozen=storage::view_shared_packet(bytes,storage_limits,callback);
    return tensor_square_evidence_codec_detail::decode_image_family(frozen,authority,policy,limits,callback,
        [&](const auto& parsed){four_use_grammar_codec_detail::model(parsed.evidence,expected);four_use_grammar_codec_detail::image(parsed,callback);});
}
} // namespace rfc::learning
