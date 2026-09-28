#include "rfc/storage/relational_code_observations.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <iostream>
#include <random>

using namespace rfc;
using namespace rfc::storage;
namespace {
using R = numeric::Rational;
using V = RelationalCodeValue;
using K = RelationalCodeObservationKind;
using OP = RelationalCodeOpcode;
using TK = RelationalCodeTypeKind;
using Query = RelationalCodeObservationQuery;
using Bytes = std::vector<std::byte>;
std::size_t checks{};
void check(bool yes, const char *message) { ++checks; if (!yes) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char *message) {
    bool caught = false;
    try { action(); } catch (const std::exception &) { caught = true; }
    check(caught, message);
}
void write_bytes(const std::filesystem::path &path, const Bytes &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("fixture write failed");
}
core::RelationalComplex source(std::set<std::uint64_t> flips = {}, bool role = false,
                               bool reorder = false, std::size_t count = 4, R q = 1) {
    core::RelationSignature sig;
    const auto custom = sig.aspects().add("unobserved_role");
    const auto type = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(sig);
    b.declare_witness_identity(10); b.declare_witness_identity(11);
    for (std::size_t i = 0; i < count; ++i) {
        const auto edge = reorder ? count - i - 1 : i;
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for (unsigned side = 0; side < 2; ++side) {
            const auto id = 1000 + 2 * edge + side;
            occurrences.push_back({10 + side, true, (side == 0) != flips.contains(id), q,
                                   {{custom, role}}, core::OccurrenceKey{id}});
        }
        b.add_grade_one(core::RelationKey{100 + edge}, type, occurrences);
    }
    return b.freeze();
}
V vector(R a = 1, R b = 0, bool support_second = true) {
    std::vector<numeric::SparseRationalEntry> entries;
    if (a != 0) entries.push_back({0, a});
    if (b != 0) entries.push_back({1, b});
    return V{numeric::SparseRationalTensor(numeric::IndexDomain({2}),
        numeric::StructuralSupport::sparse(2, support_second ? std::vector<std::size_t>{0,1} : std::vector<std::size_t>{0}),
        numeric::SparseRationalVector(2, entries))};
}
RelationalCodeOperation identity(std::uint64_t key, std::uint64_t type) {
    RelationalCodeOperation op; op.key = key; op.input = op.output = type; return op;
}
RelationalCodeOperation linear(std::uint64_t key, std::uint64_t input, std::uint64_t output,
                               const std::vector<std::vector<R>> &values) {
    RelationalCodeOperation op;
    op.key = key; op.input = input; op.output = output; op.opcode = OP::Linear;
    numeric::ExactMatrix m(values.size(), values.at(0).size());
    for (std::size_t i=0;i<values.size();++i) for(std::size_t j=0;j<values[i].size();++j) m.set(i,j,values[i][j]);
    op.kernel = temporal_matrix_tensor(m); return op;
}
RelationalCodeOperation word(std::uint64_t key, std::uint64_t input, std::uint64_t output,
                             std::vector<std::uint64_t> terms) {
    RelationalCodeOperation op; op.key = key; op.input = input; op.output = output;
    op.opcode = OP::Word; op.word = std::move(terms); return op;
}
TemporalEndpoint fixture(std::size_t sections = 24) {
    TemporalEndpoint e(source());
    RelationalCodeState c; c.namespace_key = 900;
    c.types = {{1,TK::Rational,{0,42},{}}, {2,TK::Rational,{7},{}}, {3,TK::Finite,{0,1},{}},
               {5,TK::Product,{},{{700,1,1,{}},{701,3,0,{}}}}, {6,TK::Finite,{10,20},{}}};
    c.types.push_back(relational_aspect_code_type(e.source,4,3,
        {{500,e.source.signature().existence_aspect()}, {501,e.source.signature().orientation_aspect()}}));
    c.fields = {100,200,300};
    c.operations = {linear(10,1,1,{{1,0},{0,1}}), linear(11,1,2,{{1,0}}),
                    word(20,1,2,{10,11}), word(21,1,2,{20}), identity(30,4), identity(32,3)};
    RelationalCodeOperation binary;
    binary.key=31; binary.input=4; binary.output=3; binary.opcode=OP::Binary;
    binary.binary = composite::BinaryProgram(3);
    binary.binary->append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});
    binary.binary->append({composite::BinaryOpcode::LoadAspect,1,0,0,0,1});
    binary.binary->append({composite::BinaryOpcode::AndNot,2,0,1}); binary.registers={2};
    c.operations.push_back(binary);
    RelationalCodeOperation projection;
    projection.key=40; projection.input=5; projection.output=2; projection.opcode=OP::FactorProjection;
    projection.legs={{700,20}}; c.operations.push_back(projection);
    // The symbols deliberately equal real operation IDs; never traverse them.
    auto finite=identity(50,6); finite.opcode=OP::FiniteMap; finite.word={20,10}; c.operations.push_back(finite);
    for (std::uint64_t i=1;i<=sections;++i) {
        RelationalCodeSection s; s.key=i; s.field=100; s.type=1; s.value=vector(); c.sections.push_back(s);
    }
    for(std::uint64_t i=0;i<8;++i)
        c.sections.push_back(relational_aspect_code_section(10001+i,200,4,{core::Grade{1},core::OccurrenceKey{1000+i}}));
    RelationalCodeSection p; p.key=20001; p.field=300; p.type=5;
    p.value=V{V::Product{vector(), V{std::uint64_t{1}}}}; c.sections.push_back(p);
    c.interfaces={{1,1,2,11,11}, {2,10001,10003,30,30}};
    e.codes=std::move(c); validate_temporal_endpoint(e); return e;
}
std::vector<Query> queries() {
    return {{1,K::Section,0,100,{}}, {2,K::Application,21,100,{}}, {3,K::Operation,0,{}, {10,11,20,21}},
            {4,K::Section,0,200,{}}, {5,K::Application,31,200,{}}, {6,K::Interface,0,{}, {}},
            {7,K::Application,40,300,{}}, {8,K::Section,0,300,{}}, {9,K::Operation,0,{}, {50}}};
}
RelationalCodeSection &section(TemporalEndpoint &e,std::uint64_t key) {
    for(auto &s:e.codes->sections) if(s.key==key)return s;
    throw std::runtime_error("test section missing");
}
RelationalCodeOperation &operation(TemporalEndpoint &e,std::uint64_t key) {
    for(auto &o:e.codes->operations) if(o.key==key)return o;
    throw std::runtime_error("test operation missing");
}
// Independent oracle: existing full materialization and algebra APIs; none of
// the new dependency graph, source-view resolver or evaluator is used here.
std::map<RelationalCodeIdentity,RelationalCodeObservation>
reference(const TemporalEndpoint &endpoint,const std::vector<Query> &qs) {
    std::map<RelationalCodeIdentity,RelationalCodeObservation> out;
    if(!endpoint.codes)return out;
    const auto resolved=materialize_relational_code_views(endpoint);
    RelationalCodeAlgebra algebra(resolved);
    const auto ss=relational_code_detail::index(resolved.sections);
    const auto applied_bytes=[&](std::uint64_t subject,const V &value,std::uint64_t type) {
        RelationalCodeState single;
        single.namespace_key=resolved.namespace_key; single.types=resolved.types;
        auto s=*ss.at(subject); s.type=type; s.value=value;
        single.fields={s.field}; single.sections={std::move(s)};
        return relational_code_section_bytes(single,subject);
    };
    const auto blob=[](temporal_codec_detail::Writer &w,const Bytes &bytes) {w.u64(bytes.size());w.raw(bytes);};
    for(const auto &q:qs) {
        const auto wanted=[&](std::uint64_t subject,std::optional<std::uint64_t> field={}) {
            return (!q.field || q.field==field) && (q.subjects.empty() ||
                    std::find(q.subjects.begin(),q.subjects.end(),subject)!=q.subjects.end());
        };
        if(q.kind==K::Operation) {
            for(const auto &op:resolved.operations)if(wanted(op.key))
                out[{q.key,op.key}]={relational_code_operation_bytes(resolved,op.key),{}};
        } else if(q.kind==K::Interface) {
            for(const auto &i:resolved.interfaces)if(wanted(i.key)) {
                auto l=algebra.apply(i.left_restriction,ss.at(i.left)->value,q.work_budget);
                auto r=algebra.apply(i.right_restriction,ss.at(i.right)->value,q.work_budget);
                bool equal=relational_code_detail::value_equal(l,r);
                temporal_codec_detail::Writer w;w.u64(0x3145434146435252ULL);w.u64(resolved.namespace_key);
                temporal_codec_detail::code_interface(w,i);
                blob(w,relational_code_operation_bytes(resolved,i.left_restriction));
                blob(w,relational_code_operation_bytes(resolved,i.right_restriction));
                blob(w,applied_bytes(i.left,l,algebra.operation(i.left_restriction).output));
                blob(w,applied_bytes(i.right,r,algebra.operation(i.right_restriction).output));w.u8(equal);
                out[{q.key,i.key}]={w.bytes(),equal};
            }
        } else for(const auto &s:resolved.sections)if(wanted(s.key,s.field)) {
            if(q.kind==K::Section)out[{q.key,s.key}]={relational_code_section_bytes(resolved,s.key),{}};
            else {
                auto value=algebra.apply(q.operation,s.value,q.work_budget);
                temporal_codec_detail::Writer w;w.u64(0x314c505041435252ULL);
                blob(w,relational_code_operation_bytes(resolved,q.operation));
                blob(w,applied_bytes(s.key,value,algebra.operation(q.operation).output));
                out[{q.key,s.key}]={w.bytes(),{}};
            }
        }
    }
    return out;
}
void oracle(RelationalCodeObservationStore &store) {
    const auto expected=reference(store.snapshot(),store.queries());
    check(expected.size()==store.size(),"oracle observation count");
    for(const auto &[id,value]:expected) {
        check(store.contains(id),"oracle missing identity");
        check(store.observation(id).bytes==value.bytes,"oracle byte mismatch");
        check(store.observation(id).compatible==value.compatible,"oracle glue mismatch");
        std::vector<RelationalCodeIdentity> matches;
        for(const auto &[other,v]:expected)if(v.bytes==value.bytes)matches.push_back(other);
        check(store.find(value.bytes)==matches,"collision-safe query mismatch");
    }
}
Bytes observations(RelationalCodeObservationStore &store) {
    temporal_codec_detail::Writer w;
    for(auto id:store.identities()) {
        const auto value=store.observation(id);w.u64(id.namespace_key);w.u64(id.section);
        w.u64(value.bytes.size());w.raw(value.bytes);
    }
    return w.bytes();
}
TemporalSegment step(RelationalCodeObservationStore &store,TemporalEndpoint target,
                     RelationalCodeEncoding encoding=RelationalCodeEncoding::Automatic,
                     std::vector<RelationalCodeTransport> transports={}) {
    const auto base=store.snapshot();target.sequence=base.sequence+1;
    const auto segment=make_temporal_segment(base,target,{},std::move(transports),encoding);
    const auto bytes=encode_temporal_segment(segment);
    const auto decoded=decode_temporal_segment(bytes);
    store.advance(decoded);
    check(encode_temporal_endpoint(store.snapshot())==encode_temporal_endpoint(target),"indexed replay changed endpoint");
    oracle(store);return segment;
}
void values_and_dependencies(const std::filesystem::path &dir) {
    auto e=fixture();RelationalCodeObservationStore store(e,queries(),[](auto){return 0;});oracle(store);
    check(store.stats().evaluated==store.size(),"initial evaluation count");
    write_rfcx(e.source,dir/"observation_base.rfcx");write_temporal_endpoint(e,dir/"observation_base.rfcxe");
    write_bytes(dir/"initial.rfco",observations(store));
    auto next=store.snapshot();section(next,3).value=vector(8);
    const auto delta=step(store,next,RelationalCodeEncoding::Sparse);
    check(store.stats().evaluated==2 && store.stats().bytes_changed==2,"one section must affect two observations only");
    write_bytes(dir/"section_change.rfcxt",encode_temporal_segment(delta));
    next=store.snapshot();section(next,1).value=vector(3);step(store,next);
    check(store.stats().evaluated==3,"section and glued-interface invalidation");
    check(store.observation({6,1}).compatible==false,"obstruction must persist without repair");
    next=store.snapshot();section(next,2).value=vector(3);step(store,next);
    check(store.observation({6,1}).compatible==true,"glue recovery missing");
    next=store.snapshot();section(next,4).value=vector(1,0,false);step(store,next);
    check(store.stats().evaluated==2 && store.stats().bytes_changed==1,"support-only edit or unchanged projection mishandled");
    next=store.snapshot();next.finite_systems.push_back({700,TemporalFiniteValueKind::SignedInteger,{},{},{}});
    step(store,next);check(store.stats().evaluated==0,"unobserved finite channel invalidated codes");
    next=store.snapshot();step(store,next);check(store.stats().evaluated==0 && store.stats().reused==store.size(),"no-op replay evaluated observations");
    // Directory order is storage layout, not observation meaning.
    next=store.snapshot();std::reverse(next.codes->sections.begin(),next.codes->sections.end());
    std::reverse(next.codes->operations.begin(),next.codes->operations.end());step(store,next);
    check(store.stats().evaluated==0,"harmless code directory reorder invalidated observations");
    auto before=observations(store);auto bad=make_temporal_segment(store.snapshot(),[&]{auto n=store.snapshot();++n.sequence;return n;}());
    bad.target_endpoint_hash^=1;rejects([&]{store.advance(bad);},"bad target binding accepted");
    check(observations(store)==before,"failed replay changed observations");
    write_bytes(dir/"values_final.rfco",observations(store));
}
void source_views(const std::filesystem::path &dir) {
    RelationalCodeObservationStore store(fixture(),queries(),[](auto){return 1;});
    auto n=store.snapshot();n.source=source({1000});const auto delta=step(store,n);
    check(!delta.codes,"source-only change unexpectedly copied code layer");
    check(store.stats().evaluated==3,"source aspect dependency did not select its section/application/interface");
    check(store.observation({6,2}).compatible==false,"source view interface obstruction missed");
    write_bytes(dir/"source_bit.rfcxt",encode_temporal_segment(delta));
    n=store.snapshot();n.source=source({1000},true);step(store,n);
    check(store.stats().evaluated==0,"unobserved aspect caused false dependency");
    n=store.snapshot();n.source=source({1000},true,true);step(store,n);
    check(store.stats().evaluated==0,"arena reorder broke stable-occurrence dependencies");
    n=store.snapshot();n.source=source({1000},true,true,4,R{0});step(store,n);
    check(store.stats().evaluated==0,"unobserved coefficient was treated as code state");
    n=store.snapshot();n.source=source({1000,1002},true,true,4,R{0});step(store,n);
    check(store.observation({6,2}).compatible==true,"source view gluing did not recover");
    write_bytes(dir/"source_final.rfco",observations(store));
}
void operation_closures(const std::filesystem::path &dir) {
    RelationalCodeObservationStore store(fixture(),queries());
    const auto old_applied=store.observation({2,1}).bytes;
    auto n=store.snapshot();operation(n,10)=linear(10,1,1,{{1,9},{0,1}});
    const auto delta=step(store,n);
    check(store.stats().evaluated==28,"transitive word/factor-map dependency closure incorrect");
    check(store.observation({2,1}).bytes!=old_applied,"unused operator direction lost from application provenance");
    check(store.observation({1,1}).bytes==relational_code_section_bytes(*n.codes,1),"operation change altered retained field");
    // New dependency branch introduced by changing a root; subsequent edits
    // must follow the new closure, and discarded dependencies must disappear.
    n=store.snapshot();n.codes->operations.push_back(linear(60,1,2,{{2,0}}));operation(n,21)=word(21,1,2,{60});
    step(store,n);
    n=store.snapshot();operation(n,60)=linear(60,1,2,{{5,0}});step(store,n);
    check(store.stats().evaluated==25,"new transitive dependency not registered");
    n=store.snapshot();operation(n,10)=linear(10,1,1,{{3,9},{0,1}});step(store,n);
    check(store.stats().evaluated==3,"obsolete dependency still invalidates root application");
    // Finite map symbols 10/20 must not become dependencies on operations 10/20.
    check(store.observation({9,50}).bytes==relational_code_operation_bytes(*n.codes,50),"finite-map observation changed");
    write_bytes(dir/"operation_change.rfcxt",encode_temporal_segment(delta));
    write_bytes(dir/"operations_final.rfco",observations(store));
}
void selection_and_carriers() {
    auto qs=queries();RelationalCodeObservationStore store(fixture(),qs);
    auto n=store.snapshot();auto born=section(n,3);born.key=99;n.codes->sections.push_back(born);step(store,n);
    check(store.stats().added==2 && store.stats().evaluated==2,"automatic section birth subscription");
    n=store.snapshot();std::erase_if(n.codes->sections,[](const auto &s){return s.key==99;});step(store,n);
    check(store.stats().removed==2 && store.stats().evaluated==0,"automatic section removal");
    qs.push_back({100,K::Section,0,{}, {99}});store.reconfigure(qs);
    check(!store.contains({100,99}) && store.stats().evaluated==0,"dormant filter not supported");
    n=store.snapshot();born=section(n,3);born.key=99;n.codes->sections.push_back(born);step(store,n);
    check(store.stats().added==3,"dormant filter did not activate");
    qs[0].subjects={1,2};store.reconfigure(qs);oracle(store);
    check(store.stats().removed==23 && store.stats().evaluated==0,"filter pruning must not recompute surviving records");
    // Removal/birth of the code layer retains the subscriptions, not stale hits.
    n=store.snapshot();const auto codes=n.codes;n.codes.reset();step(store,n);
    check(store.size()==0,"code layer removal left observations");
    n=store.snapshot();n.codes=codes;step(store,n);check(store.size()!=0,"code layer birth not indexed");
    // Carrier change: target query can change atomically with temporal replay.
    auto e=fixture(2);std::erase_if(e.codes->interfaces,[](const auto &i){return i.key==1;});
    RelationalCodeObservationStore rectangular(e,{{1,K::Section,0,100,{}},{2,K::Application,21,100,{1}}});
    n=e;++n.sequence;section(n,1).type=2;
    section(n,1).value=V{numeric::SparseRationalTensor(numeric::IndexDomain({1}),numeric::StructuralSupport::full(1),
        numeric::SparseRationalVector(1,{{0,R{7}}}))};n.codes->operations.push_back(identity(70,2));
    auto segment=make_temporal_segment(e,n,{},{{1,20}});
    const auto before=observations(rectangular);
    rejects([&]{rectangular.advance(segment);},"ill-typed target query was silently ignored");
    check(rectangular.sequence()==0 && observations(rectangular)==before,"failed query plan partially published");
    rectangular.advance(segment,std::vector<Query>{{1,K::Section,0,100,{}},{2,K::Application,70,100,{1}}});
    oracle(rectangular);check(rectangular.sequence()==1,"rectangular replay failed");
}
void failure_atomicity() {
    bool fail=false;RelationalCodeObservationStore *self=nullptr;
    auto selector=[&](std::span<const std::byte>) -> std::uint64_t {
        if(fail)throw std::runtime_error("injected selector failure");
        return 0;
    };
    RelationalCodeObservationStore store(fixture(),queries(),selector);self=&store;(void)self;
    const auto before=observations(store), seed=encode_temporal_endpoint(store.snapshot());
    auto n=store.snapshot();++n.sequence;section(n,1).value=vector(9);
    const auto segment=make_temporal_segment(store.snapshot(),n);
    fail=true;rejects([&]{store.advance(segment);},"throwing selector published update");fail=false;
    check(observations(store)==before && encode_temporal_endpoint(store.snapshot())==seed,"atomic selector rollback");
    oracle(store);store.advance(segment);oracle(store);
    auto qs=queries();qs[1].work_budget=1;
    const auto save=observations(store); const auto seq=store.sequence();
    rejects([&]{store.reconfigure(qs);},"work budget ignored");
    check(observations(store)==save && store.sequence()==seq,"budget failure changed store");
    auto bad=queries();bad.push_back(bad[0]);rejects([&]{store.reconfigure(bad);},"duplicate query accepted");
    bad=queries();bad[0].kind=static_cast<K>(255);rejects([&]{store.reconfigure(bad);},"unknown query kind accepted");
    bad=queries();bad[0].subjects={1,1};rejects([&]{store.reconfigure(bad);},"duplicate subject accepted");
    bad=queries();bad[0].subjects={0};rejects([&]{store.reconfigure(bad);},"zero subject accepted");
    bad=queries();bad[0].operation=10;rejects([&]{store.reconfigure(bad);},"invalid operation lane accepted");
    bad=queries();bad[2].field=100;rejects([&]{store.reconfigure(bad);},"operation field filter accepted");
    // Selector reentry cannot mutate, query or copy the in-flight owning store.
    bool reenter=false;RelationalCodeObservationStore *ptr=nullptr;
    auto callback=[&](std::span<const std::byte>) -> std::uint64_t {
        if(reenter)(void)ptr->snapshot();
        return 3;
    };
    RelationalCodeObservationStore guarded(fixture(),queries(),callback);ptr=&guarded;
    n=guarded.snapshot();++n.sequence;section(n,1).value=vector(9);
    const auto delta=make_temporal_segment(guarded.snapshot(),n);reenter=true;
    rejects([&]{guarded.advance(delta);},"store selector read reentry accepted");reenter=false;
    check(guarded.sequence()==0,"reentry changed store");oracle(guarded);
    reenter=true;rejects([&]{(void)guarded.find(before);},"query selector reentry accepted");reenter=false;
    oracle(guarded);
    for(unsigned which=0;which<5;++which) {
        RelationalCodeObservationLimits limits;
        if(which==0)limits.max_queries=0;
        if(which==1)limits.max_observations=1;
        if(which==2)limits.max_dependency_edges=1;
        if(which==3)limits.max_dependencies=1;
        if(which==4)limits.max_canonical_bytes=1;
        rejects([&]{RelationalCodeObservationStore limited(fixture(),queries(),[](auto){return 0;},limits);},"index resource limit ignored");
    }
    // Invalid endpoints reject even when no query would read the bad record.
    auto invalid=fixture();invalid.codes->sections.back().type=999;
    rejects([&]{RelationalCodeObservationStore s(invalid,{});},"unobserved invalid endpoint accepted");
}

