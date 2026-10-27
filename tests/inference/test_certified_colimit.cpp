#include "../../examples/colimit_gluing_data.hpp"
#include <future>
#include <iostream>
using namespace colimit_example;
std::size_t checks{};
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,std::string expected){bool no=false;try{f();}catch(const std::exception& e){no=std::string(e.what()).find(expected)!=std::string::npos;if(!no)throw;}check(no,"expected rejection missing");}
bool eq(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
Q dot(const S& a,const S& b){Q q=0;for(const auto& x:a.entries())for(const auto& y:b.entries())if(x.index==y.index)q+=x.value*y.value;return q;}
S sum(const std::vector<S>& xs,std::size_t n){std::map<std::size_t,Q> q;for(const auto& x:xs)for(const auto& e:x.entries())q[e.index]+=e.value;std::vector<numeric::SparseRationalEntry> es;for(const auto& [i,v]:q)if(v!=0)es.push_back({i,v});return S(n,es);}
void primitive(){
    Guard g;CertifiedStalkColimit c(spaces(),links(),witness(),g);
    check(c.dimension()==5 && c.relations().rank()==3,"actual finite colimit rank");
    // Three distinct lines in A=Q^2 cannot simultaneously be coordinate blocks.
    check(links()[2].sides[0].map==M::from_rows({{1},{1}}),"non-coordinate third line retained");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int k=-2;k<=2;++k){
        Guard guard;const S x(5,{{0,a},{1,b},{2,k},{3,Q{1,3}},{4,Q{-2,7}}});std::vector<S> reads,parts;
        for(std::size_t p=0;p<4;++p){
            reads.push_back(c.restrict(p,x,guard));parts.push_back(c.include(p,c.representative(p,x,guard),guard));
            for(std::size_t j=0;j<2;++j){const auto u=S::basis(2,j);
                check(eq(c.restrict(p,c.include(p,u,guard),guard),u),"every local direction survives");
                check(dot(c.include(p,u,guard),x)==dot(u,c.include_transpose(p,x,guard)),"injection algebraic transpose");
                check(dot(c.representative(p,x,guard),u)==dot(x,c.representative_transpose(p,u,guard)),"representative algebraic transpose");
            }
        }
        check(eq(c.glue(reads,guard),x),"all readouts reconstruct actual global field");check(eq(sum(parts,5),x),"sum J H is identity");
    }
    auto rs=std::vector<S>{S(2,{{0,2},{1,3}}),S(2,{{0,2},{1,5}}),S(2,{{0,3},{1,7}}),S(2,{{0,Q{5,2}},{1,11}})};
    const auto x=c.glue(rs,g);std::vector<S> added;for(std::size_t p=0;p<4;++p)added.push_back(c.include(p,rs[p],g));
    check(!eq(sum(added,5),x),"gluing descriptions is not chain addition");rs[3]=S(2,{{0,9},{1,11}});rejects([&]{c.glue(rs,g);},"observation image");
    auto ls=links();auto w=witness();ls.push_back(ls.front());ls.back().name="AB duplicate";M z(4,8);for(const auto& [ij,q]:w.relation_lift.entries())z.set(ij.first,ij.second,q);w.relation_lift=z;
    CertifiedStalkColimit redundant(spaces(),ls,w,g);check(redundant.dimension()==5,"redundant relation not subtracted again");
    // Complete certificates are required; numeric agreement on zero is not enough.
    auto fail=[&](StalkColimitWitness bad,std::string reason){rejects([&]{Guard gg;CertifiedStalkColimit no(spaces(),links(),bad,gg);},reason);};
    w=witness();w.section.set(0,0,2);fail(w,"right inverse");
    w=witness();w.quotient.set(0,2,2);fail(w,"interface equality");
    w=witness();w.relation_lift.set(0,2,0);fail(w,"kernel differs");
    w=witness();w.readouts[0].set(0,0,2);fail(w,"left inverse");
    w=witness();w.reconstruct_observations.set(0,0,0);fail(w,"reconstructible");
    w=witness();w.target.declaration.axes[0].coordinates={1,2};fail(w,"coordinate");
    ls=links();ls[2].sides[0].left_inverse=M::from_rows({{0,0}});rejects([&]{Guard gg;CertifiedStalkColimit no(spaces(),ls,witness(),gg);},"injective");
    for(const auto limits:std::vector<numeric::ExactActionLimits>{{7,16000000,64000000,1000},{4096,8,64000000,1000},{4096,16000000,1,1000},{4096,16000000,64000000,0}}){
        rejects([&]{Guard limited(limits);CertifiedStalkColimit no(spaces(),links(),witness(),limited);},"limit");
    }
    int calls=0;rejects([&]{Guard cancelled({},[&]{if(++calls==10)throw std::runtime_error("cancelled");});CertifiedStalkColimit no(spaces(),links(),witness(),cancelled);},"cancelled");
    // Square witness identities do not suffice: an inconsistent cycle may kill a
    // local direction even though each individual interface arrow is injective.
    std::vector<ColimitCoordinateSpace> ps{space("A",2),space("B",2),space("C",2)};
    const M e=M::from_rows({{1},{0}}),twice=M::from_rows({{2},{0}}),l=M::from_rows({{1,0}}),half=M::from_rows({{Q{1,2},0}});
    std::vector<ColimitLinearLink> cycle{{"AB",0,1,space("K",1),{{{e,l},{e,l}}}},{"BC",1,2,space("K",1),{{{e,l},{e,l}}}},{"CA",2,0,space("K",1),{{{e,l},{twice,half}}}}};
    M q(3,6),h(6,3),zcy(3,6);for(std::size_t i=0;i<3;++i){q.set(i,2*i+1,1);h.set(2*i+1,i,1);}
    const auto inverse=M::from_rows({{1,0,-2},{-1,1,0},{0,-1,1}}).inverse();for(const auto& [ij,v]:inverse.entries())zcy.set(ij.first,2*ij.second,v);
    std::vector<M> ro;for(std::size_t i=0;i<3;++i){M r(2,3);r.set(1,i,1);ro.push_back(r);}
    rejects([&]{Guard gg;CertifiedStalkColimit no(ps,cycle,{space("collapsed",3),q,h,zcy,ro,h.transpose()},gg);},"collapses a local direction");
}
void covariance(){
    // Independent finite matrix calculations are confined to test oracles.
    for(int seed=0;seed<40;++seed){
        Guard g;const auto w=witness();auto ww=w;auto ls=links();M d(8,8),inverse(8,8);
        std::array<M,4> local;for(std::size_t p=0;p<4;++p){const Q k=int((seed+3*p)%7)-3;local[p]=M::from_rows({{1,k},{0,1}});auto inv=M::from_rows({{1,-k},{0,1}});
            for(const auto& [ij,v]:local[p].entries())d.set(2*p+ij.first,2*p+ij.second,v);
            for(const auto& [ij,v]:inv.entries())inverse.set(2*p+ij.first,2*p+ij.second,v);
        }
        ww.quotient=w.quotient.multiply(inverse);ww.section=d.multiply(w.section);ww.relation_lift=w.relation_lift.multiply(inverse);ww.reconstruct_observations=w.reconstruct_observations.multiply(inverse);
        for(std::size_t p=0;p<4;++p)ww.readouts[p]=local[p].multiply(w.readouts[p]);
        for(auto& e:ls)for(std::size_t side=0;side<2;++side){auto p=side?e.right:e.left;e.sides[side].map=local[p].multiply(e.sides[side].map);e.sides[side].left_inverse=e.sides[side].left_inverse.multiply(local[p].inverse());}
        CertifiedStalkColimit c(spaces(),ls,ww,g);const S x(5,{{0,Q{2,3}},{1,-3},{3,5},{4,7}});std::vector<S> reads;
        for(std::size_t p=0;p<4;++p){reads.push_back(c.restrict(p,x,g));}
        check(eq(c.glue(reads,g),x),"nonorthogonal coordinate covariance");
    }
    std::array<std::size_t,4> order{0,1,2,3};do{
        Guard g;const auto old=witness();auto w=old;auto ls=links();auto ps=spaces();auto before=ps;std::array<std::size_t,4> inverse{};M p(8,8);
        for(std::size_t i=0;i<4;++i){ps[i]=before[order[i]];inverse[order[i]]=i;for(std::size_t j=0;j<2;++j)p.set(2*order[i]+j,2*i+j,1);w.readouts[i]=old.readouts[order[i]];}
        for(auto& e:ls){e.left=inverse[e.left];e.right=inverse[e.right];}
        w.quotient=old.quotient.multiply(p);w.section=p.transpose().multiply(old.section);w.relation_lift=old.relation_lift.multiply(p);w.reconstruct_observations=old.reconstruct_observations.multiply(p);
        CertifiedStalkColimit c(ps,ls,w,g);const S x(5,{{0,2},{1,3},{2,5},{3,7},{4,11}});std::vector<S> reads;for(std::size_t i=0;i<4;++i)reads.push_back(c.restrict(i,x,g));check(eq(c.glue(reads,g),x),"all 24 patch orderings with explicit correspondence");
    }while(std::next_permutation(order.begin(),order.end()));
}
int main(){try{primitive();covariance();
    const auto d=dataset();const auto p=plan_unsplit(d);const auto r=result_unsplit(p,d);
    check(r.total==730 && r.matches_reference==true,"unsplit result reaches native moment engine");
    const std::array<Q,4> self{26,250,980,726};const std::array<Q,6> cross{80,252,-220,0,-440,-924};
    for(std::size_t i=0;i<4;++i)check(r.components[i].self==self[i],"retained component moment");
    for(std::size_t i=0;i<6;++i)check(r.interactions[i].doubled==cross[i],"signed cross contribution");
    check(r.cross_sum==-1252 && r.diagonal_sum==1982,"interaction cancellation not omitted");
    Guard g;const auto& v=p.view();for(std::size_t part=0;part<4;++part){field::SparseStalkFieldActions f(v.patches()[part].fibers());
        for(std::size_t c=0;c<2;++c){const auto x=f.bind(g1,{{0,S::basis(2,c)}});const auto dx=f.boundary(x).field;
            check(eq(v.boundary(g1).apply(v.include_chain_field(g1,x,part,g),g),v.include_chain_field(g0,dx,part,g)),"complete local boundary inclusion");}}
    auto bad=d;bad.records[0].origins[0].local.carrier.coordinate_system+=1;rejects([&]{prepare_unsplit(bad);},"coordinates");
    bad=d;bad.records.push_back(bad.records.back());rejects([&]{prepare_unsplit(bad);},"duplicate colimit");
    bad=d;bad.records.pop_back();rejects([&]{prepare_unsplit(bad);},"missing complete");
    bad=d;for(auto& t:bad.locals[0].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto m=temporal_tensor_matrix(t);m.set(1,0,3);t.tensor=temporal_matrix_tensor(m);break;}
    set_field(bad.locals[0],g1,S(2));rejects([&]{prepare_unsplit(bad);},"whole unsplit interface descent");
    const auto extra=dataset(true);const auto ep=plan_unsplit(extra);check(ep.view().occurrence_count(g1)==6,"inactive present-zero and cancelling occurrences retained");check(result_unsplit(ep,extra).total==730,"zero inactive and cancellation preserve result");
    const auto hd=dataset(false,true);const auto hp=plan_unsplit(hd);check(hp.view().cell_count(g0)==3 && hp.view().cell_count(g1)==3 && hp.view().cell_count({2})==1,"whole triangle shared once");
    check(hp.view().dimension(g0)==15 && hp.view().dimension(g1)==15 && hp.view().dimension({2})==5,"unsplit higher-grade dimensions");
    for(int i=-2;i<=2;++i)for(int j=-2;j<=2;++j){Guard gg;S x(5,{{0,i},{4,j}});const auto y=hp.view().boundary({2}).apply(x,gg);check(hp.view().boundary(g1).apply(y,gg).nonzeros()==0,"second boundary exactly zero");}
    auto task=std::async(std::launch::async,[p,d]{return result_unsplit(p,d).total;});check(task.get()==730,"immutable prepared colimit concurrent invocation");
    std::cout<<"certified_colimit checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
