#include "../../examples/native_covering_homotopy_base_change/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_covering_base_change_example;
using Fixture = native_covering_base_change_example::Fixture;
std::size_t checks{},witnesses{};
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
void source_record(const inference::PreparedNativeCylinder& cylinder, const topology::OwnedRelationalHomotopy& numeric,unsigned kind,std::string_view label="source") {
    const auto carrier=numeric.carrier();
    std::cout<<"{\"kind\":\""<<label<<"\",\"id\":"<<kind<<",\"source\":";json(cylinder.factor(false).endpoint()->source);
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
void record_witness(unsigned id,std::uint16_t k,const Values& v,const topology::RelationalHomotopyWitness& w,std::string_view label="witness") {
    std::cout<<"{\"kind\":\""<<label<<"\",\"id\":"<<id<<",\"grade\":"<<k<<",\"input\":";json(v);
    std::cout<<",\"source_boundary\":";const auto& cw=w.cylinder_witness();if(cw.source_boundary())json(cw.source_boundary()->values());else std::cout<<"null";
    std::cout<<",\"first\":";json(w.first().values());std::cout<<",\"second\":";json(w.second().values());std::cout<<",\"h\":";json(w.homotopy().values());
    std::cout<<",\"dh\":";json(w.boundary_homotopy().values());std::cout<<",\"hd\":";json(w.homotopy_boundary().values());
    std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";
}

std::vector<runtime::WorkAvailability*> roles(BaseAvailability& a) {
    return {&a.original.original.cylinder.left,&a.original.original.cylinder.right,&a.original.original.carrier,&a.original.original.target,
        &a.original.covering.cover,&a.original.covering.base,&a.changed.original.cylinder.left,&a.changed.original.cylinder.right,
        &a.changed.original.carrier,&a.changed.original.target,&a.changed.covering.cover,&a.changed.covering.base,
        &a.pullback.original.cover,&a.pullback.original.base,&a.pullback.pulled.cover,&a.pullback.pulled.base};
}
void maps_record(const Fixture& f,unsigned id) {
    const auto a=f.original.numeric_plan(),b=f.changed.numeric_plan();const auto h=a.original(),hp=b.original();
    const auto e=a.covering(),p=b.covering();const auto sq=f.pullback.numeric_plan();Guard guard;
    const auto w=inference::native_gluing_detail::indexed(h.target(),hp.target(),f.request.comparison.cells,f.request.comparison.occurrences,guard);
    std::cout<<"{\"kind\":\"square\",\"id\":"<<id<<",\"y\":";json(h.target());std::cout<<",\"z\":";json(hp.target());
    std::cout<<",\"base\":";json(e.source(local::CoveringChannel::Base));std::cout<<",\"new_base\":";json(p.source(local::CoveringChannel::Base));
    std::cout<<",\"projection\":";json(e.projection());std::cout<<",\"new_projection\":";json(p.projection());
    std::cout<<",\"into\":";json(sq.map());std::cout<<",\"base_map\":";json(sq.map(true));
    std::cout<<",\"comparison\":";json(w);std::cout<<",\"v\":";json(a.declaration().base_map);std::cout<<",\"g\":";json(b.declaration().base_map);
    std::cout<<",\"h\":";json(h.declaration().cylinder_target);std::cout<<",\"hp\":";json(hp.declaration().cylinder_target);
    std::cout<<"}\n";
}
void exercise(unsigned kind,std::size_t sheet,unsigned mode,bool records) {
    const auto f=native_covering_base_change_example::fixture(kind,sheet,mode);
    const auto p=BaseChange::prepare(f.original,f.changed,f.pullback,f.request,f.available);
    const auto a=f.available;const auto c=p.domain();const auto n=p.numeric_plan();const auto old=n.original(),changed=n.changed();
    const auto oh=old.numeric_plan(),nh=changed.numeric_plan();const auto id=kind*6+static_cast<unsigned>(sheet)*3+mode;
    if(records) {source_record(c,oh,id);source_record(c,nh,id,"changed_source");maps_record(f,id);}
    check(p.original().same_owner(f.original)&&p.changed().same_owner(f.changed),"actual lift owners lost");
    check(p.pullback().original().same_owner(f.original.covering())&&p.pullback().pulled().same_owner(f.changed.covering()),"actual square owners lost");
    const std::array<const Map*,3> maps{&nh.declaration().cylinder_target,&nh.declaration().first,&nh.declaration().second};
    for(std::size_t j=0;j<3;++j) {
        const auto& m=n.certificate().mediators[j];check(m.map().cells==maps[j]->cells&&m.map().occurrences==maps[j]->occurrences,"mediator changed");
        const auto original_h=f.original.original();
        const auto& cone=p.cones()[j];check(cone.request.source.endpoint()==(j?c.factor(false):original_h.declaration().carrier).endpoint(),"cone source publication lost");
        Guard guard;const auto sq=p.pullback().numeric_plan();const auto op=sq.original(),np=sq.pulled();
        const auto ce=inference::native_gluing_detail::indexed(m.source(),op.source(),cone.request.into_cover.cells,cone.request.into_cover.occurrences,guard);
        const auto cd=inference::native_gluing_detail::indexed(m.source(),np.source(local::CoveringChannel::Base),cone.request.into_base.cells,cone.request.into_base.occurrences,guard);
        check(ce.cells==m.cone_map().cells&&ce.occurrences==m.cone_map().occurrences&&cd.cells==m.cone_map(true).cells&&cd.occurrences==m.cone_map(true).occurrences,"stable cone differs");
    }
    const auto& source=c.factor(false).endpoint()->source;
    for(std::uint16_t k=0;k<source.grade_count();++k) {
        std::vector<Values> cases{{}};Values all;
        for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i) {
            for(const auto& q:std::array<Q,3>{Q{0},Q{2,5},Q{-3,7}})cases.push_back({{i,q}});
            all.emplace(i,i%2?Q{-2,9}:Q{4,11});
        }
        if(!all.empty())cases.push_back(all);
        if(kind==7&&k==1)cases.push_back({{0,Q{1}},{1,Q{-1}}});
        for(const auto& v:cases) {
            const auto x=c.chain(native_chain(c,grade(k),v),a.original.original.cylinder);const auto result=p.witness(x,a);++witnesses;
            const auto& w=result.witness();
            check(result.plan().same_owner(p)&&w.plan().same_owner(n),"witness owner lost");
            check(w.original().plan().same_owner(old)&&w.changed().plan().same_owner(changed),"lift witness owner lost");
            check(result.input()->native_input()->same_execution_authority(c.factor(false)),"native input lost");
            check(zero(w.original().witness().residual().values())&&zero(w.changed().witness().residual().values()),"signed-prism identity failed");
            for(const auto& r:w.residuals())check(zero(r),"projection naturality failed");
            const auto& seq=result.provenance().sequences;
            check(seq.size()==16&&seq[4]==23&&seq[5]==29&&seq[10]==31&&seq[11]==37&&seq[12]==23&&seq[15]==37,"cutoff roles lost");
            if(records) {
                record_witness(id,k,v,w.original().witness());
                record_witness(id,k,v,w.changed().witness(),"changed_witness");
                std::cout<<"{\"kind\":\"projection\",\"id\":"<<id<<",\"values\":[";
                for(std::size_t j=0;j<7;++j){if(j)std::cout<<',';json(w.projected()[j]);}
                std::cout<<"],\"residuals\":[";
                for(std::size_t j=0;j<7;++j){if(j)std::cout<<',';json(w.residuals()[j]);}std::cout<<"]}\n";
            }
        }
    }
}
BaseChange certify(const Fixture& f,BaseAvailability a) { return BaseChange::prepare(f.original,f.changed,f.pullback,f.request,a); }
void contracts() {
    const auto f=native_covering_base_change_example::fixture(0,0,1);const auto p=certify(f,f.available);const auto a=f.available;const auto c=p.domain();
    const auto x=c.chain(grade(1),{{0,Q{1}}},a.original.original.cylinder);
    const auto bad=[&](auto change){auto r=f.request;change(r);rejects([&]{BaseChange::prepare(f.original,f.changed,f.pullback,r,a);},"invalid declaration accepted");};
    bad([](auto& r){r.key=0;});bad([](auto& r){r.revision=0;});bad([](auto& r){r.comparison.key=0;});
    bad([](auto& r){r.cone_maps[0]=0;});bad([](auto& r){r.cone_maps[1]=r.cone_maps[0];});
    bad([](auto& r){r.comparison.key=1601;});bad([](auto& r){r.cone_maps[2]=1951;});
    bad([](auto& r){r.comparison.from=73;});bad([](auto& r){r.comparison.to=81;});
    bad([](auto& r){r.comparison.cells.pop_back();});bad([](auto& r){r.comparison.occurrences.pop_back();});
    bad([](auto& r){r.comparison.cells.push_back(r.comparison.cells.front());});
    // Supported-zero flag permutation is strict and numerically invisible,
    // but differs from the full comparison of these actual homotopies.
    auto r=f.request;std::swap(r.comparison.occurrences[4].to,r.comparison.occurrences[5].to);
    rejects([&]{BaseChange::prepare(f.original,f.changed,f.pullback,r,a);},"equal zero flags identify full maps");
    const auto wrong=native_covering_base_change_example::fixture(6,0,1,true);
    rejects([&]{certify(wrong,wrong.available);},"wrong initial sheet accepted");
    const auto original_cover=f.original.covering();const auto cr=original_cover.declaration();const auto replacement=inference::PreparedNativeCovering::prepare(cr,a.original.covering);
    const auto sq=f.pullback.declaration();const auto foreign=inference::PreparedNativeCoveringPullback::prepare(replacement,f.changed.covering(),sq,a.pullback);
    rejects([&]{BaseChange::prepare(f.original,f.changed,foreign,f.request,a);},"equal cover accepted as actual owner");
    const auto other=native_covering_base_change_example::prepare(0,0,1);const auto oc=other.domain();
    const auto ox=oc.chain(grade(1),{{0,Q{1}}},other.preparation_availability().original.original.cylinder);
    rejects([&]{p.witness(ox,a);},"foreign common cylinder input accepted");
    const auto numeric=p.numeric_plan();auto d=numeric.declaration();auto* alias=&d.comparison.cells[0][0];bool mutated=false;
    Guard callback({},[&]{if(!mutated){mutated=true;*alias=999;}});
    const auto copied=topology::OwnedCoveringHomotopyBaseChange::prepare(numeric.original(),numeric.changed(),numeric.pullback(),std::move(d),callback);
    check(mutated&&copied.declaration().comparison.cells==numeric.declaration().comparison.cells,"mutable alias changed captured comparison");
    auto victim=numeric;const auto other_numeric=other.numeric_plan();bool replaced=false;
    Guard replacement_guard({},[&]{if(!replaced){replaced=true;victim=other_numeric;}});
    const auto kept=victim.witness(x.chain(),replacement_guard);check(replaced&&kept.plan().same_owner(numeric),"callback replaces active certificate");
    auto task=std::async(std::launch::async,[p,x,a]{const auto w=p.witness(x,a);return w.witness().residuals();});
    const auto serial=p.witness(x,a);check(task.get()==serial.witness().residuals(),"concurrent scratch changed");
    runtime::ExecutionHostPolicy policy;policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{p.witness(x,a,runtime::ExecutionLease(policy));},"unsupported GPU runs");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(x,a,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"fallback not reported");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(x,a,runtime::ExecutionLease({},stop));},"cancelled witness runs");
    rejects<runtime::ExecutionCancelled>([&]{BaseChange::prepare(f.original,f.changed,f.pullback,f.request,a,runtime::ExecutionLease({},stop));},"cancelled preparation runs");
    for(unsigned mode=0;mode<6;++mode) {
        policy={};if(mode==0)policy.limits.records=0;if(mode==1)policy.limits.rational_slots=0;if(mode==2)policy.limits.sparse_entries=0;
        if(mode==3)policy.limits.scalar_work=0;
        if(mode==4)policy.limits.integer_bits=0;
        if(mode==5)policy.allow_allocation=false;
        rejects<std::exception>([&]{p.witness(c.chain(grade(1),{},a.original.original.cylinder),a,runtime::ExecutionLease(policy));},"empty witness skips admission");
        rejects<std::exception>([&]{BaseChange::prepare(f.original,f.changed,f.pullback,f.request,a,runtime::ExecutionLease(policy));},"preparation skips admission");
    }
    const auto observed=p.witness(x,a);policy={};policy.limits.scalar_work=observed.provenance().execution_steps-1;
    rejects<std::length_error>([&]{p.witness(x,a,runtime::ExecutionLease(policy));},"actual work bound ignored");
    for(std::size_t i=0;i<16;++i) {
        auto bad_a=a;--roles(bad_a)[i]->max_sequence;
        rejects([&]{p.witness(x,bad_a);},"witness loses independent cutoff");
        rejects([&]{p.witness(c.chain(grade(1),{},a.original.original.cylinder),bad_a);},"empty witness loses cutoff");
        rejects([&]{certify(f,bad_a);},"preparation loses cutoff");
        bad_a=a;++roles(bad_a)[i]->source_namespace;rejects([&]{p.witness(x,bad_a);},"foreign namespace accepted");
    }
}
void interpretation() {
    const auto f=native_covering_base_change_example::fixture(4);const auto changed_h=f.changed.original();auto hr=changed_h.declaration();
    const auto c=f.original.domain();const auto product=c.tensor_product();bool changed=false;
    for(const auto& t:product.declaration().types)if(t.left&&t.right) {
        for(auto& u:hr.realization.types)if(u.product_type==t.key){u.carrier_type=0;changed=true;break;}
        if(changed)break;
    }
    const auto h=inference::PreparedNativeRelationalHomotopy::prepare(c,hr,f.available.changed.original);
    const auto altered=Plan::prepare(h,f.changed.covering(),f.changed.declaration(),f.available.changed);
    rejects([&]{BaseChange::prepare(f.original,altered,f.pullback,f.request,f.available);},"unused interpretation changed on empty sources");
    check(changed,"missing unused interpretation test");
    const auto full=native_covering_base_change_example::fixture();const auto full_h=full.changed.original();hr=full_h.declaration();
    storage::NativeAccessionDirectory cd(*hr.carrier.endpoint(),hr.carrier.source_namespace());hr.carrier=native_cylinder_example::anchor(cd);
    const auto same_h=inference::PreparedNativeRelationalHomotopy::prepare(full.original.domain(),hr,full.available.changed.original);
    const auto same_lift=Plan::prepare(same_h,full.changed.covering(),full.changed.declaration(),full.available.changed);
    rejects([&]{BaseChange::prepare(full.original,same_lift,full.pullback,full.request,full.available);},"equal scalar carrier publication substituted");
    hr=full_h.declaration();hr.cylinder_target.key=1601;
    const auto alias_h=inference::PreparedNativeRelationalHomotopy::prepare(full.original.domain(),hr,full.available.changed.original);
    const auto alias_lift=Plan::prepare(alias_h,full.changed.covering(),full.changed.declaration(),full.available.changed);
    rejects([&]{BaseChange::prepare(full.original,alias_lift,full.pullback,full.request,full.available);},"distinct inherited maps share an identity");
}
void temporal_and_caps() {
    const auto f=native_covering_base_change_example::fixture(1,0,1);const auto p=certify(f,f.available);const auto c=p.domain();const auto a=f.available;
    const auto x=c.chain(grade(1),{{0,Q{253,251}}},a.original.original.cylinder);const auto original_result=p.witness(x,a);
    const auto pins=inference::native_covering_base_change_detail::pins(f.original,f.changed,f.pullback);
    // Shared actual roles use one actual directory/authority. Each cutoff remains
    // separate, while minimum caps include all roles naming that authority.
    for(unsigned mode=0;mode<3;++mode)for(std::size_t selected=0;selected<16;++selected) {
        std::map<std::pair<std::uint64_t,std::uint64_t>,std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
        std::array<storage::PinnedNativeAccession,16> pp=pins;
        for(std::size_t i=0;i<16;++i) {
            const auto id=std::pair{pins[i].source_namespace(),pins[i].sequence()};
            if(!dirs.contains(id)) {
                storage::NativeAccessionLimits limits;
                if(pins[i].endpoint()==pins[selected].endpoint()) {if(mode==1)limits.max_action_work=3999999-selected;if(mode==2)limits.max_integer_bytes=1;}
                auto dir=std::make_unique<storage::NativeAccessionDirectory>(*pins[i].endpoint(),pins[i].source_namespace(),
                    std::vector<storage::RelationalCodeObservationQuery>{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits);
                if(mode==0){const auto old=dir->pin_snapshot();auto next=*old;++next.sequence;dir->advance(storage::make_temporal_segment(*old,next));}
                dirs.emplace(id,std::move(dir));
            }
            pp[i]=native_cylinder_example::anchor(*dirs.at(id));
        }
        auto current=a;if(mode==0)for(auto* role:roles(current))++role->max_sequence;
        auto cr=c.declaration();cr.left=pp[0];cr.right=pp[1];
        const auto domain=inference::PreparedNativeCylinder::prepare(cr,current.original.original.cylinder);
        const auto make_lift=[&](std::size_t offset,const Plan& prior,Availability aa) {
            const auto prior_h=prior.original();auto hr=prior_h.declaration();hr.carrier=pp[offset+2];hr.target=pp[offset+3];
            const auto h=inference::PreparedNativeRelationalHomotopy::prepare(domain,hr,aa.original);
            const auto prior_cover=prior.covering();auto cover=prior_cover.declaration();cover.cover=pp[offset+4];cover.base=pp[offset+5];
            const auto cov=inference::PreparedNativeCovering::prepare(cover,aa.covering);
            return Plan::prepare(h,cov,prior.declaration(),aa);
        };
        const auto old=make_lift(0,f.original,current.original),now=make_lift(6,f.changed,current.changed);
        const auto square=inference::PreparedNativeCoveringPullback::prepare(old.covering(),now.covering(),f.pullback.declaration(),current.pullback);
        if(mode==0) {
            auto short_a=current;--roles(short_a)[selected]->max_sequence;
            rejects([&]{BaseChange::prepare(old,now,square,f.request,short_a);},"future publication admitted at old cutoff");
        }
        const auto candidate=BaseChange::prepare(old,now,square,f.request,current);
        const auto input=domain.chain(grade(1),{{0,Q{253,251}}},current.original.original.cylinder);
        if(mode==2)rejects<std::length_error>([&]{candidate.witness(input,current);},"native integer cap lost");
        else {
            const auto w=candidate.witness(input,current);
            if(mode==1) {
                check(w.provenance().limits.steps==3999999-selected,"native work cap lost");
                const auto empty=candidate.witness(domain.chain(grade(1),{},current.original.original.cylinder),current);
                check(empty.provenance().limits.steps==3999999-selected,"empty input loses cap");
            } else {
                rejects([&]{candidate.witness(x,current);},"old input adopts later owner");
                rejects([&]{p.witness(input,a);},"later input leaks to old owner");
                const auto retained=p.witness(x,a);check(retained.witness().projected()==original_result.witness().projected(),"directory advance changes retained outputs");
                for(std::size_t i=0;i<16;++i)check(w.provenance().sequences[i]==original_result.provenance().sequences[i]+1,"role sequence substituted");
            }
        }
    }
}
template<class T>concept TemporaryDeclaration=requires(T t){std::move(t).declaration();};
template<class T>concept TemporaryCertificate=requires(T t){std::move(t).certificate();};
template<class T>concept TemporaryCones=requires(T t){std::move(t).cones();};
template<class T>concept TemporaryWitness=requires(T t){std::move(t).witness();};
static_assert(!TemporaryDeclaration<BaseChange> && !TemporaryDeclaration<topology::OwnedCoveringHomotopyBaseChange>);
static_assert(!TemporaryCertificate<topology::OwnedCoveringHomotopyBaseChange> && !TemporaryCones<BaseChange>);
static_assert(!TemporaryWitness<inference::NativeCoveringHomotopyBaseChangeWitness>);
} // namespace
int main(int argc,char** argv) {
    try {
        const bool records=argc==2 && std::string_view(argv[1])=="--records";
        for(unsigned kind=0;kind<10;++kind)for(std::size_t sheet=0;sheet<2;++sheet)for(unsigned mode=0;mode<3;++mode)exercise(kind,sheet,mode,records);
        if(!records){contracts();interpretation();temporal_and_caps();std::cout<<"checks="<<checks<<" witnesses="<<witnesses<<'\n';}
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
