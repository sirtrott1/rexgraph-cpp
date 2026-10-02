#include "rfc/storage/native_series_prefix.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include "rfc/models/native_temporal_context.hpp"
#include "rfc/temporal/chronological_prefix.hpp"
#include "../storage/native_series_fixture.hpp"
#include <iostream>
#include <random>
using namespace rfc;using namespace rfc::storage;using namespace native_series_fixture;
namespace {
unsigned checks{};
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected cutoff rejection");}
void same(const TemporalEndpoint& a,const TemporalEndpoint& b){check(encode_rfcx(a.source).bytes==encode_rfcx(b.source).bytes,"prefix source changed");check(encode_temporal_endpoint(a)==encode_temporal_endpoint(b),"prefix temporal data changed");}
void same(NativeSeriesEndpoint a,NativeSeriesEndpoint b){same(a.endpoint(),b.endpoint());}
void same(NativeSeriesEndpoint a,const TemporalEndpoint& b){same(a.endpoint(),b);}
void history() {
    PaperHistory h;NativeTemporalSeries a(h.a,800);a.append(h.first);auto b=a;
    a.append(h.second);auto adversarial=endpoint(42,{Q{999999},Q{-123456}});
    b.append(make_temporal_segment(h.b,adversarial,{{1,h.J1}}));a.repack_checkpoints({41,42});b.repack_checkpoints({42});
    auto p=NativeSeriesPrefix::capture(a,41);auto q=NativeSeriesPrefix::capture(b,41);
    same(p.tail(),q.tail());check(p.cutoff()==41 && p.segment_count()==1,"prefix metadata includes future");
    check(std::equal(p.segment_bytes(0).begin(),p.segment_bytes(0).end(),a.segment_bytes(0).begin(),a.segment_bytes(0).end()),"prefix rewrote ordered frame");
    for(std::uint64_t seq=40;seq<=41;++seq){same(a.at(seq),b.at(seq));same(p.at(seq),a.at(seq,true));check(a.at(seq).stats().starting_sequence<=seq,"future checkpoint chosen");}
    rejects([&]{(void)p.at(42);});rejects([&]{(void)p.difference(40,42);});rejects([&]{(void)p.segment_bytes(1);});
    rejects([&]{(void)NativeSeriesPrefix::capture(a,39);});rejects([&]{(void)NativeSeriesPrefix::capture(a,43);});
    auto empty=NativeSeriesPrefix::capture(a,40);check(empty.segment_count()==0,"zero prefix kept frame");
    same(empty.tail(),h.a);check(a.segment_count()==2 && b.segment_count()==2,"source history erased");
    auto evo=p.difference(40,41).field_evolution(10);check(evo.accumulation.terminal==Vec({3,1,6}),"past evolution changed");
    auto round=decode_native_series(encode_native_series(a));same(NativeSeriesPrefix::capture(round,41).tail(),p.tail());
    a.append(make_temporal_segment(h.c,endpoint(43,{Q{-100},Q{100}})));same(p.tail(),h.b);
}
void context() {
    using namespace models;
    auto past=endpoint(40,{1,2});auto future=endpoint(41,{999,888});NativeAtlasManifest manifest;manifest.key=901;
    NativeAtlas atlas({NativeAtlasSource(10,800,past),NativeAtlasSource(20,801,future)},manifest);
    NativeAccessionRequest field;field.kind=NativeAccessionKind::Field;field.subject=10;
    std::vector<NativeContextSelection> selections{{10,field}};std::vector<NativeContextCutoff> cutoffs{{10,800,40}};
    unsigned calls=0;auto allow=[&](const auto&){++calls;return true;};
    auto bounded=NativeTemporalContext::build(atlas,selections,cutoffs,allow);
    check(bounded.items().size()==1 && calls==1,"past context denied");
    check(std::get<NativeTensorContext>(bounded.items()[0].contents).tensor.state(0)==Q{1},"past field leaked");
    selections={{20,field}};rejects([&]{(void)NativeTemporalContext::build(atlas,selections,{{20,801,40}},allow);});check(calls==1,"future grant callback executed");
    rejects([&]{(void)NativeTemporalContext::build(atlas,selections,{},allow);});
    rejects([&]{(void)NativeTemporalContext::build(atlas,{{10,field}},{{10,999,40}},allow);});
    rejects([&]{(void)NativeTemporalContext::build(atlas,{{10,field}},{{10,800,40},{10,800,40}},allow);});
    rejects([&]{(void)NativeTemporalContext::build(atlas,{{10,field}},cutoffs,{});});
    selections={{10,field}};calls=0;
    auto stable=NativeTemporalContext::build(atlas,selections,cutoffs,[&](const auto&){++calls;selections={{20,field}};cutoffs[0].max_sequence=0;atlas=NativeAtlas({NativeAtlasSource(10,800,future)},manifest);return true;});
    check(calls==1 && stable.items()[0].origin.sequence==40 && stable.cutoffs()[0].max_sequence==40,"callback changed temporal boundary");
    check(std::get<NativeTensorContext>(stable.items()[0].contents).tensor.state(0)==Q{1},"callback mixed future source");
    auto proposal=stable.propose(ProspectiveEncoding::ExactCandidate,"bounded","prospective",{});
    check(proposal.provenance()[0].sequence==40,"proposal provenance lost cutoff");
    rejects([&]{(void)NativeTemporalContext::build(atlas,{{10,field}},{{10,800,40}},allow);});
}
void chronological() {
    std::mt19937_64 random(2019);
    for(unsigned trial=0;trial<300;++trial){
        Vec initial{Q{1},Q{-2}};std::vector<M> maps;std::vector<Vec> injections;
        for(unsigned i=0;i<6;++i){M map(2,2);for(unsigned r=0;r<2;++r)for(unsigned c=0;c<2;++c){Q value{static_cast<long>(random()%9)-4,3};value.canonicalize();map.set(r,c,value);}maps.push_back(map);injections.push_back({Q{static_cast<long>(i)},Q{-1}});}
        for(unsigned cutoff=0;cutoff<6;++cutoff){
            auto prefix=temporal::ChronologicalPrefix::capture(initial,maps,injections,cutoff);
            auto changed=maps;auto altered=injections;changed[cutoff]=M(7,9);altered[cutoff]={Q{99999}};
            auto after=temporal::ChronologicalPrefix::capture(initial,changed,altered,cutoff);
            Vec reference=initial;for(unsigned i=0;i<cutoff;++i)reference=numeric::add(maps[i].apply(reference),injections[i]);
            check(prefix.terminal()==reference && after.terminal()==reference,"future chronological map/injection leaked");
            check(prefix.states().size()==cutoff+1 && prefix.steps()==cutoff,"future states exposed");
        }
    }
    rejects([&]{(void)temporal::ChronologicalPrefix::capture({},std::vector<M>{},std::vector<Vec>{},1);});
}
}
int main(){history();context();chronological();std::cout<<checks<<" temporal cutoff/noninterference checks passed\n";}
