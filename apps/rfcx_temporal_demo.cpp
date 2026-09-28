#include "rfc/storage/temporal_seed.hpp"
#include "rfc/storage/temporal_diagnostics.hpp"
#include <iostream>

int main(int argc,char** argv) {
    try {
        using namespace rfc;
        using namespace rfc::storage;
        using R=numeric::Rational;
        if(argc!=2){std::cerr<<"usage: rfc_rfcx_temporal_demo OUTPUT_DIRECTORY\n";return 2;}
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        for(const auto name:{"base.rfcx","base.rfcxe","history.rfcxt"})
            if(std::filesystem::exists(dir/name))throw std::runtime_error("demo refuses to overwrite existing files");
        core::RelationSignature sig;
        const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
        const auto empty=sig.add_type("empty",core::BoundaryPolicy::DeclaredExact,0,0);
        auto endpoint=[&](bool changed){
            core::RelationalComplexBuilder b(sig);
            b.add_grade_one(core::RelationKey{100},edge,{
                {10,true,!changed,R{1},{},core::OccurrenceKey{1000}},
                {11,true,changed,R{1},{},core::OccurrenceKey{1001}}});
            if(changed)b.add_grade_one(core::RelationKey{101},empty,{});
            TemporalEndpoint e(b.freeze(),changed?1:0);
            const auto n=e.source.grade(core::Grade{1}).cell_count();
            e.carriers.push_back({1,TemporalCarrierKind::SourceGrade,core::Grade{1},std::nullopt,0,numeric::IndexDomain::linear(n)});
            TemporalTensor t;t.key=1;t.role=TemporalTensorRole::Field;t.output=1;
            t.tensor={numeric::IndexDomain::linear(n),numeric::StructuralSupport::full(n),numeric::SparseRationalVector(n,{{0,R{2}}})};
            e.tensors.push_back(t);return e;
        };
        const auto old=endpoint(false),next=endpoint(true);
        const auto segment=make_temporal_segment(old,next);
        const auto source=write_rfcx(old.source,dir/"base.rfcx");
        const auto seed_bytes=write_temporal_endpoint(old,dir/"base.rfcxe");
        const auto delta_bytes=append_temporal_segment(dir/"history.rfcxt",old,segment);
        MappedRfcx mapped(dir/"base.rfcx");
        const auto reloaded=read_temporal_endpoint(mapped,dir/"base.rfcxe");
        const auto replayed=replay_temporal_segment(reloaded,read_temporal_segment(dir/"history.rfcxt"));
        if(temporal_endpoint_hash(replayed)!=temporal_endpoint_hash(next))throw std::logic_error("demo replay mismatch");
        std::cout<<"base_rfcx_bytes="<<source.bytes<<" endpoint_sidecar_bytes="<<seed_bytes<<" segment_bytes="<<delta_bytes<<'\n'
                 <<"replayed_sequence="<<replayed.sequence<<" retained_relations="<<replayed.source.grade(core::Grade{1}).cell_count()<<'\n'
                 <<"field_innovation_nonzeros="<<temporal_field_innovation(old,replayed,1).nonzeros()<<'\n'
                 <<"exact_disk_replay=true\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
