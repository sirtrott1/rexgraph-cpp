#pragma once

#include "rfc/learning/relational_available_finite_operator.hpp"

namespace rfc::learning {
namespace available_finite_operator_codec_detail {
using namespace available_finite_operator_detail;
using PassiveModel=available_finite_codec_detail::PassiveModel;
using PassiveSource=available_finite_codec_detail::PassiveSource;
struct PassiveOperator {
    Bytes archive;PassiveModel reference;RelationalFiniteOperatorDeclaration declaration;Status status;
    RelationalFiniteOperatorOrigins keys;std::vector<Origin> required;Bytes native;
};
inline void receipts(Reader& r,const std::vector<Origin>& required) {
    inference::field_program_detail::require(r.count(8)==required.size(),"available operator complete receipt count");
    for(const auto& origin:required)canonical(r.blob(),origin.receipt());
}
inline PassiveOperator preflight_operator(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,operator_magic);auto reference=available_finite_codec_detail::preflight_model(r.blob(),context,policy,limits,check);
    auto d=finite_operator_codec_detail::operator_declaration(r);const auto status=static_cast<Status>(r.u8());const RelationalFiniteOperatorOrigins keys{r.u64(),r.u64()};
    auto required=reference.required;const auto payload=operator_payload(reference.archive,d,status);
    if(check){check();}required.push_back(context.require(keys.maps,Role::Maps,payload));
    if(check){check();}required.push_back(context.require(keys.relationship,relationship_role(status),payload));receipts(r,required);
    const auto native=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing available operator");
    return {Bytes(bytes.begin(),bytes.end()),std::move(reference),std::move(d),status,keys,std::move(required),Bytes(native.begin(),native.end())};
}
inline PreparedAvailableRelationalFiniteFieldOperator replay_operator(const PassiveOperator& parsed,const Context& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,const std::function<void()>& check) {
    recheck(parsed.required,context,check);const auto reference=available_finite_codec_detail::replay_model(parsed.reference,context,policy,limits,check);
    PreparedAvailableRelationalFiniteFieldOperator op(reference,parsed.declaration,parsed.status,parsed.keys,context,limits,check);
    canonical(parsed.native,encode_relational_finite_operator(op.operator_program()));canonical(parsed.archive,operator_bytes(op));return op;
}
struct PassiveInstance {
    Bytes archive;PassiveOperator op;PassiveSource source;RelationalFiniteOperatorInstanceDeclaration declaration;
    std::vector<PassivePort> rows;Status status;RelationalFiniteOperatorInstanceOrigins keys;
    std::vector<Origin> required;PassiveModel child;Bytes native;
};
inline Bytes schema_bytes(const PassiveDeclaration& d) {Writer w;schema(w,d);return std::move(w).take_bytes();}
inline Bytes ports_bytes(const std::vector<PassivePort>& rows) {Writer w;ports(w,rows);return std::move(w).take_bytes();}
inline PassiveInstance preflight_instance(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,instance_magic);auto op=preflight_operator(r.blob(),context,policy,limits,check);
    auto source=available_finite_codec_detail::preflight_source(r.blob(),context,policy,check);const auto d=finite_operator_codec_detail::instance_declaration(r);
    auto rows=ports(r);const auto status=static_cast<Status>(r.u8());const auto keys=instance_keys(r);
    numeric::ExactActionGuard guard(limits,check);const auto canonical_rows=ordered(op.declaration,rows,guard);canonical(ports_bytes(rows),ports_bytes(canonical_rows));
    const auto child_d=child_declaration(op.reference.declaration,op.declaration,d,canonical_rows,guard);
    std::vector<Bytes> programs;for(const auto& target:op.reference.targets)programs.push_back(available_finite_codec_detail::program_bytes(target));
    auto required=op.required;required.insert(required.end(),source.obligations.begin(),source.obligations.end());
    const auto input_required=input_origins(child_d,source.asset,programs,context,check);required.insert(required.end(),input_required.begin(),input_required.end());
    const auto transport=transport_payload(op.archive,source.archive,d.transport,status);
    const auto child_payload=interpretation_payload(source.archive,op.reference.target_archives,child_d,status,input_required);
    std::vector<Origin> own;
    for(const auto& spec:std::array<std::tuple<std::uint64_t,Role,const Bytes*>,4>{{
        {keys.transport_maps,Role::Maps,&transport},{keys.transport_relationship,relationship_role(status),&transport},
        {keys.maps,Role::Maps,&child_payload},{keys.relationship,relationship_role(status),&child_payload}}}) {
        if(check){check();}own.push_back(context.require(std::get<0>(spec),std::get<1>(spec),*std::get<2>(spec)));
    }
    required.insert(required.end(),own.begin(),own.end());const auto payload=lineage_payload(op.archive,source.archive,d,rows,status,keys,own);
    if(check){check();}required.push_back(context.require(keys.instance,Role::Prospective,payload));receipts(r,required);
    auto child=available_finite_codec_detail::preflight_model(r.blob(),context,policy,limits,check);
    canonical(child.source.archive,source.archive);inference::field_program_detail::require(child.target_archives==op.reference.target_archives,"operator child changes actual targets");
    canonical(schema_bytes(child.declaration),schema_bytes(child_d));
    inference::field_program_detail::require(child.status==status&&child.keys.maps==keys.maps&&child.keys.relationship==keys.relationship,"operator child changes publications");
    const auto native=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing available operator instance");
    return {Bytes(bytes.begin(),bytes.end()),std::move(op),std::move(source),d,std::move(rows),status,keys,std::move(required),std::move(child),Bytes(native.begin(),native.end())};
}
inline AvailableRelationalFiniteOperatorInstance replay_instance(const PassiveInstance& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    // The complete incoming authority, including instance lineage, precedes ANY reference/source replay.
    recheck(parsed.required,context,check);const auto op=replay_operator(parsed.op,context,policy,limits,check);
    const auto source=available_finite_codec_detail::replay_source(parsed.source,context,policy,limits,check);const auto finite=source.finite();const auto asset=finite.asset();
    std::vector<RelationalAvailableFiniteOperatorInput> inputs;for(const auto& row:parsed.rows)
        inputs.push_back({row.port,{{asset,row.input.declaration,row.input.use,row.input.field},row.input.keys}});
    auto instance=op.instantiate(source,parsed.declaration,std::move(inputs),parsed.status,parsed.keys,context,limits,check);
    canonical(parsed.child.archive,model_bytes(instance.field_map()));canonical(parsed.native,encode_relational_finite_operator_instance(instance.raw()));
    canonical(parsed.archive,instance_bytes(instance));return instance;
}
struct PassiveResult {Bytes archive;PassiveInstance instance;Bytes native;};
inline PassiveResult preflight_result(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,family_magic);auto instance=preflight_instance(r.blob(),context,policy,limits,check);
    const auto native=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing available operator result");
    return {Bytes(bytes.begin(),bytes.end()),std::move(instance),Bytes(native.begin(),native.end())};
}
inline AvailableRelationalFiniteOperatorResult replay_result(const PassiveResult& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    const auto instance=replay_instance(parsed.instance,context,policy,limits,check);auto result=instance.execute(context,limits,check);
    canonical(parsed.native,encode_relational_finite_operator_result(result.result()));canonical(parsed.archive,family_bytes(result));return result;
}
inline std::uint64_t claimed_rule(const PassiveResult& family,std::size_t ordinal,const storage::TemporalReadLimits& policy) {
    Reader r(family.native,policy);header(r,family.native,finite_operator_codec_detail::family_magic);
    canonical(r.blob(),family.instance.native);const auto field=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing claimed operator result");
    // Reuse the I19 passive full-family witness reader; no native execution occurs here.
    return available_finite_codec_detail::claimed_rule({{},family.instance.child,Bytes(field.begin(),field.end())},ordinal,policy);
}
struct PassiveEvaluation {PassiveResult family;std::size_t ordinal;numeric::RationalVector free;std::optional<Origin> setting;Origin choice;Bytes native;};
inline PassiveEvaluation preflight_evaluation(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,evaluation_magic);auto family=preflight_result(r.blob(),context,policy,limits,check);
    const auto ordinal=size(r);auto free=relational_affine_codec_detail::vector(r);const auto parameter=r.u64();const auto has_setting=r.flag();
    const auto key=claimed_rule(family,ordinal,policy);const auto& rules=family.instance.child.declaration.rules;
    const auto rule=std::find_if(rules.begin(),rules.end(),[&](const auto& row){return row.key==key;});
    inference::field_program_detail::require(rule!=rules.end()&&rule->target&&*rule->target<family.instance.child.targets.size(),"claimed operator choice loses target");
    std::optional<Origin> setting;
    if(const auto* evidence=std::get_if<available_evidence_codec_detail::PassiveEvidence>(&family.instance.child.targets[*rule->target])) {
        inference::field_program_detail::require(parameter&&has_setting,"affine operator choice needs global setting receipt");if(check){check();}
        setting=context.require(parameter,Role::Prospective,relational_parameter_setting_payload(evidence->native,evidence->obligations,free));canonical(r.blob(),setting->receipt());
    }else inference::field_program_detail::require(!parameter&&!has_setting&&free.empty(),"fixed operator choice has spurious global setting");
    const auto choice_key=r.u64();const auto receipt=r.blob();if(check){check();}
    auto origin=context.require(choice_key,Role::Prospective,available_finite_operator_detail::completion_payload(family.archive,ordinal,free,setting));canonical(receipt,origin.receipt());
    const auto native=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing available operator evaluation");
    return {std::move(family),ordinal,std::move(free),std::move(setting),std::move(origin),Bytes(native.begin(),native.end())};
}
inline AvailableRelationalFiniteOperatorEvaluation replay_evaluation(const PassiveEvaluation& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    if(parsed.setting){if(check){check();}context.recheck(*parsed.setting);}if(check){check();}context.recheck(parsed.choice);
    const auto family=replay_result(parsed.family,context,policy,limits,check);
    auto evaluation=family.evaluate(parsed.ordinal,parsed.free,parsed.setting?parsed.setting->key():0,parsed.choice.key(),context,limits,check);
    canonical(parsed.native,encode_relational_finite_operator_evaluation(evaluation.result()));return evaluation;
}
inline Bytes evaluation_bytes(const AvailableRelationalFiniteOperatorEvaluation& evaluation) {
    const auto pin=evaluation;auto w=envelope(evaluation_magic);blob(w,family_bytes(pin.family()));const auto raw=pin.result().result();w.u64(pin.ordinal());
    relational_affine_codec_detail::vector(w,raw.free_values());const auto setting=pin.parameter_setting();w.u64(setting?setting->key():0);w.u8(setting?1:0);
    if(setting){blob(w,setting->receipt());}const auto origin=pin.origin();w.u64(origin.key());blob(w,origin.receipt());
    blob(w,encode_relational_finite_operator_evaluation(pin.result()));return finish(std::move(w));
}
} // namespace available_finite_operator_codec_detail
inline std::vector<std::byte> encode_available_relational_finite_operator(const PreparedAvailableRelationalFiniteFieldOperator& op) {return available_finite_operator_detail::operator_bytes(op);}
inline PreparedAvailableRelationalFiniteFieldOperator decode_available_relational_finite_operator(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_finite_operator_codec_detail;const auto authority=context;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    const auto parsed=preflight_operator(frozen,authority,read,limits,check);auto result=replay_operator(parsed,authority,read,limits,check);canonical(frozen,operator_bytes(result));return result;
}
inline std::vector<std::byte> encode_available_relational_finite_operator_instance(const AvailableRelationalFiniteOperatorInstance& instance) {return available_finite_operator_detail::instance_bytes(instance);}
inline AvailableRelationalFiniteOperatorInstance decode_available_relational_finite_operator_instance(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_finite_operator_codec_detail;const auto authority=context;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    const auto parsed=preflight_instance(frozen,authority,read,limits,check);auto result=replay_instance(parsed,authority,read,limits,check);canonical(frozen,instance_bytes(result));return result;
}
inline std::vector<std::byte> encode_available_relational_finite_operator_result(const AvailableRelationalFiniteOperatorResult& result) {return available_finite_operator_detail::family_bytes(result);}
inline AvailableRelationalFiniteOperatorResult decode_available_relational_finite_operator_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_finite_operator_codec_detail;const auto authority=context;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    const auto parsed=preflight_result(frozen,authority,read,limits,check);auto result=replay_result(parsed,authority,read,limits,check);canonical(frozen,family_bytes(result));return result;
}
inline std::vector<std::byte> encode_available_relational_finite_operator_evaluation(const AvailableRelationalFiniteOperatorEvaluation& evaluation) {return available_finite_operator_codec_detail::evaluation_bytes(evaluation);}
inline AvailableRelationalFiniteOperatorEvaluation decode_available_relational_finite_operator_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_finite_operator_codec_detail;const auto authority=context;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    const auto parsed=preflight_evaluation(frozen,authority,read,limits,check);auto result=replay_evaluation(parsed,authority,read,limits,check);canonical(frozen,evaluation_bytes(result));return result;
}
} // namespace rfc::learning
