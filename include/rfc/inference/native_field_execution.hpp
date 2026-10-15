#pragma once
#include "rfc/inference/field_execution.hpp"
#include "rfc/storage/native_execution_binding.hpp"
#include "rfc/runtime/execution_policy.hpp"

namespace rfc::inference {

enum class NativeMetricSelection { NamedUnit, DeclaredDiagonal, DeclaredPositive };
struct NativeExactTowerRequest {
    NativeMetricSelection metrics{NativeMetricSelection::NamedUnit};
    // Ordered by retained grade. These are SourceGrade declarations, not
    // dimension-based identifications with explicit or cell-stalk carriers.
    std::vector<std::uint64_t> grade_carriers;
    std::vector<storage::PinnedNativeAccession> metric_tensors;
    field::PreparedExactProfile profile{field::PreparedExactProfile::FiniteActions};
    std::size_t global_grade_dimension_limit{64};
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
    std::string action_preparation_policy;
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
// Clearing denominators row by row bounds determinant minors by integer
// products. The extra dimension factor covers unreduced elimination products,
// triangular sums and residual verification; it is not a linear-time claim.
inline std::size_t solve_bound(std::size_t matrix_bits,std::size_t input_bits,std::size_t dimension){
    const auto n=add(dimension,1);std::size_t log=0;for(auto k=n;k;k>>=1u)++log;
    return mul(mul(32,n),add(add(mul(n,matrix_bits),input_bits),add(log,1)));
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

class DerivedNativeExactBatch {
public:
    const numeric::RationalVector& values() const & { return values_; }
    numeric::RationalVector values() const && { return std::move(values_); }
    const NativeExactProvenance& provenance() const & { return report_; }
    const std::vector<storage::PinnedNativeAccession>& inputs() const & { return inputs_; }
    std::size_t samples() const noexcept { return inputs_.size(); }
    std::size_t cells() const noexcept { return values_.size() / inputs_.size(); }
private:
    friend class PreparedNativeExactTower;
    DerivedNativeExactBatch(std::vector<storage::PinnedNativeAccession> inputs,
                           numeric::RationalVector values, NativeExactProvenance report)
        : inputs_(std::move(inputs)), values_(std::move(values)), report_(std::move(report)) {}
    std::vector<storage::PinnedNativeAccession> inputs_;
    numeric::RationalVector values_;
    NativeExactProvenance report_;
};

// Host-only: restricted model projections deliberately expose no source pin.
// This bridge does not add a way to obtain one or publish derived/model values.
class PreparedNativeExactTower {
    struct State;
public:
    class Action {
    public:
        field::OperatorRequest operation() const { return action_.operation(); }
        storage::PinnedNativeAccession publication() const { return owner_->publication; }
        std::size_t solve_dimension() const noexcept { return action_.solve_dimension(); }
        std::size_t retained_entries() const noexcept { return action_.retained_entries(); }
        std::size_t preparation_steps() const noexcept { return action_.preparation_steps(); }
        DerivedNativeExactField execute(const storage::PinnedNativeAccession& input,
                const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
            return PreparedNativeExactTower(owner_).execute_impl(operation(), input, lease, &action_, preparation_policy_, preparation_fallback_);
        }
    private:
        friend class PreparedNativeExactTower;
        Action(std::shared_ptr<const State> owner, field::PreparedGlobalAction action, std::string policy, bool fallback)
            : owner_(std::move(owner)), action_(std::move(action)), preparation_policy_(std::move(policy)), preparation_fallback_(fallback) {}
        std::shared_ptr<const State> owner_;
        field::PreparedGlobalAction action_;
        std::string preparation_policy_;
        bool preparation_fallback_{};
    };
    static PreparedNativeExactTower prepare(const storage::PinnedNativeAccession& publication,
        NativeExactTowerRequest request,const runtime::ExecutionHostPolicy& policy={}) {
        using namespace native_field_detail;
        (void)runtime::resolve_reference_backend(policy);
        require(policy.allow_allocation,"native exact preparation requires owning allocations");
        const auto endpoint=publication.endpoint();const auto& source=endpoint->source;
        require(request.metrics==NativeMetricSelection::NamedUnit || request.metrics==NativeMetricSelection::DeclaredDiagonal ||
                request.metrics==NativeMetricSelection::DeclaredPositive,
                "unknown native metric selection");
        require(request.profile==field::PreparedExactProfile::FiniteActions || request.profile==field::PreparedExactProfile::GlobalActions,
                "unknown native exact profile");
        const bool global=request.profile==field::PreparedExactProfile::GlobalActions;
        require(request.metrics!=NativeMetricSelection::DeclaredPositive || global,"general metrics require the global exact profile");
        require(!global || request.global_grade_dimension_limit>0,"global exact dimension limit must be positive");
        require(request.grade_carriers.size()==source.grade_count(),"one declared SourceGrade carrier required per grade");
        require(request.metrics==NativeMetricSelection::NamedUnit?request.metric_tensors.empty():request.metric_tensors.size()==source.grade_count(),
                "native metric selection/count mismatch");
        const auto carriers=storage::temporal_endpoint_detail::carriers(*endpoint);
        std::size_t cells=0,occurrences=0,scale_bits=1,lattice_bits=1,metric_bits=1,metric_slots=0,max_boundary_terms=0;
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};const auto& arena=source.grade(grade);
            const auto key=request.grade_carriers[k];
            require(carriers.contains(key),"selected grade carrier absent");const auto& carrier=*carriers.at(key);
            require(carrier.kind==storage::TemporalCarrierKind::SourceGrade && carrier.grade==grade,
                    "native tower requires the declared SourceGrade carrier at every retained grade");
            if(global && arena.cell_count()>request.global_grade_dimension_limit)throw std::length_error("global exact grade dimension limit");
            cells=add(cells,arena.cell_count());occurrences=add(occurrences,arena.incidence_count());
            if (k) {
                for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
                    const auto [begin, end] = arena.incidence_range(cell);
                    max_boundary_terms = std::max(max_boundary_terms, end - begin);
                }
                const auto below = source.grade(core::Grade{static_cast<std::uint16_t>(k - 1u)}).cell_count();
                for (std::size_t cell = 0; cell < below; ++cell) {
                    const auto [begin, end] = arena.coface_incidence_range(cell);
                    max_boundary_terms = std::max(max_boundary_terms, end - begin);
                }
            }
            for(const auto& scale:arena.cell_scales)scale_bits=std::max(scale_bits,integer_bits(scale));
            for(const auto& coefficient:arena.lattice_coefficient)
                lattice_bits=std::max(lattice_bits,mpz_sgn(coefficient.get_mpz_t())==0?std::size_t{0}:mpz_sizeinbase(coefficient.get_mpz_t(),2));
            if(request.metrics!=NativeMetricSelection::NamedUnit){
                const auto& pin=request.metric_tensors[k];same_publication(publication,pin);
                const auto declaration=pin.tensor_declaration();const auto& tensor=declaration->tensor;
                require(declaration->role==storage::TemporalTensorRole::Metric && declaration->input==key && declaration->output==key,
                        "metric must bind the selected native grade carrier");
                const auto n=arena.cell_count();
                require(tensor.domain().extents()==std::vector<std::size_t>{n,n} && tensor.support().count()==tensor.domain().logical_size(),
                        "native metric requires complete square support");
                std::size_t diagonal=0;
                for(const auto& x:tensor.values().entries()){
                    if(request.metrics==NativeMetricSelection::DeclaredDiagonal){
                        require(x.index/n==x.index%n && x.value>0,"finite native bridge requires positive diagonal metrics");++diagonal;
                    }
                }
                if(request.metrics==NativeMetricSelection::DeclaredDiagonal)
                    require(diagonal==n,"native metric must measure every retained direction");
                metric_bits=std::max(metric_bits,tensor_bits(tensor));
                metric_slots=add(metric_slots,!global && request.metrics==NativeMetricSelection::DeclaredDiagonal?n:tensor.domain().logical_size());
            }
        }
        runtime::ExecutionRequirements requirements;
        requirements.records=add(add(cells,occurrences),mul(source.grade_count(),4));
        requirements.rational_slots=add(mul(cells,12),metric_slots);
        requirements.sparse_entries=add(occurrences,metric_slots);
        // Includes the all-grade metric checks performed by cached Hodge setup.
        requirements.scalar_work=add(mul(add(cells,occurrences),16),add(metric_slots,mul(source.grade_count(),source.grade_count())));
        requirements.integer_bits=std::max(add(mul(scale_bits,2),lattice_bits),metric_bits);
        if(global) {
            const auto square=mul(cells,cells),cube=mul(square,cells);
            requirements.records=add(requirements.records,mul(square,8));
            requirements.rational_slots=add(requirements.rational_slots,mul(square,16));
            requirements.sparse_entries=add(requirements.sparse_entries,mul(square,16));
            requirements.scalar_work=add(requirements.scalar_work,mul(add(cube,mul(cells,occurrences)),64));
            const auto coefficient_bits=sum_bound(add(add(mul(scale_bits,2),lattice_bits),metric_bits),add(cells,occurrences));
            requirements.integer_bits=solve_bound(coefficient_bits,0,mul(cells,2));
        }
        runtime::admit_execution(requirements,policy);
        if(requirements.scalar_work>publication.limits().max_action_work)throw std::length_error("native exact accession work budget");
        std::vector<field::ExactMetric> metrics;metrics.reserve(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto n=source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
            if(request.metrics==NativeMetricSelection::NamedUnit)metrics.push_back(field::ExactMetric::identity(n));
            else if(request.metrics==NativeMetricSelection::DeclaredDiagonal){
                numeric::RationalVector diagonal(n,numeric::Rational{0});
                for(const auto& x:request.metric_tensors[k].tensor()->values().entries())diagonal[x.index/n]=x.value;
                metrics.push_back(field::ExactMetric::diagonal(std::move(diagonal)));
            } else metrics.emplace_back(storage::temporal_tensor_matrix(*request.metric_tensors[k].tensor_declaration()));
        }
        auto state=std::make_shared<const State>(publication,std::move(request),policy,requirements,
            scale_bits,lattice_bits,metric_bits,occurrences,cells,max_boundary_terms,std::move(metrics));
        if(state->tower.harmonic_frame_integer_bits()>requirements.integer_bits)throw std::runtime_error("native frame integer envelope violated");
        return PreparedNativeExactTower(std::move(state));
    }
    PreparedNativeExactTower(const PreparedNativeExactTower&)=default;
    PreparedNativeExactTower& operator=(const PreparedNativeExactTower&)=default;
    std::size_t grade_size(core::Grade grade) const {return state_->publication.endpoint()->source.grade(grade).cell_count();}
    std::uint64_t grade_carrier(core::Grade grade) const {return state_->request.grade_carriers.at(grade.value);}
    storage::PinnedNativeAccession publication() const {return state_->publication;}
    const std::vector<storage::PinnedNativeAccession>& metric_pins() const & {return state_->request.metric_tensors;}
    const std::vector<storage::PinnedNativeAccession>& metric_pins() const &&=delete;
    const runtime::ExecutionHostPolicy& preparation_policy() const & {return state_->policy;}
    const runtime::ExecutionHostPolicy& preparation_policy() const && = delete;
    NativeMetricSelection metric_selection() const {return state_->request.metrics;}
    field::PreparedExactProfile profile() const {return state_->request.profile;}
    std::vector<std::uint64_t> metric_keys() const {
        std::vector<std::uint64_t> out;for(const auto& metric:state_->request.metric_tensors)out.push_back(metric.tensor_declaration()->key);return out;
    }
    const field::PreparedExactExecutionTower& tower() const & {return state_->tower;}
    const field::PreparedExactExecutionTower& tower() const && = delete;
    Action prepare_action(const field::OperatorRequest& operation,
            const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        field::PreparedExactExecutionTower::validate_request(operation, profile()); lease.check_cancelled();
        if (profile() != field::PreparedExactProfile::GlobalActions ||
            (operation.kind != field::OperatorKind::Green && operation.kind != field::OperatorKind::HarmonicProjector &&
             operation.kind != field::OperatorKind::HarmonicInverse && operation.kind != field::OperatorKind::GradientPotential))
            throw std::invalid_argument("operator requires the prepared global exact profile");
        if (operation.kind == field::OperatorKind::GradientPotential && operation.grade.value == 0)
            throw std::invalid_argument("grade zero has no lower potential carrier");
        (void)grade_carrier(operation.grade);
        const auto required = requirements(0, operation.kind, operation.lambda);
        runtime::admit_execution(required, lease.policy());
        if (required.scalar_work > state_->publication.limits().max_action_work)
            throw std::length_error("native exact accession work budget");
        numeric::ExactActionGuard guard(action_limits(required), [&lease] { lease.check_cancelled(); });
        auto action = state_->tower.prepare_action(operation, guard);
        lease.check_cancelled();
        return Action(state_, std::move(action), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu);
    }
    runtime::ExecutionRequirements requirements(std::size_t input_bits,field::OperatorKind kind,const numeric::Rational& lambda=numeric::Rational{0}) const {
        using namespace native_field_detail;
        auto out=state_->requirements;
        // Source actions use S_lower * B_lattice * S_upper^-1 and its
        // transpose. Charge both scales and the retained lattice integers,
        // including intermediate products whose final value may cancel.
        const bool global=profile()==field::PreparedExactProfile::GlobalActions;
        // Each finite contraction sums one retained row. Inactive, repeated and
        // zero occurrences remain included in this conservative row bound.
        const auto terms=global?state_->occurrences:state_->max_boundary_terms;
        const auto b=[&](auto bits){return add(sum_bound(add(add(bits,state_->scale_bits),state_->lattice_bits),terms),state_->scale_bits);};
        const auto adj=[&](auto bits){
            if(global)return solve_bound(state_->metric_bits,b(sum_bound(add(bits,state_->metric_bits),state_->cells)),state_->cells);
            return add(b(add(bits,state_->metric_bits)),state_->metric_bits);
        };
        switch(kind){
        case field::OperatorKind::Boundary:out.integer_bits=b(input_bits);break;
        case field::OperatorKind::Adjoint:out.integer_bits=adj(input_bits);break;
        case field::OperatorKind::HodgeDown:out.integer_bits=adj(b(input_bits));break;
        case field::OperatorKind::HodgeUp:out.integer_bits=b(adj(input_bits));break;
        case field::OperatorKind::Hodge:out.integer_bits=sum_bound(std::max(adj(b(input_bits)),b(adj(input_bits))),2);break;
        case field::OperatorKind::Green: case field::OperatorKind::HarmonicInverse: case field::OperatorKind::GradientPotential: {
            if(!global)throw std::invalid_argument("solve requires the global exact profile");
            const auto dimension=mul(state_->cells,8),square=mul(dimension,dimension);
            out.records=add(out.records,mul(square,4));out.rational_slots=add(out.rational_slots,mul(square,4));
            out.sparse_entries=add(out.sparse_entries,mul(square,4));
            out.scalar_work=add(out.scalar_work,mul(mul(square,dimension),8));
            auto matrix_bits=add(std::max(sum_bound(add(mul(state_->scale_bits,2),state_->lattice_bits),state_->occurrences),state_->metric_bits),integer_bits(lambda));
            if(kind!=field::OperatorKind::Green)
                matrix_bits=std::max(matrix_bits,sum_bound(add(state_->tower.harmonic_frame_integer_bits(),state_->metric_bits),state_->cells));
            out.integer_bits=solve_bound(matrix_bits,sum_bound(add(input_bits,state_->metric_bits),state_->cells),dimension);
            // The potential is an unknown of the same original augmented
            // system. Its solve envelope already includes b = B x; no second
            // boundary contraction is performed after solving.
            break;
        }
        case field::OperatorKind::HarmonicProjector: {
            if(!global)throw std::invalid_argument("projector requires the global exact profile");
            const auto frame_bits=state_->tower.harmonic_frame_integer_bits();
            const auto gram_bits=sum_bound(add(mul(frame_bits,2),state_->metric_bits),state_->cells);
            const auto rhs_bits=sum_bound(add(add(frame_bits,state_->metric_bits),input_bits),mul(state_->cells,state_->cells));
            out.integer_bits=sum_bound(add(frame_bits,solve_bound(gram_bits,rhs_bits,state_->cells)),state_->cells);
            break;
        }
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
    runtime::ExecutionRequirements batch_requirements(const field::OperatorRequest& operation,
            std::span<const storage::PinnedNativeAccession> inputs, std::size_t scratch_slots) const {
        using namespace native_field_detail;
        require(profile() == field::PreparedExactProfile::FiniteActions,
                "native exact batch requires the finite action profile");
        field::PreparedExactExecutionTower::validate_request(operation, profile());
        require(!inputs.empty(), "native exact batch requires at least one field");
        require(operation.grade.value || (operation.kind != field::OperatorKind::Boundary && operation.kind != field::OperatorKind::Adjoint),
                "grade zero has no boundary or adjoint");
        const auto input_grade = operation.kind == field::OperatorKind::Adjoint
            ? core::Grade{static_cast<std::uint16_t>(operation.grade.value - 1u)} : operation.grade;
        const auto output_grade = operation.kind == field::OperatorKind::Boundary
            ? core::Grade{static_cast<std::uint16_t>(operation.grade.value - 1u)} : operation.grade;
        (void)grade_carrier(operation.grade);
        std::size_t bits = 1;
        for (const auto& input : inputs) {
            validate_input(input, input_grade);
            bits = std::max(bits, tensor_bits(*input.tensor()));
        }
        auto required = requirements(bits, operation.kind, operation.lambda);
        required.records = add(required.records, inputs.size());
        required.rational_slots = add(required.rational_slots,
            add(scratch_slots, mul(add(grade_size(input_grade), grade_size(output_grade)), inputs.size())));
        required.scalar_work = mul(required.scalar_work, inputs.size());
        return required;
    }
    numeric::RationalVector input_batch_values(std::span<const storage::PinnedNativeAccession> inputs,
                                               core::Grade grade) const {
        using namespace native_field_detail;
        require(!inputs.empty(), "native exact batch requires at least one field");
        for (const auto& input : inputs) validate_input(input, grade);
        numeric::RationalVector out(mul(grade_size(grade), inputs.size()), numeric::Rational{0});
        for (std::size_t s = 0; s < inputs.size(); ++s)
            for (const auto& entry : inputs[s].tensor()->values().entries()) out[entry.index * inputs.size() + s] = entry.value;
        return out;
    }
    DerivedNativeExactBatch execute_batch(const field::OperatorRequest& operation,
            std::span<const storage::PinnedNativeAccession> inputs,
            field::PreparedExactExecutionTower::BatchWorkspace& workspace,
            const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) const {
        using namespace native_field_detail;
        lease.check_cancelled();
        state_->tower.validate_batch_workspace(operation.grade, inputs.size(), workspace);
        const auto required = batch_requirements(operation, inputs, workspace.retained_slots());
        runtime::admit_execution(required, lease.policy());
        if (required.scalar_work > state_->publication.limits().max_action_work)
            throw std::length_error("native exact batch accession work budget");
        const auto input_grade = operation.kind == field::OperatorKind::Adjoint
            ? core::Grade{static_cast<std::uint16_t>(operation.grade.value - 1u)} : operation.grade;
        const auto output_grade = operation.kind == field::OperatorKind::Boundary
            ? core::Grade{static_cast<std::uint16_t>(operation.grade.value - 1u)} : operation.grade;
        const auto packed = input_batch_values(inputs, input_grade);
        numeric::RationalVector values(mul(grade_size(output_grade), inputs.size()));
        state_->tower.apply_batch_into(operation, packed, values, inputs.size(), workspace, [&lease] { lease.check_cancelled(); });
        for (const auto& value : values)
            if (integer_bits(value) > required.integer_bits) throw std::runtime_error("native batch integer envelope violated");
        NativeExactProvenance report;
        report.profile = "source-grade-diagonal-finite-batch-v1";
        report.implementation = "PreparedExactExecutionTower/GMP/reused-batch-v1";
        report.source_namespace = state_->publication.source_namespace(); report.sequence = state_->publication.sequence();
        report.input_carrier = grade_carrier(input_grade); report.output_carrier = grade_carrier(output_grade);
        report.metric_selection = metric_selection(); report.metrics = metric_keys(); report.operation = operation;
        report.requirements = required; report.preparation_policy = state_->policy.name; report.execution_policy = lease.policy().name;
        report.fallback = state_->policy.backend != runtime::ExecutionBackend::ExactCpu || lease.policy().backend != runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();
        return DerivedNativeExactBatch({inputs.begin(), inputs.end()}, std::move(values), std::move(report));
    }
    DerivedNativeExactField execute(const field::OperatorRequest& operation,const storage::PinnedNativeAccession& input,
                                   const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        return execute_impl(operation, input, lease, nullptr, lease.policy().name, false);
    }
private:
    numeric::ExactActionLimits action_limits(const runtime::ExecutionRequirements& required) const {
        return {native_field_detail::mul(state_->cells, 8), required.sparse_entries, required.scalar_work, required.integer_bits};
    }
    DerivedNativeExactField execute_impl(const field::OperatorRequest& operation, const storage::PinnedNativeAccession& input,
            const runtime::ExecutionLease& lease, const field::PreparedGlobalAction* prepared,
            const std::string& action_policy, bool action_fallback) const {
        field::PreparedExactExecutionTower::validate_request(operation,profile());lease.check_cancelled();
        const auto grade=operation.kind==field::OperatorKind::Adjoint
            ?core::Grade{static_cast<std::uint16_t>(operation.grade.value?operation.grade.value-1u:0u)}:operation.grade;
        if((operation.kind==field::OperatorKind::Boundary || operation.kind==field::OperatorKind::Adjoint ||
            operation.kind==field::OperatorKind::GradientPotential) && !operation.grade.value)
            throw std::invalid_argument("grade zero has no boundary, adjoint or lower potential carrier");
        // Validate both the requested operator grade and input carrier before allocation.
        (void)grade_carrier(operation.grade);validate_input(input,grade);
        const auto required=requirements(native_field_detail::tensor_bits(*input.tensor()),operation.kind,operation.lambda);
        runtime::admit_execution(required,lease.policy());
        if(required.scalar_work>state_->publication.limits().max_action_work)throw std::length_error("native exact accession work budget");
        numeric::RationalVector values;
        const bool global_solve = operation.kind == field::OperatorKind::Green || operation.kind == field::OperatorKind::HarmonicProjector ||
            operation.kind == field::OperatorKind::HarmonicInverse || operation.kind == field::OperatorKind::GradientPotential;
        if (global_solve) {
            numeric::ExactActionGuard guard(action_limits(required), [&lease] { lease.check_cancelled(); });
            const auto input_field = input_values(input, grade);
            if (prepared) values = prepared->apply(input_field, guard);
            else values = state_->tower.prepare_action(operation, guard).apply(input_field, guard);
        } else values = state_->tower.apply(operation,input_values(input,grade));
        for(const auto& value:values)if(native_field_detail::integer_bits(value)>required.integer_bits)
            throw std::runtime_error("native finite integer envelope violated");
        NativeExactProvenance report;report.source_namespace=state_->publication.source_namespace();report.sequence=state_->publication.sequence();
        if(profile()==field::PreparedExactProfile::GlobalActions){
            report.profile="source-grade-global-exact-v1";
            report.implementation=global_solve ? "PreparedGlobalAction/GMP/prepared-solves-v1" : "PreparedExactExecutionTower/GMP/sparse-solves-v1";
        }
        report.input_field=input.tensor_declaration()->key;report.input_carrier=grade_carrier(grade);
        report.output_carrier=grade_carrier((operation.kind==field::OperatorKind::Boundary || operation.kind==field::OperatorKind::GradientPotential)
            ?core::Grade{static_cast<std::uint16_t>(operation.grade.value-1u)}:operation.grade);
        report.metric_selection=state_->request.metrics;for(const auto& metric:state_->request.metric_tensors)report.metrics.push_back(metric.tensor_declaration()->key);
        report.operation=operation;report.requirements=required;report.preparation_policy=state_->policy.name;report.execution_policy=lease.policy().name;
        if(global_solve)report.action_preparation_policy=action_policy;
        report.fallback=action_fallback || state_->policy.backend!=runtime::ExecutionBackend::ExactCpu || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu;
        lease.check_cancelled();return DerivedNativeExactField(state_->publication,std::move(values),std::move(report));
    }
private:
    struct State {
        storage::PinnedNativeAccession publication;
        NativeExactTowerRequest request;
        runtime::ExecutionHostPolicy policy;
        runtime::ExecutionRequirements requirements;
        std::size_t scale_bits,lattice_bits,metric_bits,occurrences,cells,max_boundary_terms;
        field::PreparedExactExecutionTower tower;
        State(storage::PinnedNativeAccession p,NativeExactTowerRequest r,runtime::ExecutionHostPolicy policy_value,
              runtime::ExecutionRequirements limits,std::size_t sb,std::size_t lb,std::size_t mb,std::size_t count,std::size_t cell_count,std::size_t row_terms,std::vector<field::ExactMetric> metrics)
            :publication(std::move(p)),request(std::move(r)),policy(std::move(policy_value)),requirements(limits),
             scale_bits(sb),lattice_bits(lb),metric_bits(mb),occurrences(count),cells(cell_count),max_boundary_terms(row_terms),
             tower(std::shared_ptr<const core::RelationalComplex>(publication.endpoint(),&publication.endpoint()->source),std::move(metrics),request.profile){}
    };
    explicit PreparedNativeExactTower(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::inference
