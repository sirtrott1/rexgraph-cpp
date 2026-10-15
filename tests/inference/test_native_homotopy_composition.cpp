#include "../../examples/native_homotopy_composition/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_homotopy_composition_example;
std::size_t checks{}, witnesses{}, pushes{};
using Output = inference::DerivedNativeComposedHomotopyChain;
Values vals(Output out) { return out.chain().chain().values(); }
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
void source_record(const inference::PreparedNativeCylinder& cylinder, const topology::OwnedRelationalHomotopy& numeric,unsigned kind) {
    const auto carrier=numeric.carrier();
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
std::array<runtime::WorkAvailability*,8> roles(inference::NativeHomotopyCompositionAvailability& a) {
    return {&a.original.cylinder.left,&a.original.cylinder.right,&a.original.carrier,&a.original.target,
        &a.domain.left,&a.domain.right,&a.carrier,&a.target};
}
void exercise(unsigned kind,bool records) {
    const auto plan=prepare(kind);const auto domain=plan.domain(),original=plan.original().cylinder();
    const auto numeric=plan.numeric_plan();const auto composed=numeric.numeric_plan();const auto& source=domain.factor(false).endpoint()->source;
    if(records) {
        source_record(original,plan.original().numeric_plan(),kind*2);source_record(domain,composed,kind*2+1);
        std::cout<<"{\"kind\":\"composition\",\"id\":"<<kind*2+1<<",\"original\":"<<kind*2<<",\"before\":";json(numeric.declaration().before);
        std::cout<<",\"interval\":";json(numeric.declaration().interval);std::cout<<",\"after\":";json(numeric.declaration().after);
        std::cout<<",\"transport\":";json(numeric.carrier_map());std::cout<<"}\n";
    }
    for(std::uint16_t k=0;k<source.grade_count();++k) {
        std::vector<Values> cases{{}};Values all;
        for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i) {
            for(const auto& q:std::array<Q,3>{Q{0},Q{2,5},Q{-3,7}})cases.push_back({{i,q}});
            all.emplace(i,i%2?Q{-2,9}:Q{4,11});
        }
        if(!all.empty())cases.push_back(all);
        if((kind==6 || kind==7) && k==1)cases.push_back({{0,Q{1}},{1,Q{-1}}});
        for(const auto& v:cases) {
            const auto input=domain.chain(native_chain(domain,grade(k),v),available.domain);const auto result=plan.witness(input,available);const auto& w=result.witness().witness();++witnesses;
            check(zero(w.residual().values()),"composed identity failed");check(result.plan().same_owner(plan),"witness loses native composition owner");
            check(result.plan().original().same_owner(plan.original()),"witness loses original homotopy");
            check(result.input()->native_input()->same_execution_authority(domain.factor(false)),"witness loses native chain authority");
            check(result.provenance().sequences==std::array<std::uint64_t,8>{11,13,17,19,23,29,31,37},"eight cutoffs changed");
            const auto h=plan.homotopy(input,available);check(vals(h)==w.homotopy().values(),"standalone composed prism differs");
            const auto d=plan.boundary(h,available);check(vals(d)==w.boundary_homotopy().values(),"composed boundary differs");
            check(d.input()->plan().same_owner(plan) && d.chain().input()->plan().same_owner(numeric),"boundary loses original composition history");
            check(vals(plan.endpoint(input,Endpoint::Start,available))==w.first().values(),"composed start differs");
            check(vals(plan.endpoint(input,Endpoint::Finish,available))==w.second().values(),"composed finish differs");
            const auto product=domain.homotopy(input,available.domain);const auto pushed=plan.push(product,available);++pushes;
            check(vals(pushed)==w.homotopy().values(),"direct composed product push differs");
            check(pushed.product_input()->plan().same_owner(domain) && h.source_input()->plan().same_owner(domain),"native inputs lost");
            if((kind==6 || kind==7) && k==1 && v.size()==2 && v.at(0)==1)check(vals(h).size()==1 && zero(vals(h)),"cancellation loses support");
            if(records) {
                std::cout<<"{\"kind\":\"witness\",\"id\":"<<kind*2+1<<",\"grade\":"<<k<<",\"input\":";json(v);
                std::cout<<",\"source_boundary\":";const auto& cw=w.cylinder_witness();if(cw.source_boundary())json(cw.source_boundary()->values());else std::cout<<"null";
                std::cout<<",\"first\":";json(w.first().values());std::cout<<",\"second\":";json(w.second().values());std::cout<<",\"h\":";json(w.homotopy().values());
                std::cout<<",\"dh\":";json(w.boundary_homotopy().values());std::cout<<",\"hd\":";json(w.homotopy_boundary().values());
                std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";
            }
        }
    }
}
void contracts() {
    const auto s=setup(0);const auto plan=Plan::prepare(s.original,s.domain,s.request,available);const auto input=s.domain.chain(grade(1),{{0,Q{1}}},available.domain);
    const auto wrong=[&](auto change) {auto r=s.request;change(r);rejects([&]{Plan::prepare(s.original,s.domain,r,available);},"invalid composition accepted");};
    wrong([](auto& r){r.key=0;});wrong([](auto& r){r.revision=0;});wrong([](auto& r){r.before.cells.pop_back();});
    wrong([](auto& r){r.before.occurrences.pop_back();});wrong([](auto& r){r.before.occurrences.push_back(r.before.occurrences.front());});
    wrong([](auto& r){r.interval.cells.pop_back();});wrong([](auto& r){std::swap(r.interval.cells[0].to,r.interval.cells[1].to);});
    wrong([](auto& r){r.after.cells.pop_back();});wrong([](auto& r){r.after.occurrences.pop_back();});
    wrong([](auto& r){r.before.to=999;});wrong([](auto& r){r.interval.from=999;});wrong([](auto& r){r.after.from=999;});
    wrong([](auto& r){r.after.key=r.before.key;});wrong([](auto& r){r.realization.types.pop_back();});
    wrong([](auto& r){r.realization.cells.pop_back();});wrong([](auto& r){r.realization.occurrences.pop_back();});
    wrong([](auto& r){std::swap(r.realization.occurrences[0].to,r.realization.occurrences[1].to);});
    wrong([](auto& r){r.carrier_source=r.target_source;});wrong([](auto& r){r.before.occurrences[4].to=r.before.occurrences[5].to;});
    const auto other=prepare(0);const auto foreign=other.domain().chain(grade(1),{{0,Q{1}}},available.domain);
    rejects([&]{plan.homotopy(foreign,available);},"equal values identify distinct owners");
    rejects([&]{plan.push(other.domain().homotopy(foreign,available.domain),available);},"foreign product accepted");
    rejects([&]{plan.boundary(other.homotopy(foreign,available),available);},"foreign native output accepted");
    rejects([&]{plan.homotopy(s.original.cylinder().chain(grade(1),{{0,Q{1}}},available.original.cylinder),available);},"X chain substitutes for W chain");
    rejects([&]{plan.endpoint(input,static_cast<Endpoint>(9),available);},"unknown endpoint accepted");
    const auto z=s.domain.chain(grade(0),{{0,Q{1}}},available.domain);
    rejects([&]{plan.boundary(plan.endpoint(z,Endpoint::Start,available),available);},"negative boundary invented");
    const auto numeric=plan.numeric_plan();const auto target=numeric.numeric_plan();auto d=numeric.declaration();auto* alias=&d.before.cells[1][0];bool mutated=false;
    Guard capture({},[&]{if(!mutated){mutated=true;*alias=999;}});
    const auto captured=topology::OwnedHomotopyComposition::prepare(numeric.original(),numeric.domain(),target.target(),std::move(d),capture);
    check(mutated && captured.declaration().before.cells[1][0]==0,"moved alias changes admitted declaration");
    const auto numeric_domain=numeric.domain();auto cd=numeric_domain.declaration();const auto pc=numeric_domain.cylinder().product();
    const auto unused=std::find_if(pc.declaration().types.begin(),pc.declaration().types.end(),[](const auto& t){return t.left==std::optional<core::RelationTypeId>{1} && t.right==std::optional<core::RelationTypeId>{0};});
    for(auto& t:cd.types)if(t.product_type==unused->key)t.carrier_type=0;
    Guard tg;const auto recoded=topology::OwnedCylinderCarrier::prepare(numeric_domain.cylinder(),numeric_domain.carrier(),cd,tg);
    rejects([&]{Guard g;topology::OwnedHomotopyComposition::prepare(numeric.original(),recoded,target.target(),numeric.declaration(),g);},"unused interpretation changed");
    const auto swapped=prepare(8);const auto ns=plan.numeric_plan(),ss=swapped.numeric_plan();
    check(ns.carrier_map().cells==ss.carrier_map().cells && ns.carrier_map().occurrences!=ss.carrier_map().occurrences,"chain action erases zero occurrence permutation");
    const auto sv=swapped.domain().chain(grade(1),{{0,Q{1}}},available.domain);
    check(vals(swapped.homotopy(sv,available))==vals(plan.homotopy(input,available)),"valid zero-flag permutation changes numbers");
    const auto copy=plan;check(copy.same_owner(plan) && !other.same_owner(plan),"plan identity lost");
    auto future=std::async(std::launch::async,[copy,input]{return vals(copy.homotopy(input,available));});
    check(future.get()==vals(plan.homotopy(input,available)),"concurrent private scratch changes output");
    runtime::ExecutionHostPolicy policy;policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{plan.homotopy(input,available,runtime::ExecutionLease(policy));},"unregistered GPU action accepted");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=plan.homotopy(input,available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"CPU fallback unreported");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{plan.witness(input,available,runtime::ExecutionLease({},stop));},"cancelled composition executes");
    for(unsigned i=0;i<6;++i) {
        policy={};
        if(i==0)policy.limits.records=0;
        if(i==1)policy.limits.rational_slots=0;
        if(i==2)policy.limits.sparse_entries=0;
        if(i==3)policy.limits.scalar_work=0;
        if(i==4)policy.limits.integer_bits=0;
        if(i==5)policy.allow_allocation=false;
        rejects<std::exception>([&]{plan.witness(input,available,runtime::ExecutionLease(policy));},"action bypasses resource admission");
        rejects<std::exception>([&]{Plan::prepare(s.original,s.domain,s.request,available,runtime::ExecutionLease(policy));},"prepare bypasses admission");
    }
    auto with_unused=s.request;storage::NativeAccessionDirectory extra(storage::TemporalEndpoint(rebuild(with_unused.target.endpoint()->source,{},true),37),1204);
    with_unused.target=native_cylinder_example::anchor(extra);const auto retained=Plan::prepare(s.original,s.domain,with_unused,available);
    const auto empty=s.domain.chain(grade(1),{},available.domain);policy={};policy.limits.integer_bits=8;
    rejects<std::length_error>([&]{retained.homotopy(empty,available,runtime::ExecutionLease(policy));},"empty input bypasses unused coefficients");
    auto victim=numeric;bool replaced=false;Guard rg({},[&]{if(!replaced){replaced=true;victim=ss;}});
    const auto pinned=victim.homotopy(input.chain(),rg);
    check(replaced && pinned.plan().same_owner(numeric) && victim.same_owner(ss),"checkpoint replacement changes active plan");
    // Explicit identity composition can reuse actual owners. All eight roles
    // remain independently admitted even when their namespace values repeat.
    const auto old_c=s.original.cylinder();const auto& old_r=s.original.declaration();
    inference::NativeHomotopyCompositionRequest reused{1800,1,old_r.carrier_source,old_r.target_source,
        old_r.carrier,old_r.target,old_r.realization,
        source_map(old_c.factor(false).endpoint()->source,old_c.factor(false).endpoint()->source,identity(old_c.factor(false).endpoint()->source),1801,71,71),
        source_map(old_c.factor(true).endpoint()->source,old_c.factor(true).endpoint()->source,identity(old_c.factor(true).endpoint()->source),1802,72,72),
        source_map(old_r.target.endpoint()->source,old_r.target.endpoint()->source,identity(old_r.target.endpoint()->source),1803,74,74)};
    const inference::NativeHomotopyCompositionAvailability aa{available.original,available.original.cylinder,available.original.carrier,available.original.target};
    const auto identities=Plan::prepare(s.original,old_c,reused,aa);const auto xi=old_c.chain(grade(1),{{0,Q{1}}},aa.domain);
    const auto iw=identities.witness(xi,aa);const auto hw=s.original.witness(xi,aa.original);
    check(iw.witness().witness().homotopy().values()==hw.witness().homotopy().values(),"explicit identity composition changes action");
    check(identities.original().same_owner(s.original) && identities.domain().same_owner(old_c),"identity composition substitutes new owners");
    for(std::size_t i=0;i<8;++i){auto a=aa;--roles(a)[i]->max_sequence;rejects([&]{identities.witness(xi,a);},"aliased roles lose independent cutoff");}
    storage::NativeAccessionDirectory equal_carrier(*old_r.carrier.endpoint(),old_r.carrier.source_namespace());
    auto foreign_anchor=reused;foreign_anchor.carrier=native_cylinder_example::anchor(equal_carrier);
    rejects([&]{Plan::prepare(s.original,old_c,foreign_anchor,aa);},"one named source merges independent publication authorities");
    // A changed inactive/zero orientation is structural data, even at zero.
    auto bad_carrier=s.request;storage::NativeAccessionDirectory bad(storage::TemporalEndpoint(rebuild(bad_carrier.carrier.endpoint()->source,11),31),1203);
    bad_carrier.carrier=native_cylinder_example::anchor(bad);
    rejects([&]{Plan::prepare(s.original,s.domain,bad_carrier,available);},"inactive zero aspect silently recoded");
}
void temporal_cases() {
    const auto s=setup(1);const auto plan=Plan::prepare(s.original,s.domain,s.request,available);
    const auto input=s.domain.chain(native_chain(s.domain,grade(1),{{0,Q{1}}}),available.domain);const auto receipt=plan.witness(input,available);
    const auto product=s.domain.homotopy(input,available.domain);const auto output=plan.homotopy(input,available);
    for(std::size_t i=0;i<8;++i) {
        auto a=available;--roles(a)[i]->max_sequence;
        rejects([&]{Plan::prepare(s.original,s.domain,s.request,a);},"prepare drops cutoff");rejects([&]{plan.witness(input,a);},"witness drops cutoff");
        rejects([&]{plan.homotopy(input,a);},"prism drops cutoff");rejects([&]{plan.endpoint(input,Endpoint::Start,a);},"start drops cutoff");
        rejects([&]{plan.endpoint(input,Endpoint::Finish,a);},"finish drops cutoff");rejects([&]{plan.push(product,a);},"push drops cutoff");
        rejects([&]{plan.boundary(output,a);},"boundary drops cutoff");
        a=available;++roles(a)[i]->source_namespace;rejects([&]{plan.homotopy(input,a);},"foreign namespace accepted");
    }
    const auto pins=inference::native_homotopy_composition_detail::pins(s.original,s.domain,s.request);
    std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
    for(const auto& pin:pins) {
        auto dir=std::make_unique<storage::NativeAccessionDirectory>(*pin.endpoint(),pin.source_namespace());
        const auto old=dir->pin_snapshot();auto next=*old;++next.sequence;dir->advance(storage::make_temporal_segment(*old,next));dirs.push_back(std::move(dir));
    }
    auto a=available;for(auto* role:roles(a))++role->max_sequence;
    const auto old_cylinder=s.original.cylinder();auto oc=old_cylinder.declaration();oc.left=native_cylinder_example::anchor(*dirs[0]);oc.right=native_cylinder_example::anchor(*dirs[1]);
    const auto nc=Cylinder::prepare(oc,a.original.cylinder);auto oh=s.original.declaration();oh.carrier=native_cylinder_example::anchor(*dirs[2]);oh.target=native_cylinder_example::anchor(*dirs[3]);
    const auto nh=inference::PreparedNativeRelationalHomotopy::prepare(nc,oh,a.original);
    auto dc=s.domain.declaration();dc.left=native_cylinder_example::anchor(*dirs[4]);dc.right=native_cylinder_example::anchor(*dirs[5]);
    const auto nd=Cylinder::prepare(dc,a.domain);auto r=s.request;r.carrier=native_cylinder_example::anchor(*dirs[6]);r.target=native_cylinder_example::anchor(*dirs[7]);
    const auto newer=Plan::prepare(nh,nd,r,a);
    for(std::size_t i=0;i<8;++i){auto wrong=a;--roles(wrong)[i]->max_sequence;rejects([&]{Plan::prepare(nh,nd,r,wrong);},"later carrier leaks through old cutoff");}
    const auto ni=nd.chain(grade(1),{{0,Q{1}}},a.domain);
    rejects([&]{newer.homotopy(input,a);},"old input adopts later owner");rejects([&]{plan.homotopy(ni,available);},"later input leaks to earlier owner");
    check(receipt.provenance().sequences==std::array<std::uint64_t,8>{11,13,17,19,23,29,31,37},"advance rewrites original cutoffs");
    const auto old_result=plan.witness(input,available);check(old_result.witness().witness().homotopy().values()==receipt.witness().witness().homotopy().values(),"advance changes retained witness");
    storage::NativeAccessionDirectory impostor(*s.domain.factor(false).endpoint(),1201);
    const auto descriptor=input.native_input()->descriptor();const auto foreign=native_cylinder_example::anchor(impostor).access(descriptor.request());
    rejects([&]{s.domain.chain(foreign,available.domain);},"same namespace merges execution authority");
}
template<class T>concept TemporaryDeclaration=requires(T t){std::move(t).declaration();};
template<class T>concept TemporaryChain=requires(T t){std::move(t).chain();};
template<class T>concept TemporaryWitness=requires(T t){std::move(t).witness();};
static_assert(!TemporaryDeclaration<Plan> && !TemporaryDeclaration<topology::OwnedHomotopyComposition>);
static_assert(!TemporaryChain<Output> && !TemporaryChain<topology::ComposedHomotopyChain>);
static_assert(!TemporaryWitness<inference::NativeComposedHomotopyWitness> && !TemporaryWitness<topology::ComposedHomotopyWitness>);
} // namespace
int main(int argc,char** argv) {
    try {
        const bool records=argc==2 && std::string_view(argv[1])=="--records";
        for(unsigned kind=0;kind<9;++kind)exercise(kind,records);
        contracts();temporal_cases();
        if(!records)std::cout<<"Checks: "<<checks<<"; witnesses: "<<witnesses<<"; product pushes: "<<pushes<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
