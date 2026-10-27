#include "../../examples/chart_gluing_data.hpp"
#include <future>
#include <iostream>
using namespace chart_gluing_example;
std::size_t checks=0;
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,m);}
bool equal(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
void charts(){
    // Whole overlap has genuinely heterogeneous stalks 1,2,3 and two relation stalks.
    core::RelationalComplexBuilder b(signature());b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});
    b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{30,true,false,1}});
    const auto first=make_source(b.freeze(),1,true);const auto second=changed(first,1,10800);
    std::vector<Fixture> fs{first,second.field};const auto m=identity_map(first.endpoint.source);
    std::vector<LocalFieldInterface> edges{{core::OwnedRelationalSource(first.endpoint.source),0,1,m,m}};
    const auto p=plan(fs,edges,second.charts),oracle=make_plan({first},{});Guard g;
    check(p.view().cell_count(g0)==3 && p.view().dimension(g0)==6 && p.view().dimension(g1)==4,"typed quotient counts");
    check(p.view().charts().size()==5,"chart declarations retained");
    rejects([&]{make_plan(fs,edges);},"no implicit cross-coordinate equality");
    for(int a=-2;a<=2;++a)for(int c=-2;c<=2;++c){
        S x(4,{{0,a},{1,Q{1,3}},{2,c},{3,Q{-2,7}}});const auto y=p.view().boundary(g1).apply(x,g);
        check(equal(y,oracle.view().boundary(g1).apply(x,g)),"all transformed occurrence maps equal common reference");
        check(equal(p.view().boundary(g1).transpose_apply(y,g),oracle.view().boundary(g1).transpose_apply(y,g)),"common coordinate transpose");
        const auto local=p.view().restrict_field(g1,x,1,g);
        check(equal(p.view().include_chain_field(g1,local,1,g),x),"local coordinate recovery/inclusion");
        const auto ld=field::SparseStalkFieldActions(p.view().patches()[1].fibers()).boundary(local).field;
        check(equal(p.view().include_chain_field(g0,ld,1,g),y),"whole local boundary inclusion square");
    }
    const auto x=p.glue_field(g1,inputs(fs,1),availability(2));
    check(equal(x.sparse_values(),oracle.glue_field(g1,inputs({first},1),availability(1)).sparse_values()),"actual transformed values glue once");
    auto read=PreparedNativeActionMoments({{"first",p.boundary(g1,0)},{"duplicate",p.boundary(g1,1)}},NativeBilinearForm::named_unit(p.boundary(g1).output()),p.boundary(g1)).execute(x,availability(2));
    check(read.matches_reference==true && read.components[1].self==0,"moment shared relation not doubled");
    for(auto bad:second.charts){
        auto req=second.charts;req[0]=bad;req[0].accession=bad.reconstruction;
        rejects([&]{plan(fs,edges,req);},"opposite role/direction rejected");
    }
    auto req=second.charts;req[0].local.declaration.axes[0].key+=1;rejects([&]{plan(fs,edges,req);},"wrong complete local type");
    req=second.charts;req[0].common_declaration.id.version=9;rejects([&]{plan(fs,edges,req);},"wrong common declaration");
    req=second.charts;req[0].common.coordinate_system+=1;rejects([&]{plan(fs,edges,req);},"wrong common carrier");
    req=second.charts;req.push_back(req[0]);rejects([&]{plan(fs,edges,req);},"duplicate chart");
    auto wrong=fs;for(auto& t:wrong[1].endpoint.tensors)if(t.key==second.charts[0].reconstruction){auto a=temporal_tensor_matrix(t);a.set(0,0,3);t.tensor=temporal_matrix_tensor(a);}
    rejects([&]{plan(wrong,edges,second.charts);},"false inverse rejected");
    wrong=fs;
    // A perturbation in an unobserved input direction must fail operator descent,
    // even when every supplied numerical input is zero.
    for(auto& f:wrong){for(auto& t:f.endpoint.tensors)if(t.role==TemporalTensorRole::Field)t.tensor={t.tensor.domain(),t.tensor.support(),S(t.tensor.domain().logical_size())};
        for(auto& v:f.endpoint.declarations->values)if(auto* t=std::get_if<numeric::SparseRationalTensor>(&v.value.data)){v.value=DeclaredValue::tensor({t->domain(),t->support(),S(t->domain().logical_size())});}}
    for(auto& t:wrong[1].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto a=temporal_tensor_matrix(t);a.set(0,1,a.get(0,1)+1);t.tensor=temporal_matrix_tensor(a);break;}
    rejects([&]{plan(wrong,edges,second.charts);},"zero field does not certify descent");
    auto leases=availability(2);leases[2].max_sequence=6;rejects([&]{p.glue_field(g1,inputs(fs,1),leases);},"chart source unavailable");
    auto limits=numeric::ExactActionLimits{};limits.steps=0;rejects([&]{PreparedNativeLocalGluing::prepare_with_charts(publications(fs),edges,second.charts,limits);},"chart preparation budget");
}
void path(){
    const auto a=changed(make(100,10,20),0,10900),c=changed(make(300,20,30),2,10902);
    std::vector<Fixture> fs{a.field,make(200,30,40),c.field};auto requests=a.charts;requests.insert(requests.end(),c.charts.begin(),c.charts.end());
    const auto p=plan(fs,{vertex_interface(fs,0,2,20),vertex_interface(fs,1,2,30)},requests);
    check(moments(p,fs)==std::array<Q,3>{1,2,5},"actual nonidentity local charts global moments");
    const auto r=third(p,fs);check(r.matches_reference==true && r.components[0].self==4 && r.components[1].self==0 && r.components[2].self==1,"contextual chart contribution");
    auto order=std::array<unsigned,3>{0,1,2};do{std::vector<Fixture> perm;std::array<std::size_t,3> index{};
        for(unsigned i=0;i<3;++i){index[order[i]]=i;perm.push_back(fs[order[i]]);}auto req=requests;for(auto& q:req)q.patch=index[q.patch];
        const auto pp=plan(perm,{vertex_interface(perm,index[0],index[2],20),vertex_interface(perm,index[1],index[2],30)},req);
        check(moments(pp,perm)==std::array<Q,3>{1,2,5},"all patch orders retain chart moments");
    }while(std::next_permutation(order.begin(),order.end()));
}
void higher_and_roles(){
    const auto triangle=[](bool right){core::RelationalComplexBuilder b(signature());b.declare_grade({2});
        b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});
        b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{right?40u:30u,true,false,1}});
        b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{right?40u:30u,true,false,1}});
        b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});return b.freeze();};
    auto a=make_source(triangle(false),2);auto b=changed(make_source(triangle(true),2),1,11100);std::vector<Fixture> fs{a,b.field};
    const auto common=edge(10,10,20);auto m=identity_map(common);m.cells[1]={0};m.occurrences[1]={0,1};
    const auto p=plan(fs,{{core::OwnedRelationalSource(common),0,1,m,m}},b.charts);Guard g;
    check(p.view().cell_count(g1)==5 && p.view().cell_count({2})==2,"two independent charted faces");
    for(int i=-2;i<=2;++i)for(int j=-2;j<=2;++j){
        const auto y=p.view().boundary({2}).apply(S(4,{{0,i},{1,Q{1,3}},{2,j},{3,Q{-2,5}}}),g);
        check(p.view().boundary(g1).apply(y,g).nonzeros()==0,"charted higher chain closure");
    }
    const auto f=make(100,10,20,2,false,true);const auto fc=changed(f,1,11200);std::vector<Fixture> both{f,fc.field};const auto im=identity_map(f.endpoint.source);
    const auto r=plan(both,{{core::OwnedRelationalSource(f.endpoint.source),0,1,im,im}},fc.charts);
    check(r.view().occurrence_count(g1)==6,"inactive zero and cancelling occurrence identities retained");
    const auto y=r.view().boundary(g1).apply(S(2,{{0,1},{1,2}}),g);
    check(equal(y,S(4,{{0,-1},{1,-2},{2,1},{3,2}})),"coefficient realized only once through charts");
    // Square non-orthogonal chart: no implicit local identity-metric claim.
    const auto Smap=coordinate_change(2),A=inverse_change(2);const S base(2,{{0,1},{1,2}});
    const auto local=numeric::checked_apply(Smap,base,g);const auto metric=numeric::checked_multiply(A.transpose(),A,g);
    const auto dot=[](const S& a,const S& b){Q q=0;for(const auto& e:a.entries())for(const auto& f:b.entries())if(e.index==f.index)q+=e.value*f.value;return q;};
    check(dot(local,local)==13 && dot(base,base)==5,"raw local identity quadrance changes under shear");
    check(dot(local,numeric::checked_apply(metric,local,g))==5,"explicit pulled-back local metric preserves quadrance");
    auto future=std::async(std::launch::async,[r]{Guard guard;return r.view().boundary(g1).apply(S::basis(2,1),guard);});
    check(equal(future.get(),r.view().boundary(g1).apply(S::basis(2,1),g)),"concurrent immutable chart actions");
}
void symbolic(){
    std::size_t old_steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        numeric::ExactActionLimits lim;lim.dimension=std::numeric_limits<std::size_t>::max();lim.entries=100000;
        const auto f=make(100,10,20,n);const DeclaredLocalFieldBinding binding(f.endpoint,f.contract,lim);auto bindings=std::vector<DeclaredLocalFieldBinding>{binding};
        std::vector<LocalStalkChart> cs;
        for(const auto& b:f.contract.downward.stalks){
            M forward(n,n),inverse(n,n);forward.set(0,n-1,1);inverse.set(0,n-1,-1);
            cs.push_back({0,b,900,b.declaration,StalkChartMap(1,forward),StalkChartMap(1,inverse)});
        }
        const GluedLocalFieldView view(bindings,{},cs,lim);Guard g(lim);
        const auto local=field::SparseStalkFieldActions(binding.fibers(),{},lim).bind(g1,{{0,S::basis(n,n-1,Q{2,3})}});
        const auto x=view.glue_fields(g1,{local},g);const auto y=view.boundary(g1).apply(x,g);
        check(x.nonzeros()==2 && y.nonzeros()==4,"symbolic shear on huge stalk");
        const auto back=view.restrict_field(g1,x,0,g);check(equal(back.values().at(0),local.values().at(0)),"symbolic chart round trip");
        if(old_steps)check(old_steps==g.steps(),"symbolic work independent of extent");else old_steps=g.steps();
        auto wrong=cs;wrong[0].from_common=StalkChartMap::identity(n);rejects([&]{GluedLocalFieldView fail(bindings,{},wrong,lim);},"sparse noninverse across huge carrier");
        auto rectangular=cs;rectangular[0].to_common=StalkChartMap::explicit_map(M(n,1));rejects([&]{GluedLocalFieldView fail(bindings,{},rectangular,lim);},"partial embedding not silently accepted as chart");
        Guard stop(lim,[]{throw std::runtime_error("cancel");});rejects([&]{view.boundary(g1).apply(x,stop);},"chart cancellation");
    }
    std::cout<<"symbolic_chart_steps="<<old_steps<<'\n';
}
void primitives(){
    const auto fixture=make(100,10,20);const DeclaredLocalFieldBinding binding(fixture.endpoint,fixture.contract);
    const GluedLocalFieldView legacy({binding},{},{});check(legacy.dimension(g0)==2,"legacy brace-init constructor remains unambiguous");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int c=-2;c<=2;++c)for(int d=-2;d<=2;++d){
        Guard g;const auto A=M::from_rows({{a,b},{c,d}}),B=M::from_rows({{1,2},{3,5}});
        const auto m=StalkChartMap::explicit_map(A),n=StalkChartMap::explicit_map(B);
        const auto p=m.multiply(n,g);const S x(2,{{0,Q{2,3}},{1,Q{-4,7}}});
        check(equal(p.apply(x,g),numeric::checked_apply(A,numeric::checked_apply(B,x,g),g)),"exact chart composition reference");
        check(equal(p.apply(x,g,true),numeric::checked_transpose_apply(B,numeric::checked_transpose_apply(A,x,g),g)),"exact transpose composition reference");
    }
}
int main(){try{primitives();charts();path();higher_and_roles();symbolic();std::cout<<"chart_local_gluing checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
