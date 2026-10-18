#pragma once

#include "rfc/learning/relational_available_literal_field_family.hpp"

namespace rfc::learning {
namespace available_joint_codec_detail {
using namespace available_joint_detail;
using namespace inference::field_program_codec_detail;
using Reader = inference::field_program_codec_detail::Reader;
inline storage::ByteRange freeze(std::span<const std::byte> bytes,const storage::TemporalReadLimits& policy) {
    inference::value_admission_detail::bound(bytes.size(),policy.max_file_bytes);require(bytes.size()>=32,"joint available archive header");
    auto owner=std::make_shared<const Bytes>(bytes.begin(),bytes.end());const auto n=owner->size();
    return storage::ByteRange::indexed(std::move(owner),{{0,n}},{0},n,policy.max_file_bytes,policy.max_file_bytes,policy.max_records);
}
inline Packet view(storage::ByteRange bytes) {return Packet::pin(std::move(bytes),[](const auto& held){return held.span();});}
inline void prefix(storage::ByteRange bytes,const Origin& origin,const std::function<void()>& check) {origin_prefix(origin).require_equal(bytes.span(),check);}
inline inference::RelationalInputOrigins input_keys(Reader& r) {return {r.u64(),r.u64(),r.u64(),r.u64()};}
inline RelationalJointReaderOrigins reader_keys(Reader& r) {
    const auto input=input_keys(r);const inference::RelationalProgramOrigins program{r.u64(),r.u64(),r.u64()};const auto original=input_keys(r);
    return {input,program,original,r.u64()};
}
struct InputImage {Bytes program,asset;inference::RelationalInputAccessionDeclaration declaration;std::uint64_t use{};numeric::SparseRationalTensor field;};
inline InputImage input_image(Reader& r) {
    const auto p=r.blob(),a=r.blob();auto d=inference::field_interface_codec_detail::accession(r);const auto use=r.u64();auto field=storage::temporal_codec_detail::tensor(r);
    return {Bytes(p.begin(),p.end()),Bytes(a.begin(),a.end()),std::move(d),use,std::move(field)};
}
inline Bytes accession_payload(const InputImage& i) {return inference::relational_input_accession_payload(i.program,i.asset,i.declaration,i.use,i.field);}
inline void origins(std::vector<Origin>& out,const InputImage& i,inference::RelationalInputOrigins k,const Context& context) {
    inference::value_admission_detail::input_origins(out,k,i.asset,inference::relational_input_value_payload(i.asset,i.declaration,i.use,i.field),accession_payload(i),context);
}
inline void literal_metadata(const inference::literal_input_codec_detail::PassiveInput& parsed,const InputImage& i) {
    require(parsed.program==i.program&&parsed.asset==i.asset&&inference::relational_input_accession_payload(parsed.program,parsed.asset,parsed.accession,parsed.declaration.use,i.field)==accession_payload(i),"joint literal metadata changes declared source input");
}
struct PassiveInput {std::uint8_t kind{};storage::ByteRange recipe;InputImage target;RelationalJointReaderOrigins keys;std::optional<InputImage> original;std::vector<Origin> required;};
inline PassiveInput preflight_input(storage::ByteRange bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,magic('I'));const auto kind=r.u8();require(kind<2,"unknown available joint reader kind");const auto recipe_image=r.view_blob();
    auto target=input_image(r);const auto k=reader_keys(r);std::optional<InputImage> original;std::vector<Origin> required;origins(required,target,k.input,context);
    if(kind==1) {
        original=input_image(r);for(const auto& [key,role]:std::vector<std::pair<std::uint64_t,Role>>{{k.original_program.program,Role::Program},{k.original_program.maps,Role::Maps},{k.original_program.metrics,Role::Metrics}})required.push_back(context.require(key,role,original->program));
        origins(required,*original,k.original_input,context);
        Reader graph_input(recipe_image,policy);header(graph_input,recipe_image,inference::known_tensor_graph_codec_detail::input_magic);const auto graph_bytes=graph_input.view_blob();const auto program=graph_input.blob();auto a=inference::field_interface_codec_detail::accession(graph_input);
        const auto source=storage::temporal_codec_detail::tensor(graph_input);(void)storage::temporal_codec_detail::tensor(graph_input);
        require(!graph_input.remaining()&&Bytes(program.begin(),program.end())==target.program&&inference::field_interface_detail::equal(source,target.field),"joint graph target metadata differs");
        require(inference::relational_input_accession_payload(target.program,target.asset,a,target.use,target.field)==accession_payload(target),"joint graph target accession differs");
        Reader graph(graph_bytes,policy);header(graph,graph_bytes,inference::known_tensor_graph_codec_detail::graph_magic);
        const numeric::ExactActionLimits recorded{inference::field_interface_codec_detail::size(graph),inference::field_interface_codec_detail::size(graph),inference::field_interface_codec_detail::size(graph),inference::field_interface_codec_detail::size(graph)};
        inference::known_tensor_graph_detail::policy(recorded,limits);require(graph.flag(),"joint graph reader omitted actual literal parent");
        literal_metadata(inference::literal_input_codec_detail::passive(graph.blob(),policy,limits,check),*original);
        (void)inference::known_tensor_graph_codec_detail::declaration(graph);(void)graph.view_blob();require(!graph.remaining(),"trailing joint graph recipe");
    } else {
        require(empty(k.original_program)&&empty(k.original_input),"direct literal has extraneous graph-parent origins");
        literal_metadata(inference::literal_input_codec_detail::passive(recipe_image.span(),policy,limits,check),target);
    }
    required.push_back(context.require_bytes(k.reader,Role::Interface,recipe_image));const auto n=r.count(8);require(n==required.size(),"joint reader complete origin count");
    for(const auto& origin:required){canonical(r.view_blob(),origin.receipt());}require(!r.remaining(),"trailing available joint reader");
    return {kind,recipe_image,std::move(target),k,std::move(original),std::move(required)};
}
inline DeclarationImage declaration(storage::ByteRange bytes,const storage::TemporalReadLimits& policy) {
    Reader r(bytes,policy);header(r,bytes,magic('D'));DeclarationImage d;d.key=r.u64();d.revision=r.u64();const auto n=r.count(80);
    for(std::size_t j=0;j<n;++j) {
        InterfaceImage i;i.key=r.u64();i.revision=r.u64();i.left=inference::field_interface_codec_detail::size(r);i.right=inference::field_interface_codec_detail::size(r);i.source_namespace=r.u64();
        const auto source=r.blob(),endpoint=r.blob();i.source.assign(source.begin(),source.end());i.endpoint.assign(endpoint.begin(),endpoint.end());
        i.common_left=relational_affine_codec_detail::source_map(r);i.common_right=relational_affine_codec_detail::source_map(r);i.left_views=relational_family_codec_detail::views(r);i.right_views=relational_family_codec_detail::views(r);d.interfaces.push_back(std::move(i));
    }require(!r.remaining(),"trailing joint available declaration");return d;
}
inline RelationalFieldFamilyDeclaration realize(const DeclarationImage& d,const storage::TemporalReadLimits& policy) {
    RelationalFieldFamilyDeclaration out{d.key,d.revision,{}};storage::NativeSeriesLimits l;l.temporal=policy;l.max_archive_bytes=policy.max_file_bytes;
    for(const auto& i:d.interfaces) {
        const storage::MappedRfcx source(i.source);const auto endpoint=storage::decode_temporal_endpoint(source.materialize(),i.endpoint,policy);
        out.interfaces.push_back({i.key,i.revision,i.left,i.right,storage::NativeTemporalSeries(endpoint,i.source_namespace,l).tail(),i.common_left,i.common_right,i.left_views,i.right_views});
    }return out;
}
struct PassiveObservation {std::uint64_t key{};std::vector<PassiveInput> inputs;};
struct PassiveModel {
    storage::ByteRange archive,content;numeric::ExactActionLimits limits;DeclarationImage declaration;
    std::vector<available_evidence_codec_detail::PassiveEvidence> members;storage::ByteRange proof;
    std::vector<std::vector<PassiveObservation>> readers;RelationalJointFamilyOrigins keys;Origin origin;std::vector<Origin> required;
};
inline PassiveModel preflight_model(storage::ByteRange bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap,const std::function<void()>& check) {
    Reader outer(bytes,policy);header(outer,bytes,magic('M'));const auto body=outer.view_blob();const auto key=outer.u64();
    const auto origin=context.require_bytes(key,Role::Interface,body);prefix(outer.view_blob(),origin,check);require(!outer.remaining(),"trailing joint available model");
    Reader r(body,policy);header(r,body,magic('B'));const numeric::ExactActionLimits limits{inference::field_interface_codec_detail::size(r),inference::field_interface_codec_detail::size(r),inference::field_interface_codec_detail::size(r),inference::field_interface_codec_detail::size(r)};
    require(limits.dimension<=cap.dimension&&limits.entries<=cap.entries&&limits.steps<=cap.steps&&limits.integer_bits<=cap.integer_bits,"joint available archived policy exceeds caller cap");
    auto d=declaration(r.view_blob(),policy);const auto n=r.count(8);require(n&&n<=limits.dimension,"joint available member dimension admission");
    std::vector<available_evidence_codec_detail::PassiveEvidence> members;std::vector<Origin> required;
    for(std::size_t m=0;m<n;++m) {if(check){check();}auto member=available_evidence_codec_detail::preflight_evidence_image(r.view_blob(),context,policy,limits,check);merge(required,member.obligations);members.push_back(std::move(member));}
    const auto proof=r.view_blob();require(r.count(8)==n,"joint available training rows omit members");std::vector<std::vector<PassiveObservation>> readers;
    for(std::size_t m=0;m<n;++m) {
        std::vector<const available_evidence_codec_detail::PassiveObservation*> observations;for(const auto& update:members[m].updates)for(const auto& observation:update)observations.push_back(&observation);
        const auto count=r.count(16);require(count==observations.size(),"joint available reader coverage omits original observation");std::vector<PassiveObservation> rows;
        for(std::size_t j=0;j<count;++j) {
            const auto observation=r.u64();const auto ni=r.count(8);require(observation==observations[j]->key&&ni==observations[j]->input.inputs.size(),"joint available reader observation order/input count");PassiveObservation row{observation,{}};
            for(std::size_t k=0;k<ni;++k) {
                auto input=preflight_input(r.view_blob(),context,policy,limits,check);const auto& expected=observations[j]->input.inputs[k];
                require(input.target.program==members[m].program.native&&accession_payload(input.target)==inference::relational_input_accession_payload(members[m].program.native,expected.asset,expected.declaration,expected.use,expected.field),"joint cold reader changes original observed input");
                for(std::size_t p=0;p<4;++p)require(std::any_of(observations[j]->input.obligations.begin(),observations[j]->input.obligations.end(),[&](const auto& old){return old.same_owner(input.required[p]);}),"joint cold reader replaces original observation receipt");
                merge(required,input.required);row.inputs.push_back(std::move(input));
            }rows.push_back(std::move(row));
        }readers.push_back(std::move(rows));
    }
    const auto interfaces=r.count(16);require(interfaces==d.interfaces.size(),"joint cold common-interface origin count");RelationalJointFamilyOrigins keys{{},key};
    for(std::size_t j=0;j<interfaces;++j) {
        const auto ikey=r.u64();const auto& i=d.interfaces[j];require(i.left<n&&i.right<n,"joint cold interface member index");
        const auto value=context.require_packet(ikey,Role::Interface,interface_payload(i,members[i.left].program.native,members[i.right].program.native,policy.max_file_bytes,check),check);
        canonical(r.view_blob(),value.receipt());keys.interfaces.push_back(ikey);merge(required,{value});
    }require(!r.remaining(),"trailing joint available content");merge(required,{origin});recheck(required,context,check);
    return {bytes,body,limits,std::move(d),std::move(members),proof,std::move(readers),std::move(keys),origin,std::move(required)};
}
inline RelationalAvailableJointInput replay_input(const PassiveInput& row,const inference::OwnedRelationalFieldProgram& program,
    const Context& context,const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,const std::function<void()>& check) {
    recheck(row.required,context,check);RelationalLiteralFamilyInput input=row.kind==0?
        RelationalLiteralFamilyInput(literal_family_codec_detail::adopt_literal(row.recipe.span(),program,policy,limits,check)):
        RelationalLiteralFamilyInput(inference::decode_relational_known_tensor_graph_input(row.recipe.span(),policy,limits,check,program));
    require(inference::relational_input_accession_payload(raw(input))==accession_payload(row.target),"joint replay reader differs from original admitted input");
    RelationalAvailableJointInput out{std::move(input),row.keys};const auto actual=input_origins(out,context,policy.max_file_bytes,check);
    require(actual.size()==row.required.size(),"joint replay origin shape");for(std::size_t i=0;i<actual.size();++i)require(actual[i].same_owner(row.required[i]),"joint replay changes actual origin owner");return out;
}
inline PreparedAvailableRelationalLiteralFieldFamily replay_model(const PassiveModel& parsed,const Context& context,
    const storage::TemporalReadLimits& policy,const std::function<void()>& check) {
    // Every retained training recipe, source, interface and model publication
    // has been admitted before any evidence fit or joint gluing is replayed.
    recheck(parsed.required,context,check);std::vector<PreparedAvailableRelationalEvidence> members;std::vector<PreparedRelationalFieldEvidence> native;
    for(const auto& member:parsed.members){auto value=available_evidence_codec_detail::replay_evidence(member,context,policy,check);native.push_back(value.evidence());members.push_back(std::move(value));}
    PreparedRelationalFieldFamily family(realize(parsed.declaration,policy),std::move(native),parsed.limits,check);Reader proof(parsed.proof,policy);header(proof,parsed.proof,magic('P'));relational_family_codec_detail::proof(proof,family.gluing());require(!proof.remaining(),"trailing joint cold proof");
    std::vector<std::vector<RelationalJointObservationReaders>> readers;
    for(std::size_t m=0;m<members.size();++m) {
        std::vector<RelationalJointObservationReaders> rows;const auto program=members[m].program().program();
        for(const auto& observation:parsed.readers[m]) {RelationalJointObservationReaders row{observation.key,{}};for(const auto& input:observation.inputs)row.inputs.push_back(replay_input(input,program,context,policy,parsed.limits,check));rows.push_back(std::move(row));}readers.push_back(std::move(rows));
    }
    PreparedAvailableRelationalLiteralFieldFamily out(std::move(family),std::move(members),std::move(readers),parsed.keys,context,policy.max_file_bytes,check);
    out.content().require_equal(parsed.content.span(),check);return out;
}
struct PassiveResult {storage::ByteRange archive;PassiveModel model;std::size_t member{};std::vector<PassiveInput> inputs;storage::ByteRange fields;std::vector<Origin> required;};
inline PassiveResult preflight_result(storage::ByteRange bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits cap,const std::function<void()>& check) {
    Reader r(bytes,policy);header(r,bytes,magic('R'));auto model=preflight_model(r.view_blob(),context,policy,cap,check);const auto member=inference::field_interface_codec_detail::size(r);
    require(member<model.members.size(),"joint cold prediction member");const auto n=r.count(8);require(n<=model.limits.dimension,"joint cold prediction input admission");auto required=model.required;std::vector<PassiveInput> inputs;
    for(std::size_t i=0;i<n;++i) {auto input=preflight_input(r.view_blob(),context,policy,model.limits,check);require(input.target.program==model.members[member].program.native,"joint cold prediction has foreign member program");merge(required,input.required);inputs.push_back(std::move(input));}
    const auto fields=r.view_blob();require(!r.remaining(),"trailing joint available result");recheck(required,context,check);return {bytes,std::move(model),member,std::move(inputs),fields,std::move(required)};
}
inline AvailableRelationalLiteralJointResult replay_result(const PassiveResult& parsed,const Context& context,
    const storage::TemporalReadLimits& policy,const std::function<void()>& check) {
    recheck(parsed.required,context,check);const auto model=replay_model(parsed.model,context,policy,check);const auto family=model.family();const auto program=family.members().at(parsed.member).program();std::vector<RelationalAvailableJointInput> inputs;
    for(const auto& input:parsed.inputs)inputs.push_back(replay_input(input,program,context,policy,family.limits(),check));
    auto out=model.execute(parsed.member,std::move(inputs),context,policy.max_file_bytes,family.limits(),check);fields_packet(out.raw_result(),policy.max_file_bytes,check).require_equal(parsed.fields.span(),check);return out;
}
inline Packet evaluation_packet(const AvailableRelationalLiteralJointEvaluation& value,std::size_t maximum,const std::function<void()>& check) {
    const auto pin=value;Builder w(magic('E'),maximum);w.blob(result_packet(pin.parent(),maximum,check));const auto raw_value=pin.raw_evaluation();
    w.write([&](auto& writer){relational_affine_codec_detail::vector(writer,raw_value.free_values());});const auto setting=pin.setting(),completion=pin.completion();
    w.u64(setting.key());w.blob(origin_prefix(setting));w.u64(completion.key());w.blob(origin_prefix(completion));PacketContext packets(maximum,check);
    w.blob(inference::field_interface_codec_detail::prepare_accession(raw_value.result(),packets));return std::move(w).finish(check);
}
} // namespace available_joint_codec_detail
inline storage::PreparedBytePacket prepare_available_relational_literal_field_family(const PreparedAvailableRelationalLiteralFieldFamily& value,
    std::size_t maximum,std::function<void()> check={}) {return available_joint_detail::model_packet(value,maximum,check);}
