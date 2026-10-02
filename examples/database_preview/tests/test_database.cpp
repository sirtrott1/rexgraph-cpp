#include "rfc_preview/sample.hpp"
#include "rfc_preview/execution.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <bit>

namespace {
using namespace rfc_preview;
using namespace rfc;
using namespace rfc::storage;
using Q=numeric::Rational;
std::size_t checks{},truncations{},mutations{},rehashed{},bits{},physical_checks{};
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F&& f,const char* why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,why);}
void patch64(Bytes& b,std::size_t p,std::uint64_t x){check(p+8<=b.size(),"test patch range");for(unsigned j=0;j<8;++j)b[p+j]=std::byte((x>>(8*j))&255);}
void refresh_manifest(Bytes& b){detail::Reader r{b};r.take(24);auto n=r.size();auto h=digest(std::span<const std::byte>(b).subspan(64,n));std::copy(h.begin(),h.end(),b.begin()+32);}
bool opened_equal(const std::filesystem::path& p,const Database& db){auto opened=Database::open(p);return equal(opened.bytes(),db.bytes());}
void endpoint_equal(const TemporalEndpoint& a,const TemporalEndpoint& b){
    check(encode_rfcx(a.source).bytes==encode_rfcx(b.source).bytes,"retained source mismatch");
    check(encode_temporal_endpoint(a)==encode_temporal_endpoint(b),"complete endpoint mismatch");}
TemporalEndpoint tiny(std::uint64_t seq=0,Q value=0){core::RelationalComplexBuilder b(core::RelationSignature{});b.declare_witness_identity(1);b.declare_grade(core::Grade{2});TemporalEndpoint e(b.freeze(),seq);
    e.carriers={{1,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(1)}};
    TemporalTensor f;f.key=1;f.role=TemporalTensorRole::Field;f.output=1;f.interpretation="exact test field";
    f.tensor=sample::tensor(numeric::IndexDomain::linear(1),{0},value==0?std::vector<numeric::SparseRationalEntry>{}:std::vector<numeric::SparseRationalEntry>{{0,value}});e.tensors={f};return e;}
