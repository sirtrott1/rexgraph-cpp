#include "../../examples/native_covering/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_covering_example;
std::size_t checks{},field_cases{},lift_cases{},monodromy_cases{},deck_cases{},square_cases{},mediator_cases{},case_id{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F f,const char* message){bool caught=false;try{f();}catch(const Error&){caught=true;}check(caught,message);}
Values values(const inference::DerivedNativeCoveringField& f){return f.field().values();}
template<class T>concept TemporaryValues=requires(T&& value){std::move(value).values();};
template<class T>concept TemporaryMap=requires(T&& value){std::move(value).map();};
static_assert(!TemporaryValues<local::OwnedCoveringField>);
static_assert(!TemporaryMap<topology::OwnedCoveringDeck>);
static_assert(!TemporaryMap<local::OwnedCoveringPullbackMediator>);
void json(const Values& v){std::cout<<'[';bool comma=false;for(const auto& [i,q]:v){if(comma)std::cout<<',';comma=true;std::cout<<'['<<i<<",\""<<q<<"\"]";}std::cout<<']';}
void json(const core::RelationalComplex& c){
    std::cout<<"{\"sizes\":[";for(std::uint16_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<c.grade(grade(k)).cell_count();}
    std::cout<<"],\"cells\":[";for(std::uint16_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';const auto& a=c.grade(grade(k));std::cout<<'[';
        for(std::size_t i=0;i<a.cell_count();++i){if(i)std::cout<<',';std::cout<<'['<<storage::stable_cell_key(c,{grade(k),static_cast<std::uint32_t>(i)}).key<<',';
            if(k)std::cout<<a.relation_types[i];else std::cout<<"null";std::cout<<",\""<<a.cell_scales[i]<<"\"]";}std::cout<<']';}
    std::cout<<"],\"ordered_types\":[";for(std::size_t i=0;i<c.signature().type_count();++i){if(i)std::cout<<',';std::cout<<c.signature().type(static_cast<core::RelationTypeId>(i)).is_attaching_loop();}
    std::cout<<"],\"occurrences\":[";for(std::uint16_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';const auto& a=c.grade(grade(k));std::cout<<'[';
        for(std::size_t p=0;p<a.incidence_count();++p){if(p)std::cout<<',';std::cout<<'['<<a.upper_index[p]<<','<<a.lower_index[p]<<','<<a.occurrence_identities[p].value<<','
            <<a.aspects.get(c.signature().existence_aspect(),p)<<",\""<<c.incidence_coefficient(grade(k),p)<<"\",[";
            for(std::size_t j=0;j<c.signature().aspects().size();++j){if(j)std::cout<<',';std::cout<<a.aspects.get(static_cast<composite::AspectId>(j),p);}std::cout<<"],\""<<a.lattice_coefficient[p]<<"\"]";}std::cout<<']';}
    std::cout<<"]}";
}
void json(const algebra::RetainedSourceMap& map){std::cout<<"{\"cells\":[";for(std::size_t k=0;k<map.cells.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<map.cells[k].size();++i){if(i)std::cout<<',';std::cout<<map.cells[k][i];}std::cout<<']';}
    std::cout<<"],\"occurrences\":[";for(std::size_t k=0;k<map.occurrences.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<map.occurrences[k].size();++i){if(i)std::cout<<',';std::cout<<map.occurrences[k][i];}std::cout<<']';}std::cout<<"]}";}