void late_failure_and_interfaces() {
    unsigned selects=0; bool injecting=false;
    RelationalCodeObservationStore store(fixture(),queries(),[&](auto) -> std::uint64_t {
        if(injecting && ++selects==2)throw std::runtime_error("late selector failure");
        return 0;
    });
    const auto before=observations(store);const auto saved_stats=store.stats();
    auto n=store.snapshot();++n.sequence;
    std::erase_if(n.codes->sections,[](const auto &s){return s.key==8;});
    section(n,3).value=vector(4);section(n,4).value=vector(5);
    auto delta=make_temporal_segment(store.snapshot(),n);
    injecting=true;rejects([&]{store.advance(delta);},"late selection failure published partial index");injecting=false;
    check(selects==2 && store.sequence()==0 && observations(store)==before,"late failure rollback lost values/deletions");
    check(store.stats().evaluated==saved_stats.evaluated && store.stats().added==saved_stats.added,"failed update published counters");
    oracle(store);store.advance(delta);oracle(store);
    n=store.snapshot();n.codes->interfaces[0].left_restriction=20;n.codes->interfaces[0].right_restriction=20;
    step(store,n);check(store.stats().evaluated==1 && store.stats().bytes_changed==1,"interface declaration edit missed");
    n=store.snapshot();operation(n,10)=linear(10,1,1,{{2,0},{0,1}});step(store,n);
    check(store.stats().evaluated==28,"interface's newly introduced operation dependency missing");
    // A factor ignored by the declared projection still causes conservative
    // evaluation, but it must not be fabricated into a changed projected value.
    n=store.snapshot();std::get<2>(section(n,20001).value.data)[1]=V{std::uint64_t{0}};step(store,n);
    check(store.stats().evaluated==2 && store.stats().bytes_changed==1,"ignored factor projection semantics");
    // Switching from a bound view to equal materialized data changes future
    // dependencies, even though this endpoint's section bytes are unchanged.
    n=store.snapshot();auto resolved=materialize_relational_code_views(n);
    const auto rs=relational_code_detail::index(resolved.sections);
    section(n,10001)=*rs.at(10001);step(store,n);
    check(store.stats().evaluated==3 && store.stats().bytes_changed==0,"source-to-value dependency transition");
    n=store.snapshot();n.source=source({1000});step(store,n);
    check(store.stats().evaluated==0,"detached source-view dependency was retained");
    const auto surviving=store.observation({1,1});n=store.snapshot();section(n,1).value=vector(17);step(store,n);
    check(surviving.bytes!=store.observation({1,1}).bytes,"observation result did not own its bytes");
}
core::RelationalComplex wide_source(bool change) {
    core::RelationSignature sig;std::vector<composite::AspectId> aspects;
    for(unsigned a=0;a<129;++a)aspects.push_back(sig.aspects().add("bit_"+std::to_string(a)));
    const auto type=sig.add_type("wide",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(sig);b.declare_witness_identity(10);b.declare_witness_identity(11);
    for(unsigned e=0;e<65;++e) {
        std::vector<core::BoundaryOccurrenceInput> legs;
        for(unsigned side=0;side<2;++side) {
            const auto slot=e*2+side;std::vector<core::AspectValue> values;
            for(unsigned a=0;a<129;++a) {
                bool bit=(slot+a)%3==0;
                if(change && a==128 && (slot==63 || slot==64 || slot==129))bit=!bit;
                values.push_back({aspects[a],bit});
            }
            legs.push_back({10+side,slot%7!=0,side==0,R{0},values,core::OccurrenceKey{1000+slot}});
        }
        b.add_grade_one(core::RelationKey{100+e},type,legs);
    }
    return b.freeze();
}
void packed_domains(const std::filesystem::path &dir) {
    TemporalEndpoint e(wide_source(false));RelationalCodeState c;c.namespace_key=300;
    c.types={{1,TK::Finite,{0,1},{}}};std::vector<std::pair<std::uint64_t,composite::AspectId>> factors;
    for(unsigned a=0;a<131;++a)factors.push_back({a+1,static_cast<composite::AspectId>(a)});
    c.types.push_back(relational_aspect_code_type(e.source,2,1,factors));c.fields={1};
    for(std::uint64_t s=0;s<130;++s)c.sections.push_back(relational_aspect_code_section(s+1,1,2,{core::Grade{1},core::OccurrenceKey{1000+s}}));
    RelationalCodeOperation op;op.key=1;op.input=2;op.output=1;op.opcode=OP::Binary;
    op.binary=composite::BinaryProgram(1);op.binary->append({composite::BinaryOpcode::LoadAspect,0,0,0,0,130});op.registers={0};
    c.operations={op};e.codes=c;
    RelationalCodeObservationStore store(e,{{1,K::Section,0,1,{}},{2,K::Application,1,1,{}}});oracle(store);
    auto n=e;n.source=wide_source(true);const auto delta=step(store,n);
    check(!delta.codes && store.stats().evaluated==6,"wide source-only sparse invalidation");
    const auto bits=execute_relational_aspect_code(store.snapshot(),1,core::Grade{1});
    auto materialized=materialize_relational_code_views(store.snapshot());
    for(std::size_t s=0;s<130;++s) {
        const auto &v=std::get<2>(materialized.sections[s].value.data);
        check(bits[0].get(s)==(std::get<1>(v[130].data)!=0),"packed and view operation disagree");
        for(unsigned a=0;a<131;++a)
            check(std::get<1>(v[a].data)==static_cast<std::uint64_t>(n.source.grade(core::Grade{1}).aspects.get(static_cast<composite::AspectId>(a),s)),"wide source bit lost");
    }
    write_rfcx(e.source,dir/"wide_base.rfcx");write_temporal_endpoint(e,dir/"wide_base.rfcxe");
    write_bytes(dir/"wide_source.rfcxt",encode_temporal_segment(delta));write_bytes(dir/"wide_final.rfco",observations(store));
}

void randomized_history(const std::filesystem::path &dir) {
    std::mt19937 rng(90217);auto e=fixture(8);
    RelationalCodeObservationStore store(e,queries(),[](auto){return 0;});
    std::set<std::uint64_t> flips;
    Bytes history;
    for(unsigned t=0;t<48;++t) {
        auto n=store.snapshot();
        const auto choice=rng()%6; const auto pick=rng()%8; const auto a=rng()%11; const auto b=rng()%5;
        switch(choice) {
        case 0: section(n,1+pick).value=vector(R{static_cast<long>(a)-5},R{static_cast<long>(b)-2});break;
        case 1: {
            const auto key=1000+pick;if(!flips.erase(key))flips.insert(key);n.source=source(flips);break;
        }
        case 2: operation(n,10)=linear(10,1,1,{{R{static_cast<long>(a)},R{static_cast<long>(b)}},{0,1}});break;
        case 3: std::get<2>(section(n,20001).value.data)[0]=vector(R{static_cast<long>(a)});break;
        case 4: operation(n,11)=linear(11,1,2,{{1,R{static_cast<long>(b)}}});break;
        case 5: section(n,1+pick).value=vector(R{static_cast<long>(a)},0,(b%2)==0);break;
        }
        const auto delta=step(store,n,t%2 ? RelationalCodeEncoding::Full : RelationalCodeEncoding::Sparse);
        const auto frame=encode_temporal_segment(delta);history.insert(history.end(),frame.begin(),frame.end());
    }
    write_bytes(dir/"mixed_history.rfcxt",history);write_bytes(dir/"mixed_final.rfco",observations(store));
    // Incremental streaming delivery feeds real decoded segments to the index.
    for(std::size_t chunk: {std::size_t{1},std::size_t{7},std::size_t{64},std::size_t{257},history.size()}) {
        RelationalCodeObservationStore replay(e,queries());
        TemporalSegmentStreamDecoder decoder;
        for(std::size_t p=0;p<history.size();) {const auto n=std::min(chunk,history.size()-p);decoder.feed(std::span<const std::byte>(history).subspan(p,n),[&](const auto &delta, auto){replay.advance(delta);});p+=n;}
        decoder.finish();check(observations(replay)==observations(store),"chunked indexed history diverged");
    }
}
void bounded_work(const std::filesystem::path &dir) {
    auto e=fixture(1024);std::vector<Query> qs={{1,K::Section,0,100,{}},{2,K::Application,21,100,{}}};
    RelationalCodeObservationStore store(e,qs);auto n=e;++n.sequence;section(n,513).value=vector(13);
    const auto delta=make_temporal_segment(e,n);store.advance(delta);
    check(store.stats().evaluated==2 && store.stats().reused==2046,"sparse update evaluated unrelated observations");
    check(store.stats().operation_applications==1,"sparse update performed unrelated operation applications");
    RelationalCodeObservationStore rebuilt(n,qs);
    check(observations(store)==observations(rebuilt),"large cached observations differ from rebuild");
    write_bytes(dir/"large_final.rfco",observations(store));
    std::cout<<"sections=1024 observations="<<store.size()<<" evaluated="<<store.stats().evaluated
             <<" reused="<<store.stats().reused<<" operation_applications="<<store.stats().operation_applications<<'\n';
}
}
int main(int argc,char **argv) {
    try {
        const std::filesystem::path dir=argc>1?argv[1]:std::filesystem::temp_directory_path()/"rfc-observation-tests";
        std::filesystem::create_directories(dir);
        const auto run=[](const char *name,auto test){test();std::cout<<"PASS "<<name<<'\n';};
        run("value/support dependencies and oracle",[&]{values_and_dependencies(dir);});
        run("source-bound views and stable identities",[&]{source_views(dir);});
        run("transitive operation closure",[&]{operation_closures(dir);});
        run("subscriptions and rectangular carriers",selection_and_carriers);
        run("failure atomicity and limits",failure_atomicity);
        run("late rollback and evolving interface dependencies",late_failure_and_interfaces);
        run("wide packed source domains",[&]{packed_domains(dir);});
        run("randomized exact history and streaming",[&]{randomized_history(dir);});
        run("bounded observation evaluation",[&]{bounded_work(dir);});
        std::cout<<"relational_code_observation_checks="<<checks<<'\n';
    } catch(const std::exception &error) {std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
