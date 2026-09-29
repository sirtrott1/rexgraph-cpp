#include "../storage/native_series_fixture.hpp"
#include "rfc/storage/declaration_codec.hpp"
#include "rfc/composite/declaration_evolution.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif
using namespace rfc;using namespace rfc::storage;
namespace {
std::size_t checks=0;
void check(bool good,const char*message){++checks;if(!good)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char*message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
void file_operations(const std::filesystem::path& root) {
 std::vector<std::byte> old(512),next((1u<<20)+9);for(std::size_t i=0;i<old.size();++i)old[i]=std::byte(i%256);for(std::size_t i=0;i<next.size();++i)next[i]=std::byte((i*17)%256);
 auto path=root/"bytes.bin";write_file_exact(path,old);check(read_file_exact(path,512)==old,"binary content changed");
 ReadOnlyMappedFile before(path);write_file_exact(path,next);check(std::ranges::equal(before.bytes(),old),"replacement invalidated pinned original inode");check(read_file_exact(path,next.size())==next,"chunked binary read/write changed");
 rejects([&]{(void)read_file_exact(path,next.size()-1);},"oversized file accepted");
 rejects([&]{publish_file_exact(path,FilePublication::ReplaceRegular,[&](std::ostream& out){write_stream_exact(out,old);throw std::runtime_error("injected producer failure");});},"producer failure ignored");
 check(read_file_exact(path,next.size())==next,"failed replacement truncated old bytes");
 rejects([&]{write_file_exact(path,old,FilePublication::CreateNew);},"exclusive write replaced old bytes");
 auto interrupted=root/"interrupted.bin";rejects([&]{publish_file_exact(interrupted,FilePublication::CreateNew,[&](std::ostream&out){write_stream_exact(out,old);throw std::runtime_error("injected");});},"new-file failure ignored");check(!std::filesystem::exists(interrupted),"failed new file exposed partial output");
 auto conflict=root/"conflict.bin";rejects([&]{publish_file_exact(conflict,FilePublication::CreateNew,[&](std::ostream&out){write_stream_exact(out,old);write_file_exact(conflict,next,FilePublication::CreateNew);});},"competing creator overwritten");check(read_file_exact(conflict,next.size())==next,"competitor content changed");
 const std::filesystem::path nul(path.string()+std::string("\0suffix",7));
 rejects([&]{(void)read_file_exact(nul,next.size());},"NUL path read prefix");rejects([&]{write_file_exact(nul,old);},"NUL path wrote prefix");rejects([&]{ReadOnlyMappedFile image(nul);},"NUL mapped path admitted");
 auto symlink=root/"symlink.bin";std::filesystem::create_symlink(path.filename(),symlink);
 rejects([&]{write_file_exact(symlink,old);},"symlink target overwritten");
 auto dangling=root/"dangling.bin";std::filesystem::create_symlink("missing.bin",dangling);rejects([&]{write_file_exact(dangling,old,FilePublication::CreateNew);},"dangling symlink overwritten");
 auto empty=root/"empty.bin";write_file_exact(empty,{});check(read_file_exact(empty,0).empty(),"empty file not exact");
 rejects([&]{write_file_exact(root,old);},"directory output accepted");rejects([&]{write_file_exact(root/"bad.bin",old,static_cast<FilePublication>(255));},"unknown publication policy executed");
 for(const auto& entry:std::filesystem::directory_iterator(root))check(!entry.path().filename().string().starts_with(".rfc-stage-"),"leaked staging directory");
#if defined(__unix__) || defined(__APPLE__)
 auto fifo=root/"fifo";check(::mkfifo(fifo.c_str(),0600)==0,"mkfifo failed");rejects([&]{ReadOnlyMappedFile image(fifo);},"nonregular mapped file accepted");
 const auto sentinel=root/"fault.bin";write_file_exact(sentinel,old);const auto child=::fork();check(child>=0,"fork failed");
 if(child==0){::signal(SIGXFSZ,SIG_IGN);rlimit limit{64,64};if(::setrlimit(RLIMIT_FSIZE,&limit)!=0)::_exit(3);try{write_file_exact(sentinel,next);::_exit(2);}catch(...){::_exit(0);}}
 int status=0;check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"physical fault not reported");check(read_file_exact(sentinel,old.size())==old,"physical failure destroyed old bytes");
 // Real concurrent exclusive publishers: exactly one publishes, no overwritten winner.
 auto race=root/"race.bin";std::array<pid_t,4> workers{};
 for(std::size_t i=0;i<workers.size();++i){workers[i]=::fork();check(workers[i]>=0,"fork failed");if(workers[i]==0){try{std::array<std::byte,1>b{std::byte(i)};write_file_exact(race,b,FilePublication::CreateNew);::_exit(0);}catch(...){::_exit(1);}}}
 unsigned winners=0;for(auto pid:workers){check(::waitpid(pid,&status,0)==pid && WIFEXITED(status),"publisher child failed");if(WEXITSTATUS(status)==0)++winners;else check(WEXITSTATUS(status)==1,"unexpected child failure");}check(winners==1,"exclusive publication not unique");auto winner=read_file_exact(race,1);check(winner.size()==1 && std::to_integer<unsigned>(winner[0])<4,"corrupted race winner");
 rejects([&]{(void)write_rfcx(native_series_fixture::path(1),"/dev/full");},"failed buffered source write reported success");
#endif
}
void history_roundtrip(const std::filesystem::path& root) {
 using namespace native_series_fixture;PaperHistory h;auto sourcefile=root/"base.rfcx",sidecar=root/"base.rfcxe",history=root/"history.rfcxt";
 (void)write_rfcx(h.a.source,sourcefile);(void)write_temporal_endpoint(h.a,sidecar);MappedRfcx source(sourcefile);
 auto base=read_temporal_endpoint(source,sidecar);check(encode_temporal_endpoint(base)==encode_temporal_endpoint(h.a),"base sidecar changed");
 const auto a=encode_temporal_segment(h.first),b=encode_temporal_segment(h.second);append_temporal_segment(history,h.first);check(read_file_exact(history,a.size())==a,"first frame changed");
 ReadOnlyMappedFile old(history);append_temporal_segment(history,h.second);auto all=a;all.insert(all.end(),b.begin(),b.end());check(read_file_exact(history,all.size())==all,"append changed ordered frame bytes");check(std::ranges::equal(old.bytes(),a),"append changed old pin");
 auto segments=read_temporal_segments(history);check(segments.size()==2,"frame count changed");auto endpoint=replay_temporal_segment(replay_temporal_segment(base,segments[0]),segments[1]);check(encode_temporal_endpoint(endpoint)==encode_temporal_endpoint(h.c),"rectangular exact history changed");
 rejects([&]{append_temporal_segment(history,h.first);},"wrong predecessor appended");check(read_file_exact(history,all.size())==all,"failed append changed existing history");
#if defined(__unix__) || defined(__APPLE__)
 auto fail=root/"append-fault.rfcxt";append_temporal_segment(fail,h.first);auto child=::fork();check(child>=0,"fork failed");
 if(child==0){::signal(SIGXFSZ,SIG_IGN);rlimit limit{64,64};if(::setrlimit(RLIMIT_FSIZE,&limit)!=0)::_exit(3);try{append_temporal_segment(fail,h.second);::_exit(2);}catch(...){::_exit(0);}}
 int status=0;check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"append write fault not reported");check(read_file_exact(fail,a.size())==a,"append write fault tore predecessor");
#endif
 NativeTemporalSeries series(base,918);series.append(h.first);series.append(h.second);series.checkpoint(41);auto canonical=encode_native_series(series);write_file_exact(root/"history.rfcxs",canonical);auto reopened=read_native_series(root/"history.rfcxs");check(encode_native_series(reopened)==canonical,"series bytes changed");check(reopened.difference(40,42).field_evolution(10).accumulation.terminal==numeric::RationalVector({13,-7}),"field innovations changed");
 auto trailing=encode_temporal_endpoint(base);trailing.push_back(std::byte{0});write_file_exact(root/"trailing.rfcxe",trailing);rejects([&]{(void)read_temporal_endpoint(source,root/"trailing.rfcxe");},"sidecar trailer ignored");
 write_rfcx(h.b.source,sourcefile);check(source.materialize().grade(core::Grade{1}).cell_count()==2,"source replacement altered pinned mapping");MappedRfcx target(sourcefile);check(target.materialize().grade(core::Grade{1}).cell_count()==3,"source replacement missing");
}
void declared_transitions(const std::filesystem::path& root){
 using namespace composite;using V=DeclaredValue;auto id=[](std::uint64_t key,std::uint64_t version=1){return DeclarationId{992,key,version};};
 auto definitions=[&](std::uint64_t version){std::vector<std::uint64_t> symbols(256);std::iota(symbols.begin(),symbols.end(),0);if(version==2)std::reverse(symbols.begin(),symbols.end());
 std::vector<DeclarationFactor> factors;for(std::uint64_t k=1;k<9;++k)factors.push_back({100+k,id(k,version),0});if(version==2)std::reverse(factors.begin(),factors.end());
 std::vector<DeclarationAxis> axes{{101,2,{1001,1002}},{102,3,{2001,2002,2003}}};if(version==2){std::reverse(axes.begin(),axes.end());for(auto& a:axes)std::reverse(a.coordinates.begin(),a.coordinates.end());}
 return DeclarationRegistry({Declaration::boolean(id(1,version)),Declaration::finite(id(2,version),symbols),Declaration::integer(id(3,version)),Declaration::rational(id(4,version)),Declaration::optional(id(5,version),id(4,version)),Declaration::repeated(id(6,version),id(2,version),0,256),Declaration::rational_tensor(id(7,version),axes),Declaration::alternative(id(8,version),{{401,id(1,version),0},{402,id(5,version),0}}),Declaration::product(id(9,version),factors)});};
 auto original=definitions(1),target=definitions(2);std::vector<DeclarationMap> forward,back;for(std::uint64_t k=1;k<=9;++k){forward.push_back({id(k),id(k,2),DeclarationMapKind::Recode,{}});back.push_back({id(k,2),id(k),DeclarationMapKind::Recode,{}});}DeclarationRecoding f(original,target,forward),b(target,original,back);
 numeric::Integer big=1;big<<=257;big+=7;numeric::Rational q(big,17);q.canonicalize();std::vector<V> octets;for(std::uint64_t i=0;i<256;++i)octets.push_back(V::symbol(i));
 auto tensor=numeric::SparseRationalTensor(numeric::IndexDomain({2,3}),numeric::StructuralSupport::sparse(6,{0,2,5}),numeric::SparseRationalVector(6,{{0,q},{5,-2}}));
 std::vector<V> fields{V::boolean(false),V::symbol(0),V::integer(-big),V::rational(q),V::absent(),V::repeated(octets),V::tensor(tensor),V::alternative(402,V::present(V::rational(0)))};
 auto value=V::product(fields);CompiledLayout layout(original,id(9)),new_layout(target,id(9,2));
 for(unsigned n=0;n<12;++n){fields[0]=V::boolean(n%2);fields[1]=V::symbol(n?255:0);fields[4]=(n%2)?V::present(V::rational(0)):V::absent();value=V::product(fields);auto mapped=f.apply(id(9),value);auto restored=b.apply(id(9,2),mapped);check(declared_value_equal(restored,value),"lossless recoding lost typed state");
 auto encoded=encode_declared_archive(layout,std::vector<V>{value});auto decoded=decode_declared_archive(encoded);check(declared_value_equal(decoded.values.at(0),value),"nested value archive lost data");check(encode_declared_archive(decoded.layout,decoded.values)==encoded,"noncanonical roundtrip");
 auto rewritten=encode_declared_archive(new_layout,std::vector<V>{mapped});auto nd=decode_declared_archive(rewritten);check(declared_value_equal(b.apply(id(9,2),nd.values.at(0)),value),"disk recoding lost state");
 if(n==0){write_file_exact(root/"all-types.rfcxv",encoded);write_file_exact(root/"recoded-types.rfcxv",rewritten);for(std::size_t end=0;end<encoded.size();++end)rejects([&]{(void)decode_declared_archive(std::span<const std::byte>(encoded).first(end));},"truncated declared archive accepted");}
 }
 // Removing a coordinate/symbol is not an encoding optimization.
 auto smaller=DeclarationRegistry({Declaration::finite(id(2,3),{0,1})});auto finite=DeclarationRegistry({Declaration::finite(id(2),{0,1,2})});rejects([&]{DeclarationRecoding lost(finite,smaller,{{id(2),id(2,3),DeclarationMapKind::Recode,{}}});},"lossy type recoding accepted");
 check(tensor.state(2)==numeric::Rational{0} && !tensor.state(1),"support/numerical state conflated");
}
}
int main(int argc,char**argv){try{auto root=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("rfc-lossless-audit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));if(!std::filesystem::create_directory(root))throw std::runtime_error("fresh test directory required");file_operations(root);history_roundtrip(root);declared_transitions(root);std::cout<<"lossless I/O checks="<<checks<<'\n';if(argc==1)std::filesystem::remove_all(root);return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
