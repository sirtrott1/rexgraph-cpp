#include "rfc/database/parallel_native_field.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <iostream>
#ifdef RFC_PROFILE_CPP_ALLOCATIONS
#include "cpu_allocation_hooks.hpp"
#endif

using namespace rfc;
using namespace rfc::storage;
using namespace rfc::database;
TemporalEndpoint make_source(std::size_t count,bool skew) {
    core::RelationSignature signature;const auto type=signature.add_type("load",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{1});
    for(std::uint64_t i=0;i<256;++i)builder.declare_witness_identity(10+i);
    for(std::size_t i=0;i<count;++i) {
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for(std::size_t j=0;j<8;++j)occurrences.push_back({10+(skew?0:(i*7+j)%256),true,j%2!=0,numeric::Rational{1},{},core::OccurrenceKey{50000+8*i+j}});
        builder.add_grade_one(core::RelationKey{1000+i},type,occurrences);
    }
    TemporalEndpoint e(builder.freeze());
    for(std::size_t k=0;k<e.source.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<e.source.grade(grade).cell_count();++i) {
            const auto key=stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)}).key;
            e.carriers.push_back({key,TemporalCarrierKind::CellStalk,grade,StableCellKey{grade,key},900,numeric::IndexDomain::linear(8)});
            if(k) {
                TemporalTensor t;t.key=1000000+i;t.role=TemporalTensorRole::Field;t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell={grade,key};t.output=key;
                std::vector<numeric::SparseRationalEntry> values;
                for(std::size_t j=0;j<8;++j)values.push_back({j,numeric::Rational{static_cast<long>((i+j)%7+1),11}});
                t.tensor={numeric::IndexDomain::linear(8),numeric::StructuralSupport::full(8),numeric::SparseRationalVector(8,values)};e.tensors.push_back(t);
            }
        }
    }
    return e;
}
NativeFieldRequest field_request(const NativeAccessionDirectory& d) {
    NativeFieldRequest r;for(const auto& c:d.pin_snapshot()->carriers)r.stalk_carriers.push_back(c.key);
    for(const auto& t:d.pin_snapshot()->tensors){NativeAccessionRequest q;q.kind=NativeAccessionKind::Field;q.subject=t.key;r.fields.push_back(d.pin(q));}return r;
}
NativeFieldAnalysis inspect(const NativeAccessionDirectory& d) {
    return NativeFieldAnalysis::inspect(d.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),field_request(d),NativeFieldRealization::constant_fiber(900));
}
template<class F> void measure(std::size_t relations,bool skew,const char* phase,std::size_t workers,std::size_t iterations,F&& f) {
    std::vector<long long> samples; samples.reserve(iterations);
    std::size_t calls{},bytes{};
    for(std::size_t i=0;i<iterations;++i) {
#ifdef RFC_PROFILE_CPP_ALLOCATIONS
        cpp_calls.store(0);cpp_bytes.store(0);accounting.store(true);
#endif
        const auto begin=std::chrono::steady_clock::now();f();
        const auto elapsed=std::chrono::steady_clock::now()-begin;
#ifdef RFC_PROFILE_CPP_ALLOCATIONS
        accounting.store(false);calls+=cpp_calls.load();bytes+=cpp_bytes.load();
#endif
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }
    std::cout<<"{\"relations\":"<<relations<<",\"skew\":"<<(skew?"true":"false")<<",\"phase\":\""<<phase
             <<"\",\"workers\":"<<workers<<",\"iterations\":"<<iterations<<",\"samples_ns\":[";
    for(std::size_t i=0;i<samples.size();++i)std::cout<<(i?",":"")<<samples[i];
    std::cout<<"],\"mode\":\""
#ifdef RFC_PROFILE_CPP_ALLOCATIONS
             <<"cpp-allocation"
#else
             <<"timing"
#endif
             <<"\",\"cpp_calls\":"<<calls<<",\"cpp_requested_bytes\":"<<bytes<<"}\n";
}
int main() {
    for(const auto& [n,skew]:{std::pair{0u,false},std::pair{64u,false},std::pair{1024u,false},std::pair{4096u,false},std::pair{1024u,true}}) {
        NativeAccessionLimits accession;accession.max_action_work=64000000;
        NativeAccessionDirectory directory(make_source(n,skew),888,{},[](auto bytes) { return rfcx_detail::fnv1a64(bytes); },accession);const auto analysis=inspect(directory);
        measure(n,skew,"analysis",0,3,[&]{const auto a=inspect(directory);if(!a.receipt().same_result(analysis.receipt()))throw std::runtime_error("analysis mismatch");});
        measure(n,skew,"prepare",0,3,[&]{const auto op=prepare_parallel_field(analysis,128);if(op.ranges().tiles().size()!=16)throw std::runtime_error("preparation mismatch");});
        const auto operation=prepare_parallel_field(analysis,128);
        measure(n,skew,"workspace",0,3,[&]{const auto workspace=make_parallel_workspace(operation);if(workspace.scratch_slots()!=2064)throw std::runtime_error("workspace mismatch");});
        const auto serial=prepare_field_action(directory.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{},{})),field_request(directory),NativeFieldRealization::constant_fiber(900));
        auto scratch=make_workspace(serial);const auto expected=execute_prepared(serial,scratch).values();
        measure(n,skew,"warm-prepared",1,7,[&]{const auto y=execute_prepared(serial,scratch);if(y.values()!=expected)throw std::runtime_error("serial mismatch");});
        for(const auto workers:{1u,2u,4u}) {
            runtime::CpuTileExecutor executor({workers,4,100});auto workspace=make_parallel_workspace(operation);
            measure(n,skew,"launch-only",workers,3,[&]{const auto run=executor.run_tiles(16,[](auto,auto,const auto&){});if(run.completed_tiles!=16)throw std::runtime_error("launch mismatch");});
            measure(n,skew,"warm-native-tiles",workers,7,[&]{const auto y=execute_parallel(operation,workspace,executor);if(y.values()!=expected || y.execution().completed_tiles!=16)throw std::runtime_error("tile mismatch");});
            measure(n,skew,"end-to-end",workers,3,[&]{const auto op=prepare_parallel_field(inspect(directory),128);auto ws=make_parallel_workspace(op);const auto y=execute_parallel(op,ws,executor);if(y.values()!=expected)throw std::runtime_error("full mismatch");});
        }
        const auto& stats=operation.ranges().action().statistics();
        if(stats.entries!=n*64 || stats.target_dimension!=2048 || stats.source_dimension!=n*8) return 1;
        std::cout<<"{\"relations\":"<<n<<",\"skew\":"<<(skew?"true":"false")<<",\"phase\":\"work\",\"entries\":"<<stats.entries
                 <<",\"retained_occurrences\":"<<stats.retained_occurrences<<",\"source_coordinates\":"<<stats.source_dimension
                 <<",\"target_coordinates\":"<<stats.target_dimension<<",\"tiles\":16,\"scratch_slots\":2064}\n";
    }
}
