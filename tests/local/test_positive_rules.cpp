#include "positive_rules_fixture.hpp"
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace positive_fixture;
using namespace rfc::local;
std::size_t checks{}, rejected{};
void check(bool b, const char* s) { ++checks; if (!b) throw std::runtime_error(s); }
template<class F> void reject(F fn) {
    ++checks; ++rejected;
    try { fn(); } catch (const std::logic_error&) { return; }
    throw std::runtime_error("expected native rejection");
}
unsigned mask_of(const PositiveClosure& c) {
    unsigned mask = 0; for (unsigned i=0;i<3;++i) if(c.contains(f(i))) mask |= 1u<<i; return mask;
}
void exhaustive() {
    const auto owner = source();
    std::cout << "{\"closures\":["; bool comma=false;
    for(unsigned m=0;m<512;++m) {
        const auto p = FinitePositiveProgram::compile(owner, universe(), rules(m));
        for(unsigned s=0;s<8;++s) {
            const auto run = p.run(seeds(s));
            check(run.status()==PositiveRunStatus::Complete && run.closure().has_value(),"closure incomplete");
            const auto& c=*run.closure(); c.require_for(p);
            const auto expected_mask=expected(m,s),actual=mask_of(c);
            check(actual==expected_mask,"least model mismatch");
            unsigned built=0;
            for(const auto& d:c.derivations()) {
                const auto bit=1u<<d.fact.index;
                check((built&bit)==0,"duplicate derived fact");
                if(!d.rule) check((s&bit)!=0,"unjustified seed");
                else {
                    const auto ri=d.rule->index;
                    check(((m>>ri)&1u)!=0 && (built&bodies[ri])==bodies[ri] && (heads[ri]&bit)!=0,"cyclic first derivation");
                }
                built|=bit;
            }
            check(built==actual,"proof misses result");
            std::set<Cell> enabled(c.enabled_rules().begin(),c.enabled_rules().end());
            check(enabled.size()==c.enabled_rules().size(),"duplicate rule justification");
            for(unsigned i=0;i<9;++i)check(enabled.contains(r(i))==(((m>>i)&1u) && (actual&bodies[i])==bodies[i]),"closure justification coverage");
            check(run.receipt().verified_rules==p.rules().size(),"not all rules checked");
            check(run.receipt().verified_derivations==c.size(),"not all derivations checked");
            check(run.receipt().generated_global_tables==0,"assignment universe generated");
            if(comma)std::cout<<',';
            comma=true;
            std::cout<<'['<<m<<','<<s<<','<<actual<<','<<enabled.size()<<']';
        }
    }
    std::cout<<"]}"<<'\n';
}
void edge_and_budget() {
    const auto owner=source();
    const auto cycle=FinitePositiveProgram::compile(owner,universe(),rules(3));
    const auto empty=cycle.run({}); check(empty.closure()->size()==0,"unseeded cycle created facts");
    const auto one=cycle.run(seeds(1)); check(mask_of(*one.closure())==3,"seeded cycle incorrect");
    check(one.closure()->enabled_rules().size()==2,"cyclic justification lost");
    auto repeated=seeds(1);repeated.push_back(f(0));
    const auto duplicates=cycle.run(repeated);
    check(duplicates.closure()->seeds().size()==2 && duplicates.receipt().unique_seeds==1,"seed provenance erased");
    const auto p=FinitePositiveProgram::compile(owner,universe(),rules(511));
    const auto base=p.run(seeds(1));
    for(std::size_t s=0;s<base.receipt().steps;++s) {
        auto limits=PositiveRunLimits{};limits.max_steps=s;
        const auto failed=p.run(seeds(1),limits);
        check(failed.status()==PositiveRunStatus::BudgetExceeded && !failed.closure(),"partial fixed-point claim");
    }
    for(std::size_t s=0;s<p.receipt().steps;++s) {
        auto limits=PositiveCompileLimits{};limits.max_steps=s;
        reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),rules(511),limits);});
    }
    for(std::size_t s=0;s<3;++s) {
        auto limits=PositiveRunLimits{};limits.max_derived_facts=s;
        const auto limited=p.run(seeds(1),limits);
        check(!limited.closure(),"derived fact limit ignored");
    }
    for(std::size_t s=0;s<9;++s) {
        auto limits=PositiveRunLimits{};limits.max_justifications=s;
        const auto limited=p.run(seeds(1),limits);
        check(!limited.closure(),"justification limit ignored");
    }
    auto sl=PositiveRunLimits{};sl.max_seed_records=0;
    const auto limited=p.run(seeds(1),sl);
    check(!limited.closure(),"seed limit ignored");
    std::stop_source stop;stop.request_stop();
    check(p.run({}, {},stop.get_token()).status()==PositiveRunStatus::Cancelled,"stop ignored");
    try {(void)FinitePositiveProgram::compile(owner,universe(),rules(3),{},stop.get_token());throw std::runtime_error("prepare stop ignored");}
    catch(const PositivePreparationCancelled&){++checks;}
    reject([&]{(void)FinitePositiveProgram::compile(nullptr,universe(),{});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,{f(0),f(0)},{});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,{f(999)},{});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{{r(99),{},f(0)}});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{{f(0),{},f(0)}});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{{r(0),{},f(0)},{r(0),{},f(1)}});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{{r(0),{f(99)},f(0)}});});
    reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{{r(0),{},f(99)}});});
    reject([&]{const std::array bad{f(99)};(void)p.run(bad);});
    reject([&]{(void)base.closure()->contains(f(99));});
    const auto equal=FinitePositiveProgram::compile(owner,universe(),rules(511));
    reject([&]{base.closure()->require_for(equal);});
    const auto copy=p; base.closure()->require_for(copy);++checks;
    const auto nothing=FinitePositiveProgram::compile(owner,{},{});const auto nil=nothing.run({});check(nil.closure()->size()==0,"empty program");
    auto l=PositiveCompileLimits{};l.max_facts=2;reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),{},l);});
    l={};l.max_rules=1;reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),rules(3),l);});
    l={};l.max_premises=0;reject([&]{(void)FinitePositiveProgram::compile(owner,universe(),rules(1),l);});
    const auto saved=[] { const auto p=FinitePositiveProgram::compile(source(),universe(),rules(3));const auto v=p.run(seeds(1));return *v.closure(); }();
    check(mask_of(saved)==3 && saved.source().grade({1}).cell_count()==9,"owner lifetime lost");
}
void long_chain() {
    const auto owner=source(130);
    std::vector<Cell> facts;
    for(unsigned i=0;i<132;++i)facts.push_back(f(i));
    std::vector<PositiveRule> rs;
    for(unsigned i=0;i<130;++i)rs.push_back({r(9+i),{f(i)},f(i+1)});
    rs.push_back({r(0),{f(130)},f(0)});
    const auto p=FinitePositiveProgram::compile(owner,std::move(facts),std::move(rs));
    const auto no=p.run({});check(no.closure()->size()==0,"long cycle bootstrapped itself");
    const auto yes=p.run(seeds(1));check(yes.closure()->size()==131,"long closure truncated");
    check(!yes.closure()->contains(f(131)),"unconnected atom invented");
    check(yes.receipt().processed_facts==131 && yes.receipt().rule_firings==131,"long worklist incomplete");
    for(unsigned i=0;i<131;++i)check(yes.closure()->contains(f(i)),"long chain missing fact");
}
void scale_and_order() {
    PositiveCompileReceipt ref{};PositiveRunReceipt runref{};
    for(unsigned extra:{0u,257u,4097u}) {
        auto owner=source(extra);const auto p=FinitePositiveProgram::compile(owner,universe(),rules(511));const auto run=p.run(seeds(1));
        check(mask_of(*run.closure())==7,"padded source changed closure");
        check(owner->grade({0}).cell_count()==3+extra,"source identities compressed");
        check(owner->grade({0}).identities[0]==1001,"source key changed");
        if(extra==0){ref=p.receipt();runref=run.receipt();}
        else{check(p.receipt().steps==ref.steps && p.receipt().watch_entries==ref.watch_entries,"preparation uses unused source");
            check(run.receipt().steps==runref.steps,"execution uses unused source");}
    }
    std::array<unsigned,3> perm{0,1,2};
    do {
        auto rs=rules(7);std::vector<PositiveRule> shuffled;
        for(auto i:perm)shuffled.push_back(rs[i]);
        auto fs=universe();std::reverse(fs.begin(),fs.end());
        const auto p=FinitePositiveProgram::compile(source(),std::move(fs),std::move(shuffled));
        const auto v=p.run(seeds(1));check(mask_of(*v.closure())==7,"ordering changed least closure");
    } while(std::next_permutation(perm.begin(),perm.end()));
}
}
int main(){try{exhaustive();edge_and_budget();scale_and_order();long_chain();
    std::cout<<"positive_rule_checks="<<checks<<" rejection_checks="<<rejected<<" exhaustive_cases=4096\n";
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
