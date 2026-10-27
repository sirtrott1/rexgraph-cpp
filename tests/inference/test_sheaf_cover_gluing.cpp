#include "../../examples/sheaf_gluing_data.hpp"
#include <future>
#include <iostream>
using namespace sheaf_gluing_example;
std::size_t checks{};
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const std::string& reason){bool rejected=false;try{f();}catch(const std::exception& e){if(std::string(e.what()).find(reason)==std::string::npos)throw;rejected=true;}check(rejected,"expected refusal absent");}
bool eq(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
Q dot(const S& a,const S& b){Q sum=0;for(const auto& x:a.entries())for(const auto& y:b.entries())if(x.index==y.index)sum+=x.value*y.value;return sum;}
std::vector<ObservationCoordinateSpace> spaces(){return {space("A",2,10),space("B",3,11),space("C",2,12)};}
std::vector<ObservationLinearLink> links(bool redundant=true){std::vector<ObservationLinearLink> out;const std::array<std::array<std::size_t,2>,3> pairs{{{0,1},{0,2},{1,2}}};for(std::size_t i=0;i<(redundant?3u:2u);++i){Guard g;const auto a=pairs[i][0],b=pairs[i][1];out.push_back({"K"+std::to_string(i),a,b,space("K",1,1),{certified_colimit_detail::block(inverse_chart(a),0,1,0,chart(a).cols(),g),certified_colimit_detail::block(inverse_chart(b),0,1,0,chart(b).cols(),g)}});}return out;}
void primitive(){
    Guard g;CertifiedStalkObservations atlas(spaces(),links(),witness(),g);check(atlas.dimension()==5,"redundant interfaces retain dimension five");
    check(atlas.compatibility().rows()==3,"redundant condition retained with its own row");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int c=-2;c<=2;++c){Guard guard;const S x(5,{{0,a},{1,b},{2,c},{3,Q{1,3}},{4,Q{-2,7}}});std::vector<S> locals;std::map<std::size_t,Q> total;
        for(std::size_t p=0;p<3;++p){locals.push_back(atlas.restrict(p,x,guard));const auto y=atlas.contribution(p,locals.back(),guard);for(const auto& e:y.entries())total[e.index]+=e.value;
            for(std::size_t i=0;i<locals.back().size();++i){const auto b0=S::basis(locals.back().size(),i);check(dot(locals.back(),b0)==dot(x,atlas.restrict_transpose(p,b0,guard)),"readout transpose pairing");check(dot(atlas.contribution(p,b0,guard),x)==dot(b0,atlas.contribution_transpose(p,x,guard)),"reconstruction transpose pairing");}
        }
        std::vector<numeric::SparseRationalEntry> es;for(const auto& [i,q]:total)if(q!=0)es.push_back({i,q});
        check(eq(S(5,es),x),"actual reconstruction channels sum once");check(eq(atlas.glue(locals,guard),x),"actual compatible local observations recover");check(atlas.compatibility_residual(locals,guard).empty(),"all interface residuals zero");
    }
    auto observed=std::vector<S>{S(2,{{0,5},{1,3}}),S(3,{{0,4},{1,5},{2,7}}),S(2,{{0,2},{1,13}})};
    const auto residual=atlas.compatibility_residual(observed,g);check(eq(residual,S(3,{{0,-2},{2,2}})),"retained signed AB/BC mismatch");
    bool caught=false;try{atlas.glue(observed,g);}catch(const ObservationCompatibilityError& e){caught=true;check(eq(e.residual(),residual),"exception retains exact mismatch");}check(caught,"incompatible tuple not averaged");
    const auto fail=[&](const StalkObservationWitness& w,const char* msg){rejects([&]{Guard gg;CertifiedStalkObservations no(spaces(),links(),w,gg);},msg);};
    auto w=witness();w.reconstruction.set(0,0,7);fail(w,"left inverse");w=witness();w.accession.set(2,0,2);w.reconstruction=M(5,7);w.reconstruction.set(0,0,1);w.reconstruction.set(0,1,-1);w.reconstruction.set(1,1,1);w.reconstruction.set(2,3,1);w.reconstruction.set(3,4,1);w.reconstruction.set(4,6,1);w.reconstruction.set(4,5,-1);fail(w,"compatibility");
    w=witness();w.compatibility_lift=M(7,3);fail(w,"omits compatible directions");
    auto ls=links();ls[0].restrictions[0]=M(2,2);rejects([&]{CertifiedStalkObservations no(spaces(),ls,witness(),g);},"shape");
    for(const auto limit:std::vector<numeric::ExactActionLimits>{{6,16000000,64000000,1000},{4096,4,64000000,1000},{4096,16000000,1,1000},{4096,16000000,64000000,0}})rejects([&]{Guard gg(limit);CertifiedStalkObservations no(spaces(),links(),witness(),gg);},"limit");
    int count=0;rejects([&]{Guard gg({},[&]{if(++count==7)throw std::runtime_error("cancelled");});CertifiedStalkObservations no(spaces(),links(),witness(),gg);},"cancelled");
    // Missing observations are not zero. All three typed records are required.
    rejects([&]{atlas.glue({S(2),S(3)},g);},"one total");
    // No interface: the exact atlas is the complete direct sum, not a quotient.
    CertifiedStalkObservations free({space("one",2,10)}, {},{space("free",2,100),M::identity(2),M::identity(2),M(2,0)},g);
    check(eq(free.glue({S(2,{{1,9}})},g),S(2,{{1,9}})),"unshared observation retained");
}
void replace_matrix(TemporalEndpoint& e,std::uint64_t key,const M& m){for(auto& t:e.tensors)if(t.key==key){t.tensor=temporal_matrix_tensor(m);return;}throw std::logic_error("fixture matrix missing");}
M get_matrix(const TemporalEndpoint& e,std::uint64_t key){for(const auto& t:e.tensors)if(t.key==key)return temporal_tensor_matrix(t);throw std::logic_error("fixture matrix missing");}
Dataset permute(Dataset d,const std::array<std::size_t,3>& order){
    const auto old=d.locals;const std::array<std::size_t,3> off{0,2,5};M r(7,7);std::size_t pos=0;std::array<std::size_t,3> inverse{};
    for(std::size_t p=0;p<3;++p){d.locals[p]=old[order[p]];inverse[order[p]]=p;for(std::size_t j=0;j<chart(order[p]).rows();++j)r.set(pos++,off[order[p]]+j,1);}
    for(auto& i:d.requests){i.left=inverse[i.left];i.right=inverse[i.right];}
    for(auto& c:d.records){const auto origins=c.origins;for(std::size_t p=0;p<3;++p)c.origins[p]={p,origins[order[p]].local};replace_matrix(d.certificates,c.accession.key,product(r,get_matrix(d.certificates,c.accession.key)));replace_matrix(d.certificates,c.reconstruction.key,product(get_matrix(d.certificates,c.reconstruction.key),r.transpose()));replace_matrix(d.certificates,c.compatibility_lift.key,product(r,get_matrix(d.certificates,c.compatibility_lift.key)));}
    return d;
}
void native(){
    const auto data=dataset();const auto p=plan(data);const auto r=result(p,data);const auto& v=p.view();Guard g;
    check(v.dimension(g0)==10 && v.dimension(g1)==5 && v.cell_count(g0)==2 && v.cell_count(g1)==1,"independent sources identify common structure once");
    check(r.total==2238 && *r.matches_reference,"native sheaf action feeds moments");check(r.components[0].self==125 && r.components[1].self==1849 && r.components[2].self==256 && r.interactions[0].doubled==8,"actual contextual channels");
    check(eq(r.aggregate.sparse_values(),S(5,{{0,-4},{1,-11},{2,-18},{3,-39},{4,16}})),"native upward result");
    check(v.patches()[0].fibers().fibers().stalks().dimension({g0,0})==1 && v.patches()[0].sheaf().sheaf().stalks().dimension({g0,0})==2,"down and up stalk spaces differ");
    const M u=M::from_rows({{2,0,0,0,0},{1,3,0,0,0},{-1,0,4,0,0},{2,0,0,5,0},{3,0,0,0,-2}});
    const auto op=v.coboundary(g0);
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int c=-2;c<=2;++c){
        const S x(10,{{0,a},{2,b},{5,c},{8,2}});const auto y=op.apply(x,g);std::vector<numeric::SparseRationalEntry> ref;
        std::array<Q,5> input{Q{c-a},0,Q{-b},2,0};for(std::size_t i=0;i<5;++i){Q q=0;for(std::size_t j=0;j<5;++j)q+=u.get(i,j)*input[j];if(q!=0)ref.push_back({i,q});}
        check(eq(y,S(5,ref)),"independent expanded operator reference");
        const S z(5,{{0,3},{2,-1}});check(dot(y,z)==dot(x,op.transpose_apply(z,g)),"actual algebraic transpose");
        for(std::size_t patch=0;patch<3;++patch){const auto lx=v.restrict_field(g0,x,patch,g),ly=v.restrict_field(g1,y,patch,g);const auto computed=local::SparseSheafActions(v.patches()[patch].sheaf()).coboundary(lx).field;
            check(computed.values().size()==ly.values().size(),"local coboundary commutes with readout inventory");for(const auto& [cell,val]:ly.values())check(eq(val,computed.values().at(cell)),"whole local readout/coboundary square");
        }
    }
    std::array<std::size_t,3> order{0,1,2};do{const auto d=permute(data,order);const auto pp=plan(d);check(result(pp,d).total==2238,"permuted chart order unchanged moment");}while(std::next_permutation(order.begin(),order.end()));
    const auto minimal=dataset(false,false,false);check(result(plan(minimal),minimal).total==2238,"redundant interface not subtracted again");
    auto bad=data;auto key=bad.locals[0].contract.upward.maps[0].tensor;auto m=get_matrix(bad.locals[0].endpoint,key);m.set(0,1,m.get(0,1)+1);replace_matrix(bad.locals[0].endpoint,key,m);
    rejects([&]{prepare(bad);},"naturality");
    const auto with_zero=dataset(true);check(result(plan(with_zero),with_zero).total==2238,"inactive present-zero and cancellation retained");
    bad=with_zero;key=bad.locals[0].contract.upward.maps[2].tensor;m=get_matrix(bad.locals[0].endpoint,key);m.set(0,1,m.get(0,1)+1);replace_matrix(bad.locals[0].endpoint,key,m);rejects([&]{prepare(bad);},"naturality");
    bad=data;bad.requests[0].stalks[0].restrictions[0].role=TemporalTensorRole::SheafRestriction;rejects([&]{prepare(bad);},"role");
    auto wrong=inputs(data.locals,0);wrong[0].pop_back();rejects([&]{p.glue_field(g0,wrong,available());},"every input stalk");
    auto foreign=v.patches()[0];const DeclaredLocalFieldBinding fresh(data.locals[0].endpoint,data.locals[0].contract);std::vector<local::SparseSheafCochain> local_values;
    local_values.push_back(fresh.bind_upward(g0,data.locals[0].inputs[0]));for(std::size_t i=1;i<3;++i)local_values.push_back(v.patches()[i].bind_upward(g0,data.locals[i].inputs[0]));rejects([&]{v.glue_fields(g0,local_values,g);},"foreign");
    // Im(A) describes overlap compatibility, not automatically ker(delta^0).
    check(!r.aggregate.sparse_values().empty(),"compatible overlap cochain need not be a global section");
    auto constants=data;for(std::size_t patch=0;patch<3;++patch){const auto input=v.patches()[patch].bind_upward(g0,data.locals[patch].inputs[0]);set_upward(constants.locals[patch],g0,1,input.values().at(0));}check(result(plan(constants),constants).total==0,"globally compatible constant section");
    const auto work=[&]{Guard gg;return v.coboundary(g0).apply(r.input.sparse_values(),gg).entries();};auto future=std::async(std::launch::async,work);check(future.get()==work(),"shared immutable plan with separate guards");
}
void higher(){
    const auto d=dataset(false,true);const auto p=plan(d);const auto& v=p.view();Guard g;
    check(v.cell_count(g0)==3 && v.cell_count(g1)==3 && v.cell_count({2})==1,"shared triangle cells once");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){const S x(15,{{0,a},{6,b},{12,1}});const auto dx=v.coboundary(g0).apply(x,g);check(v.coboundary(g1).apply(dx,g).empty(),"cochain square zero on compatible triangle");}
    // Interface naturality survives a private output perturbation, but cochain
    // closure can fail. No cohomology or flatness certificate is inferred.
    auto bad=d;const auto key=bad.locals[1].contract.upward.maps[6].tensor;auto m=get_matrix(bad.locals[1].endpoint,key);m.set(1,1,m.get(1,1)+1);replace_matrix(bad.locals[1].endpoint,key,m);const auto bp=plan(bad);
    const S x(15,{{2,1}});const auto dx=bp.view().coboundary(g0).apply(x,g);check(!bp.view().coboundary(g1).apply(dx,g).empty(),"nonzero higher cochain defect remains inspectable");
}
SheafCoverLocalFieldView one_patch(const Fixture& f){
    const DeclaredLocalFieldBinding binding(f.endpoint,f.contract);std::vector<ObservationCellWitness> certificates;
    for(const auto& st:f.contract.upward.stalks){const auto n=st.carrier.domain.logical_size();ObservationCoordinateSpace target{"single",st.carrier.coordinate_system,st.declaration};certificates.push_back({{{0,st}},{target,M::identity(n),M::identity(n),M(n,0)}});}
    return SheafCoverLocalFieldView({binding},certificates,{});
}
void directional(){
    core::RelationalComplexBuilder source(signature());source.add_grade_one(core::RelationKey{100},0,{{10,true,true,1},{10,true,false,1}});
    auto f=make_source(source.freeze());replace_upward(f,space("scalar up",1,1),M::from_rows({{1}}));set_upward(f,g0,0,S::basis(1,0,3));
    replace_matrix(f.endpoint,f.contract.upward.maps[1].tensor,M::from_rows({{2}}));
    const auto v=one_patch(f);Guard g;const auto input=v.patches()[0].bind_upward(g0,f.inputs[0]);const auto x=v.glue_fields(g0,{input},g);
    check(eq(v.coboundary(g0).apply(x,g),S::basis(1,0,3)),"opposite scalar coefficients do not erase directional upward mismatch");
    check(v.occurrence_count(g1)==2,"cancelling source occurrence identities retained");
    core::RelationalComplexBuilder b(signature());b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{30,true,false,1}});
    const auto h=make_source(b.freeze(),1,true);const auto hv=one_patch(h);const auto hi=hv.patches()[0].bind_upward(g0,h.inputs[0]);const auto hx=hv.glue_fields(g0,{hi},g);const auto hy=hv.coboundary(g0).apply(hx,g);
    const auto local=hv.restrict_field(g1,hy,0,g);const auto reference=local::SparseSheafActions(hv.patches()[0].sheaf()).coboundary(hi).field;
    check(hv.dimension(g0)==6 && hv.dimension(g1)==4,"1 2 3 heterogeneous observation stalks");
    for(const auto& [cell,value]:reference.values())check(eq(value,local.values().at(cell)),"rectangular restrictions agree with native source action");
    rejects([&]{Guard stop({},[]{throw std::runtime_error("cancelled");});hv.coboundary(g0).apply(hx,stop);},"cancelled");check(eq(hv.coboundary(g0).apply(hx,g),hy),"cancelled call leaves immutable view reusable");
    // Active calls pin their atlas even if a host checkpoint replaces the handle.
    auto w=witness();
    auto scaled=w;for(const auto& [ij,q]:w.accession.entries())scaled.accession.set(ij.first,ij.second,2*q);
    for(const auto& [ij,q]:w.reconstruction.entries())scaled.reconstruction.set(ij.first,ij.second,q/2);
    CertifiedStalkObservations atlas(spaces(),links(),w,g),other(spaces(),links(),scaled,g);const S original(5,{{0,2},{1,3}});std::vector<S> vs;for(std::size_t i=0;i<3;++i)vs.push_back(atlas.restrict(i,original,g));bool changed=false;
    Guard callback({},[&]{if(!changed){changed=true;atlas=other;}});check(eq(atlas.glue(vs,callback),original),"active atlas call pins original maps");check(eq(atlas.glue(vs,g),S(5,{{0,1},{1,Q{3,2}}})),"subsequent atlas call sees replacement");
}

int main(){try{primitive();native();higher();directional();std::cout<<"sheaf_cover_gluing checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
