#include "../../examples/native_cylinder/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_cylinder_example;
std::size_t checks{},chains{},maps{},route_cases{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F f,const char* message){bool caught=false;try{f();}catch(const Error&){caught=true;}check(caught,message);}
ProductValues values(inference::DerivedNativeCylinderField f){return f.field().values();}
ProductValues values(inference::MappedNativeCylinderField f){return f.field().values();}
Values values(inference::DerivedNativeCylinderChain c){return c.chain().values();}
void json(const Values& v){std::cout<<'[';bool comma=false;for(const auto& [i,q]:v){if(comma)std::cout<<',';comma=true;std::cout<<'['<<i<<",\""<<q<<"\"]";}std::cout<<']';}
void json(algebra::ProductCell id){std::cout<<'['<<id.left.grade.value<<','<<id.left.index<<','<<id.right.grade.value<<','<<id.right.index<<']';}
void json(const ProductValues& v){std::cout<<'[';bool comma=false;for(const auto& [id,q]:v){if(comma)std::cout<<',';comma=true;std::cout<<'[';json(id);std::cout<<",\""<<q<<"\"]";}std::cout<<']';}
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
void source_record(const Plan& plan){const auto n=plan.numeric_plan();std::cout<<"\"source\":";json(n.source());std::cout<<",\"interval\":";json(n.interval());}
void map_record(const Map& map,const inference::DerivedNativeCylinderField& input,const inference::MappedNativeCylinderField& output,bool records){
    const auto from=map.source(),to=map.target();const auto numeric=map.numeric_plan();Guard guard;
    std::optional<ProductValues> before,after;
    if(input.field().grade().value){
        const auto d=from.boundary(input,available);before=d.field().values();const auto td=to.numeric_plan().boundary(output.field(),guard);after=td.values();
        check(values(map.execute(d,map_available))==*after,"cylinder map fails boundary/support naturality");
    }
    check(output.input()->plan().same_owner(from),"cylinder map loses source field owner");
    check(output.field().product().same_owner(to.tensor_product()),"cylinder map output has wrong target product");
    ++maps;
    if(records){std::cout<<"{\"kind\":\"map\",";source_record(from);std::cout<<",\"target\":";const auto target=to.numeric_plan();json(target.source());
        std::cout<<",\"target_interval\":";json(target.interval());std::cout<<",\"source_map\":";json(numeric.factor_map(false));std::cout<<",\"interval_map\":";json(numeric.factor_map(true));
        std::cout<<",\"input\":";json(input.field().values());std::cout<<",\"output\":";json(output.field().values());std::cout<<",\"grade\":"<<input.field().grade().value;
        std::cout<<",\"boundary\":";if(before)json(*before);else std::cout<<"null";std::cout<<",\"target_boundary\":";if(after)json(*after);else std::cout<<"null";std::cout<<"}\n";}
}
void exercise(core::RelationalComplex c,bool records){
    const auto plan=prepare(c),target=prepare(c,true);const auto numeric=plan.numeric_plan();
    const auto map=Map::prepare(plan,target,map_request(plan,target),map_available);
    for(std::uint16_t k=0;k<c.grade_count();++k)for(std::uint32_t i=0;i<c.grade(grade(k)).cell_count();++i){
        Guard guard;const core::CellId original{grade(k),i};const auto routes=numeric.retained_prism_boundary(original,guard);++route_cases;
        const auto start=numeric.endpoint_cell(original,Endpoint::Start),finish=numeric.endpoint_cell(original,Endpoint::Finish);
        check(start.source_identity.left==finish.source_identity.left && start.source_identity.right!=finish.source_identity.right,"endpoint source identity pair changed");
        check(start.source_identity==numeric.reference().endpoint_identity(original,Endpoint::Start),"canonical endpoint identity differs");
        check(numeric.prism_cell(original).source_identity==numeric.reference().prism_identity(original),"canonical prism identity differs");
        const auto canonical=numeric.reference().prism_boundary(original);std::size_t active=0;
        for(const auto& r:routes){check(r.product().same_owner(numeric.product()),"retained route loses product owner");
            if(r.exists && r.coefficient!=0){check(active<canonical.size(),"canonical route population changed");const auto& t=canonical[active++];
                check(r.lower==t.lower && r.factor_occurrence==t.factor_occurrence && r.coefficient==t.coefficient && r.koszul_flip==t.koszul_flip,"canonical realized prism route differs");}}
        check(active==canonical.size(),"canonical prism route missing");
        if(records){std::cout<<"{\"kind\":\"routes\",";source_record(plan);std::cout<<",\"grade\":"<<k<<",\"cell\":"<<i<<",\"routes\":[";
            for(std::size_t p=0;p<routes.size();++p){if(p)std::cout<<',';const auto& r=routes[p];std::cout<<'[';json(r.lower);std::cout<<','<<static_cast<unsigned>(r.side)<<','<<r.factor_occurrence.value<<','<<r.attaching_position<<','<<r.koszul_flip<<','<<r.exists<<",\""<<r.factor_coefficient<<"\",\""<<r.coefficient<<"\",[";
                for(std::size_t j=0;j<r.factor_aspects.size();++j){if(j)std::cout<<',';std::cout<<r.factor_aspects[j];}std::cout<<"]]";}std::cout<<"]}\n";}
        for(const auto& q:std::array<Q,2>{Q{0},Q{-3,7}}){
            const auto input=plan.chain(native_chain(plan,grade(k),{{i,q}}),available);check(input.chain().values().size()==1,"supported zero source chain lost");
            const auto witness=plan.homotopy_witness(input,available);const auto& w=witness.witness();++chains;
            check(zero(w.residual().values()),"signed prism chain identity failed");check(witness.input()->native_input().has_value(),"witness loses native input");
            const auto j0=plan.endpoint(input,Endpoint::Start,available),j1=plan.endpoint(input,Endpoint::Finish,available);
            const auto p=plan.prism(input,available),h=plan.homotopy(input,available);
            check(j0.field().values()==w.start().values() && j1.field().values()==w.finish().values() && h.field().values()==w.homotopy().values(),"witness components differ from actions");
            check(p.field().values().size()==1 && p.field().grade().value==k+1u,"prism expands a primary source relation");
            if(k){const auto d=plan.source_boundary(input,available);check(values(plan.boundary(j0,available))==values(plan.endpoint(d,Endpoint::Start,available)),"start inclusion fails chain identity");
                check(values(plan.boundary(j1,available))==values(plan.endpoint(d,Endpoint::Finish,available)),"finish inclusion fails chain identity");
                check(zero(values(plan.boundary(plan.boundary(p,available),available))),"complete cylinder chain family fails");}
            for(const auto* f:{&j0,&j1,&p,&h})map_record(map,*f,map.execute(*f,map_available),records);
            if(records){std::cout<<"{\"kind\":\"witness\",";source_record(plan);std::cout<<",\"grade\":"<<k<<",\"input\":";json(input.chain().values());
                std::cout<<",\"source_boundary\":";if(w.source_boundary())json(w.source_boundary()->values());else std::cout<<"null";
                std::cout<<",\"start\":";json(w.start().values());std::cout<<",\"finish\":";json(w.finish().values());std::cout<<",\"homotopy\":";json(w.homotopy().values());
                std::cout<<",\"dH\":";json(w.boundary_homotopy().values());std::cout<<",\"Hd\":";json(w.homotopy_boundary().values());
                std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";}
        }
    }
}
void contracts(bool records){
    const auto plan=prepare();const auto numeric=plan.numeric_plan();const auto input=plan.chain(grade(1),{{0,Q{1}}},available);const auto p=plan.prism(input,available);
    check(storage::encode_rfcx(numeric.source()).bytes==storage::encode_rfcx(source()).bytes,"source canonical bytes changed");
    topology::RelationalInterval canonical;check(storage::encode_rfcx(numeric.interval()).bytes==storage::encode_rfcx(canonical.complex()).bytes,"interval canonical bytes changed");
    const auto branch_plan=prepare(branch());const auto branch_input=branch_plan.chain(grade(1),{{0,Q{1}}},available);const auto branch_numeric=branch_plan.numeric_plan();Guard branch_guard;
    const auto branch_routes=branch_numeric.retained_prism_boundary({grade(1),0},branch_guard);check(branch_routes.size()==6,"branch prism flattened into pairwise edges");
    check(values(branch_plan.homotopy(branch_input,available)).begin()->second==-1,"odd-grade signed prism orientation wrong");
    const auto survived=[](){auto owner=prepare();return owner.homotopy(owner.chain(grade(2),{{0,Q{1}}},available),available);}();check(survived.input()==nullptr && survived.source_input()->plan().same_owner(survived.plan()),"cylinder lifetime/ancestry lost");
    const auto d=plan.boundary(p,available);check(d.input()->source_input()->chain().values().at(0)==1,"boundary drops original source input");
    check(values(plan.source_boundary(input,available)).size()==3,"source boundary drops retained zero routes");
    const auto twin=prepare();rejects<std::invalid_argument>([&]{plan.chain(native_chain(twin,grade(1),{{0,Q{0}}}),available);},"equal-key foreign native Chain accepted");
    rejects<std::invalid_argument>([&]{plan.prism(twin.chain(grade(1),{{0,Q{0}}},available),available);},"equal-valued foreign source owner accepted");
    rejects<std::invalid_argument>([&]{plan.boundary(twin.prism(twin.chain(grade(1),{{0,Q{0}}},available),available),available);},"foreign cylinder field accepted");
    rejects<std::invalid_argument>([&]{plan.endpoint(input,static_cast<Endpoint>(7),available);},"unknown endpoint accepted");
    rejects<std::invalid_argument>([&]{plan.source_boundary(plan.chain(grade(0),{},available),available);},"grade-zero boundary fabricated");
    const auto empty=prepare(unit(true,4));const auto empty_witness=empty.homotopy_witness(empty.chain(grade(4),{},available),available);
    check(empty_witness.witness().residual().values().empty(),"empty cylinder action failed");
    auto cut=available;cut.left.max_sequence=12;rejects<std::invalid_argument>([&]{empty.chain(grade(4),{},cut);},"empty action accepts future source");
    cut=available;cut.right.max_sequence=16;rejects<std::invalid_argument>([&]{plan.homotopy_witness(input,cut);},"future interval accepted");
    cut=available;cut.right.source_namespace=999;rejects<std::invalid_argument>([&]{plan.prism(input,cut);},"foreign interval namespace accepted");
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;rejects<std::invalid_argument>([&]{empty.chain(grade(4),{},available,runtime::ExecutionLease(policy));},"empty action bypasses allocation admission");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;rejects<std::invalid_argument>([&]{plan.prism(input,available,runtime::ExecutionLease(policy));},"unsupported GPU selected");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;const auto fallback=plan.prism(input,available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"fallback missing from receipt");
    for(unsigned cap=0;cap<3;++cap){policy={};if(cap==0)policy.limits.scalar_work=1;else if(cap==1)policy.limits.integer_bits=0;else policy.limits.rational_slots=1;
        rejects<std::length_error>([&]{empty.homotopy_witness(empty.chain(grade(4),{},available),available,runtime::ExecutionLease(policy));},"current empty action skips full source/interval caps");}
    policy={};policy.limits.integer_bits=3;
    const auto unused_source=prepare(coalescing(false,true));
    rejects<std::length_error>([&]{unused_source.chain(grade(2),{},available,runtime::ExecutionLease(policy));},"unused cancelling source magnitude bypasses current bit cap");
    for(unsigned cap=0;cap<3;++cap){policy={};if(cap==0)policy.limits.scalar_work=1;else if(cap==1)policy.limits.integer_bits=0;else policy.limits.rational_slots=1;
        rejects<std::length_error>([&]{Plan::prepare(plan.declaration(),available,runtime::ExecutionLease(policy));},"preparation skips complete guarded sources");}
    const auto stop=std::make_shared<std::atomic_bool>(true);rejects<runtime::ExecutionCancelled>([&]{plan.boundary(p,available,runtime::ExecutionLease({},stop));},"owned cancellation bypassed");
    auto caller=plan.declaration();caller.declaration.types[0].key=123;check(plan.declaration().declaration.types[0].key!=123,"caller edits change cylinder declaration");
    Values draft{{0,Q{1}}};auto* alias=&draft.begin()->second;bool changed=false;Guard copied({},[&]{if(!changed){changed=true;*alias=Q{99};}});
    const auto captured=numeric.chain(grade(1),std::move(draft),copied);check(captured.values().at(0)==1,"moved value alias changed source chain");
    auto replaced=numeric;changed=false;const auto other=twin.numeric_plan();Guard replace({},[&]{if(!changed){changed=true;replaced=other;}});
    const auto own_chain=numeric.chain(grade(1),{{0,Q{1}}},branch_guard);check(replaced.prism(own_chain,replace).product().same_owner(numeric.product()),"callback replacement changes in-flight cylinder");
    const auto target=prepare(source(),true);const auto declaration=map_request(plan,target);const auto map=Map::prepare(plan,target,declaration,map_available);
    const auto numerical_map=map.numeric_plan();auto moved_map=numerical_map.factor_map(false);auto* map_alias=&moved_map.cells[0][0];changed=false;
    Guard map_copy({},[&]{if(!changed){changed=true;*map_alias=99;}});
    const auto frozen_map=topology::OwnedRelationalCylinderMap::prepare(numeric,target.numeric_plan(),std::move(moved_map),numerical_map.factor_map(true),map_copy);
    check(frozen_map.factor_map(false).cells[0][0]==0,"moved factor map alias changed captured directions");
    auto mutable_map=declaration;const auto fixed_map=Map::prepare(plan,target,mutable_map,map_available);mutable_map.interval.cells[0].to.key=999;
    check(fixed_map.declaration().interval.cells[0].to.key!=999,"caller edits change complete native interval map");
    auto bad=declaration;bad.source.cells.pop_back();rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"unused source map direction omitted");
    bad=declaration;bad.source.occurrences.pop_back();rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"inactive source occurrence map omitted");
    bad=declaration;bad.interval.occurrences.pop_back();rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"interval occurrence omitted");
    bad=declaration;std::swap(bad.interval.cells[0].to,bad.interval.cells[1].to);rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"endpoint reversal inferred without declared orientation");
    const auto loop=prepare(ordered()),loop_target=prepare(ordered(),true);auto loop_map=map_request(loop,loop_target);
    std::swap(loop_map.source.occurrences[2].to,loop_map.source.occurrences[4].to);
    rejects<std::invalid_argument>([&]{Map::prepare(loop,loop_target,loop_map,map_available);},"equal-coefficient ordered occurrences permuted");
    auto map_cut=map_available;map_cut.target.right.max_sequence=22;rejects<std::invalid_argument>([&]{map.execute(p,map_cut);},"future target interval accepted");
    map_cut=map_available;map_cut.target.left.max_sequence=18;rejects<std::invalid_argument>([&]{map.execute(p,map_cut);},"future target source accepted");
    map_cut=map_available;map_cut.source.right.max_sequence=16;rejects<std::invalid_argument>([&]{map.execute(p,map_cut);},"future source interval accepted by map");
    rejects<std::invalid_argument>([&]{map.execute(twin.prism(twin.chain(grade(1),{},available),available),map_available);},"foreign empty map input accepted");
    policy={};policy.limits.integer_bits=0;rejects<std::length_error>([&]{map.execute(plan.field(grade(4),{},available),map_available,runtime::ExecutionLease(policy));},"empty map skips full current source/target caps");
    std::vector<std::future<ProductValues>> jobs;for(unsigned i=0;i<6;++i)jobs.push_back(std::async(std::launch::async,[map,p]{return values(map.execute(p,map_available));}));for(auto& job:jobs)check(job.get()==p.field().values(),"concurrent cylinder map scratch changed");
    const auto from=prepare(coalescing()),to=prepare(coalescing(true),true);auto merge=map_request(from,to);
    for(auto& image:merge.source.cells)if(image.from.grade==grade(1))image.to.key=100;
    const auto merged_map=Map::prepare(from,to,merge,map_available);const auto paired=from.prism(from.chain(grade(1),{{0,Q{1}},{1,Q{-1}}},available),available);
    const auto merged=merged_map.execute(paired,map_available);check(merged.field().values().size()==1 && zero(merged.field().values()),"noninjective cylinder map loses supported cancellation");
    check(merged.input()->field().values().size()==2,"coalescing map erases original primary directions");map_record(merged_map,paired,merged,records);
    const auto unused_target=prepare(coalescing(true,true),true);auto unused_request=map_request(from,unused_target);
    for(auto& image:unused_request.source.cells)if(image.from.grade==grade(1))image.to.key=100;
    const auto unused_map=Map::prepare(from,unused_target,unused_request,map_available);policy={};policy.limits.integer_bits=3;
    rejects<std::length_error>([&]{unused_map.execute(from.field(grade(2),{},available),map_available,runtime::ExecutionLease(policy));},"unmapped cancelling target magnitude bypasses bit cap");
    const auto point=prepare(unit()),padded=prepare(unit(false,2,true),true);const auto point_map=Map::prepare(point,padded,map_request(point,padded),map_available);
    const auto point_prism=point.prism(point.chain(grade(0),{{0,Q{1}}},available),available);map_record(point_map,point_prism,point_map.execute(point_prism,map_available),records);
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(source(),13),930),b(storage::TemporalEndpoint(canonical.complex(),17),931);
    const auto old_plan=Plan::prepare(request(a,b),available);const auto old_chain=old_plan.chain(native_chain(old_plan,grade(1),{{0,Q{1}}}),available);
    const auto old=a.pin_snapshot();auto newer=*old;newer.sequence=14;a.advance(storage::make_temporal_segment(*old,newer));
    const auto later=a.pin(storage::NativeAccessionRequest::sparse_chain(grade(1),{100},{{100,Q{1}}}));
    rejects<std::invalid_argument>([&]{old_plan.chain(later,available);},"later native source substituted into old cylinder");
    const auto old_witness=old_plan.homotopy_witness(old_chain,available);
    check(old_plan.factor(false).sequence()==13 && zero(old_witness.witness().residual().values()),"source advancement rewrites old cylinder");
    const auto first=b.pin_snapshot();auto next=*first;next.sequence=18;b.advance(storage::make_temporal_segment(*first,next));check(old_plan.factor(true).sequence()==17,"interval advancement rewrites old cylinder");
    const auto future_request=request(a,b);rejects<std::invalid_argument>([&]{Plan::prepare(future_request,available);},"preparation reads future endpoints");
    storage::NativeAccessionDirectory ta(storage::TemporalEndpoint(source(),19),932),tb(storage::TemporalEndpoint(canonical.complex(),23),933);
    const auto old_target=Plan::prepare(request(ta,tb,true),target_available);const auto historical_map=Map::prepare(plan,old_target,map_request(plan,old_target),map_available);
    const auto ta_first=ta.pin_snapshot();auto ta_next=*ta_first;ta_next.sequence=20;ta.advance(storage::make_temporal_segment(*ta_first,ta_next));
    const auto tb_first=tb.pin_snapshot();auto tb_next=*tb_first;tb_next.sequence=24;tb.advance(storage::make_temporal_segment(*tb_first,tb_next));
    check(historical_map.target().factor(false).sequence()==19 && historical_map.target().factor(true).sequence()==23 && values(historical_map.execute(p,map_available))==p.field().values(),"target advancement rewrites old cylinder map");
    core::RelationalComplexBuilder renamed_builder(canonical.complex().signature());
    renamed_builder.add_grade_one(0,{{10,true,true,Q{1}},{20,true,false,Q{1}}});const auto renamed=renamed_builder.freeze();
    check(renamed.boundary_apply(grade(1),{Q{1}})==canonical.complex().boundary_apply(grade(1),{Q{1}}),"equal-matrix noncanonical interval case malformed");
    storage::NativeAccessionDirectory wrong_interval(storage::TemporalEndpoint(renamed,17),931);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(a,wrong_interval),{{930,14},{931,17}});},"noncanonical equal-matrix interval admitted");
}
}
int main(int argc,char** argv){try{const bool records=argc>1 && std::string_view(argv[1])=="--json";
    for(const auto& e:std::array<Q,2>{Q{1},Q{2,3}})for(const auto& f:std::array<Q,2>{Q{1},Q{3,4}})exercise(source(e,f),records);
    exercise(branch(),records);exercise(ordered(),records);exercise(unit(),records);exercise(coalescing(),records);contracts(records);
    if(!records)std::cout<<"Checks: "<<checks<<"; source chains: "<<chains<<"; map actions: "<<maps<<"; prism route cases: "<<route_cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
