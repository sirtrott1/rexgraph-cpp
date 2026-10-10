#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_evidence.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,text);}
learning::PreparedTransportObservations fixture(Guard& guard,bool tied=false) {
    core::RelationSignature signature;const auto type=signature.add_type("transport",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    builder.add_grade_one(type,{{0,true,true,Q{1,2}},{0,true,false,Q{1,3}},
        {1,false,false,Q{7}},{1,true,false,Q{0}},{1,true,false,Q{2}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);
    stalks.set_dimension({core::Grade{0},0},2);stalks.set_dimension({core::Grade{1},0},3);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<5;++p){M map(p<2?2:1,3);if(p==0)map.set(1,2,Q{-3,2});fibers.set_transport(core::Grade{1},p,std::move(map));}
    local::OwnedLinearFiberSystem owner(fibers);
    const std::vector<learning::TransportCoordinate> fixed{{0,1,2},{4,0,2}},variables{{0,0,0},{1,0,0},{1,1,1},{2,0,0},{3,0,1},{4,0,1}};
    if(tied){M basis(6,2);basis.set(0,0,2);basis.set(1,0,3);basis.set(2,1,Q{-1,2});basis.set(5,1,7);return {owner,fixed,variables,basis,guard};}
    return {owner,fixed,variables,guard};
}
numeric::SparseRationalTensor tensor(const V& field) {
    std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<field.size();++i)if(field[i]!=0)entries.push_back({i,field[i]});
    return {numeric::IndexDomain({field.size()}),numeric::StructuralSupport::full(field.size()),numeric::SparseRationalVector(field.size(),std::move(entries))};
}
void compare(const learning::TransportObservationSystem& a,const learning::TransportObservationSystem& b) {
    check(a.design==b.design && a.augmented==b.augmented && a.target==b.target,"column equations differ from whole-field oracle");
    check(a.constraints.size()==b.constraints.size(),"constraint count differs");
    for(std::size_t i=0;i<a.constraints.size();++i)check(a.constraints[i].sample==b.constraints[i].sample && a.constraints[i].cell==b.constraints[i].cell && a.constraints[i].axis==b.constraints[i].axis,"original constraint identity differs");
}
void families() {
    for(const bool tied:{false,true}) {
        Guard g;const auto model=fixture(g,tied);learning::PreparedColumnTransportObservations columns(model,g);
        learning::PreparedTransportEvidence empty(model,g),dense=empty,sparse=empty;
        for(long seed=-9;seed<=9;++seed) {
            const V input{Q{seed,5},Q{seed+1,7},Q{seed-1,11}};
            V weights(model.parameter_count());for(std::size_t i=0;i<weights.size();++i)weights[i]=Q{static_cast<long>(i+1),3};
            Guard oracle;auto target=numeric::checked_apply(model.observation_map(input,true,oracle),weights,oracle);
            const auto fixed=model.fixed_response(input,oracle);for(std::size_t i=0;i<target.size();++i)target[i]+=fixed.boundary[i];
            const bool second=seed%2==0;
            learning::TransportObservation full{input,{target[0],target[1],second?target[2]:Q{0}},{1,second?1u:0u}};
            std::vector<learning::TransportColumnField> targets{{0,tensor({target[0],target[1]})}};
            if(second)targets.push_back({1,tensor({target[2]})});
            const auto packet=columns.observation({{0,tensor(input)}},std::move(targets));
            compare(columns.assemble({packet},oracle),model.assemble({full},oracle));
            Guard dg,sg;dense=dense.append({full},dg);sparse=sparse.append_columns({packet},sg);
            compare(dense.system(),sparse.system());
            check(dense.particular()==sparse.particular() && dense.kernel()==sparse.kernel() && dense.pivots()==sparse.pivots() && dense.free_columns()==sparse.free_columns(),"joint affine family differs");
        }
        check(empty.observation_count()==0 && sparse.column_observation_count()==19,"immutable original column history differs");
        const auto fresh=sparse;std::vector<std::future<std::size_t>> cold_jobs;
        for(std::size_t i=0;i<4;++i)cold_jobs.push_back(std::async(std::launch::async,[fresh]{Guard local;(void)fresh.observations(local);return local.steps();}));
        const auto cold_steps=cold_jobs.front().get();
        for(std::size_t i=1;i<cold_jobs.size();++i)check(cold_jobs[i].get()==cold_steps,"concurrent first-use materialization accounting differs");
        Guard compat;const auto& whole=sparse.observations(compat);compare(model.assemble(whole,compat),sparse.system());
        auto jobs=std::vector<std::future<std::size_t>>{};
        for(std::size_t i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[sparse]{Guard local;return sparse.observations(local).size();}));
        for(auto& job:jobs)check(job.get()==19,"owned concurrent compatibility differs");
        Guard mix;const auto mixed=sparse.append({whole.front()},mix);check(mixed.column_observation_count()==19 && mixed.observation_count()==20,"mixed history lost sample order");
    }
}
void sparse_columns() {
    constexpr std::size_t count=1024;
    core::RelationSignature signature;const auto type=signature.add_type("column",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    for(std::size_t i=0;i<count;++i)builder.add_grade_one(type,{{0,true,true,Q{1}},{1,true,false,Q{1}}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source);local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<count*2;++p)fibers.set_transport(core::Grade{1},p,M(1,1));
    Guard g;learning::PreparedTransportObservations model(local::OwnedLinearFiberSystem(fibers),{},{{0,0,0},{1,0,0},{2,0,0},{3,0,0}},g);
    learning::PreparedColumnTransportObservations columns(model,g);
    const auto sample=columns.observation({{0,tensor({7})}},{{0,tensor({-14})},{1,tensor({21})}});
    V input(count);input[0]=7;const learning::TransportObservation whole{input,{-14,21},{1,1}};
    Guard dense,sparse;compare(model.assemble({whole},dense),columns.assemble({sample},sparse));
    check(dense.steps()>sparse.steps()*10,"column work still scans whole original input field");
    auto limits=numeric::ExactActionLimits{};limits.entries=columns.retained_entries()+512;
    Guard bounded(limits);(void)columns.assemble({sample},bounded);
    rejects([&]{Guard q(limits);(void)model.assemble({whole},q);},"whole-field expansion unexpectedly fits compact entry admission");
    const auto other=columns.observation({{1,tensor({7})}},{{0,tensor({-35})},{1,tensor({49})}});
    const auto second=columns.assemble({other},g);
    check(second.design.get(0,0)==0 && second.design.get(0,2)==-7,"equal boundary columns were identified");
    learning::PreparedTransportEvidence empty(model,g);const auto fitted=empty.append_columns({sample,other},g);
    check(fitted.particular()==V({2,3,5,7}) && fitted.kernel().cols()==0,"original columns lost independent learned maps");
    auto jobs=std::vector<std::future<M>>{};
    for(std::size_t i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[columns,sample]{Guard local;return columns.assemble({sample},local).design;}));
    for(auto& job:jobs)check(job.get().get(0,0)==-7,"concurrent original column assembly differs");
    std::cout<<"1024 original columns: dense steps "<<dense.steps()<<", sparse steps "<<sparse.steps()<<'\n';
}
void admission() {
    rejects([&]{Guard q;learning::PreparedColumnTransportObservations empty(std::shared_ptr<const learning::PreparedTransportObservations>{},q);},"missing original owner accepted");
    Guard g;const auto model=fixture(g);learning::PreparedColumnTransportObservations columns(model,g);
    const auto zero=columns.observation({},{{0,tensor({0,0})}});check(columns.assemble({zero},g).design.nonzeros()==0,"declared zero input changed equations");
    auto bad=zero;bad.target.clear();rejects([&]{Guard q;(void)columns.assemble({bad},q);},"empty targets accepted");
    bad=zero;const auto foreign=fixture(g);bad.owner=foreign.owner();rejects([&]{Guard q;(void)columns.assemble({bad},q);},"equal-shaped foreign owner accepted");
    bad=zero;bad.grade=core::Grade{2};rejects([&]{Guard q;(void)columns.assemble({bad},q);},"different grade accepted");
    bad=zero;bad.input={{0,tensor({1,2})}};rejects([&]{Guard q;(void)columns.assemble({bad},q);},"wrong original stalk shape accepted");
    bad=zero;bad.input={{0,tensor({1,0,0})},{0,tensor({0,1,0})}};rejects([&]{Guard q;(void)columns.assemble({bad},q);},"duplicate original column accepted");
    auto absent=numeric::SparseRationalTensor::zero(numeric::IndexDomain({3}),numeric::StructuralSupport::sparse(3,{0,1}));
    bad=zero;bad.input={{0,std::move(absent)}};rejects([&]{Guard q;(void)columns.assemble({bad},q);},"absence silently became known zero");
    bad=zero;bad.input={{0,tensor({8,0,0})}};auto limits=numeric::ExactActionLimits{};limits.integer_bits=2;
    rejects([&]{Guard q(limits);(void)columns.assemble({bad},q);},"coefficient bit admission ignored");
    limits={};limits.entries=columns.retained_entries();rejects([&]{Guard q(limits);(void)columns.assemble({zero},q);},"workspace admission ignored");
    rejects([&]{Guard q({},[]{throw std::runtime_error("cancelled");});(void)columns.assemble({zero},q);},"cancellation ignored");
    learning::PreparedTransportEvidence empty(model,g);auto first=empty.append_columns({zero},g);
    auto inconsistent=columns.observation({},{{0,tensor({1,0})}});
    rejects([&]{Guard q;(void)first.append_columns({inconsistent},q);},"contradictory target accepted");
    check(first.observation_count()==1 && first.system().target==V({0,0}),"failed append mutated parent");
}
}
int main(){try{families();sparse_columns();admission();std::cout<<checks<<" column transport checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
