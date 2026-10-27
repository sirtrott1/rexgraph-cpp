#include "../../examples/sparse_transport_data.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include "rfc/field/sparse_stalk_actions.hpp"
#include <future>
#include <limits>
#include <iostream>
using namespace sparse_transport_example;
namespace {
std::size_t checks{};
void check(bool c,const char* why){++checks;if(!c)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,why);}
void parity() {
 const std::size_t n=3;const auto l=limits(n);Guard g(l);const auto m=model(n,g);Fit base(m,{},g);
 for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b) {
    V input{Q{a,3},Q{b,5},0};for(auto& q:input)q.canonicalize();
    V theta{Q{3,5},Q{4,5},Q{-4,5},Q{3,5},7,9};
    Guard dg(l);auto fixed=m.fixed_response(input,dg).boundary;
    auto target=numeric::checked_apply(m.observation_map(input,true,dg),theta,dg);
    for(std::size_t i=0;i<target.size();++i)target[i]+=fixed[i];
    for(const std::vector<std::size_t>& mask:{std::vector<std::size_t>{1,0},std::vector<std::size_t>{0,1},std::vector<std::size_t>{1,1}}) {
      auto dense_target=target;std::vector<learning::SparseTransportTarget> ts;
      for(std::size_t c=0;c<2;++c)for(std::size_t i=0;i<n;++i)if(mask[c])ts.push_back({c,i,target[c*n+i]});else dense_target[c*n+i]=0;
      std::vector<numeric::SparseRationalEntry> es;for(std::size_t i=0;i<n;++i)if(input[i]!=0)es.push_back({i,input[i]});
      const Sample sample{S(n,es),ts};const learning::TransportObservation dense{input,dense_target,mask};
      Guard a1(l),a2(l);const auto ds=m.assemble({dense},a1);const auto ss=m.assemble_sparse(std::span<const Sample>(&sample,1),a2);
      check(ds.design==ss.design && ds.augmented==ss.augmented && ds.target==ss.target,"dense/sparse row equations differ");
      Guard f1(l),f2(l);const auto d=base.append({dense},f1);const auto s=base.append_sparse({sample},f2);
      check(d.particular()==s.particular() && d.kernel()==s.kernel(),"dense/sparse complete family differs");
      check(d.rank()==s.rank() && d.free_columns()==s.free_columns(),"dense/sparse original parameter axes differ");
    }
 }
 // A nontrivial parameter basis retains dependent and unused named directions.
 M basis(6,3);basis.set(0,0,1);basis.set(0,1,2);basis.set(1,0,3);basis.set(4,2,1);
 Guard bg(l);auto tied=learning::PreparedTransportObservations::at_grade(m.owner(),core::Grade{1},m.fixed_coordinates(),m.variable_coordinates(),bg,basis);
 Sample sample{S::basis(n,0),{{1,0,5},{1,1,15},{1,2,0}}};
 learning::TransportObservation dense{{1,0,0},{0,0,0,5,15,0},{0,1}};
 Guard sg(l),dg(l);check(tied.assemble_sparse(std::span<const Sample>(&sample,1),sg).design==tied.assemble({dense},dg).design,"basis routing differs");
}
learning::PreparedTransportObservations completion(Guard& g) {
 core::RelationSignature sig;auto t=sig.add_type("boundary",core::BoundaryPolicy::DeclaredExact,0,16);
 core::RelationalComplexBuilder b(sig);
 b.add_grade_one(t,{{10,true,true,1},{20,true,false,1}});b.add_grade_one(t,{{20,true,true,1},{30,true,false,1}});
 b.add_grade_one(t,{{10,true,true,1},{30,true,false,1}});
 for(int i=0;i<2;++i)b.add_higher(core::Grade{2},t,{{0,true,false,1},{1,true,false,1},{2,true,true,1},{1,true,false,1},{1,true,true,1}});
 b.add_higher(core::Grade{3},t,{{0,true,false,1},{1,true,true,1}});
 const auto c=b.freeze();local::LinearStalkBundle sb(c,1);local::LinearRelationalFiberSystem f(sb);
 std::vector<learning::TransportCoordinate> fixed,vars;
 for(std::size_t k=1;k<c.grade_count();++k){const core::Grade gr{static_cast<std::uint16_t>(k)};
 for(std::size_t p=0;p<c.grade(gr).incidence_count();++p){M map(1,1);
   if(k==2){if(p==0){map.set(0,0,1);fixed.push_back({p,0,0});}else vars.push_back({p,0,0});}
   else map.set(0,0,c.incidence_coefficient(gr,p));
   f.set_transport(gr,p,std::move(map));}}
 return learning::PreparedTransportObservations::at_grade(local::OwnedLinearFiberSystem(f),core::Grade{2},fixed,vars,g);
}
void chain() {
 Guard g;Fit base(completion(g),{},g);check(!numeric::is_zero(base.chain_target()),"affine fixed defect lost");
 bool below=false,above=false;for(auto c:base.chain_coordinates()){below|=c.grade==2;above|=c.grade==3;}
 check(below && above,"both neighboring chain equations required");
 const Sample obs{S::basis(2,0),{{0,0,1},{1,0,1},{2,0,-1}}};Guard a;auto fit=base.append_sparse({obs},a);
 check(fit.kernel().cols()==4,"cancelling original occurrence freedoms lost");
 for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y){Guard h;V z(fit.kernel().cols());z[0]=x;z[1]=y;
   const auto own=fit.materialize(z,h);const auto& f=own.fibers();std::optional<M> prev;
   for(std::size_t k=1;k<f.complex().grade_count();++k){const M b=local::GradeTensorField(f,core::Grade{static_cast<std::uint16_t>(k)}).matrix();
     if(prev)check(prev->multiply(b).nonzeros()==0,"materialized adjacent chain failed");
     prev=b;}
 }
 auto bad=obs;bad.targets[1].value=2;rejects([&]{Guard h;(void)fit.append_sparse({bad},h);},"chain-conflicting observation accepted");
 check(base.sparse_observations().empty(),"append mutated base");
}
void sparse_cases() {
 std::optional<std::vector<std::size_t>> counters;
 for(const std::size_t n:{7ULL,1000003ULL,1000000000000000000ULL}){
   const auto l=limits(n);Guard g(l);Fit base(model(n,g),{},g);const auto obs=samples(n);
   Guard oneg(l);auto one=base.append_sparse({obs[0]},oneg);Guard twog(l);auto two=one.append_sparse({obs[1]},twog);
   check(base.kernel().cols()==6 && one.kernel().cols()==4 && two.kernel().cols()==2,"sparse dimensions differ");
   check(two.free_columns()==std::vector<std::size_t>({4,5}),"zero/absent occurrence parameters deleted");
   check(two.particular()==V({Q{3,5},Q{4,5},Q{-4,5},Q{3,5},0,0}),"rotation not learned");
   check(&two.chain()==&base.chain() && &two.model()==&base.model(),"incremental fixed owners not shared");
   check(two.update().chain_reused && two.update().reused_observations==1 && two.update().assembled_observations==1,"incremental accounting wrong");
   Guard allg(l);auto all=base.append_sparse(obs,allg);
   check(all.design()==two.design() && all.kernel()==two.kernel() && all.preparation_steps()==two.preparation_steps(),"grouped replay differs");
   const std::vector<std::size_t> c{g.steps(),oneg.steps(),twog.steps(),two.retained_entries()};
   if(counters)check(*counters==c,"work grew with unused coordinates");else counters=c;
   Sample none{S::basis(n,0),{}};Guard eg(l);auto empty=base.append_sparse({none},eg);
   check(empty.kernel()==base.kernel() && empty.sparse_observations().size()==1,"empty coverage became zero observations or disappeared");
   Sample zero{S::basis(n,0),{{1,0,0}}};Guard zg(l);auto zeros=base.append_sparse({zero},zg);
   check(zeros.kernel().cols()==5,"supplied zero did not constrain coordinate");
   auto bad=zero;bad.input=S(n);bad.targets[0].value=1;
   rejects([&]{Guard h(l);(void)base.append_sparse({bad},h);},"zero input nonzero target accepted");
   bad=zero;bad.targets.push_back(bad.targets[0]);rejects([&]{Guard h(l);(void)base.append_sparse({bad},h);},"duplicate target accepted");
   bad=zero;bad.targets[0].cell=std::numeric_limits<std::size_t>::max();rejects([&]{Guard h(l);(void)base.append_sparse({bad},h);},"overflow target cell accepted");
   bad=zero;bad.targets[0].axis=n;rejects([&]{Guard h(l);(void)base.append_sparse({bad},h);},"outside target accepted");
   bad=zero;bad.input=S(n+1);rejects([&]{Guard h(l);(void)base.append_sparse({bad},h);},"wrong input extent accepted");
   rejects([&]{Guard h(l,[]{throw std::runtime_error("cancelled");});(void)two.append_sparse({zero},h);},"cancellation ignored");
   check(two.sparse_observations().size()==2,"failure mutated previous evidence");
   rejects([&]{(void)two.observations();},"sparse evidence silently exposed as empty dense history");
   rejects([&]{Guard h(l);(void)base.append_sparse({},h);},"empty suffix accepted");
   auto over=obs[0];over.targets[0].value=Q{numeric::Integer{1}<<8192};
   rejects([&]{Guard h(l);(void)base.append_sparse({over},h);},"target rational bit budget ignored");
   auto disorder=obs[0];std::reverse(disorder.targets.begin(),disorder.targets.end());
   rejects([&]{Guard h(l);(void)base.append_sparse({disorder},h);},"unordered target coordinates accepted");
   Guard kg(l);numeric::PreparedExactFrames frames(two.design(),kg);
   check(frames.kernel()==two.kernel(),"complete kernel differs from independent frame primitive");

   rejects([&]{auto low=l;low.entries=1;Guard h(low);(void)model(n,h);},"entry budget ignored");
   if(n==7){
    std::vector<std::future<bool>> jobs;
    for(int i=0;i<3;++i)jobs.push_back(std::async(std::launch::async,[&]{Guard h(l);auto f=base.append_sparse(obs,h);return f.particular()==two.particular();}));
    for(auto& j:jobs)check(j.get(),"concurrent immutable append differs");
    rejects([&]{Guard h(l);(void)two.append({learning::TransportObservation{V(n),V(2*n),{1,1}}},h);},"mixed history implicitly converted");
   }
 }
}
}
int main(){try{parity();chain();sparse_cases();std::cout<<checks<<" sparse transport checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
