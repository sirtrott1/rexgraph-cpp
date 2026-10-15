#include "../../examples/native_relative/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_relative_example;
std::size_t checks{},cases{},map_cases{},witnesses{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F f,const char* message){bool caught=false;try{f();}catch(const Error&){caught=true;}check(caught,message);}
Values values(inference::DerivedNativeRelativeChain c){return c.chain().values();}
Values values(inference::MappedNativeRelativeChain c){return c.chain().values();}
Values values(algebra::OwnedRelativeChain c){return c.values();}
bool fallback(inference::DerivedNativeRelativeChain c){return c.provenance().fallback;}
void json(const Values& values){std::cout<<'[';bool comma=false;for(const auto& [i,q]:values){if(comma)std::cout<<',';comma=true;std::cout<<'['<<i<<",\""<<q<<"\"]";}std::cout<<']';}
void json(const core::RelationalComplex& c){
    std::cout<<"{\"sizes\":[";for(std::uint16_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<c.grade(grade(k)).cell_count();}
    std::cout<<"],\"occurrences\":[";
    for(std::uint16_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';const auto g=grade(k);const auto& a=c.grade(g);std::cout<<'[';
        for(std::size_t p=0;p<a.incidence_count();++p){if(p)std::cout<<',';std::cout<<'['<<a.upper_index[p]<<','<<a.lower_index[p]<<','<<a.occurrence_identities[p].value<<','<<a.aspects.get(c.signature().existence_aspect(),p)<<",\""<<c.incidence_coefficient(g,p)<<"\",[";
            for(std::size_t j=0;j<c.signature().aspects().size();++j){if(j)std::cout<<',';std::cout<<a.aspects.get(static_cast<composite::AspectId>(j),p);}std::cout<<"]]";}std::cout<<']';}
    std::cout<<"]}";
}
void json(const algebra::OwnedRelativePair& pair){std::cout<<'[';for(const auto s:{Space::Whole,Space::Sub,Space::Quotient}){if(s!=Space::Whole)std::cout<<',';std::cout<<'[';
    for(std::uint16_t k=0;k<pair.ambient().grade_count();++k){if(k)std::cout<<',';const auto& row=pair.coordinates(s,grade(k));std::cout<<'[';for(std::size_t i=0;i<row.size();++i){if(i)std::cout<<',';std::cout<<row[i];}std::cout<<']';}std::cout<<']';}std::cout<<']';}
void json(const algebra::RetainedSourceMap& map){std::cout<<"{\"cells\":[";for(std::size_t k=0;k<map.cells.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<map.cells[k].size();++i){if(i)std::cout<<',';std::cout<<map.cells[k][i];}std::cout<<']';}
    std::cout<<"],\"occurrences\":[";for(std::size_t k=0;k<map.occurrences.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<map.occurrences[k].size();++i){if(i)std::cout<<',';std::cout<<map.occurrences[k][i];}std::cout<<']';}std::cout<<"]}";}
