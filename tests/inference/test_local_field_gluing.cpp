#include "../../examples/local_gluing_data.hpp"
#include <iostream>
#include <future>
using namespace local_gluing_example;
std::size_t checks=0;
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,m);}
bool equal(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
void path(){
    std::vector<Fixture> fs{make(100,10,20),make(200,30,40),make(300,20,30)};
    const auto plan=make_plan(fs,{vertex_interface(fs,0,2,20),vertex_interface(fs,1,2,30)});
    const auto& v=plan.view();check(v.cell_count(g0)==4 && v.cell_count(g1)==3 && v.occurrence_count(g1)==6,"quotient counts");
    check(moments(plan,fs)==std::array<Q,3>{1,2,5},"global moments");const auto r=third(plan,fs);check(r.total==5 && r.matches_reference==true,"actual action-moment sum");
    check(r.components[0].self==4 && r.components[1].self==0 && r.components[2].self==1,"contextual component moments");
    std::vector<Fixture> ab{fs[0],fs[1]};check(moments(make_plan(ab,{}),ab)==std::array<Q,3>{1,2,4},"independent two-piece moments");
    for(std::size_t p=0;p<3;++p){std::vector<Fixture> local{fs[p]};const auto m=moments(make_plan(local,{}),local);check(m==(p==0?std::array<Q,3>{1,2,4}:std::array<Q,3>{0,0,0}),"intrinsic local moments");}
    // Independent assembled matrix is TEST REFERENCE ONLY.
    const auto oracle=M::from_rows({{-1,0,0},{1,0,-1},{0,-1,1},{0,1,0}});Guard g;
    const auto d=v.boundary(g1);
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int c=-2;c<=2;++c){
        S x(3,{{0,Q{a,3}},{1,Q{b,5}},{2,Q{c,7}}});
        check(equal(d.apply(x,g),numeric::checked_apply(oracle,x,g)),"assembled boundary equality");
        auto combined=v.boundary(g1,0).plus(v.boundary(g1,1)).plus(v.boundary(g1,2)).apply(x,g);
        check(equal(combined,d.apply(x,g)),"component operator sum");
    }
    for(int a=-1;a<=1;++a)for(int b=-1;b<=1;++b)for(int c=-1;c<=1;++c)for(int e=-1;e<=1;++e){
        S x(4,{{0,a},{1,b},{2,c},{3,e}});check(equal(d.transpose_apply(x,g),numeric::checked_transpose_apply(oracle,x,g)),"assembled transpose equality");
    }
    std::vector<field::SparseStalkField> locals;for(std::size_t p=0;p<3;++p)locals.push_back(v.patches()[p].bind_downward(g0,fs[p].inputs[0]));
    const auto glued=v.glue_fields(g0,locals,g);check(glued.nonzeros()==1,"shared zeros retained implicitly");
    for(std::size_t p=0;p<3;++p){auto restored=v.restrict_field(g0,glued,p,g);check(restored.owner().same_owner(locals[p].owner()),"restriction owner");check(restored.values().size()==locals[p].values().size(),"restriction support");for(const auto& [cell,value]:restored.values())check(equal(value,locals[p].values().at(cell)),"restriction recovers local field");}
    auto bad=locals;bad[2]=field::SparseStalkFieldActions(v.patches()[2].fibers()).bind(g0,{{0,S::basis(1,0,1)}});rejects([&]{v.glue_fields(g0,bad,g);},"shared value disagreement");
    const DeclaredLocalFieldBinding foreign(fs[0].endpoint,fs[0].contract);bad=locals;bad[0]=foreign.bind_downward(g0,fs[0].inputs[0]);rejects([&]{v.glue_fields(g0,bad,g);},"equal data foreign owner");
    auto avail=availability(3);avail.back().max_sequence=6;rejects([&]{plan.glue_field(g0,inputs(fs,0),avail);},"all local availability dependencies");
    std::array<unsigned,3> order{0,1,2};do{
        std::vector<Fixture> perm;std::array<std::size_t,3> inverse{};for(unsigned i=0;i<3;++i){perm.push_back(fs[order[i]]);inverse[order[i]]=i;}
        const auto p=make_plan(perm,{vertex_interface(perm,inverse[0],inverse[2],20),vertex_interface(perm,inverse[1],inverse[2],30)});check(moments(p,perm)==std::array<Q,3>{1,2,5},"gluing order total");
    }while(std::next_permutation(order.begin(),order.end()));
    auto first=std::async(std::launch::async,[&]{Guard guard;return d.apply(S::basis(3,0,2),guard);});Guard other;check(equal(first.get(),d.apply(S::basis(3,0,2),other)),"immutable concurrent actions");
}
void overlap(){
    const auto f=make(100,10,20,1,false,true);std::vector<Fixture> fs{f,f};const auto map=identity_map(f.endpoint.source);
    LocalFieldInterface interface{core::OwnedRelationalSource(f.endpoint.source),0,1,map,map};const auto p=make_plan(fs,{interface});Guard g;
    check(p.view().cell_count(g0)==2 && p.view().cell_count(g1)==1 && p.view().occurrence_count(g1)==6,"shared relation and occurrences counted once");
    const auto x=p.glue_field(g1,inputs(fs,1),availability(2));check(x.sparse_values().entries()==S::basis(1,0,2).entries(),"same-field overlap not doubled");
    check(equal(p.view().boundary(g1).apply(x.sparse_values(),g),S(2,{{0,-2},{1,2}})),"inactive/zero/cancelling occurrences");
    check(p.view().boundary(g1,1).apply(x.sparse_values(),g).nonzeros()==0,"shared contribution assigned once");
    const auto separate=make_plan(fs,{});check(separate.view().cell_count(g0)==4 && separate.view().cell_count(g1)==2,"equal keys do not glue without declaration");
    auto wrong=interface;wrong.common_right.occurrences[1].pop_back();rejects([&]{make_plan(fs,{wrong});},"missing occurrence map");
    wrong=interface;wrong.common_right.cells[0][1]=0;rejects([&]{make_plan(fs,{wrong});},"noninjective interface");
    // Nonzero source maps with identical scalar boundaries still have to agree.
    auto h=make(100,10,20,1,true,true);std::vector<Fixture> hs{h,h};auto& changed=hs[1].endpoint.tensors;
    for(auto& t:changed)if(t.key==200){auto m=temporal_tensor_matrix(t);m.set(0,0,2);t.tensor=temporal_matrix_tensor(m);}
    rejects([&]{make_plan(hs,{interface});},"directional mismatch on shared occurrence");
    auto coords=fs;for(auto& b:coords[1].contract.downward.stalks){b.carrier.coordinate_system=99;for(auto& c:coords[1].endpoint.carriers)if(c.key==b.carrier.key)c.coordinate_system=99;}
    // Keep the separately declared upward family valid too.
    for(auto& b:coords[1].contract.upward.stalks)b.carrier.coordinate_system=99;
    rejects([&]{make_plan(coords,{interface});},"same dimensions foreign coordinate systems");
    auto native=PreparedNativeLocalGluing::prepare(publications(fs),{interface});auto ce=coordinate_endpoint(native.view());NativeAccessionDirectory cd(ce,999);auto anchor=cd.pin(NativeAccessionRequest::sparse_chain(g0,{},{}));
    std::vector<NativeGluedCarrierBinding> cb;for(std::size_t k=0;k<2;++k)cb.push_back({NativeCarrier::bind(anchor,800+k),native.view().layout({static_cast<std::uint16_t>(k)})});
    cb[0].expected_layout[0].coordinates=99;rejects([&]{native.bind_carriers(cb);},"wrong quotient coordinate correspondence");
    // Twist a three-patch interface cycle to identify both vertices in patch 0.
    std::vector<Fixture> cyc{f,f,f};auto a=vertex_interface(cyc,0,1,10),b=vertex_interface(cyc,1,2,10),c=vertex_interface(cyc,0,2,20);c.common_right.cells[0][0]=0;
    rejects([&]{make_plan(cyc,{a,b,c});},"noninjective multi-interface cycle");
}
void higher(){
    const auto triangle=[](bool right){core::RelationalComplexBuilder b(signature());b.declare_grade({2});
        b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});
        b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{right?40u:30u,true,false,1}});
        b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{right?40u:30u,true,false,1}});
        b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});return b.freeze();};
    // Reuse helper for dimensions and explicit total field declarations, then add
    // the grade-2 typed field (helper supports all retained grades).
    auto l=make_source(triangle(false)),r=make_source(triangle(true));std::vector<Fixture> fs{l,r};
    const auto common=edge(10,10,20);auto m=identity_map(common);m.cells[1]={0};m.occurrences[1]={0,1};
    const auto p=make_plan(fs,{{core::OwnedRelationalSource(common),0,1,m,m}});Guard g;
    check(p.view().cell_count(g0)==4 && p.view().cell_count(g1)==5 && p.view().cell_count({2})==2,"two real local triangles glued along an edge");
    const auto d2=p.view().boundary({2}),d1=p.view().boundary(g1);
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){const auto y=d2.apply(S(2,{{0,a},{1,b}}),g);check(d1.apply(y,g).nonzeros()==0,"glued graded chain");}
    check(d2.apply(S(2,{{0,1},{1,-1}}),g).nonzeros()==4,"shared edge boundary contributions cancel");
}
void large_and_limits(){
    std::size_t steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        std::vector<Fixture> fs{make(100,10,20,n),make(200,20,30,n)};numeric::ExactActionLimits limits;limits.dimension=std::numeric_limits<std::size_t>::max();limits.entries=100000;
        auto p=PreparedNativeLocalGluing::prepare(publications(fs),{vertex_interface(fs,0,1,20)},limits);Guard g(limits);
        const auto y=p.view().boundary(g1).apply(S::basis(2*n,n-1,Q{2,3}),g);
        check(y.size()==3*n && y.nonzeros()==2,"symbolic stalk extent");check(y.entries()[0].index==n-1 && y.entries()[0].value==Q{-2,3} && y.entries()[1].index==2*n-1,"exact large boundary");
        if(!steps)steps=g.steps();else check(g.steps()==steps,"work independent of unused local extent");
        auto small=limits;small.entries=1;rejects([&]{Guard bad(small);p.view().boundary(g1).apply(S::basis(2*n,0),bad);},"action resource refusal");
    }
    std::cout<<"symbolic_action_steps="<<steps<<'\n';
    auto f=make(100,10,20);std::vector<Fixture> fs{f};auto pubs=publications(fs);numeric::ExactActionLimits tiny;tiny.steps=0;
    rejects([&]{PreparedNativeLocalGluing::prepare(pubs,{},tiny);},"preparation work bound");
    auto native=make_plan(fs,{});numeric::ExactActionLimits lim;Guard stop(lim,[]{throw std::runtime_error("stopped");});rejects([&]{native.view().boundary(g1).apply(S::basis(1,0),stop);},"cooperative action cancellation");
}
int main(){try{path();overlap();higher();large_and_limits();std::cout<<"local_field_gluing checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
