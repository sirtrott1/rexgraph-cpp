#pragma once

#include "rfc/storage/native_execution_binding.hpp"
#include "rfc/runtime/execution_policy.hpp"

namespace rfc::database {

struct NativeExecutionReport {
    std::string profile{"one-grade-rational-contribution-v1"};
    std::string implementation{"GradeTensorField/GMP/serial-reference-v1"};
    runtime::ExecutionBackend selected_backend{runtime::ExecutionBackend::ExactCpu};
    bool fallback{};
    std::uint64_t source_namespace{}, sequence{};
    core::Grade upper_grade{};
    runtime::ExecutionRequirements requirements;
    storage::NativeAccessionLimits accession_limits;
    runtime::ExecutionHostPolicy preparation_policy;
    std::optional<runtime::ExecutionHostPolicy> execution_policy;
    std::vector<runtime::ExecutionDecision> decisions;
};
class DerivedNativeField;
using NativeFieldWorkspace = storage::NativeFieldBinding::Workspace;
class PreparedNativeField {
public:
    PreparedNativeField(const PreparedNativeField&) = default;
    PreparedNativeField& operator=(const PreparedNativeField&) = default;
    const NativeExecutionReport& report() const & noexcept { return report_; }
    const NativeExecutionReport& report() const && = delete;
    const storage::NativeFieldPlan& binding() const & noexcept { return binding_.plan(); }
    const storage::NativeFieldPlan& binding() const && = delete;
    const local::PreparedGradeFieldAction& kernel() const & noexcept { return binding_.prepared_action(); }
    const local::PreparedGradeFieldAction& kernel() const && = delete;
private:
    PreparedNativeField(const storage::NativeFieldPlan& plan, NativeExecutionReport report)
        : binding_(plan), report_(std::move(report)) {}
    storage::NativeFieldBinding binding_;
    NativeExecutionReport report_;
    friend PreparedNativeField prepare_field_action(const storage::PinnedNativeAccession&,
        const storage::NativeFieldRequest&, const storage::NativeFieldRealization&, const runtime::ExecutionHostPolicy&);
    friend class DerivedNativeField;
    friend DerivedNativeField execute(const PreparedNativeField&, const runtime::ExecutionLease&);
    friend NativeFieldWorkspace make_workspace(const PreparedNativeField&, const runtime::ExecutionLease&);
    friend DerivedNativeField execute_prepared(const PreparedNativeField&, NativeFieldWorkspace&, const runtime::ExecutionLease&);
};

namespace native_field_detail {
struct Inspection { storage::NativeFieldPlan plan; NativeExecutionReport report; };
inline Inspection inspect_field_action(const storage::PinnedNativeAccession& publication,
    const storage::NativeFieldRequest& request, const storage::NativeFieldRealization& realization = {},
    const runtime::ExecutionHostPolicy& host = {}) {
    // Resolve capabilities before inspection; a label never creates a backend.
    const auto backend = runtime::resolve_reference_backend(host);
    if(!host.allow_allocation) throw std::invalid_argument("reference preparation requires owning allocations");
    std::size_t records=request.fields.size();
    for(std::size_t k=0;k<publication.endpoint()->source.grade_count();++k) {
        const auto& arena=publication.endpoint()->source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        records=machine::checked_size_add(records,arena.cell_count());
        records=machine::checked_size_add(records,arena.incidence_count());
    }
    if(records>host.limits.records) throw runtime::ExecutionLimitExceeded("binding records",records,host.limits.records);
    const auto plan = storage::NativeFieldPlan::inspect(publication,request,realization);
    const auto& r = plan.requirements();
    const runtime::ExecutionRequirements requirements{r.records,r.rational_slots,r.sparse_entries,r.scalar_work,r.integer_bits};
    runtime::admit_execution(requirements,host);
    if(requirements.scalar_work > plan.accession_limits().max_action_work)
        throw runtime::ExecutionLimitExceeded("pinned accession field action work",requirements.scalar_work,plan.accession_limits().max_action_work);
    NativeExecutionReport report;
    report.source_namespace = plan.source_namespace(); report.sequence = plan.endpoint()->sequence;
    report.upper_grade = plan.upper_grade(); report.requirements = requirements;
    report.accession_limits = plan.accession_limits(); report.preparation_policy = host;
    report.selected_backend = backend; report.fallback = host.backend != backend;
    using K = runtime::DecisionKind;
    report.decisions = {
        {K::Declared,"arithmetic","request","1","exact-rational","rational temporal field declarations","request or publication"},
        {K::Declared,"maps","realization","1",realization.maps == storage::NativeFieldMaps::ExplicitOnly ? "explicit-occurrence-maps" : "declared-constant-fiber:"+std::to_string(realization.common_coordinates),
            "all retained maps admitted before zero shortcuts; realized and coefficient-independent roles preserved","carriers, maps or source"},
        {K::Derived,"layout","retained-source","1","checked-cell-slices-and-occurrences","stable identities, selected carriers and complete field support","publication or request"},
        {K::Profile,"kernel",host.name,"1",report.implementation,"registered serial CPU reference; no measured optimizer choice","host implementation"},
        {K::Profile,"allocation-and-growth",host.name,"2","checked-reference-and-prepared-envelope","both owners, compact schedule and scratch charged; inherited F3 work cap; no process/GMP quota","host caps or publication"},
        {K::Declared,"fallback","host-policy","1",report.fallback ? "explicit-exact-CPU-fallback; GPU implementation absent" : "none",
            "required absent implementation rejects","host backend registry or policy"},
        {K::Derived,"output-support","selected-lower-stalks","1","complete-owned-unpublished","supported zero and cancellation retain cell support","publication or request"}
    };
    return {plan,std::move(report)};
}

} // namespace native_field_detail

inline PreparedNativeField prepare_field_action(const storage::PinnedNativeAccession& publication,
    const storage::NativeFieldRequest& request, const storage::NativeFieldRealization& realization = {},
    const runtime::ExecutionHostPolicy& host = {}) {
    auto inspection = native_field_detail::inspect_field_action(publication,request,realization,host);
    return PreparedNativeField(inspection.plan,std::move(inspection.report));
}

// A result pins the originating immutable publication. No canonical field,
// directory entry or observation is installed by execution.
class DerivedNativeField {
public:
    const NativeExecutionReport& provenance() const & noexcept { return report_; }
    NativeExecutionReport provenance() && { return std::move(report_); }
    NativeExecutionReport provenance() const && { return report_; }
    const auto& carriers() const & noexcept { return outputs_; }
    std::vector<storage::NativeFieldCellBinding> carriers() && { return std::move(outputs_); }
    std::vector<storage::NativeFieldCellBinding> carriers() const && { return outputs_; }
    const numeric::RationalVector& values() const & noexcept { return values_; }
    numeric::RationalVector values() && { return std::move(values_); }
    numeric::RationalVector values() const && { return values_; }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return publication_; }
    std::size_t observed_integer_bits() const noexcept { return bits_; }
    const numeric::Rational& state(storage::StableCellKey cell, std::size_t coordinate) const & {
        const auto it = std::find_if(outputs_.begin(),outputs_.end(),[&](const auto& b){ return b.cell == cell; });
        if (it == outputs_.end()) throw std::out_of_range("cell outside derived lower support");
        if (coordinate >= it->dimension) throw std::out_of_range("derived field coordinate");
        return values_.at(it->offset+coordinate);
    }
    numeric::Rational state(storage::StableCellKey cell, std::size_t coordinate) const && {
        return std::as_const(*this).state(cell,coordinate);
    }
private:
    DerivedNativeField(const PreparedNativeField& operation, numeric::RationalVector values, std::size_t bits)
        : publication_(operation.binding().endpoint()), report_(operation.report()),
          outputs_(operation.binding().outputs()), values_(std::move(values)), bits_(bits) {}
    std::shared_ptr<const storage::TemporalEndpoint> publication_;
    NativeExecutionReport report_;
    std::vector<storage::NativeFieldCellBinding> outputs_;
    numeric::RationalVector values_;
    std::size_t bits_{};
    static DerivedNativeField finish(const PreparedNativeField&,numeric::RationalVector,const runtime::ExecutionLease&,bool);
    friend DerivedNativeField execute(const PreparedNativeField&,const runtime::ExecutionLease&);
    friend DerivedNativeField execute_prepared(const PreparedNativeField&,NativeFieldWorkspace&,const runtime::ExecutionLease&);
};

