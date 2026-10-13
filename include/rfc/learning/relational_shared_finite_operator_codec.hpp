#pragma once

#include "rfc/storage/shared_packet_codec.hpp"
#include "rfc/learning/relational_available_finite_operator_codec.hpp"

namespace rfc::learning {
namespace shared_finite_operator_detail {
inline storage::SharedPacketLimits admitted(storage::SharedPacketLimits sharing,const storage::TemporalReadLimits& policy){
    sharing.max_stored_bytes=std::min(sharing.max_stored_bytes,policy.max_file_bytes);
    sharing.max_expanded_bytes=std::min(sharing.max_expanded_bytes,policy.max_file_bytes);return sharing;
}
} // namespace shared_finite_operator_detail
inline std::vector<std::byte> encode_shared_available_relational_finite_operator(const PreparedAvailableRelationalFiniteFieldOperator& op,
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto pin=op;const auto bytes=encode_available_relational_finite_operator(pin);return storage::encode_shared_packet(bytes,sharing,check);
}
inline PreparedAvailableRelationalFiniteFieldOperator decode_shared_available_relational_finite_operator(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto authority=context;const auto read=policy;const auto action=limits;const auto packing=shared_finite_operator_detail::admitted(sharing,read);
    const auto original=storage::decode_shared_packet(bytes,packing,check);return decode_available_relational_finite_operator(original,authority,read,action,check);
}
inline std::vector<std::byte> encode_shared_available_relational_finite_operator_instance(const AvailableRelationalFiniteOperatorInstance& instance,
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto pin=instance;const auto bytes=encode_available_relational_finite_operator_instance(pin);return storage::encode_shared_packet(bytes,sharing,check);
}
inline AvailableRelationalFiniteOperatorInstance decode_shared_available_relational_finite_operator_instance(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto authority=context;const auto read=policy;const auto action=limits;const auto packing=shared_finite_operator_detail::admitted(sharing,read);
    const auto original=storage::decode_shared_packet(bytes,packing,check);return decode_available_relational_finite_operator_instance(original,authority,read,action,check);
}
inline std::vector<std::byte> encode_shared_available_relational_finite_operator_result(const AvailableRelationalFiniteOperatorResult& family,
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto pin=family;const auto bytes=encode_available_relational_finite_operator_result(pin);return storage::encode_shared_packet(bytes,sharing,check);
}
inline AvailableRelationalFiniteOperatorResult decode_shared_available_relational_finite_operator_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto authority=context;const auto read=policy;const auto action=limits;const auto packing=shared_finite_operator_detail::admitted(sharing,read);
    const auto original=storage::decode_shared_packet(bytes,packing,check);return decode_available_relational_finite_operator_result(original,authority,read,action,check);
}
inline std::vector<std::byte> encode_shared_available_relational_finite_operator_evaluation(const AvailableRelationalFiniteOperatorEvaluation& evaluation,
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto pin=evaluation;const auto bytes=encode_available_relational_finite_operator_evaluation(pin);return storage::encode_shared_packet(bytes,sharing,check);
}
inline AvailableRelationalFiniteOperatorEvaluation decode_shared_available_relational_finite_operator_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},
    storage::SharedPacketLimits sharing={},std::function<void()> check={}){
    const auto authority=context;const auto read=policy;const auto action=limits;const auto packing=shared_finite_operator_detail::admitted(sharing,read);
    const auto original=storage::decode_shared_packet(bytes,packing,check);return decode_available_relational_finite_operator_evaluation(original,authority,read,action,check);
}
} // namespace rfc::learning
