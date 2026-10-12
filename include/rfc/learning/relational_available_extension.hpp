#pragma once

#include "rfc/inference/relational_field_extension_codec.hpp"
#include "rfc/learning/relational_available_evidence_codec.hpp"
#include <variant>

namespace rfc::learning {
using AvailableRelationalExtensionParent = std::variant<inference::PreparedAvailableRelationalProgram, AvailableRelationalAffineEvaluation>;
enum class RelationalExtensionStatus : std::uint8_t { Observed, Prospective };
namespace available_extension_detail {
using namespace inference::field_extension_detail;
using Origin=inference::OwnedRelationalValueOrigin;
using Context=inference::RelationalAvailabilityContext;
using Role=inference::RelationalDependencyRole;
using Bytes=std::vector<std::byte>;
using Writer=inference::field_program_codec_detail::Writer;
inline inference::OwnedRelationalFieldProgram source(const AvailableRelationalExtensionParent& parent){
    return std::visit([](const auto& p)->inference::OwnedRelationalFieldProgram{
        if constexpr(std::is_same_v<std::decay_t<decltype(p)>,inference::PreparedAvailableRelationalProgram>)return p.program();
        else return p.result().result().result().program();},parent);
}
inline void admit(const AvailableRelationalExtensionParent& parent,const Context& context,const std::function<void()>& check){std::visit([&](const auto& p){p.admit(context,check);},parent);}
inline Bytes encode_parent(const AvailableRelationalExtensionParent& parent){
    return std::visit([](const auto& p)->Bytes{
        if constexpr(std::is_same_v<std::decay_t<decltype(p)>,inference::PreparedAvailableRelationalProgram>)return inference::encode_available_relational_program(p);
        else return encode_available_relational_affine_evaluation(p);},parent);
}
inline Bytes payload(std::uint8_t kind,const Bytes& before,const Bytes& target,const inference::RelationalFieldExtensionDeclaration& d,RelationalExtensionStatus status){
    Writer w;w.u8(1);w.u8(kind);inference::field_program_codec_detail::blob(w,before);inference::field_program_codec_detail::blob(w,target);
    inference::field_extension_codec_detail::declaration(w,d);w.u8(static_cast<std::uint8_t>(status));return std::move(w).take_bytes();
}
inline Role role(RelationalExtensionStatus status){require(status==RelationalExtensionStatus::Observed || status==RelationalExtensionStatus::Prospective,"extension publication status");return status==RelationalExtensionStatus::Observed?Role::Evidence:Role::Prospective;}
inline Bytes lineage_bytes(const Bytes& payload,const Origin& origin,const Bytes& native){
    Writer w;inference::field_program_codec_detail::blob(w,payload);inference::field_program_codec_detail::blob(w,origin.receipt());
    inference::field_program_codec_detail::blob(w,native);return std::move(w).take_bytes();
}
inline Bytes lineage(const Bytes& payload,const Origin& origin,const inference::OwnedRelationalFieldExtension& native){
    return lineage_bytes(payload,origin,inference::encode_relational_field_extension(native));
}
} // namespace available_extension_detail
inline std::vector<std::byte> relational_extension_payload(const AvailableRelationalExtensionParent& parent,
    const inference::PreparedAvailableRelationalProgram& target,const inference::RelationalFieldExtensionDeclaration& d,RelationalExtensionStatus status){
    return available_extension_detail::payload(static_cast<std::uint8_t>(parent.index()),available_extension_detail::encode_parent(parent),inference::encode_available_relational_program(target),d,status);
}
class AvailableRelationalExtensionFamily;
class AvailableRelationalExtensionEvaluation;
class PreparedAvailableRelationalExtension {
    struct State {
        std::optional<AvailableRelationalExtensionParent> root;
        std::shared_ptr<const State> parent;
        inference::PreparedAvailableRelationalProgram target;
        inference::OwnedRelationalFieldExtension native;
        RelationalExtensionStatus status;
        available_extension_detail::Origin origin;
        available_extension_detail::Bytes payload;
    };
public:
    PreparedAvailableRelationalExtension(AvailableRelationalExtensionParent parent,inference::PreparedAvailableRelationalProgram target,
        inference::RelationalFieldExtensionDeclaration declaration,RelationalExtensionStatus status,std::uint64_t origin,
        const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}){
        const auto before=parent;const auto next=target;const auto d=declaration;const auto authority=context;const auto callback=std::move(check);
        available_extension_detail::admit(before,authority,callback);next.admit(authority,callback);
        const auto payload=relational_extension_payload(before,next,d,status);const auto actual=authority.require(origin,available_extension_detail::role(status),payload);
        if(callback)callback();
        auto native=inference::OwnedRelationalFieldExtension(available_extension_detail::source(before),next.program(),d,limits,callback);
        state_=std::make_shared<const State>(State{before,nullptr,next,std::move(native),status,actual,payload});
    }
    inference::OwnedRelationalFieldExtension extension() const{return state_->native;}
    inference::PreparedAvailableRelationalProgram target() const{return state_->target;}
    std::optional<AvailableRelationalExtensionParent> root() const{return state_->root;}
    std::optional<PreparedAvailableRelationalExtension> parent() const{return state_->parent?std::optional<PreparedAvailableRelationalExtension>{PreparedAvailableRelationalExtension(state_->parent)}:std::nullopt;}
    RelationalExtensionStatus status() const{return state_->status;}
    available_extension_detail::Origin origin() const{return state_->origin;}
    const available_extension_detail::Bytes& publication_payload() const &{return state_->payload;}
    const available_extension_detail::Bytes& publication_payload() const &&=delete;
    available_extension_detail::Bytes lineage_receipt() const{const auto pin=state_;return available_extension_detail::lineage(pin->payload,pin->origin,pin->native);}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const{
        const auto pin=state_;const auto authority=context;const auto callback=std::move(check);
        if(pin->parent)PreparedAvailableRelationalExtension(pin->parent).admit(authority,callback);else available_extension_detail::admit(*pin->root,authority,callback);
        pin->target.admit(authority,callback);if(callback)callback();authority.recheck(pin->origin);
    }
    available_extension_detail::Bytes extension_payload(const inference::PreparedAvailableRelationalProgram& target,
        const inference::RelationalFieldExtensionDeclaration& d,RelationalExtensionStatus status) const{
        return available_extension_detail::payload(2,lineage_receipt(),inference::encode_available_relational_program(target),d,status);
    }
    PreparedAvailableRelationalExtension extend(inference::PreparedAvailableRelationalProgram target,inference::RelationalFieldExtensionDeclaration declaration,
        RelationalExtensionStatus status,std::uint64_t origin,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const{
        const auto owner=*this;const auto pin=owner.state_;const auto next=target;const auto d=declaration;const auto authority=context;const auto callback=std::move(check);
        owner.admit(authority,callback);next.admit(authority,callback);const auto payload=owner.extension_payload(next,d,status);
        const auto actual=authority.require(origin,available_extension_detail::role(status),payload);if(callback)callback();
        auto native=pin->native.extend(next.program(),d,limits,callback);
        return PreparedAvailableRelationalExtension(std::make_shared<const State>(State{std::nullopt,pin,next,std::move(native),status,actual,payload}));
    }
    AvailableRelationalExtensionFamily execute_inputs(std::vector<inference::RelationalAvailableInputSubmission> inputs,
        const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
private:
    explicit PreparedAvailableRelationalExtension(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
class AvailableRelationalExtensionFamily {
    struct State {
        PreparedAvailableRelationalExtension model;
        std::vector<inference::RelationalAvailableInputSubmission> inputs;
        std::vector<available_extension_detail::Origin> origins;
        inference::RelationalExtensionFieldFamily native;
    };
public:
    PreparedAvailableRelationalExtension model() const{return state_->model;}
    inference::RelationalExtensionFieldFamily family() const{return state_->native;}
    const std::vector<inference::RelationalAvailableInputSubmission>& inputs() const &{return state_->inputs;}
    const std::vector<available_extension_detail::Origin>& obligations() const &{return state_->origins;}
    const std::vector<inference::RelationalAvailableInputSubmission>& inputs() const &&=delete;
    const std::vector<available_extension_detail::Origin>& obligations() const &&=delete;
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const{
        const auto pin=state_;const auto authority=context;const auto callback=std::move(check);pin->model.admit(authority,callback);
        for(const auto& origin:pin->origins){if(callback)callback();authority.recheck(origin);}
    }
    AvailableRelationalExtensionEvaluation evaluate(numeric::RationalVector free,std::uint64_t origin,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
private:
    friend class PreparedAvailableRelationalExtension;
    explicit AvailableRelationalExtensionFamily(State state):state_(std::make_shared<const State>(std::move(state))){}
    std::shared_ptr<const State> state_;
};
inline AvailableRelationalExtensionFamily PreparedAvailableRelationalExtension::execute_inputs(std::vector<inference::RelationalAvailableInputSubmission> inputs,
    const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) const{
    const auto owner=*this;const auto pin=owner.state_;const auto frozen=inputs;const auto authority=context;const auto callback=std::move(check);owner.admit(authority,callback);
    const auto program=pin->native.source();const auto program_bytes=inference::encode_relational_field_program(program);std::vector<available_extension_detail::Origin> origins;
    for(const auto& row:frozen){if(callback)callback();const auto asset=inference::encode_relational_input_use_asset(row.input.asset);
        const auto value=inference::relational_input_value_payload(asset,row.input.declaration,row.input.use,row.input.source_field);
        const auto accession=inference::relational_input_accession_payload(program_bytes,asset,row.input.declaration,row.input.use,row.input.source_field);
        inference::value_admission_detail::input_origins(origins,row.origins,asset,value,accession,authority);}
    std::vector<inference::RelationalAccessionInput> admitted;
    for(const auto& row:frozen){if(callback)callback();const inference::OwnedRelationalInputAccession a(row.input.asset,program,row.input.declaration,limits,callback);
        admitted.push_back(a.admit(row.input.use,row.input.source_field,limits,callback));}
    auto source_input=inference::execute_relational_inputs(program,std::move(admitted),limits,callback);auto family=pin->native.execute(std::move(source_input),limits,callback);
    return AvailableRelationalExtensionFamily({owner,frozen,std::move(origins),std::move(family)});
}
inline std::vector<std::byte> relational_extension_completion_payload(const AvailableRelationalExtensionFamily& family,const numeric::RationalVector& free){
    available_extension_detail::Writer w;inference::field_program_codec_detail::blob(w,family.model().lineage_receipt());
    inference::field_program_codec_detail::blob(w,inference::encode_relational_extension_family(family.family()));w.u64(family.obligations().size());
    for(const auto& origin:family.obligations())inference::field_program_codec_detail::blob(w,origin.receipt());
    inference::field_extension_codec_detail::vector(w,free);return std::move(w).take_bytes();
}
class AvailableRelationalExtensionEvaluation {
public:
    AvailableRelationalExtensionFamily family() const{return family_;}
    inference::RelationalExtensionEvaluation result() const{return result_;}
    available_extension_detail::Origin origin() const{return origin_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const{
        const auto pin=*this;const auto authority=context;const auto callback=std::move(check);pin.family_.admit(authority,callback);if(callback)callback();authority.recheck(pin.origin_);
    }
private:
    friend class AvailableRelationalExtensionFamily;
    AvailableRelationalExtensionEvaluation(AvailableRelationalExtensionFamily family,inference::RelationalExtensionEvaluation result,available_extension_detail::Origin origin)
        :family_(std::move(family)),result_(std::move(result)),origin_(std::move(origin)){}
    AvailableRelationalExtensionFamily family_;inference::RelationalExtensionEvaluation result_;available_extension_detail::Origin origin_;
};
inline AvailableRelationalExtensionEvaluation AvailableRelationalExtensionFamily::evaluate(numeric::RationalVector free,std::uint64_t origin,
    const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) const{
    const auto owner=*this;const auto pin=owner.state_;const auto frozen=free;const auto authority=context;const auto callback=std::move(check);owner.admit(authority,callback);
    const auto actual=authority.require(origin,available_extension_detail::Role::Prospective,relational_extension_completion_payload(owner,frozen));if(callback)callback();
    return {owner,pin->native.evaluate(frozen,limits,callback),actual};
}
} // namespace rfc::learning
