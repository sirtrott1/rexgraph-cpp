#include "temporal_product_fixture.hpp"
#include "product_measure.hpp"
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sys/resource.h>

#include "product_allocation_overrides.hpp"
using namespace rfc_temporal_product;
namespace {
std::size_t number(const char* text,std::size_t min,std::size_t max) {
    const std::string_view s(text);std::size_t value{};const auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),value);
    if(ec!=std::errc{} || p!=s.data()+s.size() || value<min || value>max)throw std::invalid_argument("numeric argument out of range");
    return value;
}
std::ofstream file(const std::filesystem::path& path,const char* header) {
    std::ofstream out(path);out.exceptions(std::ios::badbit|std::ios::failbit);out<<header<<'\n';return out;
}
std::size_t innovation_nonzeros(const TemporalSegment& s) {
    std::size_t n=0;for(const auto& t:s.tensors)if(t.target.role==TemporalTensorRole::Field)n+=t.innovation.nonzeros();return n;
}
void emit_gram(std::ostream& out,const char* name,const NativeSeriesMoments& result) {
    for(std::size_t i=0;i<result.gram.rows();++i)for(std::size_t j=0;j<result.gram.cols();++j)
        out<<name<<','<<i<<','<<j<<','<<result.gram.get(i,j).get_str()<<'\n';
}
void verify_boundary(Checks& check,const PinnedNativeAccession& chain,const PinnedNativeAccession& boundary,std::size_t n,std::size_t step) {
    std::map<std::uint64_t,Q> expected;
    for(const auto& leg:legs(n,step))if(leg.key.upper_grade.value==1) {
        auto& x=expected[leg.lower.key];if(leg.active)x+=leg.coefficient*chain.state(leg.upper.key).value();
    }
    check(boundary.support_count()==expected.size(),"boundary support including inactive/supported zeros");
    for(const auto& [key,x]:expected)check(boundary.state(key)==std::optional<Q>{x},"action-first exact boundary");
}
void trial_run(const std::filesystem::path& output,std::size_t n,std::size_t trial,Checks& check,
               std::ostream& times,std::ostream& counts,std::ostream& snapshots,std::ostream& checkpoint_counts,
               std::ostream& routes,std::ostream& grams) {
    Measurements time(times,n,trial);
    auto base=time.run("fixture_generation",0,[&]{return endpoint(n,0);});
    const auto source_bytes=time.run("source_encode",0,[&]{return encode_rfcx(base.source).bytes;});
    const auto channels=time.run("endpoint_encode",0,[&]{return encode_temporal_endpoint(base);});
    const auto data=output/("trial_"+std::to_string(trial));std::filesystem::create_directory(data);
    time.run("source_channel_write",0,[&]{save(data/"base.rfcx",source_bytes);save(data/"base.rfcxe",channels);return 0;});
    const auto local_stats=time.run("local_write",0,[&]{return write_local_blocks(base.source,encode_rfcx(base.source).stats.payload_hash,base.local_blocks,data/"base.rfcxl");});
    auto mapped=time.run("mapped_open_validate",0,[&]{return std::make_unique<MappedRfcx>(data/"base.rfcx");});
    auto local=time.run("local_open_validate",0,[&]{auto p=std::make_unique<MappedLocalBlocks>(data/"base.rfcxl");p->validate_against(*mapped);return p;});
    check(local->shared_between(1,2)==std::vector<StableCellKey>{cell(0,1)},"shared identity stored once");
    auto current=time.run("endpoint_read_reconstruct",0,[&]{return read_temporal_endpoint(*mapped,data/"base.rfcxe");});
    same_endpoint(check,current,base);verify_declared_program(check,n,0,current);
    auto series=time.run("series_construct",0,[&]{return NativeTemporalSeries(current,source_namespace);});
    auto index=time.run("defect_index_build",0,[&]{return std::make_unique<NativeDefectStore>(current,source_namespace,realization(current),queries(),NativeDefectLimits{},observations());});
    auto directory=time.run("accession_index_build",0,[&]{return std::make_unique<NativeAccessionDirectory>(current,source_namespace,observations());});
    const auto request=NativeAccessionRequest::chain(10,core::Grade{1});
    auto handle=time.run("descriptor_intern",0,[&]{return directory->intern(request);});
    const auto initial_pin=directory->resolve(handle,NativeAccessionKind::Chain);
    const auto stats_before=directory->stats();
    const auto sequence_sum=time.run("known_handle_10000",0,[&]{std::uint64_t sum=0;for(std::size_t k=0;k<10000;++k)sum+=directory->resolve(handle,NativeAccessionKind::Chain).sequence()+1;return sum;});
    check(sequence_sum==10000,"known handle results used");
    const auto stats_after=directory->stats();
    check(stats_before.descriptor_hashes==stats_after.descriptor_hashes && stats_before.bucket_comparisons==stats_after.bucket_comparisons &&
          stats_before.cold_bindings==stats_after.cold_bindings && stats_before.indexed_records==stats_after.indexed_records,"known handle performs no key construction or indexing");
    auto chain=directory->resolve(handle,NativeAccessionKind::Chain);
    const auto boundary=time.run("boundary_action",0,[&]{return chain.boundary();});verify_boundary(check,chain,boundary,n,0);
    auto cochain_request=NativeAccessionRequest::sparse_chain(core::Grade{0},{},{});
    cochain_request.kind=NativeAccessionKind::Cochain;cochain_request.pairing=NativePairing::AlgebraicDual;cochain_request.all_cells=true;
    for(const auto key:current.source.grade(core::Grade{0}).identities)cochain_request.coefficients.push_back({key,Q{static_cast<long>(key%5)}});
    const auto f=directory->pin(cochain_request),df=f.coboundary();
    check(df.pair_with(chain)==f.pair_with(boundary),"dual chain/cochain law");
    const auto program=composite::compile_active_orientation_query(current.source.signature().existence_aspect(),current.source.signature().orientation_aspect(),false);
    const auto plan=time.run("packed_plan_prepare",0,[&]{return composite::BinaryExecutionPlan(program,{2});});
    composite::BinaryExecutionWorkspace workspace;
    (void)mapped->binary_count(core::Grade{1},plan,workspace);
    const auto mask_count=time.run("packed_count_warm",0,[&]{return mapped->binary_count(core::Grade{1},plan,workspace);});
    std::size_t expected_positive=0;for(const auto& leg:legs(n,0))if(leg.key.upper_grade.value==1 && leg.active && leg.coefficient>=0)++expected_positive;
    check(mask_count==expected_positive,"mapped composite-binary exact count");
    if(trial==0)emit_routes(routes,index->pin());
    const auto initial_defect=index->pin();
    std::size_t stream_bytes=0;
    for(std::size_t step=1;step<=last_step;++step) {
        auto target=time.run("fixture_generation",step,[&]{return endpoint(n,step);});
        const auto update=time.run("delta_construct",step,[&]{return segment(current,target);});
        const auto frame=time.run("segment_encode",step,[&]{return encode_temporal_segment(update);});stream_bytes+=frame.size();
        const auto parsed=time.run("segment_decode",step,[&]{return decode_temporal_segment(frame);});
        const auto replay=time.run("endpoint_replay",step,[&]{return replay_temporal_segment(current,parsed);});same_endpoint(check,replay,target);
        time.run("series_append",step,[&]{series.append(parsed);return 0;});
        const auto previous=index->pin();
        const auto incremental=time.run("defect_advance_replay_and_E",step,[&]{return index->advance(parsed,realization(target));});
        const auto fresh=time.run("defect_full_advance_reference",step,[&]{auto reference=replay_temporal_segment(current,parsed);return std::make_unique<NativeDefectStore>(reference,source_namespace,realization(reference),queries(),NativeDefectLimits{},observations());});
        const auto a=index->pin(),b=fresh->pin();same_endpoint(check,*a.endpoint(),target);
        const auto series_pin=series.tail();same_endpoint(check,series_pin.endpoint(),target);
        verify_word(check,step,previous,a);verify_routes(check,n,step,a);verify_declared_program(check,n,step,*a.endpoint());
        check(a.route_keys()==b.route_keys(),"incremental/full route identities");
        for(const auto key:b.route_keys())check(a.route(key).contribution==b.route(key).contribution,"incremental/full route matrices");
        for(const auto& q:queries())check(a.certificate(q.key).sums==b.certificate(q.key).sums,"incremental/full chart sums");
        for(const auto id:std::vector<RelationalCodeIdentity>{{1,1},{2,3},{3,1},{3,2}})
            check(index->observation(id)->bytes==fresh->observation(id)->bytes,"incremental/full observations");
        check(index->observation({1,1})->compatible==std::optional<bool>{!deformed(step)},"compatible/obstructed gluing context");
        if(step==1 || step==5 || step==7 || step==8 || step==12 || step==13)check(incremental.route_products==0,"unrelated channels do not recompute routes");
        if(step==9)check(innovation_nonzeros(update)==0,"relation birth remains separate from field innovation");
        time.run("accession_refresh_replay_and_E",step,[&]{return directory->advance(parsed);});
        check.rejects([&]{directory->resolve(handle,NativeAccessionKind::Chain);},"stale handle rejects");
        handle=directory->handle_at(0);
        const auto accessed=directory->resolve(handle,NativeAccessionKind::Chain);
        check(accessed.state(100)==target.tensors[0].tensor.state(resolve_stable_cell(target.source,cell(1,100))->index),"refreshed source-grade chain value");
        if(step%4==0)time.run("checkpoint_create",step,[&]{series.checkpoint(step);return 0;});
        const auto glued=time.run("strict_glue_materialize",step,[&]{return materialize_glued_strict_blocks(target.source,std::span<const LocalBlockDefinition>(target.local_blocks).first(2));});
        check(encode_rfcx(glued.complex).bytes==encode_rfcx(target.source).bytes,"glued retained source, including empty grade and inactive fibers");
        std::size_t births=0,removals=0,born_occ=0,removed_occ=0,words=0,bits=0,coefficient_edits=0;
        for(const auto& g:update.source.grades) {
            for(const auto& c:g.cells)births+=c.birth;
            removals+=g.removed_cells.size();born_occ+=g.born_occurrences.size();removed_occ+=g.removed_occurrences.size();
            words+=g.aspect_xor.size();for(const auto& w:g.aspect_xor)bits+=static_cast<std::size_t>(std::popcount(w.bits));
            for(const auto& o:g.occurrences)coefficient_edits+=o.coefficient.has_value();
        }
        const auto source_image=time.run("source_encode",step,[&]{return encode_rfcx(target.source).bytes;});
        const auto channel_image=time.run("endpoint_encode",step,[&]{return encode_temporal_endpoint(target);});
        std::size_t max_bits=0;
        for(const auto& t:target.tensors)for(const auto& v:t.tensor.values().entries())max_bits=std::max(max_bits,mpz_sizeinbase(mpq_numref(v.value.get_mpq_t()),2));
        const auto obs=index->observation_stats();
        counts<<n<<','<<trial<<','<<step<<','<<events[step]<<','<<occurrence_count(target)<<','<<births<<','<<removals<<','<<born_occ<<','<<removed_occ<<','
            <<words<<','<<bits<<','<<coefficient_edits<<','<<innovation_nonzeros(update)<<','<<a.route_count()<<','<<b.stats().route_products<<','
            <<incremental.route_products<<','<<incremental.reused_routes<<','<<incremental.copied_blocks<<','<<incremental.scanned_occurrences<<','
            <<obs.evaluated<<','<<obs.bytes_changed<<','<<static_cast<unsigned>(a.certificate(1).status())<<','<<static_cast<unsigned>(a.certificate(2).status())<<','<<max_bits<<'\n';
        snapshots<<n<<','<<trial<<','<<step<<','<<source_image.size()<<','<<channel_image.size()<<','<<frame.size()<<'\n';
        if(trial==0) {
            emit_routes(routes,a);
            time.run("fixture_files_write",step,[&]{save(data/("frame_"+std::to_string(step)+".rfcxt"),frame);return 0;});
        }
        current=std::move(target);
    }
    check(initial_pin.state(100)!=directory->resolve(handle,NativeAccessionKind::Chain).state(100),"old accession pin retains its field");
    check(initial_defect.flat(core::Grade{2}),"old route pin survives every update");
    const auto history=time.run("series_encode",last_step,[&]{return encode_native_series(series);});
    time.run("series_write",last_step,[&]{save(data/"history.rfcxs",history);return 0;});
    const auto read=time.run("series_file_read",last_step,[&]{std::ifstream in(data/"history.rfcxs",std::ios::binary|std::ios::ate);const auto size=in.tellg();if(size<0)throw std::runtime_error("series read size");Bytes bytes(static_cast<std::size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),size);if(!in)throw std::runtime_error("series read");return bytes;});
    check(read==history,"disk byte equality");
    auto loaded=time.run("series_decode_validate_all",last_step,[&]{return decode_native_series(read);});
    check(loaded.same_history(series),"complete ordered history equal after disk");
    const auto cp=time.run("checkpoint_at_15",15,[&]{return loaded.at(15);});
    const auto full=time.run("base_at_15",15,[&]{return loaded.at(15,true);});same_endpoint(check,cp.endpoint(),full.endpoint());same_endpoint(check,cp.endpoint(),endpoint(n,15));
    check(cp.stats().replayed_segments==3 && full.stats().replayed_segments==15,"checkpoint replay work");
    const auto interval=loaded.difference(0,last_step);
    const auto moments=time.run("multi_time_moments",last_step,[&]{return interval.moments({{20,{}}},21);});
    const auto typed=time.run("typed_moments",last_step,[&]{return interval.moments({{20,22}},23);});
    const auto samples=time.run("sampled_moments",last_step,[&]{return interval.sample_moments({{0,20,{}},{8,20,{}},{16,20,{}}},21);});
    Q sum=0,typed_sum=0;
    for(std::size_t i=0;i<moments.gram.rows();++i)for(std::size_t j=0;j<moments.gram.cols();++j)sum+=moments.gram.get(i,j);
    for(std::size_t i=0;i<typed.gram.rows();++i)for(std::size_t j=0;j<typed.gram.cols();++j)typed_sum+=typed.gram.get(i,j);
    check(sum==Q{50,49} && typed_sum==Q{25,49},"complete multi-time cross terms");
    if(trial==0) {emit_gram(grams,"innovation",moments);emit_gram(grams,"typed",typed);emit_gram(grams,"samples",samples);}
    time.run("checkpoint_repack",last_step,[&]{loaded.repack_checkpoints({4,12});return 0;});
    check(loaded.same_history(series),"physical compaction never replaces history");
    const auto repacked=time.run("repacked_encode",last_step,[&]{return encode_native_series(loaded);});
    check(repacked.size()<history.size(),"checkpoint storage reclaimed");
    checkpoint_counts<<n<<','<<trial<<','<<source_bytes.size()<<','<<channels.size()<<','<<local_stats.bytes<<','<<stream_bytes<<','
        <<history.size()<<','<<repacked.size()<<','<<full.stats().replayed_segments<<','<<cp.stats().replayed_segments<<'\n';
    if(trial==0) {save(data/"repacked.rfcxs",repacked);save(data/"target.rfcx",encode_rfcx(current.source).bytes);save(data/"target.rfcxe",encode_temporal_endpoint(current));}
    // Deliberate collision load is measured separately, not hidden behind an
    // expected/amortized constant-time assertion. Bound it for this corpus.
    auto collisions=time.run("collision_index_build",last_step,[&]{return std::make_unique<NativeAccessionDirectory>(current,source_namespace,std::vector<RelationalCodeObservationQuery>{},[](auto){return std::uint64_t{0};});});
    for(std::size_t i=0;i<std::min(n,std::size_t{256});++i) {
        auto r=request;r.all_cells=false;r.cells={edge_key(i,0)};collisions->intern(r);
    }
    auto last=request;last.all_cells=false;last.cells={edge_key(std::min(n,std::size_t{256})-1,0)};
    const auto before=collisions->stats().bucket_comparisons;
    const auto found=time.run("collision_exact_lookup",last_step,[&]{return collisions->find(last);});
    check(found.has_value(),"collision-resolved descriptor found");
    check(collisions->stats().bucket_comparisons-before==std::min(n,std::size_t{256}),"collision work published");
}
}
int main(int argc,char** argv) {
    try {
        if(argc<2 || argc>4)throw std::invalid_argument("usage: rfc_temporal_product NEW_DIRECTORY [TRIANGLES=128 (2..512)] [TRIALS=3 (1..11)]");
        const auto n=argc>2?number(argv[2],2,512):128;
        const auto trials=argc>3?number(argv[3],1,11):3;
        const std::filesystem::path out(argv[1]);
        if(!std::filesystem::create_directory(out))throw std::invalid_argument("fresh output directory required");
        auto times=file(out/"timings.csv","triangles,trial,step,operation,nanoseconds,cpp_allocations,cpp_requested_bytes");
        auto counts=file(out/"work.csv","triangles,trial,step,event,retained_occurrences,born_cells,removed_cells,born_occurrences,removed_occurrences,xor_words,xor_bits,coefficient_edits,innovation_nonzeros,routes,fresh_products,updated_products,reused_routes,copied_blocks,scanned_occurrences,observations_evaluated,observation_bytes_changed,complete_status,partial_status,max_numerator_bits");
        auto snapshots=file(out/"storage.csv","triangles,trial,step,source_bytes,endpoint_bytes,segment_bytes");
        auto checkpoints=file(out/"checkpoints.csv","triangles,trial,base_source_bytes,base_endpoint_bytes,local_index_bytes,ordered_segment_bytes,archive_bytes,repacked_bytes,base_replays,checkpoint_replays");
        auto routes=file(out/"routes.csv","step,upper_occ,lower_occ,upper,middle,lower,active,m00,m01,m10,m11");
        auto grams=file(out/"moments.csv","kind,row,column,value");
        Checks check;
        for(std::size_t trial=0;trial<trials;++trial)trial_run(out,n,trial,check,times,counts,snapshots,checkpoints,routes,grams);
        for(auto* stream:{&times,&counts,&snapshots,&checkpoints,&routes,&grams})stream->close();
        rusage usage{};if(getrusage(RUSAGE_SELF,&usage)!=0)throw std::runtime_error("process high-water usage unavailable");
        auto summary=file(out/"summary.json","{");
        summary<<"  \"format\": \"RFCX_PHASE06_PRODUCT_V1\",\n  \"triangles\": "<<n<<",\n  \"trials\": "<<trials<<",\n  \"endpoints\": 17,\n  \"checks\": "<<check.count
               <<",\n  \"max_rss_kib_linux\": "<<usage.ru_maxrss<<",\n  \"canonical_history_preserved\": true,\n  \"exact_reference_equal\": true,\n  \"cold_os_cache_controlled\": false,\n  \"timing_instrumented\": true\n}\n";
        summary.close();
        std::cout<<"triangles="<<n<<" endpoints=17 trials="<<trials<<" exact_checks="<<check.count<<" exact_disk_replay=true partial_is_not_curvature=true canonical_history_preserved=true\n";return 0;
    }catch(const std::exception& e){product_allocations::active=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
