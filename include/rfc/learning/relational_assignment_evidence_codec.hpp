#pragma once

#include "rfc/learning/relational_assignment_evidence.hpp"

namespace rfc::learning {
namespace assignment_evidence_codec_detail {
using namespace assignment_evidence_detail;
using namespace inference::field_interface_codec_detail;
inline constexpr auto magic=[] {auto m=root_magic;m[4]=std::byte{'E'};return m;}();
inline RelationalAssignmentDeclaration declaration(Reader& r) {
    RelationalAssignmentDeclaration d;d.key=r.u64();d.revision=r.u64();d.parameter_carrier=r.u64();
    const auto n=r.count(8);require(n<=r.limits().max_dimension,"assignment parameter read bound");
    for(std::size_t i=0;i<n;++i){d.parameters.push_back(r.u64());}
    return d;
}
inline CandidateRecord candidate(Reader& r) {
    CandidateRecord c;c.key=r.u64();const auto n=r.count(8);require(n<=r.limits().max_dimension,"assignment symbol read bound");
    for(std::size_t i=0;i<n;++i)c.assignment.push_back(r.u64());
    c.binding=task_outcome_codec_detail::binding(r);c.binding.origin=r.u64();
    c.shift=relational_affine_codec_detail::vector(r);c.injection=storage::temporal_codec_detail::matrix(r);
    const auto evidence=r.blob();c.evidence.assign(evidence.begin(),evidence.end());return c;
}
inline void coordinates(Writer& w,const RelationalAssignmentCoordinates& c) {
    relational_affine_codec_detail::vector(w,c.shift);storage::temporal_codec_detail::matrix(w,c.map);
}
inline storage::PreparedBytePacket witness(const PreparedRelationalAssignmentEvidence& model,const Context& authority,
    relational_affine_codec_detail::PacketContext& packets) {
    storage::BytePacketBuilder w(magic,packets.max_bytes,packets.byte_guard.get());w.u64(model.revision());w.u64(model.candidates().size());
    for(std::size_t i=0;i<model.candidates().size();++i){
        const auto& branch=model.branch(i,authority);w.u8(branch.rejected);w.write([&](auto& raw){coordinates(raw,branch.coordinates);});w.u64(branch.solves.size());
        for(const auto& retained:branch.solves){const auto& solve=*retained;
            w.u64(solve.observation);w.blob(available_evidence_codec_detail::prepare_family(solve.response,packets));
            w.blob(task_outcome_codec_detail::proof(solve.certificate));w.write([&](auto& raw){coordinates(raw,solve.child);});
        }
    }
    w.u64(model.remainder().size());for(const auto& row:model.remainder())w.write([&](auto& raw){inference::relational_finite_source_codec_detail::finite_keys(raw,row);});
    return std::move(w).finish(packets.check);
}
inline storage::PreparedBytePacket prepare(const PreparedRelationalAssignmentEvidence& model,const Context& authority,
    relational_affine_codec_detail::PacketContext& packets) {
    const auto pin=model;pin.admit(authority,packets.check);
    std::vector<PreparedRelationalAssignmentEvidence> lineage;auto current=pin;
    while(true){lineage.push_back(current);const auto parent=current.parent();if(!parent)break;current=*parent;}
    std::reverse(lineage.begin(),lineage.end());const auto& root=lineage.front();const auto limits=root.limits();
    storage::BytePacketBuilder w(magic,packets.max_bytes,packets.byte_guard.get());
    w.u64(limits.dimension);w.u64(limits.entries);w.u64(limits.steps);w.u64(limits.integer_bits);
    w.write([&](auto& raw){assignment_evidence_detail::declaration(raw,root.declaration());});
    w.blob(inference::encode_relational_finite_source(root.finite_source()));w.u64(root.candidates().size());
    for(const auto& c:root.candidates()){
        w.write([&](auto& raw){raw.u64(c.key);inference::relational_finite_source_codec_detail::finite_keys(raw,c.assignment);
            binding(raw,c.binding);raw.u64(c.binding.origin);relational_affine_codec_detail::vector(raw,c.shift);
            storage::temporal_codec_detail::matrix(raw,c.injection);});
        w.blob(available_evidence_codec_detail::prepare_evidence(c.evidence,packets));
        w.receipt(admit_binding(c.binding,c.evidence.program().program(),authority));
    }
    const auto& d=root.declaration();const auto root_origin=root.root_origin();authority.recheck(root_origin);
    w.u64(d.origin);w.receipt(root_origin);w.u64(lineage.size()-1);
    for(std::size_t step=1;step<lineage.size();++step){
        const auto first=lineage[step-1].observations().size();const auto& observations=lineage[step].observations();w.u64(observations.size()-first);
        for(auto i=first;i<observations.size();++i){const auto& o=observations[i];
            w.u64(o.key);w.u64(o.origin);w.write([&](auto& raw){binding(raw,o.binding);raw.u64(o.binding.origin);});
            w.blob(inference::value_admission_codec_detail::prepare_result(o.measured,packets));w.u64(o.inputs.size());
            for(const auto& input:o.inputs){w.u64(input.candidate);w.blob(inference::value_admission_codec_detail::prepare_result(input.input,packets));}
            w.receipt(authority.require(o.origin,Role::TargetValue,relational_assignment_measurement_payload(o)));
            w.receipt(admit_binding(o.binding,o.measured.result().result().program(),authority));
        }
    }
    w.blob(witness(pin,authority,packets));return std::move(w).finish(packets.check);
}
struct PassiveObservation {
    std::uint64_t key{},origin{};
    RelationalWorkerBindingDeclaration binding;
    inference::value_admission_codec_detail::PassiveAvailableResult measured;
    std::vector<inference::value_admission_codec_detail::PassiveAvailableResult> inputs;
    std::vector<std::uint64_t> candidates;
};
struct PassiveModel {
    numeric::ExactActionLimits limits;
    RelationalAssignmentDeclaration declaration;
    std::vector<std::byte> finite;
    std::vector<CandidateRecord> candidates;
    std::vector<available_evidence_codec_detail::PassiveEvidence> evidence;
    std::vector<std::vector<PassiveObservation>> suffixes;
};
inline PassiveModel preflight(std::span<const std::byte> bytes,const Context& authority,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,magic);PassiveModel p;
    p.limits={size(r),size(r),size(r),size(r)};const auto l=p.limits;
    require(l.dimension<=cap.dimension&&l.entries<=cap.entries&&l.steps<=cap.steps&&l.integer_bits<=cap.integer_bits,"assignment action policy exceeds caller cap");
    p.declaration=declaration(r);const auto finite=r.blob();p.finite.assign(finite.begin(),finite.end());
    const auto n=r.count(80);require(n<=policy.max_dimension,"assignment candidate read bound");
    for(std::size_t i=0;i<n;++i){
        auto c=candidate(r);auto e=available_evidence_codec_detail::preflight_evidence(c.evidence,authority,policy,cap,check);
        const auto receipt=r.blob();const auto binding_origin=authority.require(c.binding.origin,Role::Maps,binding_payload(c.binding,e.program.native));
        canonical(receipt,binding_origin.receipt());p.candidates.push_back(std::move(c));p.evidence.push_back(std::move(e));
    }
    p.declaration.origin=r.u64();const auto receipt=r.blob();
    const auto origin=authority.require(p.declaration.origin,Role::Evidence,root_payload(p.declaration,p.finite,p.candidates,p.limits));
    canonical(receipt,origin.receipt());const auto versions=r.count(8);require(versions<=policy.max_dimension,"assignment lineage read bound");
    for(std::size_t v=0;v<versions;++v){
        const auto count=r.count(60);require(count&&count<=policy.max_dimension,"assignment observation suffix read bound");
        std::vector<PassiveObservation> suffix;
        for(std::size_t j=0;j<count;++j){
            PassiveObservation o;o.key=r.u64();o.origin=r.u64();o.binding=task_outcome_codec_detail::binding(r);o.binding.origin=r.u64();
            const auto measured=r.blob();o.measured=inference::value_admission_codec_detail::preflight_result(measured,authority,policy,check);
            const auto inputs=r.count(16);require(inputs==n,"assignment input catalog read bound");
            std::vector<std::pair<std::uint64_t,Bytes>> input_bytes;
            for(std::size_t k=0;k<inputs;++k){const auto key=r.u64();const auto child=r.blob();
                o.candidates.push_back(key);o.inputs.push_back(inference::value_admission_codec_detail::preflight_result(child,authority,policy,check));
                input_bytes.emplace_back(key,Bytes(child.begin(),child.end()));}
            const auto measurement_receipt=r.blob(),binding_receipt=r.blob();
            const auto observed=authority.require(o.origin,Role::TargetValue,measurement_payload(o.key,o.binding,measured,input_bytes));
            const auto provider=authority.require(o.binding.origin,Role::Maps,binding_payload(o.binding,o.measured.program));
            canonical(measurement_receipt,observed.receipt());canonical(binding_receipt,provider.receipt());suffix.push_back(std::move(o));
        }
        p.suffixes.push_back(std::move(suffix));
    }
    (void)r.blob();require(!r.remaining(),"trailing assignment evidence bytes");return p;
}
inline PreparedRelationalAssignmentEvidence replay(const PassiveModel& p,const Context& authority,const storage::TemporalReadLimits& policy,
    const std::function<void()>& check) {
    // The whole passive history has current authority before any original
    // source, finite section, parameter family or constraint is reconstructed.
    auto finite=inference::decode_relational_finite_source(p.finite,policy,p.limits);
    std::vector<RelationalAssignmentCandidate> candidates;
    for(std::size_t i=0;i<p.candidates.size();++i){const auto& c=p.candidates[i];
        candidates.push_back({c.key,c.assignment,c.binding,available_evidence_codec_detail::replay_evidence(p.evidence[i],authority,policy,check),c.shift,c.injection});}
    PreparedRelationalAssignmentEvidence model(p.declaration,std::move(finite),std::move(candidates),authority,p.limits,check);
    for(const auto& suffix:p.suffixes){std::vector<RelationalAssignmentObservation> observations;
        for(const auto& o:suffix){
            auto measured=inference::value_admission_codec_detail::replay_result(o.measured,authority,policy,p.limits,check);
            std::vector<RelationalAssignmentInput> inputs;
            for(std::size_t i=0;i<o.inputs.size();++i){const auto program=model.candidates().at(i).evidence.program().program();
                inputs.push_back({o.candidates[i],inference::value_admission_codec_detail::replay_result(o.inputs[i],authority,policy,p.limits,check,program)});}
            observations.push_back({o.key,o.origin,o.binding,std::move(measured),std::move(inputs)});
        }
        model=model.append(std::move(observations),authority,check);
    }
    return model;
}
} // namespace assignment_evidence_codec_detail

