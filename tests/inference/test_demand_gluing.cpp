#include <rfc/inference/demand_gluing.hpp>
#include <rfc/core/builder.hpp>
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>
#include <condition_variable>
namespace s=rfc::storage;namespace d=rfc::database;namespace i=rfc::inference;using Q=rfc::numeric::Rational;using V=rfc::numeric::RationalVector;
namespace {
std::size_t checks{};void ck(bool value,std::source_location at=std::source_location::current()){++checks;if(!value)throw std::runtime_error("demand assertion line "+std::to_string(at.line()));}
template<class F>void refuses(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}ck(failed);}
struct F {
 std::vector<s::DeclaredRfcxPayload> payloads;std::vector<std::pair<std::uint64_t,std::uint64_t>> ends;std::filesystem::path path;s::PayloadManifest manifest;
 static auto records(const std::vector<std::pair<std::uint64_t,std::uint64_t>>& edges,bool positive){std::vector<s::DeclaredRfcxPayload> out;
  for(std::size_t n=0;n<edges.size();++n){rfc::core::RelationSignature sig;auto type=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,4);rfc::core::RelationalComplexBuilder b(sig);
   b.add_grade_one({10},type,{{1,true,!positive,1,{},rfc::core::OccurrenceKey{100}},{1,false,false,1,{},rfc::core::OccurrenceKey{101}},{2,true,false,1,{},rfc::core::OccurrenceKey{102}}});
   out.push_back(s::declare_rfcx_payload(990,n+1,7,n+1,b.freeze()));}return out;}
 static auto meta(const std::vector<s::DeclaredRfcxPayload>& p){std::vector<s::PayloadSourceRevision> sources;std::vector<s::PayloadDescriptor> values;for(const auto& v:p){sources.push_back(v.descriptor.identity.source);values.push_back(v.descriptor);}return s::PayloadManifest::create(sources,values);}
 F(std::filesystem::path p,std::vector<std::pair<std::uint64_t,std::uint64_t>> e={{1,2},{3,4},{2,3}},bool positive=false):payloads(records(e,positive)),ends(std::move(e)),path(std::move(p)),manifest(meta(payloads)){
  std::vector<std::span<const std::byte>> bytes;for(const auto& x:payloads)bytes.push_back(x.bytes);s::write_payload_archive(path,manifest,bytes);}
 auto cover(std::vector<std::size_t> order={0,1,2},std::uint64_t cutoff=10)const{
  s::DemandCoverDeclaration decl{901,44,1,902,cutoff,2,3,{}};
  for(auto n:order){const auto& source=payloads[n].descriptor;s::DemandPatch p;p.payload=source.identity;p.available_at=5;
   for(auto c:source.cells)p.cells.push_back(c.grade.value?s::PayloadCell{{1},101+n}:s::PayloadCell{{0},c.key==1?ends[n].first:ends[n].second});
   for(std::size_t j=0;j<source.occurrences.size();++j)p.occurrences.push_back((101+n)*10+j);
   decl.patches.push_back(std::move(p));}return s::DemandCover::create(manifest,decl);}
 auto cache(std::size_t entries=1)const{d::NativePayloadLimits l;l.max_entries=entries;return d::NativePayloadCache(s::PayloadArchiveProvider::open(path,manifest),l);}
};
void main_cases(F& f,const std::filesystem::path& root){
 auto cover=f.cover();auto decoded=s::DemandCover::decode(cover.bytes(),f.manifest);ck(std::ranges::equal(decoded.bytes(),cover.bytes()));auto cache=f.cache();i::DemandGluing plan(cache,cover);ck(!plan.index_qualification().verified);plan.qualify_index();ck(plan.index_qualification().verified && plan.index_qualification().patches==3);ck(cache.raw_usage().resident==0);auto x=plan.field(70,0,1,{{1,{1}}});
 for(auto word:{"","L","LL","LLL"}){auto result=plan.word(x,word);ck(result.moment==(word==std::string_view("LLL")?5:word==std::string_view("LL")?2:1));ck(result.trace.loaded.size()<=3);}
 auto w=plan.word(x,"L",true);ck(w.channels.at(101)==4 && w.channels.at(103)==1);ck(w.trace.loaded.size()==2);
 auto before=f.cover({0,1});i::DemandGluing ab(f.cache(),before);auto ax=ab.field(71,0,1,{{1,{1}}});ck(ab.word(ax,"L").moment==1);ck(ab.word(ax,"LL").moment==2);ck(ab.word(ax,"LLL").moment==4);
 for(auto patch:{std::size_t{1},std::size_t{2}}){i::DemandGluing local(f.cache(),f.cover({patch}));auto zero=local.field(72,0,1,{});ck(local.word(zero,"LLL").moment==0);}
 Q huge=Q(rfc::numeric::Integer{1}<<130,7);auto exact=plan.field(79,0,1,{{1,{huge}}});ck(plan.word(exact,"LLL").moment==5*huge*huge);
 auto vector=plan.field(73,0,2,{{1,{1,2}}});ck(plan.word(vector,"LLL").moment==25);
 ck(plan.word(x,"UD").field==plan.word(x,"L").field);refuses([&]{(void)plan.word(x,"D");});
 i::DemandGluing foreign(f.cache(),f.cover());refuses([&]{(void)foreign.word(x,"L");});
 auto first=plan.word(x,"LLL");for(const auto& p:f.payloads)(void)cache.evict(p.descriptor.identity);auto again=plan.word(x,"LLL");ck(first.field==again.field && again.moment==5);ck(again.trace.generations!=first.trace.generations);
 std::array<std::size_t,3> order{0,1,2};unsigned orders{};do{i::DemandGluing other(f.cache(),f.cover({order.begin(),order.end()}));auto y=other.field(74,0,1,{{1,{1}}});ck(other.word(y,"LLL").moment==5);++orders;}while(std::next_permutation(order.begin(),order.end()));ck(orders==6);
 auto closed=cover.declaration();closed.patches[0].cells[0]=closed.patches[0].cells[1];refuses([&]{(void)s::DemandCover::create(f.manifest,closed);});
 s::DemandCoverLimits cl;cl.max_encoded_bytes=1;refuses([&]{(void)s::DemandCover::create(f.manifest,cover.declaration(),cl);});cl={};cl.max_index_bytes=1;refuses([&]{(void)s::DemandCover::decode(cover.bytes(),f.manifest,cl);});
 i::DemandExecutionLimits l;l.max_work_bytes=1;i::DemandGluing tiny(f.cache(),cover,l);refuses([&]{(void)tiny.word(tiny.field(1,0,1,{{1,{1}}}),"L");});
 l={};l.max_output_bytes=1;i::DemandGluing no_output(f.cache(),cover,l);refuses([&]{(void)no_output.word(no_output.field(1,0,1,{{1,{1}}}),"L");});
 l={};l.max_patch_visits=0;i::DemandGluing no_reads(f.cache(),cover,l);refuses([&]{(void)no_reads.word(no_reads.field(1,0,1,{{1,{1}}}),"L");});
 i::DemandGluing old(f.cache(),f.cover({0,1,2},4));refuses([&]{(void)old.word(old.field(1,0,1,{{1,{1}}}),"L");});
 bool stopped=false;refuses([&]{(void)plan.word(x,"LLL",false,[&]{stopped=true;throw rfc::runtime::ExecutionCancelled();});});ck(stopped && cache.usage().loading==0);
 // A callback cannot change the in-flight field or selected word.
 auto mutable_x=x;std::string word="LLL";bool edited=false;auto frozen=plan.word(mutable_x,word,false,[&]{if(!edited){edited=true;mutable_x.values.clear();word="D";}});ck(frozen.moment==5);
 // The emitted inputs are original archive/manifest/map bytes, not regenerated observations.
 s::write_file_exact(root/"manifest.bin",f.manifest.bytes());s::write_file_exact(root/"cover.bin",cover.bytes());
 std::ofstream out(root/"observations.tsv");for(auto text:{"","L","LL","LLL"})out<<text<<'\t'<<plan.word(x,text).moment<<'\n';
}
void families(const std::filesystem::path& root){
 F correlated(root/"correlated.archive",{{1,2},{2,3}},true);auto cover=correlated.cover({0,1});i::DemandGluing plan(correlated.cache(),cover);
 auto family=plan.family({1},{{101,1},{102,2}},false);ck(family.consistent && family.variables==std::vector<std::uint64_t>({1,2,3}) && family.kernel.cols()==1);ck(family.particular[2]-family.particular[0]==1);ck(family.kernel.get(2,0)-family.kernel.get(0,0)==0);ck(family.trace.loaded.size()==2 && !family.trace.global);
 auto bits=plan.family({1},{{101,1},{102,1}},true,true);ck(bits.boolean_complete && bits.boolean_members.size()==2);for(const auto& v:bits.boolean_members)ck(v[0]==v[2] && v[0]+v[1]==1);
 i::DemandExecutionLimits small;small.max_boolean_candidates=1;i::DemandGluing bounded(correlated.cache(),cover,small);refuses([&]{(void)bounded.family({1},{{101,1}},true,true);});
 F odd(root/"odd.archive",{{1,2},{2,3},{3,1}},true);i::DemandGluing triangle(odd.cache(),odd.cover());auto half=triangle.family({1},{{101,1},{102,1},{103,1}},true,true);ck(half.consistent && half.particular==V({Q(1,2),Q(1,2),Q(1,2)}) && half.boolean_complete && half.boolean_members.empty());
 F cycle(root/"cycle.archive",{{1,2},{2,3},{3,1}});i::DemandGluing joined(cycle.cache(),cycle.cover());auto contradiction=joined.family({1},{{101,1},{102,1},{103,1}},false);ck(!contradiction.consistent && !contradiction.contradiction.empty() && contradiction.trace.loaded.size()==3);
 // A fine closed edge direction is invisible under divergence but stays reloadable.
 auto fine=joined.field(80,1,1,{{101,{1}},{102,{1}},{103,{1}}});ck(joined.word(fine,"D").field.empty());ck(joined.word(fine,"L").field.empty());ck(joined.word(fine,"").field==fine.values);
 F disconnected(root/"disconnected.archive",{{1,2},{3,4},{4,5},{5,3}});auto dc=disconnected.cover({0,1,2,3});i::DemandGluing all(disconnected.cache(),dc);
 auto local=all.family({1},{{102,1},{103,1},{104,1}},false);ck(local.consistent && local.edges.size()==1 && local.trace.loaded.size()==1);
 auto global=all.family({1},{{102,1},{103,1},{104,1}},true);ck(!global.consistent && global.trace.global && global.trace.loaded.size()==4);
}
void ownership_and_schedule(const std::filesystem::path& root){
 F duplicate(root/"duplicate.archive",{{1,2},{3,4},{2,3},{1,2}});auto initial_cover=duplicate.cover({0,1,2,3});auto declaration=initial_cover.declaration();
 declaration.patches[3].cells.back()={{1},101};declaration.patches[3].occurrences=declaration.patches[0].occurrences;
 auto cover=s::DemandCover::create(duplicate.manifest,declaration);auto cache=duplicate.cache();i::DemandGluing plan(cache,cover);auto x=plan.field(1,0,1,{{1,{1}}});ck(plan.word(x,"LLL").moment==5);
 auto broken=declaration;broken.patches[3].occurrences.back()=99999;refuses([&]{(void)s::DemandCover::create(duplicate.manifest,broken);});
 auto changed=declaration;++changed.revision;++changed.parameter_revision;++changed.arrangement_revision;auto second=s::DemandCover::create(duplicate.manifest,changed);i::DemandGluing newer(duplicate.cache(),second);refuses([&]{(void)newer.word(x,"L");});ck(plan.word(x,"LLL").moment==5);
 // Original stream ownership survives atomic replacement; a fresh open must
 // validate the selected whole original manifest again.
 F replacement(root/"replacement.archive",{{20,21},{30,31},{21,30}});std::filesystem::rename(replacement.path,duplicate.path);ck(plan.word(x,"LLL").moment==5);
 // Bounded exact requests refuse before producing a partial result.
 for(int mode=0;mode<4;++mode){i::DemandExecutionLimits l;if(mode==0)l.exact.steps=1;if(mode==1)l.max_rows=1;if(mode==2)l.exact.integer_bits=0;if(mode==3)l.max_coordinates=1;
  i::DemandGluing bounded(cache,cover,l);refuses([&]{(void)bounded.family({1},{{101,1},{102,2}},true);});}
 // Controlled cancellation at an active source load. No stale query publishes.
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false,failed=false;std::exception_ptr unexpected;
 std::thread worker([&]{try{(void)plan.word(x,"LLL",false,[&]{std::unique_lock lock(mutex);if(!entered && cache.usage().loading){entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});}});}catch(const rfc::database::PayloadGenerationChanged&){failed=true;}catch(...){unexpected=std::current_exception();}});
 {std::unique_lock lock(mutex);cv.wait(lock,[&]{return entered;});}bool cancelled=false;for(const auto& record:duplicate.payloads)cancelled=cache.cancel(record.descriptor.identity)||cancelled;ck(cancelled);
 {std::lock_guard lock(mutex);release=true;}cv.notify_all();worker.join();if(unexpected)std::rethrow_exception(unexpected);ck(failed && cache.usage().loading==0 && cache.raw_usage().loading_payload_bytes==0);ck(plan.word(x,"LLL").moment==5);
}
void hidden_index(const std::filesystem::path& root){
 F actual(root/"hidden-source.archive",{{1,2},{3,1},{2,3}});auto records=actual.payloads;
 // The actual B source reaches vertex1. The passive index falsely declares
 // its last occurrence attached to vertex3, hiding B from a local frontier.
 records[1].descriptor.occurrences.back().lower=records[1].descriptor.cells.front();
 auto manifest=F::meta(records);auto original_cover=actual.cover();auto declaration=original_cover.declaration();
 auto cover=s::DemandCover::create(manifest,declaration);std::vector<std::span<const std::byte>> bytes;for(const auto& value:records)bytes.push_back(value.bytes);
 const auto path=root/"hidden-false-index.archive";s::write_payload_archive(path,manifest,bytes);
 d::NativePayloadLimits limits;limits.max_entries=1;d::NativePayloadCache cache(s::PayloadArchiveProvider::open(path,manifest),limits);i::DemandGluing plan(cache,cover);
 refuses([&]{(void)plan.word(plan.field(1,0,1,{{1,{1}}}),"L");});ck(!plan.index_qualification().verified);ck(cache.usage().loading==0);
 // Variable-length declaration bytes have their own live workspace charge.
 rfc::core::RelationSignature signature;auto type=signature.add_type(std::string(200000,'n'),rfc::core::BoundaryPolicy::DeclaredExact,2,2);rfc::core::RelationalComplexBuilder builder(signature);
 builder.add_grade_one({10},type,{{1,true,true,1,{},rfc::core::OccurrenceKey{100}},{2,true,false,1,{},rfc::core::OccurrenceKey{101}}});
 auto declared=s::declare_rfcx_payload(991,1,1,1,builder.freeze());auto long_manifest=F::meta({declared});const auto long_path=root/"long-signature.archive";std::vector<std::span<const std::byte>> payloads{declared.bytes};s::write_payload_archive(long_path,long_manifest,payloads);
 s::DemandCoverDeclaration spec{991,1,1,992,1,1,1,{}};s::DemandPatch patch;patch.payload=declared.descriptor.identity;patch.available_at=1;patch.cells=declared.descriptor.cells;patch.occurrences={100,101};spec.patches.push_back(patch);auto long_cover=s::DemandCover::create(long_manifest,spec);
 i::DemandExecutionLimits policy;policy.exact.integer_bits=64;policy.max_work_bytes=512u<<10;i::DemandGluing bounded(d::NativePayloadCache(s::PayloadArchiveProvider::open(long_path,long_manifest)),long_cover,policy);
 refuses([&]{(void)bounded.word(bounded.field(1,0,1,{{1,{1}}}),"L");});ck(!bounded.index_qualification().verified);
}
void unrelated(const std::filesystem::path& root){std::vector<std::pair<std::uint64_t,std::uint64_t>> edges{{1,2},{3,4},{2,3}};for(std::uint64_t k=0;k<30;++k)edges.emplace_back(100+2*k,101+2*k);F f(root/"unrelated.archive",edges);std::vector<std::size_t> order(edges.size());std::iota(order.begin(),order.end(),0);auto c=f.cache();i::DemandGluing plan(c,f.cover(order));auto r=plan.word(plan.field(1,0,1,{{1,{1}}}),"LLL");ck(r.moment==5 && r.trace.loaded.size()==3);ck(c.raw_usage().resident<=1);}
}
int main(int argc,char** argv)try{const bool emit=argc==3 && std::string_view(argv[1])=="--emit";const auto root=emit?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("rfc_demand_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);F f(root/"source.archive");main_cases(f,root);families(root);unrelated(root);ownership_and_schedule(root);hidden_index(root);std::cout<<checks<<" demand gluing checks passed\n";if(!emit)std::filesystem::remove_all(root);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