inline std::vector<std::byte> encode_available_relational_literal_field_family(const PreparedAvailableRelationalLiteralFieldFamily& value,
    std::size_t maximum,std::function<void()> check={}) {return prepare_available_relational_literal_field_family(value,maximum,check).bytes(check);}
inline PreparedAvailableRelationalLiteralFieldFamily decode_available_relational_literal_field_family(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_joint_codec_detail;const auto policy=caller_policy;const auto authority=context;const auto image=freeze(bytes,policy);
    const auto parsed=preflight_model(image,authority,policy,limits,check);auto out=replay_model(parsed,authority,policy,check);
    model_packet(out,policy.max_file_bytes,check).require_equal(image.span(),check);return out;
}
inline storage::PreparedBytePacket prepare_available_relational_literal_joint_result(const AvailableRelationalLiteralJointResult& value,
    std::size_t maximum,std::function<void()> check={}) {return available_joint_detail::result_packet(value,maximum,check);}
inline std::vector<std::byte> encode_available_relational_literal_joint_result(const AvailableRelationalLiteralJointResult& value,
    std::size_t maximum,std::function<void()> check={}) {return prepare_available_relational_literal_joint_result(value,maximum,check).bytes(check);}
inline AvailableRelationalLiteralJointResult decode_available_relational_literal_joint_result(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_joint_codec_detail;const auto policy=caller_policy;const auto authority=context;const auto image=freeze(bytes,policy);
    const auto parsed=preflight_result(image,authority,policy,limits,check);auto out=replay_result(parsed,authority,policy,check);
    result_packet(out,policy.max_file_bytes,check).require_equal(image.span(),check);return out;
}
inline storage::PreparedBytePacket prepare_available_relational_literal_joint_evaluation(const AvailableRelationalLiteralJointEvaluation& value,
    std::size_t maximum,std::function<void()> check={}) {return available_joint_codec_detail::evaluation_packet(value,maximum,check);}