void formats(const std::filesystem::path& out){
    auto history=sample::history(9);auto db=Database::from_series(history);auto file=out/"sample.rfcxdb";db.save_file(file);db.save_directory(out/"sample.db");
    auto a=Database::open(file);auto d=Database::open(out/"sample.db");check(equal(db.bytes(),a.bytes())&&equal(a.bytes(),d.bytes()),"file/directory bytes disagree");
    check(encode_native_series(a.series())==encode_native_series(history),"ordered archive changed");
    write_new_file(out/"sample.rfcxs",encode_native_series(history));d.save_file(out/"repacked.rfcxdb");check(read_file(file)==read_file(out/"repacked.rfcxdb"),"directory roundtrip");
    d.save_directory(out/"copied.db");check(opened_equal(out/"copied.db",db),"directory copy changed bytes");
    auto place=db.placement_tensor();check(place.domain().logical_size()==db.manifest().entries.size()*3,"placement shape");
    check(place.support().count()==place.domain().logical_size(),"zero-offset support lost");
    std::map<Digest,std::size_t> ids;
    for(std::size_t i=0;i<db.manifest().entries.size();++i){const auto& e=db.manifest().entries[i];auto [it,_]=ids.emplace(e.sha,ids.size());
        check(digest(db.component(i).bytes())==e.sha,"physical bytes do not match binding");
        check(place.state(3*i)==Q(numeric::Integer(std::to_string(it->second)))&&place.state(3*i+1)==Q(numeric::Integer(std::to_string(e.offset)))&&place.state(3*i+2)==Q(numeric::Integer(std::to_string(e.length))),"placement tensor mismatch");}
    std::ofstream circuit(out/"circuit.csv");circuit<<"sequence,drive,voltage,previous,radius,valid,low,high,uncertain,negative,falling\n";
    for(std::uint64_t t=0;t<=9;++t){auto expected=sample::endpoint(t);auto pin=a.series().at(t);endpoint_equal(pin.endpoint(),expected);
        auto from_base=a.series().at(t,true);endpoint_equal(pin.endpoint(),from_base.endpoint());
        check(expected.source.grade_count()==3 && expected.source.grade(core::Grade{2}).cell_count()==0,"empty grade lost");
        const auto& actual=pin.endpoint();const auto& g=actual.source.grade(core::Grade{1});const auto& eg=expected.source.grade(core::Grade{1});
        check(g.cell_count()==(t>=3&&t<=6?7u:6u),"assertion birth/removal lost");
        check(g.incidence_count()==(t>=3&&t<=6?147u:145u),"occurrence count lost");
        for(std::size_t j=0;j<g.incidence_count();++j)for(std::size_t k=0;k<131;++k){check(g.aspects.get(static_cast<composite::AspectId>(k),j)==eg.aspects.get(static_cast<composite::AspectId>(k),j),"bit differs");++bits;}
        auto mask=execute_declared_source_program(actual.source,*actual.declarations,1,core::Grade{1});
        const auto high=actual.source.signature().aspects().id("level-high"),valid=actual.source.signature().aspects().id("measurement-valid");
        for(std::size_t j=0;j<g.incidence_count();++j)check(mask.get(j)==(g.aspects.get(high,j)&&g.aspects.get(valid,j)),"packed operation differs from per-bit oracle");
        NativeAccessionDirectory views(actual,pin.source_namespace());auto f=views.pin(NativeAccessionRequest::cochain(10,core::Grade{0}));
        const auto c=views.pin(NativeAccessionRequest::sparse_chain(core::Grade{1},{200},{{200,1}}));
        auto boundary=c.boundary();auto derivative=f.coboundary();
        check(f.pair_with(boundary)==derivative.pair_with(c),"cochain pairing law");check(f.pair_with(boundary)==sample::voltage(t),"wire voltage drop");
        check(f.state(10)==sample::voltage(t),"native field accession lost exact value");
        auto amplitudes=views.pin(NativeAccessionRequest::chain(15,core::Grade{1}));
        if(t>=3&&t<=6)check(amplitudes.state(160)==0,"zero-amplitude birth erased");
        if(t){const auto evolution=a.series().difference(t-1,t).field_evolution(15);
            for(const auto& channel:evolution.accumulation.propagated_innovations)for(const auto& value:channel)check(value==0,"structural birth invented amplitude innovation");}
        const auto data=temporal_endpoint_detail::tensors(actual).at(20);
        check(data->tensor.support().contains(0)&&data->tensor.state(0)==0,"supported zero erased");
        check(data->tensor.support().contains(1)==(t>=2),"support-only change erased");
        check(data->tensor.values().nonzeros()==2,"unsupported numerical values introduced");
        const auto& pv=actual.declarations->values.front().value;check(std::get<composite::DeclaredValue::Product>(pv.data).elements.size()==8,"product declaration lost");
        auto legs=sample::directory_legs(actual);auto roles=sample::directory_tensor(actual);check(legs.size()==138,"directory support incomplete");
        std::size_t repeats=0;for(std::size_t i=0;i<legs.size();++i){const auto& l=legs[i];check(roles.support().contains(2*i)==l.parent&&roles.support().contains(2*i+1)==l.child,"occurrence tensor projection");if(l.relation==102&&l.target==10)++repeats;}
        check(repeats==2,"shared participant collapsed repeated leg");
        auto glued=materialize_glued_strict_blocks(actual.source,actual.local_blocks);const auto& gg=glued.complex.grade(core::Grade{1});
        check(gg.cell_count()==2&&gg.incidence_count()==135,"gluing lost inactive occurrence or duplicated shared source");
        check(glued.complex.grade(core::Grade{0}).cell_count()==132,"gluing duplicated common cells");
        const auto v=sample::voltage(t),prev=sample::voltage(t?t-1:0),u=t<=5?Q{1}:Q{-1};auto o=sample::observe(v,prev,Q{1,100},t!=7);
        circuit<<t<<','<<u<<','<<v<<','<<prev<<",1/100,"<<o.valid<<','<<o.low<<','<<o.high<<','<<o.uncertain<<','<<o.negative<<','<<o.falling<<'\n';
        if(t){const Q dv=v-prev;check(2*dv+v==u,"discrete KCL");
            const auto& parameters=std::get<composite::DeclaredValue::Product>(actual.declarations->values.back().value.data).elements;
            const auto C=std::get<Q>(parameters[0].data),R=std::get<Q>(parameters[1].data),dt=std::get<Q>(parameters[2].data),drive=std::get<Q>(parameters[3].data);
            check(C>0&&R>0&&dt>0&&drive==u,"retained physical parameters");check(C*dv/dt+v/R==drive/R,"retained-parameter KCL");
            const Q energy=C*(v*v-prev*prev)/2,step_loss=C*dv*dv/2,resistive=dt*v*v/R,work=dt*drive*v/R;
            check(energy+step_loss+resistive==work,"exact discrete energy balance");physical_checks+=2;}
    }
    check(a.series().at(9).stats().replayed_segments==5,"checkpoint not selected");
    auto before=encode_native_series(history);auto stream=Bytes(history.segment_bytes(0).begin(),history.segment_bytes(0).end());
    write_new_file(out/"first.rfcxt",stream);rejects([&]{history.append_stream(stream);},"wrong predecessor accepted");check(before==encode_native_series(history),"failed append changed history");
    auto base=NativeTemporalSeries(sample::endpoint(0),sample::namespace_key);stream.push_back(std::byte{0});
    rejects([&]{base.append_stream(stream,7);},"trailing frame prefix accepted");check(base.last_sequence()==0,"partial prefix published");
    auto oldframes=history;history.repack_checkpoints({});check(history.same_history(oldframes),"physical compaction changed history");
    auto repack=Database::from_series(history);check(repack.bytes().size()<db.bytes().size(),"checkpoint removal not smaller");
    for(std::size_t i=0;i<history.segment_count();++i)check(equal(history.segment_bytes(i),db.series().segment_bytes(i)),"original word history changed");
    repack.save_file(out/"compact.rfcxdb");
    auto survivor=[&]{auto tmp=Database::from_series(NativeTemporalSeries(tiny(),77));return tmp.component(0);}();check(!survivor.bytes().empty(),"component outlived owner incorrectly");
    auto invalid=survivor;invalid.offset=SIZE_MAX;rejects([&]{(void)invalid.bytes();},"modified pin overflow");
    auto moved=std::move(a);rejects([&]{(void)a.manifest();},"moved database accessible");check(moved.series().last_sequence()==9,"move lost database");
}
void boundary_cases(const std::filesystem::path& out){
    auto series=NativeTemporalSeries(tiny(),77);auto e1=tiny(1,3);series.append(make_temporal_segment(tiny(),e1));series.checkpoint(1);
    auto db=Database::from_series(series);check(db.manifest().entries.size()==5,"tiny directory size");
    check(db.manifest().entries[0].sha==db.manifest().entries[3].sha,"unchanged source was duplicated");
    check(db.manifest().entries[0].offset==db.manifest().entries[3].offset,"duplicate object has different extent");
    check(db.manifest().entries[0].kind!=db.manifest().entries[3].kind,"physical dedup merged semantic component identities");
    auto packed=Bytes(db.bytes().begin(),db.bytes().end());db.save_file(out/"tiny.rfcxdb");
    for(std::size_t n=0;n<packed.size();++n){rejects([&]{(void)Database::decode(std::span<const std::byte>(packed).first(n));},"truncated container accepted");++truncations;}
    std::mt19937_64 rng(0x20260928);for(std::size_t i=0;i<500;++i){auto x=packed;auto pos=static_cast<std::size_t>(rng()%x.size());x[pos]^=static_cast<std::byte>(1u<<unsigned(rng()%8));
        rejects([&]{(void)Database::decode(x);},"bit mutation bypassed component/header validation");++mutations;}
    auto bad_manifest=[&](std::size_t offset,std::uint64_t v){auto x=packed;patch64(x,offset,v);refresh_manifest(x);rejects([&]{(void)Database::decode(x);},"rehashed invalid manifest accepted");++rehashed;};
    bad_manifest(64+16,0);bad_manifest(64+24,2);bad_manifest(64+32,UINT64_MAX);bad_manifest(64+40,UINT64_MAX);
    bad_manifest(64+48,255);bad_manifest(64+48+8,1);bad_manifest(64+48+16,UINT64_MAX);bad_manifest(64+48+24,UINT64_MAX);
    bad_manifest(64+48+64+16,0);bad_manifest(64+48+128+8,0);bad_manifest(64+48+192+16,1);bad_manifest(64+48+256,4);
    auto native_bad=packed;const auto& source=db.manifest().entries[0];const auto start=64+detail::encode_manifest(db.manifest()).size()+source.offset;
    native_bad[start]=std::byte{0};const auto changed=digest(std::span<const std::byte>(native_bad).subspan(start,source.length));
    for(std::size_t i:std::array<std::size_t,2>{0,3})std::copy(changed.begin(),changed.end(),native_bad.begin()+static_cast<std::ptrdiff_t>(64+48+64*i+32));
    refresh_manifest(native_bad);rejects([&]{(void)Database::decode(native_bad);},"valid outer integrity hid invalid native source");++rehashed;
    auto trailing=packed;trailing.push_back(std::byte{0});patch64(trailing,16,trailing.size());rejects([&]{(void)Database::decode(trailing);},"trailing payload accepted");
    Limits limit;limit.max_components=4;rejects([&]{(void)Database::decode(packed,limit);},"component budget bypass");limit={};limit.max_file_bytes=packed.size()-1;
    rejects([&]{(void)Database::decode(packed,limit);},"file budget bypass");limit={};limit.max_component_bytes=1;rejects([&]{(void)Database::decode(packed,limit);},"component size budget bypass");
    limit={};limit.series.max_archive_bytes=20;rejects([&]{(void)Database::decode(packed,limit);},"expanded archive budget bypass");
    auto limit_steps=Limits{};limit_steps.series.max_segments=0;rejects([&]{(void)Database::decode(packed,limit_steps);},"history budget bypass");
    auto noframes=Database::from_series(NativeTemporalSeries(tiny(),79));noframes.save_file(out/"empty-history.rfcxdb");
    check(noframes.manifest().entries.size()==2,"empty history not supported");
}
void filesystem_cases(const std::filesystem::path& out){
    const auto good=Database::from_series(NativeTemporalSeries(tiny(),81));const auto other=Database::from_series(NativeTemporalSeries(tiny(0,9),82));
    good.save_directory(out/"safe.db");good.save_file(out/"safe.rfcxdb");
    rejects([&]{other.save_directory(out/"safe.db");},"existing directory overwritten");rejects([&]{other.save_file(out/"safe.rfcxdb");},"existing file overwritten");
    check(opened_equal(out/"safe.db",good),"old database changed");
    std::filesystem::create_directory_symlink(out/"safe.db",out/"link.db");rejects([&]{(void)Database::open(out/"link.db");},"root symlink accepted");
    std::filesystem::create_symlink(out/"safe.rfcxdb",out/"link.rfcxdb");rejects([&]{other.save_file(out/"link.rfcxdb");},"output symlink overwritten");
    rejects([&]{(void)Database::open_file(out/"link.rfcxdb");},"input symlink accepted");
    good.save_directory(out/"bad-object.db");auto obj=out/"bad-object.db"/"objects"/(hex(good.manifest().entries[0].sha)+".blob");
    auto ob=read_file(obj);ob.back()^=std::byte{1};std::filesystem::remove(obj);write_new_file(obj,ob);rejects([&]{(void)Database::open_directory(out/"bad-object.db");},"corrupt object accepted");
    good.save_directory(out/"missing-object.db");std::filesystem::remove(out/"missing-object.db"/"objects"/(hex(good.manifest().entries[0].sha)+".blob"));
    rejects([&]{(void)Database::open_directory(out/"missing-object.db");},"missing object accepted");
    good.save_directory(out/"linked-object.db");obj=out/"linked-object.db"/"objects"/(hex(good.manifest().entries[0].sha)+".blob");std::filesystem::remove(obj);
    std::filesystem::create_symlink(out/"safe.db"/"objects"/obj.filename(),obj);rejects([&]{(void)Database::open_directory(out/"linked-object.db");},"object symlink accepted");
    good.save_directory(out/"uncommitted.db");std::filesystem::remove(out/"uncommitted.db"/"COMMIT");rejects([&]{(void)Database::open_directory(out/"uncommitted.db");},"uncommitted directory accepted");
    good.save_directory(out/"bad-commit.db");auto cm=read_file(out/"bad-commit.db"/"COMMIT");cm[0]^=std::byte{1};std::filesystem::remove(out/"bad-commit.db"/"COMMIT");write_new_file(out/"bad-commit.db"/"COMMIT",cm);
    rejects([&]{(void)Database::open_directory(out/"bad-commit.db");},"wrong commit accepted");
    rejects([&]{good.save_directory(out/"fault.db",[](std::size_t){throw std::runtime_error("injected write boundary failure");});},"fault injection escaped");
    check(!std::filesystem::exists(out/"fault.db"/"COMMIT"),"failure published commit");rejects([&]{(void)Database::open_directory(out/"fault.db");},"partial new directory accepted");
    check(opened_equal(out/"safe.db",good),"failed new write affected existing directory");
    // Filesystem paths are typed native strings: embedded NUL must not select
    // or create the C-string prefix at any public file/directory boundary.
    auto nul_file=std::filesystem::path((out/"safe.rfcxdb").string()+std::string("\0suffix",7));
    auto nul_dir=std::filesystem::path((out/"safe.db").string()+std::string("\0suffix",7));
    rejects([&]{(void)read_file(nul_file);},"NUL file path read prefix");
    rejects([&]{write_new_file(nul_file,Bytes{});},"NUL file path wrote prefix");
    rejects([&]{(void)Database::open(nul_file);},"NUL generic input admitted");
    rejects([&]{(void)Database::open_file(nul_file);},"NUL packed input admitted");
    rejects([&]{(void)Database::open_directory(nul_dir);},"NUL directory input admitted");
    rejects([&]{good.save_directory(nul_dir);},"NUL directory output admitted");
    const auto fresh_prefix=out/"nul-must-not-exist.db";
    rejects([&]{good.save_directory(std::filesystem::path(fresh_prefix.string()+std::string("\0suffix",7)));},"NUL fresh output admitted");
    check(!std::filesystem::exists(fresh_prefix),"NUL path created unrequested prefix");
    check(opened_equal(out/"safe.rfcxdb",good)&&opened_equal(out/"safe.db",good),"path rejection altered prior database");
    auto callback_mutated=good;callback_mutated.save_directory(out/"callback.db",[&](std::size_t){callback_mutated=other;});
    check(opened_equal(out/"callback.db",good),"callback produced mixed publication");
    struct stat st{};check(::stat((out/"safe.rfcxdb").c_str(),&st)==0 && (st.st_mode&0777)==0600,"private output permissions");
}
void accession_work(const std::filesystem::path& out){
    auto db=Database::open_file(out/"sample.rfcxdb");auto end=db.series().tail();NativeAccessionDirectory directory(end.endpoint(),end.source_namespace());
    NativeAccessionRequest a;a.kind=NativeAccessionKind::Field;a.subject=10;NativeAccessionRequest b;b.kind=NativeAccessionKind::Tensor;b.subject=20;
    const std::array handles{directory.intern(a),directory.intern(b)};const auto before=directory.stats();
    std::uint64_t checksum=0;const auto begin=std::chrono::steady_clock::now();
    for(std::size_t i=0;i<100000;++i){auto view=directory.resolve(handles[i%2]);checksum+=view.sequence()+static_cast<std::uint64_t>(view.kind());}
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();const auto after=directory.stats();
    check(after.handle_resolutions-before.handle_resolutions==100000,"hot resolution counter");
    check(after.descriptor_hashes==before.descriptor_hashes&&after.bucket_comparisons==before.bucket_comparisons&&after.cold_bindings==before.cold_bindings,"hot access repeated binding/hash work");
    auto pin=db.component(0);std::uint64_t sum=0;const auto start=std::chrono::steady_clock::now();
    for(std::size_t i=0;i<100000;++i)sum+=std::to_integer<unsigned>(pin.bytes()[i%pin.length]);
    const auto component_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::ofstream f(out/"accession_work.json");f<<"{\"resolutions\":100000,\"new_descriptor_hashes\":0,\"new_bucket_searches\":0,\"resolve_ms\":"<<elapsed<<",\"component_span_ms\":"<<component_ms<<",\"checksum\":"<<checksum<<",\"byte_sum\":"<<sum<<"}\n";
}
void catalogue_profile_cases(){
    auto missing=sample::endpoint(0);missing.declarations.reset();
    rejects([&]{(void)sample::directory_legs(missing);},"catalogue guessed absent declaration");
    auto different=sample::endpoint(0);different.declarations->namespace_key^=1;
    rejects([&]{(void)sample::directory_legs(different);},"catalogue guessed declaration namespace");
    auto role=sample::endpoint(0);role.declarations->branches[1].declaration.population=core::BranchPopulation::Active;
    rejects([&]{(void)sample::directory_legs(role);},"catalogue accepted changed role population");
    auto changed_signature=sample::signature();changed_signature.aspects().add("new-context");
    core::RelationalComplexBuilder b(changed_signature);b.declare_grade(core::Grade{2});
    TemporalEndpoint changed(b.freeze(),0);changed.declarations=sample::endpoint(0).declarations;
    rejects([&]{(void)sample::directory_legs(changed);},"catalogue silently accepted changed source schema");
}
void electrical_cases(){
    using sample::observe;check(observe(Q{3,10},0,0).low,"low threshold boundary");check(observe(Q{7,10},0,0).high,"high threshold boundary");
    check(observe(Q{3,10},0,Q{1,100}).uncertain,"interval crossed threshold silently");
    auto invalid=observe(1,0,0,false);check(!invalid.valid&&!invalid.low&&!invalid.high&&!invalid.uncertain,"missing observation became false voltage");
    rejects([&]{(void)observe(1,0,-1);},"negative uncertainty accepted");
    auto observation_a=observe(Q{4,5},0,0),observation_b=observe(Q{9,10},0,0);check(observation_a.high&&observation_b.high,"same logical class fixture");check(Q{4,5}!=Q{9,10},"logic class not injective");
    check(observe(-1,0,0).negative && observe(-1,0,0).low,"physical polarity distinct from logic threshold");
    // Same present state but distinct drive => different physical model futures.
    const Q v=Q{1,2};check((2*v+1)/3 != (2*v-1)/3,"drive information erased");
    physical_checks+=8;
    // Demonstrate a transition-aware finite layout through the actual F1 codec.
    // This counts encoded bit changes; it is NOT a measurement of CPU energy.
    std::vector<std::uint64_t> symbols(16),reflected(16);
    for(unsigned i=0;i<16;++i){symbols[i]=100+i;reflected[i^(i>>1)]=100+i;}
    const auto da=composite::Declaration::finite({91,1,1},symbols),db=composite::Declaration::finite({91,1,2},reflected);
    const composite::CompiledLayout binary(composite::DeclarationRegistry({da}),da.id),gray(composite::DeclarationRegistry({db}),db.id);
    unsigned binary_flips=0,gray_flips=0,old_binary=0,old_gray=0;
    for(unsigned i=0;i<16;++i){const auto value=composite::DeclaredValue::symbol(100+i);
        const auto b=encode_declared_bits(binary,value),g=encode_declared_bits(gray,value);
        check(b.bit_count==4&&g.bit_count==4,"finite code grew unexpectedly");
        check(composite::declared_value_equal(decode_declared_bits(binary,b),value)&&composite::declared_value_equal(decode_declared_bits(gray,g),value),"layout changed semantic state");
        const auto bv=std::to_integer<unsigned>(b.bytes[0]),gv=std::to_integer<unsigned>(g.bytes[0]);
        check(bv==i&&gv==(i^(i>>1)),"code assignment differs");
        if(i){binary_flips+=static_cast<unsigned>(std::popcount(bv^old_binary));gray_flips+=static_cast<unsigned>(std::popcount(gv^old_gray));}
        old_binary=bv;old_gray=gv;}
    check(binary_flips==26&&gray_flips==15,"transition-count oracle");physical_checks+=49;
}
}
int main(int argc,char** argv){try{
    const std::filesystem::path out=argc>1?argv[1]:"database-test-fixtures";
    check(!std::filesystem::exists(out),"test output must be fresh");std::filesystem::create_directories(out);
    formats(out);boundary_cases(out);filesystem_cases(out);catalogue_profile_cases();electrical_cases();accession_work(out);
    auto session=[] {
        const auto db=Database::from_series(action_sample_history(1));
        return std::make_unique<PreparedFieldDiagnostic>(db,1,core::Grade{1},9000001,runtime::ExecutionHostPolicy{});
    }();
    const auto reference=database::execute(session->operation());
    for(unsigned i=0;i<100;++i) {
        const auto result=session->execute();
        check(result.values()==reference.values(),"database prepared session changed exact field");
        check(result.provenance().implementation=="PreparedGradeFieldAction/GMP/serial-prepared-v1","database used reference execution");
    }
    std::ofstream json(out/"results.json");json<<"{\"checks\":"<<checks<<",\"retained_bits_compared\":"<<bits<<",\"truncations_rejected\":"<<truncations<<",\"mutations_rejected\":"<<mutations<<",\"rehashed_invalid_manifests_rejected\":"<<rehashed<<",\"electrical_model_checks\":"<<physical_checks<<"}\n";
    std::cout<<"checks="<<checks<<" retained_bits="<<bits<<" truncations="<<truncations<<" mutations="<<mutations<<" exact_file_directory_roundtrip=true\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