void samples(bool records){
    for(const auto& e:std::array<Q,2>{Q{1},Q{2,3}})for(const auto& f:std::array<Q,2>{Q{1},Q{3,4}})
    for(const auto mode:{Pair::Face,Pair::Full,Pair::Point,Pair::Path,Pair::Empty,Pair::Equal,Pair::Boundary}){
        const auto plan=prepare(mode,e,f);const auto pair=plan.numeric_plan();
        storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(source(e,f),15),921);
        for(unsigned family=0;family<2;++family){
            const auto target=Plan::prepare(request(directory,family?Pair::Equal:mode,600),target_available);
            const auto map=Map::prepare(plan,target,identity_map(plan,target),map_available);const auto numeric_map=map.numeric_plan();
            for(const auto s:{Space::Whole,Space::Sub,Space::Quotient})for(std::uint16_t k=0;k<5;++k)
            for(std::uint32_t i=0;i<pair.coordinates(s,grade(k)).size();++i)for(const auto& q:std::array<Q,2>{Q{0},Q{-3,7}}){
                const auto input=plan.chain(s,grade(k),{{i,q}},available);const auto out=map.execute(input,map_available);
                check(input.chain().values().size()==1,"supported zero chain disappeared");check(out.input()->plan().same_owner(plan),"map loses original pair/input");
                check(out.chain().pair().same_owner(target.numeric_plan()),"map output has foreign target pair");
                std::optional<Values> before,after;
                if(k){
                    const auto d=plan.boundary(input,available);before=d.chain().values();Guard guard;const auto td=target.numeric_plan().boundary(out.chain(),guard);after=td.values();
                    check(values(map.execute(d,map_available))==td.values(),"relative map fails full boundary/support naturality");
                    if(k>1)check(zero(values(plan.boundary(d,available))),"relative chain family fails");
                    numeric::RationalVector dense(pair.coordinates(s,grade(k)).size(),Q{0});dense[i]=q;
                    if(s==Space::Quotient){const auto reference=pair.reference().boundary_apply(grade(k),dense);for(std::uint32_t j=0;j<reference.size();++j){const auto it=before->find(j);check(reference[j]==(it==before->end()?Q{0}:it->second),"canonical relative constructor disagrees");}}
                    if(s==Space::Whole){const auto p=plan.project(input,available);const auto pd=plan.project(d,available);check(values(plan.boundary(p,available))==pd.chain().values(),"projection is not a chain map");}
                    if(s==Space::Sub){const auto inj=plan.inject(input,available);const auto di=plan.inject(d,available);check(values(plan.boundary(inj,available))==di.chain().values(),"inclusion is not a chain map");}
                    if(s==Space::Quotient && zero(*before)){
                        const auto w=plan.connecting_witness(input,available);if(k>1)check(zero(values(pair.boundary(w.witness().sub_boundary(),guard))),"connecting boundary is not a cycle");
                        ++witnesses;
                        if(records){std::cout<<"{\"kind\":\"witness\",\"source\":";json(pair.ambient());std::cout<<",\"coordinates\":";json(pair);std::cout<<",\"grade\":"<<k<<",\"input\":";json(input.chain().values());std::cout<<",\"representative\":";json(w.witness().representative().values());std::cout<<",\"whole_boundary\":";json(w.witness().whole_boundary().values());std::cout<<",\"sub_boundary\":";json(w.witness().sub_boundary().values());std::cout<<"}\n";}
                    }
                }
                if(s==Space::Quotient){const auto lift=plan.representative(input,available);check(values(plan.project(lift,available))==input.chain().values(),"declared representative does not project to input");}
                if(records){std::cout<<"{\"kind\":\"chain\",\"family\":"<<family<<",\"source\":";json(pair.ambient());std::cout<<",\"coordinates\":";json(pair);std::cout<<",\"target\":";const auto target_pair=numeric_map.target();json(target_pair.ambient());std::cout<<",\"target_coordinates\":";json(target.numeric_plan());std::cout<<",\"map\":";json(numeric_map.declaration());std::cout<<",\"space\":"<<static_cast<unsigned>(s)<<",\"grade\":"<<k<<",\"input\":";json(input.chain().values());std::cout<<",\"output\":";json(out.chain().values());std::cout<<",\"boundary\":";if(before)json(*before);else std::cout<<"null";std::cout<<",\"target_boundary\":";if(after)json(*after);else std::cout<<"null";std::cout<<"}\n";}
                ++map_cases;if(!family)++cases;
            }
        }
    }
}
void contracts(){
    const auto plan=prepare();const auto pair=plan.numeric_plan();
    check(pair.ambient().grade_count()==5 && pair.coordinates(Space::Quotient,grade(4)).empty(),"declared empty grade lost");
    check(storage::encode_rfcx(pair.ambient()).bytes==storage::encode_rfcx(source()).bytes,"original canonical bytes changed");
    const auto relative=plan.project(native_chain(plan,grade(2),{{0,Q{1}}}),available);
    check(relative.native_input().has_value() && relative.input()->chain().space()==Space::Whole,"native projection loses original field/whole chain");
    const auto w=plan.connecting_witness(relative,available);check(w.witness().sub_boundary().values().size()==4 && w.witness().sub_boundary().values().at(3)==0,"inactive route missing from connecting witness");
    check(!zero(w.witness().whole_boundary().values()) && values(plan.boundary(relative,available)).empty(),"representative wrongly treated as chain inverse");
    const auto full=prepare(Pair::Full);const auto fpair=full.numeric_plan();
    rejects<std::invalid_argument>([&]{full.connecting_witness(full.chain(Space::Quotient,grade(1),{{0,Q{1}}},available),available);},"non-cycle has a connecting witness");
    const auto zero_edge=full.chain(Space::Quotient,grade(1),{{3,Q{1}}},available);const auto empty_sub=full.connecting_witness(zero_edge,available);
    check(empty_sub.witness().whole_boundary().values().size()==1 && empty_sub.witness().sub_boundary().values().empty(),"supported zero outside subcomplex erased or fabricated inside subcomplex");
    const auto [whole,sub]=masks(source(),Pair::Face);auto active=whole;active.select({grade(1),3},false);
    check(active.active_boundary_closed(source()) && !active.boundary_closed(source()),"inactive closure counterexample malformed");
    algebra::SubcomplexMask no_subcomplex(source());Guard closure;rejects<std::invalid_argument>([&]{algebra::OwnedRelativePair::prepare(source(),active,no_subcomplex,closure);},"active-only closure accepted");
    algebra::SubcomplexMask missing_zero(source()),empty(source());missing_zero.select({grade(1),3});
    Guard zero_guard;rejects<std::invalid_argument>([&]{algebra::OwnedRelativePair::prepare(source(),missing_zero,empty,zero_guard);},"zero numerical column bypassed retained closure");
    auto external=whole;auto* mask_alias=&external.grade(grade(0));bool changed=false;Guard copy_guard({},[&]{if(!changed){changed=true;mask_alias->set(0,false);}});
    const auto frozen=algebra::OwnedRelativePair::prepare(source(),std::move(external),sub,copy_guard);check(frozen.mask(false).contains({grade(0),0}),"moved mask aliases checkpoint mutation");
    Values draft{{0,Q{1}}};auto* alias=&draft.begin()->second;changed=false;Guard values_guard({},[&]{if(!changed){changed=true;*alias=Q{99};}});
    const auto captured=pair.chain(Space::Quotient,grade(2),std::move(draft),values_guard);check(captured.values().at(0)==1,"moved numeric values alias checkpoint mutation");
    auto replaced=pair;changed=false;Guard replacement({},[&]{if(!changed){changed=true;replaced=fpair;}});
    check(values(replaced.boundary(captured,replacement)).empty(),"pair replacement changed in-flight owner");
    auto cut=available;cut.max_sequence=10;rejects<std::invalid_argument>([&]{plan.chain(Space::Quotient,grade(4),{},cut);},"future source cutoff accepted for empty action");
    cut=available;cut.source_namespace=999;rejects<std::invalid_argument>([&]{plan.boundary(relative,cut);},"foreign namespace accepted");
    const auto twin=prepare();rejects<std::invalid_argument>([&]{plan.project(native_chain(twin,grade(2),{{0,1}}),available);},"foreign equal-valued native field accepted");
    rejects<std::invalid_argument>([&]{plan.boundary(twin.chain(Space::Quotient,grade(2),{{0,1}},available),available);},"foreign equal-shaped pair accepted");
    rejects<std::invalid_argument>([&]{plan.project(native_chain(plan,grade(3),{{0,0}}),available);},"supported zero outside whole accepted");
    rejects<std::invalid_argument>([&]{plan.inject(relative,available);},"wrong coordinate space accepted");
    const auto empty_pair=prepare(Pair::Empty);check(values(empty_pair.chain(Space::Quotient,grade(4),{},available)).empty(),"empty pair action failed");
    const auto survived=[](){auto p=prepare();return p.chain(Space::Quotient,grade(2),{{0,1}},available);}();check(survived.plan().connecting_witness(survived,available).input()->plan().same_owner(survived.plan()),"pair lifetime/ancestry lost");
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;rejects<std::invalid_argument>([&]{plan.chain(Space::Quotient,grade(4),{},available,runtime::ExecutionLease(policy));},"allocation policy bypass");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;rejects<std::invalid_argument>([&]{plan.boundary(relative,available,runtime::ExecutionLease(policy));},"unsupported GPU selected");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;check(fallback(plan.boundary(relative,available,runtime::ExecutionLease(policy))),"fallback absent from receipt");
    for(unsigned cap=0;cap<3;++cap){policy={};if(cap==0)policy.limits.scalar_work=1;else if(cap==1)policy.limits.integer_bits=0;else policy.limits.rational_slots=1;
        rejects<std::length_error>([&]{plan.chain(Space::Quotient,grade(4),{},available,runtime::ExecutionLease(policy));},"current full-source limits bypassed for empty action");}
    const auto stop=std::make_shared<std::atomic_bool>(true);rejects<runtime::ExecutionCancelled>([&]{plan.boundary(relative,available,runtime::ExecutionLease({},stop));},"owned cancellation bypass");
    storage::NativeAccessionDirectory target_dir(storage::TemporalEndpoint(source(),15),921);const auto target=Plan::prepare(request(target_dir,Pair::Face,600),target_available);
    auto declaration=identity_map(plan,target);const auto map=Map::prepare(plan,target,declaration,map_available);
    auto bad=declaration;bad.map.cells.pop_back();rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"unused original cell map direction omitted");
    bad=declaration;bad.map.occurrences.pop_back();rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"inactive occurrence map omitted");
    bad=declaration;std::swap(bad.map.occurrences[0].to,bad.map.occurrences[1].to);rejects<std::invalid_argument>([&]{Map::prepare(plan,target,bad,map_available);},"changed aspects/attachments accepted");
    const auto no_sub=Plan::prepare(request(target_dir,Pair::Full,601),target_available);rejects<std::invalid_argument>([&]{Map::prepare(plan,no_sub,identity_map(plan,no_sub),map_available);},"map does not preserve subcomplex");
    const auto no_whole=Plan::prepare(request(target_dir,Pair::Empty,602),target_available);rejects<std::invalid_argument>([&]{Map::prepare(plan,no_whole,identity_map(plan,no_whole),map_available);},"map does not preserve whole");
    const auto mapped=map.execute(relative,map_available);check(mapped.input()->native_input().has_value() && mapped.plan().target().same_owner(target),"map loses target or input provenance");
    auto map_cut=map_available;map_cut.target.max_sequence=14;rejects<std::invalid_argument>([&]{map.execute(relative,map_cut);},"future target cutoff accepted");
    map_cut=map_available;map_cut.source.max_sequence=10;rejects<std::invalid_argument>([&]{map.execute(relative,map_cut);},"future source map cutoff accepted");
    rejects<std::invalid_argument>([&]{map.execute(twin.chain(Space::Quotient,grade(2),{{0,1}},available),map_available);},"foreign map input accepted");
    auto caller=declaration;const auto fixed_map=Map::prepare(plan,target,caller,map_available);caller.map.cells[0].to.key=999;check(fixed_map.declaration().map.cells[0].to.key!=999,"caller edits change owned map");
    std::vector<std::future<Values>> jobs;for(unsigned i=0;i<6;++i)jobs.push_back(std::async(std::launch::async,[plan,map,relative]{return values(map.execute(plan.boundary(relative,available),map_available));}));for(auto& job:jobs)check(job.get().empty(),"concurrent scratch changed exact result");
    storage::NativeAccessionDirectory original_dir(storage::TemporalEndpoint(source(),11),920);
    auto native_request=request(original_dir,Pair::Face);const auto captured_native=Plan::prepare(native_request,available);native_request.whole[0].key=999;
    check(captured_native.declaration().whole[0].key!=999,"native pair keeps a caller declaration alias");
    const auto first=original_dir.pin_snapshot();auto later=*first;later.sequence=12;original_dir.advance(storage::make_temporal_segment(*first,later));
    const auto later_field=original_dir.pin(storage::NativeAccessionRequest::sparse_chain(grade(2),{100},{{100,Q{1}}}));
    rejects<std::invalid_argument>([&]{captured_native.project(later_field,available);},"later field substituted into historical pair");
    check(captured_native.source().sequence()==11,"source advancement rewrites old pair");
    policy={};policy.limits.integer_bits=0;rejects<std::length_error>([&]{map.execute(plan.chain(Space::Quotient,grade(4),{},available),map_available,runtime::ExecutionLease(policy));},"empty map bypasses full target/source limits");
    core::RelationSignature small_signature;small_signature.add_type("empty-assertion");
    core::RelationalComplexBuilder from_builder(small_signature),to_builder(small_signature);
    from_builder.declare_grade(grade(2));to_builder.declare_grade(grade(2));from_builder.declare_witness_identity(10);to_builder.declare_witness_identity(10);
    from_builder.add_grade_one(core::RelationKey{100},0,{});from_builder.add_grade_one(core::RelationKey{200},0,{});to_builder.add_grade_one(core::RelationKey{100},0,{});
    storage::NativeAccessionDirectory from_dir(storage::TemporalEndpoint(from_builder.freeze(),11),920),to_dir(storage::TemporalEndpoint(to_builder.freeze(),15),921);
    const auto from_pair=Plan::prepare(request(from_dir,Pair::Full),available),to_pair=Plan::prepare(request(to_dir,Pair::Full,600),target_available);
    auto collapse=identity_map(from_pair,to_pair);for(auto& image:collapse.map.cells)if(image.from.grade==grade(1))image.to.key=100;
    const auto collapse_map=Map::prepare(from_pair,to_pair,collapse,map_available);const auto distinct=from_pair.chain(Space::Quotient,grade(1),{{0,Q{1}},{1,Q{-1}}},available);
    const auto cancelled=collapse_map.execute(distinct,map_available);check(cancelled.chain().values().size()==1 && cancelled.chain().values().at(0)==0,"noninjective pair map loses supported cancellation");
    const auto cancelled_plan=cancelled.plan();
    check(cancelled.input()->chain().values().size()==2 && cancelled_plan.declaration().map.cells.size()==3,"cancelled map erases original directions/history");
    rejects<std::invalid_argument>([&]{core::RelationSignature signature;signature.add_type("edge");signature.add_type("face");core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(core::RelationKey{100},0,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},{20,true,false,Q{1},{},core::OccurrenceKey{102}}});
        builder.add_higher(grade(2),core::RelationKey{200},1,{{0,true,false,Q{1},{},core::OccurrenceKey{201}}});},"canonical builder accepted an adjacent chain defect");
    const auto old=target_dir.pin_snapshot();auto next=*old;next.sequence=16;target_dir.advance(storage::make_temporal_segment(*old,next));check(map.target().source().sequence()==15 && values(map.execute(relative,map_available)).at(0)==1,"target advancement rewrites old map");
}
}
int main(int argc,char** argv){try{const bool records=argc>1 && std::string_view(argv[1])=="--json";samples(records);contracts();if(!records)std::cout<<"Checks: "<<checks<<"; relative chain cases: "<<cases<<"; map cases: "<<map_cases<<"; connecting witnesses: "<<witnesses<<'\n';return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