inline std::vector<std::byte> encode_available_relational_literal_joint_evaluation(const AvailableRelationalLiteralJointEvaluation& value,
    std::size_t maximum,std::function<void()> check={}) {return prepare_available_relational_literal_joint_evaluation(value,maximum,check).bytes(check);}
inline AvailableRelationalLiteralJointEvaluation decode_available_relational_literal_joint_evaluation(std::span<const std::byte> bytes,
    const inference::RelationalAvailabilityContext& context,const storage::TemporalReadLimits& caller_policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace available_joint_codec_detail;const auto policy=caller_policy;const auto authority=context;const auto image=freeze(bytes,policy);Reader r(image,policy);header(r,image,magic('E'));
    const auto parsed=preflight_result(r.view_blob(),authority,policy,limits,check);auto free=relational_affine_codec_detail::vector(r);const auto setting_key=r.u64();
    require(free.size()<=parsed.model.limits.dimension,"joint cold complete free setting dimension");numeric::ExactActionGuard guard(limits,check);free=guard.canonical_input(free);
    Builder selected(magic('S'),policy.max_file_bytes);selected.blob(view(parsed.model.archive));selected.write([&](auto& writer){relational_affine_codec_detail::vector(writer,free);});
    const auto setting=authority.require_packet(setting_key,Role::Prospective,std::move(selected).finish(check),check);prefix(r.view_blob(),setting,check);
    const auto completion_key=r.u64();Builder decision(magic('C'),policy.max_file_bytes);decision.blob(view(parsed.archive));decision.write([&](auto& writer){relational_affine_codec_detail::vector(writer,free);});decision.u64(setting.key());decision.blob(origin_prefix(setting));
    const auto completion=authority.require_packet(completion_key,Role::Prospective,std::move(decision).finish(check),check);prefix(r.view_blob(),completion,check);const auto native=r.view_blob();require(!r.remaining(),"trailing joint available evaluation");
    // No fitting, gluing, predicate or field action precedes admission of the
    // whole family, supplied readers, common setting and reader completion.
    recheck(parsed.required,authority,check);recheck({setting,completion},authority,check);const auto result=replay_result(parsed,authority,policy,check);
    auto out=result.evaluate(std::move(free),setting_key,completion_key,authority,policy.max_file_bytes,limits,check);PacketContext packets(policy.max_file_bytes,check);
    inference::field_interface_codec_detail::prepare_accession(out.raw_evaluation().result(),packets).require_equal(native.span(),check);evaluation_packet(out,policy.max_file_bytes,check).require_equal(image.span(),check);return out;
}
} // namespace rfc::learning
