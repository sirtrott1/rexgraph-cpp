#pragma once
#include "rfc/inference/field_execution.hpp"
#include "rfc/storage/native_execution_binding.hpp"
#include "rfc/runtime/execution_policy.hpp"

namespace rfc::inference {

enum class NativeMetricSelection { NamedUnit, DeclaredDiagonal };
struct NativeExactTowerRequest {
    NativeMetricSelection metrics{NativeMetricSelection::NamedUnit};
    // Ordered by retained grade. These are SourceGrade declarations, not
    // dimension-based identifications with explicit or cell-stalk carriers.
    std::vector<std::uint64_t> grade_carriers;
    std::vector<storage::PinnedNativeAccession> metric_tensors;
};
struct NativeExactProvenance {
    std::string profile{"source-grade-diagonal-finite-v1"};
    std::string implementation{"PreparedExactExecutionTower/GMP/serial-finite-v1"};
    std::uint64_t source_namespace{},sequence{},input_field{},input_carrier{},output_carrier{};
    NativeMetricSelection metric_selection{};
    std::vector<std::uint64_t> metrics;
    field::OperatorRequest operation;
    runtime::ExecutionRequirements requirements;
    std::string preparation_policy,execution_policy;
    bool fallback{};
};

namespace native_field_detail {
using storage::native_execution_detail::add;
using storage::native_execution_detail::mul;
using storage::native_execution_detail::integer_bits;
inline void require(bool condition,const char* message){if(!condition)throw std::invalid_argument(message);}
inline void same_publication(const storage::PinnedNativeAccession& a,const storage::PinnedNativeAccession& b){
    require(a.endpoint().get()==b.endpoint().get() && a.source_namespace()==b.source_namespace(),
            "native exact bridge requires the same authentic publication");
}
inline std::size_t sum_bound(std::size_t bits,std::size_t terms){
    std::size_t log=0;for(auto n=terms;n;n>>=1u)++log;
    return add(mul(add(terms,2),bits),add(log,2));
}
inline std::size_t tensor_bits(const numeric::SparseRationalTensor& tensor){
    std::size_t result=1;for(const auto& x:tensor.values().entries())result=std::max(result,integer_bits(x.value));return result;
}
inline numeric::RationalVector dense(const numeric::SparseRationalTensor& tensor){
    numeric::RationalVector out(tensor.domain().logical_size(),numeric::Rational{0});
    for(const auto& x:tensor.values().entries())out[x.index]=x.value;
    return out;
}
}

class PreparedNativeExactTower;
class DerivedNativeExactField {
public:
    const numeric::RationalVector& values() const & {return values_;}
    numeric::RationalVector values() const && {return values_;}
    const NativeExactProvenance& provenance() const & {return report_;}
    NativeExactProvenance provenance() const && {return report_;}
    storage::PinnedNativeAccession publication() const {return publication_;}
private:
    friend class PreparedNativeExactTower;
    DerivedNativeExactField(storage::PinnedNativeAccession p,numeric::RationalVector v,NativeExactProvenance r)
        :publication_(std::move(p)),values_(std::move(v)),report_(std::move(r)){}
    storage::PinnedNativeAccession publication_;
    numeric::RationalVector values_;
    NativeExactProvenance report_;
};

// Host-only: restricted model projections deliberately expose no source pin.
// This bridge does not add a way to obtain one or publish derived/model values.
class PreparedNativeExactTower {
public:
    static PreparedNativeExactTower prepare(const storage::PinnedNativeAccession& publication,
        NativeExactTowerRequest request,const runtime::ExecutionHostPolicy& policy={}) {
        using namespace native_field_detail;
        (void)runtime::resolve_reference_backend(policy);
        require(policy.allow_allocation,"native exact preparation requires owning allocations");
        const auto endpoint=publication.endpoint();const auto& source=endpoint->source;
        require(request.metrics==NativeMetricSelection::NamedUnit || request.metrics==NativeMetricSelection::DeclaredDiagonal,
                "unknown native metric selection");
        require(request.grade_carriers.size()==source.grade_count(),"one declared SourceGrade carrier required per grade");
        require(request.metrics==NativeMetricSelection::NamedUnit?request.metric_tensors.empty():request.metric_tensors.size()==source.grade_count(),
                "native metric selection/count mismatch");
        const auto carriers=storage::temporal_endpoint_detail::carriers(*endpoint);
        std::size_t cells=0,occurrences=0,scale_bits=1,lattice_bits=1,metric_bits=1,metric_slots=0;
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};const auto& arena=source.grade(grade);
            const auto key=request.grade_carriers[k];
            require(carriers.contains(key),"selected grade carrier absent");const auto& carrier=*carriers.at(key);
            require(carrier.kind==storage::TemporalCarrierKind::SourceGrade && carrier.grade==grade && arena.cell_count()>0,
                    "finite native tower requires nonempty declared SourceGrade carriers");
            cells=add(cells,arena.cell_count());occurrences=add(occurrences,arena.incidence_count());
            for(const auto& scale:arena.cell_scales)scale_bits=std::max(scale_bits,integer_bits(scale));
            for(const auto& coefficient:arena.lattice_coefficient)
                lattice_bits=std::max(lattice_bits,mpz_sgn(coefficient.get_mpz_t())==0?std::size_t{0}:mpz_sizeinbase(coefficient.get_mpz_t(),2));
            if(request.metrics==NativeMetricSelection::DeclaredDiagonal){
                const auto& pin=request.metric_tensors[k];same_publication(publication,pin);
                const auto declaration=pin.tensor_declaration();const auto& tensor=declaration->tensor;
                require(declaration->role==storage::TemporalTensorRole::Metric && declaration->input==key && declaration->output==key,
                        "metric must bind the selected native grade carrier");
                const auto n=arena.cell_count();
                require(tensor.domain().extents()==std::vector<std::size_t>{n,n} && tensor.support().count()==tensor.domain().logical_size(),
                        "native metric requires complete square support");
                std::size_t diagonal=0;
                for(const auto& x:tensor.values().entries()){
                    require(x.index/n==x.index%n && x.value>0,"finite native bridge requires positive diagonal metrics");++diagonal;
                }
                require(diagonal==n,"native metric must measure every retained direction");
                metric_bits=std::max(metric_bits,tensor_bits(tensor));metric_slots=add(metric_slots,tensor.domain().logical_size());
            }
        }
        runtime::ExecutionRequirements requirements;
        requirements.records=add(add(cells,occurrences),mul(source.grade_count(),4));
        requirements.rational_slots=add(mul(cells,12),metric_slots);
        requirements.sparse_entries=add(occurrences,metric_slots);
        // Includes the all-grade metric checks performed by cached Hodge setup.
        requirements.scalar_work=add(mul(add(cells,occurrences),16),add(metric_slots,mul(source.grade_count(),source.grade_count())));
        requirements.integer_bits=std::max(add(mul(scale_bits,2),lattice_bits),metric_bits);
        runtime::admit_execution(requirements,policy);
        if(requirements.scalar_work>publication.limits().max_action_work)throw std::length_error("native exact accession work budget");
        std::vector<field::ExactMetric> metrics;metrics.reserve(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto n=source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
            if(request.metrics==NativeMetricSelection::NamedUnit)metrics.push_back(field::ExactMetric::identity(n));
            else{
                numeric::RationalVector diagonal(n,numeric::Rational{0});
                for(const auto& x:request.metric_tensors[k].tensor()->values().entries())diagonal[x.index/n]=x.value;
                metrics.push_back(field::ExactMetric::diagonal(std::move(diagonal)));
            }
        }
        return PreparedNativeExactTower(std::make_shared<const State>(publication,std::move(request),policy,requirements,
            scale_bits,lattice_bits,metric_bits,occurrences,std::move(metrics)));
    }
    PreparedNativeExactTower(const PreparedNativeExactTower&)=default;
    PreparedNativeExactTower& operator=(const PreparedNativeExactTower&)=default;
    std::size_t grade_size(core::Grade grade) const {return state_->publication.endpoint()->source.grade(grade).cell_count();}
    std::uint64_t grade_carrier(core::Grade grade) const {return state_->request.grade_carriers.at(grade.value);}
    storage::PinnedNativeAccession publication() const {return state_->publication;}
    const runtime::ExecutionHostPolicy& preparation_policy() const & {return state_->policy;}
    const runtime::ExecutionHostPolicy& preparation_policy() const && = delete;
    NativeMetricSelection metric_selection() const {return state_->request.metrics;}
    std::vector<std::uint64_t> metric_keys() const {
        std::vector<std::uint64_t> out;for(const auto& metric:state_->request.metric_tensors)out.push_back(metric.tensor_declaration()->key);return out;
    }
    const field::PreparedExactExecutionTower& tower() const & {return state_->tower;}
    const field::PreparedExactExecutionTower& tower() const && = delete;
    runtime::ExecutionRequirements requirements(std::size_t input_bits,field::OperatorKind kind) const {
        using namespace native_field_detail;
        auto out=state_->requirements;
        // Source actions use S_lower * B_lattice * S_upper^-1 and its
        // transpose. Charge both scales and the retained lattice integers,
        // including intermediate products whose final value may cancel.
        const auto b=[&](auto bits){return add(sum_bound(add(add(bits,state_->scale_bits),state_->lattice_bits),state_->occurrences),state_->scale_bits);};
        const auto adj=[&](auto bits){return add(b(add(bits,state_->metric_bits)),state_->metric_bits);};
        switch(kind){
        case field::OperatorKind::Boundary:out.integer_bits=b(input_bits);break;
        case field::OperatorKind::Adjoint:out.integer_bits=adj(input_bits);break;
        case field::OperatorKind::HodgeDown:out.integer_bits=adj(b(input_bits));break;
        case field::OperatorKind::HodgeUp:out.integer_bits=b(adj(input_bits));break;
        case field::OperatorKind::Hodge:out.integer_bits=sum_bound(std::max(adj(b(input_bits)),b(adj(input_bits))),2);break;
        default:throw std::invalid_argument("operator outside native finite profile");
        }
        out.integer_bits=std::max(out.integer_bits,state_->requirements.integer_bits);return out;
    }
    numeric::RationalVector input_values(const storage::PinnedNativeAccession& input,core::Grade grade) const {
        validate_input(input,grade);return native_field_detail::dense(*input.tensor());
    }
    void validate_input(const storage::PinnedNativeAccession& input,core::Grade grade) const {
        using namespace native_field_detail;same_publication(state_->publication,input);
        require(input.kind()==storage::NativeAccessionKind::Field,"native exact input requires a stored field pin");
        const auto declaration=input.tensor_declaration();
        require(declaration->role==storage::TemporalTensorRole::Field && declaration->output==grade_carrier(grade),
                "native exact field carrier identity mismatch");
        const auto tensor=input.tensor();
        require(tensor->domain()==numeric::IndexDomain::linear(grade_size(grade)) && tensor->support().count()==grade_size(grade),
                "native exact field requires full retained grade support");
    }
    DerivedNativeExactField execute(const field::OperatorRequest& operation,const storage::PinnedNativeAccession& input,
                                   const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        field::PreparedExactExecutionTower::validate_request(operation);lease.check_cancelled();
        const auto grade=operation.kind==field::OperatorKind::Adjoint
            ?core::Grade{static_cast<std::uint16_t>(operation.grade.value?operation.grade.value-1u:0u)}:operation.grade;
        if((operation.kind==field::OperatorKind::Boundary || operation.kind==field::OperatorKind::Adjoint) && !operation.grade.value)
            throw std::invalid_argument("grade zero has no boundary/adjoint");
        // Validate both the requested operator grade and input carrier before allocation.
        (void)grade_carrier(operation.grade);validate_input(input,grade);
        const auto required=requirements(native_field_detail::tensor_bits(*input.tensor()),operation.kind);
        runtime::admit_execution(required,lease.policy());
        auto values=state_->tower.apply(operation,input_values(input,grade));
        for(const auto& value:values)if(native_field_detail::integer_bits(value)>required.integer_bits)
            throw std::runtime_error("native finite integer envelope violated");
        NativeExactProvenance report;report.source_namespace=state_->publication.source_namespace();report.sequence=state_->publication.sequence();
        report.input_field=input.tensor_declaration()->key;report.input_carrier=grade_carrier(grade);
        report.output_carrier=grade_carrier(operation.kind==field::OperatorKind::Boundary
            ?core::Grade{static_cast<std::uint16_t>(operation.grade.value-1u)}:operation.grade);
        report.metric_selection=state_->request.metrics;for(const auto& metric:state_->request.metric_tensors)report.metrics.push_back(metric.tensor_declaration()->key);
        report.operation=operation;report.requirements=required;report.preparation_policy=state_->policy.name;report.execution_policy=lease.policy().name;
        report.fallback=state_->policy.backend!=runtime::ExecutionBackend::ExactCpu || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();return DerivedNativeExactField(state_->publication,std::move(values),std::move(report));
    }
private:
    struct State {
        storage::PinnedNativeAccession publication;
        NativeExactTowerRequest request;
        runtime::ExecutionHostPolicy policy;
        runtime::ExecutionRequirements requirements;
        std::size_t scale_bits,lattice_bits,metric_bits,occurrences;
        field::PreparedExactExecutionTower tower;
        State(storage::PinnedNativeAccession p,NativeExactTowerRequest r,runtime::ExecutionHostPolicy policy_value,
              runtime::ExecutionRequirements limits,std::size_t sb,std::size_t lb,std::size_t mb,std::size_t count,std::vector<field::ExactMetric> metrics)
            :publication(std::move(p)),request(std::move(r)),policy(std::move(policy_value)),requirements(limits),
             scale_bits(sb),lattice_bits(lb),metric_bits(mb),occurrences(count),
             tower(std::shared_ptr<const core::RelationalComplex>(publication.endpoint(),&publication.endpoint()->source),std::move(metrics)){}
    };
    explicit PreparedNativeExactTower(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::inference
