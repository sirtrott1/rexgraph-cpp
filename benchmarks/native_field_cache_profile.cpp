#include "rfc/database/native_field_cache.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <iostream>
#include "exact_allocation_hooks.hpp"

using namespace rfc;
using namespace rfc::storage;
using namespace rfc::database;
TemporalEndpoint source(std::size_t count) {
    core::RelationSignature signature;const auto type=signature.add_type("transfer",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(signature);b.declare_grade(core::Grade{1});b.declare_witness_identity(10);b.declare_witness_identity(11);
    for(std::size_t i=0;i<count;++i)b.add_grade_one(core::RelationKey{1000+i},type,{{10,true,true,numeric::Rational{1},{},core::OccurrenceKey{5000+2*i}},
        {11,true,false,numeric::Rational{1},{},core::OccurrenceKey{5001+2*i}}});
    TemporalEndpoint e(b.freeze());
    for(std::size_t k=0;k<e.source.grade_count();++k)for(std::size_t i=0;i<e.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();++i) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};const auto key=stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)}).key;
        e.carriers.push_back({key,TemporalCarrierKind::CellStalk,grade,StableCellKey{grade,key},900,numeric::IndexDomain::linear(1)});
        if(k) {TemporalTensor t;t.key=100000+i;t.role=TemporalTensorRole::Field;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell={grade,key};t.output=key;
            t.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{1,3}}})};e.tensors.push_back(t);}
    }
    return e;
}
NativeFieldRequest request(const NativeAccessionDirectory& d) {
    NativeFieldRequest r;for(const auto& c:d.pin_snapshot()->carriers)r.stalk_carriers.push_back(c.key);
    for(const auto& t:d.pin_snapshot()->tensors){NativeAccessionRequest q;q.kind=NativeAccessionKind::Field;q.subject=t.key;r.fields.push_back(d.pin(q));}return r;
}
NativeFieldAnalysis inspect(const NativeAccessionDirectory& d) {
    return NativeFieldAnalysis::inspect(d.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),request(d),NativeFieldRealization::constant_fiber(900));
}
template<class F> void measure(std::size_t relations,const char* phase,std::size_t iterations,F&& f) {
    counts={};accounting=true;const auto begin=std::chrono::steady_clock::now();
    for(std::size_t i=0;i<iterations;++i)f();
    const auto elapsed=std::chrono::steady_clock::now()-begin;accounting=false;
    std::cout<<"{\"relations\":"<<relations<<",\"phase\":\""<<phase<<"\",\"iterations\":"<<iterations
             <<",\"nanoseconds\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()
             <<",\"cpp_calls\":"<<counts.cpp_calls<<",\"cpp_requested_bytes\":"<<counts.cpp_bytes
             <<",\"gmp_allocation_calls\":"<<counts.gmp_calls<<",\"gmp_requested_bytes\":"<<counts.gmp_bytes
             <<",\"gmp_reallocations\":"<<counts.gmp_reallocations<<",\"gmp_reallocation_bytes\":"<<counts.gmp_reallocation_bytes<<"}\n";
}
int main() {
    // Isolated, caller-only benchmark instrumentation, before any GMP objects.
    mp_set_memory_functions(gmp_allocate,gmp_reallocate,gmp_free);
    for(const auto n:{0u,1u,16u,128u,1024u}) {
        auto e=source(n);NativeAccessionDirectory d(e,888);auto a=inspect(d);
        measure(n,"analysis",5,[&]{const auto next=inspect(d);if(!next.receipt().same_result(a.receipt()))throw std::runtime_error("analysis mismatch");});
        NativeFieldCache cache;const auto expected=cache.evaluate(a).values();
        measure(n,"cold-cache",5,[&]{NativeFieldCache cold;const auto y=cold.evaluate(a);if(y.values()!=expected)throw std::runtime_error("cold mismatch");});
        auto op=prepare_field_action(d.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),request(d),NativeFieldRealization::constant_fiber(900));
        auto workspace=make_workspace(op);
        measure(n,"warm-prepared",30,[&]{const auto y=execute_prepared(op,workspace);if(y.values()!=expected)throw std::runtime_error("prepared mismatch");});
        measure(n,"warm-result",30,[&]{const auto y=cache.evaluate(a);if(y.values()!=expected || !y.reuse().result)throw std::runtime_error("warm mismatch");});
        if(n) {
            std::vector<NativeFieldAnalysis> changed;
            for(unsigned i=0;i<5;++i) {
                auto next=e;next.sequence=i+1;next.tensors[0].tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{static_cast<long>(i+2),3}}})};
                d.advance(make_temporal_segment(e,next));changed.push_back(inspect(d));e=std::move(next);
            }
            std::size_t cursor{};
            measure(n,"changed-field-plan-reuse",changed.size(),[&]{const auto y=cache.evaluate(changed.at(cursor++));if(!y.reuse().plan || y.reuse().result)throw std::runtime_error("plan not reused");});
            std::vector<numeric::RationalVector> expected_changed;
            for(const auto& analysis:changed)expected_changed.push_back(cache.evaluate(analysis).values());
            for(const bool retain:{false,true}) {
                NativeFieldCacheLimits limits;limits.idle_workspaces=retain?8:0;
                NativeFieldCache tiled(limits);runtime::CpuTileExecutor caller({1,1,100000});
                (void)tiled.evaluate_parallel(a,caller,1);cursor=0;
                measure(n,retain?"changed-field-retained-scratch":"changed-field-fresh-scratch",changed.size(),[&] {
                    const auto index=cursor++;const auto y=tiled.evaluate_parallel(changed.at(index),caller,1);
                    if(!y.reuse().plan || y.values()!=expected_changed.at(index))throw std::runtime_error("parallel cache mismatch");
                });
                const auto facts=tiled.stats();
                if(facts.workspace_creations!=(retain?1u:6u) || facts.workspace_hits!=(retain?5u:0u))return 1;
                std::cout<<"{\"relations\":"<<n<<",\"phase\":\"scratch-work\",\"retained\":"<<(retain?"true":"false")
                    <<",\"creations\":"<<facts.workspace_creations<<",\"hits\":"<<facts.workspace_hits
                    <<",\"idle_slots\":"<<facts.idle_workspace_slots<<",\"workers\":1}\n";
            }
        }
        const auto stats=cache.stats();
        if(stats.compiled_plans!=1 || stats.applications!=(n?6u:1u) || stats.result_hits!=(n?35u:30u) || stats.plan_hits!=(n?5u:0u))return 1;
        std::cout<<"{\"relations\":"<<n<<",\"phase\":\"work\",\"compilations\":"<<stats.compiled_plans
                 <<",\"applications\":"<<stats.applications<<",\"plan_hits\":"<<stats.plan_hits<<",\"result_hits\":"<<stats.result_hits<<"}\n";
    }
}
