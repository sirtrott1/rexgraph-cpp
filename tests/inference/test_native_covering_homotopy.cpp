#include "../../examples/native_covering_homotopy/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_covering_homotopy_example;
std::size_t checks{}, witnesses{}, pushes{};
using Output = inference::DerivedNativeLiftedHomotopyChain;
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
void record_witness(unsigned id,std::uint16_t k,const Values& v,const topology::RelationalHomotopyWitness& w) {
    std::cout<<"{\"kind\":\"witness\",\"id\":"<<id<<",\"grade\":"<<k<<",\"input\":";json(v);
    std::cout<<",\"source_boundary\":";const auto& cw=w.cylinder_witness();if(cw.source_boundary())json(cw.source_boundary()->values());else std::cout<<"null";
    std::cout<<",\"first\":";json(w.first().values());std::cout<<",\"second\":";json(w.second().values());std::cout<<",\"h\":";json(w.homotopy().values());
    std::cout<<",\"dh\":";json(w.boundary_homotopy().values());std::cout<<",\"hd\":";json(w.homotopy_boundary().values());
    std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";
}

std::vector<runtime::WorkAvailability*> roles(Availability& a) {
    return {&a.original.cylinder.left,&a.original.cylinder.right,&a.original.carrier,&a.original.target,&a.covering.cover,&a.covering.base};
}
void lift_record(const Plan& p,unsigned id) {
    const auto n=p.numeric_plan();const auto h=n.original(),lifted=n.numeric_plan();const auto cover=n.covering();
    std::cout<<"{\"kind\":\"lift\",\"id\":"<<id<<",\"base\":";json(cover.source(local::CoveringChannel::Base));
    std::cout<<",\"original_target\":";json(h.target());std::cout<<",\"original_h\":";json(h.declaration().cylinder_target);
    std::cout<<",\"original_first\":";json(h.declaration().first);std::cout<<",\"original_second\":";json(h.declaration().second);
    std::cout<<",\"projection\":";json(cover.projection());std::cout<<",\"base_map\":";json(n.declaration().base_map);
    std::cout<<",\"initial\":";json(n.declaration().initial);std::cout<<",\"projected_h\":";json(n.certificate().projected_homotopy);
    std::cout<<",\"projected_first\":";json(n.certificate().projected_first);std::cout<<",\"projected_second\":";json(n.certificate().projected_second);
    std::cout<<",\"seeds\":"<<n.certificate().seeds<<",\"routes\":[";
    const auto& routes=n.certificate().routes;
    for(std::size_t i=0;i<routes.size();++i) { if(i)std::cout<<',';const auto& r=routes[i];
        std::cout<<'['<<r.from.grade.value<<','<<r.from.index<<','<<r.upper_grade.value<<','<<r.occurrence<<','<<r.lifted_occurrence<<','<<r.upward<<']';}
    std::cout<<"]}\n";
}
void exercise(unsigned kind,std::size_t sheet,bool records) {
    const auto f=fixture(kind,sheet);const auto p=Plan::prepare(f.original,f.cover,f.request,f.available);
    const auto a=p.preparation_availability();const auto c=p.domain();const auto n=p.numeric_plan();const auto h=n.numeric_plan();
    const auto id=kind*2+static_cast<unsigned>(sheet);const auto& source=c.factor(false).endpoint()->source;
    if(records) { source_record(c,h,id);lift_record(p,id); }
    check(p.original().same_owner(f.original) && p.covering().same_owner(f.cover),"actual original owners lost");
    check(h.declaration().first.cells==n.declaration().initial.cells && h.declaration().first.occurrences==n.declaration().initial.occurrences,"initial full map changed");
    std::size_t seed_count{};
    for(std::uint16_t k=0;k<source.grade_count();++k)seed_count+=source.grade(grade(k)).cell_count();
    check(n.certificate().seeds==seed_count,"seed inventory changed");
    for(std::uint16_t k=0;k<source.grade_count();++k) {
        std::vector<Values> cases{{}};Values all;
        for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i) {
            for(const auto& q:std::array<Q,3>{Q{0},Q{2,5},Q{-3,7}})cases.push_back({{i,q}});
            all.emplace(i,i%2?Q{-2,9}:Q{4,11});
        }
        if(!all.empty())cases.push_back(all);
        if(kind==7 && k==1)cases.push_back({{0,Q{1}},{1,Q{-1}}});
        for(const auto& v:cases) {
            const auto x=c.chain(native_chain(c,grade(k),v),a.original.cylinder);const auto receipt=p.witness(x,a);const auto& w=receipt.witness().witness();++witnesses;
            check(zero(w.residual().values()),"lifted signed-prism identity failed");check(receipt.plan().same_owner(p),"witness loses its owner");
            check(receipt.input()->native_input()->same_execution_authority(c.factor(false)),"original input authority lost");
            check(receipt.provenance().sequences==std::array<std::uint64_t,6>{11,13,17,19,23,29},"publication roles lost");
            const auto out=p.homotopy(x,a);check(vals(out)==w.homotopy().values(),"prism differs");
            const auto d=p.boundary(out,a);check(vals(d)==w.boundary_homotopy().values(),"boundary differs");
            check(d.input()->plan().same_owner(p) && d.chain().input()->plan().same_owner(n),"boundary predecessor lost");
            check(vals(p.endpoint(x,Endpoint::Start,a))==w.first().values(),"start differs");
            check(vals(p.endpoint(x,Endpoint::Finish,a))==w.second().values(),"finish differs");
            const auto product=c.homotopy(x,a.original.cylinder);const auto push=p.push(product,a);++pushes;
            check(vals(push)==w.homotopy().values() && push.product_input()->plan().same_owner(c),"product action or owner differs");
            if(kind==7 && k==1 && v.size()==2 && v.at(0)==1)check(vals(out).size()==1 && zero(vals(out)),"noninjective cancellation loses support");
            if(records)record_witness(id,k,v,w);
        }
    }
    if(kind==6) {
        const auto first=p.endpoint(c.chain(grade(0),{{0,Q{1}}},a.original.cylinder),Endpoint::Start,a);
        const auto second=p.endpoint(c.chain(grade(0),{{0,Q{1}}},a.original.cylinder),Endpoint::Finish,a);
        check(vals(first).contains(static_cast<std::uint32_t>(2+3*sheet)) && vals(second).contains(static_cast<std::uint32_t>((3+3*sheet)%6)),"C6 lift loses sheet change");
    }
}
void obstructions(bool records) {
    const auto reference=fixture();const auto numeric=reference.original.numeric_plan();const auto carrier=numeric.carrier();
    const auto& c=carrier.carrier();std::vector<std::pair<core::Grade,std::size_t>> choices;
    for(std::uint16_t k=2;k<c.grade_count();++k)for(std::size_t p=0;p<c.grade(grade(k)).incidence_count();++p)
        if(!c.incidence_coefficient(grade(k),p))choices.emplace_back(grade(k),p);
    check(!choices.empty(),"no structural obstruction fixture");
    for(const auto twist:choices) {
        const auto f=fixture(0,0,twist); // The covering and initial morphism are valid.
        rejects([&]{Plan::prepare(f.original,f.cover,f.request,f.available);},"incidence obstruction hidden by realized zero");
        if(records) {
            const auto h=f.original.numeric_plan();const auto cover=f.cover.numeric_plan();Guard guard;
            const auto init=inference::native_gluing_detail::indexed(f.original.cylinder().factor(false).endpoint()->source,cover.source(),f.request.initial.cells,f.request.initial.occurrences,guard);
            std::cout<<"{\"kind\":\"obstruction\",\"source\":";json(f.original.cylinder().factor(false).endpoint()->source);
            std::cout<<",\"carrier\":";json(carrier.carrier());std::cout<<",\"cover\":";json(cover.source());std::cout<<",\"base\":";json(cover.source(local::CoveringChannel::Base));
            std::cout<<",\"projection\":";json(cover.projection());std::cout<<",\"h\":";json(h.declaration().cylinder_target);
            std::cout<<",\"initial\":";json(init);std::cout<<",\"start\":";json(carrier.endpoint(Endpoint::Start));std::cout<<"}\n";
        }
    }
}
void contracts() {
    const auto f=fixture();const auto p=Plan::prepare(f.original,f.cover,f.request,f.available);const auto a=f.available;const auto c=p.domain();
    const auto x=c.chain(grade(1),{{0,Q{1}}},a.original.cylinder);
    const auto bad=[&](auto change){auto r=f.request;change(r);rejects([&]{Plan::prepare(f.original,f.cover,r,a);},"invalid lifting request accepted");};
    bad([](auto& r){r.key=0;});bad([](auto& r){r.revision=0;});bad([](auto& r){r.base_map.key=0;});bad([](auto& r){r.initial.key=r.base_map.key;});
    bad([](auto& r){r.base_map.key=1601;});bad([](auto& r){r.initial.key=1701;});
    bad([](auto& r){r.base_map.from=73;});bad([](auto& r){r.initial.to=82;});
    bad([](auto& r){r.base_map.cells.pop_back();});bad([](auto& r){r.base_map.occurrences.pop_back();});
    bad([](auto& r){r.initial.cells.pop_back();});bad([](auto& r){r.initial.occurrences.pop_back();});
    bad([](auto& r){r.initial.cells.push_back(r.initial.cells.front());});bad([](auto& r){r.base_map.occurrences.push_back(r.base_map.occurrences.front());});
    bad([](auto& r){std::swap(r.initial.occurrences[0].to,r.initial.occurrences[1].to);});
    bad([](auto& r){std::swap(r.initial.occurrences[4].to,r.initial.occurrences[5].to);}); // Equal supported zeros retain flag identity.
    auto cr=f.cover.declaration();cr.projection.to=74;
    const auto alias=inference::PreparedNativeCovering::prepare(cr,a.covering);auto rr=f.request;rr.base_map.to=74;
    rejects([&]{Plan::prepare(f.original,alias,rr,a);},"equal source bytes merge distinct native base owners");
    // An actual aliased base role is permitted but still has its own cutoff.
    cr.base=f.original.declaration().target;const auto actual=inference::PreparedNativeCovering::prepare(cr,{a.covering.cover,a.original.target});
    auto aa=a;aa.covering.base=a.original.target;const auto allowed=Plan::prepare(f.original,actual,rr,aa);
    auto short_base=aa;--short_base.covering.base.max_sequence;
    rejects([&]{allowed.homotopy(x,short_base);},"alias merges independent availability roles");
    const auto other=prepare();const auto oa=other.preparation_availability();const auto oc=other.domain();const auto foreign=oc.chain(grade(1),{{0,Q{1}}},oa.original.cylinder);
    rejects([&]{p.homotopy(foreign,a);},"equal input from another cylinder accepted");
    rejects([&]{p.push(oc.homotopy(foreign,oa.original.cylinder),a);},"foreign product accepted");
    rejects([&]{p.boundary(other.homotopy(foreign,oa),a);},"foreign output accepted");
    rejects([&]{p.endpoint(x,static_cast<Endpoint>(9),a);},"invalid endpoint accepted");
    rejects([&]{p.boundary(p.endpoint(c.chain(grade(0),{},a.original.cylinder),Endpoint::Start,a),a);},"negative boundary grade invented");
    const auto n=p.numeric_plan();auto d=n.declaration();d.key=1900;auto* alias_map=&d.initial.cells[1][0];bool changed=false;
    Guard callback({},[&]{if(!changed){changed=true;*alias_map=999;}});
    const auto copied=topology::OwnedCoveringHomotopy::prepare(n.original(),n.covering(),std::move(d),callback);
    check(changed && copied.declaration().initial.cells[1][0]==n.declaration().initial.cells[1][0],"mutable alias changes captured map");
    auto victim=n;const auto on=other.numeric_plan();bool replaced=false;Guard replacement({},[&]{if(!replaced){replaced=true;victim=on;}});
    const auto retained=victim.homotopy(x.chain(),replacement);check(replaced && retained.plan().same_owner(n),"callback replacement changes active plan");
    auto job=std::async(std::launch::async,[p,x,a]{return vals(p.homotopy(x,a));});check(job.get()==vals(p.homotopy(x,a)),"concurrent actions corrupt scratch");
    runtime::ExecutionHostPolicy policy;policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{p.witness(x,a,runtime::ExecutionLease(policy));},"unsupported GPU accepted");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;const auto fallback=p.witness(x,a,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"fallback unreported");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(x,a,runtime::ExecutionLease({},stop));},"cancelled action runs");
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare(f.original,f.cover,f.request,a,runtime::ExecutionLease({},stop));},"cancelled preparation runs");
    for(unsigned i=0;i<6;++i) {
        policy={};if(i==0)policy.limits.records=0;if(i==1)policy.limits.rational_slots=0;if(i==2)policy.limits.sparse_entries=0;
        if(i==3)policy.limits.scalar_work=0;
        if(i==4)policy.limits.integer_bits=0;
        if(i==5)policy.allow_allocation=false;
        rejects<std::exception>([&]{p.witness(c.chain(grade(1),{},a.original.cylinder),a,runtime::ExecutionLease(policy));},"empty action skips resource admission");
        rejects<std::exception>([&]{Plan::prepare(f.original,f.cover,f.request,a,runtime::ExecutionLease(policy));},"preparation skips resource admission");
    }
}
struct Rebound { inference::PreparedNativeRelationalHomotopy original;inference::PreparedNativeCovering cover; };
Rebound rebound(const Fixture& f,const std::vector<std::unique_ptr<storage::NativeAccessionDirectory>>& dirs,Availability a) {
    const auto old=f.original.cylinder();auto d=old.declaration();d.left=native_cylinder_example::anchor(*dirs[0]);d.right=native_cylinder_example::anchor(*dirs[1]);
    const auto c=inference::PreparedNativeCylinder::prepare(d,a.original.cylinder);auto h=f.original.declaration();h.carrier=native_cylinder_example::anchor(*dirs[2]);h.target=native_cylinder_example::anchor(*dirs[3]);
    auto cover=f.cover.declaration();cover.cover=native_cylinder_example::anchor(*dirs[4]);cover.base=native_cylinder_example::anchor(*dirs[5]);
    return {inference::PreparedNativeRelationalHomotopy::prepare(c,h,a.original),inference::PreparedNativeCovering::prepare(cover,a.covering)};
}
void temporal_cases() {
    const auto f=fixture(1);const auto p=Plan::prepare(f.original,f.cover,f.request,f.available);const auto a=f.available;const auto c=p.domain();
    const auto x=c.chain(native_chain(c,grade(1),{{0,Q{1}}}),a.original.cylinder);const auto w=p.witness(x,a);
    const auto product=c.homotopy(x,a.original.cylinder);const auto out=p.homotopy(x,a);
    for(std::size_t i=0;i<6;++i) {
        auto bad=a;--roles(bad)[i]->max_sequence;
        rejects([&]{p.witness(x,bad);},"witness omits temporal cutoff");rejects([&]{p.homotopy(x,bad);},"prism omits temporal cutoff");
        rejects([&]{p.endpoint(x,Endpoint::Start,bad);},"start omits cutoff");rejects([&]{p.endpoint(x,Endpoint::Finish,bad);},"finish omits cutoff");
        rejects([&]{p.push(product,bad);},"push omits cutoff");rejects([&]{p.boundary(out,bad);},"boundary omits cutoff");
        rejects([&]{Plan::prepare(f.original,f.cover,f.request,bad);},"preparation omits cutoff");
        bad=a;++roles(bad)[i]->source_namespace;rejects([&]{p.witness(x,bad);},"foreign namespace accepted");
    }
    std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
    for(const auto& pin:inference::native_covering_homotopy_detail::pins(f.original,f.cover)) {
        auto dir=std::make_unique<storage::NativeAccessionDirectory>(*pin.endpoint(),pin.source_namespace());const auto old=dir->pin_snapshot();auto next=*old;++next.sequence;
        dir->advance(storage::make_temporal_segment(*old,next));dirs.push_back(std::move(dir));
    }
    auto later=a;for(auto* role:roles(later))++role->max_sequence;const auto now=rebound(f,dirs,later);
    for(std::size_t i=0;i<6;++i) {auto bad=later;--roles(bad)[i]->max_sequence;rejects([&]{Plan::prepare(now.original,now.cover,f.request,bad);},"future publication admitted at old cutoff");}
    const auto new_plan=Plan::prepare(now.original,now.cover,f.request,later);const auto newer=new_plan.domain();
    const auto nx=newer.chain(grade(1),{{0,Q{1}}},later.original.cylinder);
    rejects([&]{new_plan.homotopy(x,later);},"old input adopts future owner");rejects([&]{p.homotopy(nx,a);},"future input leaks to old owner");
    const auto unchanged=p.witness(x,a);
    check(unchanged.witness().witness().homotopy().values()==w.witness().witness().homotopy().values(),"advancing directories changes retained result");
    const auto fresh=new_plan.witness(nx,later);
    for(std::size_t i=0;i<6;++i)check(fresh.provenance().sequences[i]==w.provenance().sequences[i]+1,"publication sequence substituted");
}
void native_limits() {
    const auto f=fixture(1);const auto p=Plan::prepare(f.original,f.cover,f.request,f.available);const auto a=f.available;const auto c=p.domain();
    const auto x=c.chain(grade(1),{{0,Q{253,251}}},a.original.cylinder);const auto w=p.witness(x,a);
    runtime::ExecutionHostPolicy short_work;short_work.limits.scalar_work=w.provenance().execution_steps-1;
    rejects<std::length_error>([&]{p.witness(x,a,runtime::ExecutionLease(short_work));},"observed scalar-work bound ignored");
    const auto pins=inference::native_covering_homotopy_detail::pins(f.original,f.cover);
    for(unsigned mode=0;mode<2;++mode)for(std::size_t role=0;role<pins.size();++role) {
        std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
        for(std::size_t i=0;i<pins.size();++i) {
            storage::NativeAccessionLimits lim;if(i==role){if(mode==0)lim.max_action_work=3999999-role;else lim.max_integer_bytes=1;}
            dirs.push_back(std::make_unique<storage::NativeAccessionDirectory>(*pins[i].endpoint(),pins[i].source_namespace(),
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},lim));
        }
        const auto owners=rebound(f,dirs,a);const auto limited=Plan::prepare(owners.original,owners.cover,f.request,a);const auto dc=limited.domain();
        const auto input=dc.chain(grade(1),{{0,Q{253,251}}},a.original.cylinder);
        if(mode==1)rejects<std::length_error>([&]{limited.witness(input,a);},"one native role integer cap ignored");
        else {
            const auto observed=limited.witness(input,a);
            check(observed.provenance().limits.steps==3999999-role,"one native role scalar-work cap ignored");
            const auto empty=limited.homotopy(dc.chain(grade(1),{},a.original.cylinder),a);
            check(empty.provenance().limits.steps==3999999-role,"empty action loses native work cap");
        }
    }
}
template<class T>concept TemporaryDeclaration=requires(T t){std::move(t).declaration();};
template<class T>concept TemporaryChain=requires(T t){std::move(t).chain();};
template<class T>concept TemporaryWitness=requires(T t){std::move(t).witness();};
template<class T>concept TemporaryCertificate=requires(T t){std::move(t).certificate();};
static_assert(!TemporaryDeclaration<Plan> && !TemporaryDeclaration<topology::OwnedCoveringHomotopy>);
static_assert(!TemporaryChain<Output> && !TemporaryChain<topology::LiftedHomotopyChain>);
static_assert(!TemporaryWitness<inference::NativeCoveringHomotopyWitness> && !TemporaryWitness<topology::CoveringHomotopyWitness> && !TemporaryCertificate<topology::OwnedCoveringHomotopy>);
} // namespace
int main(int argc,char** argv) {
    try {
        const bool records=argc==2 && std::string_view(argv[1])=="--records";
        for(unsigned kind=0;kind<10;++kind)for(std::size_t sheet=0;sheet<2;++sheet)exercise(kind,sheet,records);
        obstructions(records);
        if(!records){contracts();temporal_cases();native_limits();std::cout<<"checks="<<checks<<" witnesses="<<witnesses<<" pushes="<<pushes<<'\n';}
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
