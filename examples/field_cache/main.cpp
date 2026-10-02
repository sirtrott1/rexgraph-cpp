#include "rfc/database/field_work.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::storage;
using namespace rfc::database;
NativeAccessionRequest field_request() { NativeAccessionRequest r; r.kind=NativeAccessionKind::Field; r.subject=201; return r; }
NativeFieldAnalysis inspect(const NativeAccessionDirectory& directory) {
    NativeFieldRequest request;request.stalk_carriers={10,11,101};request.fields={directory.pin(field_request())};
    return NativeFieldAnalysis::inspect(directory.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),
        request,NativeFieldRealization::constant_fiber(900));
}
CachedNativeField evaluate(runtime::CpuResourceCoordinator& resources,const NativeFieldAnalysis& analysis,
    const std::shared_ptr<NativeFieldCache>& cache) {
    auto run=runtime::WorkGraph({make_cached_field_work({1,1},analysis,cache,resources.capacity(),1,{},
        runtime::WorkAvailability{analysis.report().source_namespace,analysis.report().sequence})}).submit(resources);
    run.wait();return *run.completion({1,1}).result<CachedNativeField>();
}
int main() {
    try {
        core::RelationSignature signature;const auto type=signature.add_type("transfer",core::BoundaryPolicy::DeclaredExact,2,2);
        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(core::RelationKey{101},type,{{10,true,true,numeric::Rational{1},{},core::OccurrenceKey{1001}},
            {11,true,false,numeric::Rational{1},{},core::OccurrenceKey{1002}}});
        TemporalEndpoint source(builder.freeze());
        for(auto [grade,key]:{std::pair{0u,10u},std::pair{0u,11u},std::pair{1u,101u}})
            source.carriers.push_back({key,TemporalCarrierKind::CellStalk,core::Grade{static_cast<std::uint16_t>(grade)},
                StableCellKey{core::Grade{static_cast<std::uint16_t>(grade)},key},900,numeric::IndexDomain::linear(1)});
        TemporalTensor t;t.key=201;t.role=TemporalTensorRole::Field;t.owner.kind=TemporalOwnerKind::Cell;
        t.owner.cell={core::Grade{1},101};t.output=101;t.interpretation="declared rational transfer amount";
        t.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{2}}})};
        source.tensors.push_back(t);NativeAccessionDirectory directory(source,700);
        auto cache=std::make_shared<NativeFieldCache>();
        runtime::CpuResourceCoordinator resources(runtime::detect_hardware(),{4,1<<24,10000});
        const auto original=evaluate(resources,inspect(directory),cache);
        auto changed=source;changed.sequence=1;changed.tensors[0].tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{3}}})};
        directory.advance(make_temporal_segment(source,changed));const auto updated=evaluate(resources,inspect(directory),cache);
        auto unread=changed;unread.sequence=2;TemporalTensor note;note.key=202;note.interpretation="independent exact payload";unread.tensors.push_back(note);
        directory.advance(make_temporal_segment(changed,unread));const auto reused=evaluate(resources,inspect(directory),cache);
        const auto reference=prepare_field_action(directory.pin(field_request()),NativeFieldRequest{core::Grade{1},{10,11,101},{directory.pin(field_request())}},NativeFieldRealization::constant_fiber(900));
        if(original.values()!=numeric::RationalVector({-2,2}) || updated.values()!=numeric::RationalVector({-3,3}) ||
            !updated.reuse().plan || !reused.reuse().result || reused.values()!=execute(reference).values() ||
            reused.provenance().sequence!=2 || reused.source_publication()!=directory.pin_snapshot())return 1;
        const auto stats=cache->stats();
        if(stats.workspace_creations!=1 || stats.workspace_hits!=1 || !original.producer_execution())return 1;
        std::cout<<"Exact field: "<<reused.values()[0]<<", "<<reused.values()[1]<<"; native coordinates "
                 <<reused.analysis().layout().source_coordinates<<" -> "<<reused.analysis().layout().target_coordinates
                 <<"; compilations "<<stats.compiled_plans<<", applications "<<stats.applications
                 <<", plan hits "<<stats.plan_hits<<", result hits "<<stats.result_hits
                 <<", scratch creations "<<stats.workspace_creations<<", scratch hits "<<stats.workspace_hits
                 <<"; current sequence 2, original pin retained\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
