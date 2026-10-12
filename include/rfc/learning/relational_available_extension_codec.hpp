#pragma once

#include "rfc/learning/relational_available_extension.hpp"

namespace rfc::learning {
namespace available_extension_codec_detail {
using namespace available_extension_detail;
using Reader=inference::field_program_codec_detail::Reader;
using inference::field_program_codec_detail::blob;
using inference::field_program_codec_detail::canonical;
using inference::field_program_codec_detail::header;
inline constexpr std::array<std::byte,8> model_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'W'},std::byte{'X'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> family_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'W'},std::byte{'Y'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> choice_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'W'},std::byte{'Z'},std::byte{1},std::byte{0},std::byte{0}};
struct PassiveModel {
    std::uint8_t kind{};
    Bytes before, target_image, source_program, native, publication;
    std::optional<inference::value_admission_codec_detail::PassiveAvailableProgram> fixed;
    std::optional<available_evidence_codec_detail::PassiveEvaluation> selected;
    std::shared_ptr<const PassiveModel> parent;
    inference::value_admission_codec_detail::PassiveAvailableProgram target;
    inference::RelationalFieldExtensionDeclaration declaration;
    RelationalExtensionStatus status;
    Origin origin;
};
inline Bytes selected_program(const available_evidence_codec_detail::PassiveEvaluation& evaluation,const storage::TemporalReadLimits& policy){
    Reader r(evaluation.native,policy);header(r,evaluation.native,relational_affine_codec_detail::evaluation_magic);(void)r.blob();(void)relational_affine_codec_detail::vector(r);
    const auto input=r.blob();Reader a(input,policy);header(a,input,inference::field_interface_codec_detail::accession_magic);const auto fields=a.blob();
    Reader f(fields,policy);header(f,fields,inference::field_program_codec_detail::result_magic);const auto program=f.blob();return {program.begin(),program.end()};
}
inline PassiveModel preflight_model(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check,std::size_t depth=0){
    require(depth<policy.max_rank,"available extension lineage depth limit");if(check)check();Reader r(bytes,policy);header(r,bytes,model_magic);
    const auto kind=r.u8();require(kind<=2,"available extension parent kind");const auto before=r.blob(),target_bytes=r.blob();const auto d=inference::field_extension_codec_detail::declaration(r);
    const auto status=static_cast<RelationalExtensionStatus>(r.u8());const auto key=r.u64();const auto receipt=r.blob();const auto native=r.blob();require(!r.remaining(),"trailing available extension model bytes");
    std::optional<inference::value_admission_codec_detail::PassiveAvailableProgram> fixed;std::optional<available_evidence_codec_detail::PassiveEvaluation> selected;
    std::shared_ptr<const PassiveModel> parent;Bytes source_program,before_payload;
    if(!kind){fixed=inference::value_admission_codec_detail::preflight_program(before,context,policy,check);source_program=fixed->native;before_payload.assign(before.begin(),before.end());}
    else if(kind==1){selected=available_evidence_codec_detail::preflight_evaluation(before,context,policy,limits,check);source_program=selected_program(*selected,policy);before_payload.assign(before.begin(),before.end());}
    else{parent=std::make_shared<const PassiveModel>(preflight_model(before,context,policy,limits,check,depth+1));source_program=parent->target.native;
        before_payload=lineage_bytes(parent->publication,parent->origin,parent->native);}
    auto target=inference::value_admission_codec_detail::preflight_program(target_bytes,context,policy,check);
    const Bytes target_image(target_bytes.begin(),target_bytes.end());const auto publication=payload(kind,before_payload,target_image,d,status);
    const auto actual=context.require(key,role(status),publication);canonical(receipt,actual.receipt());
    // Claimed native parents, programs and declarations must match the admitted closure.
    Reader witness(native,policy);header(witness,native,inference::field_extension_codec_detail::model_magic);const auto continued=witness.flag();const auto native_before=witness.blob(),native_target=witness.blob();
    require(continued==(kind==2),"extension witness parent relationship changed");canonical(native_before,kind==2?parent->native:source_program);canonical(native_target,target.native);
    const auto native_d=inference::field_extension_codec_detail::declaration(witness);Writer expected_declaration,actual_declaration;
    inference::field_extension_codec_detail::declaration(expected_declaration,d);inference::field_extension_codec_detail::declaration(actual_declaration,native_d);
    canonical(expected_declaration.bytes(),actual_declaration.bytes());(void)witness.blob();require(!witness.remaining(),"trailing native extension witness bytes");
    return {kind,Bytes(before.begin(),before.end()),target_image,std::move(source_program),Bytes(native.begin(),native.end()),publication,
        std::move(fixed),std::move(selected),std::move(parent),std::move(target),d,status,actual};
}
inline PreparedAvailableRelationalExtension replay_model(const PassiveModel& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check){
    context.recheck(parsed.origin);if(check)check();auto target=inference::value_admission_codec_detail::replay_program(parsed.target,context,policy,limits,check);
    auto out=[&]{if(parsed.parent){const auto parent=replay_model(*parsed.parent,context,policy,limits,check);return parent.extend(target,parsed.declaration,parsed.status,parsed.origin.key(),context,limits,check);}
        AvailableRelationalExtensionParent before=parsed.fixed?AvailableRelationalExtensionParent{inference::value_admission_codec_detail::replay_program(*parsed.fixed,context,policy,limits,check)}:
            AvailableRelationalExtensionParent{available_evidence_codec_detail::replay_evaluation(*parsed.selected,context,policy,limits,check)};
        return PreparedAvailableRelationalExtension(std::move(before),target,parsed.declaration,parsed.status,parsed.origin.key(),context,limits,check);}();
    canonical(parsed.native,inference::encode_relational_field_extension(out.extension()));return out;
}
struct PassiveInput {
    Bytes asset;
    inference::RelationalInputAccessionDeclaration declaration;
    std::uint64_t use{};
    numeric::SparseRationalTensor field;
    inference::RelationalInputOrigins keys;
};
struct PassiveFamily {PassiveModel model;std::vector<PassiveInput> inputs;std::vector<Origin> origins;Bytes native;};
inline PassiveFamily preflight_family(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check){
    Reader r(bytes,policy);header(r,bytes,family_magic);auto model=preflight_model(r.blob(),context,policy,limits,check);std::vector<PassiveInput> inputs;std::vector<Origin> origins;
    const auto count=r.count(102);
    for(std::size_t i=0;i<count;++i){if(check)check();const auto raw=r.blob();Bytes asset(raw.begin(),raw.end());auto d=inference::field_interface_codec_detail::accession(r);
        const auto use=r.u64();auto field=storage::temporal_codec_detail::tensor(r);const inference::RelationalInputOrigins keys{r.u64(),r.u64(),r.u64(),r.u64()};
        const auto start=origins.size();inference::value_admission_detail::input_origins(origins,keys,asset,inference::relational_input_value_payload(asset,d,use,field),
            inference::relational_input_accession_payload(model.source_program,asset,d,use,field),context);
        for(std::size_t j=start;j<origins.size();++j)canonical(r.blob(),origins[j].receipt());
        inputs.push_back({std::move(asset),std::move(d),use,std::move(field),keys});}
    const auto native=r.blob();require(!r.remaining(),"trailing available extension family bytes");
    Reader witness(native,policy);header(witness,native,inference::field_extension_codec_detail::family_magic);canonical(witness.blob(),model.native);
    const auto source_input=witness.blob();Reader input(source_input,policy);header(input,source_input,inference::field_interface_codec_detail::accession_magic);
    const auto fields=input.blob();Reader value(fields,policy);header(value,fields,inference::field_program_codec_detail::result_magic);canonical(value.blob(),model.source_program);
    const auto input_count=input.count(70);require(input_count==inputs.size(),"extension native input order/count changed");
    for(const auto& row:inputs){canonical(input.blob(),row.asset);const auto actual_d=inference::field_interface_codec_detail::accession(input);Writer expected_d,claimed_d;
        inference::field_interface_codec_detail::accession(expected_d,row.declaration);inference::field_interface_codec_detail::accession(claimed_d,actual_d);canonical(expected_d.bytes(),claimed_d.bytes());
        require(input.u64()==row.use,"extension native input use changed");require(inference::field_interface_detail::equal(storage::temporal_codec_detail::tensor(input),row.field),"extension native source field changed");}
    require(!input.remaining(),"trailing extension native input bytes");(void)witness.blob();require(!witness.remaining(),"trailing extension native family witness");
    return {std::move(model),std::move(inputs),std::move(origins),Bytes(native.begin(),native.end())};
}
inline AvailableRelationalExtensionFamily replay_family(const PassiveFamily& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check){
    for(const auto& origin:parsed.origins){if(check)check();context.recheck(origin);}const auto model=replay_model(parsed.model,context,policy,limits,check);
    std::vector<inference::RelationalAvailableInputSubmission> inputs;
    for(const auto& row:parsed.inputs){if(check)check();auto asset=inference::decode_relational_input_use_asset(row.asset,policy,limits);
        inputs.push_back({{std::move(asset),row.declaration,row.use,row.field},row.keys});}
    auto out=model.execute_inputs(std::move(inputs),context,limits,check);canonical(parsed.native,inference::encode_relational_extension_family(out.family()));return out;
}
inline Bytes completion_payload(const PassiveFamily& family,const numeric::RationalVector& free){Writer w;
    blob(w,lineage_bytes(family.model.publication,family.model.origin,family.model.native));blob(w,family.native);w.u64(family.origins.size());
    for(const auto& origin:family.origins)blob(w,origin.receipt());
    inference::field_extension_codec_detail::vector(w,free);return std::move(w).take_bytes();}
