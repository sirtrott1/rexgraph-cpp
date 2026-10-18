#pragma once

#include "rfc/learning/relational_available_evidence_codec.hpp"
#include "rfc/learning/relational_literal_field_family_codec.hpp"

namespace rfc::learning {
struct RelationalJointReaderOrigins {
    inference::RelationalInputOrigins input;
    inference::RelationalProgramOrigins original_program;
    inference::RelationalInputOrigins original_input;
    std::uint64_t reader{};
};
struct RelationalAvailableJointInput {
    RelationalLiteralFamilyInput input;
    RelationalJointReaderOrigins origins;
};
struct RelationalJointObservationReaders {
    std::uint64_t observation{};
    std::vector<RelationalAvailableJointInput> inputs;
};
struct RelationalJointFamilyOrigins {
    std::vector<std::uint64_t> interfaces;
    std::uint64_t family{};
};
namespace available_joint_detail {
using Origin = inference::OwnedRelationalValueOrigin;
using Context = inference::RelationalAvailabilityContext;
using Role = inference::RelationalDependencyRole;
using Bytes = std::vector<std::byte>;
using Packet = storage::PreparedBytePacket;
using Builder = storage::BytePacketBuilder;
using PacketContext = relational_affine_codec_detail::PacketContext;
using Writer = inference::field_program_codec_detail::Writer;
using namespace inference::field_program_detail;
inline constexpr auto magic(char kind) {
    return std::array<std::byte,8>{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'A'},std::byte{'J'},std::byte{1},static_cast<std::byte>(kind),std::byte{0}};
}
inline void recheck(const std::vector<Origin>& values,const Context& context,const std::function<void()>& check) {
    for(const auto& value:values) { if(check) check(); context.recheck(value); }
}
inline void merge(std::vector<Origin>& out,const std::vector<Origin>& values) {
    for(const auto& value:values) {
        const auto found=std::find_if(out.begin(),out.end(),[&](const auto& old){return old.key()==value.key();});
        if(found==out.end()) out.push_back(value);
        else require(found->same_owner(value),"joint closure changes an actual origin owner");
    }
}
inline inference::RelationalAccessionInput raw(const RelationalLiteralFamilyInput& value) {
    return std::visit([](const auto& input){return input.input();},value);
}
inline bool empty(inference::RelationalProgramOrigins keys) { return !keys.program && !keys.maps && !keys.metrics; }
inline bool empty(inference::RelationalInputOrigins keys) { return !keys.asset && !keys.value && !keys.accession && !keys.reconstruction; }
inline void keys(Builder& w,inference::RelationalInputOrigins k) {
    for(const auto key:{k.asset,k.value,k.accession,k.reconstruction}) w.u64(key);
}
inline void keys(Builder& w,const RelationalJointReaderOrigins& k) {
    keys(w,k.input);for(const auto key:{k.original_program.program,k.original_program.maps,k.original_program.metrics})w.u64(key);
    keys(w,k.original_input);w.u64(k.reader);
}
// The omitted payload is retained once in the containing typed value. A decoder
// compares that complete payload before checking this exact publisher prefix.
inline Packet origin_prefix(Origin origin) {
    return Packet::pin(std::move(origin),[](const auto& value){
        return std::span<const std::byte>(value.receipt()).first(value.receipt().size()-value.payload_bytes().size());
    });
}
inline Packet recipe(const RelationalLiteralFamilyInput& value,std::size_t maximum,const std::function<void()>& check) {
    return std::visit([&](const auto& input)->Packet {
        using Value=std::decay_t<decltype(input)>;
        if constexpr(std::is_same_v<Value,inference::OwnedRelationalLiteralInput>)return inference::prepare_relational_literal_input(input,maximum,check);
        else {
            const auto graph=input.graph();require(graph.literal_parent().has_value(),"available joint reader needs the original literal recipe");
            return inference::prepare_relational_known_tensor_graph_input(input,maximum,check);
        }
    },value);
}
inline std::vector<Origin> input_origins(const RelationalAvailableJointInput& row,const Context& context,
    std::size_t maximum,const std::function<void()>& check) {
    if(check){check();}std::vector<Origin> out;
    inference::value_admission_detail::input_origins(out,{raw(row.input),row.origins.input},context);
    if(const auto* graph=std::get_if<inference::RelationalKnownTensorGraphInput>(&row.input)) {
        const auto source=graph->graph().parent();const auto a=source.accession();
        auto programs=inference::value_admission_detail::program_origins(a.program(),row.origins.original_program,context);
        out.insert(out.end(),programs.begin(),programs.end());
        inference::value_admission_detail::input_origins(out,{source,row.origins.original_input},context);
    } else require(empty(row.origins.original_program)&&empty(row.origins.original_input),"direct literal has extraneous graph-parent origins");
    out.push_back(context.require_packet(row.origins.reader,Role::Interface,recipe(row.input,maximum,check),check));return out;
}
inline void input_image(Builder& w,const inference::RelationalAccessionInput& value,PacketContext& packets) {
    const auto a=value.accession();w.blob(packets.program(a.program()));w.blob(packets.asset(a.asset()));
    w.write([&](auto& writer){inference::field_interface_codec_detail::accession(writer,a.declaration());writer.u64(value.use());storage::temporal_codec_detail::tensor(writer,value.source_field());});
}
inline Packet input_packet(const RelationalAvailableJointInput& value,const std::vector<Origin>& required,PacketContext& packets) {
    Builder w(magic('I'),packets.max_bytes);w.u8(static_cast<std::uint8_t>(value.input.index()));
    w.blob(recipe(value.input,packets.max_bytes,packets.check));input_image(w,raw(value.input),packets);keys(w,value.origins);
    if(const auto* graph=std::get_if<inference::RelationalKnownTensorGraphInput>(&value.input))input_image(w,graph->graph().parent(),packets);
    w.u64(required.size());for(const auto& origin:required)w.receipt(origin);return std::move(w).finish(packets.check);
}
struct InterfaceImage {
    std::uint64_t key{},revision{};std::size_t left{},right{};std::uint64_t source_namespace{};
    Bytes source,endpoint;algebra::RetainedSourceMap common_left,common_right;
    std::vector<std::vector<TransportContextView>> left_views,right_views;
};
struct DeclarationImage { std::uint64_t key{},revision{};std::vector<InterfaceImage> interfaces; };
inline DeclarationImage image(const RelationalFieldFamilyDeclaration& d) {
    DeclarationImage out{d.key,d.revision,{}};
    for(const auto& i:d.interfaces)out.interfaces.push_back({i.key,i.revision,i.left,i.right,i.common.source_namespace(),
        storage::encode_rfcx(i.common.endpoint().source).bytes,storage::encode_temporal_endpoint(i.common.endpoint()),i.common_left,i.common_right,i.left_views,i.right_views});
    return out;
}
inline void interface_image(Writer& w,const InterfaceImage& i) {
    using namespace inference::field_program_codec_detail;
    w.u64(i.key);w.u64(i.revision);w.u64(i.left);w.u64(i.right);w.u64(i.source_namespace);blob(w,i.source);blob(w,i.endpoint);
    relational_affine_codec_detail::source_map(w,i.common_left);relational_affine_codec_detail::source_map(w,i.common_right);
    relational_family_codec_detail::views(w,i.left_views);relational_family_codec_detail::views(w,i.right_views);
}
inline Packet declaration_packet(const DeclarationImage& d,std::size_t maximum,const std::function<void()>& check) {
    Builder w(magic('D'),maximum);w.u64(d.key);w.u64(d.revision);w.u64(d.interfaces.size());
    for(const auto& i:d.interfaces) {if(check){check();}w.write([&](auto& writer){interface_image(writer,i);});}return std::move(w).finish(check);
}
inline Packet interface_payload(const InterfaceImage& i,std::span<const std::byte> left,std::span<const std::byte> right,
    std::size_t maximum,const std::function<void()>& check) {
    Builder w(magic('F'),maximum);w.write([&](auto& writer){interface_image(writer,i);});w.blob(left);w.blob(right);return std::move(w).finish(check);
}
inline Packet proof_packet(const PreparedRelationalFieldFamily& family,std::size_t maximum,const std::function<void()>& check) {
    Builder w(magic('P'),maximum);w.write([&](auto& writer){relational_family_codec_detail::proof(writer,family.gluing());});return std::move(w).finish(check);
}
struct Content { Packet packet;std::vector<Origin> required; };
inline Content content(const PreparedRelationalFieldFamily& family,const std::vector<PreparedAvailableRelationalEvidence>& members,
    const std::vector<std::vector<RelationalJointObservationReaders>>& readers,const RelationalJointFamilyOrigins& origins,
    const Context& context,std::size_t maximum,const std::function<void()>& check) {
    const auto limits=family.limits();numeric::ExactActionGuard guard(limits,check);guard.dimension(members.size());
    require(members.size()==family.members().size()&&readers.size()==members.size(),"joint available members or training reader rows omitted");
    require(origins.interfaces.size()==family.declaration().interfaces.size(),"joint common interface origins omitted");
    PacketContext packets(maximum,check);Builder w(magic('B'),maximum);std::vector<Origin> required;
    w.u64(limits.dimension);w.u64(limits.entries);w.u64(limits.steps);w.u64(limits.integer_bits);
    const auto d=image(family.declaration());w.blob(declaration_packet(d,maximum,check));w.u64(members.size());
    for(std::size_t m=0;m<members.size();++m) {
        guard.step();const auto actual=members[m].evidence();require(actual.same_owner(family.members()[m]),"joint family requires actual available evidence member owners");
        members[m].admit(context,check);merge(required,members[m].obligations());w.blob(available_evidence_codec_detail::prepare_evidence(members[m],packets));
    }
    w.blob(proof_packet(family,maximum,check));w.u64(readers.size());
    for(std::size_t m=0;m<readers.size();++m) {
        const auto evidence=members[m].evidence();const auto& observations=evidence.observations();
        require(readers[m].size()==observations.size(),"joint training reader coverage omits an original observation");w.u64(readers[m].size());
        for(std::size_t j=0;j<readers[m].size();++j) {
            guard.step();const auto& row=readers[m][j];const auto& observation=observations[j];
            require(row.observation==observation.key&&row.inputs.size()==observation.input.inputs().size(),"joint training reader order or input count differs");
            const auto dependencies=members[m].observation_dependencies(j,context,limits.entries,check);w.u64(row.observation);w.u64(row.inputs.size());
            for(std::size_t k=0;k<row.inputs.size();++k) {
                const auto input=raw(row.inputs[k].input);require(input.accession().program().same_owner(evidence.program()),"joint reader has a foreign actual training program");
                require(inference::relational_input_accession_payload(input)==inference::relational_input_accession_payload(observation.input.inputs()[k]),"joint reader changes an original observed input");
                const auto checked=input_origins(row.inputs[k],context,maximum,check);
                for(std::size_t p=0;p<4;++p)require(std::any_of(dependencies.begin(),dependencies.end(),[&](const auto& original){return original.same_owner(checked[p]);}),"joint training input replaces an original availability receipt");
                merge(required,checked);w.blob(input_packet(row.inputs[k],checked,packets));
            }
        }
    }
    w.u64(origins.interfaces.size());
    for(std::size_t j=0;j<origins.interfaces.size();++j) {
        guard.step();const auto& i=d.interfaces[j];const auto left=members.at(i.left).program().program(),right=members.at(i.right).program().program();
        const auto origin=context.require_packet(origins.interfaces[j],Role::Interface,
            interface_payload(i,inference::encode_relational_field_program(left),inference::encode_relational_field_program(right),maximum,check),check);
        merge(required,{origin});w.u64(origin.key());w.receipt(origin);
    }
    guard.entries(required.size());recheck(required,context,check);return {std::move(w).finish(check),std::move(required)};
}
} // namespace available_joint_detail

