#pragma once

#include "rfc/learning/relational_finite_operator.hpp"

namespace rfc::learning {
namespace finite_operator_codec_detail {
using namespace inference::relational_finite_source_codec_detail;
inline constexpr std::array<std::byte, 8> operator_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Q'},std::byte{'O'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> instance_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Q'},std::byte{'I'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> family_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Q'},std::byte{'R'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> completion_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Q'},std::byte{'E'},std::byte{1},std::byte{0},std::byte{0}};
inline void operator_declaration(Writer& w, const RelationalFiniteOperatorDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.ports.size()); for (const auto& p : d.ports) { w.u64(p.key); w.u64(p.rule); w.u64(p.input); }
}
inline RelationalFiniteOperatorDeclaration operator_declaration(Reader& r) {
    RelationalFiniteOperatorDeclaration d{r.u64(),r.u64(),{}}; const auto count=r.count(24);
    for(std::size_t i=0;i<count;++i) { d.ports.push_back({r.u64(),r.u64(),size(r)}); } return d;
}
inline void instance_declaration(Writer& w, const RelationalFiniteOperatorInstanceDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); const auto& t=d.transport; w.u64(t.key); w.u64(t.revision);
    inference::field_interface_codec_detail::source_map(w,t.source_map); w.u64(t.uses.size());
    for(const auto& use:t.uses){w.u64(use.reference);w.u64(use.input);}
}
inline RelationalFiniteOperatorInstanceDeclaration instance_declaration(Reader& r) {
    RelationalFiniteOperatorInstanceDeclaration d;d.key=r.u64();d.revision=r.u64();auto& t=d.transport;t.key=r.u64();t.revision=r.u64();
    t.source_map=inference::field_interface_codec_detail::source_map(r);const auto count=r.count(16);
    for(std::size_t i=0;i<count;++i){t.uses.push_back({r.u64(),r.u64()});}return d;
}
} // namespace finite_operator_codec_detail
inline std::vector<std::byte> encode_relational_finite_operator(const PreparedRelationalFiniteFieldOperator& op) {
    using namespace finite_operator_codec_detail;const auto pin=op;auto w=envelope(operator_magic);
    blob(w,encode_relational_finite_field_map(pin.reference()));operator_declaration(w,pin.declaration());return finish(std::move(w));
}
inline PreparedRelationalFiniteFieldOperator decode_relational_finite_operator(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace finite_operator_codec_detail;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    Reader r(frozen,read);header(r,frozen,operator_magic);const auto reference=r.blob();auto d=operator_declaration(r);
    inference::field_program_detail::require(!r.remaining(),"trailing finite operator archive");if(check){check();}
    const auto parent=decode_relational_finite_field_map(reference,read,limits,check);PreparedRelationalFiniteFieldOperator op(parent,std::move(d),limits,check);
    canonical(frozen,encode_relational_finite_operator(op));return op;
}
inline std::vector<std::byte> encode_relational_finite_operator_instance(const RelationalFiniteOperatorInstance& instance) {
    using namespace finite_operator_codec_detail;const auto pin=instance;auto w=envelope(instance_magic);
    blob(w,encode_relational_finite_operator(pin.operator_program()));blob(w,inference::encode_relational_finite_source(pin.source()));instance_declaration(w,pin.declaration());
    w.u64(pin.inputs().size());for(const auto& row:pin.inputs()) {w.u64(row.port);accession(w,row.input.declaration);w.u64(row.input.use);storage::temporal_codec_detail::tensor(w,row.input.source_field);}
    blob(w,encode_relational_finite_field_map(pin.map()));return finish(std::move(w));
}
inline RelationalFiniteOperatorInstance decode_relational_finite_operator_instance(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace finite_operator_codec_detail;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);
    Reader r(frozen,read);header(r,frozen,instance_magic);const auto parent=r.blob();const auto source=r.blob();const auto d=instance_declaration(r);
    struct PassiveInput {std::uint64_t port,use;inference::RelationalInputAccessionDeclaration declaration;numeric::SparseRationalTensor field;};
    std::vector<PassiveInput> passive;const auto count=r.count(89);
    for(std::size_t i=0;i<count;++i){const auto port=r.u64();auto declaration=accession(r);const auto use=r.u64();auto field=storage::temporal_codec_detail::tensor(r);passive.push_back({port,use,std::move(declaration),std::move(field)});}
    const auto witness=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing finite operator instance");if(check){check();}
    const auto op=decode_relational_finite_operator(parent,read,limits,check);const auto independent=inference::decode_relational_finite_source(source,read,limits,check);const auto asset=independent.asset();
    std::vector<RelationalFiniteOperatorInput> inputs;for(auto& row:passive) inputs.push_back({row.port,{asset,std::move(row.declaration),row.use,std::move(row.field)}});
    auto instance=op.instantiate(independent,d,std::move(inputs),limits,check);canonical(witness,encode_relational_finite_field_map(instance.map()));canonical(frozen,encode_relational_finite_operator_instance(instance));return instance;
}
inline std::vector<std::byte> encode_relational_finite_operator_result(const RelationalFiniteOperatorResult& family) {
    using namespace finite_operator_codec_detail;const auto pin=family;auto w=envelope(family_magic);blob(w,encode_relational_finite_operator_instance(pin.instance()));
    blob(w,encode_relational_finite_field_result(pin.result()));return finish(std::move(w));
}
inline RelationalFiniteOperatorResult decode_relational_finite_operator_result(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace finite_operator_codec_detail;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);Reader r(frozen,read);header(r,frozen,family_magic);
    const auto instance=r.blob();const auto witness=r.blob();inference::field_program_detail::require(!r.remaining(),"trailing finite operator result");if(check){check();}
    const auto parent=decode_relational_finite_operator_instance(instance,read,limits,check);auto family=parent.execute(limits,check);
    canonical(witness,encode_relational_finite_field_result(family.result()));canonical(frozen,encode_relational_finite_operator_result(family));return family;
}
inline std::vector<std::byte> encode_relational_finite_operator_evaluation(const RelationalFiniteOperatorEvaluation& evaluation) {
    using namespace finite_operator_codec_detail;const auto pin=evaluation;auto w=envelope(completion_magic);blob(w,encode_relational_finite_operator_result(pin.family()));
    const auto raw=pin.result();w.u64(raw.ordinal());relational_affine_codec_detail::vector(w,raw.free_values());blob(w,encode_relational_finite_field_evaluation(raw));return finish(std::move(w));
}
inline RelationalFiniteOperatorEvaluation decode_relational_finite_operator_evaluation(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& policy,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace finite_operator_codec_detail;const auto read=policy;const auto frozen=frozen_bytes(bytes,read);Reader r(frozen,read);header(r,frozen,completion_magic);
    const auto family=r.blob();const auto ordinal=size(r);auto free=relational_affine_codec_detail::vector(r);const auto witness=r.blob();
    inference::field_program_detail::require(!r.remaining(),"trailing finite operator evaluation");if(check){check();}
    const auto parent=decode_relational_finite_operator_result(family,read,limits,check);auto result=parent.evaluate(ordinal,std::move(free),limits,check);
    canonical(witness,encode_relational_finite_field_evaluation(result.result()));canonical(frozen,encode_relational_finite_operator_evaluation(result));return result;
}
} // namespace rfc::learning
