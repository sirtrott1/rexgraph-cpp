#include "../../benchmarks/temporal_product_fixture.hpp"
#include <iostream>
using namespace rfc_temporal_product;
int main(int argc,char** argv) {
    try {
        Checks check;
        std::optional<std::filesystem::path> out;
        if(argc>2)throw std::invalid_argument("usage: rfc_temporal_product_tests [NEW_DIRECTORY]");
        if(argc==2) {out=argv[1];if(!std::filesystem::create_directory(*out))throw std::invalid_argument("fresh directory required");}
        const std::size_t n=3;
        auto current=endpoint(n,0);
        NativeTemporalSeries series(current,source_namespace);
        NativeDefectStore index(current,source_namespace,realization(current),queries(),{},observations());
        const auto old=index.pin();const auto old_series=series.tail();
        verify_routes(check,n,0,old);verify_fresh(check,index,current);verify_declared_program(check,n,0,current);
        std::ofstream routes;
        if(out) {routes.open(*out/"routes.csv");routes<<"step,upper_occ,lower_occ,upper,middle,lower,active,m00,m01,m10,m11\n";emit_routes(routes,old);}
        for(std::size_t step=1;step<=last_step;++step) {
            auto target=endpoint(n,step);auto update=segment(current,target);
            if(step==13) {
                const auto identity=make_temporal_segment(current,target);
                check(encode_temporal_segment(update)!=encode_temporal_segment(identity),"equal endpoint, distinct operations retain different frames");
                same_endpoint(check,replay_temporal_segment(current,identity),target);
            }
            const auto frame=encode_temporal_segment(update);
            const auto decoded=decode_temporal_segment(frame);
            same_endpoint(check,replay_temporal_segment(current,decoded),target);
            const auto previous=index.pin();series.append(decoded);index.advance(decoded,realization(target));
            const auto pin=index.pin();same_endpoint(check,*pin.endpoint(),target);const auto series_pin=series.tail();same_endpoint(check,series_pin.endpoint(),target);
            verify_word(check,step,previous,pin);verify_routes(check,n,step,pin);verify_fresh(check,index,target);verify_declared_program(check,n,step,*pin.endpoint());
            if(step==1 || step==5 || step==7 || step==8 || step==12 || step==13)
                check(pin.stats().route_products==0,"non-route changes do not multiply routes");
            if(step==9) {
                check(pin.route_count()==15*(n+1),"retained zero-field birth routes");
                for(const auto& tensor:update.tensors)if(tensor.target.role==TemporalTensorRole::Field)
                    check(tensor.innovation.nonzeros()==0,"birth must not invent field innovation");
            }
            const auto wrong=segment(current,target);
            check.rejects([&]{index.advance(wrong);},"wrong predecessor rejected");same_endpoint(check,*index.pin().endpoint(),target);
            if(step%4==0)series.checkpoint(step);
            if(out) {emit_routes(routes,pin);save(*out/("frame_"+std::to_string(step)+".rfcxt"),frame);}
            current=std::move(target);
        }
        check(old.flat(core::Grade{2}),"old defect pin preserved");check(old_series.sequence()==0,"old series pin preserved");
        for(std::size_t step=0;step<=last_step;++step) {
            const auto cp=series.at(step),base=series.at(step,true);same_endpoint(check,cp.endpoint(),base.endpoint());
            same_endpoint(check,cp.endpoint(),endpoint(n,step));
            check(cp.stats().replayed_segments<=base.stats().replayed_segments,"checkpoint work accounting");
        }
        auto repacked=series;repacked.repack_checkpoints({4,12});check(repacked.same_history(series),"compaction retains all ordered bytes");
        auto bytes=encode_native_series(series),packed=encode_native_series(repacked);
        auto disk=decode_native_series(bytes),disk_packed=decode_native_series(packed);
        check(disk.same_history(series)&&disk_packed.same_history(series),"canonical series disk replay");
        check(encode_native_series(disk)==bytes,"canonical series bytes");
        const auto interval=disk.difference(0,last_step);const auto evolution=interval.field_evolution(20);
        check(evolution.accumulation.terminal==latent_x(last_step),"transported multi-time latent field");
        const auto gram=interval.moments({{20,{}}},21);
        const auto typed=interval.moments({{20,22}},23);
        Q total=0,typed_total=0;
        for(std::size_t i=0;i<gram.gram.rows();++i)for(std::size_t j=0;j<gram.gram.cols();++j)total+=gram.gram.get(i,j);
        for(std::size_t i=0;i<typed.gram.rows();++i)for(std::size_t j=0;j<typed.gram.cols();++j)typed_total+=typed.gram.get(i,j);
        check(total==Q{50,49},"all Gram cross terms sum to terminal quadrance");
        check(typed_total==Q{25,49},"typed accession moment");
        check.rejects([&]{disk.at(last_step+1);},"future endpoint rejected");
        check.rejects([&]{disk.difference(0,12).source_patch();},"signature collapse is not a direct source patch");
        NativeTemporalSeries prefix(endpoint(n,15),source_namespace);
        auto truncated=encode_temporal_segment(segment(endpoint(n,15),current));truncated.pop_back();
        const auto prefix_bytes=encode_native_series(prefix);
        check.rejects([&]{prefix.append_stream(truncated,7);},"truncated final frame rejects atomically");
        check(encode_native_series(prefix)==prefix_bytes,"failed stream publication preserved");
        if(out) {save(*out/"history.rfcxs",bytes);save(*out/"repacked.rfcxs",packed);save(*out/"target.rfcx",encode_rfcx(current.source).bytes);save(*out/"target.rfcxe",encode_temporal_endpoint(current));}
        std::cout<<check.count<<" checks passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
