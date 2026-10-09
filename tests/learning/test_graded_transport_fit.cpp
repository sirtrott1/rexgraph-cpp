#include "rfc/core/builder.hpp"
#include "rfc/learning/graded_transport_fit.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Fit=learning::PreparedGradedTransportFit;
namespace {
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused,message);}
learning::PreparedTransportObservations fixture(std::size_t selected,Guard& guard,bool named=false) {
    core::RelationSignature signature;const auto type=signature.add_type("boundary",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(type,{{10,true,true,1},{20,true,false,1},{10,true,false,Q{1,2}},{10,true,true,Q{1,2}},{20,false,false,7},{30,true,false,0}});
    builder.add_grade_one(type,{{20,true,true,1},{30,true,false,1}});
    builder.add_grade_one(type,{{10,true,true,1},{30,true,false,1}});
    for(int i=0;i<2;++i)builder.add_higher(core::Grade{2},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    builder.add_higher(core::Grade{3},type,{{0,true,false,1},{1,true,true,1}});
    const auto source=builder.freeze();local::LinearStalkBundle stalks(source,2);stalks.set_dimension({core::Grade{0},0},3);
    local::LinearRelationalFiberSystem fibers(stalks);std::vector<learning::TransportCoordinate> fixed,variables;
    for(std::size_t k=1;k<source.grade_count();++k) {
        const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto& arena=source.grade(g);
        for(std::size_t p=0;p<arena.incidence_count();++p) {
            const auto r=stalks.dimension({core::Grade{static_cast<std::uint16_t>(k-1)},arena.lower_index[p]});
            M map(r,2);
            for(std::size_t i=0;i<r;++i)for(std::size_t j=0;j<2;++j) {
                if(k==selected)variables.push_back({p,i,j});
                else if(i==j)map.set(i,j,source.incidence_coefficient(g,p));
            }
            fibers.set_transport(g,p,std::move(map));
        }
    }
    auto owner=local::OwnedLinearFiberSystem(fibers);const auto g=core::Grade{static_cast<std::uint16_t>(selected)};
    if(named) {
        M basis(variables.size(),3);
        for(std::size_t i=0;i<variables.size();++i)if(variables[i].row==variables[i].column){basis.set(i,0,1);basis.set(i,1,2);}
        return learning::PreparedTransportObservations::at_grade(owner,g,fixed,variables,guard,basis);
    }
    return learning::PreparedTransportObservations::at_grade(owner,g,fixed,variables,guard);
}
void oracle(const Fit& fit) {
    Guard guard;numeric::PreparedExactFrames frames(fit.design(),guard);
    check(frames.kernel()==fit.kernel() && frames.pivots()==fit.pivots() && frames.free_columns()==fit.free_columns(),"full original affine axes differ");
    check(numeric::checked_apply(fit.design(),fit.particular(),guard)==fit.target(),"particular solution misses evidence or chain law");
    check(numeric::checked_multiply(fit.design(),fit.kernel(),guard).nonzeros()==0,"free family leaves evidence or chain law");
    for(int v=-3;v<=3;++v) {
        V free(fit.free_columns().size(),Q{v,7});const auto owner=fit.materialize(free,guard);
        const auto& fibers=owner.fibers();std::optional<M> previous;
        for(std::size_t k=1;k<fibers.complex().grade_count();++k) {
            auto boundary=local::GradeTensorField(fibers,core::Grade{static_cast<std::uint16_t>(k)}).matrix();
            if(previous)check(numeric::checked_multiply(*previous,boundary,guard).nonzeros()==0,"materialized full chain family fails");
            previous=std::move(boundary);
        }
    }
}
void learning_cases() {
    for(std::size_t k=1;k<=3;++k)for(const bool named:{false,true}) {
        Guard guard;Fit empty(fixture(k,guard,named),{},guard);oracle(empty);
        check(empty.model().grade().value==k,"selected original grade lost");
        const auto& model=empty.model();const auto n=model.owner().fibers().stalks().total_dimension(model.grade());
        const auto lo=core::Grade{static_cast<std::uint16_t>(k-1)};
        const auto cells=model.owner().fibers().complex().grade(lo).cell_count();
        const auto truth=empty.parameters(V(empty.free_columns().size(),Q{2,3}),guard);
        std::vector<learning::TransportObservation> all;
        for(std::size_t j=0;j<n;++j) {
            V input(n);input[j]=Q{3,2};auto target=numeric::checked_apply(model.observation_map(input,true,guard),truth,guard);
            all.push_back({input,target,std::vector<std::size_t>(cells,1)});
            Fit fit(model,all,guard);oracle(fit);
            check(fit.observations().size()==j+1 && fit.kernel().rows()==model.parameter_count(),"original evidence or parameter carrier lost");
        }
        check(empty.observations().empty(),"parent mutated");
        if(named)check(empty.free_columns().size()==3,"dependent or unused named parameters discarded");
        auto contradictory=all;contradictory.push_back(all.front());contradictory.back().target[0]+=1;
        rejects([&]{Guard g;Fit fit(model,contradictory,g);},"inconsistent observed suffix accepted");
        rejects([&]{Guard g;(void)empty.materialize(V(empty.free_columns().size()+1),g);},"incomplete free parameter carrier accepted");
    }
}
void refusals() {
    Guard g;const auto model=fixture(2,g);Fit empty(model,{},g);
    auto limits=numeric::ExactActionLimits{};limits.steps=1;
    rejects([&]{Guard guard(limits);Fit fit(model,{},guard);},"step admission ignored");
    limits=numeric::ExactActionLimits{};limits.entries=1;
    rejects([&]{Guard guard(limits);Fit fit(model,{},guard);},"entry admission ignored");
    rejects([&]{Guard guard({},[]{throw std::runtime_error("cancelled");});Fit fit(model,{},guard);},"cancellation ignored");
    rejects([&]{Guard guard;(void)learning::PreparedTransportObservations::at_grade(model.owner(),core::Grade{0},{},{},guard);},"grade zero learned as a boundary");
    rejects([&]{Guard guard;learning::PreparedTransportObservations legacy(model.owner(),{},{},guard);},"legacy grade-one profile accepts higher grades");
    rejects([&]{model.require_grade_one_profile();},"graded assembler enters an incompatible grade-one consumer");
    std::vector<std::future<bool>> jobs;
    for(int i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[&]{Guard guard;const Fit fit(model,{},guard);return fit.kernel()==empty.kernel();}));
    for(auto& job:jobs)check(job.get(),"concurrent immutable fits differ");
}
}
int main(){try{learning_cases();refusals();std::cout<<checks<<" exact assertions passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