template<class Consume> inline void stream_relational_assignment_evidence(const PreparedRelationalAssignmentEvidence& model,
    const inference::RelationalAvailabilityContext& context,Consume&& consume,std::size_t max_bytes=256u<<20,std::function<void()> check={}) {
    const auto authority=context;relational_affine_codec_detail::PacketContext packets(max_bytes,std::move(check));
    assignment_evidence_codec_detail::prepare(model,authority,packets).stream(std::forward<Consume>(consume),packets.check);
}
inline std::vector<std::byte> encode_relational_assignment_evidence(const PreparedRelationalAssignmentEvidence& model,
    const inference::RelationalAvailabilityContext& context,std::size_t max_bytes=256u<<20,std::function<void()> check={}) {
    const auto authority=context;relational_affine_codec_detail::PacketContext packets(max_bytes,std::move(check));
    return assignment_evidence_codec_detail::prepare(model,authority,packets).bytes();
}
inline PreparedRelationalAssignmentEvidence decode_relational_assignment_evidence(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap={},std::function<void()> check={}) {
    const auto authority=context;const auto read_policy=policy;const auto callback=std::move(check);
    inference::value_admission_detail::bound(bytes.size(),read_policy.max_file_bytes);const std::vector<std::byte> frozen(bytes.begin(),bytes.end());
    const auto parsed=assignment_evidence_codec_detail::preflight(frozen,authority,read_policy,cap,callback);
    auto model=assignment_evidence_codec_detail::replay(parsed,authority,read_policy,callback);
    inference::field_program_codec_detail::canonical(frozen,encode_relational_assignment_evidence(model,authority,read_policy.max_file_bytes,callback));return model;
}
namespace assignment_evidence_codec_detail {
inline constexpr std::array<std::byte,8> response_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'A'},std::byte{'S'},std::byte{'R'},std::byte{1},std::byte{0}};
inline storage::PreparedBytePacket prepare_response(const RelationalAssignmentResponse& response,const Context& authority,
    relational_affine_codec_detail::PacketContext& packets) {
    const auto pin=response;pin.admit(authority,packets.check);const auto model=pin.model();
    storage::BytePacketBuilder w(response_magic,packets.max_bytes,packets.byte_guard.get());
    w.blob(prepare(model,authority,packets));w.u64(pin.candidate());const auto original=pin.original();
    w.blob(available_evidence_codec_detail::prepare_family(original,packets));
    const auto c=pin.coordinates();w.write([&](auto& raw){coordinates(raw,c);});
    const auto native=original.result();const auto program=native.evidence().program();Guard guard(model.limits(),packets.check);
    w.u64(program.declaration().nodes.size());
    for(const auto& node:program.declaration().nodes){
        w.u64(node.key);const auto field=restrict_field(native.field(node.key),c,guard);
        w.write([&](auto& raw){relational_affine_codec_detail::field(raw,field);});
        if(node.kind==inference::FieldProgramNodeKind::Boundary){const auto flags=restrict_field(native.occurrence_field(node.key),c,guard);
            w.write([&](auto& raw){relational_affine_codec_detail::field(raw,flags);});}
    }
    return std::move(w).finish(packets.check);
}
} // namespace assignment_evidence_codec_detail

