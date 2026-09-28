#include "interpretation_fixture.hpp"
#include "rfc/database/native_query.hpp"
#include "../storage/native_accession_fixture.hpp"
#include "../storage/native_atlas_fixture.hpp"
#include <iostream>
#include <random>
#include <type_traits>

namespace {
using namespace f5_fixture;
std::size_t checks{},mutation_rejected{},mutation_valid{},prefixes{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F&& f,const char* message){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,message);}
void save(const std::filesystem::path& path,std::span<const std::byte> bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!out)throw std::runtime_error("fixture write");}
void rehash(std::vector<std::byte>& bytes){const auto h=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));for(std::size_t i=0;i<8;++i)bytes[24+i]=static_cast<std::byte>((h>>(8*i))&255u);}
void basics(const std::filesystem::path& out){
    auto original=snapshot();auto s=InterpretedSnapshot::interpret(schema(),original);const auto bytes=encode_interpreted_snapshot(s);save(out/"base.rfcxi",bytes);
    auto copy=decode_interpreted_snapshot(bytes);check(bytes==encode_interpreted_snapshot(copy),"snapshot canonical roundtrip");
    check(same(copy.export_snapshot(),original),"logical roundtrip");
    check(s.endpoint()->source.grade(core::Grade{0}).cell_count()==4,"unreferenced subject lost");
    check(s.endpoint()->source.grade(core::Grade{1}).cell_count()==4,"duplicate assertions collapsed");
    check(s.endpoint()->source.grade(core::Grade{1}).incidence_count()==4,"occurrences lost");
    check(s.participants(1,11).size()==2 && s.participants(1,11,true).size()==1,"inactive occurrence lost or activated");
    check(s.participants(1,11)[0].participant()==s.participants(1,11)[1].participant(),"repeated participant failed to share");
    check(s.participants(1,11)[0].key()!=s.participants(1,11)[1].key(),"retained occurrence identities merged");
    check(s.participants(1,12)[0].participant()==s.participants(1,11)[0].participant(),"same participant in distinct roles lost");
    check(s.reference_state(2,11)==ReferenceState::Null && s.reference_state(2,12)==ReferenceState::Present,"null versus empty role");
    check(s.reference_state(3,11)==ReferenceState::Missing,"missing role lost");
    check(composite::declared_value_equal(*s.payload(1).declared_value(),*s.payload(2).declared_value()),"duplicate payload changed");
    check(s.payload(1).descriptor()!=s.payload(2).descriptor(),"equal payload identified distinct assertion");
    const auto t=s.payload(1,{4}).tensor();check(t->support().count()==3 && t->state(1)==Q{0} && !t->state(2),"tensor zero versus absent support");
    const auto pin=s.payload(1);original.assertions[0].payload=V::boolean(false);check(composite::declared_value_equal(*pin.declared_value(),payload()),"publication aliases input");
    std::mt19937 random(271828);
    for(unsigned i=0;i<24;++i){auto reordered=snapshot();std::shuffle(reordered.subjects.begin(),reordered.subjects.end(),random);std::shuffle(reordered.assertions.begin(),reordered.assertions.end(),random);
        for(auto& a:reordered.assertions)std::reverse(a.roles.begin(),a.roles.end());
        check(encode_interpreted_snapshot(InterpretedSnapshot::interpret(schema(),reordered))==bytes,"scan/role-declaration order changed retained identity");}
    auto namespace_schema=schema();namespace_schema.source_namespace=900;auto foreign=InterpretedSnapshot::interpret(namespace_schema,snapshot());
    check(s.payload(1).descriptor()!=foreign.payload(1).descriptor(),"cross-namespace descriptor collision");
    auto empty=snapshot();empty.assertions.clear();auto no_assertions=InterpretedSnapshot::interpret(schema(),empty);
    check(no_assertions.endpoint()->source.grade_count()==2 && no_assertions.assertions(10).empty(),"empty grade lost");
    check(same(decode_interpreted_snapshot(encode_interpreted_snapshot(no_assertions)).export_snapshot(),empty),"empty-fiber snapshot roundtrip");
    auto no_roles=schema();no_roles.assertions[0].roles.clear();auto inputs=snapshot();for(auto& a:inputs.assertions)a.roles.clear();
    const auto no_edges=InterpretedSnapshot::interpret(no_roles,inputs);check(no_edges.endpoint()->source.grade(core::Grade{1}).incidence_count()==0,"empty relations lost");
    write_interpreted_snapshot(s,out/"disk.rfcxi");check(encode_interpreted_snapshot(read_interpreted_snapshot(out/"disk.rfcxi"))==bytes,"disk-only reconstruction");
    rejects([&]{write_interpreted_snapshot(s,out/"disk.rfcxi");},"overwrite accepted");
}
void temporal_checks(const std::filesystem::path& out){
    auto s=InterpretedSnapshot::interpret(schema(),snapshot());const auto old_pin=s.payload(1);
    auto next_input=next();auto target=s.replace(next_input);auto delta=s.difference(target);const auto bytes=encode_interpretation_update(delta);
    save(out/"update.rfcxiu",bytes);save(out/"target.rfcxi",encode_interpreted_snapshot(target));
    const auto replay=decode_interpreted_snapshot(encode_interpreted_snapshot(s)).apply(decode_interpretation_update(bytes));
    check(encode_interpreted_snapshot(replay)==encode_interpreted_snapshot(target),"temporal disk replay differs");
    check(delta.segment.tensors.empty(),"external value change invented primary-field innovation");
    check(old_pin.sequence()==0 && composite::declared_value_equal(*old_pin.declared_value(),payload()),"old pin rebound");
    check(replay.participants(1,11)[0].key()==s.participants(1,11)[1].key(),"ordered role evolution lost occurrence identity");
    check(replay.participants(1,11)[0].aspect("exists")==false,"inactive aspect changed on replay");
    for(const auto& old:s.lineage().subjects){const auto it=std::find(replay.lineage().subjects.begin(),replay.lineage().subjects.end(),old);check(it!=replay.lineage().subjects.end(),"subject lineage changed");}
    for(const auto& old:s.lineage().assertions)check(std::find(replay.lineage().assertions.begin(),replay.lineage().assertions.end(),old)!=replay.lineage().assertions.end(),"tombstone erased");
    auto restore_input=snapshot();restore_input.revision="snapshot/2";auto restored=target.replace(restore_input);const auto second=target.difference(restored);
    auto twice=replay.apply(decode_interpretation_update(encode_interpretation_update(second)));save(out/"update2.rfcxiu",encode_interpretation_update(second));
    check(encode_interpreted_snapshot(twice)==encode_interpreted_snapshot(restored),"two-segment replay");
    check(restored.assertions(10)==s.assertions(10),"same producer lineage did not recover retained keys");
    auto failed=delta;failed.target.subjects[0].native_key=999;rejects([&]{s.apply(failed);},"lineage reassignment accepted");
    failed=delta;failed.base_revision="foreign";rejects([&]{s.apply(failed);},"wrong producer snapshot accepted");
    failed=delta;failed.source_namespace=99;rejects([&]{s.apply(failed);},"foreign namespace accepted");
    failed=delta;failed.target.occurrences.pop_back();rejects([&]{s.apply(failed);},"omitted occurrence origin accepted");
    rejects([&]{target.apply(delta);},"stale update accepted");
    rejects([&]{s.replace(snapshot());},"reused producer revision accepted");
    auto schema_change=schema();schema_change.assertions[0].fields[0].label="different interpretation";
    rejects([&]{s.replace(schema_change,next_input);},"external declaration drift silently accepted");
    auto wrong=*s.endpoint();wrong.declarations->values[0].branch=0;
    rejects([&]{InterpretedSnapshot::restore(s.schema(),s.lineage(),wrong,s.revision());},"unrepresented endpoint declaration accepted");
    check(encode_interpreted_snapshot(s)==encode_interpreted_snapshot(InterpretedSnapshot::interpret(schema(),snapshot())),"failed update changed base");
    for(std::size_t width:{1u,2u,3u,7u,8u,31u,64u,127u,512u}){
        std::vector<std::byte> accumulated;for(std::size_t i=0;i<bytes.size();i+=width)accumulated.insert(accumulated.end(),bytes.begin()+static_cast<std::ptrdiff_t>(i),bytes.begin()+static_cast<std::ptrdiff_t>(std::min(bytes.size(),i+width)));
        check(encode_interpreted_snapshot(s.apply(decode_interpretation_update(accumulated)))==encode_interpreted_snapshot(target),"chunk-collected update replay");}
}
void declared_occurrence_order(){
    const auto s=InterpretedSnapshot::interpret(schema(),snapshot());
    auto input=snapshot();input.revision="order/1";
    std::reverse(input.assertions[0].roles[0].occurrences.begin(),input.assertions[0].roles[0].occurrences.end());
    const auto target=s.replace(input);
    check(storage::encode_rfcx(s.endpoint()->source).bytes==storage::encode_rfcx(target.endpoint()->source).bytes,"meaningful role order leaked into source arena layout");
    check(storage::encode_temporal_endpoint(*s.endpoint())!=storage::encode_temporal_endpoint(*target.endpoint()),"declared role order change vanished");
    check(target.participants(1,11)[0].key()==s.participants(1,11)[1].key(),"participants ignored declared native order");
    const auto replay=s.apply(decode_interpretation_update(encode_interpretation_update(s.difference(target))));
    check(same(replay.export_snapshot(),input),"order-only temporal replay failed");
    storage::NativeAccessionRequest order;order.kind=storage::NativeAccessionKind::DeclaredValue;order.subject=1;order.factors={3,11};
    check(std::get<V::Repeated>(target.source().access(order).declared_value()->data).elements.size()==2,"order not exposed through native declared accession");
    const auto rejected_order=[&](std::vector<V> keys,const char* message){
        auto bad=*s.endpoint();auto& product=std::get<V::Product>(bad.declarations->values[0].value.data);
        auto& orders=std::get<V::Product>(product.elements[2].data);
        orders.elements[0]=V::repeated(std::move(keys));
        rejects([&]{InterpretedSnapshot::restore(s.schema(),s.lineage(),bad,s.revision());},message);
    };
    rejected_order({V::integer(1),V::integer(1)},"duplicate role order accepted");
    rejected_order({V::integer(1),V::integer(999)},"stale role order accepted");
    rejected_order({V::integer(1),V::integer(3)},"wrong-role order accepted");
    rejected_order({V::integer(1),V::integer(4)},"foreign-owner order accepted");
    rejected_order({V::integer(1)},"incomplete order accepted");
    rejected_order({V::integer(1),V::integer(-1)},"negative order identity accepted");
    rejected_order({V::integer(1),V::integer(numeric::Integer("18446744073709551616"))},"oversized order identity accepted");
    InterpretationSchema small;small.source_namespace=801;small.binding_namespace=802;
    const composite::DeclarationId boolean{803,1,1};small.values=composite::DeclarationRegistry({composite::Declaration::boolean(boolean)});
    small.subjects={{1,"subject"}};small.assertions={{1,"branch",boolean,{{1,1,0,5,false,false}}, {}}};
    LogicalSnapshot small_input;small_input.revision="small/0";small_input.subjects={{1,"p"}};
    small_input.assertions={{{1,"a"},V::boolean(true),{{1,ReferenceState::Present,{{"x",{1,"p"},true},{"y",{1,"p"},true},{"z",{1,"p"},true}}}}}};
    InterpretationLimits generated_budget;generated_budget.declarations.max_value_nodes=8;
    rejects([&]{InterpretedSnapshot::interpret(small,small_input,generated_budget);},"generated occurrence order bypassed value-work budget");
    auto tighter=InterpretationLimits{};tighter.max_records=5000;
    check(encode_interpreted_snapshot(InterpretedSnapshot::interpret(schema(),snapshot(),tighter))==encode_interpreted_snapshot(s),"resource policy changed generated declaration identity");
}
void rejection_and_limits(){
    auto good=snapshot();auto declared=schema();
    auto x=good;x.assertions[0].origin.token.clear();rejects([&]{InterpretedSnapshot::interpret(declared,x);},"unkeyed duplicate lineage inferred");
    x=good;x.assertions.push_back(x.assertions[0]);rejects([&]{InterpretedSnapshot::interpret(declared,x);},"duplicate origin accepted");
    x=good;x.assertions[0].roles[0].occurrences[0].participant.token="missing";rejects([&]{InterpretedSnapshot::interpret(declared,x);},"dangling reference accepted");
    x=good;x.assertions[0].roles[0].occurrences[0].participant={2,"unused"};rejects([&]{InterpretedSnapshot::interpret(declared,x);},"wrong participant kind accepted");
    x=good;x.assertions[0].roles[0].occurrences[1].lineage="a";rejects([&]{InterpretedSnapshot::interpret(declared,x);},"occurrence lineage collision");
    x=good;x.assertions[0].roles[0].state=ReferenceState::Null;rejects([&]{InterpretedSnapshot::interpret(declared,x);},"null role with occurrences accepted");
    x=good;x.assertions[0].roles[0].state=static_cast<ReferenceState>(255);rejects([&]{InterpretedSnapshot::interpret(declared,x);},"unknown reference state accepted");
    x=good;x.assertions[0].roles.pop_back();rejects([&]{InterpretedSnapshot::interpret(declared,x);},"omitted role treated as null");
    x=good;x.assertions[0].payload=V::boolean(true);rejects([&]{InterpretedSnapshot::interpret(declared,x);},"wrong payload type accepted");
    auto bad_schema=declared;bad_schema.assertions[0].roles[0].maximum=1;rejects([&]{InterpretedSnapshot::interpret(bad_schema,good);},"branch occurrence cardinality ignored");
    bad_schema=declared;bad_schema.binding_namespace=503;rejects([&]{InterpretedSnapshot::interpret(bad_schema,good);},"reserved namespace collision");
    InterpretationLimits low;low.max_records=3;rejects([&]{InterpretedSnapshot::interpret(declared,good,low);},"record budget ignored");
    low={};low.max_origin_bytes=3;rejects([&]{InterpretedSnapshot::interpret(declared,good,low);},"metadata budget ignored");
    low={};low.declarations.max_value_nodes=5;rejects([&]{InterpretedSnapshot::interpret(declared,good,low);},"value work budget ignored");
    auto source=InterpretedSnapshot::interpret(declared,good);auto ledger=source.lineage();ledger.subjects[0].native_key=std::numeric_limits<std::uint64_t>::max();
    rejects([&]{InterpretedSnapshot::restore(declared,ledger,*source.endpoint(),source.revision());},"native key wraparound accepted");
    auto bytes=encode_interpreted_snapshot(source);low={};low.max_archive_bytes=bytes.size()-1;rejects([&]{decode_interpreted_snapshot(bytes,low);},"archive byte budget ignored");
    low={};low.max_archive_bytes=bytes.size();check(encode_interpreted_snapshot(decode_interpreted_snapshot(bytes,low))==bytes,"exact archive byte budget");
}
void manifests(){
    const auto input=snapshot();LogicalSnapshotManifest m;m.revision=input.revision;
    m.fragments={{"part/b",input.subjects,{input.assertions[2],input.assertions[3]}},{"part/a",input.subjects,{input.assertions[0],input.assertions[1]}}};
    auto s=interpret_logical_manifest(schema(),m);check(same(s.export_snapshot(),input),"logical fragment gluing");
    std::reverse(m.fragments.begin(),m.fragments.end());check(encode_interpreted_snapshot(interpret_logical_manifest(schema(),m))==encode_interpreted_snapshot(s),"manifest scan reorder");
    m.revision="deleted";m.deletions={{10,"a2"}};auto deleted=materialize_logical_snapshot(m);check(deleted.assertions.size()==3,"declared logical delete");
    check(s.replace(deleted).assertions(10).size()==3,"logical delete temporal update");
    auto bad=m;bad.deletions.push_back({10,"absent"});rejects([&]{materialize_logical_snapshot(bad);},"unmatched delete accepted");
    bad=m;bad.fragments[0].assertions.push_back(bad.fragments[0].assertions[0]);rejects([&]{materialize_logical_snapshot(bad);},"duplicate fragment assertion merged");
    bad=m;bad.fragments[1].identity=bad.fragments[0].identity;rejects([&]{materialize_logical_snapshot(bad);},"duplicate fragment identity accepted");
}
void context(){
    using namespace rfc::models;using namespace rfc::storage;
    const auto s=InterpretedSnapshot::interpret(schema(),snapshot());NativeAtlasManifest m;m.key=1;m.revision=1;
    NativeAtlas atlas({s.source(),NativeAtlasSource(2,777,*s.endpoint())},m);
    NativeAccessionRequest q;q.kind=NativeAccessionKind::DeclaredValue;q.subject=1;q.factors={1,2};
    const std::vector<NativeContextSelection> requests{{1,q}};
    const auto allow=[](const auto& a){return a.source==1 && a.source_namespace==501;};
    auto c=NativeConsumerContext::build(atlas,requests,allow);
    check(c.items().size()==1 && c.items()[0].provenance==NativeContextProvenance::ExactStored,"context provenance");
    const auto& value=std::get<NativeDeclaredContext>(c.items()[0].contents);check(std::get<V::Symbol>(value.value.data).key==8,"selected contextual value");
    check(value.declarations.definitions().size()==1,"context leaked unrelated declarations");
    const auto before=encode_interpreted_snapshot(s);
    auto proposal=c.propose(ProspectiveEncoding::Approximate,"model/test","opaque output/1",{std::byte{1}});
    check(proposal.encoding()==ProspectiveEncoding::Approximate && proposal.provenance().at(0).source==1,"approximate output lost provenance");
    static_assert(!std::is_convertible_v<NativeModelProposal,TemporalEndpoint>);
    check(encode_interpreted_snapshot(s)==before,"model output mutated exact endpoint");
    auto literal=c.propose(ProspectiveEncoding::ExactCandidate,"model/test","exact candidate, unvalidated",{});check(literal.encoding()==ProspectiveEncoding::ExactCandidate,"exact candidate provenance");
    rejects([&]{NativeConsumerContext::build(atlas,requests,{});},"authorization default allowed");
    rejects([&]{NativeConsumerContext::build(atlas,{{2,q}},allow);},"same raw source key granted authority");
    rejects([&]{NativeConsumerContext::build(atlas,{{1,q},{2,q}},allow);},"late denial published partial context");
    rejects([&]{c.propose(static_cast<ProspectiveEncoding>(255),"m","i",{});},"unknown model encoding");
    auto tiny=NativeContextLimits{};tiny.max_bytes=c.accounted_bytes()-1;rejects([&]{NativeConsumerContext::build(atlas,requests,allow,tiny);},"context bytes budget");
    tiny.max_bytes=c.accounted_bytes();check(NativeConsumerContext::build(atlas,requests,allow,tiny).accounted_bytes()==c.accounted_bytes(),"exact context budget");
    tiny={};tiny.max_elements=0;rejects([&]{NativeConsumerContext::build(atlas,requests,allow,tiny);},"context value budget");
    auto source_bits=q;source_bits.kind=NativeAccessionKind::SourceAspects;source_bits.subject=10;source_bits.factors.clear();source_bits.occurrence=s.participants(1,11)[0].key();
    auto b=NativeConsumerContext::build(atlas,{{1,source_bits}},allow);
    const auto& aspects=std::get<NativeAspectContext>(b.items()[0].contents);check(aspects.factors==std::vector<std::pair<std::uint64_t,bool>>{{11,true},{12,false}},"source-backed role context");
    auto inline_chain=NativeAccessionRequest::sparse_chain(core::Grade{0},{1},{{1,Q{99}}});
    rejects([&]{NativeConsumerContext::build(atlas,{{1,inline_chain}},allow);},"request literal certified as stored data");
    auto next_source=s.replace(next());NativeAtlas next_atlas({next_source.source()},m);auto changed=NativeConsumerContext::build(next_atlas,requests,allow);
    check(c.items()[0].origin.sequence==0 && changed.items()[0].origin.sequence==1,"context temporal provenance rebound");
    auto callback_atlas=atlas;std::vector<NativeContextSelection> callback_requests{{1,q},{1,q}};
    auto callback_limits=NativeContextLimits{};unsigned calls=0;
    const auto isolated=NativeConsumerContext::build(callback_atlas,callback_requests,[&](const auto&){
        if(!calls++){callback_atlas=next_atlas;callback_requests.clear();callback_limits.max_bytes=0;}return true;
    },callback_limits);
    check(isolated.items().size()==2 && isolated.items()[0].origin.sequence==0 && isolated.items()[1].origin.sequence==0,"callback produced mixed snapshot or invalidated selection");
    q.factors={1,4};auto context_only=NativeConsumerContext::build(atlas,{{1,q}},allow);
    check(context_only.items()[0].origin.sequence==0 && encode_interpreted_snapshot(s)==before,"context selection invented source update");
    // Policies remain per source even through a declared, valid common interface.
    auto glued=native_atlas_fixture::atlas();auto field=NativeAccessionRequest::chain(5001,core::Grade{0});
    const auto restricted=[](const auto& a){return a.source==2;};
    auto permitted=NativeConsumerContext::build(glued,{{2,field}},restricted);
    check(std::get<NativeChainContext>(permitted.items()[0].contents).components.size()==3,"chain context");
    rejects([&]{NativeConsumerContext::build(glued,{{3,field}},restricted);},"gluing transferred source authority");
    auto cochain=NativeAccessionRequest::cochain(5001,core::Grade{0});auto dual=NativeConsumerContext::build(glued,{{2,cochain}},restricted);
    check(std::get<NativeChainContext>(dual.items()[0].contents).pairing==NativePairing::AlgebraicDual,"cochain variance lost");
    // Check E-derived observations and stored operator channels against F3 fixture.
    auto e=native_accession_fixture::endpoint();NativeAccessionDirectory directory(e,909,native_accession_fixture::queries());
    NativeAtlas from_directory({NativeAtlasSource(8,directory)},m);
    bool observed=false;
    for(const auto& observation:directory.queries()){
        if(observation.subjects.empty())continue;
        NativeAccessionRequest r;r.kind=NativeAccessionKind::Observation;r.subject=observation.key;r.secondary=observation.subjects[0];
        auto result=NativeConsumerContext::build(from_directory,{{8,r}},[](const auto&){return true;});
        check(result.items()[0].provenance==NativeContextProvenance::ExactDerived,"observation mislabeled stored");observed=true;break;
    }
    check(observed,"derived observation context was not exercised");
}
void exact_queries(){
    InterpretationSchema s;s.source_namespace=8101;s.binding_namespace=8102;const auto id=[](std::uint64_t k){return composite::DeclarationId{8103,k,1};};using D=composite::Declaration;
    s.values=composite::DeclarationRegistry({D::integer(id(1)),D::optional(id(2),id(1)),D::boolean(id(3)),D::finite(id(4),{7,9}),D::product(id(5),{{1,id(2),0},{2,id(3),0},{3,id(4),0}})});s.assertions={{10,"scalars",id(5),{}, {}}};
    LogicalSnapshot input;input.revision="scalars";const numeric::Integer huge("1234567890123456789012345678901234567890");
    for(unsigned i=0;i<8;++i)input.assertions.push_back({{10,std::to_string(i)},V::product({i<2?V::absent():V::present(V::integer(i<4?huge:numeric::Integer{-2})),V::boolean(i%2!=0),V::symbol(i<4?7:9)}),{}});
    const auto native=InterpretedSnapshot::interpret(s,input);
    check(*sum_exact(native,10,{1})==numeric::Rational(huge*2-8),"sum overflowed machine integer");
    check(select_exact(native,10,{{1},ExactPredicateKind::IsNull,{}}).size()==2,"optional scalar null selection");
    check(select_exact(native,10,{{1},ExactPredicateKind::Greater,V::integer(0)}).size()==2,"unknown became false value instead of excluded comparison");
    check(select_exact(native,10,{{2},ExactPredicateKind::Equal,V::boolean(true)}).size()==4,"Boolean predicate");
    check(select_exact(native,10,{{3},ExactPredicateKind::NotEqual,V::symbol(7)}).size()==4,"finite predicate");
    rejects([&]{select_exact(native,10,{{3},ExactPredicateKind::Less,V::symbol(9)});},"finite states given implicit numeric ordering");
    rejects([&]{select_exact(native,10,{{1},ExactPredicateKind::Equal,V::rational(1)});},"numeric domains silently coerced");
    rejects([&]{select_exact(native,10,{{1},static_cast<ExactPredicateKind>(255),{}});},"unknown predicate no-op");
    rejects([&]{sum_exact(native,10,{1},7);},"query work budget ignored");
    input.assertions.clear();const auto empty=InterpretedSnapshot::interpret(s,input);check(!sum_exact(empty,10,{1}),"empty sum fabricated zero");
    rejects([&]{select_exact(empty,10,{{3},ExactPredicateKind::Less,V::symbol(7)});},"empty source skipped query typing");
}
void constructor_edges(const std::filesystem::path& out){
    using D=composite::Declaration;const auto did=[](std::uint64_t k){return composite::DeclarationId{9203,k,1};};
    InterpretationSchema declaration;declaration.source_namespace=9201;declaration.binding_namespace=9202;
    declaration.values=composite::DeclarationRegistry({D::boolean(did(1)),D::finite(did(2),{8,9}),D::integer(did(3)),D::rational(did(4)),
        D::optional(did(5),did(4)),D::repeated(did(6),did(5),0,8),D::alternative(did(7),{{10,did(3),0},{20,did(6),0}}),
        D::rational_tensor(did(8),{{1,65536,{}}}),D::product(did(9),{{1,did(1),0},{2,did(2),0},{3,did(3),0},{4,did(4),0},{5,did(5),0},{6,did(6),0},{7,did(7),0},{8,did(8),0}})});
    declaration.subjects={{1,"participants"}};declaration.assertions={{10,"collective",did(9),{{11,1,2,2,false,false},{12,1,3,3,false,false}}, {}}};
    const auto tensor_value=V::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({65536}),numeric::StructuralSupport::packed(65536,{7}),numeric::SparseRationalVector(65536,{{7,Q{1,7}}})));
    const auto sequence=V::repeated({V::absent(),V::present(V::rational(0))});
    const auto value=V::product({V::boolean(true),V::symbol(9),V::integer(numeric::Integer("-999999999999999999999999999999999999999")),V::rational(Q{17,23}),V::absent(),sequence,V::alternative(20,sequence),tensor_value});
    LogicalSnapshot input;input.revision="constructors/0";input.subjects={{1,"a"},{1,"b"},{1,"c"}};
    input.assertions={{{10,"group"},value,{{11,ReferenceState::Present,{{"l1",{1,"a"},true},{"l2",{1,"b"},false}}},{12,ReferenceState::Present,{{"r1",{1,"a"},true},{"r2",{1,"b"},true},{"r3",{1,"c"},true}}}}}};
    const auto native=InterpretedSnapshot::interpret(declaration,input);const auto bytes=encode_interpreted_snapshot(native);save(out/"constructors.rfcxi",bytes);
    check(same(decode_interpreted_snapshot(bytes).export_snapshot(),input),"nine-constructor logical roundtrip");
    check(native.assertions(10).size()==1 && native.endpoint()->source.grade(core::Grade{1}).incidence_count()==5,"collective branching became Cartesian pair assertions");
    check(native.participants(1,11).size()==2 && native.participants(1,12).size()==3,"many-to-many roles changed");
    check(native.participants(1,11,true).size()==1,"inactive collective occurrence lost");
    using namespace storage;using namespace models;NativeAtlasManifest m;m.key=8;m.revision=1;NativeAtlas atlas({native.source()},m);
    NativeAccessionRequest request;request.kind=NativeAccessionKind::DeclaredValue;request.subject=1;request.factors={1,8};
    NativeContextLimits policy;policy.max_bytes=1024;
    const auto context=NativeConsumerContext::build(atlas,{{1,request}},[](const auto&){return true;},policy);
    const auto& projected=std::get<NativeDeclaredContext>(context.items()[0].contents);
    const auto& tensor=std::get<numeric::SparseRationalTensor>(projected.value.data);
    check(tensor.support().packed_bits()==nullptr && tensor.support().count()==1 && tensor.state(7)==Q{1,7},"context copied oversized support backing instead of canonical selected value");
    check(context.accounted_bytes()<1024,"context allocation accounting");
    auto bad=input;std::get<V::Product>(bad.assertions[0].payload.data).elements[6]=V::alternative(30,V::integer(1));
    rejects([&]{InterpretedSnapshot::interpret(declaration,bad);},"unknown alternative branch admitted");
    InterpretationLimits low;low.declarations.max_materialized_bytes=1024;
    rejects([&]{InterpretedSnapshot::interpret(declaration,input,low);},"input packed backing bypassed physical copy budget");
}
void wire(const std::filesystem::path& out){
    const auto s=InterpretedSnapshot::interpret(schema(),snapshot());const auto bytes=encode_interpreted_snapshot(s);
    const auto update=encode_interpretation_update(s.difference(s.replace(next())));
    for(std::size_t n=0;n<bytes.size();++n){rejects([&]{decode_interpreted_snapshot(std::span<const std::byte>(bytes).first(n));},"truncated snapshot accepted");++prefixes;}
    for(std::size_t n=0;n<update.size();++n){rejects([&]{decode_interpretation_update(std::span<const std::byte>(update).first(n));},"truncated update accepted");++prefixes;}
    std::mt19937 random(161803);
    for(unsigned i=0;i<500;++i){auto mutated=bytes;const auto p=32+random()%(bytes.size()-32);mutated[p]^=static_cast<std::byte>(1u<<(random()%8));rehash(mutated);
        std::optional<InterpretedSnapshot> result;
        try{result=decode_interpreted_snapshot(mutated);}
        catch(const std::exception&){++mutation_rejected;++checks;continue;}
        check(encode_interpreted_snapshot(*result)==mutated,"accepted noncanonical mutation");++mutation_valid;}
    auto trailing=bytes;trailing.push_back(std::byte{0});rejects([&]{decode_interpreted_snapshot(trailing);},"trailing snapshot byte");
    auto version=bytes;version[8]=std::byte{2};rejects([&]{decode_interpreted_snapshot(version);},"unknown version accepted");
    auto flags=bytes;flags[12]=std::byte{1};rejects([&]{decode_interpreted_snapshot(flags);},"unknown flags accepted");
    auto invalid_update=s.difference(s.replace(next()));invalid_update.target.assertions.erase(invalid_update.target.assertions.begin());
    rejects([&]{encode_interpretation_update(invalid_update);},"writer admitted lineage deletion");
    invalid_update=s.difference(s.replace(next()));invalid_update.target_revision=invalid_update.base_revision;
    rejects([&]{encode_interpretation_update(invalid_update);},"writer admitted repeated revision");
    invalid_update=s.difference(s.replace(next()));invalid_update.target.occurrences.back().origin.lineage=std::string(4097,'x');
    rejects([&]{encode_interpretation_update(invalid_update);},"writer bypassed metadata cap");
    auto unkeyed=s.lineage();unkeyed.assertions[0].origin.token.clear();rejects([&]{InterpretedSnapshot::restore(schema(),unkeyed,*s.endpoint(),s.revision());},"bad lineage restore");
    std::ofstream report(out/"results.json");report<<"{\"checks\":"<<checks<<",\"truncations\":"<<prefixes<<",\"mutations_rejected\":"<<mutation_rejected<<",\"mutations_valid\":"<<mutation_valid<<"}\n";
}
}
int main(int argc,char** argv){try{
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"rfc-f5-native-tests";
    if(std::filesystem::exists(out)){if(argc>1)throw std::invalid_argument("fresh output directory required");std::filesystem::remove_all(out);}std::filesystem::create_directories(out);
    basics(out);temporal_checks(out);declared_occurrence_order();rejection_and_limits();manifests();context();exact_queries();constructor_edges(out);wire(out);
    std::cout<<checks<<" native interpretation/context checks passed; truncations="<<prefixes<<" rejected_mutations="<<mutation_rejected<<" valid_mutations="<<mutation_valid<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