void json(const topology::OwnedCoveringWalk& w){
    std::cout<<"{\"start\":["<<w.start().grade.value<<','<<w.start().index<<"],\"steps\":[";
    for(std::size_t i=0;i<w.steps().size();++i){if(i)std::cout<<',';const auto s=w.steps()[i];std::cout<<'['<<s.upper_grade.value<<','<<s.occurrence<<','<<s.upward<<']';}
    std::cout<<"],\"visits\":[";for(std::size_t i=0;i<w.visits().size();++i){if(i)std::cout<<',';const auto v=w.visits()[i];std::cout<<'['<<v.grade.value<<','<<v.index<<']';}std::cout<<"]}";
}
void covering_record(const Plan& p,bool records){if(!records)return;const auto n=p.numeric_plan();std::cout<<"{\"kind\":\"covering\",\"case\":"<<case_id<<",\"cover\":";json(n.source());std::cout<<",\"base\":";json(n.source(Channel::Base));std::cout<<",\"projection\":";json(n.projection());std::cout<<"}\n";}
void field_record(const char* action,const inference::DerivedNativeCoveringField& input,const inference::DerivedNativeCoveringField& output,bool records){
    ++field_cases;check(output.input()!=nullptr && output.input()->plan().same_owner(input.plan()),"field action drops original owner/input");
    if(!records)return;
    std::cout<<"{\"kind\":\"field\",\"case\":"<<case_id<<",\"action\":\""<<action<<"\",\"grade\":"<<input.field().grade().value<<",\"input\":";json(values(input));std::cout<<",\"output\":";json(values(output));std::cout<<"}\n";
}
void walk_record(const Plan& plan,const inference::DerivedNativeCoveringWalk& walk,bool records){
    const auto n=plan.numeric_plan();Guard guard;const auto fiber=n.fiber(walk.walk().start(),guard);
    for(const auto start:fiber){
        const auto lift=plan.lift(walk,storage::stable_cell_key(n.source(),start),available);++lift_cases;
        check(lift.input()->plan().same_owner(plan),"lift loses actual native walk owner");check(lift.lift().lifted.covering().same_owner(n),"lift loses actual cover");
        if(records){std::cout<<"{\"kind\":\"lift\",\"case\":"<<case_id<<",\"base_walk\":";json(lift.lift().base);std::cout<<",\"lifted_walk\":";json(lift.lift().lifted);std::cout<<"}\n";}
    }
    if(walk.walk().is_loop()){
        const auto action=plan.monodromy(walk,available);++monodromy_cases;check(action.action().lifts().size()==fiber.size(),"monodromy omits a fiber lift");
        if(records){std::cout<<"{\"kind\":\"monodromy\",\"case\":"<<case_id<<",\"loop\":";json(action.action().loop());std::cout<<",\"fiber\":[";
            for(std::size_t i=0;i<fiber.size();++i){if(i)std::cout<<',';std::cout<<fiber[i].index;}std::cout<<"],\"image\":[";
            for(std::size_t i=0;i<action.action().image().size();++i){if(i)std::cout<<',';std::cout<<action.action().image()[i];}std::cout<<"]}\n";}
    }
}
void deck_record(const Plan& p,const inference::NativeGluingSourceMap& request,bool records){
    const auto d=p.deck(request,available);++deck_cases;const auto n=p.numeric_plan();check(topology::is_deck_transformation(n.reference(),d.transformation().map().cells),"canonical deck reference differs");
    if(records){std::cout<<"{\"kind\":\"deck\",\"case\":"<<case_id<<",\"forward\":";json(d.transformation().map());std::cout<<",\"inverse\":";json(d.transformation().map(true));std::cout<<"}\n";}
}
void square_record(const Plan& p,const Plan& q,inference::NativeCoveringPullbackRequest request,bool records){
    const auto square=Square::prepare(p,q,request,square_available);const auto witness=square.witness(square_available);++square_cases;
    check(witness.provenance().sequences==std::array<std::uint64_t,4>{31,37,41,43},"square loses independent source cutoffs");
    const auto cone_request=cone(square);const auto m=square.mediate(cone_request,pulled_available.cover,square_available);++mediator_cases;
    const auto numeric=square.numeric_plan();const auto pn=p.numeric_plan(),qn=q.numeric_plan();const auto& map=m.mediator().map();
    for(std::size_t k=0;k<map.cells.size();++k){for(std::size_t i=0;i<map.cells[k].size();++i)check(map.cells[k][i]==i,"universal identity cone loses a cell");for(std::size_t i=0;i<map.occurrences[k].size();++i)check(map.occurrences[k][i]==i,"universal identity cone loses an occurrence");}
    check(m.request().source.endpoint()==q.anchor(Channel::Cover).endpoint(),"mediator loses original cone publication");
    if(records){std::cout<<"{\"kind\":\"square\",\"case\":"<<case_id<<",\"E\":";json(pn.source());std::cout<<",\"B\":";json(pn.source(Channel::Base));std::cout<<",\"P\":";json(qn.source());std::cout<<",\"Y\":";json(qn.source(Channel::Base));
        std::cout<<",\"p\":";json(pn.projection());std::cout<<",\"q\":";json(qn.projection());std::cout<<",\"into\":";json(numeric.map());std::cout<<",\"base_map\":";json(numeric.map(true));std::cout<<",\"T\":";json(m.mediator().source());
        std::cout<<",\"cone_into\":";json(m.mediator().cone_map());std::cout<<",\"cone_base\":";json(m.mediator().cone_map(true));std::cout<<",\"mediator\":";json(map);std::cout<<"}\n";}
    auto cut=square_available;for(unsigned which=0;which<4;++which){cut=square_available;auto* selected=which==0?&cut.original.cover:which==1?&cut.original.base:which==2?&cut.pulled.cover:&cut.pulled.base;--selected->max_sequence;
        rejects<std::invalid_argument>([&]{square.witness(cut);},"square accepted a future endpoint");rejects<std::invalid_argument>([&]{square.mediate(cone_request,pulled_available.cover,cut);},"mediator accepted a future square endpoint");}
    auto cone_cut=pulled_available.cover;--cone_cut.max_sequence;rejects<std::invalid_argument>([&]{square.mediate(cone_request,cone_cut,square_available);},"mediator ignores its fifth cutoff");
    auto incomplete=cone_request;if(!incomplete.into_cover.occurrences.empty()){incomplete.into_cover.occurrences.pop_back();rejects<std::invalid_argument>([&]{square.mediate(incomplete,pulled_available.cover,square_available);},"universal cone omits an original occurrence");}
    runtime::ExecutionHostPolicy policy;policy.limits.integer_bits=0;rejects<std::length_error>([&]{square.witness(square_available,runtime::ExecutionLease(policy));},"square skips current full-source bit caps");
}
void exercise(core::RelationalComplex source,core::RelationalComplex base,bool cycle_case,bool records){
    ++case_id;const auto p=prepare(source,base),q=prepare(source,base,true);const auto n=p.numeric_plan();covering_record(p,records);
    check(storage::encode_rfcx(n.source()).bytes==storage::encode_rfcx(source).bytes,"cover source canonical bytes changed");check(storage::encode_rfcx(n.source(Channel::Base)).bytes==storage::encode_rfcx(base).bytes,"base canonical bytes changed");
    for(std::uint16_t k=0;k<base.grade_count();++k)for(std::uint32_t i=0;i<base.grade(grade(k)).cell_count();++i)for(const auto& coefficient:std::array<Q,2>{Q{0},Q{-3,7}}){
        const auto input=p.chain(Channel::Base,native_chain(p,Channel::Base,grade(k),{{i,coefficient}}),available);const auto t=p.transfer(input,available),round=p.pushforward(t,available);
        Guard fiber_guard;check(values(t).size()==n.fiber({grade(k),i},fiber_guard).size(),"transfer loses complete fiber support");
        if(k){const auto before=p.boundary(input,available);check(values(p.boundary(t,available))==values(p.transfer(before,available)),"transfer fails full boundary/support naturality");}
        field_record("transfer",input,t,records);field_record("pushforward",t,round,records);
        const auto section=p.section(Channel::Base,42,grade(k),{{i,coefficient}},available),pull=p.pullback(section,available),descent=p.descend(pull,available);
        check(values(descent)==values(section) && descent.field().field_identity()==42,"scalar section pullback/descent changed identity or supported zero");field_record("pullback",section,pull,records);field_record("descent",pull,descent,records);
    }
    for(std::uint16_t k=0;k<source.grade_count();++k)for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i)for(const auto& coefficient:std::array<Q,2>{Q{0},Q{-3,7}}){
        const auto input=p.chain(Channel::Cover,grade(k),{{i,coefficient}},available),out=p.pushforward(input,available);field_record("pushforward",input,out,records);
        if(k)check(values(p.boundary(out,available))==values(p.pushforward(p.boundary(input,available),available)),"pushforward fails full boundary/support naturality");
    }
    for(std::uint16_t k=1;k<base.grade_count();++k)for(std::size_t slot=0;slot<base.grade(grade(k)).incidence_count();++slot){
        const auto& a=base.grade(grade(k));for(const bool up:{false,true}){
            const core::CellId start=up?core::CellId{grade(k-1),a.lower_index[slot]}:core::CellId{grade(k),a.upper_index[slot]};
            const auto w=p.walk(flag_walk(p,start,{{grade(k),slot,up},{grade(k),slot,!up}}),available);walk_record(p,w,records);
            const auto prefix=p.prefix(w,1,available);check(prefix.request().steps.size()==1 && prefix.walk().steps().size()==1 && !prefix.pairwise_input(),"scoped prefix exposes a suffix or whole pairwise input");
            Guard guard;const auto inverse=w.walk().inverse(guard);const auto joined=topology::OwnedCoveringWalk::compose(w.walk(),inverse,guard);check(joined.steps().size()==4 && joined.is_loop(),"walk composition silently rewrites the ordered word");
        }
    }
    if(cycle_case){
        const auto walk=p.pairwise_walk({grade(0),10},loop_steps(p),available);walk_record(p,walk,records);const auto twice=p.pairwise_walk({grade(0),10},loop_steps(p,2),available);walk_record(p,twice,records);
        const topology::PairwisePath reference(n.source(Channel::Base),0,[&]{std::vector<topology::EdgeStep> steps;for(std::uint32_t i=0;i<base.grade(grade(1)).cell_count();++i)steps.push_back({i,true});return steps;}());
        const auto action=p.monodromy(walk,available);check(topology::monodromy(n.reference(),reference).image==action.action().image(),"canonical pairwise monodromy differs");
        const auto old_lift=topology::lift_path(n.reference(),reference,0);const auto actual_lift=p.lift(walk,{grade(0),10},available);check(old_lift.endpoint==actual_lift.lift().lifted.finish().index,"canonical pairwise lift differs");
        deck_record(p,deck_request(p,base.grade(grade(0)).cell_count()),records);
    }
    square_record(p,q,square_request(p,q),records);
}
void contracts(bool records){
    const auto p=prepare();const auto n=p.numeric_plan();const auto x=p.chain(Channel::Cover,grade(1),{{0,Q{1}},{3,Q{-1}}},available);const auto y=p.pushforward(x,available);
    check(values(y).size()==1 && zero(values(y)) && y.input()->field().values().size()==2,"pushforward loses supported cancellation or original directions");
    rejects<std::invalid_argument>([&]{p.pullback(x,available);},"Chain implicitly treated as scalar section");rejects<std::invalid_argument>([&]{p.transfer(p.section(Channel::Base,7,grade(1),{},available),available);},"section implicitly treated as Chain");
    rejects<std::invalid_argument>([&]{p.section(Channel::Base,0,grade(1),{},available);},"undeclared scalar field identity accepted");
    rejects<std::invalid_argument>([&]{p.descend(p.section(Channel::Cover,7,grade(0),{{0,Q{0}}},available),available);},"partial zero fiber support descends");
    rejects<std::invalid_argument>([&]{p.descend(p.section(Channel::Cover,7,grade(0),{{0,Q{0}},{3,Q{1}}},available),available);},"incompatible fiber values descend");
    const auto twin=prepare();rejects<std::invalid_argument>([&]{p.pushforward(twin.chain(Channel::Cover,grade(1),{},available),available);},"foreign empty field owner accepted");
    rejects<std::invalid_argument>([&]{p.chain(Channel::Cover,native_chain(twin,Channel::Cover,grade(1),{{0,Q{0}}}),available);},"foreign equal-valued native publication accepted");
    auto bad=p.declaration();bad.projection.occurrences.pop_back();rejects<std::invalid_argument>([&]{Plan::prepare(bad,available);},"covering omits unused occurrence");
    bad=p.declaration();bad.projection.cells.pop_back();rejects<std::invalid_argument>([&]{Plan::prepare(bad,available);},"covering omits unused cell");
    const auto rep=repeated(),rep_source=copies(rep,2);const auto rp=prepare(rep_source,rep);auto rm=projection(rep_source,rep_source);std::swap(rm.occurrences[1][1],rm.occurrences[1][3]);
    const auto swapped=named_map(rep_source,rep_source,rm,101,101);rejects<std::invalid_argument>([&]{rp.deck(swapped,available);},"deck equates identical-valued original occurrence identities");
    rejects<std::invalid_argument>([&]{p.deck(deck_request(p,1),available);},"non-fiber-preserving deck accepted");
    core::RelationSignature sig;sig.add_type("inactive");core::RelationalComplexBuilder sb(sig),bb(sig);
    bb.add_grade_one(0,{{10,false,true,Q{0},{},core::OccurrenceKey{101}}});sb.add_grade_one(0,{{110,false,true,Q{0},{},core::OccurrenceKey{201}}});sb.add_grade_one(0,{{110,false,true,Q{0},{},core::OccurrenceKey{202}}});
    const auto inactive_base=bb.freeze(),inactive_cover=sb.freeze();local::StrictRelationalCovering legacy(inactive_cover,inactive_base,{{0},{0,0}});check(legacy.validate(),"canonical active-only profile changed");
    Guard guard;rejects<std::invalid_argument>([&]{local::OwnedRelationalCovering::prepare(inactive_cover,inactive_base,{{{0},{0,0}},{{},{0,0}}},5,1,guard);},"duplicate inactive upward flag accepted in retained covering");
    auto empty_base=native_cylinder_example::unit(true,3);const auto empty=prepare(empty_base,empty_base);const auto empty_field=empty.chain(Channel::Cover,grade(3),{},available);check(values(empty.pushforward(empty_field,available)).empty(),"empty declared grades lost");
    for(unsigned i=0;i<2;++i){auto cut=available;auto& selected=i?cut.base:cut.cover;--selected.max_sequence;rejects<std::invalid_argument>([&]{empty.pushforward(empty_field,cut);},"empty action reads a future endpoint");rejects<std::invalid_argument>([&]{Plan::prepare(p.declaration(),cut);},"preparation accepts a future endpoint");}
    auto cut=available;cut.base.source_namespace=1;rejects<std::invalid_argument>([&]{p.transfer(p.chain(Channel::Base,grade(1),{},available),cut);},"foreign cutoff namespace accepted");
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;rejects<std::invalid_argument>([&]{empty.pushforward(empty_field,available,runtime::ExecutionLease(policy));},"empty action bypasses allocation admission");
    for(unsigned cap=0;cap<3;++cap){policy={};if(cap==0)policy.limits.integer_bits=0;else if(cap==1)policy.limits.scalar_work=1;else policy.limits.rational_slots=1;
        rejects<std::length_error>([&]{empty.pushforward(empty_field,available,runtime::ExecutionLease(policy));},"empty action bypasses current guard");rejects<std::length_error>([&]{Plan::prepare(empty.declaration(),available,runtime::ExecutionLease(policy));},"empty preparation bypasses current guard");}
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;rejects<std::invalid_argument>([&]{p.pushforward(x,available,runtime::ExecutionLease(policy));},"unsupported GPU admitted");policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;const auto fallback=p.pushforward(x,available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"explicit fallback absent from receipt");
    const auto stop=std::make_shared<std::atomic_bool>(true);rejects<runtime::ExecutionCancelled>([&]{p.pushforward(x,available,runtime::ExecutionLease({},stop));},"owned cancellation ignored");
    Values draft{{0,Q{1}}};auto* alias=&draft.begin()->second;bool changed=false;Guard frozen({},[&]{if(!changed){changed=true;*alias=99;}});const auto captured=n.chain(Channel::Cover,grade(1),std::move(draft),frozen);check(captured.values().at(0)==1,"moved value alias changes frozen field");
    auto replaced=n;const auto other=twin.numeric_plan();changed=false;Guard replace({},[&]{if(!changed){changed=true;replaced=other;}});check(replaced.chain(Channel::Cover,grade(1),{{0,Q{1}}},replace).covering().same_owner(n),"callback replacement changes an in-flight owner");
    auto map=n.projection();auto* map_alias=&map.cells[0][0];changed=false;Guard map_freeze({},[&]{if(!changed){changed=true;*map_alias=99;}});const auto fixed=local::OwnedRelationalCovering::prepare(n.source(),n.source(Channel::Base),std::move(map),3,1,map_freeze);check(fixed.projection().cells[0][0]==0,"moved projection alias changes captured source map");
    const auto native_walk=p.pairwise_walk({grade(0),10},loop_steps(p),available);rejects<std::invalid_argument>([&]{twin.monodromy(native_walk,available);},"foreign walk owner accepted");rejects<std::invalid_argument>([&]{p.lift(native_walk,{grade(0),11},available);},"lift starts over a different fiber");
    std::vector<topology::CoveringFlagStep> draft_steps{{grade(1),0,true},{grade(1),1,false}};auto* step_alias=&draft_steps[0].occurrence;changed=false;
    Guard step_freeze({},[&]{if(!changed){changed=true;*step_alias=1;}});const auto captured_walk=topology::OwnedCoveringWalk::prepare(n,Channel::Base,{grade(0),0},std::move(draft_steps),step_freeze);
    check(captured_walk.steps()[0].occurrence==0 && captured_walk.finish().index==1,"moved flag alias changes the original walk word");
    const auto lifetime=[](){const auto owner=prepare();const auto walk=owner.pairwise_walk({grade(0),10},loop_steps(owner),available);return owner.monodromy(walk,available);}();
    check(lifetime.input()->request().steps.size()==6 && lifetime.action().image()==std::vector<std::size_t>{1,0},"monodromy outlives neither its word nor native source");
    const auto branch=native_cylinder_example::branch();const auto branch_plan=prepare(copies(branch,2),branch);
    rejects<std::invalid_argument>([&]{branch_plan.pairwise_walk({grade(0),10},{{{grade(1),1},true}},available);},"branch silently replaced by a pairwise path");
    core::RelationSignature defect_signature;defect_signature.add_type("edge");defect_signature.add_type("face");core::RelationalComplexBuilder defect_builder(defect_signature);
    defect_builder.add_grade_one(0,{{10,true,true,Q{1}},{20,true,false,Q{1}}});
    rejects<std::invalid_argument>([&]{defect_builder.add_higher(grade(2),1,{{0,true,false,Q{1}}});},"canonical source builder accepted an adjacent chain defect");
    std::vector<std::future<Values>> jobs;for(unsigned i=0;i<6;++i)jobs.push_back(std::async(std::launch::async,[p,x]{return values(p.pushforward(x,available));}));for(auto& job:jobs)check(job.get()==values(y),"concurrent scratch changes a field");
    const auto weighted=native_cylinder_example::coalescing(true,true);const auto guarded=prepare(copies(weighted,2),weighted);policy={};policy.limits.integer_bits=3;
    rejects<std::length_error>([&]{guarded.chain(Channel::Base,grade(2),{},available,runtime::ExecutionLease(policy));},"unused cancelling coefficient bypasses current bit cap");
    // Nontrivial base change: C6 x_C3 C6 has two declared C6 components.
    const auto large=cycle(6);const auto pulled=prepare(copies(large,2),large,true);const auto qn=pulled.numeric_plan();auto into=projection(qn.source(),n.source());
    for(std::size_t k=0;k<2;++k)for(std::size_t i=6;i<into.cells[k].size();++i)into.cells[k][i]=static_cast<std::uint32_t>((i+3)%6);
    for(std::size_t i=12;i<into.occurrences[1].size();++i)into.occurrences[1][i]=((i/2+3)%6)*2+i%2;
    inference::NativeCoveringPullbackRequest sr{9000,3,named_map(qn.source(),n.source(),into,121,101,9001),named_map(large,n.source(Channel::Base),projection(large,n.source(Channel::Base)),122,102,9002)};
    square_record(p,pulled,sr,records);const auto square=Square::prepare(p,pulled,sr,square_available);
    auto false_square=sr;false_square.into_cover=named_map(qn.source(),n.source(),projection(qn.source(),n.source()),121,101,9001);
    rejects<std::invalid_argument>([&]{Square::prepare(p,pulled,false_square,square_available);},"pullback repeats pairs and omits a sheet");
    auto cone_request=cone(square);auto rotation=projection(qn.source(),large);for(std::size_t k=0;k<2;++k)for(auto& i:rotation.cells[k])i=(i+1)%6;for(std::size_t i=0;i<rotation.occurrences[1].size();++i)rotation.occurrences[1][i]=((i/2+1)%6)*2+i%2;
    cone_request.into_base=named_map(qn.source(),large,rotation,9900,122,9902);rejects<std::invalid_argument>([&]{square.mediate(cone_request,pulled_available.cover,square_available);},"full strict cone accepted despite a noncommuting square");
    const auto square_numeric=square.numeric_plan();auto borrowed_into=square_numeric.map();auto* into_alias=&borrowed_into.cells[0][0];changed=false;Guard into_freeze({},[&]{if(!changed){changed=true;*into_alias=99;}});
    const auto owned_square=local::OwnedCoveringPullback::prepare(n,qn,std::move(borrowed_into),square_numeric.map(true),9,1,into_freeze);
    check(owned_square.map().cells[0][0]==0,"moved pullback map alias changes a captured square");
    const auto old_cover=p.anchor(Channel::Cover),old_base=p.anchor(Channel::Base);storage::NativeAccessionDirectory dc(*old_cover.endpoint(),old_cover.source_namespace()),db(*old_base.endpoint(),old_base.source_namespace());
    const auto actual_cover=anchor(dc),actual_base=anchor(db);const auto historical=Plan::prepare({actual_cover,actual_base,1100,7,p.declaration().projection,128},available);const auto historical_input=historical.chain(Channel::Cover,grade(1),{{0,Q{1}}},available);
    const auto before=dc.pin_snapshot();auto after=*before;after.sequence=32;dc.advance(storage::make_temporal_segment(*before,after));const auto base_before=db.pin_snapshot();auto base_after=*base_before;base_after.sequence=38;db.advance(storage::make_temporal_segment(*base_before,base_after));
    check(historical.anchor(Channel::Cover).sequence()==31 && historical.anchor(Channel::Base).sequence()==37 && values(historical.pushforward(historical_input,available)).at(0)==1,"advancement rewrites an old covering result");
    const auto later=dc.pin(storage::NativeAccessionRequest::sparse_chain(grade(1),{},{}));rejects<std::invalid_argument>([&]{historical.chain(Channel::Cover,later,available);},"later input replaces an old source publication");
    storage::NativeAccessionDirectory dp(*pulled.anchor(Channel::Cover).endpoint(),942),dy(*pulled.anchor(Channel::Base).endpoint(),943);
    const auto live_pulled=Plan::prepare({anchor(dp),anchor(dy),1200,7,pulled.declaration().projection,128},pulled_available);
    const auto historical_square=Square::prepare(historical,live_pulled,sr,square_available);const auto old_cone=cone(historical_square);
    const auto pp=dp.pin_snapshot();auto pp_next=*pp;pp_next.sequence=42;dp.advance(storage::make_temporal_segment(*pp,pp_next));const auto yy=dy.pin_snapshot();auto yy_next=*yy;yy_next.sequence=44;dy.advance(storage::make_temporal_segment(*yy,yy_next));
    const auto unchanged=historical_square.mediate(old_cone,pulled_available.cover,square_available);
    check(unchanged.provenance().sequences==std::array<std::uint64_t,4>{31,37,41,43} && unchanged.request().source.sequence()==41,"advancement rewrites a square or mediator endpoint");
    auto future_cone=old_cone;future_cone.source=anchor(dp);rejects<std::invalid_argument>([&]{historical_square.mediate(future_cone,pulled_available.cover,square_available);},"later cone source leaks into an earlier cutoff");
}
}
int main(int argc,char** argv){try{const bool records=argc>1 && std::string_view(argv[1])=="--json";
    for(const auto& coefficient:std::array<Q,2>{Q{1},Q{2,3}}){const auto b=native_relative_example::source(coefficient,Q{3,4});exercise(copies(b,2),b,false,records);}
    for(const auto& b:{native_cylinder_example::branch(),native_cylinder_example::ordered(),repeated(),native_cylinder_example::unit()})exercise(copies(b,2),b,false,records);
    exercise(cycle(6),cycle(3),true,records);exercise(cycle(9),cycle(3),true,records);contracts(records);
    if(!records)std::cout<<"Checks: "<<checks<<"; field cases: "<<field_cases<<"; lifts: "<<lift_cases<<"; monodromy cases: "<<monodromy_cases<<"; deck cases: "<<deck_cases<<"; pullback squares: "<<square_cases<<"; mediator cases: "<<mediator_cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
