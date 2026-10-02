#pragma once

#include "rfc_preview/sample.hpp"
#include "rfc/database/native_execution.hpp"
#include <iomanip>
#include <iostream>

namespace rfc_preview {

// A separate synthetic fixture profile. Existing sample/archive bytes are not
// recoded. These scalar stalks and fields are retained in the new native history.
inline rfc::storage::TemporalEndpoint action_sample_endpoint(std::uint64_t sequence) {
    using namespace rfc;
    using namespace rfc::storage;
    auto e=sample::endpoint(sequence);
    for(std::size_t k=0;k<e.source.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        const auto& arena=e.source.grade(grade);
        for(std::size_t i=0;i<arena.cell_count();++i) {
            const auto cell=stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)});
            require(k<100 && cell.key<10000,"action sample identity outside declared profile");
            const auto carrier=1000000+k*10000+cell.key;
            e.carriers.push_back({carrier,TemporalCarrierKind::CellStalk,grade,cell,9000001,numeric::IndexDomain::linear(1)});
            if(k!=1)continue;
            TemporalTensor field;field.key=5000000+cell.key;field.role=TemporalTensorRole::Field;field.output=carrier;
            field.owner.kind=TemporalOwnerKind::Cell;field.owner.cell=cell;
            field.interpretation="sample-action-v1 scalar amplitude; common coordinate 9000001; synthetic input";
            const numeric::Rational amplitude=cell.key==200?numeric::Rational{3,static_cast<unsigned long>(7+sequence)}:numeric::Rational{0};
            field.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,amplitude}})};
            e.tensors.push_back(field);
        }
    }
    validate_temporal_endpoint(e);return e;
}
inline rfc::storage::NativeTemporalSeries action_sample_history(std::uint64_t steps) {
    using namespace rfc::storage;
    require(steps<=64,"action sample step budget");auto prev=action_sample_endpoint(0);
    NativeTemporalSeries history(prev,sample::namespace_key);
    for(std::uint64_t t=1;t<=steps;++t) {
        auto next=action_sample_endpoint(t);auto declarations=make_temporal_declaration_update(prev.declarations,next.declarations);
        history.append(make_temporal_segment(prev,next,{}, {},RelationalCodeEncoding::Automatic,{},declarations));prev=std::move(next);
    }
    return history;
}

inline rfc::database::PreparedNativeField prepare_field_diagnostic(
    const Database& db,std::uint64_t sequence,rfc::core::Grade grade,
    std::uint64_t coordinates,const rfc::runtime::ExecutionHostPolicy& policy) {
    using namespace rfc;
    using namespace rfc::storage;
    auto owner=db.series().at(sequence);
    NativeAccessionDirectory directory(owner.endpoint(),owner.source_namespace());
    auto anchor=directory.pin(NativeAccessionRequest::sparse_chain(grade,{},{}));
    NativeFieldRequest request;request.upper_grade=grade;
    for(const auto& c:directory.pin_snapshot()->carriers)if(c.kind==TemporalCarrierKind::CellStalk)request.stalk_carriers.push_back(c.key);
    for(const auto& t:directory.pin_snapshot()->tensors)
        if(t.role==TemporalTensorRole::Field && t.owner.kind==TemporalOwnerKind::Cell && t.owner.cell.grade==grade) {
            NativeAccessionRequest r;r.kind=NativeAccessionKind::Field;r.subject=t.key;request.fields.push_back(directory.pin(r));
        }
    const auto realization=coordinates?NativeFieldRealization::constant_fiber(coordinates):NativeFieldRealization{};
    return database::prepare_field_action(anchor,request,realization,policy);
}

// A synchronous session owns the prepared action and its exclusive scratch.
// Database lifetime and later publication do not affect its pinned endpoint.
class PreparedFieldDiagnostic {
public:
    PreparedFieldDiagnostic(const Database& db,std::uint64_t sequence,rfc::core::Grade grade,
                            std::uint64_t coordinates,const rfc::runtime::ExecutionHostPolicy& policy)
        : operation_(prepare_field_diagnostic(db,sequence,grade,coordinates,policy)),
          workspace_(rfc::database::make_workspace(operation_,rfc::runtime::ExecutionLease(policy))) {}
    rfc::database::DerivedNativeField execute(const rfc::runtime::ExecutionLease& lease = rfc::runtime::ExecutionLease{}) {
        return rfc::database::execute_prepared(operation_,workspace_,lease);
    }
    const rfc::database::PreparedNativeField& operation() const & { return operation_; }
    const rfc::database::PreparedNativeField& operation() const && = delete;
private:
    rfc::database::PreparedNativeField operation_;
    rfc::database::NativeFieldWorkspace workspace_;
};

