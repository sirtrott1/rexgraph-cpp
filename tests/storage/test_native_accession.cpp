#include "native_accession_fixture.hpp"
#include <iostream>
#include <random>
#include <chrono>

using namespace native_accession_fixture;
namespace {
std::size_t checks{};
void check(bool yes,const char* message) {++checks;if(!yes)throw std::runtime_error(message);}
template<class F> void rejects(F&& f,const char* message) {
    bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,message);
}
void save(const std::filesystem::path& path,std::span<const std::byte> bytes) {
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!f)throw std::runtime_error("fixture output failed");
}
NativeAccessionRequest obs(std::uint64_t query,std::uint64_t subject) {
    auto r=request(NativeAccessionKind::Observation,query);r.secondary=subject;return r;
}
NativeAccessionRequest aspect(std::uint64_t occurrence=10000) {
    auto r=request(NativeAccessionKind::SourceAspects,400);r.occurrence=StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{occurrence}};return r;
}
void basic(const std::filesystem::path& out) {
    auto e=endpoint();NativeAccessionDirectory d(e,800,queries(),[](auto){return 0;});
    const auto chain=d.intern(NativeAccessionRequest::chain(101,core::Grade{1}));
    const auto cochain=d.intern(NativeAccessionRequest::cochain(102,core::Grade{1}));
    const auto x=d.resolve(chain),f=d.resolve(cochain);
    check(x.support_count()==8 && x.state(1000)==Q{0} && !x.state(42),"source-grade support/state");
    check(f.pair_with(x)==Q{61},"canonical algebraic pairing");
    check(x.cell(1000).occurrence_count()==3,"retained branch occurrence count");
    check(x.cell(1000).declared_values().size()==1 && x.cell(1000).declared_values()[0]->key==301,"direct owner-content lookup");
    check(x.cell(1000).grade_fields().size()==2,"source-grade fields missing");
    check(x.cell(1000).local_memberships()==std::vector<std::uint64_t>{1,2},"shared local membership directory");
    check(x.cell(1000).source_schema()->type_count()==1,"pinned source schema missing");
    auto first=x.cell(1000).occurrence(0),inactive=x.cell(1000).occurrence(2);
    check(first.parent_cell().key()==x.cell(1000).key() && first.participant_cell().key()==first.participant(),"pinned incidence traversal");
    check(first.participant()==inactive.participant() && first.key()!=inactive.key(),"repeated participant occurrences merged");
    check(!inactive.aspect("exists") && inactive.coefficient()==0,"inactive occurrence lost");
    check(x.endpoint().get()==d.pin_snapshot().get(),"accession copied whole endpoint");
    rejects([&]{(void)x.tensor();},"selected chain exposes unselected tensor");
    rejects([&]{(void)d.resolve(chain,NativeAccessionKind::Cochain);},"wrong-kind handle accepted");
    rejects([&]{(void)x.pair_with(f);},"chain used as implicit cochain");
    rejects([&]{(void)f.boundary();},"cochain boundary accepted");
    rejects([&]{(void)x.coboundary();},"chain coboundary accepted");
    auto bad=NativeAccessionRequest::cochain(101,core::Grade{1});bad.pairing=NativePairing::None;
    rejects([&]{d.intern(bad);},"cochain without declared pairing accepted");
    const auto tensor=d.resolve(d.intern(request(NativeAccessionKind::Tensor,200)));
    check(tensor.tensor()->state(2)==Q{0} && !tensor.tensor()->state(1),"tensor support and zero merged");
    const auto op=d.resolve(d.intern(request(NativeAccessionKind::Operator,201)));
    const auto applied=op.apply_coordinates(numeric::SparseRationalVector(8,{{0,Q{3}},{1,Q{5}}}));
    check(temporal_endpoint_detail::values_equal(op.apply(x),op.apply_coordinates(x.tensor_declaration()->tensor.values())),"typed field operation mismatch");
    rejects([&]{op.apply(f);},"cochain silently treated as map input field");
    check(temporal_endpoint_detail::values_equal(applied,numeric::SparseRationalVector(3,{{0,Q{3}},{1,Q{-5}},{2,Q{6}}})),"sparse operator application");
    rejects([&]{d.intern(request(NativeAccessionKind::Operator,202));},"metric silently identified with linear operator");
    rejects([&]{op.apply_coordinates(numeric::SparseRationalVector(9));},"operator dimension mismatch accepted");
    const auto local=d.resolve(d.intern(request(NativeAccessionKind::LocalComplex,1)));
    check(local.local()->kind==LocalBlockKind::StrictSubcomplex && local.local_cell(2).key().key==1000,"local stable selection");
    check(local.local_cell(2).occurrence(2).key()==inactive.key(),"strict block omits inactive incidence");
    const auto partial=d.resolve(d.intern(request(NativeAccessionKind::LocalComplex,2)));
    check(partial.local()->kind==LocalBlockKind::PartialChart && partial.local()->occurrences.size()==1,"partial chart became full");
    auto r=request(NativeAccessionKind::DeclaredValue,301);r.factors={502};
    const auto value=d.resolve(d.intern(r));
    check(value.declaration()->id==id(3) && value.tensor()->state(1)==Q{0},"stable factor accession");
    check(value.declared_owner()->cell==StableCellKey{core::Grade{1},1000},"declared value owner lost");
    r.factors={501};check(std::get<V::Symbol>(d.resolve(d.intern(r)).declared_value()->data).key==7,"finite factor lookup");
    r.factors={99};rejects([&]{d.intern(r);},"unknown factor accepted");
    const auto a=d.intern(request(NativeAccessionKind::DeclaredValue,301));
    const auto b=d.intern(request(NativeAccessionKind::DeclaredValue,302));
    check(a!=b && d.descriptor(a)!=d.descriptor(b),"equal payloads merged retained owners");
    const auto bits=d.resolve(d.intern(aspect()));
    check(bits.source_bit(9000) && !bits.source_bit(9001),"source-backed factor lookup");
    check(bits.declaration()->id==id(5),"source layout declaration missing");
    const auto packed=bits.execute_source_program(401);check(packed.size()==24 && packed.get(0) && !packed.get(1),"source program execution");
    const auto cs=d.resolve(d.intern(request(NativeAccessionKind::CodeSection,1001)));
    check(cs.code_section()->source_aspects,"code source view copied bits");
    check(cs.code_type()->factors.size()==2 && cs.source_occurrence().key().key.value==10000,"code schema/source traversal");
    const auto operation=d.resolve(d.intern(request(NativeAccessionKind::CodeOperation,20)));
    check(operation.code_operation()->kernel.has_value(),"complete operation definition missing");
    const auto observation=d.resolve(d.intern(obs(5,50)));
    check(observation.observation()->compatible==true,"interface compatibility lost");
    check(d.intern(NativeAccessionRequest::chain(101,core::Grade{1}))==chain,"intern duplicate request");
    check(d.find(NativeAccessionRequest::chain(101,core::Grade{1})).value()==chain,"exact lookup after forced collision");
    check(!d.find(request(NativeAccessionKind::Tensor,999)),"absent descriptor found");
    NativeAccessionDirectory other(e,800,queries());
    rejects([&]{other.resolve(chain);},"foreign live handle accepted");
    auto descriptor=d.descriptor(chain);
    const auto other_h=other.intern(descriptor);check(other.descriptor(other_h)==descriptor,"exact descriptor context import");
    NativeAccessionDirectory foreign(e,801,queries());
    rejects([&]{foreign.intern(descriptor);},"different source namespace accepted");
    e.tensors[0]=native_accession_fixture::field(101,1,8,17);check(x.state(1000)==Q{0},"mutable caller endpoint alias");
    auto metadata=tensor.tensor_declaration();const auto original=metadata->tensor.state(0);
    const auto snapshot=d.pin_snapshot();check(snapshot->tensors[3].tensor.state(0)==original,"pinned metadata alias incorrect");
    const auto key=descriptor.key_bytes();save(out/"chain.rfca",key);
    save(out/"source.rfcx",encode_rfcx(*&snapshot->source).bytes);save(out/"endpoint.rfcxe",encode_temporal_endpoint(*snapshot));
}
void chain_laws(const std::filesystem::path& out) {
    auto e=endpoint();NativeAccessionDirectory d(e,800);
    std::mt19937 rng(171);std::ofstream oracle(out/"pairing.tsv");
    for(std::size_t k=0;k<128;++k) {
        std::vector<NativeCoefficient> xs,fs;std::vector<std::uint64_t> support;
        numeric::RationalVector dense(8,Q{0});
        for(std::size_t i=0;i<8;++i) {
            Q q{static_cast<long>(rng()%19)-9,static_cast<unsigned long>(rng()%7)+1};q.canonicalize();
            support.push_back(1000+i);xs.push_back({1000+i,q});dense[i]=q;
        }
        const auto xr=NativeAccessionRequest::sparse_chain(core::Grade{1},support,xs);
        auto fr=NativeAccessionRequest::cochain(0,core::Grade{0});fr.inline_value=true;
        for(std::uint64_t key=10;key<=12;++key) {
            Q q{static_cast<long>(rng()%11)-5,static_cast<unsigned long>(rng()%5)+1};q.canonicalize();fs.push_back({key,q});
        }
        fr.coefficients=fs;
        const auto x=d.resolve(d.intern(xr)),f=d.resolve(d.intern(fr));
        const auto boundary=x.boundary(),delta=f.coboundary();
        const auto left=f.pair_with(boundary),right=delta.pair_with(x);
        check(left==right,"chain/cochain pullback law");
        const auto native=e.source.boundary_apply(core::Grade{1},dense);
        for(std::uint64_t key=10;key<12;++key) {
            const auto cell=resolve_stable_cell(e.source,{core::Grade{0},key}).value();
            check(boundary.state(key)==native[cell.index],"action-first boundary disagrees with native exact action");
        }
        oracle<<left.get_str();for(const auto& q:xs)oracle<<'\t'<<q.value.get_str();for(const auto& q:fs)oracle<<'\t'<<q.value.get_str();oracle<<'\n';
        check(d.resolve(d.intern(boundary.descriptor())).state(10)==boundary.state(10),"derived chain binding");
    }
    auto zero=NativeAccessionRequest::sparse_chain(core::Grade{1},{1000},{{1000,Q{0}}});
    const auto zero_handle=d.intern(zero);zero.coefficients.clear();check(d.intern(zero)==zero_handle,"supported zero descriptor normalization");
    auto partial=NativeAccessionRequest::cochain(0,core::Grade{0});partial.inline_value=true;partial.all_cells=false;partial.cells={10};
    const auto f=d.resolve(d.intern(partial));
    const auto x=d.resolve(d.intern(NativeAccessionRequest::sparse_chain(core::Grade{0},{10,11},{{10,Q{2}}})));
    rejects([&]{f.pair_with(x);},"partial cochain undefined at supported zero was silently extended");
    rejects([&]{f.coboundary();},"partial cochain global coboundary");
    auto bad=zero;bad.cells.push_back(1000);rejects([&]{d.intern(bad);},"duplicate support accepted");
    bad=zero;bad.coefficients={{1001,Q{1}}};rejects([&]{d.intern(bad);},"value outside support accepted");
    bad=zero;bad.coefficients={{1000,Q{1}},{1000,Q{-1}}};rejects([&]{d.intern(bad);},"duplicate coefficient aggregated silently");
    auto empty=zero;empty.cells.clear();empty.coefficients.clear();
    check(d.resolve(d.intern(empty)).support_count()==0,"empty selection confused with full support");
}
void temporal_and_failures(const std::filesystem::path& out) {
    auto e=endpoint();bool throw_selector=false;NativeAccessionDirectory* owner=nullptr;bool reenter=false;
    auto selector=[&](auto)->std::uint64_t {
        if(throw_selector)throw std::runtime_error("injected directory selector failure");
        if(reenter)owner->size();
        return 0;
    };
    NativeAccessionDirectory d(e,800,queries(),selector);owner=&d;
    const auto tensor_h=d.intern(NativeAccessionRequest::chain(101,core::Grade{1}));
    const auto bits_h=d.intern(aspect());const auto obs_h=d.intern(obs(4,1001));
    const auto op_h=d.intern(obs(3,20));
    auto old=d.resolve(tensor_h);auto bits=d.resolve(bits_h);auto old_obs=d.resolve(obs_h).observation();auto old_op=d.resolve(op_h).observation();
    auto old_descriptor=d.descriptor(bits_h);auto next=e;next.sequence=1;next.source=source(8,2,true);
    next.tensors[0]=native_accession_fixture::field(101,1,8,9);
    numeric::ExactMatrix m(2,2);m.set(0,0,Q{1});m.set(1,1,Q{99});next.codes->operations[0].kernel=temporal_matrix_tensor(m);
    const auto segment=make_temporal_segment(e,next);const auto bytes=encode_temporal_segment(segment);save(out/"update.rfcxt",bytes);
    const auto before=d.stats();throw_selector=true;
    rejects([&]{d.advance(segment);},"failed directory publication succeeded");throw_selector=false;
    check(d.sequence()==0 && d.resolve(tensor_h).state(1000)==Q{0},"failed update changed endpoint/handle");
    check(d.observation_stats().evaluated==8,"failed update changed E stats");
    check(d.stats().cold_bindings==before.cold_bindings,"failed update changed index stats");
    reenter=true;rejects([&]{d.advance(segment);},"directory callback reentry accepted");reenter=false;
    auto invalid=segment;invalid.target_endpoint_hash^=1;
    rejects([&]{d.advance(invalid);},"wrong target binding accepted");
    d.advance(decode_temporal_segment(bytes));
    check(d.sequence()==1 && old.sequence()==0 && old.state(1000)==Q{0},"snapshot pin did not survive update");
    check(bits.source_bit(9001)==false,"old source-bit pin changed");
    rejects([&]{d.resolve(bits_h);},"old handle resolved new source");
    rejects([&]{d.intern(old_descriptor);},"stale descriptor rebound without correspondence");
    check(d.resolve(d.find(aspect()).value()).source_bit(9001),"source-only aspect update not visible");
    check(d.resolve(d.find(obs(4,1001)).value()).observation()->bytes!=old_obs->bytes,"E source dependency not invalidated");
    check(d.resolve(d.find(NativeAccessionRequest::chain(101,core::Grade{1})).value()).state(1000)==Q{9},"new field missing");
    check(d.resolve(d.find(obs(3,20)).value()).observation()->bytes!=old_op->bytes,"unused operation direction erased");
    (void)op_h;
    const auto seed=encode_temporal_endpoint(e);MappedRfcx mapped(out/"source.rfcx");
    auto disk=decode_temporal_endpoint(mapped.materialize(),seed);
    disk=replay_temporal_segment(disk,decode_temporal_segment(bytes));
    NativeAccessionDirectory rebuilt(disk,800,queries());
    const auto desc=d.descriptor(d.find(aspect()).value());
    check(rebuilt.descriptor(rebuilt.intern(desc))==desc,"disk rebuilt descriptor context differs");
    save(out/"target.rfcxe",encode_temporal_endpoint(disk));
    // An unbound request explicitly subscribes to a stable key over time. Missing
    // subjects may become dormant; invalid kind/carrier conversions do not.
    auto request_missing=request(NativeAccessionKind::Tensor,999);request_missing.missing=NativeMissingPolicy::Dormant;
    const auto missing=d.intern(request_missing);check(!d.active(missing),"missing subject not dormant");
    rejects([&]{d.resolve(missing);},"dormant view usable");
    auto born=next;born.sequence=2;auto t=born.tensors[3];t.key=999;born.tensors.push_back(t);
    d.advance(make_temporal_segment(next,born));check(d.active(d.find(request_missing).value()),"dormant subject did not activate");
    auto dead=born;dead.sequence=3;dead.tensors.pop_back();d.advance(make_temporal_segment(born,dead));
    check(!d.active(d.find(request_missing).value()),"removed subject not dormant");
    const auto pinned=d.resolve(d.find(NativeAccessionRequest::chain(101,core::Grade{1})).value());
    auto removed=dead;removed.sequence=4;removed.tensors.erase(removed.tensors.begin());
    rejects([&]{d.advance(make_temporal_segment(dead,removed));},"required subject removal published");
    check(d.sequence()==3 && pinned.state(1000)==Q{9},"failed required binding changed state");
    d.advance(make_temporal_segment(dead,removed),std::vector<NativeAccessionRequest>{aspect()});
    check(d.size()==1 && d.sequence()==4,"atomic request replacement failed");
    auto escaped=[&]() {
        NativeAccessionDirectory short_lived(e,42,queries());
        return short_lived.resolve(short_lived.intern(request(NativeAccessionKind::DeclaredValue,301))).declared_value();
    }();
    check(std::get<bool>(std::get<V::Product>(escaped->data).elements[0].data),"aliasing payload did not outlive directory");
    // E publications are shared immutably, forks update independently.
    RelationalCodeObservationStore store(e,queries());auto pin=store.pin_snapshot();auto o=store.pin_observation({4,1001});
    auto fork=store.fork();fork->advance(segment);
    check(store.sequence()==0 && fork->sequence()==1 && pin->sequence==0,"fork mutated original E publication");
    check(o->bytes==store.observation({4,1001}).bytes,"pinned E observation changed");
}
void schema_evolution() {
    auto e=endpoint();NativeAccessionDirectory d(e,800,queries());
    auto r=request(NativeAccessionKind::DeclaredValue,301);r.factors={501};auto h=d.intern(r);auto old=d.resolve(h);
    d.intern(aspect());auto next=e;next.sequence=1;
    auto defs=next.declarations->registry.definitions();
    std::vector<composite::DeclarationMap> maps;
    for(auto& def:defs) {
        const auto from=def.id;def.id.version=2;
        for(auto& f:def.factors)f.type.version=2;
        if(def.id.key==2)std::reverse(def.symbols.begin(),def.symbols.end());
        if(def.id.key==4)std::reverse(def.factors.begin(),def.factors.end());
        maps.push_back({from,def.id,composite::DeclarationMapKind::Recode,{}});
    }
    next.declarations->registry=composite::DeclarationRegistry(defs);
    const composite::DeclarationRecoding recode(e.declarations->registry,next.declarations->registry,maps);
    for(auto& v:next.declarations->values){v.value=recode.apply(v.type,v.value);v.type.version=2;}
    next.declarations->source_layouts[0].type.version=2;
    const auto update=make_temporal_declaration_update(e.declarations,next.declarations,maps);
    const auto segment=make_temporal_segment(e,next,{}, {},RelationalCodeEncoding::Automatic,{},update);
    d.advance(segment);
    check(old.declaration()->id.version==1 && d.resolve(d.find(r).value()).declaration()->id.version==2,"declaration version pinning");
    check(std::get<V::Symbol>(old.declared_value()->data).key==std::get<V::Symbol>(d.resolve(d.find(r).value()).declared_value()->data).key,
          "factor order changed stable accession value");
    check(d.resolve(d.find(aspect()).value()).source_bit(9000),"schema-recoded source view failed");
}
void rectangular_and_aliases() {
    auto e=endpoint();
    e.carriers.push_back({9,TemporalCarrierKind::Explicit,core::Grade{0},{},900,numeric::IndexDomain::linear(8)});
    e.tensors.push_back(native_accession_fixture::field(203,9,8,1));
    NativeAccessionDirectory directory(e,800,queries());
    const auto op=directory.resolve(directory.intern(request(NativeAccessionKind::Operator,201)));
    const auto wrong=directory.resolve(directory.intern(request(NativeAccessionKind::Field,203)));
    rejects([&]{op.apply(wrong);},"equal dimensions substituted for carrier identity");
    auto partial=NativeAccessionRequest::chain(101,core::Grade{1});partial.all_cells=false;partial.cells={1000};
    rejects([&]{op.apply(directory.resolve(directory.intern(partial)));},"partial field silently zero-extended");
    const auto r=NativeAccessionRequest::chain(101,core::Grade{1});
    const auto h=directory.intern(r);const auto pinned=directory.resolve(h);
    auto next=endpoint(9);next.sequence=1;
    // A new retained relation with supported zero is not an absent relation.
    auto vector=next.tensors[0].tensor.values().entries();
    std::vector<numeric::SparseRationalEntry> values(vector.begin(),vector.end());
    std::erase_if(values,[](const auto& x){return x.index==8;});
    next.tensors[0].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(9),numeric::StructuralSupport::full(9),numeric::SparseRationalVector(9,values));
    directory.advance(make_temporal_segment(e,next),std::vector<NativeAccessionRequest>{r});
    const auto current=directory.resolve(directory.find(r).value());
    check(pinned.support_count()==8 && current.support_count()==9 && current.state(1008)==Q{0},"rectangular birth/support evolution");
    check(!pinned.state(1008) && current.cell(1008).occurrence_count()==3,"birth missing retained incidence");
    auto moved=current;auto copy=std::move(moved);
    check(moved.state(1008)==copy.state(1008),"moved immutable pin invalidated source alias");
    auto descriptor=current.descriptor();auto descriptor_copy=std::move(descriptor);
    check(descriptor==descriptor_copy,"moved descriptor loses binding");
    auto detached=[&]{NativeAccessionDirectory local(e,9);return local.cell({core::Grade{1},1000}).occurrence(2);}();
    check(!detached.aspect("exists") && detached.parent_cell().key().key==1000,"detached occurrence/parent lifetime");
    // Registration copies caller-owned vectors before publishing their support.
    auto selected=NativeAccessionRequest::sparse_chain(core::Grade{1},{1000,1001},{{1000,Q{7}}});
    auto* raw=selected.coefficients.data();const auto selected_handle=directory.intern(std::move(selected));raw->value=99;
    check(directory.resolve(selected_handle).state(1000)==Q{7},"inline request retained mutable caller storage");
    // Field-structure edits must not be mistaken for numerical zero changes.
    auto support_only=next;support_only.sequence=2;
    support_only.tensors[0].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(9),
        numeric::StructuralSupport::sparse(9,{1,2,3,4,5,6,7,8}),numeric::SparseRationalVector(9,values));
    // Remove the existing zero at coordinate zero; all nonzero entries remain.
    directory.reconfigure({r});directory.advance(make_temporal_segment(next,support_only));
    check(!directory.resolve(directory.find(r).value()).state(1000),"supported zero removal invisible to accession");
    check(current.state(1000)==Q{0},"support-only update mutated old pin");
}
void wide_and_scaling(const std::filesystem::path& out) {
    auto e=endpoint(44,129);NativeAccessionDirectory d(e,800);
    for(std::uint64_t slot=0;slot<130;++slot) {
        const auto h=d.intern(aspect(10000+slot));const auto view=d.resolve(h);
        for(std::size_t a=0;a<131;++a) {
            const bool expected=a==0?slot%3!=2:a==1?slot%3==1:(slot+a-2)%3==0;
            check(view.source_bit(9000+a)==expected,"source-backed 131-bit layout mismatch");
        }
    }
    const auto first=d.resolve(d.handle_at(0));const auto last=d.resolve(d.handle_at(129));
    check(first.endpoint().get()==last.endpoint().get(),"each bit view copied source endpoint");
    std::ofstream counts(out/"lookup_counts.tsv");
    for(const std::size_t count:{8u,128u,2048u}) {
        auto large=endpoint(count);NativeAccessionDirectory dir(large,800);
        auto request_full=NativeAccessionRequest::chain(101,core::Grade{1});const auto h=dir.intern(request_full);
        const auto before=dir.stats();const auto start=std::chrono::steady_clock::now();
        std::uint64_t sum=0;for(std::size_t i=0;i<10000;++i)sum+=dir.resolve(h).sequence();
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        const auto after=dir.stats();
        check(after.handle_resolutions-before.handle_resolutions==10000,"known-handle resolve counter");
        check(after.cold_bindings==before.cold_bindings && after.descriptor_hashes==before.descriptor_hashes &&
              after.bucket_comparisons==before.bucket_comparisons && after.indexed_records==before.indexed_records,
              "hot handle lookup repeated source indexing/key work");
        check(dir.resolve(h).support_count()==count,"full support not lazy");
        counts<<count<<'\t'<<10000<<'\t'<<elapsed<<'\t'<<after.binding_bytes<<'\t'<<sum<<'\n';
    }
}
void higher_and_arena() {
    core::RelationSignature sig;const auto edge=sig.add_type("edge");const auto face=sig.add_type("face");
    core::RelationalComplexBuilder builder(sig);builder.declare_grade(core::Grade{3});
    const auto a=builder.add_grade_one(core::RelationKey{1},edge,{{10,true,false,Q{1},{},core::OccurrenceKey{1}},
        {11,true,true,Q{1},{},core::OccurrenceKey{2}}});
    const auto b=builder.add_grade_one(core::RelationKey{2},edge,{{11,true,false,Q{1},{},core::OccurrenceKey{3}},
        {10,true,true,Q{1},{},core::OccurrenceKey{4}}});
    builder.add_higher(core::Grade{2},core::RelationKey{3},face,{{a.index,true,false,Q{1},{},core::OccurrenceKey{5}},
        {b.index,true,false,Q{1},{},core::OccurrenceKey{6}}});
    NativeAccessionDirectory higher(TemporalEndpoint(builder.freeze()),800);
    auto cell=NativeAccessionRequest::sparse_chain(core::Grade{2},{3},{{3,Q{7,13}}});
    const auto x=higher.resolve(higher.intern(cell));const auto twice=x.boundary().boundary();
    check(twice.support_count()==2 && twice.state(10)==Q{0} && twice.state(11)==Q{0},"higher boundary cancellation erased support");
    auto q=NativeAccessionRequest::cochain(0,core::Grade{0});q.inline_value=true;q.coefficients={{10,Q{4}},{11,Q{7}}};
    const auto dd=higher.resolve(higher.intern(q)).coboundary().coboundary();
    check(dd.state(3)==Q{0},"cochain differential does not square to zero");
    auto empty=NativeAccessionRequest::chain(0,core::Grade{3});empty.inline_value=true;
    check(higher.resolve(higher.intern(empty)).support_count()==0,"declared empty grade not accessible");
    auto e=endpoint();NativeAccessionDirectory d(e,800,queries());const auto r=NativeAccessionRequest::chain(101,core::Grade{1});
    const auto before=d.resolve(d.intern(r));auto next=e;next.sequence=1;next.source=source(8,2,false,true);
    for(std::size_t tensor=0;tensor<2;++tensor) {
        std::vector<numeric::SparseRationalEntry> entries;
        for(const auto& value:e.tensors[tensor].tensor.values().entries())entries.push_back({7-value.index,value.value});
        next.tensors[tensor].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(8),numeric::StructuralSupport::full(8),numeric::SparseRationalVector(8,entries));
    }
    auto matrix=temporal_tensor_matrix(next.tensors[4]);numeric::ExactMatrix target(matrix.rows(),matrix.cols());
    for(const auto& [ij,value]:matrix.entries())target.set(ij.first,7-ij.second,value);
    next.tensors[4].tensor=temporal_matrix_tensor(target);
    d.advance(make_temporal_segment(e,next));const auto after=d.resolve(d.find(r).value());
    for(std::uint64_t key=1000;key<1008;++key) {
        check(after.state(key)==before.state(key),"arena reordering changed stable chain coordinates");
        for(std::size_t i=0;i<3;++i)check(after.cell(key).occurrence(i).key()==before.cell(key).occurrence(i).key(),"arena offset used as occurrence identity");
    }
    check(d.observation_stats().evaluated==0,"harmless arena reorder invalidated source observations");
}
void bound_code_invalidation() {
    auto e=endpoint();auto definitions=e.declarations->registry.definitions();
    definitions.push_back(D::rational_tensor(id(6),{{1,2,{10,11}}}));
    e.declarations->registry=composite::DeclarationRegistry(definitions);e.declarations->code_types={{1,id(6)}};
    NativeAccessionDirectory d(e,800,queries());const auto r=obs(2,10);const auto h=d.intern(r);const auto old=d.resolve(h).observation();
    const auto code=d.resolve(d.intern(request(NativeAccessionKind::CodeSection,10)));
    check(code.declaration()->id==id(6),"code accession loses F2 interpretation binding");
    auto next=e;next.sequence=1;definitions=next.declarations->registry.definitions();
    for(auto& definition:definitions)if(definition.id==id(6))definition.id.version=2;
    next.declarations->registry=composite::DeclarationRegistry(definitions);next.declarations->code_types={{1,id(6,2)}};
    auto maps=composite::identity_declaration_maps(e.declarations->registry);
    for(auto& map:maps)if(map.from==id(6))map.to=id(6,2);
    const auto update=make_temporal_declaration_update(e.declarations,next.declarations,maps);
    d.advance(make_temporal_segment(e,next,{}, {},RelationalCodeEncoding::Automatic,{},update));
    check(d.observation_stats().invalidated>0 && d.resolve(d.find(r).value()).observation()->bytes!=old->bytes,
          "reachable declaration version not observed through E");
    auto irrelevant=next;irrelevant.sequence=2;
    std::get<V::Product>(irrelevant.declarations->values[0].value.data).elements[0]=V::boolean(false);
    d.advance(make_temporal_segment(next,irrelevant));
    check(d.observation_stats().evaluated==0,"unrelated declared payload invalidated code observations");
}
void source_layout_rebinding(const std::filesystem::path& out) {
    auto base=endpoint();NativeAccessionDirectory directory(base,800,queries());
    const auto request_bits=aspect();const auto old=directory.resolve(directory.intern(request_bits));
    composite::AspectSchema target_aspects;SourceDeclarationMigration migration;
    migration.base_signature=base.source.signature();
    const auto& original=migration.base_signature.aspects();
    for(std::size_t i=original.size();i-->0;) {
        const auto& a=original.at(static_cast<composite::AspectId>(i));
        target_aspects.add(a.name,a.role,!a.default_value);migration.aspects.push_back({a.name,a.name});
    }
    target_aspects.add("new-role");migration.introduced={{"new-role",true,{}}};
    migration.target_signature=core::RelationSignature(target_aspects);
    migration.target_signature.add_type("branch",core::BoundaryPolicy::DeclaredExact,3,3);
    migration.types={{"branch","branch"}};
    auto next=base;next.sequence=1;next.source=recode_temporal_source(base.source,migration);
    const auto segment=make_temporal_segment(base,next,{}, {},RelationalCodeEncoding::Automatic,migration);
    save(out/"source_migration.rfcxt",encode_temporal_segment(segment));
    directory.advance(decode_temporal_segment(encode_temporal_segment(segment)));
    const auto current=directory.resolve(directory.find(request_bits).value());
    check(current.endpoint()->source.signature().existence_aspect()!=old.endpoint()->source.signature().existence_aspect(),
          "source signature fixture did not reorder bits");
    for(std::size_t slot=0;slot<24;++slot) {
        const auto a=directory.resolve(directory.intern(aspect(10000+slot)));
        const auto occurrence=StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{10000+slot}};
        for(std::size_t factor=0;factor<4;++factor)
            check(a.source_bit(9000+factor)==base.source.grade(core::Grade{1}).aspects.get(static_cast<composite::AspectId>(factor),slot),
                  "source aspect position substituted for stable factor/name after migration");
        check(directory.occurrence(occurrence).aspect("new-role"),"explicit new aspect fill missing on retained occurrence");
    }
    const auto before=old.execute_source_program(401),after=current.execute_source_program(401);
    check(before.word_count()==after.word_count() && std::equal(before.words().begin(),before.words().end(),after.words().begin()),"source-index reorder changed bound program");
    check(directory.observation_stats().evaluated==0,"unreferenced aspect or source position triggered semantic observation change");
    check(old.endpoint()->source.signature().aspects().size()==4 && current.endpoint()->source.signature().aspects().size()==5,
          "old source schema pin changed in place");
}
void budgets_and_collision() {
    const auto e=endpoint();NativeAccessionLimits limits;limits.max_bucket_entries=4;
    NativeAccessionDirectory d(e,800,{},[](auto){return 0;},limits);
    std::vector<NativeAccessionHandle> handles;
    for(std::uint64_t key=1000;key<1004;++key)handles.push_back(d.intern(NativeAccessionRequest::sparse_chain(core::Grade{1},{key},{{key,Q{1}}})));
    const auto before=d.stats();
    rejects([&]{d.intern(NativeAccessionRequest::sparse_chain(core::Grade{1},{1004},{{1004,Q{1}}}));},"collision budget not enforced");
    check(d.size()==4 && d.stats().cold_bindings==before.cold_bindings && d.stats().bucket_comparisons==before.bucket_comparisons,
          "failed registration mutated directory stats");
    for(std::size_t i=0;i<handles.size();++i)check(d.resolve(handles[i]).state(1000+i)==Q{1},"forced bucket collision merged assertions");
    limits.max_index_entries=1;rejects([&]{NativeAccessionDirectory x(e,800,{},[](auto){return 0;},limits);},"source index budget ignored");
    limits=NativeAccessionLimits{};limits.max_binding_bytes=1;
    rejects([&]{NativeAccessionDirectory x(e,800,{},[](auto){return 0;},limits);},"snapshot witness budget ignored");
    limits=NativeAccessionLimits{};limits.max_descriptor_bytes=1;
    NativeAccessionDirectory tiny(e,800,{},[](auto){return 0;},limits);
    rejects([&]{tiny.intern(request(NativeAccessionKind::Tensor,200));},"descriptor byte budget ignored");
    limits=NativeAccessionLimits{};limits.max_action_work=1;
    NativeAccessionDirectory bounded(e,800,{},[](auto){return 0;},limits);
    const auto x=bounded.resolve(bounded.intern(NativeAccessionRequest::chain(101,core::Grade{1})));
    rejects([&]{x.boundary();},"boundary work limit ignored");
    const auto op=bounded.resolve(bounded.intern(request(NativeAccessionKind::Operator,201)));
    rejects([&]{op.apply_coordinates(numeric::SparseRationalVector(8,{{0,Q{2}}}));},"operator work limit ignored");
    for(unsigned code=11;code<256;++code) {
        auto r=request(static_cast<NativeAccessionKind>(code),200);rejects([&]{d.intern(r);},"unknown accession kind accepted");
    }
    rejects([&]{d.resolve(NativeAccessionHandle{});},"empty handle accepted");
    rejects([&]{d.handle_at(99);},"invalid handle slot accepted");
    auto r=request(NativeAccessionKind::Tensor,200);r.factors={1};rejects([&]{d.intern(r);},"extraneous factor path accepted");
}
}
int main(int argc,char** argv) {
    try {
        const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"rfc_native_accession_tests";
        std::filesystem::create_directories(out);
        basic(out);chain_laws(out);temporal_and_failures(out);schema_evolution();rectangular_and_aliases();higher_and_arena();bound_code_invalidation();source_layout_rebinding(out);wide_and_scaling(out);budgets_and_collision();
        std::cout<<"native accession: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"native accession failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
