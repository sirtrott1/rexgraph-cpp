#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include <iostream>

namespace {
using namespace rfc;
std::size_t checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
core::RelationalComplex vertices(std::size_t n){
    core::RelationSignature signature;core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<n;++i)builder.declare_witness_identity(i+100);
    return builder.freeze();
}
using System=local::FiniteConstraintSystem<int>;
void exhaustive(){
    const auto source=vertices(3);std::cout<<"{\"cases\":[";bool comma=false;
    for(unsigned a=1;a<16;++a)for(unsigned b=1;b<16;++b)for(unsigned c=1;c<16;++c){
        System system(source);for(std::uint32_t i=0;i<3;++i)system.set_boolean_domain({{0},i});
        const unsigned tables[3]={a,b,c};
        for(std::uint32_t i=0;i<3;++i){
            const core::CellId x{{0},i},y{{0},(i+1)%3};
            system.add_boolean_truth_chart("local",join(system.carrier().cell_patch(x),system.carrier().cell_patch(y)),{x,y},tables[i]);
        }
        unsigned valid=0;
        for(unsigned bits=0;bits<8;++bits){
            bool good=true;for(unsigned i=0;i<3;++i){const unsigned row=((bits>>i)&1u)|(((bits>>((i+1)%3))&1u)<<1u);good=good&&(((tables[i]>>row)&1u)!=0);}
            if(good)valid|=1u<<bits;
        }
        const auto found=system.bounded_global_section();
        check((found.status==local::FiniteSearchStatus::Satisfiable)==(valid!=0),"bounded solver differs from exhaustive reference");
        check(found.witness.has_value()==(valid!=0),"witness status mismatch");
        if(found.witness)check(system.globally_valid(*found.witness),"native predicate rejects witness");
        check(system.global_section().has_value()==found.witness.has_value(),"legacy solver changed");
        // Pin the first variable but keep all three constraints.
        const auto fixed=system.bounded_global_section({{{{0},0},1}});
        check((fixed.status==local::FiniteSearchStatus::Satisfiable)==((valid&0xaa)!=0),"fixed-value solver differs");
        if(comma)std::cout<<',';
        comma=true;std::cout<<'['<<a<<','<<b<<','<<c<<','<<(found.witness?1:0)<<','<<(fixed.witness?1:0)<<']';
    }
    std::cout<<"]";
}
struct ObservedValue {
    int value{};
    static inline std::stop_source* stop=nullptr;
    static inline unsigned comparisons=0;
    friend bool operator==(const ObservedValue& a,const ObservedValue& b){
        if(stop && ++comparisons==2)stop->request_stop();
        return a.value==b.value;
    }
};
void controls(){
    const auto source=vertices(3);System system(source);
    for(std::uint32_t i=0;i<3;++i)system.set_boolean_domain({{0},i});
    auto patch=join(system.carrier().cell_patch({{0},0}),system.carrier().cell_patch({{0},1}));
    system.add_boolean_truth_chart("equality",patch,{{{0},0},{{0},1}},9);
    auto result=system.bounded_global_section();
    for(std::size_t steps=0;steps<result.work.steps;++steps){
        const auto limited=system.bounded_global_section({}, {3,steps});
        check(limited.status==local::FiniteSearchStatus::BudgetExceeded&&!limited.witness,"interrupted search claimed completion");
        check(limited.work.steps<=steps,"search exceeded step limit");
    }
    auto limited=system.bounded_global_section({}, {2,100000});check(limited.status==local::FiniteSearchStatus::BudgetExceeded,"variable budget ignored");
    std::stop_source stop;stop.request_stop();check(system.bounded_global_section({}, {},stop.get_token()).status==local::FiniteSearchStatus::Cancelled,"pre-cancel failed");
    bool rejected=false;try{(void)system.bounded_global_section({{{{0},0},2}});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"out-of-domain fixed value accepted");
    local::FiniteConstraintSystem<ObservedValue> observed(source);
    observed.set_domain({{0},0},{{0},{1}});observed.set_domain({{0},1},{{0},{1}});
    observed.add_chart("same",patch,{{{0},0},{{0},1}},{{{0},{0}},{{1},{1}}});
    std::stop_source during;ObservedValue::comparisons=0;ObservedValue::stop=&during;
    const auto cancelled=observed.bounded_global_section({}, {},during.get_token());ObservedValue::stop=nullptr;
    check(cancelled.status==local::FiniteSearchStatus::Cancelled && !cancelled.witness,"mid-predicate cancellation was ignored");
    // More than a machine word of variables. The native assignment is one
    // witness, not an integer-indexed universe or an enumerated truth table.
    const auto many=vertices(130);System large(many);
    for(std::uint32_t i=0;i<130;++i)large.set_boolean_domain({{0},i});
    large.add_chart("last",large.carrier().cell_patch({{0},129}),{{{0},129}},{{1}});
    const auto l=large.bounded_global_section();check(l.witness&&l.witness->size()==130&&l.witness->at({{0},129})==1,"large factor domain rejected");
    check(l.work.value_trials<=131 && l.work.steps<20000,"large domain enumerated a universe");
    large.add_chart("opposite",large.carrier().cell_patch({{0},129}),{{{0},129}},{{0}});
    const auto conflict=large.bounded_global_section();check(conflict.status==local::FiniteSearchStatus::Unsatisfiable,"unqueried factor contradiction ignored");
    for(std::size_t b=0;b<conflict.work.steps;++b){const auto x=large.bounded_global_section({}, {130,b});check(x.status==local::FiniteSearchStatus::BudgetExceeded && !x.witness,"partial rejection called unsat");}
}
}
int main(){try{exhaustive();controls();std::cout<<",\"checks\":"<<checks<<",\"passed\":true}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
