#include "../../examples/native_relational_homotopy/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_relational_homotopy_example;
std::size_t checks{}, witnesses{}, pushes{}, flags{};
Values vals(inference::DerivedNativeHomotopyChain output) { return output.chain().values(); }
void check(bool ok,const char* message) { ++checks;if(!ok)throw std::runtime_error(message); }
template<class E=std::invalid_argument,class F>void rejects(F action,const char* message) {
    bool caught=false;try{action();}catch(const E&){caught=true;}check(caught,message);
}
void json(const Values& v) {
    std::cout<<'[';bool comma=false;for(const auto& [i,q]:v){if(comma)std::cout<<',';comma=true;std::cout<<'['<<i<<",\""<<q<<"\"]";}std::cout<<']';
}
void json(algebra::ProductCell id) { std::cout<<'['<<id.left.grade.value<<','<<id.left.index<<','<<id.right.grade.value<<','<<id.right.index<<']'; }
void json(const core::RelationalComplex& c) {
    std::cout<<"{\"sizes\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<c.grade(topology::owned_cylinder_detail::grade(k)).cell_count();}
    std::cout<<"],\"types\":[";
    for(std::size_t i=0;i<c.signature().type_count();++i){if(i)std::cout<<',';std::cout<<'"'<<c.signature().type(static_cast<core::RelationTypeId>(i)).name<<'"';}
    std::cout<<"],\"cell_types\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<'[';const auto& a=c.grade(topology::owned_cylinder_detail::grade(k));
        for(std::size_t i=0;i<a.cell_count();++i){if(i)std::cout<<',';if(k)std::cout<<a.relation_types[i];else std::cout<<"null";}std::cout<<']';}
    std::cout<<"],\"flags\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';const auto g=topology::owned_cylinder_detail::grade(k);const auto& a=c.grade(g);std::cout<<'[';
        for(std::size_t p=0;p<a.incidence_count();++p){if(p)std::cout<<',';std::cout<<'['<<a.upper_index[p]<<','<<a.lower_index[p]<<','<<a.occurrence_identities[p].value<<",\""<<c.incidence_coefficient(g,p)<<"\",[";
            for(std::size_t j=0;j<c.signature().aspects().size();++j){if(j)std::cout<<',';std::cout<<a.aspects.get(static_cast<composite::AspectId>(j),p);}std::cout<<"]]";}std::cout<<']';}
    std::cout<<"]}";
}
void json(const algebra::RetainedSourceMap& m) {
    const auto rows=[](const auto& matrix){std::cout<<'[';for(std::size_t k=0;k<matrix.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<matrix[k].size();++i){if(i)std::cout<<',';std::cout<<matrix[k][i];}std::cout<<']';}std::cout<<']';};
    std::cout<<"{\"cells\":";rows(m.cells);std::cout<<",\"flags\":";rows(m.occurrences);std::cout<<'}';
}
void source_record(const Plan& plan,unsigned kind) {
    const auto cylinder=plan.cylinder();const auto numeric=plan.numeric_plan();const auto carrier=numeric.carrier();
    std::cout<<"{\"kind\":\"source\",\"id\":"<<kind<<",\"source\":";json(cylinder.factor(false).endpoint()->source);
    std::cout<<",\"interval\":";json(cylinder.factor(true).endpoint()->source);
    std::cout<<",\"carrier\":";json(carrier.carrier());std::cout<<",\"target\":";json(numeric.target());
    std::cout<<",\"type_interpretation\":[";const auto& d=carrier.declaration();const auto product=cylinder.tensor_product();
    for(std::size_t i=0;i<product.declaration().types.size();++i){if(i)std::cout<<',';const auto& t=product.declaration().types[i];std::cout<<'[';
        if(t.left)std::cout<<*t.left;else std::cout<<"null";std::cout<<',';if(t.right)std::cout<<*t.right;else std::cout<<"null";
        const auto it=std::find_if(d.types.begin(),d.types.end(),[&](const auto& value){return value.product_type==t.key;});std::cout<<',';if(it->carrier_type)std::cout<<*it->carrier_type;else std::cout<<"null";std::cout<<']';}
    std::cout<<"],\"cells\":[";
    for(std::size_t i=0;i<d.cells.size();++i){if(i)std::cout<<',';std::cout<<'[';json(d.cells[i].from);std::cout<<','<<d.cells[i].to<<']';}
    std::cout<<"],\"flags\":[";
    for(std::size_t i=0;i<d.occurrences.size();++i){if(i)std::cout<<',';const auto& f=d.occurrences[i];std::cout<<'[';json(f.from.upper);std::cout<<','<<static_cast<unsigned>(f.from.side)<<','<<f.from.occurrence.value<<','<<f.to<<']';}
    std::cout<<"],\"h\":";json(numeric.declaration().cylinder_target);std::cout<<",\"first\":";json(numeric.declaration().first);
    std::cout<<",\"second\":";json(numeric.declaration().second);std::cout<<"}\n";
}
void exercise(unsigned kind,bool records,Plan plan) {
    const auto c=plan.cylinder();const auto source=c.factor(false).endpoint()->source;
    if(records)source_record(plan,kind);
    flags+=plan.declaration().realization.occurrences.size();
    for(std::uint16_t k=0;k<source.grade_count();++k) {
        std::vector<Values> cases{{}}; Values all;
        for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i){for(const auto& q:std::array<Q,3>{Q{0},Q{2,5},Q{-3,7}})cases.push_back({{i,q}});all.emplace(i,i%2?Q{-2,9}:Q{4,11});}
        if(!all.empty())cases.push_back(all);
        for(const auto& values:cases) {
            const auto input=c.chain(native_chain(c,grade(k),values),available.cylinder);const auto output=plan.witness(input,available);const auto& w=output.witness();++witnesses;
            check(zero(w.residual().values()),"target homotopy identity failed");check(output.plan().same_owner(plan),"witness loses actual homotopy owner");
            check(output.input()->native_input().has_value(),"witness loses original native Chain");
            check(w.cylinder_witness().input().values()==values,"witness loses supported original input");
            check(w.first().values()==vals(plan.endpoint(input,Endpoint::Start,available)),"first endpoint differs");
            check(w.second().values()==vals(plan.endpoint(input,Endpoint::Finish,available)),"second endpoint differs");
            const auto h=plan.homotopy(input,available);check(h.chain().values()==w.homotopy().values(),"homotopy action differs");
            const auto dh=plan.boundary(h,available);check(dh.chain().values()==w.boundary_homotopy().values(),"target boundary action differs");
            check(dh.input()->source_input()->native_input().has_value(),"target boundary loses original native input history");
            check(w.homotopy().grade().value==k+1u,"homotopy grade is not source grade plus one");
            const auto product=c.homotopy(input,available.cylinder);const auto pushed=plan.push(product,available);++pushes;
            check(pushed.chain().values()==w.homotopy().values() && pushed.product_input()->source_input()->native_input().has_value(),"product push loses actual history");
            if(k){const auto d=c.source_boundary(input,available.cylinder);check(w.homotopy_boundary().values()==vals(plan.homotopy(d,available)),"Hd differs");}
            else check(w.homotopy_boundary().values().empty(),"grade-zero witness invents negative source boundary");
            for(std::size_t p=0;p<4;++p)check(output.provenance().sequences[p]==std::array<std::uint64_t,4>{11,13,17,19}[p],"witness loses one original native cutoff");
            if(values.size()==1 && values.begin()->second==0)check(w.homotopy().values().size()==1,"supported zero homotopy lost");
            if(records){std::cout<<"{\"kind\":\"witness\",\"id\":"<<kind<<",\"grade\":"<<k<<",\"input\":";json(values);
                std::cout<<",\"source_boundary\":";if(w.cylinder_witness().source_boundary())json(w.cylinder_witness().source_boundary()->values());else std::cout<<"null";
                std::cout<<",\"first\":";json(w.first().values());std::cout<<",\"second\":";json(w.second().values());std::cout<<",\"h\":";json(w.homotopy().values());
                std::cout<<",\"dh\":";json(w.boundary_homotopy().values());std::cout<<",\"hd\":";json(w.homotopy_boundary().values());
                std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";}
        }
    }
}
core::RelationalComplex rebuild(const core::RelationalComplex& c,std::optional<std::size_t> flip={},bool extra=false) {
    core::RelationalComplexBuilder b(c.signature(),c.semantic_scale());b.declare_grade(c.max_grade());
    for(const auto key:c.grade(grade(0)).identities)b.declare_witness_identity(key);
    for(std::uint16_t k=1;k<c.grade_count();++k){const auto g=grade(k);const auto& a=c.grade(g);
        for(std::size_t i=0;i<a.cell_count();++i){std::vector<core::BoundaryOccurrenceInput> low;std::vector<core::HigherIncidenceInput> high;const auto [begin,end]=a.incidence_range(i);
            for(auto p=begin;p<end;++p){std::vector<core::AspectValue> aspects;
                for(std::size_t j=0;j<c.signature().aspects().size();++j)if(j!=c.signature().existence_aspect() && j!=c.signature().orientation_aspect())aspects.push_back({static_cast<composite::AspectId>(j),a.aspects.get(static_cast<composite::AspectId>(j),p)});
                const auto q=c.incidence_coefficient(g,p);const auto magnitude=q<0?-q:q;
                const bool negative=a.aspects.get(c.signature().orientation_aspect(),p)!=(k==1 && flip && *flip==p);
                const bool exists=a.aspects.get(c.signature().existence_aspect(),p);
                if(k==1)low.emplace_back(c.grade(grade(0)).identities[a.lower_index[p]],exists,negative,magnitude,aspects,a.occurrence_identities[p]);
                else high.emplace_back(a.lower_index[p],exists,negative,magnitude,aspects,a.occurrence_identities[p]);
            }
            if(k==1)b.add_grade_one(a.relation_identities[i],a.relation_types[i],low);else b.add_higher(g,a.relation_identities[i],a.relation_types[i],high);
        }
        if(k==1 && extra){const auto vertex=c.grade(grade(0)).identities.front();b.add_grade_one(core::RelationKey{999999},0,
            {{vertex,true,true,Q{257},{},core::OccurrenceKey{999991}}, {vertex,true,false,Q{257},{},core::OccurrenceKey{999992}}});}
    }return b.freeze();
}
void contracts() {
    const auto plan=prepare(1);const auto c=plan.cylinder();const auto request=plan.declaration();
    const auto invalid=[&](auto edit,const char* message){auto draft=request;edit(draft);rejects([&]{Plan::prepare(c,std::move(draft),available);},message);};
    invalid([](auto& r){r.realization.cells.pop_back();},"partial product cell carrier accepted");
    invalid([](auto& r){r.realization.cells.push_back(r.realization.cells.front());},"duplicate product cell accepted");
    invalid([](auto& r){r.realization.cells[0].to=r.realization.cells[1].to;},"distinct pairs identified in supplied carrier");
    invalid([](auto& r){r.realization.cells[0].from.left.key=999999;},"foreign factor identity accepted");
    invalid([](auto& r){r.realization.occurrences.pop_back();},"partial retained product flags accepted");
    invalid([](auto& r){r.realization.occurrences[0].occurrence=core::OccurrenceKey{999999};},"invented factor occurrence accepted");
    invalid([](auto& r){r.realization.occurrences[0].side=static_cast<algebra::ProductBoundarySide>(99);},"invented product side accepted");
    invalid([](auto& r){std::swap(r.realization.occurrences[0].to,r.realization.occurrences[1].to);},"changed product attaching order accepted");
    invalid([](auto& r){r.realization.types.pop_back();},"unused product type omitted");
    invalid([](auto& r){r.realization.types[2].carrier_type=2;},"endpoint unit type law violated");
    invalid([](auto& r){r.realization.types[0].carrier_type=0;},"entity product interpreted as a relation");
    invalid([](auto& r){r.first=r.second;r.first.key=1602;},"wrong full endpoint restriction accepted");
    invalid([](auto& r){std::swap(r.first.occurrences[4].to,r.first.occurrences[5].to);},"equal zero flags conceal wrong endpoint restriction");
    invalid([](auto& r){r.first.occurrences.pop_back();},"partial endpoint occurrence map accepted");
    invalid([](auto& r){r.cylinder_target.cells.pop_back();},"partial cylinder target map accepted");
    invalid([](auto& r){r.second.from=r.carrier_source;},"foreign endpoint source accepted");
    invalid([](auto& r){r.target_source=r.carrier_source;},"indistinguishable declared carrier names accepted");
    invalid([](auto& r){r.realization.revision=0;},"missing constructor revision accepted");
    // These supplied carriers have exactly the same aggregate boundaries.
    // Original inactive and zero orientation words still constrain the source.
    const auto& carrier_source=request.carrier.endpoint()->source;
    std::optional<std::size_t> inactive,active_zero;const auto& arena=carrier_source.grade(grade(1));
    for(std::size_t p=0;p<arena.incidence_count();++p){if(!arena.aspects.get(carrier_source.signature().existence_aspect(),p))inactive=p;
        else if(carrier_source.incidence_coefficient(grade(1),p)==0)active_zero=p;}
    check(inactive && active_zero,"aspect rejection fixture lacks inactive/zero flags");
    for(const auto p:{*inactive,*active_zero}){auto draft=request;const auto altered=rebuild(carrier_source,p);
        storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(altered,17),1103);draft.carrier=native_cylinder_example::anchor(directory);
        rejects([&]{Plan::prepare(c,draft,available);},"equal aggregate boundary conceals a changed original aspect");}
    const auto foreign=prepare(1);const auto input=c.chain(grade(1),{{0,Q{1}}},available.cylinder);
    rejects([&]{foreign.homotopy(input,available);},"equal source bytes identify unrelated cylinder owner");
    rejects([&]{foreign.push(c.prism(input,available.cylinder),available);},"foreign equal cylinder field accepted");
    rejects([&]{foreign.boundary(plan.homotopy(input,available),available);},"foreign equal target chain accepted");
    rejects([&]{plan.endpoint(input,static_cast<Endpoint>(99),available);},"unknown endpoint accepted");
    rejects([&]{plan.boundary(plan.endpoint(c.chain(grade(0),{},available.cylinder),Endpoint::Start,available),available);},"grade-zero target has invented boundary");
    for(unsigned i=0;i<4;++i){auto a=available;auto* cutoff=i==0?&a.cylinder.left:i==1?&a.cylinder.right:i==2?&a.carrier:&a.target;--cutoff->max_sequence;
        rejects([&]{Plan::prepare(c,request,a);},"preparation skips one native availability");
        rejects([&]{plan.witness(input,a);},"execution skips one native availability");
        cutoff->source_namespace=999999;rejects([&]{plan.homotopy(input,a);},"foreign cutoff namespace accepted");}
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
    rejects([&]{plan.homotopy(input,available,runtime::ExecutionLease(policy));},"allocation admission bypassed");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{plan.homotopy(input,available,runtime::ExecutionLease(policy));},"unsupported GPU backend silently accepted");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=plan.homotopy(input,available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"explicit fallback not retained");
    for(unsigned cap=0;cap<4;++cap){policy={};if(cap==0)policy.limits.scalar_work=1;else if(cap==1)policy.limits.integer_bits=0;else if(cap==2)policy.limits.rational_slots=1;else policy.limits.records=1;
        rejects<std::exception>([&]{plan.witness(input,available,runtime::ExecutionLease(policy));},"full action cap bypassed");
        rejects<std::exception>([&]{Plan::prepare(c,request,available,runtime::ExecutionLease(policy));},"preparation cap bypassed");}
    auto cancelled=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{plan.homotopy(input,available,runtime::ExecutionLease({},cancelled));},"cancelled action accepted");
    const auto numeric=plan.numeric_plan();const auto carrier=numeric.carrier();auto declaration=carrier.declaration();auto* alias=&declaration.cells.front().to;bool changed=false;
    Guard copied({},[&]{if(!changed){changed=true;*alias=9999;}});
    const auto frozen=topology::OwnedCylinderCarrier::prepare(c.numeric_plan(),carrier.carrier(),std::move(declaration),copied);
    check(frozen.declaration().cells.front().to==carrier.declaration().cells.front().to,"moved carrier map alias changed captured declaration");
    auto maps=numeric.declaration();auto* map_alias=&maps.cylinder_target.cells[0][0];changed=false;
    Guard map_guard({},[&]{if(!changed){changed=true;*map_alias=9999;}});
    const auto frozen_map=topology::OwnedRelationalHomotopy::prepare(carrier,numeric.target(),std::move(maps),map_guard);
    check(frozen_map.declaration().cylinder_target.cells[0][0]==0,"moved homotopy map alias changed captured map");
    Guard source_guard;const auto chain=c.numeric_plan().chain(grade(1),{{0,Q{1}}},source_guard);auto replacement=numeric;changed=false;
    Guard replace({},[&]{if(!changed){changed=true;replacement=foreign.numeric_plan();}});
    const auto original=replacement.homotopy(chain,replace);check(original.plan().same_owner(numeric),"callback replacement changes in-flight homotopy owner");
    auto a=std::async(std::launch::async,[plan,input]{const auto out=plan.witness(input,available);return out.witness().residual().values();});
    auto b=std::async(std::launch::async,[plan,input]{return vals(plan.homotopy(input,available));});
    check(zero(a.get()) && b.get()==vals(plan.homotopy(input,available)),"concurrent actions share mutable scratch");
    const auto empty=prepare(4);const auto zero_input=empty.cylinder().chain(grade(0),{},available.cylinder);policy={};policy.limits.integer_bits=0;
    rejects<std::length_error>([&]{empty.homotopy(zero_input,available,runtime::ExecutionLease(policy));},"empty homotopy skips full-source caps");
    auto supplied=supply(c);auto d=supplied.declaration;d.types[3].carrier_type=0;Guard bad_guard;
    rejects([&]{topology::OwnedCylinderCarrier::prepare(c.numeric_plan(),supplied.source,d,bad_guard);},"wrong swept relation type accepted");
    auto extra=request;const auto target_with_unused=rebuild(request.target.endpoint()->source,{},true);
    storage::NativeAccessionDirectory unused_directory(storage::TemporalEndpoint(target_with_unused,19),1104);extra.target=native_cylinder_example::anchor(unused_directory);
    const auto unused=Plan::prepare(c,extra,available);policy={};policy.limits.integer_bits=8;
    const auto control=plan.homotopy(input,available,runtime::ExecutionLease(policy));check(!control.chain().values().empty(),"integer-cap control action failed");
    rejects<std::length_error>([&]{unused.homotopy(input,available,runtime::ExecutionLease(policy));},"unused target cancelling coefficients bypass current integer limits");
    const auto empty_input=c.chain(grade(0),{},available.cylinder);
    rejects<std::length_error>([&]{unused.homotopy(empty_input,available,runtime::ExecutionLease(policy));},"empty input bypasses unused target integer limits");
}
void temporal_cases() {
    const auto old=prepare(1);const auto c=old.cylinder();const auto input=c.chain(native_chain(c,grade(1),{{0,Q{1}}}),available.cylinder);const auto receipt=old.witness(input,available);
    const auto& req=old.declaration();
    storage::NativeAccessionDirectory a(*c.factor(false).endpoint(),1101),b(*c.factor(true).endpoint(),1102),cc(*req.carrier.endpoint(),1103),d(*req.target.endpoint(),1104);
    for(auto* directory:{&a,&b,&cc,&d}){const auto first=directory->pin_snapshot();auto next=*first;++next.sequence;directory->advance(storage::make_temporal_segment(*first,next));}
    auto cr=c.declaration();cr.left=native_cylinder_example::anchor(a);cr.right=native_cylinder_example::anchor(b);
    auto later=available;++later.cylinder.left.max_sequence;++later.cylinder.right.max_sequence;++later.carrier.max_sequence;++later.target.max_sequence;
    const auto new_c=Cylinder::prepare(cr,later.cylinder);auto r=req;r.carrier=native_cylinder_example::anchor(cc);r.target=native_cylinder_example::anchor(d);
    const auto newer=Plan::prepare(new_c,r,later);
    for(unsigned i=0;i<4;++i){auto wrong=later;if(i==0)wrong.cylinder.left=available.cylinder.left;else if(i==1)wrong.cylinder.right=available.cylinder.right;else if(i==2)wrong.carrier=available.carrier;else wrong.target=available.target;
        rejects([&]{Plan::prepare(new_c,r,wrong);},"new carrier leaks at one earlier cutoff");}
    rejects([&]{newer.homotopy(input,later);},"old input silently adopted into a later source publication");
    const auto later_input=new_c.chain(grade(1),{{0,Q{1}}},later.cylinder);
    rejects([&]{old.homotopy(later_input,available);},"later input leaks into an earlier homotopy endpoint");
    check(receipt.provenance().sequences==std::array<std::uint64_t,4>{11,13,17,19},"advancement rewrites retained result cutoffs");
    const auto retained=old.witness(input,available);check(retained.witness().homotopy().values()==receipt.witness().homotopy().values(),"advancement rewrites old retained target result");
    storage::NativeAccessionDirectory equal(*c.factor(false).endpoint(),1101);
    const auto descriptor=input.native_input()->descriptor();const auto impostor=native_cylinder_example::anchor(equal).access(descriptor.request());
    rejects([&]{c.chain(impostor,available.cylinder);},"equal cutoff identifies foreign execution authority");
}
template<class T>concept TemporaryDeclaration=requires(T t){std::move(t).declaration();};
template<class T>concept TemporaryValues=requires(T t){std::move(t).values();};
static_assert(!TemporaryDeclaration<Plan> && !TemporaryDeclaration<topology::OwnedCylinderCarrier> && !TemporaryDeclaration<topology::OwnedRelationalHomotopy>);
static_assert(!TemporaryValues<topology::OwnedHomotopyChain>);
Plan coalescing() {
    const auto c=cylinder(source(3)),small=cylinder(source(5));auto r=request(c);const auto target=supply(small,9000);Guard guard;
    const auto source_carrier=topology::OwnedCylinderCarrier::prepare(c.numeric_plan(),r.carrier.endpoint()->source,supply(c).declaration,guard);
    const auto target_carrier=topology::OwnedCylinderCarrier::prepare(small.numeric_plan(),target.source,target.declaration,guard);
    auto h=identity(source_carrier.carrier());
    for(const auto& cell:source_carrier.declaration().cells){auto id=cell.from;if(id.left.grade.value)id.left.index=0;h.cells[id.grade().value][cell.to]=target_carrier.image(id);}
    for(const auto& flag:source_carrier.declaration().occurrences){auto id=flag.from;if(id.upper.left.grade.value)id.upper.left.index=0;
        h.occurrences[id.upper.grade().value][flag.to]=target_carrier.image(id);}
    auto first=source_carrier.endpoint(Endpoint::Start),second=source_carrier.endpoint(Endpoint::Finish);
    for(auto* m:{&first,&second})for(std::size_t k=0;k<m->cells.size();++k){for(auto& i:m->cells[k])i=h.cells[k][i];for(auto& p:m->occurrences[k])p=h.occurrences[k][p];}
    storage::NativeAccessionDirectory d(storage::TemporalEndpoint(target.source,19),1104);r.target=native_cylinder_example::anchor(d);
    r.cylinder_target=source_map(source_carrier.carrier(),target.source,h,1601,73,74);
    r.first=source_map(c.factor(false).endpoint()->source,target.source,first,1602,c.declaration().left_factor,74);
    r.second=source_map(c.factor(false).endpoint()->source,target.source,second,1603,c.declaration().left_factor,74);
    const auto plan=Plan::prepare(c,r,available);const auto input=c.chain(grade(1),{{0,Q{1}},{1,Q{-1}}},available.cylinder);
    const auto result=plan.homotopy(input,available);check(result.chain().values().size()==1 && zero(result.chain().values()),"noninjective homotopy loses supported cancellation");
    return plan;
}
} // namespace
int main(int argc,char** argv) {
    try{const bool records=argc==2 && std::string_view(argv[1])=="--records";
        for(unsigned kind=0;kind<5;++kind)exercise(kind,records,prepare(kind));
        exercise(5,records,coalescing());
        contracts();temporal_cases();
        if(!records)std::cout<<"Checks: "<<checks<<"; witnesses: "<<witnesses<<"; product pushes: "<<pushes<<"; retained product flags: "<<flags<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
