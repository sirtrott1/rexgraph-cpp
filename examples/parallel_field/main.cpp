#include "rfc/database/parallel_native_field.hpp"
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
        const auto analysis=inspect(directory);const auto operation=prepare_parallel_field(analysis,1);
        auto workspace=make_parallel_workspace(operation);
        runtime::CpuTileExecutor one({1,4,100}),four({4,4,100});
        const auto serial=execute_parallel(operation,workspace,one);
        const auto parallel=execute_parallel(operation,workspace,four);
        const auto reference=prepare_field_action(directory.pin(field_request()),
            NativeFieldRequest{core::Grade{1},{10,11,101},{directory.pin(field_request())}},NativeFieldRealization::constant_fiber(900));
        if(parallel.values()!=numeric::RationalVector({-2,2}) || parallel.values()!=serial.values() ||
            parallel.values()!=execute(reference).values() || parallel.execution().completed_tiles!=2 ||
            parallel.execution().worker_slots!=2 || parallel.execution().launched_threads!=1)return 1;
        auto changed=source;changed.sequence=1;changed.tensors[0].tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{3}}})};
        directory.advance(make_temporal_segment(source,changed));
        const auto updated=prepare_parallel_field(inspect(directory),1);auto updated_workspace=make_parallel_workspace(updated);
        const auto result=execute_parallel(updated,updated_workspace,four);
        if(result.values()!=numeric::RationalVector({-3,3}) || result.provenance().sequence!=1 ||
            parallel.provenance().sequence!=0 || execute_parallel(operation,workspace,four).values()!=serial.values())return 1;
        std::cout<<"Exact field: "<<result.values()[0]<<", "<<result.values()[1]
                 <<"; native coordinates 1 -> 2; requested workers 4, owner worker slots "<<result.execution().worker_slots
                 <<", child threads "<<result.execution().launched_threads<<", completed tiles "<<result.execution().completed_tiles
                 <<"; sequence 1, original operation retained\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
