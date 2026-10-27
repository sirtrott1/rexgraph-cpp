#include "../../examples/injective_gluing_data.hpp"
#include <future>
#include <iostream>
using namespace injective_gluing_example;
std::size_t checks{};
void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f,const char* s){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,s);}
bool equal(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
Q dot(const S& a,const S& b){Q q=0;for(const auto& x:a.entries())for(const auto& y:b.entries())if(x.index==y.index)q+=x.value*y.value;return q;}
void primitive(){
    const auto a=StalkChartMap::explicit_map(M::from_rows({{1,1},{0,1}})),ai=StalkChartMap::explicit_map(M::from_rows({{1,-1},{0,1}}));
    const auto b=StalkChartMap::explicit_map(M::from_rows({{1,0,1},{0,2,0},{0,0,1}})),bi=StalkChartMap::explicit_map(M::from_rows({{1,0,-1},{0,Q{1,2},0},{0,0,1}}));
    Guard g;SplitStalkPushout po(1,{a,b},{ai,bi},g);
    check(po.dimension()==4,"2+3-1 dimensions retained");
    for(int s=-2;s<=2;++s)for(int u=-2;u<=2;++u)for(int v=-2;v<=2;++v)for(int w=-1;w<=1;++w){
        S x(4,{{0,s},{1,u},{2,v},{3,w}});const auto l=po.restrict(0,x,g),r=po.restrict(1,x,g);
        check(equal(po.glue(l,r,g),x),"complete section reconstruction");
        const auto left=po.include(0,po.representative(0,x,g),g),right=po.include(1,po.representative(1,x,g),g);
        for(std::size_t i=0;i<4;++i){const auto probe=S::basis(4,i);check(dot(left,probe)+dot(right,probe)==dot(x,probe),"quotient section identity on full common+private field");}
        check(equal(po.restrict(0,po.include(0,l,g),g),l) && equal(po.restrict(1,po.include(1,r,g),g),r),"local injections retain all directions");
        check(dot(po.include(0,l,g),x)==dot(l,po.include_transpose(0,x,g)),"inclusion algebraic transpose");
        check(dot(po.representative(1,x,g),r)==dot(x,po.representative_transpose(1,r,g)),"quotient section transpose");
    }
    for(int s=-2;s<=2;++s){const auto l=ai.apply(S::basis(2,0,s),g),r=bi.apply(S::basis(3,0,s),g);check(equal(po.include(0,l,g),po.include(1,r,g)),"two common injections identify exactly");}
    rejects([&]{po.glue(ai.apply(S::basis(2,0,1),g),bi.apply(S::basis(3,0,2),g),g);},"no averaging shared values");
    rejects([&]{SplitStalkPushout(1,{a,b},{a,bi},g);},"false inverse rejected");
    rejects([&]{SplitStalkPushout(3,{a,b},{ai,bi},g);},"interface cannot exceed local rank");
    rejects([&]{SplitStalkPushout(0,{a,b},{ai,bi},g);},"zero common stalk not silently introduced");
    const auto t=StalkChartMap::explicit_map(M::from_rows({{1,2},{0,3}}));
    check(SplitStalkPushout::descends(t,StalkChartMap::identity(1),1,1,g),"private-to-shared term allowed");
    const auto bad=StalkChartMap::explicit_map(M::from_rows({{1,2},{1,3}}));
    check(!SplitStalkPushout::descends(bad,StalkChartMap::identity(1),1,1,g),"shared-to-private leak fails descent");
}
void native(){
    const auto data=make_dataset(true);const auto p=plan(data);const auto& view=p.view();Guard g;
    check(view.cell_count(g0)==2 && view.cell_count(g1)==1 && view.dimension(g0)==8 && view.dimension(g1)==4,"one shared cell identity with private directions");
    check(view.occurrence_count(g1)==6,"all common occurrence identities retained exactly once");
    const auto r=result(p,data);check(r.total==454 && r.components[0].self==68 && r.components[1].self==246 && r.matches_reference==true,"actual native moment channels");
    check(2*dot(r.components[0].field.sparse_values(),r.components[1].field.sparse_values())==140,"retained signed cross interaction");
    check(equal(r.input.sparse_values(),S(4,{{0,2},{1,3},{2,5},{3,7}})),"unshared local values survive assembly");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){
        S x(4,{{0,a},{1,b},{2,Q{1,3}},{3,Q{-2,7}}});
        const Q z=Q{a}+Q{b}+Q{-2,7};S expected(8,{{0,-z},{1,-b},{2,Q{-1,3}},{3,Q{2,7}},{4,z},{5,b},{6,Q{1,3}},{7,Q{-2,7}}});
        const auto y=view.boundary(g1).apply(x,g);check(equal(y,expected),"independent full sparse formula");
        const auto l=view.restrict_field(g1,x,0,g),q=view.restrict_field(g1,x,1,g);
        check(equal(view.glue_fields(g1,{l,q},g),x),"local restrictions recover whole global section");
        const auto dt=view.boundary(g1).transpose_apply(y,g);check(dot(y,y)==dot(x,dt),"transpose in quotient coordinates");
    }
    // Entire local basis, including private directions, commutes with boundary.
    for(std::size_t side=0;side<2;++side)for(std::size_t i=0;i<side+2;++i){
        field::SparseStalkFieldActions fa(view.patches()[side].fibers());const auto x=fa.bind(g1,{{0,S::basis(side+2,i)}});
        const auto local=fa.boundary(x).field;
        check(equal(view.boundary(g1).apply(view.include_chain_field(g1,x,side,g),g),view.include_chain_field(g0,local,side,g)),"whole local injection square including complement");
    }
    auto bad=data;bad.maps[1].occurrences[1][0]=1;rejects([&]{plan(bad);},"structural occurrence identity mismatch");
    bad=data;bad.requests.pop_back();rejects([&]{plan(bad);},"missing interface cell");
    bad=data;bad.requests.push_back(bad.requests[0]);rejects([&]{plan(bad);},"duplicate interface cell");
    bad=data;bad.requests[0].common.declaration.axes[0].key+=99;rejects([&]{plan(bad);},"wrong common coordinate declaration");
    bad=data;bad.requests[0].split[0].patch=1;rejects([&]{plan(bad);},"wrong local side");
    bad=data;bad.requests[0].split[0].local.carrier.coordinate_system+=1;rejects([&]{plan(bad);},"wrong expected local coordinate identity");
    bad=data;bad.requests[0].split[1].accession=bad.requests[0].split[1].reconstruction;rejects([&]{plan(bad);},"reversed accession role");
    bad=data;
    for(auto& f:bad.locals){set_field(f,g0,S(f.contract.downward.stalks[0].carrier.domain.logical_size()));set_field(f,g1,S(f.contract.downward.stalks.back().carrier.domain.logical_size()));}
    for(auto& t:bad.locals[0].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto m=temporal_tensor_matrix(t);m.set(1,0,m.get(1,0)+1);t.tensor=temporal_matrix_tensor(m);break;}
    rejects([&]{plan(bad);},"zero observations do not hide whole-map descent defect");
    bad=data;set_field(bad.locals[0],g1,S::basis(2,0,10));const auto mismatch=plan(bad);
    rejects([&]{mismatch.glue_field(g1,inputs(bad.locals,1),availability(3));},"shared field mismatch rejected after valid map preparation");
    auto lim=numeric::ExactActionLimits{};lim.steps=0;rejects([&]{prepare(data,lim);},"preparation work refusal");
    lim=numeric::ExactActionLimits{};lim.entries=1;rejects([&]{prepare(data,lim);},"preparation entries refusal");
    lim=numeric::ExactActionLimits{};lim.integer_bits=1;rejects([&]{prepare(data,lim);},"exact coefficient growth refusal");
    auto available=availability(3);available.back().max_sequence=6;rejects([&]{p.glue_field(g1,inputs(data.locals,1),available);},"common source has availability dependency");
    const auto foreign=plan(data);const auto fx=foreign.glue_field(g1,inputs(data.locals,1),availability(3));rejects([&]{p.boundary(g1).execute(fx,availability(3));},"equal values in a foreign quotient owner rejected");
    auto swapped=data;std::swap(swapped.locals[0],swapped.locals[1]);std::swap(swapped.maps[0],swapped.maps[1]);for(auto& d:swapped.requests){std::swap(d.split[0],d.split[1]);d.split[0].patch=0;d.split[1].patch=1;}
    const auto q=plan(swapped);check(result(q,swapped).total==454,"swapped patches preserve total while changing coordinate ordering/attribution");
    auto task=std::async(std::launch::async,[p,data]{return result(p,data).total;});check(task.get()==r.total,"immutable native plan concurrent independent workspaces");
    const auto op=view.boundary(g1);const auto x=r.input.sparse_values();Guard cancel({},[]{throw std::runtime_error("cancelled");});rejects([&]{op.apply(x,cancel);},"cooperative callback cancellation");
    check(equal(op.apply(x,g),r.aggregate.sparse_values()),"operation reusable after cancellation");
}
void higher(){
    const auto tri=[](bool right){core::RelationalComplexBuilder b(signature());b.declare_grade({2});b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{right?40u:30u,true,false,1}});b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{right?40u:30u,true,false,1}});b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});return b.freeze();};
    auto k=make(10,10,20);auto m=identity_map(k.endpoint.source);const auto data=assemble(k,make_source(tri(false),2),make_source(tri(true),3),{m,m});const auto p=plan(data);const auto& v=p.view();Guard g;
    check(v.cell_count(g0)==4 && v.cell_count(g1)==5 && v.cell_count({2})==2,"two independent triangles retain structural quotient");
    check(v.dimension(g0)==13 && v.dimension(g1)==14 && v.dimension({2})==5,"shared rank1 retains rank2 and rank3 private directions");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){S x(5,{{0,a},{1,Q{1,3}},{2,b},{3,Q{-1,2}},{4,3}});const auto y=v.boundary({2}).apply(x,g);check(v.boundary(g1).apply(y,g).nonzeros()==0,"two-step boundary on proper stalk pushout");}
    for(std::size_t side=0;side<2;++side){field::SparseStalkFieldActions fa(v.patches()[side].fibers());for(std::size_t i=0;i<side+2;++i){const auto x=fa.bind({2},{{0,S::basis(side+2,i)}});const auto y=fa.boundary(x).field;check(equal(v.include_chain_field(g1,y,side,g),v.boundary({2}).apply(v.include_chain_field({2},x,side,g),g)),"higher whole-carrier descent");}}
}
void symbolic(){
    std::size_t steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        numeric::ExactActionLimits lim;lim.dimension=std::numeric_limits<std::size_t>::max();lim.entries=1000;lim.steps=10000;
        M shear(n,n);shear.set(0,n-1,1);M back(n,n);back.set(0,n-1,-1);Guard prep(lim);
        SplitStalkPushout p(1,{StalkChartMap(1,shear),StalkChartMap::identity(n)},{StalkChartMap(1,back),StalkChartMap::identity(n)},prep);Guard g(lim);
        S x(p.dimension(),{{0,2},{n-1,3},{p.dimension()-1,4}});
        const auto left=p.restrict(0,x,g),right=p.restrict(1,x,g);check(equal(p.glue(left,right,g),x),"huge private directions retained without dense padding");
        check(SplitStalkPushout::descends(StalkChartMap::identity(n),StalkChartMap::identity(1),1,1,g),"symbolic whole common injection check");
        if(steps)check(g.steps()==steps,"symbolic action steps independent of private extent");else steps=g.steps();
        // Huge shared interface also checked symbolically (not one test per basis coordinate).
        SplitStalkPushout all(n,{StalkChartMap::identity(n),StalkChartMap::identity(n)},{StalkChartMap::identity(n),StalkChartMap::identity(n)},prep);
        check(SplitStalkPushout::descends(StalkChartMap::identity(n),StalkChartMap::identity(n),n,n,prep),"huge shared identity not enumerated");
        check(all.dimension()==n,"full shared limiting case");
    }
    std::cout<<"split_symbolic_steps="<<steps<<'\n';
}
void symbolic_view(){
    std::size_t steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        numeric::ExactActionLimits lim;lim.dimension=std::numeric_limits<std::size_t>::max();lim.entries=100000;
        const auto k=make(100,10,20),a=make(100,10,20,n),b=make(100,10,20,n);
        const DeclaredLocalFieldBinding kb(k.endpoint,k.contract,lim),ab(a.endpoint,a.contract,lim),bb(b.endpoint,b.contract,lim);
        std::vector<InjectiveStalkInterface> ds;
        for(std::size_t i=0;i<k.contract.downward.stalks.size();++i){
            M f(n,n),r(n,n);f.set(0,n-1,1);r.set(0,n-1,-1);
            ds.push_back({k.contract.downward.stalks[i],{LocalStalkChart{0,a.contract.downward.stalks[i],900,a.contract.downward.stalks[i].declaration,StalkChartMap(1,f),StalkChartMap(1,r)},LocalStalkChart{1,b.contract.downward.stalks[i],901,b.contract.downward.stalks[i].declaration,StalkChartMap::identity(n),StalkChartMap::identity(n)}}});
        }
        const auto im=identity_map(k.endpoint.source);const InjectiveLocalFieldView view(kb,{ab,bb},{im,im},ds,lim);
        const auto xleft=field::SparseStalkFieldActions(ab.fibers(),{},lim).bind(g1,{{0,S(n,{{0,-1},{n-1,3}})}});
        const auto xright=field::SparseStalkFieldActions(bb.fibers(),{},lim).bind(g1,{{0,S(n,{{0,2},{n-1,4}})}});
        Guard g(lim);const auto x=view.glue_fields(g1,{xleft,xright},g),y=view.boundary(g1).apply(x,g);
        const auto recovered=view.restrict_field(g1,x,0,g);
        check(equal(x,S(2*n-1,{{0,2},{n-1,3},{2*n-2,4}})),"symbolic source pushout stores common once and both private tails");
        check(y.nonzeros()==6 && y.size()==4*n-2,"actual huge boundary acts on six output entries");
        check(equal(recovered.values().at(0),xleft.values().at(0)),"huge view local recovery");
        if(steps)check(steps==g.steps(),"actual view steps independent of logical extent");else steps=g.steps();
        Guard stop(lim,[]{throw std::runtime_error("stop");});rejects([&]{view.boundary(g1).apply(x,stop);},"symbolic view cancellation");
    }
    std::cout<<"injective_symbolic_view_steps="<<steps<<'\n';
}
int main(){try{primitive();native();higher();symbolic();symbolic_view();std::cout<<"injective_local_gluing checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