struct PassiveEvaluation { PassiveFamily family; numeric::RationalVector free; Origin origin; Bytes native; };
inline PassiveEvaluation preflight_evaluation(std::span<const std::byte> bytes,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check){
    Reader r(bytes,policy);header(r,bytes,choice_magic);auto family=preflight_family(r.blob(),context,policy,limits,check);auto free=inference::field_extension_codec_detail::vector(r);
    const auto key=r.u64();const auto receipt=r.blob();const auto expected=r.blob();require(!r.remaining(),"trailing available extension evaluation bytes");
    auto origin=context.require(key,Role::Prospective,completion_payload(family,free));canonical(receipt,origin.receipt());
    return {std::move(family),std::move(free),std::move(origin),Bytes(expected.begin(),expected.end())};
}
inline AvailableRelationalExtensionEvaluation replay_evaluation(const PassiveEvaluation& parsed,const Context& context,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check){
    const auto prepared=replay_family(parsed.family,context,policy,limits,check);auto out=prepared.evaluate(parsed.free,parsed.origin.key(),context,limits,check);
    canonical(parsed.native,inference::encode_relational_extension_evaluation(out.result()));return out;
}
} // namespace available_extension_codec_detail
inline std::vector<std::byte> encode_available_relational_extension(const PreparedAvailableRelationalExtension& model){
    using namespace available_extension_codec_detail;const auto pin=model;const auto parent=pin.parent();const auto root=pin.root();const auto native=pin.extension();
    auto w=inference::field_program_codec_detail::envelope(model_magic);w.u8(parent?2:static_cast<std::uint8_t>(root->index()));
    blob(w,parent?encode_available_relational_extension(*parent):encode_parent(*root));blob(w,inference::encode_available_relational_program(pin.target()));
    inference::field_extension_codec_detail::declaration(w,native.declaration());w.u8(static_cast<std::uint8_t>(pin.status()));const auto origin=pin.origin();w.u64(origin.key());blob(w,origin.receipt());
    blob(w,inference::encode_relational_field_extension(native));return inference::field_program_codec_detail::finish(std::move(w));
}
inline PreparedAvailableRelationalExtension decode_available_relational_extension(std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}){
    using namespace available_extension_codec_detail;inference::value_admission_detail::bound(bytes.size(),policy.max_file_bytes);const Bytes frozen(bytes.begin(),bytes.end());const auto authority=context;const auto callback=std::move(check);
    const auto parsed=preflight_model(frozen,authority,policy,limits,callback);auto out=replay_model(parsed,authority,policy,limits,callback);canonical(frozen,encode_available_relational_extension(out));return out;
}
inline std::vector<std::byte> encode_available_relational_extension_family(const AvailableRelationalExtensionFamily& family){
    using namespace available_extension_codec_detail;const auto pin=family;auto w=inference::field_program_codec_detail::envelope(family_magic);blob(w,encode_available_relational_extension(pin.model()));
    w.u64(pin.inputs().size());std::size_t origin_index{};
    for(const auto& row:pin.inputs()){blob(w,inference::encode_relational_input_use_asset(row.input.asset));inference::field_interface_codec_detail::accession(w,row.input.declaration);
        w.u64(row.input.use);storage::temporal_codec_detail::tensor(w,row.input.source_field);for(const auto key:{row.origins.asset,row.origins.value,row.origins.accession,row.origins.reconstruction})w.u64(key);
        for(unsigned i=0;i<4;++i)blob(w,pin.obligations().at(origin_index++).receipt());}
    blob(w,inference::encode_relational_extension_family(pin.family()));return inference::field_program_codec_detail::finish(std::move(w));
}
inline AvailableRelationalExtensionFamily decode_available_relational_extension_family(std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}){
    using namespace available_extension_codec_detail;inference::value_admission_detail::bound(bytes.size(),policy.max_file_bytes);const Bytes frozen(bytes.begin(),bytes.end());const auto authority=context;const auto callback=std::move(check);
    const auto parsed=preflight_family(frozen,authority,policy,limits,callback);auto out=replay_family(parsed,authority,policy,limits,callback);canonical(frozen,encode_available_relational_extension_family(out));return out;
}
inline std::vector<std::byte> encode_available_relational_extension_evaluation(const AvailableRelationalExtensionEvaluation& evaluation){
    using namespace available_extension_codec_detail;const auto pin=evaluation;auto w=inference::field_program_codec_detail::envelope(choice_magic);blob(w,encode_available_relational_extension_family(pin.family()));
    const auto native=pin.result();inference::field_extension_codec_detail::vector(w,native.free_values());const auto origin=pin.origin();w.u64(origin.key());blob(w,origin.receipt());blob(w,inference::encode_relational_extension_evaluation(native));
    return inference::field_program_codec_detail::finish(std::move(w));
}
inline AvailableRelationalExtensionEvaluation decode_available_relational_extension_evaluation(std::span<const std::byte> bytes,const inference::RelationalAvailabilityContext& context,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}){
    using namespace available_extension_codec_detail;inference::value_admission_detail::bound(bytes.size(),policy.max_file_bytes);const Bytes frozen(bytes.begin(),bytes.end());const auto authority=context;const auto callback=std::move(check);
    const auto parsed=preflight_evaluation(frozen,authority,policy,limits,callback);auto out=replay_evaluation(parsed,authority,policy,limits,callback);
    canonical(frozen,encode_available_relational_extension_evaluation(out));return out;
}
} // namespace rfc::learning