template<class Consume> inline void stream_relational_assignment_response(const RelationalAssignmentResponse& response,
    const inference::RelationalAvailabilityContext& context,Consume&& consume,std::size_t max_bytes=256u<<20,std::function<void()> check={}) {
    const auto authority=context;relational_affine_codec_detail::PacketContext packets(max_bytes,std::move(check));
    assignment_evidence_codec_detail::prepare_response(response,authority,packets).stream(std::forward<Consume>(consume),packets.check);
}
inline std::vector<std::byte> encode_relational_assignment_response(const RelationalAssignmentResponse& response,
    const inference::RelationalAvailabilityContext& context,std::size_t max_bytes=256u<<20,std::function<void()> check={}) {
    const auto authority=context;relational_affine_codec_detail::PacketContext packets(max_bytes,std::move(check));
    return assignment_evidence_codec_detail::prepare_response(response,authority,packets).bytes();
}
inline RelationalAssignmentResponse decode_relational_assignment_response(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap={},std::function<void()> check={}) {
    using namespace assignment_evidence_codec_detail;
    const auto authority=context;const auto read_policy=policy;const auto callback=std::move(check);
    inference::value_admission_detail::bound(bytes.size(),read_policy.max_file_bytes);const Bytes frozen(bytes.begin(),bytes.end());
    Reader r(frozen,read_policy);header(r,frozen,response_magic);const auto model_bytes=r.blob();const auto selected=size(r);const auto child=r.blob();
    const auto model_passive=preflight(model_bytes,authority,read_policy,cap,callback);
    const auto family=available_evidence_codec_detail::preflight_family(child,authority,read_policy,cap,callback);
    require(selected<model_passive.candidates.size(),"assignment response names an absent candidate");
    Reader root(child,read_policy);header(root,child,available_evidence_codec_detail::family_magic);
    canonical(model_passive.candidates[selected].evidence,root.blob());
    // All model and held-input origins have been admitted before replay.
    auto model=replay(model_passive,authority,read_policy,callback);const auto program=model.candidates()[selected].evidence.program().program();
    auto input=inference::value_admission_codec_detail::replay_result(family.input,authority,read_policy,model.limits(),callback,program);
    auto response=model.execute(selected,std::move(input),authority,callback);
    canonical(frozen,encode_relational_assignment_response(response,authority,read_policy.max_file_bytes,callback));return response;
}
// Explicit archive adoption into the retained candidate owner. Live foreign
// field owners still require their declared accession; equal values do not bind.
inline inference::AvailableRelationalFieldResult restore_relational_assignment_input(
    const PreparedRelationalAssignmentEvidence& model,std::size_t candidate,std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& policy,
    std::function<void()> check={}) {
    const auto pin=model;const auto authority=context;const auto read_policy=policy;const auto callback=std::move(check);
    inference::value_admission_detail::bound(bytes.size(),read_policy.max_file_bytes);const std::vector<std::byte> frozen(bytes.begin(),bytes.end());
    pin.admit(authority,callback);const auto parsed=inference::value_admission_codec_detail::preflight_result(frozen,authority,read_policy,callback);
    const auto program=pin.candidates().at(candidate).evidence.program().program();
    auto result=inference::value_admission_codec_detail::replay_result(parsed,authority,read_policy,pin.limits(),callback,program);
    inference::field_program_codec_detail::canonical(frozen,inference::encode_available_relational_result(result));return result;
}
// Reattach a saved prospective goal only after comparing the complete original
// family and its native publications. This does not turn a prediction into an
// execution receipt or create a new origin.
inline OwnedRelationalTaskOutcome restore_relational_assignment_goal(const RelationalAssignmentResponse& response,
    std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,std::function<void()> check={}) {
    const auto pin=response;const auto authority=context;const auto read_policy=policy;const auto callback=std::move(check);
    pin.admit(authority,callback);auto goal=decode_relational_task_outcome(bytes,authority,read_policy,pin.model().limits(),callback);
    const auto value=goal.response();const auto* original=std::get_if<AvailableRelationalAffineResult>(&value);
    inference::field_program_detail::require(original,"assignment goal must retain its prospective original response family");
    inference::field_program_codec_detail::canonical(encode_available_relational_affine_result(*original),encode_available_relational_affine_result(pin.original()));
    return {goal.task(),goal.contract_asset(),goal.binding(),RelationalTaskResponse(pin.original()),authority,pin.model().limits(),callback};
}
} // namespace rfc::learning
