#include "declaration_temporal_fixture.hpp"
#include "rfc/field/exact_hodge.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

using namespace f2_fixture;
namespace {
using Bytes=std::vector<std::byte>;
std::size_t checks{},truncations{},mutations_rejected{},mutations_accepted{},mutations_replayed{};
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F&& f,const char* message){bool threw=false;try{f();}catch(const std::exception&){threw=true;}check(threw,message);}
void save(const std::filesystem::path& p,const Bytes& bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!f)throw std::runtime_error("fixture write");}
Bytes load(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);f.seekg(0,std::ios::end);const auto size=f.tellg();if(size<0)throw std::runtime_error("fixture read");Bytes b(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),size);if(!f)throw std::runtime_error("fixture read");return b;}
void retained_resource_policy_tests() {
    auto endpoint=TemporalEndpoint(source());
    TemporalDeclarationState state;state.namespace_key=988;
    state.resource_limits.max_integer_bits=2000000;
    const auto integer_id=id(987);
    state.registry=DeclarationRegistry({Declaration::integer(integer_id)},state.resource_limits);
    numeric::Integer value;mpz_setbit(value.get_mpz_t(),1599999);
    state.values.push_back({1,integer_id,0,0,{},{},V::integer(value)});
    endpoint.declarations=state;
    validate_temporal_endpoint(endpoint);
    const auto wire=encode_temporal_endpoint(endpoint);
    TemporalReadLimits read;read.declarations=state.resource_limits;
    const auto decoded=decode_temporal_endpoint(endpoint.source,wire,read);
    check(declared_value_equal(decoded.declarations->values.front().value,state.values.front().value),"raised resource policy lost by owned codec");
    check(decoded.declarations->resource_limits.max_integer_bits==read.declarations.max_integer_bits,"reader policy not retained by decoded owner");
    check(encode_temporal_endpoint(decoded)==wire,"resource policy changed canonical bytes");
    rejects([&]{(void)decode_temporal_endpoint(endpoint.source,wire);},"default read integer budget bypassed");
    auto tight=state.resource_limits;tight.max_integer_bits=100;
    rejects([&]{validate_temporal_declarations(endpoint.source,{},state,tight);},"explicit tighter owned policy ignored");
}
void source_tests() {
    const auto base=source();const auto m=source_map(base.signature());const auto next=recode_temporal_source(base,m);
    check(next.grade_count()==4 && next.grade(core::Grade{3}).cell_count()==0,"empty grade lost");
    SourceDeclarationLimits exact;exact.max_aspect_bytes=103;
    check(encode_rfcx(recode_temporal_source(base,m,exact)).bytes==encode_rfcx(next).bytes,"source exact aspect budget");
    --exact.max_aspect_bytes;rejects([&]{(void)recode_temporal_source(base,m,exact);},"source aspect amplification not bounded");
    exact={};exact.max_occurrences=4;rejects([&]{(void)recode_temporal_source(base,m,exact);},"source occurrence budget ignored");
    exact={};exact.max_aspect_bytes=0;rejects([&]{(void)recode_temporal_source(base,m,exact);},"zero source budget accepted");
    const auto& old=base.grade(core::Grade{1});const auto& now=next.grade(core::Grade{1});
    check(old.incidence_count()==now.incidence_count() && old.occurrence_identities==now.occurrence_identities,"retained occurrence identity lost");
    for(std::size_t i=0;i<5;++i){
        for(const auto& p:m.aspects)check(old.aspects.get(base.signature().aspects().id(p.from),i)==now.aspects.get(next.signature().aspects().id(p.to),i),"source bit not transported");
        check(now.aspects.get(next.signature().aspects().id("introduced"),i)==(i==1),"new aspect default applied retroactively");
        check(base.incidence_coefficient(core::Grade{1},i)==next.incidence_coefficient(core::Grade{1},i),"coefficient changed during source recoding");
    }
    check(!now.aspects.get(next.signature().existence_aspect(),4),"inactive occurrence dropped/reactivated");
    check(now.relation_types.at(0)==2 && now.relation_types.at(1)==1,"type renumbering guessed");
    auto explicit_m=m;explicit_m.introduced[0].fill.reset();explicit_m.introduced[0].assignments.clear();
    for(std::size_t i=0;i<5;++i)explicit_m.introduced[0].assignments.push_back({{core::Grade{1},core::OccurrenceKey{100+i}},i==1});
    check(encode_rfcx(recode_temporal_source(base,explicit_m)).bytes==encode_rfcx(next).bytes,"explicit assignments disagree with fill");
    explicit_m.introduced[0].assignments.pop_back();rejects([&]{(void)recode_temporal_source(base,explicit_m);},"missing inactive assignment accepted");
    auto bad=m;bad.aspects.pop_back();rejects([&]{(void)recode_temporal_source(base,bad);},"incomplete aspect correspondence");
    bad=m;bad.aspects[0].to="sign";rejects([&]{(void)recode_temporal_source(base,bad);},"aspect role reinterpreted");
    bad=m;bad.types[0].to="empty-v2";rejects([&]{(void)recode_temporal_source(base,bad);},"noninjective type map accepted");
    bad=m;bad.introduced.clear();rejects([&]{(void)recode_temporal_source(base,bad);},"implicit new aspect default");
    bad=m;bad.introduced[0].assignments.push_back({{core::Grade{1},core::OccurrenceKey{999}},true});rejects([&]{(void)recode_temporal_source(base,bad);},"foreign source assignment");
    bad=m;bad.introduced[0].assignments.push_back(bad.introduced[0].assignments.front());rejects([&]{(void)recode_temporal_source(base,bad);},"duplicate source assignment");
    bad=m;bad.base_signature.aspects().add("wrong");rejects([&]{(void)recode_temporal_source(base,bad);},"wrong owning base signature");
    rejects([&]{(void)make_temporal_source_patch(base,next);},"legacy alignment silently accepts source migration");
    // Explicit changing declarations are not a license to change constructor semantics.
    auto constrained=m.target_signature;core::RelationSignature target(constrained.aspects());
    target.add_type("new-type");target.add_type("empty-v2");target.add_type("collective-v2",core::BoundaryPolicy::DeclaredExact,1,100);
    bad=m;bad.target_signature=target;rejects([&]{(void)recode_temporal_source(base,bad);},"rule weakening/strengthening called a rename");
}
void recoding_tests() {
    auto old=definitions(),next=definitions(true);const DeclarationRecoding recoding(old,next,maps());
    for(auto symbol:{10u,20u,30u,40u,50u,60u}) {
        auto v=V::product({V::symbol(symbol),V::absent()});
        auto expected=V::product({V::absent(),V::symbol(symbol),V::boolean(true)});
        check(declared_value_equal(recoding.apply(id(6),v),expected),"product role permutation/fill");
    }
    auto bad=maps();bad.pop_back();rejects([&]{(void)DeclarationRecoding(old,next,bad);},"partial declaration map accepted");
    bad=maps();bad[5].fills.clear();rejects([&]{(void)DeclarationRecoding(old,next,bad);},"product default guessed");
    bad=maps();bad[5].kind=DeclarationMapKind::Recode;rejects([&]{(void)DeclarationRecoding(old,next,bad);},"embedding certified bijective");
    bad=maps();bad[0].kind=static_cast<DeclarationMapKind>(255);rejects([&]{(void)DeclarationRecoding(old,next,bad);},"unknown declaration morphism");
    auto defs=old.definitions();defs[0]=Declaration::integer(id(1));rejects([&]{(void)DeclarationRecoding(old,DeclarationRegistry(defs),identity_declaration_maps(old));},"immutable identity redefined");
    // Every permutation is checked against stable symbols, not ordinal bit coincidence.
    std::vector<std::uint64_t> perm{10,20,30,40,50,60};
    const DeclarationRegistry finite({Declaration::finite(id(50),perm)});
    do {
        const DeclarationRegistry target({Declaration::finite(id(50,2),perm)});
        const DeclarationRecoding map(finite,target,{{id(50),id(50,2),DeclarationMapKind::Recode,{}}});
        const CompiledLayout layout(target,id(50,2));
        for(auto symbol:perm) {
            const auto value=map.apply(id(50),V::symbol(symbol));
            check(declared_value_equal(value,V::symbol(symbol)),"finite recoding changed symbol");
            check(declared_value_equal(decode_declared_bits(layout,encode_declared_bits(layout,value)),value),"recoded bit exactness");
        }
    }while(std::next_permutation(perm.begin(),perm.end()));
    const DeclarationRegistry a({Declaration::integer(id(60)),Declaration::boolean(id(61)),
        Declaration::alternative(id(62),{{7,id(60),0},{8,id(61),0}}),Declaration::repeated(id(63),id(62),0,3)});
    const DeclarationRegistry b({Declaration::integer(id(60,2)),Declaration::boolean(id(61,2)),
        Declaration::alternative(id(62,2),{{8,id(61,2),0},{7,id(60,2),0},{9,id(60,2),0}}),Declaration::repeated(id(63,2),id(62,2),0,5)});
    const DeclarationRecoding alt(a,b,{{id(60),id(60,2),DeclarationMapKind::Recode,{}},{id(61),id(61,2),DeclarationMapKind::Recode,{}},
        {id(62),id(62,2),DeclarationMapKind::Embedding,{}},{id(63),id(63,2),DeclarationMapKind::Embedding,{}}});
    numeric::Integer big=1;big<<=1024;big+=3;
    const auto repeated=V::repeated({V::alternative(7,V::integer(big)),V::alternative(8,V::boolean(false))});
    check(declared_value_equal(alt.apply(id(63),repeated),repeated),"exact nested repeated/alternative recode");
    // Tensor coordinate and axis permutations preserve supported zero, not only nonzero entries.
    const DeclarationRegistry ta({Declaration::rational_tensor(id(70),{{1,2,{10,20}},{2,3,{30,40,50}}})});
    const DeclarationRegistry tb({Declaration::rational_tensor(id(70,2),{{2,4,{50,30,40,99}},{1,3,{20,10,88}}})});
    const DeclarationRecoding tensor_map(ta,tb,{{id(70),id(70,2),DeclarationMapKind::Embedding,{}}});
    const V tv=V::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({2,3}),numeric::StructuralSupport::sparse(6,{0,2,5}),
        numeric::SparseRationalVector(6,{{0,Q{1,3}},{5,Q{-7}}})));
    const auto mapped=tensor_map.apply(id(70),tv);const auto& t=std::get<numeric::SparseRationalTensor>(mapped.data);
    check(t.domain()==numeric::IndexDomain({4,3}),"target tensor dimensions");
    check(t.support().count()==3 && t.support().contains(4) && t.support().contains(1) && t.support().contains(0),"tensor support embedding");
    const auto expected=V::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({4,3}),numeric::StructuralSupport::sparse(12,{0,1,4}),
        numeric::SparseRationalVector(12,{{4,Q{1,3}},{0,Q{-7}}})));
    check(declared_value_equal(mapped,expected),"tensor stable-coordinate mapping");
    auto failed=std::vector<DeclarationMap>{{id(70),id(70,2),DeclarationMapKind::Recode,{}}};
    rejects([&]{(void)DeclarationRecoding(ta,tb,failed);},"tensor growth called pure recode");
    DeclarationLimits tiny;tiny.max_value_nodes=2;
    rejects([&]{(void)DeclarationRecoding(ta,tb,{{id(70),id(70,2),DeclarationMapKind::Embedding,{}}},tiny).apply(id(70),tv);},"tensor migration ignored work budget");
}
void wide_program_tests(const std::filesystem::path& out) {
    core::RelationSignature sig;
    for(std::size_t i=0;i<129;++i)sig.aspects().add("a"+std::to_string(i));
    const auto type=sig.add_type("wide");core::RelationalComplexBuilder builder(sig);
    std::vector<core::BoundaryOccurrenceInput> occurrences;
    for(std::size_t slot=0;slot<130;++slot){
        std::vector<core::AspectValue> aspects;
        for(std::size_t a=2;a<131;++a)aspects.push_back({static_cast<AspectId>(a),(slot+a)%3==0});
        occurrences.push_back({slot%2+10,slot%3!=0,slot%2!=0,Q{0},aspects,core::OccurrenceKey{1000+slot}});
    }
    builder.add_grade_one(core::RelationKey{10},type,occurrences);builder.declare_grade(core::Grade{2});
    TemporalEndpoint base(builder.freeze());TemporalDeclarationState state;state.namespace_key=888;
    std::vector<DeclarationFactor> factors;
    TemporalSourceAspectLayout layout;layout.key=1;layout.type=id(101);
    for(std::size_t a=0;a<131;++a){factors.push_back({1000+a,id(100),0});layout.factors.push_back({1000+a,sig.aspects().at(static_cast<AspectId>(a)).name});}
    state.registry=DeclarationRegistry({Declaration::boolean(id(100)),Declaration::product(id(101),factors)});
    state.source_layouts={layout};TemporalSourceLayoutProgram op;op.key=1;op.layout=1;op.program=BinaryProgram(3);op.output=2;
    op.program.append({BinaryOpcode::LoadAspect,0,0,0,0,0});op.program.append({BinaryOpcode::LoadAspect,1,0,0,0,128});
    op.program.append({BinaryOpcode::Xor,2,0,1});state.programs={op};base.declarations=state;
    AspectSchema aspects;SourceDeclarationMigration migration;migration.base_signature=sig;
    for(std::size_t a=131;a-->0;){const auto& old=sig.aspects().at(static_cast<AspectId>(a));aspects.add("new-"+old.name,old.role,!old.default_value);migration.aspects.push_back({old.name,"new-"+old.name});}
    migration.target_signature=core::RelationSignature(aspects);migration.target_signature.add_type("wide-v2");migration.types={{"wide","wide-v2"}};
    auto next=base;next.sequence=1;next.source=recode_temporal_source(base.source,migration);
    auto& target=*next.declarations;
    std::reverse(factors.begin(),factors.end());for(auto& f:factors)f.type=id(100,2);
    target.registry=DeclarationRegistry({Declaration::boolean(id(100,2)),Declaration::product(id(101,2),factors)});
    target.source_layouts[0].type=id(101,2);for(auto& f:target.source_layouts[0].factors)f.source_aspect="new-"+f.source_aspect;
    const std::vector<DeclarationMap> mapping{{id(100),id(100,2),DeclarationMapKind::Recode,{}},{id(101),id(101,2),DeclarationMapKind::Recode,{}}};
    target.programs={recode_declared_program(state,target,op,1,mapping)};
    check(target.programs[0].program.instructions()[0].aspect==130 && target.programs[0].program.instructions()[1].aspect==2,"program operands not rebound by factor identity");
    const auto before=execute_declared_source_program(base.source,state,1,core::Grade{1});
    const auto after=execute_declared_source_program(next.source,target,1,core::Grade{1});
    check(before.size()==130 && after.size()==130,"packed declared program domain");
    for(std::size_t i=0;i<before.word_count();++i)check(before.word(i)==after.word(i),"packed program changed after layout reorder");
    const auto delta=make_temporal_segment(base,next,{}, {},RelationalCodeEncoding::Automatic,migration,
        make_temporal_declaration_update(base.declarations,next.declarations,mapping));
    const auto bytes=encode_temporal_segment(delta);const auto replay=replay_temporal_segment(base,decode_temporal_segment(bytes));
    for(std::size_t slot=0;slot<130;++slot) {
        const auto value=materialize_declared_source_aspects(replay.source,*replay.declarations,1,{core::Grade{1},core::OccurrenceKey{1000+slot}});
        const auto& bits=std::get<V::Product>(value.data).elements;
        for(std::size_t a=0;a<131;++a) {
            const auto old_aspect=130-a;
            const bool expected=old_aspect==0?slot%3!=0:old_aspect==1?slot%2!=0:(slot+old_aspect)%3==0;
            check(std::get<bool>(bits[a].data)==expected,"wide source bit migration mismatch");
        }
    }
    save(out/"wide.rfcx",encode_rfcx(base.source).bytes);save(out/"wide.rfcxe",encode_temporal_endpoint(base));save(out/"wide.rfcxt",bytes);
    save(out/"wide_target.rfcxe",encode_temporal_endpoint(replay));
    temporal_codec_detail::Writer old_state,new_state;
    temporal_codec_detail::declaration_state(old_state,state);temporal_codec_detail::declaration_state(new_state,target);
    save(out/"reference_wide_base.rfcxds",old_state.bytes());save(out/"reference_wide_target.rfcxds",new_state.bytes());
    auto bad=next;bad.declarations->programs[0].program.append({BinaryOpcode::LoadAspect,0,0,0,0,131});
    rejects([&]{validate_temporal_endpoint(bad);},"persisted program foreign factor accepted");
    auto bad_op=op;bad_op.program.append({BinaryOpcode::LoadAspect,0,0,0,0,65535});
    rejects([&]{(void)recode_declared_program(state,target,bad_op,1,mapping);},"invalid old program operand recoded");
}
void ordered_source_tests() {
    core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);const auto face=sig.add_attaching_loop_type("loop",4,4);
    core::RelationalComplexBuilder b(sig);b.declare_grade(core::Grade{3});
    const auto a=b.add_grade_one(core::RelationKey{10},edge,{{1,true,false,Q{1},{},core::OccurrenceKey{100}},{1,true,true,Q{1},{},core::OccurrenceKey{101}}});
    const auto c=b.add_grade_one(core::RelationKey{20},edge,{{1,true,false,Q{1},{},core::OccurrenceKey{200}},{1,true,true,Q{1},{},core::OccurrenceKey{201}}});
    b.add_higher(core::Grade{2},core::RelationKey{30},face,{{a.index,true,false,Q{1},{},core::OccurrenceKey{300}},
        {c.index,true,false,Q{1},{},core::OccurrenceKey{301}},{a.index,true,true,Q{1},{},core::OccurrenceKey{302}},
        {c.index,true,true,Q{1},{},core::OccurrenceKey{303}}});
    TemporalEndpoint base(b.freeze());AspectSchema aspects;aspects.add("sign",AspectRole::Orientation);aspects.add("present",AspectRole::Existence);
    core::RelationSignature target(aspects);target.add_attaching_loop_type("loop-v2",4,4);target.add_type("edge-v2",core::BoundaryPolicy::DeclaredExact,2,2);
    SourceDeclarationMigration m{sig,target,{{"exists","present"},{"orientation","sign"}},{{"edge","edge-v2"},{"loop","loop-v2"}}, {}};
    TemporalEndpoint next(recode_temporal_source(base.source,m),1);
    const auto old=temporal_source_detail::capture(base.source,2);const auto recoded=temporal_source_detail::capture(next.source,2);
    check(old.fibers==recoded.fibers,"migration changed ordered attaching words");
    const auto s=make_temporal_segment(base,next,{}, {},RelationalCodeEncoding::Automatic,m);
    check(s.source.grades.empty(),"pure word-preserving recode generated source patch");
    auto images=std::vector<temporal_source_detail::GradeImage>{};for(std::size_t k=0;k<4;++k)images.push_back(temporal_source_detail::capture(next.source,k));
    images[2].fibers[30]={300,302,301,303};
    next.source=temporal_source_detail::materialize_images(images,target,next.source.semantic_scale());
    const auto changed=make_temporal_segment(base,next,{}, {},RelationalCodeEncoding::Automatic,m);
    check(changed.source.grades.size()==1 && changed.source.grades[0].fibers.size()==1,"ordered source change hidden in signature migration");
    check(encode_rfcx(replay_temporal_segment(base,changed).source).bytes==encode_rfcx(next.source).bytes,"ordered source replay");
}
void resource_tests() {
    const DeclarationRegistry base({Declaration::product(id(200),{}),Declaration::repeated(id(201),id(200),0,100)});
    std::vector<DeclarationFactor> factors;std::vector<DeclarationFactorFill> fills;
    for(std::size_t i=0;i<100;++i){factors.push_back({i+1,id(202,2),0});fills.push_back({i+1,V::boolean(true)});}
    const DeclarationRegistry target({Declaration::product(id(200,2),factors),Declaration::repeated(id(201,2),id(200,2),0,100),Declaration::boolean(id(202,2))});
    const auto value=V::repeated(std::vector<V>(100,V::product({})));
    const std::vector<DeclarationMap> maps{{id(200),id(200,2),DeclarationMapKind::Embedding,fills},{id(201),id(201,2),DeclarationMapKind::Embedding,{}}};
    DeclarationLimits limits;limits.max_value_nodes=1000;
    rejects([&]{(void)DeclarationRecoding(base,target,maps,limits).apply(id(201),value);},"recoding expansion not preflighted");
    limits={};limits.max_materialized_bytes=10000;
    rejects([&]{(void)DeclarationRecoding(base,target,maps,limits).apply(id(201),value);},"recoding aggregate buffers not bounded");
    auto singleton=endpoint();singleton.codes.reset();
    TemporalDeclarationState state;state.namespace_key=999;state.registry=DeclarationRegistry({Declaration::boolean(id(300))});
    TemporalDeclaredValue scalar;scalar.key=1;scalar.type=id(300);scalar.value=V::boolean(true);state.values={scalar};singleton.declarations=state;
    const auto encoded=encode_temporal_endpoint(singleton);TemporalReadLimits exact;
    exact.declarations.max_materialized_bytes=sizeof(TemporalDeclaredValue);
    check(encode_temporal_endpoint(decode_temporal_endpoint(singleton.source,encoded,exact))==encoded,"zero-payload root at exact allocation budget rejected");
    --exact.declarations.max_materialized_bytes;
    rejects([&]{(void)decode_temporal_endpoint(singleton.source,encoded,exact);},"root allocation omitted from temporal decode budget");
    limits={};limits.max_encoded_bytes=100;
    rejects([&]{(void)DeclarationRecoding(base,target,maps,limits).apply(id(201),value);},"recoding expanded encoded size ignored");
}
void endpoint_tests(const std::filesystem::path& out) {
    const auto base=endpoint();const auto migration=source_map(base.source.signature());const auto next=evolved(base,migration);
    const auto delta=segment(base,next,migration);const auto bytes=encode_temporal_segment(delta);
    SourceDeclarationLimits low;low.max_aspect_bytes=1;
    rejects([&]{(void)replay_temporal_segment(base,delta,low);},"replay ignored source expansion policy");
    auto low_read=TemporalReadLimits{};low_read.source_declarations=low;
    rejects([&]{(void)append_temporal_segment(out/"source_limit_reject.rfcxt",base,delta,low_read);},"append ignored source expansion policy");
    check(!std::filesystem::exists(out/"source_limit_reject.rfcxt"),"bounded replay failure created output");
    check(delta.source.grades.empty(),"presentation migration reported incidence changes");
    check(delta.tensors.empty(),"declaration recoding invented field innovation");
    check(delta.declarations && delta.declarations->replaced_values.empty(),"declared recoding called a value assignment");
    check(!delta.correspondences.empty(),"migrating source-grade J not persisted");
    const auto J=temporal_carrier_correspondence(base,next,10,delta.correspondences);
    check(J==numeric::ExactMatrix::identity(2),"migration stable-source J wrong");
    const auto replay=replay_temporal_segment(base,decode_temporal_segment(bytes));
    check(encode_temporal_endpoint(replay)==encode_temporal_endpoint(next),"migrating complete endpoint replay mismatch");
    check(encode_rfcx(replay.source).bytes==encode_rfcx(next.source).bytes,"migrating retained source mismatch");
    for(std::size_t i=0;i<5;++i){
        const auto value=materialize_declared_source_aspects(replay.source,*replay.declarations,1,{core::Grade{1},core::OccurrenceKey{100+i}});
        check(declared_value_equal(value,V::product({V::boolean(i>=2),V::boolean(i<2)})),"source-backed factor order/roles not rebound");
    }
    auto after=next;after.sequence=2;
    TemporalSourcePatch patch;patch.target_grade_count=4;
    TemporalGradePatch g;g.grade=core::Grade{1};g.cells={{9,0,true}};g.cell_order=std::vector<std::uint64_t>{7,8,9};g.fibers={{9,{}}};patch.grades.push_back(g);
    after.source=replay_temporal_source(next.source,patch);
    after.carriers[0].domain=numeric::IndexDomain::linear(3);
    after.tensors[0].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(3),numeric::StructuralSupport::full(3),numeric::SparseRationalVector(3,{{0,Q{5}}}));
    after.declarations->values[1].value=V::repeated({V::boolean(false)});
    const auto second=make_temporal_segment(next,after);
    check(second.tensors.size()==1 && second.tensors[0].innovation.nonzeros()==0,"zero-amplitude relation birth invented innovation");
    check(second.declarations->replaced_values==std::vector<std::uint64_t>{2},"declared value changes not distinguished");
    check(encode_temporal_endpoint(replay_temporal_segment(replay,decode_temporal_segment(encode_temporal_segment(second))))==encode_temporal_endpoint(after),"two-segment mixed history");
    auto corrupt=delta;corrupt.source_migration->base_signature.aspects().add("foreign");
    const auto frozen=encode_temporal_endpoint(base);
    rejects([&]{(void)replay_temporal_segment(base,corrupt);},"wrong migration accepted");
    check(encode_temporal_endpoint(base)==frozen,"failed replay mutated predecessor");
    corrupt=delta;corrupt.declarations->maps.pop_back();rejects([&]{(void)replay_temporal_segment(base,corrupt);},"truncated schema map accepted");
    rejects([&]{(void)make_temporal_segment(base,next);},"source declaration migration guessed");
    auto changed=next;changed.declarations->branches[0].declaration.id=id(90);
    rejects([&]{(void)make_temporal_declaration_update(base.declarations,changed.declarations,maps());},"branch immutable name redefined");
    auto missed=second;missed.declarations->replaced_values.clear();rejects([&]{(void)replay_temporal_segment(next,missed);},"unrecorded changed value accepted");
    auto badowner=next;badowner.declarations->values[0].cell=StableCellKey{core::Grade{1},999};rejects([&]{validate_temporal_endpoint(badowner);},"dangling declared owner");
    auto badbranch=next;badbranch.declarations->branches[0].declaration.roles[0].min_count=3;rejects([&]{validate_temporal_endpoint(badbranch);},"branch cardinality not enforced");
    auto badbinding=next;badbinding.declarations->code_types[1].type=id(6,2);rejects([&]{validate_temporal_endpoint(badbinding);},"code type incorrectly bound to layout");
    auto missing=next;missing.declarations->source_layouts[0].factors[0].source_aspect="left";rejects([&]{validate_temporal_endpoint(missing);},"source layout stale after rename");
    save(out/"base.rfcx",encode_rfcx(base.source).bytes);save(out/"base.rfcxe",frozen);
    save(out/"migration.rfcxt",bytes);save(out/"second.rfcxt",encode_temporal_segment(second));
    save(out/"target.rfcx",encode_rfcx(next.source).bytes);save(out/"target.rfcxe",encode_temporal_endpoint(next));
    save(out/"final.rfcx",encode_rfcx(after.source).bytes);save(out/"final.rfcxe",encode_temporal_endpoint(after));
    const auto disk=read_temporal_endpoint(MappedRfcx(out/"base.rfcx"),out/"base.rfcxe");
    auto from_disk=replay_temporal_segment(disk,read_temporal_segment(out/"migration.rfcxt"));
    from_disk=replay_temporal_segment(from_disk,read_temporal_segment(out/"second.rfcxt"));
    check(encode_temporal_endpoint(from_disk)==encode_temporal_endpoint(after),"disk-only declarations/bindings replay");
    auto joined=bytes;const auto tail=encode_temporal_segment(second);joined.insert(joined.end(),tail.begin(),tail.end());
    save(out/"history.rfcxt",joined);
    const auto append_path=out/"appended.rfcxt";
    std::filesystem::remove(append_path);
    (void)append_temporal_segment(append_path,disk,delta);
    (void)append_temporal_segment(append_path,next,second);
    check(load(append_path)==joined,"append stream differs from canonical concatenation");
    const auto append_frozen=load(append_path);
    rejects([&]{(void)append_temporal_segment(append_path,delta);},"wrong predecessor appended");
    check(load(append_path)==append_frozen,"failed append changed existing frames");
    auto short_policy=TemporalReadLimits{};short_policy.max_file_bytes=32;
    rejects([&]{(void)append_temporal_segment(out/"limit_reject.rfcxt",delta,short_policy);},"oversized migration append accepted");
    check(!std::filesystem::exists(out/"limit_reject.rfcxt"),"failed append created a file");
    for(std::size_t n=1;n<32;++n) {
        TemporalSegmentStreamDecoder partial;
        partial.feed(std::span<const std::byte>(joined).first(n),[](const auto&,auto){});
        rejects([&]{partial.finish();},"truncated streamed header accepted");
    }
    for(const auto tail_bytes:{1u,31u,32u,127u}) {
        TemporalSegmentStreamDecoder partial;
        partial.feed(std::span<const std::byte>(joined).first(bytes.size()+tail_bytes),[](const auto&,auto){});
        rejects([&]{partial.finish();},"truncated streamed migration tail accepted");
        check(partial.frame_count()==1,"partial stream exposed incomplete target");
    }
    // Export exactly the suffixes embedded in the wire frames for the independent parser.
    temporal_codec_detail::Writer base_defs,next_defs,decl_update,source_update;
    temporal_codec_detail::declaration_state(base_defs,*base.declarations);
    temporal_codec_detail::declaration_state(next_defs,*next.declarations);
    temporal_codec_detail::declaration_update(decl_update,*delta.declarations);
    temporal_codec_detail::source_migration(source_update,*delta.source_migration);
    save(out/"reference_base.rfcxds",base_defs.bytes());save(out/"reference_target.rfcxds",next_defs.bytes());
    save(out/"reference_update.rfcxdu",decl_update.bytes());save(out/"reference_source.rfcxsm",source_update.bytes());
    for(const auto chunk:{1u,2u,3u,7u,31u,64u,129u,1024u,8192u}) {
        TemporalSegmentStreamDecoder decoder;auto current=disk;
        for(std::size_t pos=0;pos<joined.size();pos+=chunk)decoder.feed(std::span<const std::byte>(joined).subspan(pos,std::min<std::size_t>(chunk,joined.size()-pos)),
            [&](const auto& s,auto frame){check(encode_temporal_segment(s)==Bytes(frame.begin(),frame.end()),"stream frame not canonical");current=replay_temporal_segment(current,s);});
        decoder.finish();check(encode_temporal_endpoint(current)==encode_temporal_endpoint(after),"chunked replay");
    }
    const auto seed=encode_temporal_endpoint(base);
    for(std::size_t n=0;n<seed.size();++n){rejects([&]{(void)decode_temporal_endpoint(base.source,std::span<const std::byte>(seed).first(n));},"truncated declaration sidecar");++truncations;}
    for(std::size_t n=0;n<bytes.size();++n){rejects([&]{(void)decode_temporal_segment(std::span<const std::byte>(bytes).first(n));},"truncated migration segment");++truncations;}
    TemporalReadLimits tiny;tiny.declarations.max_definitions=1;rejects([&]{(void)decode_temporal_endpoint(base.source,seed,tiny);},"declaration policy bypassed");
    tiny={};tiny.max_records=2;rejects([&]{(void)decode_temporal_segment(bytes,tiny);},"record policy bypassed");
    std::mt19937_64 rng(73242);
    for(std::size_t i=0;i<500;++i){
        auto mutated=bytes;const auto pos=32+rng()%(mutated.size()-32);mutated[pos]^=std::byte{static_cast<unsigned char>(1u<<(rng()%8))};
        const auto hash=rfcx_detail::fnv1a64(std::span<const std::byte>(mutated).subspan(32));
        for(unsigned b=0;b<8;++b)mutated[24+b]=std::byte{static_cast<unsigned char>(hash>>(8*b))};
        std::optional<TemporalSegment> parsed;
        try { parsed=decode_temporal_segment(mutated); } catch(const std::exception&) { ++mutations_rejected; }
        if(parsed) {
            ++mutations_accepted;check(encode_temporal_segment(*parsed)==mutated,"accepted noncanonical mutation");
            std::optional<TemporalEndpoint> result;
            try { result=replay_temporal_segment(base,*parsed); } catch(const std::exception&) {}
            if(result){++mutations_replayed;check(encode_temporal_endpoint(*result)==encode_temporal_endpoint(next),"accepted different target under fixed hash");}
        }
    }
    auto wrongflag=bytes;wrongflag[12]|=std::byte{128};rejects([&]{(void)decode_temporal_segment(wrongflag);},"unknown migration wire flag");
    // Explicit attach/detach alongside legacy frames; no schema-free bytes change.
    auto bare=base;bare.declarations.reset();bare.codes.reset();bare.sequence=0;
    auto attached=bare;attached.sequence=1;attached.declarations=base.declarations;attached.declarations->code_types.clear();
    const auto attach=make_temporal_segment(bare,attached);auto detached=bare;detached.sequence=2;
    const auto detach=make_temporal_segment(attached,detached);
    check(!replay_temporal_segment(replay_temporal_segment(bare,attach),detach).declarations,"declaration detach failed");
    save(out/"legacy.rfcxe",encode_temporal_endpoint(bare));
    save(out/"attach.rfcxt",encode_temporal_segment(attach));save(out/"detach.rfcxt",encode_temporal_segment(detach));
}
void observation_tests() {
    const auto base=endpoint();const auto migration=source_map(base.source.signature());const auto next=evolved(base,migration);
    RelationalCodeObservationQuery query;query.key=1;query.kind=RelationalCodeObservationKind::Section;
    RelationalCodeObservationStore store(base,{query},[](auto){return 0;});
    const auto before=store.observation({1,1000}).bytes;
    auto delta=segment(base,next,migration);const auto stats=store.advance(delta);
    check(stats.evaluated==1 && store.observation({1,1000}).bytes!=before,"source/layout migration did not invalidate observation");
    const RelationalCodeObservationStore rebuilt(next,{query},[](auto){return 0;});
    check(store.observation({1,1000}).bytes==rebuilt.observation({1,1000}).bytes,"incremental/rebuild observation mismatch");
    auto later=next;later.sequence=2;later.declarations->values[1].value=V::repeated({V::boolean(true)});
    const auto unrelated=store.advance(make_temporal_segment(next,later));
    check(unrelated.evaluated==0 && unrelated.reused==1,"unrelated declared payload invalidated code view");
    // A changed declaration version must be visible even when code/type/value bits are equal.
    auto leaf=endpoint();leaf.declarations->code_types.clear();leaf.declarations->values.clear();leaf.declarations->branches.clear();leaf.declarations->source_layouts.clear();
    leaf.declarations->registry=DeclarationRegistry({Declaration::boolean(id(1))});leaf.declarations->code_types={{1,id(1)}};
    leaf.codes->types.resize(1);leaf.codes->sections[0].source_aspects=false;leaf.codes->sections[0].type=1;leaf.codes->sections[0].occurrence.reset();leaf.codes->sections[0].value=RelationalCodeValue{std::uint64_t{1}};
    auto version=leaf;version.sequence=1;version.declarations->registry=DeclarationRegistry({Declaration::boolean(id(1,2))});version.declarations->code_types={{1,id(1,2)}};
    auto update=make_temporal_declaration_update(leaf.declarations,version.declarations,{{id(1),id(1,2),DeclarationMapKind::Recode,{}}});
    auto change=make_temporal_segment(leaf,version,{}, {},RelationalCodeEncoding::Automatic,{},update);
    RelationalCodeObservationStore versions(leaf,{query});const auto old=versions.observation({1,1000}).bytes;
    const auto vstats=versions.advance(change);check(vstats.evaluated==1 && vstats.bytes_changed==1 && versions.observation({1,1000}).bytes!=old,"same-valued schema version not observed");
    RelationalCodeObservationStore pending(leaf,{query});
    auto bad=change;bad.target_endpoint_hash^=1;const auto frozen=encode_temporal_endpoint(pending.snapshot());
    const auto frozen_observation=pending.observation({1,1000}).bytes;
    rejects([&]{(void)pending.advance(bad);},"invalid target accepted by observation publisher");
    check(encode_temporal_endpoint(pending.snapshot())==frozen && pending.observation({1,1000}).bytes==frozen_observation,"failed publish modified endpoint/index");
    bool fail_selector=false;
    RelationalCodeObservationStore callback(leaf,{query},[&](auto){if(fail_selector)throw std::runtime_error("test selector failure");return std::uint64_t{0};});
    fail_selector=true;
    rejects([&]{(void)callback.advance(change);},"selector failure not propagated");
    check(encode_temporal_endpoint(callback.snapshot())==frozen && callback.observation({1,1000}).bytes==frozen_observation,"selector failure exposed partially recoded state");
    fail_selector=false;(void)callback.advance(change);
    check(callback.observation({1,1000}).bytes==versions.observation({1,1000}).bytes,"retry after failed recoding publication");
}
}
int main(int argc,char** argv) {
    try {
        const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::path("declaration_temporal_fixtures");
        std::filesystem::create_directories(out);
        retained_resource_policy_tests();source_tests();recoding_tests();resource_tests();ordered_source_tests();endpoint_tests(out);observation_tests();wide_program_tests(out);
        std::cout<<"declaration_temporal_checks="<<checks<<" truncations="<<truncations
                 <<" mutation_rejected="<<mutations_rejected<<" mutation_canonical="<<mutations_accepted
                 <<" mutation_replayed="<<mutations_replayed<<"\n";return 0;
    }catch(const std::exception& e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
