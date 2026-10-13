#pragma once

#include "rfc/learning/relational_finite_operator_codec.hpp"
#include "rfc/learning/relational_available_finite_field_codec.hpp"
#include <tuple>

namespace rfc::learning {
struct RelationalAvailableFiniteOperatorInput { std::uint64_t port{}; inference::RelationalAvailableInputSubmission input; };
struct RelationalFiniteOperatorOrigins { std::uint64_t maps{}, relationship{}; };
struct RelationalFiniteOperatorInstanceOrigins {
    std::uint64_t transport_maps{}, transport_relationship{}, maps{}, relationship{}, instance{};
};
class PreparedAvailableRelationalFiniteFieldOperator;
class AvailableRelationalFiniteOperatorInstance;
class AvailableRelationalFiniteOperatorResult;
class AvailableRelationalFiniteOperatorBranch;
class AvailableRelationalFiniteOperatorEvaluation;
namespace available_finite_operator_detail {
using namespace available_finite_detail;
inline constexpr std::array<std::byte, 8> operator_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Z'},std::byte{'O'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> instance_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Z'},std::byte{'X'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> family_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Z'},std::byte{'Y'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> evaluation_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'Z'},std::byte{'Z'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> lift_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'O'},std::byte{'L'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> transport_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'O'},std::byte{'T'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> lineage_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'O'},std::byte{'N'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte, 8> completion_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'O'},std::byte{'V'},std::byte{1},std::byte{0},std::byte{0}};
inline Bytes operator_bytes(const PreparedAvailableRelationalFiniteFieldOperator&);
inline Bytes instance_bytes(const AvailableRelationalFiniteOperatorInstance&);
inline Bytes family_bytes(const AvailableRelationalFiniteOperatorResult&);
inline Bytes operator_payload(const Bytes& reference, const RelationalFiniteOperatorDeclaration& d, Status status) {
    (void)relationship_role(status); auto w=envelope(lift_magic); blob(w,reference);
    finite_operator_codec_detail::operator_declaration(w,d); w.u8(static_cast<std::uint8_t>(status)); return finish(std::move(w));
}
inline Bytes transport_payload(const Bytes& op, const Bytes& source, const RelationalFiniteSourceTransportDeclaration& d, Status status) {
    (void)relationship_role(status); auto w=envelope(transport_magic); blob(w,op); blob(w,source);
    finite_operator_codec_detail::instance_declaration(w,{0,0,d}); w.u8(static_cast<std::uint8_t>(status)); return finish(std::move(w));
}
inline void instance_keys(Writer& w, RelationalFiniteOperatorInstanceOrigins keys) {
    for (const auto key : {keys.transport_maps,keys.transport_relationship,keys.maps,keys.relationship,keys.instance}) w.u64(key);
}
inline RelationalFiniteOperatorInstanceOrigins instance_keys(Reader& r) { return {r.u64(),r.u64(),r.u64(),r.u64(),r.u64()}; }
struct PassivePort { std::uint64_t port{}; PassiveInput input; };
inline void ports(Writer& w,const std::vector<PassivePort>& rows) {
    w.u64(rows.size()); for(const auto& row:rows) {
        w.u64(row.port); blob(w,row.input.asset); accession(w,row.input.declaration); w.u64(row.input.use);
        storage::temporal_codec_detail::tensor(w,row.input.field);
        for(const auto key:{row.input.keys.asset,row.input.keys.value,row.input.keys.accession,row.input.keys.reconstruction}) w.u64(key);
    }
}
inline std::vector<PassivePort> ports(Reader& r) {
    std::vector<PassivePort> out; const auto count=r.count(118);
    for(std::size_t i=0;i<count;++i) {
        const auto key=r.u64(); const auto asset=r.blob(); auto d=accession(r); const auto use=r.u64(); auto field=storage::temporal_codec_detail::tensor(r);
        const inference::RelationalInputOrigins keys{r.u64(),r.u64(),r.u64(),r.u64()};
        out.push_back({key,{Bytes(asset.begin(),asset.end()),std::move(d),use,std::move(field),keys}});
    } return out;
}
inline std::vector<PassivePort> passive(const std::vector<RelationalAvailableFiniteOperatorInput>& rows) {
    std::vector<PassivePort> out; for(const auto& row:rows) {const auto& input=row.input.input;
        out.push_back({row.port,{inference::encode_relational_input_use_asset(input.asset),input.declaration,input.use,input.source_field,row.input.origins}});
    } return out;
}
template<class Row> inline std::vector<Row> ordered(const RelationalFiniteOperatorDeclaration& d, const std::vector<Row>& rows,
    numeric::ExactActionGuard& guard) {
    using inference::field_program_detail::require; require(rows.size()==d.ports.size(),"available operator requires every port"); guard.entries(rows.size());
    std::map<std::uint64_t,std::size_t> positions; for(std::size_t i=0;i<rows.size();++i) {guard.step();require(rows[i].port&&positions.emplace(rows[i].port,i).second,"available operator duplicate port");}
    std::vector<Row> out; std::set<std::uint64_t> declared;
    for(const auto& port:d.ports) {guard.step();require(port.key&&declared.insert(port.key).second&&positions.contains(port.key),"available operator missing or duplicate declared port");out.push_back(rows[positions.at(port.key)]);}
    return out;
}
template<class Declaration,class Row> inline Declaration child_declaration(Declaration base,const RelationalFiniteOperatorDeclaration& op,
    const RelationalFiniteOperatorInstanceDeclaration& d,const std::vector<Row>& canonical,numeric::ExactActionGuard& guard) {
    using inference::field_program_detail::require; require(d.key&&d.revision,"available operator instance identity/revision");base.key=d.key;base.revision=d.revision;
    std::set<std::pair<std::uint64_t,std::size_t>> used;
    for(std::size_t i=0;i<op.ports.size();++i) {
        guard.step();const auto& port=op.ports[i];auto rule=std::find_if(base.rules.begin(),base.rules.end(),[&](const auto& row){return row.key==port.rule;});
        require(rule!=base.rules.end()&&port.input<rule->inputs.size()&&used.emplace(port.rule,port.input).second,"available operator port loses reference input");
        rule->inputs[port.input]=canonical.at(i).input;
    }
    std::size_t count{};for(const auto& rule:base.rules) {guard.step();count=inference::field_program_detail::add(count,rule.inputs.size());}
    require(count==used.size(),"available operator omits reference input");return base;
}
inline Bytes lineage_payload(const Bytes& op,const Bytes& source,const RelationalFiniteOperatorInstanceDeclaration& d,
    const std::vector<PassivePort>& rows,Status status,RelationalFiniteOperatorInstanceOrigins keys,const std::vector<Origin>& own) {
    using inference::field_program_detail::require; require(keys.instance&&own.size()==4,"positive instance lineage and four antecedent receipts required");
    (void)relationship_role(status);auto w=envelope(lineage_magic);blob(w,op);blob(w,source);finite_operator_codec_detail::instance_declaration(w,d);
    ports(w,rows);w.u8(static_cast<std::uint8_t>(status));instance_keys(w,keys);for(const auto& origin:own) blob(w,origin.receipt());return finish(std::move(w));
}
} // namespace available_finite_operator_detail
class PreparedAvailableRelationalFiniteFieldOperator {
    struct State {
        PreparedAvailableRelationalFiniteFieldMap reference; RelationalFiniteOperatorDeclaration declaration;
        RelationalFiniteRelationshipStatus status; RelationalFiniteOperatorOrigins keys;
        std::vector<available_finite_detail::Origin> obligations; PreparedRelationalFiniteFieldOperator native;
    };
public:
    PreparedAvailableRelationalFiniteFieldOperator(PreparedAvailableRelationalFiniteFieldMap reference,RelationalFiniteOperatorDeclaration declaration,
        RelationalFiniteRelationshipStatus status,RelationalFiniteOperatorOrigins origins,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={});
    PreparedAvailableRelationalFiniteFieldMap reference() const {return state_->reference;}
    const RelationalFiniteOperatorDeclaration& declaration() const & {return state_->declaration;}
    const RelationalFiniteOperatorDeclaration& declaration() const && = delete;
    RelationalFiniteRelationshipStatus status() const noexcept {return state_->status;}
    RelationalFiniteOperatorOrigins origins() const noexcept {return state_->keys;}
    const std::vector<available_finite_detail::Origin>& obligations() const & {return state_->obligations;}
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    PreparedRelationalFiniteFieldOperator operator_program() const {return state_->native;}
    bool same_owner(const PreparedAvailableRelationalFiniteFieldOperator& other) const noexcept {return state_==other.state_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=state_;const auto authority=context;available_finite_detail::recheck(pin->obligations,authority,check);
    }
    std::vector<RelationalFiniteOperatorPort> select_ports(std::size_t begin,std::size_t count,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.admit(authority,check);return pin.operator_program().select_ports(begin,count,limits,check);
    }
    AvailableRelationalFiniteOperatorInstance instantiate(PreparedAvailableRelationalFiniteSource source,RelationalFiniteOperatorInstanceDeclaration declaration,
        std::vector<RelationalAvailableFiniteOperatorInput> inputs,RelationalFiniteRelationshipStatus status,RelationalFiniteOperatorInstanceOrigins origins,
        const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
private:
    std::shared_ptr<const State> state_;
};
namespace available_finite_operator_detail {
inline Bytes operator_bytes(const PreparedAvailableRelationalFiniteFieldOperator& op) {
    const auto pin=op;auto w=envelope(operator_magic);blob(w,model_bytes(pin.reference()));finite_operator_codec_detail::operator_declaration(w,pin.declaration());
    w.u8(static_cast<std::uint8_t>(pin.status()));const auto keys=pin.origins();w.u64(keys.maps);w.u64(keys.relationship);
    w.u64(pin.obligations().size());for(const auto& origin:pin.obligations())blob(w,origin.receipt());blob(w,encode_relational_finite_operator(pin.operator_program()));return finish(std::move(w));
}
struct PreflightInstance {
    std::vector<RelationalAvailableFiniteOperatorInput> rows; RelationalAvailableFiniteFieldMapDeclaration child;
    std::vector<Origin> required,own;
};
inline PreflightInstance antecedents(const PreparedAvailableRelationalFiniteFieldOperator& op,const PreparedAvailableRelationalFiniteSource& source,
    const RelationalFiniteOperatorInstanceDeclaration& d,const std::vector<RelationalAvailableFiniteOperatorInput>& rows,
    const Context& context,numeric::ExactActionLimits limits,const std::function<void()>& check) {
    op.admit(context,check);source.admit(context,check);numeric::ExactActionGuard guard(limits,check);
    auto canonical=ordered(op.declaration(),rows,guard);const auto parent=op.reference();auto child=child_declaration(parent.declaration(),op.declaration(),d,canonical,guard);
    const auto finite=source.finite();const auto asset=finite.asset();for(const auto& row:canonical)
        inference::field_program_detail::require(row.input.input.asset.same_owner(asset),"available operator port has foreign actual source owner");
    std::vector<Bytes> programs;for(const auto& target:parent.targets())programs.push_back(inference::encode_relational_field_program(program(target).program()));
    auto required=op.obligations();required.insert(required.end(),source.obligations().begin(),source.obligations().end());
    auto input_required=input_origins(available_finite_detail::passive(child),inference::encode_relational_input_use_asset(asset),programs,context,check);
    required.insert(required.end(),input_required.begin(),input_required.end());return {std::move(canonical),std::move(child),std::move(required),{}};
}
inline PreflightInstance preflight(const PreparedAvailableRelationalFiniteFieldOperator& op,const PreparedAvailableRelationalFiniteSource& source,
    const RelationalFiniteOperatorInstanceDeclaration& d,const std::vector<RelationalAvailableFiniteOperatorInput>& rows,Status status,
    RelationalFiniteOperatorInstanceOrigins keys,const Context& context,numeric::ExactActionLimits limits,const std::function<void()>& check,bool instance) {
    auto out=antecedents(op,source,d,rows,context,limits,check);
    const auto transport=transport_payload(operator_bytes(op),source_bytes(source),d.transport,status);
    const auto parent=op.reference();const auto child=live_payload(source,parent.targets(),out.child,status,context,check);
    for(const auto& spec:std::array<std::tuple<std::uint64_t,Role,const Bytes*>,4>{{
        {keys.transport_maps,Role::Maps,&transport},{keys.transport_relationship,relationship_role(status),&transport},
        {keys.maps,Role::Maps,&child},{keys.relationship,relationship_role(status),&child}}}) {
        if(check){check();}out.own.push_back(context.require(std::get<0>(spec),std::get<1>(spec),*std::get<2>(spec)));
    }
    const auto payload=lineage_payload(operator_bytes(op),source_bytes(source),d,passive(out.rows),status,keys,out.own);
    out.required.insert(out.required.end(),out.own.begin(),out.own.end());
    if(instance) {if(check){check();}out.required.push_back(context.require(keys.instance,Role::Prospective,payload));}return out;
}
} // namespace available_finite_operator_detail
inline available_finite_detail::Bytes relational_finite_operator_payload(const PreparedAvailableRelationalFiniteFieldMap& reference,
    const RelationalFiniteOperatorDeclaration& declaration,RelationalFiniteRelationshipStatus status,const inference::RelationalAvailabilityContext& context) {
    const auto pin=reference;const auto d=declaration;const auto authority=context;pin.admit(authority);
    return available_finite_operator_detail::operator_payload(available_finite_detail::model_bytes(pin),d,status);
}
inline PreparedAvailableRelationalFiniteFieldOperator::PreparedAvailableRelationalFiniteFieldOperator(PreparedAvailableRelationalFiniteFieldMap reference,
    RelationalFiniteOperatorDeclaration declaration,RelationalFiniteRelationshipStatus status,RelationalFiniteOperatorOrigins origins,
    const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) {
    using namespace available_finite_operator_detail;const auto pin=reference;const auto d=declaration;const auto authority=context;pin.admit(authority,check);
    auto required=pin.obligations();const auto payload=operator_payload(model_bytes(pin),d,status);
    required.push_back(authority.require(origins.maps,Role::Maps,payload));required.push_back(authority.require(origins.relationship,relationship_role(status),payload));
    PreparedRelationalFiniteFieldOperator native(pin.map(),d,limits,check);state_=std::make_shared<const State>(State{pin,d,status,origins,std::move(required),std::move(native)});
}
inline inference::RelationalFiniteSourceDeclaration relational_finite_operator_source_declaration(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const inference::OwnedRelationalInputUseAsset& asset,const RelationalFiniteSourceTransportDeclaration& declaration,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    const auto pin=op;const auto input=asset;const auto d=declaration;return relational_finite_operator_source_declaration(pin.operator_program(),input,d,limits,check);
}
inline inference::RelationalInputAccessionDeclaration relational_finite_operator_input_schema(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const inference::OwnedRelationalInputUseAsset& asset,const RelationalFiniteSourceTransportDeclaration& declaration,std::uint64_t port,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    const auto pin=op;const auto input=asset;const auto d=declaration;return relational_finite_operator_input_schema(pin.operator_program(),input,d,port,limits,check);
}
inline available_finite_detail::Bytes relational_finite_operator_transport_payload(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const PreparedAvailableRelationalFiniteSource& source,const RelationalFiniteSourceTransportDeclaration& declaration,
    RelationalFiniteRelationshipStatus status,const inference::RelationalAvailabilityContext& context) {
    const auto pin=op;const auto input=source;const auto d=declaration;const auto authority=context;pin.admit(authority);input.admit(authority);
    return available_finite_operator_detail::transport_payload(available_finite_operator_detail::operator_bytes(pin),available_finite_detail::source_bytes(input),d,status);
}
inline RelationalAvailableFiniteFieldMapDeclaration relational_finite_operator_child_map_declaration(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const RelationalFiniteOperatorInstanceDeclaration& declaration,const std::vector<RelationalAvailableFiniteOperatorInput>& rows,
    numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    const auto pin=op;const auto d=declaration;const auto frozen=rows;numeric::ExactActionGuard guard(limits,check);const auto parent=pin.reference();
    const auto canonical=available_finite_operator_detail::ordered(pin.declaration(),frozen,guard);
    return available_finite_operator_detail::child_declaration(parent.declaration(),pin.declaration(),d,canonical,guard);
}
inline available_finite_detail::Bytes relational_finite_operator_child_map_payload(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const PreparedAvailableRelationalFiniteSource& source,const RelationalFiniteOperatorInstanceDeclaration& declaration,
    const std::vector<RelationalAvailableFiniteOperatorInput>& rows,RelationalFiniteRelationshipStatus status,const inference::RelationalAvailabilityContext& context) {
    const auto pin=op;const auto input=source;const auto d=declaration;const auto frozen=rows;const auto authority=context;
    const auto parsed=available_finite_operator_detail::antecedents(pin,input,d,frozen,authority,{},{});const auto parent=pin.reference();
    return available_finite_detail::live_payload(input,parent.targets(),parsed.child,status,authority,{});
}
inline available_finite_detail::Bytes relational_finite_operator_instance_payload(const PreparedAvailableRelationalFiniteFieldOperator& op,
    const PreparedAvailableRelationalFiniteSource& source,const RelationalFiniteOperatorInstanceDeclaration& declaration,
    const std::vector<RelationalAvailableFiniteOperatorInput>& rows,RelationalFiniteRelationshipStatus status,RelationalFiniteOperatorInstanceOrigins origins,
    const inference::RelationalAvailabilityContext& context) {
    const auto pin=op;const auto input=source;const auto d=declaration;const auto frozen=rows;const auto authority=context;
    const auto parsed=available_finite_operator_detail::preflight(pin,input,d,frozen,status,origins,authority,{},{},false);
    return available_finite_operator_detail::lineage_payload(available_finite_operator_detail::operator_bytes(pin),available_finite_detail::source_bytes(input),d,
        available_finite_operator_detail::passive(parsed.rows),status,origins,parsed.own);
}
class AvailableRelationalFiniteOperatorInstance {
    struct State {
        PreparedAvailableRelationalFiniteFieldOperator op;PreparedAvailableRelationalFiniteSource source;
        RelationalFiniteOperatorInstanceDeclaration declaration;std::vector<RelationalAvailableFiniteOperatorInput> inputs;
        RelationalFiniteRelationshipStatus status;RelationalFiniteOperatorInstanceOrigins keys;std::vector<available_finite_detail::Origin> obligations;
        RelationalFiniteOperatorInstance native;PreparedAvailableRelationalFiniteFieldMap child;
    };
public:
    PreparedAvailableRelationalFiniteFieldOperator operator_program() const {return state_->op;}
    PreparedAvailableRelationalFiniteSource source() const {return state_->source;}
    OwnedRelationalFiniteSourceTransport source_transport() const {return state_->native.source_transport();}
    const RelationalFiniteOperatorInstanceDeclaration& declaration() const & {return state_->declaration;}
    const RelationalFiniteOperatorInstanceDeclaration& declaration() const && = delete;
    const std::vector<RelationalAvailableFiniteOperatorInput>& inputs() const & {return state_->inputs;}
    const std::vector<RelationalAvailableFiniteOperatorInput>& inputs() const && = delete;
    RelationalFiniteRelationshipStatus status() const noexcept {return state_->status;}
    RelationalFiniteOperatorInstanceOrigins origins() const noexcept {return state_->keys;}
    const std::vector<available_finite_detail::Origin>& obligations() const & {return state_->obligations;}
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    RelationalFiniteOperatorInstance raw() const {return state_->native;}
    PreparedAvailableRelationalFiniteFieldMap field_map() const {return state_->child;}
    bool same_owner(const AvailableRelationalFiniteOperatorInstance& other) const noexcept {return state_==other.state_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=state_;const auto authority=context;available_finite_detail::recheck(pin->obligations,authority,check);
    }
    std::vector<RelationalFiniteVariableCorrespondence> variable_positions(std::size_t begin,std::size_t count,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.admit(authority,check);return pin.source_transport().variable_positions(begin,count,limits,check);
    }
    std::vector<RelationalFiniteUseCorrespondence> use_correspondences(std::size_t begin,std::size_t count,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.admit(authority,check);return pin.source_transport().use_correspondences(begin,count,limits,check);
    }
    AvailableRelationalFiniteOperatorResult execute(const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
private:
    friend class PreparedAvailableRelationalFiniteFieldOperator;
    explicit AvailableRelationalFiniteOperatorInstance(State state):state_(std::make_shared<const State>(std::move(state))){}
    std::shared_ptr<const State> state_;
};
inline AvailableRelationalFiniteOperatorInstance PreparedAvailableRelationalFiniteFieldOperator::instantiate(PreparedAvailableRelationalFiniteSource source,
    RelationalFiniteOperatorInstanceDeclaration declaration,std::vector<RelationalAvailableFiniteOperatorInput> inputs,RelationalFiniteRelationshipStatus status,
    RelationalFiniteOperatorInstanceOrigins origins,const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) const {
    using namespace available_finite_operator_detail;const auto pin=*this;const auto input=source;const auto d=declaration;const auto frozen=inputs;const auto authority=context;
    auto parsed=preflight(pin,input,d,frozen,status,origins,authority,limits,check,true);std::vector<RelationalFiniteOperatorInput> raw;
    for(const auto& row:parsed.rows) raw.push_back({row.port,row.input.input});
    auto native=pin.operator_program().instantiate(input.finite(),d,std::move(raw),limits,check);const auto parent=pin.reference();
    PreparedAvailableRelationalFiniteFieldMap child(input,parent.targets(),parsed.child,status,{origins.maps,origins.relationship},authority,limits,check);
    canonical(encode_relational_finite_field_map(native.map()),encode_relational_finite_field_map(child.map()));if(check){check();}
    return AvailableRelationalFiniteOperatorInstance({pin,input,d,std::move(parsed.rows),status,origins,std::move(parsed.required),std::move(native),std::move(child)});
}
class AvailableRelationalFiniteOperatorResult {
public:
    AvailableRelationalFiniteOperatorInstance instance() const {return instance_;}
    RelationalFiniteOperatorResult result() const {return native_;}
    std::size_t size() const noexcept {return native_.size();}
    const std::vector<available_finite_detail::Origin>& obligations() const & {return instance_.obligations();}
    const std::vector<available_finite_detail::Origin>& obligations() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {const auto pin=*this;pin.instance_.admit(context,std::move(check));}
    std::vector<AvailableRelationalFiniteOperatorBranch> select(std::size_t begin,std::size_t count,const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
    AvailableRelationalFiniteOperatorEvaluation evaluate(std::size_t ordinal,numeric::RationalVector free,std::uint64_t parameter_setting,std::uint64_t choice_origin,
        const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}) const;
private:
    friend class AvailableRelationalFiniteOperatorInstance;
    AvailableRelationalFiniteOperatorResult(AvailableRelationalFiniteOperatorInstance instance,RelationalFiniteOperatorResult native):instance_(std::move(instance)),native_(std::move(native)){}
    AvailableRelationalFiniteOperatorInstance instance_;RelationalFiniteOperatorResult native_;
};
class AvailableRelationalFiniteOperatorBranch {
public:
    AvailableRelationalFiniteOperatorResult parent() const {return parent_;}
    std::size_t ordinal() const noexcept {return native_.ordinal();}
    const RelationalFiniteFieldBranch& branch() const & {return native_.branch();}
    const RelationalFiniteFieldBranch& branch() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {parent_.admit(context,std::move(check));}
private:
    friend class AvailableRelationalFiniteOperatorResult;
    AvailableRelationalFiniteOperatorBranch(AvailableRelationalFiniteOperatorResult parent,RelationalFiniteOperatorBranch native):parent_(std::move(parent)),native_(std::move(native)){}
    AvailableRelationalFiniteOperatorResult parent_;RelationalFiniteOperatorBranch native_;
};
inline AvailableRelationalFiniteOperatorResult AvailableRelationalFiniteOperatorInstance::execute(const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin=*this;const auto authority=context;pin.admit(authority,check);return {pin,pin.raw().execute(limits,check)};
}
inline std::vector<AvailableRelationalFiniteOperatorBranch> AvailableRelationalFiniteOperatorResult::select(std::size_t begin,std::size_t count,
    const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin=*this;const auto authority=context;pin.admit(authority,check);const auto rows=pin.native_.select(begin,count,limits,check);
    std::vector<AvailableRelationalFiniteOperatorBranch> out;for(const auto& row:rows)out.push_back(AvailableRelationalFiniteOperatorBranch(pin,row));return out;
}
namespace available_finite_operator_detail {
inline Bytes instance_bytes(const AvailableRelationalFiniteOperatorInstance& instance) {
    const auto pin=instance;auto w=envelope(instance_magic);blob(w,operator_bytes(pin.operator_program()));blob(w,source_bytes(pin.source()));
    finite_operator_codec_detail::instance_declaration(w,pin.declaration());ports(w,passive(pin.inputs()));w.u8(static_cast<std::uint8_t>(pin.status()));instance_keys(w,pin.origins());
    w.u64(pin.obligations().size());for(const auto& origin:pin.obligations())blob(w,origin.receipt());blob(w,model_bytes(pin.field_map()));
    blob(w,encode_relational_finite_operator_instance(pin.raw()));return finish(std::move(w));
}
inline Bytes family_bytes(const AvailableRelationalFiniteOperatorResult& family) {
    const auto pin=family;auto w=envelope(family_magic);blob(w,instance_bytes(pin.instance()));blob(w,encode_relational_finite_operator_result(pin.result()));return finish(std::move(w));
}
inline std::optional<Origin> setting(const AvailableRelationalFiniteOperatorResult& family,std::size_t ordinal,const numeric::RationalVector& free,std::uint64_t key,const Context& context) {
    using inference::field_program_detail::require;const auto raw=family.result().result();const auto& branch=raw.branch(ordinal);const auto child=family.instance().field_map();
    require(branch.status==RelationalFiniteFieldStatus::Interpreted&&branch.rule&&branch.value,"operator choice needs interpreted branch");
    const auto& rules=child.declaration().rules;const auto rule=std::find_if(rules.begin(),rules.end(),[&](const auto& row){return row.key==*branch.rule;});
    require(rule!=rules.end()&&rule->target,"operator choice loses target");const auto& target=child.targets().at(*rule->target);
    if(const auto* evidence=std::get_if<PreparedAvailableRelationalEvidence>(&target)) {
        const auto native=evidence->evidence();require(free.size()==native.kernel().cols(),"operator choice needs complete common axes");
        return context.require(key,Role::Prospective,relational_parameter_setting_payload(*evidence,free));
    }require(!key&&free.empty(),"fixed operator choice has no parameter setting");return {};
}
inline Bytes completion_payload(const Bytes& family,std::size_t ordinal,const numeric::RationalVector& free,const std::optional<Origin>& setting) {
    auto w=envelope(completion_magic);blob(w,family);w.u64(ordinal);relational_affine_codec_detail::vector(w,free);w.u8(setting?1:0);
    if(setting){blob(w,setting->receipt());}return finish(std::move(w));
}
} // namespace available_finite_operator_detail
inline available_finite_detail::Bytes relational_finite_operator_completion_payload(const AvailableRelationalFiniteOperatorResult& family,std::size_t ordinal,
    const numeric::RationalVector& free,std::uint64_t parameter_setting,const inference::RelationalAvailabilityContext& context) {
    const auto pin=family;const auto frozen=free;const auto authority=context;pin.admit(authority);
    return available_finite_operator_detail::completion_payload(available_finite_operator_detail::family_bytes(pin),ordinal,frozen,
        available_finite_operator_detail::setting(pin,ordinal,frozen,parameter_setting,authority));
}
class AvailableRelationalFiniteOperatorEvaluation {
public:
    AvailableRelationalFiniteOperatorResult family() const {return family_;}
    RelationalFiniteOperatorEvaluation result() const {return native_;}
    std::size_t ordinal() const noexcept {return native_.ordinal();}
    std::optional<available_finite_detail::Origin> parameter_setting() const {return setting_;}
    available_finite_detail::Origin origin() const {return origin_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.family_.admit(authority,check);
        if(pin.setting_){if(check){check();}authority.recheck(*pin.setting_);}if(check){check();}authority.recheck(pin.origin_);
    }
private:
    friend class AvailableRelationalFiniteOperatorResult;
    AvailableRelationalFiniteOperatorEvaluation(AvailableRelationalFiniteOperatorResult family,RelationalFiniteOperatorEvaluation native,
        std::optional<available_finite_detail::Origin> setting,available_finite_detail::Origin origin)
        :family_(std::move(family)),native_(std::move(native)),setting_(std::move(setting)),origin_(std::move(origin)){}
    AvailableRelationalFiniteOperatorResult family_;RelationalFiniteOperatorEvaluation native_;
    std::optional<available_finite_detail::Origin> setting_;available_finite_detail::Origin origin_;
};
inline AvailableRelationalFiniteOperatorEvaluation AvailableRelationalFiniteOperatorResult::evaluate(std::size_t ordinal,numeric::RationalVector free,
    std::uint64_t parameter_setting,std::uint64_t choice_origin,const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin=*this;const auto frozen=free;const auto authority=context;pin.admit(authority,check);
    const auto setting=available_finite_operator_detail::setting(pin,ordinal,frozen,parameter_setting,authority);
    const auto payload=available_finite_operator_detail::completion_payload(available_finite_operator_detail::family_bytes(pin),ordinal,frozen,setting);
    const auto origin=authority.require(choice_origin,available_finite_detail::Role::Prospective,payload);if(check){check();}
    return {pin,pin.native_.evaluate(ordinal,frozen,limits,check),setting,origin};
}
} // namespace rfc::learning