inline DerivedNativeField DerivedNativeField::finish(const PreparedNativeField& operation, numeric::RationalVector values,
                                                    const runtime::ExecutionLease& lease, bool prepared) {
    lease.check_cancelled();
    std::size_t bits = 0;
    for (const auto& q : values) bits = std::max(bits,storage::native_execution_detail::integer_bits(q));
    if (bits > lease.policy().limits.integer_bits)
        throw runtime::ExecutionLimitExceeded("result exact integer bits",bits,lease.policy().limits.integer_bits);
    auto result = DerivedNativeField(operation,std::move(values),bits);
    if(prepared) {
        result.report_.implementation="PreparedGradeFieldAction/GMP/serial-prepared-v1";
        auto& decision=result.report_.decisions.at(3);
        decision.origin="compiled-field-v1"; decision.value=result.report_.implementation;
        decision.constraints="retained occurrence blocks; target schedule; reusable operation-owned scratch; exact CPU";
    }
    // Record the effective execution lease, which may have different caps or
    // backend intent from preparation. This does not rebind mathematical owners.
    result.report_.fallback = lease.policy().backend != runtime::ExecutionBackend::ExactCpu;
    result.report_.execution_policy = lease.policy();
    result.report_.decisions.push_back({runtime::DecisionKind::Profile,"execution-lease",lease.policy().name,"1",
        result.report_.fallback ? "explicit-exact-CPU-fallback" : "serial-exact-CPU",
        "requirements readmitted; cancellation checked at synchronous boundaries","each execution"});
    lease.check_cancelled();
    return result;
}

// The reference entry point and inspectable intermediate APIs remain available.
inline DerivedNativeField execute(const PreparedNativeField& operation, const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) {
    lease.check_cancelled();
    runtime::admit_execution(operation.report().requirements,lease.policy());
    return DerivedNativeField::finish(operation,operation.binding_.apply_reference(),lease,false);
}

// Workspaces are movable, exclusive to one synchronous caller, and pin the same
// immutable owner as the operation. Copies of that operation share its identity.
inline NativeFieldWorkspace make_workspace(const PreparedNativeField& operation,
                                          const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) {
    lease.check_cancelled();
    runtime::admit_execution(operation.report().requirements,lease.policy());
    auto workspace=operation.binding_.make_workspace();
    lease.check_cancelled();
    return workspace;
}
inline DerivedNativeField execute_prepared(const PreparedNativeField& operation, NativeFieldWorkspace& workspace,
                                          const runtime::ExecutionLease& lease = runtime::ExecutionLease{}) {
    lease.check_cancelled();
    runtime::admit_execution(operation.report().requirements,lease.policy());
    return DerivedNativeField::finish(operation,operation.binding_.apply_prepared(workspace,lease.policy().limits.integer_bits),lease,true);
}

} // namespace rfc::database