inline void execute_field_diagnostic(const Database& db,std::uint64_t sequence,rfc::core::Grade grade,
                                     std::uint64_t coordinates,const rfc::runtime::ExecutionHostPolicy& policy) {
    using namespace rfc;
    using namespace rfc::storage;
    PreparedFieldDiagnostic session(db,sequence,grade,coordinates,policy);
    const auto& operation=session.operation();
    const auto result=session.execute(runtime::ExecutionLease(policy));
    const auto& report=result.provenance();const auto& limits=report.requirements;
    std::ostringstream out;
    out<<"{\"source_namespace\":"<<report.source_namespace<<",\"sequence\":"<<report.sequence<<",\"grade\":"<<grade.value
       <<",\"publication_sha256\":\""<<hex(db.publication_digest())<<"\",\"implementation\":"<<std::quoted(report.implementation)
       <<",\"backend\":\"exact-cpu\",\"fallback\":"<<(report.fallback?"true":"false")<<",\"published\":false"
       <<",\"requirements\":{\"records\":"<<limits.records<<",\"rational_slots\":"<<limits.rational_slots<<",\"sparse_entries\":"<<limits.sparse_entries
       <<",\"scalar_work\":"<<limits.scalar_work<<",\"integer_bits\":"<<limits.integer_bits<<"},\"observed_integer_bits\":"<<result.observed_integer_bits();
    out<<",\"accession_max_action_work\":"<<report.accession_limits.max_action_work
       <<",\"host_policy\":"<<std::quoted(policy.name)<<",\"host_limits\":{\"records\":"<<policy.limits.records
       <<",\"rational_slots\":"<<policy.limits.rational_slots<<",\"sparse_entries\":"<<policy.limits.sparse_entries
       <<",\"scalar_work\":"<<policy.limits.scalar_work<<",\"integer_bits\":"<<policy.limits.integer_bits<<'}';
    out<<",\"decisions\":[";bool comma=false;
    for(const auto& decision:report.decisions) {
        if(comma)out<<',';
        comma=true;
        const auto kind=decision.kind==runtime::DecisionKind::Declared?"Declared":decision.kind==runtime::DecisionKind::Derived?"Derived":decision.kind==runtime::DecisionKind::Profile?"Profile":"Measured";
        out<<"{\"kind\":"<<std::quoted(kind)<<",\"choice\":"<<std::quoted(decision.choice)<<",\"origin\":"<<std::quoted(decision.origin)
           <<",\"version\":"<<std::quoted(decision.version)<<",\"value\":"<<std::quoted(decision.value)<<",\"constraints\":"<<std::quoted(decision.constraints)<<",\"revalidate\":"<<std::quoted(decision.revalidate)<<'}';
    }
    out<<"],\"inputs\":[";comma=false;
    for(const auto& input:operation.binding().inputs()) {
        if(comma)out<<',';
        comma=true;
        out<<"{\"cell\":"<<input.cell.key<<",\"field\":"<<input.field<<",\"carrier\":"<<input.carrier<<",\"coordinates\":"<<input.coordinates<<",\"support\":"<<input.dimension<<'}';
    }
    out<<"],\"occurrences\":[";comma=false;
    for(const auto& occurrence:operation.binding().occurrences()) {
        if(comma)out<<',';
        comma=true;
        out<<"{\"key\":"<<occurrence.occurrence.key.value<<",\"upper\":"<<occurrence.upper.key<<",\"lower\":"<<occurrence.lower.key
           <<",\"input_carrier\":"<<occurrence.input_carrier<<",\"output_carrier\":"<<occurrence.output_carrier<<",\"active\":"<<(occurrence.active?"true":"false")
           <<",\"coefficient\":\""<<occurrence.coefficient<<"\",\"map\":";
        if(occurrence.map)out<<*occurrence.map;else out<<"null";
        out<<",\"role\":\""<<(occurrence.role==TemporalTensorRole::RealizedIncidenceMap?"realized":"coefficient-independent")
           <<"\",\"dependencies\":"<<occurrence.dependencies<<'}';
    }
    out<<"],\"output\":[";comma=false;
    for(const auto& carrier:result.carriers()) {
        if(comma)out<<',';
        comma=true;
        out<<"{\"cell\":"<<carrier.cell.key<<",\"carrier\":"<<carrier.carrier<<",\"coordinates\":"<<carrier.coordinates<<",\"support\":"<<carrier.dimension<<",\"values\":[";
        for(std::size_t i=0;i<carrier.dimension;++i) {if(i)out<<',';out<<'"'<<result.state(carrier.cell,i)<<'"';}
        out<<"]}";
    }
    out<<"]}\n";std::cout<<out.str();
}
} // namespace rfc_preview