inline storage::PreparedBytePacket prepare_relational_joint_family_interface_payload(const PreparedRelationalFieldFamily& family,
    std::size_t interface,std::size_t maximum,std::function<void()> check={}) {
    const auto pin=family;const auto d=available_joint_detail::image(pin.declaration());const auto& i=d.interfaces.at(interface);
    return available_joint_detail::interface_payload(i,inference::encode_relational_field_program(pin.members().at(i.left).program()),
        inference::encode_relational_field_program(pin.members().at(i.right).program()),maximum,check);
}
inline storage::PreparedBytePacket prepare_relational_joint_family_payload(const PreparedRelationalFieldFamily& family,
    const std::vector<PreparedAvailableRelationalEvidence>& members,const std::vector<std::vector<RelationalJointObservationReaders>>& readers,
    const RelationalJointFamilyOrigins& origins,const inference::RelationalAvailabilityContext& context,std::size_t maximum,std::function<void()> check={}) {
    const auto pin=family;const auto actual=members;const auto frozen=readers;const auto keys=origins;const auto authority=context;
    return available_joint_detail::content(pin,actual,frozen,keys,authority,maximum,check).packet;
}
class AvailableRelationalLiteralJointResult;
class AvailableRelationalLiteralJointEvaluation;
// Availability is an external obligation on the actual complete source owners.
// Extracted raw families and fields retain their narrower mathematical contract.
class PreparedAvailableRelationalLiteralFieldFamily {
    struct State {
        PreparedRelationalFieldFamily family;std::vector<PreparedAvailableRelationalEvidence> members;
        std::vector<std::vector<RelationalJointObservationReaders>> readers;RelationalJointFamilyOrigins keys;
        storage::PreparedBytePacket content;std::vector<available_joint_detail::Origin> required;available_joint_detail::Origin origin;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedAvailableRelationalLiteralFieldFamily(PreparedRelationalFieldFamily family,std::vector<PreparedAvailableRelationalEvidence> members,
        std::vector<std::vector<RelationalJointObservationReaders>> readers,RelationalJointFamilyOrigins origins,
        const inference::RelationalAvailabilityContext& context,std::size_t maximum,std::function<void()> check={}) {
        const auto authority=context;const auto frozen=origins;
        auto payload=available_joint_detail::content(family,members,readers,frozen,authority,maximum,check);
        auto origin=authority.require_packet(frozen.family,available_joint_detail::Role::Interface,payload.packet,check);
        available_joint_detail::merge(payload.required,{origin});available_joint_detail::recheck(payload.required,authority,check);
        state_=std::make_shared<const State>(State{std::move(family),std::move(members),std::move(readers),frozen,std::move(payload.packet),std::move(payload.required),std::move(origin)});
    }
    PreparedRelationalFieldFamily family() const {return state_->family;}
    const std::vector<PreparedAvailableRelationalEvidence>& members() const & {return state_->members;}
    const std::vector<PreparedAvailableRelationalEvidence>& members() const &&=delete;
    const std::vector<std::vector<RelationalJointObservationReaders>>& training_readers() const & {return state_->readers;}
    const std::vector<std::vector<RelationalJointObservationReaders>>& training_readers() const &&=delete;
    const RelationalJointFamilyOrigins& origins() const & {return state_->keys;}
    const RelationalJointFamilyOrigins& origins() const &&=delete;
    const std::vector<available_joint_detail::Origin>& obligations() const & {return state_->required;}
    const std::vector<available_joint_detail::Origin>& obligations() const &&=delete;
    available_joint_detail::Origin origin() const {return state_->origin;}
    storage::PreparedBytePacket content() const {return state_->content;}
    bool same_owner(const PreparedAvailableRelationalLiteralFieldFamily& other) const noexcept {return state_==other.state_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=state_;const auto authority=context;available_joint_detail::recheck(pin->required,authority,check);
    }
    AvailableRelationalLiteralJointResult execute(std::size_t member,std::vector<RelationalAvailableJointInput> inputs,
        const inference::RelationalAvailabilityContext& context,std::size_t maximum,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
};
namespace available_joint_detail {
inline Packet model_packet(const PreparedAvailableRelationalLiteralFieldFamily& model,std::size_t maximum,const std::function<void()>& check) {
    const auto pin=model;Builder w(magic('M'),maximum);w.blob(pin.content());const auto origin=pin.origin();w.u64(origin.key());w.blob(origin_prefix(origin));return std::move(w).finish(check);
}
inline Packet fields_packet(const RelationalLiteralJointResult& value,std::size_t maximum,const std::function<void()>& check) {
    const auto pin=value;const auto family=pin.family();const auto program=family.members().at(pin.member()).program();Builder w(magic('H'),maximum);
    w.u64(program.declaration().nodes.size());for(const auto& n:program.declaration().nodes) {
        if(check){check();}w.write([&](auto& writer){writer.u64(n.key);relational_affine_codec_detail::field(writer,pin.field(n.key));
            if(n.kind==inference::FieldProgramNodeKind::Boundary)relational_affine_codec_detail::field(writer,pin.occurrence_field(n.key));});
    }return std::move(w).finish(check);
}
inline Packet setting_payload(const PreparedAvailableRelationalLiteralFieldFamily& model,const numeric::RationalVector& free,
    std::size_t maximum,const std::function<void()>& check) {
    Builder w(magic('S'),maximum);w.blob(model_packet(model,maximum,check));w.write([&](auto& writer){relational_affine_codec_detail::vector(writer,free);});return std::move(w).finish(check);
}
} // namespace available_joint_detail
class AvailableRelationalLiteralJointResult {
    struct State {
        PreparedAvailableRelationalLiteralFieldFamily model;std::vector<RelationalAvailableJointInput> inputs;
        RelationalLiteralJointResult raw;std::vector<available_joint_detail::Origin> required;
    };
    std::shared_ptr<const State> state_;
    friend class PreparedAvailableRelationalLiteralFieldFamily;
    explicit AvailableRelationalLiteralJointResult(State state):state_(std::make_shared<const State>(std::move(state))){}
public:
    PreparedAvailableRelationalLiteralFieldFamily model() const {return state_->model;}
    RelationalLiteralJointResult raw_result() const {return state_->raw;}
    const std::vector<RelationalAvailableJointInput>& inputs() const & {return state_->inputs;}
    const std::vector<RelationalAvailableJointInput>& inputs() const &&=delete;
    const std::vector<available_joint_detail::Origin>& obligations() const & {return state_->required;}
    const std::vector<available_joint_detail::Origin>& obligations() const &&=delete;
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=state_;const auto authority=context;available_joint_detail::recheck(pin->required,authority,check);
    }
    AvailableRelationalLiteralJointEvaluation evaluate(numeric::RationalVector free,std::uint64_t setting,std::uint64_t completion,
        const inference::RelationalAvailabilityContext& context,std::size_t maximum,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
};
inline AvailableRelationalLiteralJointResult PreparedAvailableRelationalLiteralFieldFamily::execute(std::size_t member,
    std::vector<RelationalAvailableJointInput> inputs,const inference::RelationalAvailabilityContext& context,
    std::size_t maximum,numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin=*this;const auto authority=context;pin.admit(authority,check);numeric::ExactActionGuard guard(limits,check);
    relational_evidence_detail::same_limits(pin.family().limits(),limits);guard.dimension(inputs.size());auto required=pin.obligations();std::vector<RelationalLiteralFamilyInput> native;
    const auto family=pin.family();const auto program=family.members().at(member).program();
    for(const auto& input:inputs) {
        guard.step();inference::field_program_detail::require(available_joint_detail::raw(input.input).accession().program().same_owner(program),"available joint prediction needs actual selected member program");
        available_joint_detail::merge(required,available_joint_detail::input_origins(input,authority,maximum,check));native.push_back(input.input);
    }
    available_joint_detail::recheck(required,authority,check);RelationalLiteralJointResult result(family,member,std::move(native),limits,check);
    return AvailableRelationalLiteralJointResult({pin,std::move(inputs),std::move(result),std::move(required)});
}
namespace available_joint_detail {
inline std::vector<Origin> record_origins(const RelationalAvailableJointInput& row,const std::vector<Origin>& required) {
    const auto& k=row.origins;std::vector<std::uint64_t> all{k.input.asset,k.input.value,k.input.accession,k.input.reconstruction};
    if(row.input.index()==1)all.insert(all.end(),{k.original_program.program,k.original_program.maps,k.original_program.metrics,k.original_input.asset,k.original_input.value,k.original_input.accession,k.original_input.reconstruction});
    all.push_back(k.reader);std::vector<Origin> out;
    for(const auto key:all){const auto at=std::find_if(required.begin(),required.end(),[&](const auto& origin){return origin.key()==key;});require(at!=required.end(),"missing retained joint reader origin");out.push_back(*at);}
    return out;
}
inline Packet result_packet(const AvailableRelationalLiteralJointResult& value,std::size_t maximum,const std::function<void()>& check) {
    const auto pin=value;Builder w(magic('R'),maximum);w.blob(model_packet(pin.model(),maximum,check));const auto raw_result=pin.raw_result();w.u64(raw_result.member());w.u64(pin.inputs().size());PacketContext packets(maximum,check);
    for(const auto& input:pin.inputs())w.blob(input_packet(input,record_origins(input,pin.obligations()),packets));
    w.blob(fields_packet(raw_result,maximum,check));return std::move(w).finish(check);
}
} // namespace available_joint_detail
namespace available_joint_detail {
inline void setting_shape(const PreparedAvailableRelationalLiteralFieldFamily& model,const numeric::RationalVector& free,
    numeric::ExactActionLimits limits,const std::function<void()>& check) {
    const auto family=model.family();relational_evidence_detail::same_limits(family.limits(),limits);
    require(family.gluing().consistent()&&free.size()==family.gluing().coordinate_kernel().cols(),"complete consistent joint setting required");
    numeric::ExactActionGuard guard(limits,check);(void)guard.canonical_input(free);
}
inline Packet completion_payload(const AvailableRelationalLiteralJointResult& value,const numeric::RationalVector& free,
    const Origin& setting,std::size_t maximum,const std::function<void()>& check) {
    Builder w(magic('C'),maximum);w.blob(result_packet(value,maximum,check));w.write([&](auto& writer){relational_affine_codec_detail::vector(writer,free);});
    // The setting payload reconstructs exactly from this result's complete
    // model and free values. Retain its publisher prefix without a second copy.
    w.u64(setting.key());w.blob(origin_prefix(setting));return std::move(w).finish(check);
}
}
inline storage::PreparedBytePacket prepare_relational_joint_setting_payload(const PreparedAvailableRelationalLiteralFieldFamily& model,
    const numeric::RationalVector& free,const inference::RelationalAvailabilityContext& context,std::size_t maximum,std::function<void()> check={}) {
    const auto pin=model;const auto values=free;const auto authority=context;pin.admit(authority,check);available_joint_detail::setting_shape(pin,values,pin.family().limits(),check);
    return available_joint_detail::setting_payload(pin,values,maximum,check);
}
inline storage::PreparedBytePacket prepare_relational_joint_completion_payload(const AvailableRelationalLiteralJointResult& result,
    const numeric::RationalVector& free,std::uint64_t setting,const inference::RelationalAvailabilityContext& context,
    std::size_t maximum,std::function<void()> check={}) {
    const auto pin=result;const auto values=free;const auto authority=context;pin.admit(authority,check);const auto model=pin.model();
    available_joint_detail::setting_shape(model,values,model.family().limits(),check);
    const auto origin=authority.require_packet(setting,available_joint_detail::Role::Prospective,available_joint_detail::setting_payload(model,values,maximum,check),check);
    return available_joint_detail::completion_payload(pin,values,origin,maximum,check);
}
class AvailableRelationalLiteralJointEvaluation {
    AvailableRelationalLiteralJointResult parent_;RelationalLiteralJointEvaluation raw_;
    available_joint_detail::Origin setting_,completion_;
    friend class AvailableRelationalLiteralJointResult;
    AvailableRelationalLiteralJointEvaluation(AvailableRelationalLiteralJointResult parent,RelationalLiteralJointEvaluation raw,
        available_joint_detail::Origin setting,available_joint_detail::Origin completion)
        :parent_(std::move(parent)),raw_(std::move(raw)),setting_(std::move(setting)),completion_(std::move(completion)){}
public:
    AvailableRelationalLiteralJointResult parent() const {return parent_;}
    RelationalLiteralJointEvaluation raw_evaluation() const {return raw_;}
    available_joint_detail::Origin setting() const {return setting_;}
    available_joint_detail::Origin completion() const {return completion_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.parent_.admit(authority,check);
        available_joint_detail::recheck({pin.setting_,pin.completion_},authority,check);
    }
};
inline AvailableRelationalLiteralJointEvaluation AvailableRelationalLiteralJointResult::evaluate(numeric::RationalVector free,
    std::uint64_t setting,std::uint64_t completion,const inference::RelationalAvailabilityContext& context,
    std::size_t maximum,numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin=*this;const auto authority=context;pin.admit(authority,check);const auto model=pin.model();
    available_joint_detail::setting_shape(model,free,limits,check);
    const auto selected=authority.require_packet(setting,available_joint_detail::Role::Prospective,available_joint_detail::setting_payload(model,free,maximum,check),check);
    const auto decision=authority.require_packet(completion,available_joint_detail::Role::Prospective,available_joint_detail::completion_payload(pin,free,selected,maximum,check),check);
    available_joint_detail::recheck({selected,decision},authority,check);auto result=pin.raw_result().evaluate(std::move(free),limits,check);
    return {pin,std::move(result),selected,decision};
}
} // namespace rfc::learning
