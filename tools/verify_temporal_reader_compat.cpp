// Compile this unchanged source against F1 and F2 headers. It uses only F1 APIs.
#include "rfc/storage/temporal_seed.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    using namespace rfc;
    using namespace rfc::storage;
    namespace fs=std::filesystem;
    try {
        if(argc!=3)throw std::invalid_argument("usage: reader_compat generate|old-reject|new-replay DIRECTORY");
        const std::string mode(argv[1]);const fs::path directory(argv[2]);
        if(mode=="generate") {
            if(fs::exists(directory))throw std::invalid_argument("generator needs a fresh directory");
            fs::create_directories(directory);
            core::RelationSignature signature;const auto type=signature.add_type("retained");
            const auto source=[&](bool extra){
                core::RelationalComplexBuilder b(signature);b.declare_grade(core::Grade{2});
                b.add_grade_one(core::RelationKey{7},type,{{10,true,false,numeric::Rational{1},{},core::OccurrenceKey{100}},
                    {11,true,true,numeric::Rational{1},{},core::OccurrenceKey{101}}});
                if(extra)b.add_grade_one(core::RelationKey{8},type,{});
                return b.freeze();
            };
            TemporalEndpoint base(source(false)),target(source(true),1);
            const auto delta=make_temporal_segment(base,target);
            write_rfcx(base.source,directory/"legacy.rfcx");
            write_temporal_endpoint(base,directory/"legacy.rfcxe");
            append_temporal_segment(directory/"legacy.rfcxt",base,delta);
            const auto decoded=read_temporal_endpoint(MappedRfcx(directory/"legacy.rfcx"),directory/"legacy.rfcxe");
            const auto replay=replay_temporal_segment(decoded,read_temporal_segment(directory/"legacy.rfcxt"));
            if(encode_temporal_endpoint(replay)!=encode_temporal_endpoint(target))throw std::runtime_error("legacy replay differs");
            std::cout<<"legacy_source_seed_segment_replay=true\n";
        }else if(mode=="old-reject") {
            const MappedRfcx source(directory/"base.rfcx");
            const auto bare=read_temporal_endpoint(source,directory/"legacy.rfcxe");
            if(bare.sequence!=0)throw std::runtime_error("legacy sidecar rejected/misread");
            bool seed_rejected=false,segment_rejected=false;
            try{(void)read_temporal_endpoint(source,directory/"base.rfcxe");}catch(const std::exception&){seed_rejected=true;}
            try{(void)read_temporal_segment(directory/"migration.rfcxt");}catch(const std::exception&){segment_rejected=true;}
            if(!seed_rejected||!segment_rejected)throw std::runtime_error("old reader did not reject unsupported semantics");
            std::cout<<"legacy_sidecar=true new_seed_rejected=true new_segment_rejected=true\n";
        }else if(mode=="new-replay") {
            auto endpoint=read_temporal_endpoint(MappedRfcx(directory/"legacy.rfcx"),directory/"legacy.rfcxe");
            endpoint=replay_temporal_segment(endpoint,read_temporal_segment(directory/"legacy.rfcxt"));
            if(endpoint.sequence!=1||endpoint.source.grade(core::Grade{1}).cell_count()!=2)throw std::runtime_error("old source history not reconstructed");
            std::cout<<"old_writer_history_replayed=true\n";
        }else throw std::invalid_argument("unknown mode");
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
