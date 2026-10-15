#pragma once
#include "rfc/inference/native_field_updates.hpp"
#include "rfc/temporal/transfer.hpp"

namespace rfc::inference {

// Complete ephemeral comparison, bound to both actual native publications.
// Persisted single-endpoint tensor roles do not acquire foreign carrier keys.
struct NativeGreenCorrespondence {
    storage::PinnedNativeAccession old_publication, new_publication;
    std::uint64_t old_carrier{}, new_carrier{};
    numeric::SparseRationalTensor map;
};
struct NativeGreenAvailability {
    runtime::WorkAvailability old_endpoint, new_endpoint;
};
struct NativeGreenResponseRequest {
    storage::PinnedNativeAccession old_operator, new_operator, old_injection, new_injection;
    NativeGreenCorrespondence ambient, source;
    numeric::Rational lambda{1};
    std::size_t dimension_limit{64};
};
struct NativeGreenEndpointWitness {
    std::uint64_t source_namespace{}, sequence{}, ambient_carrier{}, source_carrier{};
    std::uint64_t resolving_operator{}, injection{}, input_field{};
    std::optional<std::uint64_t> output_carrier, accession;
};
struct NativeGreenProvenance {
    std::string profile{"source-grade-native-green-interval-v1"};
    NativeGreenEndpointWitness old_endpoint, new_endpoint;
    numeric::Rational lambda;
    // Operand bit minima and scoped containers; factor/intermediate growth
    // remains subject to action_limits, including zero/empty observations.
    runtime::ExecutionRequirements selected_requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t green_preparation_steps{}, preparation_steps{}, execution_steps{};
    std::string green_preparation_policy, preparation_policy, execution_policy;
    NativeGreenAvailability green_preparation_availability, preparation_availability, execution_availability;
    bool observed{}, reused_green{}, fallback{};
};

namespace native_green_detail {
using namespace native_update_detail;
using Carrier=std::shared_ptr<const storage::TemporalCarrier>;
inline void availability(const Pin& old_pin,const Pin& new_pin,const NativeGreenAvailability& value) {
    runtime::admit_work_availability(old_pin.source_namespace(),old_pin.sequence(),value.old_endpoint);
    runtime::admit_work_availability(new_pin.source_namespace(),new_pin.sequence(),value.new_endpoint);
}
inline void linear(const Carrier& carrier,Guard& guard) {
    require(carrier->domain.extents().size()==1,"native Green carrier requires declared linear coordinates");
    guard.dimension(carrier->domain.logical_size());
}
inline Carrier carrier(const Pin& pin,std::uint64_t key,Guard& guard) {
    const auto endpoint=pin.endpoint();
    for(const auto& value:endpoint->carriers) {
        guard.step();if(value.key==key){Carrier result{endpoint,&value};linear(result,guard);return result;}
    }
    throw std::invalid_argument("native Green declared carrier absent");
}
inline Matrix materialize(const numeric::SparseRationalTensor& tensor,Guard& guard) {
    const auto& shape=tensor.domain().extents();
    require(shape.size()==2 && tensor.support().count()==tensor.domain().logical_size(),
            "native Green correspondence requires complete map support");
    guard.dimension(shape[0]);guard.dimension(shape[1]);guard.entries(tensor.values().nonzeros());
    Matrix result(shape[0],shape[1]);
    for(const auto& e:tensor.values().entries()){
        guard.step();guard.value(e.value);result.set(e.index/shape[1],e.index%shape[1],e.value);
    }
    return result;
}
inline void map_requirements(runtime::ExecutionRequirements& result,const numeric::SparseRationalTensor& tensor) {
    result.rational_slots=add(result.rational_slots,tensor.values().nonzeros());
    for(const auto& e:tensor.values().entries())
        result.integer_bits=std::max(result.integer_bits,native_field_detail::integer_bits(e.value));
    result.sparse_entries=result.rational_slots;
}
inline std::array<Pin,8> pins(const NativeGreenResponseRequest& r) {
    return {r.old_operator,r.new_operator,r.old_injection,r.new_injection,
        r.ambient.old_publication,r.ambient.new_publication,r.source.old_publication,r.source.new_publication};
}
inline runtime::ExecutionRequirements payload_requirements(const NativeGreenResponseRequest& r) {
    const std::array<Pin,4> maps{r.old_operator,r.new_operator,r.old_injection,r.new_injection};
    auto result=selected(maps,6); // Four carrier references and two comparisons.
    map_requirements(result,r.ambient.map);map_requirements(result,r.source.map);
    result.rational_slots=add(result.rational_slots,1);result.sparse_entries=result.rational_slots;
    result.integer_bits=std::max(result.integer_bits,native_field_detail::integer_bits(r.lambda));return result;
}
inline Matrix comparison(const NativeGreenCorrespondence& value,const Pin& old_pin,const Pin& new_pin,
                         const Carrier& old_carrier,const Carrier& new_carrier,Guard& guard) {
    same(value.old_publication,old_pin);same(value.new_publication,new_pin);
    require(value.old_carrier==old_carrier->key && value.new_carrier==new_carrier->key,
            "native Green correspondence binds different declared carriers");
    const auto& shape=value.map.domain().extents();
    require(shape.size()==2 && shape[0]==new_carrier->domain.logical_size() && shape[1]==old_carrier->domain.logical_size(),
            "native Green correspondence dimensions disagree with its actual carriers");
    return materialize(value.map,guard);
}
struct Bindings {Carrier old_ambient,new_ambient,old_source,new_source;};
inline Bindings bind(const NativeGreenResponseRequest& r,Guard& guard) {
    require(r.dimension_limit>0,"native Green dimension limit must be positive");
    same(r.old_operator,r.old_injection);same(r.new_operator,r.new_injection);
    for(const auto* pin:{&r.old_operator,&r.new_operator,&r.old_injection,&r.new_injection})complete_map(*pin);
    const auto old_L=r.old_operator.tensor_declaration(),new_L=r.new_operator.tensor_declaration(),
               old_I=r.old_injection.tensor_declaration(),new_I=r.new_injection.tensor_declaration();
    const auto old_X=r.old_operator.carrier(),new_X=r.new_operator.carrier();
    require(old_L->input==old_L->output && new_L->input==new_L->output &&
            old_X->kind==storage::TemporalCarrierKind::SourceGrade && new_X->kind==storage::TemporalCarrierKind::SourceGrade,
            "native Green resolving operators require declared SourceGrade endomorphisms");
    linear(old_X,guard);linear(new_X,guard);
    require(old_I->role==storage::TemporalTensorRole::SourceRealization && new_I->role==storage::TemporalTensorRole::SourceRealization &&
            old_I->output==old_L->output && new_I->output==new_L->output,
            "native Green injections require source realizations on their actual ambient carriers");
    return {old_X,new_X,carrier(r.old_injection,*old_I->input,guard),carrier(r.new_injection,*new_I->input,guard)};
}
inline void field(const Pin& input,const Pin& publication,const Carrier& source) {
    same(input,publication);require(input.kind()==storage::NativeAccessionKind::Field,"native Green input requires a stored Field");
    const auto tensor=input.tensor_declaration();
    require(tensor->role==storage::TemporalTensorRole::Field && input.carrier().get()==source.get() &&
            tensor->tensor.domain()==source->domain && tensor->tensor.support().count()==source->domain.logical_size(),
            "native Green field requires the actual source-coordinate carrier and complete support");
}
inline NativeGreenEndpointWitness witness(const Pin& operation,const Pin& injection,const Carrier& source) {
    return {operation.source_namespace(),operation.sequence(),operation.carrier()->key,source->key,
            operation.tensor_declaration()->key,injection.tensor_declaration()->key,0,{},{}};
}
} // namespace native_green_detail

class PreparedNativeGreenResponseDelta;
class PreparedNativeObservedGreenResponseDelta;
template<class Value,class Owner> class DerivedNativeGreenResult {
public:
    DerivedNativeGreenResult(const DerivedNativeGreenResult&)=default;
    DerivedNativeGreenResult& operator=(const DerivedNativeGreenResult&)=default;
    const Value& values() const & {return values_;}
    Value values() const && {return values_;}
    const NativeGreenProvenance& provenance() const & {return report_;}
    NativeGreenProvenance provenance() const && {return report_;}
    const Owner& plan() const & {return *owner_;}
    const Owner& plan() const &&=delete;
    storage::PinnedNativeAccession old_input() const {return old_input_;}
    storage::PinnedNativeAccession new_input() const {return new_input_;}
private:
    friend class PreparedNativeGreenResponseDelta;
    friend class PreparedNativeObservedGreenResponseDelta;
    DerivedNativeGreenResult(Owner owner,storage::PinnedNativeAccession old_input,storage::PinnedNativeAccession new_input,
                             Value value,NativeGreenProvenance report)
        :owner_(std::make_shared<const Owner>(std::move(owner))),old_input_(std::move(old_input)),new_input_(std::move(new_input)),
         values_(std::move(value)),report_(std::move(report)){}
    std::shared_ptr<const Owner> owner_;
    storage::PinnedNativeAccession old_input_,new_input_;
    Value values_;
    NativeGreenProvenance report_;
};
using DerivedNativeGreenResponse=DerivedNativeGreenResult<temporal::GreenResponseDelta,PreparedNativeGreenResponseDelta>;
using DerivedNativeObservedGreenResponse=DerivedNativeGreenResult<temporal::ObservedGreenResponseDelta,PreparedNativeObservedGreenResponseDelta>;

class PreparedNativeGreenResponseDelta {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        NativeGreenResponseRequest request;
        native_green_detail::Bindings bindings;
        temporal::PreparedGreenResponseDelta solve;
        runtime::ExecutionRequirements retained;
        NativeGreenAvailability availability;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    PreparedNativeGreenResponseDelta(const PreparedNativeGreenResponseDelta&)=default;
    PreparedNativeGreenResponseDelta& operator=(const PreparedNativeGreenResponseDelta&)=default;
    static PreparedNativeGreenResponseDelta prepare(NativeGreenResponseRequest request,NativeGreenAvailability available,
                                                     runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_green_detail;
        lease.check_cancelled();availability(request.old_operator,request.new_operator,available);
        auto required=payload_requirements(request);runtime::admit_execution(required,lease.policy());
        const auto selected_pins=pins(request);
        Guard guard(limits(lease,selected_pins,request.dimension_limit,required.rational_slots),[&lease]{lease.check_cancelled();});
        guard.value(request.lambda);request.lambda=numeric::canonical_rational(request.lambda);
        auto bindings=bind(request,guard);
        const auto J=comparison(request.ambient,request.old_operator,request.new_operator,bindings.old_ambient,bindings.new_ambient,guard),
                   K=comparison(request.source,request.old_operator,request.new_operator,bindings.old_source,bindings.new_source,guard);
        auto old_L=native_update_detail::materialize(request.old_operator,guard);
        auto new_L=native_update_detail::materialize(request.new_operator,guard);
        auto old_I=native_update_detail::materialize(request.old_injection,guard);
        auto new_I=native_update_detail::materialize(request.new_injection,guard);
        temporal::PreparedGreenResponseDelta solve(std::move(old_L),std::move(new_L),J,std::move(old_I),std::move(new_I),K,request.lambda,guard);
        required.rational_slots=add(required.rational_slots,solve.stored_entries());required.sparse_entries=required.rational_slots;
        runtime::admit_execution(required,lease.policy());guard.entries(solve.stored_entries());lease.check_cancelled();
        return PreparedNativeGreenResponseDelta(std::make_shared<const State>(State{std::move(request),std::move(bindings),std::move(solve),
            required,available,lease.policy().name,guard.steps(),lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    const temporal::PreparedGreenResponseDelta& numeric_plan() const & {return state_->solve;}
    const temporal::PreparedGreenResponseDelta& numeric_plan() const &&=delete;
    Pin old_publication() const {return state_->request.old_operator;}
    Pin new_publication() const {return state_->request.new_operator;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    DerivedNativeGreenResponse execute(Pin old_input,Pin new_input,NativeGreenAvailability available,
                                        runtime::ExecutionLease lease=runtime::ExecutionLease{}) const {
        using namespace native_green_detail;
        const auto pin=state_;lease.check_cancelled();check_inputs(*pin,old_input,new_input,available);
        const std::array<Pin,10> selected_pins{pin->request.old_operator,pin->request.new_operator,pin->request.old_injection,pin->request.new_injection,
            pin->request.ambient.old_publication,pin->request.ambient.new_publication,pin->request.source.old_publication,
            pin->request.source.new_publication,old_input,new_input};
        auto payload=payload_requirements(pin->request);map_requirements(payload,*old_input.tensor());map_requirements(payload,*new_input.tensor());
        auto required=pin->retained;required.records=add(required.records,2);
        required.rational_slots=add(required.rational_slots,add(native_update_detail::payload(old_input),native_update_detail::payload(new_input)));
        required.sparse_entries=required.rational_slots;required.integer_bits=std::max(required.integer_bits,payload.integer_bits);
        runtime::admit_execution(required,lease.policy());
        const auto action_limits=limits(lease,selected_pins,pin->request.dimension_limit,payload.rational_slots);
        Guard guard(action_limits,[&lease]{lease.check_cancelled();});
        const auto old_values=field_values(old_input,guard),new_values=field_values(new_input,guard);
        auto value=pin->solve.apply(old_values,new_values,guard);
        auto report=provenance(*pin,old_input,new_input,available,lease,required,action_limits,guard.steps());
        lease.check_cancelled();return DerivedNativeGreenResponse(PreparedNativeGreenResponseDelta(pin),std::move(old_input),std::move(new_input),
            std::move(value),std::move(report));
    }
private:
    friend class PreparedNativeObservedGreenResponseDelta;
    explicit PreparedNativeGreenResponseDelta(std::shared_ptr<const State> value):state_(std::move(value)){}
    static void check_inputs(const State& s,const Pin& old_input,const Pin& new_input,const NativeGreenAvailability& current) {
        using namespace native_green_detail;
        availability(s.request.old_operator,s.request.new_operator,s.availability);
        availability(s.request.old_operator,s.request.new_operator,current);
        native_green_detail::field(old_input,s.request.old_operator,s.bindings.old_source);
        native_green_detail::field(new_input,s.request.new_operator,s.bindings.new_source);
    }
    static NativeGreenProvenance provenance(const State& s,const Pin& old_input,const Pin& new_input,NativeGreenAvailability available,
            const runtime::ExecutionLease& lease,runtime::ExecutionRequirements required,numeric::ExactActionLimits action,std::size_t steps) {
        NativeGreenProvenance report;
        report.old_endpoint=native_green_detail::witness(s.request.old_operator,s.request.old_injection,s.bindings.old_source);
        report.new_endpoint=native_green_detail::witness(s.request.new_operator,s.request.new_injection,s.bindings.new_source);
        report.old_endpoint.input_field=old_input.tensor_declaration()->key;report.new_endpoint.input_field=new_input.tensor_declaration()->key;
        report.lambda=s.request.lambda;report.selected_requirements=required;report.action_limits=action;
        report.green_preparation_steps=s.preparation_steps;report.preparation_steps=s.preparation_steps;report.execution_steps=steps;
        report.green_preparation_policy=s.policy;report.preparation_policy=s.policy;report.execution_policy=lease.policy().name;
        report.green_preparation_availability=s.availability;report.preparation_availability=s.availability;report.execution_availability=available;
        report.fallback=s.fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;return report;
    }
    std::shared_ptr<const State> state_;
};

class PreparedNativeObservedGreenResponseDelta {
    using Pin=storage::PinnedNativeAccession;
    struct State {
        PreparedNativeGreenResponseDelta green;
        Pin old_accession,new_accession;
        NativeGreenCorrespondence output;
        native_green_detail::Carrier old_output,new_output;
        temporal::PreparedObservedGreenResponseDelta solve;
        runtime::ExecutionRequirements retained;
        NativeGreenAvailability availability;
        std::string policy;
        std::size_t preparation_steps;
        bool fallback;
    };
public:
    PreparedNativeObservedGreenResponseDelta(const PreparedNativeObservedGreenResponseDelta&)=default;
    PreparedNativeObservedGreenResponseDelta& operator=(const PreparedNativeObservedGreenResponseDelta&)=default;
    static PreparedNativeObservedGreenResponseDelta prepare(PreparedNativeGreenResponseDelta green,Pin old_accession,Pin new_accession,
            NativeGreenCorrespondence output,NativeGreenAvailability available,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_green_detail;
        const auto g=green.state_;lease.check_cancelled();availability(g->request.old_operator,g->request.new_operator,g->availability);
        availability(g->request.old_operator,g->request.new_operator,available);
        same(old_accession,g->request.old_operator);same(new_accession,g->request.new_operator);
        complete_map(old_accession);complete_map(new_accession);
        const auto old_A=old_accession.tensor_declaration(),new_A=new_accession.tensor_declaration();
        require(old_A->role==storage::TemporalTensorRole::Accession && new_A->role==storage::TemporalTensorRole::Accession &&
                old_A->input==g->bindings.old_ambient->key && new_A->input==g->bindings.new_ambient->key,
                "native Green observation requires declared accessions on both actual ambient carriers");
        const std::array<Pin,2> maps{old_accession,new_accession};auto added=selected(maps,3);map_requirements(added,output.map);
        auto required=g->retained;required.records=add(required.records,added.records);
        required.rational_slots=add(required.rational_slots,added.rational_slots);required.sparse_entries=required.rational_slots;
        required.integer_bits=std::max(required.integer_bits,added.integer_bits);runtime::admit_execution(required,lease.policy());
        const auto& r=g->request;
        const std::array<Pin,12> selected_pins{r.old_operator,r.new_operator,r.old_injection,r.new_injection,
            r.ambient.old_publication,r.ambient.new_publication,r.source.old_publication,r.source.new_publication,
            old_accession,new_accession,output.old_publication,output.new_publication};
        const auto resident=add(payload_requirements(g->request).rational_slots,added.rational_slots);
        Guard guard(limits(lease,selected_pins,g->request.dimension_limit,resident),[&lease]{lease.check_cancelled();});
        const auto old_O=old_accession.carrier(),new_O=new_accession.carrier();linear(old_O,guard);linear(new_O,guard);
        const auto K=comparison(output,g->request.old_operator,g->request.new_operator,old_O,new_O,guard);
        auto old_matrix=native_update_detail::materialize(old_accession,guard);
        auto new_matrix=native_update_detail::materialize(new_accession,guard);
        temporal::PreparedObservedGreenResponseDelta solve(g->solve,std::move(old_matrix),std::move(new_matrix),K,guard);
        required.rational_slots=add(required.rational_slots,solve.stored_entries()-g->solve.stored_entries());
        required.sparse_entries=required.rational_slots;runtime::admit_execution(required,lease.policy());
        guard.entries(solve.stored_entries());lease.check_cancelled();
        return PreparedNativeObservedGreenResponseDelta(std::make_shared<const State>(State{std::move(green),std::move(old_accession),
            std::move(new_accession),std::move(output),old_O,new_O,std::move(solve),required,available,lease.policy().name,guard.steps(),
            g->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeGreenResponseDelta green_plan() const {return state_->green;}
    const temporal::PreparedObservedGreenResponseDelta& numeric_plan() const & {return state_->solve;}
    const temporal::PreparedObservedGreenResponseDelta& numeric_plan() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    runtime::ExecutionRequirements retained_requirements() const {return state_->retained;}
    DerivedNativeObservedGreenResponse execute(Pin old_input,Pin new_input,NativeGreenAvailability available,
                                                runtime::ExecutionLease lease=runtime::ExecutionLease{}) const {
        using namespace native_green_detail;
        const auto pin=state_;const auto g=pin->green.state_;lease.check_cancelled();
        PreparedNativeGreenResponseDelta::check_inputs(*g,old_input,new_input,available);
        availability(g->request.old_operator,g->request.new_operator,pin->availability);
        const auto& r=g->request;
        const std::array<Pin,14> selected_pins{r.old_operator,r.new_operator,r.old_injection,r.new_injection,
            r.ambient.old_publication,r.ambient.new_publication,r.source.old_publication,r.source.new_publication,
            pin->old_accession,pin->new_accession,pin->output.old_publication,pin->output.new_publication,old_input,new_input};
        auto payload=payload_requirements(g->request);
        map_requirements(payload,*pin->old_accession.tensor());map_requirements(payload,*pin->new_accession.tensor());map_requirements(payload,pin->output.map);
        map_requirements(payload,*old_input.tensor());map_requirements(payload,*new_input.tensor());
        auto required=pin->retained;required.records=add(required.records,2);
        required.rational_slots=add(required.rational_slots,add(native_update_detail::payload(old_input),native_update_detail::payload(new_input)));
        required.sparse_entries=required.rational_slots;required.integer_bits=std::max(required.integer_bits,payload.integer_bits);
        runtime::admit_execution(required,lease.policy());
        const auto action=limits(lease,selected_pins,g->request.dimension_limit,payload.rational_slots);
        Guard guard(action,[&lease]{lease.check_cancelled();});
        const auto old_values=field_values(old_input,guard),new_values=field_values(new_input,guard);
        auto value=pin->solve.apply(old_values,new_values,guard);
        auto report=PreparedNativeGreenResponseDelta::provenance(*g,old_input,new_input,available,lease,required,action,guard.steps());
        report.old_endpoint.output_carrier=pin->old_output->key;report.new_endpoint.output_carrier=pin->new_output->key;
        report.old_endpoint.accession=pin->old_accession.tensor_declaration()->key;report.new_endpoint.accession=pin->new_accession.tensor_declaration()->key;
        report.preparation_policy=pin->policy;report.preparation_availability=pin->availability;report.preparation_steps=pin->preparation_steps;
        report.observed=true;report.reused_green=true;report.fallback=pin->fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();return DerivedNativeObservedGreenResponse(PreparedNativeObservedGreenResponseDelta(pin),std::move(old_input),
            std::move(new_input),std::move(value),std::move(report));
    }
private:
    explicit PreparedNativeObservedGreenResponseDelta(std::shared_ptr<const State> value):state_(std::move(value)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::inference
