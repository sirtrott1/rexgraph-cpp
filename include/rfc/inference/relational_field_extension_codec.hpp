#pragma once

#include "rfc/inference/relational_field_extension.hpp"
#include "rfc/inference/relational_field_interface_codec.hpp"

namespace rfc::inference {
namespace field_extension_codec_detail {
using namespace field_program_codec_detail;
inline constexpr std::array<std::byte, 8> model_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'F'},std::byte{'X'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> family_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'F'},std::byte{'Y'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> choice_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'F'},std::byte{'Z'},std::byte{1},std::byte{0},std::byte{0}};
inline void declaration(Writer& w, const RelationalFieldExtensionDeclaration& d) {
    w.u64(d.key);w.u64(d.revision);w.u64(d.free_carrier);w.u64(d.free_basis);w.u64(d.free_directions.size());
    for(const auto key:d.free_directions)w.u64(key);
    field_interface_codec_detail::source_map(w,d.source_map);w.u64(d.stalks.size());
    for(const auto& s:d.stalks){field_interface_codec_detail::local_cell(w,s.cell);w.u64(s.source_basis);w.u64(s.target_basis);
        storage::temporal_codec_detail::matrix(w,s.inclusion);storage::temporal_codec_detail::matrix(w,s.reconstruction);}
    w.u8(d.isometric?1:0);
}
inline RelationalFieldExtensionDeclaration declaration(Reader& r) {
    RelationalFieldExtensionDeclaration d;d.key=r.u64();d.revision=r.u64();d.free_carrier=r.u64();d.free_basis=r.u64();const auto free=r.count(8);
    for(std::size_t i=0;i<free;++i)d.free_directions.push_back(r.u64());
    d.source_map=field_interface_codec_detail::source_map(r);const auto n=r.count(30);
    for(std::size_t i=0;i<n;++i){const auto cell=field_interface_codec_detail::local_cell(r);const auto source=r.u64(),target=r.u64();
        auto j=storage::temporal_codec_detail::matrix(r);auto back=storage::temporal_codec_detail::matrix(r);d.stalks.push_back({cell,source,target,std::move(j),std::move(back)});}
    d.isometric=r.flag();return d;
}
inline void vector(Writer& w,const numeric::RationalVector& v){w.u64(v.size());for(const auto& q:v)storage::temporal_codec_detail::rational(w,q);}
inline numeric::RationalVector vector(Reader& r){numeric::RationalVector v;const auto n=r.count(10);for(std::size_t i=0;i<n;++i)v.push_back(r.rational());return v;}
inline std::vector<std::byte> witness(const OwnedRelationalFieldExtension& model) {
    Writer w;const auto p=model.target().publication();w.u64(p.endpoint().source.grade_count());
    for(std::size_t k=0;k<p.endpoint().source.grade_count();++k){const auto g=field_program_detail::grade(k);
        storage::temporal_codec_detail::matrix(w,model.inclusion(g));storage::temporal_codec_detail::matrix(w,model.reconstruction(g));storage::temporal_codec_detail::matrix(w,model.kernel(g));}
    return std::move(w).take_bytes();
}
inline void affine(Writer& w,const RelationalExtensionAffineField& f){storage::temporal_codec_detail::tensor(w,f.particular);storage::temporal_codec_detail::matrix(w,f.directions);}
inline std::vector<std::byte> witness(const RelationalExtensionFieldFamily& family){
    Writer w;w.u64(family.fields().size());for(const auto& f:family.fields()){w.u64(f.node);w.u16(f.grade.value);affine(w,f.value);w.u8(f.occurrences?1:0);if(f.occurrences)affine(w,*f.occurrences);}return std::move(w).take_bytes();
}
inline OwnedAccessionFieldResult replay_input(std::span<const std::byte> bytes, const OwnedRelationalFieldProgram& actual,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits,const std::function<void()>& check={}) {
    Reader r(bytes,policy);header(r,bytes,field_interface_codec_detail::accession_magic);const auto expected=r.blob();
    Reader field(expected,policy);header(field,expected,result_magic);canonical(field.blob(),encode_relational_field_program(actual));
    std::vector<RelationalAccessionInput> inputs;const auto n=r.count(70);
    for(std::size_t i=0;i<n;++i){if(check)check();auto asset=decode_relational_input_use_asset(r.blob(),policy,limits);auto d=field_interface_codec_detail::accession(r);
        const OwnedRelationalInputAccession accession(std::move(asset),actual,std::move(d),limits,check);const auto use=r.u64();auto values=storage::temporal_codec_detail::tensor(r);
        inputs.push_back(accession.admit(use,std::move(values),limits,check));}
    field_program_detail::require(!r.remaining(),"trailing extension source input bytes");auto out=execute_relational_inputs(actual,std::move(inputs),limits,check);
    canonical(expected,encode_relational_field_result(out.result()));canonical(bytes,encode_accession_field_result(out));return out;
}
} // namespace field_extension_codec_detail
inline std::vector<std::byte> encode_relational_field_extension(const OwnedRelationalFieldExtension& model) {
    using namespace field_extension_codec_detail;const auto pin=model;auto w=envelope(model_magic);const auto parent=pin.parent();w.u8(parent?1:0);
    blob(w,parent?encode_relational_field_extension(*parent):encode_relational_field_program(pin.source()));blob(w,encode_relational_field_program(pin.target()));
    declaration(w,pin.declaration());blob(w,witness(pin));return finish(std::move(w));
}
namespace field_extension_codec_detail {
inline OwnedRelationalFieldExtension decode_model(std::span<const std::byte> bytes,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits,const std::function<void()>& check,std::size_t depth) {
    field_program_detail::require(depth<policy.max_rank,"extension lineage depth limit");if(check)check();Reader r(bytes,policy);header(r,bytes,model_magic);
    const auto has_parent=r.flag();const auto before=r.blob(),next=r.blob();const auto d=declaration(r);const auto expected=r.blob();field_program_detail::require(!r.remaining(),"trailing extension model bytes");
    auto target=decode_relational_field_program(next,policy,limits);auto out=[&]{if(has_parent){auto previous=decode_model(before,policy,limits,check,depth+1);return previous.extend(target,d,limits,check);}
        return OwnedRelationalFieldExtension(decode_relational_field_program(before,policy,limits),target,d,limits,check);}();
    canonical(expected,witness(out));canonical(bytes,encode_relational_field_extension(out));return out;
}
} // namespace field_extension_codec_detail
inline OwnedRelationalFieldExtension decode_relational_field_extension(std::span<const std::byte> bytes,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}){return field_extension_codec_detail::decode_model(bytes,policy,limits,check,0);}
inline std::vector<std::byte> encode_relational_extension_family(const RelationalExtensionFieldFamily& family){
    using namespace field_extension_codec_detail;const auto pin=family;auto w=envelope(family_magic);blob(w,encode_relational_field_extension(pin.extension()));
    blob(w,encode_accession_field_result(pin.input()));blob(w,witness(pin));return finish(std::move(w));
}
inline RelationalExtensionFieldFamily decode_relational_extension_family(std::span<const std::byte> bytes,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}){
    using namespace field_extension_codec_detail;Reader r(bytes,policy);header(r,bytes,family_magic);const auto model=decode_relational_field_extension(r.blob(),policy,limits,check);
    const auto input=replay_input(r.blob(),model.source(),policy,limits,check);const auto expected=r.blob();field_program_detail::require(!r.remaining(),"trailing extension family bytes");
    auto out=model.execute(input,limits,check);canonical(expected,witness(out));canonical(bytes,encode_relational_extension_family(out));return out;
}
inline std::vector<std::byte> encode_relational_extension_evaluation(const RelationalExtensionEvaluation& evaluation){
    using namespace field_extension_codec_detail;const auto pin=evaluation;auto w=envelope(choice_magic);blob(w,encode_relational_extension_family(pin.family()));
    vector(w,pin.free_values());blob(w,encode_relational_field_result(pin.result()));return finish(std::move(w));
}
inline RelationalExtensionEvaluation decode_relational_extension_evaluation(std::span<const std::byte> bytes,const storage::TemporalReadLimits& policy,
    numeric::ExactActionLimits limits={},std::function<void()> check={}){
    using namespace field_extension_codec_detail;Reader r(bytes,policy);header(r,bytes,choice_magic);const auto family=decode_relational_extension_family(r.blob(),policy,limits,check);
    const auto free=vector(r);const auto expected=r.blob();field_program_detail::require(!r.remaining(),"trailing extension choice bytes");auto out=family.evaluate(free,limits,check);
    canonical(expected,encode_relational_field_result(out.result()));canonical(bytes,encode_relational_extension_evaluation(out));return out;
}
} // namespace rfc::inference
