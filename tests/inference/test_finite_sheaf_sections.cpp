#include "../../examples/finite_sections_data.hpp"
#include <iostream>
#include <future>
using namespace finite_sections_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,"expected refusal");}
std::map<std::size_t,std::uint64_t> fixed(unsigned a){return {{0,a&1u},{1,(a>>1)&1u},{2,(a>>2)&1u}};}
int main(){try{
    const auto d=data();const auto all=prepare_family(d);const auto eq=prepare_family(d,policies(false));const auto bad=prepare_family(d,policies());
    for(unsigned a=0;a<8;++a){
        const auto any=all.query(availability(),fixed(a));check(any.status==Status::Satisfiable,"complete uncorrelated catalogue");
        const auto r=eq.query(availability(),fixed(a));check((r.status==Status::Satisfiable)==(a==0 || a==7),"joint family retains correlations");
        check(bad.query(availability(),fixed(a)).status==Status::Unsatisfiable,"three-clause conflict");
        const auto x=all.materialize(any,availability());check(x.carrier().size()==10,"actual glued observation dimension");
    }
    check(eq.query(availability(),{{0,0}}).status==Status::Satisfiable && eq.query(availability(),{{0,1}}).status==Status::Satisfiable,"both marginals possible without erasing correlation");
    for(std::size_t omit=0;omit<3;++omit){auto c=policies();c.erase(c.begin()+static_cast<std::ptrdiff_t>(omit));const auto p=prepare_family(d,c);check(p.query(availability()).status==Status::Satisfiable,"every pair feasible");}
    const auto core=bad.conflict(availability());check(core.status==Status::Unsatisfiable && core.subset_minimal && core.clauses.size()==3,"local-field-case-4 checked global conflict");
    for(std::size_t i=0;i<3;++i){const auto& w=core.deletion_witnesses[i];for(std::size_t j=0;j<3;++j)if(i!=j){const auto& c=bad.family().clauses()[core.clauses[j]].condition;bool found=false;for(const auto& t:c.allowed)if(t[0]==w[c.patches[0]] && t[1]==w[c.patches[1]])found=true;check(found,"deletion witness validates all retained other clauses");}}
    check(eq.conflict(availability()).status==Status::Satisfiable,"satisfiable family has no conflict");
    // Exhaust all binary relations on the three pairs (including empty ones).
    // Independent truth check on all 8 whole assignments, not pairwise reasoning.
    for(unsigned mask=0;mask<256;++mask){std::vector<FiniteSheafCondition> c;for(std::size_t pair=0;pair<2;++pair){FiniteSheafCondition v{"c"+std::to_string(pair),{pair,pair+1},{}};for(unsigned k=0;k<4;++k)if(mask&(1u<<(4*pair+k)))v.allowed.push_back({k&1u,(k>>1)&1u});c.push_back(v);}
        const auto f=prepare_family(d,c);bool any=false;
        for(unsigned x=0;x<8;++x){const unsigned left=(x&1u)|((x>>1&1u)<<1),right=(x>>1&1u)|((x>>2&1u)<<1);const bool expected=(mask&(1u<<left)) && (mask&(1u<<(4+right)));any|=expected;check((f.query(availability(),fixed(x)).status==Status::Satisfiable)==expected,"finite family exhaustive independent reference");}
        check((f.query(availability()).status==Status::Satisfiable)==any,"existential exact reference");
    }
    // Interface agreement is derived from P, not equality of original arrays.
    const auto shared=data(true,true);const auto same=prepare_family(shared);
    for(unsigned a=0;a<8;++a)check((same.query(availability(),fixed(a)).status==Status::Satisfiable)==(a==0 || a==7),"actual nonorthogonal shared observation maps");
    const auto nonconstant=data(false);const auto sections=prepare_family(nonconstant);const auto cochains=prepare_family(nonconstant,{},false);
    for(unsigned a=0;a<8;++a){check(cochains.query(availability(),fixed(a)).status==Status::Satisfiable,"overlap-compatible cochains");check((sections.query(availability(),fixed(a)).status==Status::Satisfiable)==(a==0),"zero coboundary required separately");}
    check(sections.family().candidate_residuals()[0][1]->nonzeros()!=0,"actual rejected cochain residual retained");
    // Missing, empty, supported zero and nonzero observation are distinct.
    auto observed=d;auto reading=observation(observed,0,1,0);const auto z=prepare_family(observed,{},true,{reading});
    check(z.family().coverage()[0].observed==1 && z.family().coverage()[0].numerical_nonzeros==0,"supported zero coverage");
    for(unsigned a=0;a<8;++a)check((z.query(availability(),fixed(a)).status==Status::Satisfiable)==(!(a&1)),"zero observation filters without filling missing coordinates");
    auto empty=observed;for(auto& t:empty.maps.locals[0].endpoint.tensors)if(t.key==reading.record.tensor)t.tensor={t.tensor.domain(),numeric::StructuralSupport::sparse(2,{}),S(2)};
    for(auto& v:empty.maps.locals[0].endpoint.declarations->values)if(v.key==reading.record.value)v.value=DeclaredValue::tensor({numeric::IndexDomain::linear(2),numeric::StructuralSupport::sparse(2,{}),S(2)});
    const auto empty_reading=prepare_family(empty,{},true,{reading});check(empty_reading.family().coverage()[0].observed==0,"explicit empty coverage is not observed zero");
    for(unsigned a=0;a<8;++a)check(empty_reading.query(availability(),fixed(a)).status==Status::Satisfiable,"empty observed support does not fix a value");
    auto one=d;auto one_reading=observation(one,0,1,1);const auto o=prepare_family(one,{},true,{one_reading});check(o.query(availability(),{{0,0}}).status==Status::Unsatisfiable && o.query(availability(),{{0,1}}).status==Status::Satisfiable,"nonzero observation");
    auto impossible=d;auto two=observation(impossible,0,1,2);const auto no=prepare_family(impossible,{},true,{two});check(no.query(availability()).status==Status::Unsatisfiable && no.conflict(availability()).clauses.size()==1,"out-of-family reading not an arbitrary repaired value");
    // Resource refusal and cancellation never become conflicts or partial witnesses.
    for(std::size_t steps=0;steps<100;++steps){local::FiniteSearchLimits l;l.max_steps=steps;const auto q=bad.query(availability(),{},l);if(q.status==Status::BudgetExceeded)check(!q.witness && q.steps<=steps,"bounded query returns no witness");const auto c=bad.conflict(availability(),l);if(c.status==Status::BudgetExceeded)check(!c.subset_minimal && c.clauses.empty() && c.deletion_witnesses.empty() && c.steps<=steps,"bounded conflict has no partial certificate");}
    std::stop_source stop;stop.request_stop();check(all.query(availability(),{},{},stop.get_token()).status==Status::Cancelled,"query cancellation");check(bad.conflict(availability(),{},stop.get_token()).status==Status::Cancelled,"conflict cancellation");
    local::FiniteSearchLimits l;l.max_variables=2;check(all.query(availability(),{},l).status==Status::BudgetExceeded,"variable bound");
    rejects([&]{all.query(availability(),{{0,9}});});rejects([&]{all.query(availability(),{{99,0}});});
    rejects([&]{all.materialize(bad.query(availability()),availability());});rejects([&]{eq.materialize(all.query(availability()),availability());});
    auto pinned=eq.query(availability(),{{0,1}});check(pinned.fixed.at(0)==1,"fixed assumptions retained in query result");
    pinned.fixed[0]=0;rejects([&]{eq.materialize(pinned,availability());});
    runtime::ExecutionHostPolicy noalloc;noalloc.allow_allocation=false;rejects([&]{all.materialize(all.query(availability()),availability(),runtime::ExecutionLease(noalloc));});
    auto forged=eq.query(availability());forged.witness=std::vector<std::uint64_t>{0,1,0};rejects([&]{eq.materialize(forged,availability());});
    for(std::size_t i=0;i<availability().size();++i){auto avail=availability();avail[i].max_sequence=6;rejects([&]{all.query(avail);});rejects([&]{all.conflict(avail);});rejects([&]{all.materialize(all.query(availability()),avail);});}
    FiniteSheafFamilyLimits bounds;bounds.alternatives=5;rejects([&]{prepare_family(d,{},true,{},bounds);});bounds={};bounds.tuple_entries=1;rejects([&]{prepare_family(d,{},true,{},bounds);});bounds={};bounds.label_bytes=1;rejects([&]{prepare_family(d,{},true,{},bounds);});
    auto duplicate=d;duplicate.catalogues[0].alternatives[1].key=0;rejects([&]{prepare_family(duplicate);});
    auto missing=d;missing.catalogues[0].alternatives[0].fields.pop_back();rejects([&]{prepare_family(missing);});
    auto partial=observed;partial.catalogues[0].alternatives[0].fields[0]=reading.record;rejects([&]{prepare_family(partial);});
    std::vector<std::future<bool>> jobs;for(unsigned i=0;i<6;++i)jobs.push_back(std::async(std::launch::async,[&,i]{return eq.query(availability(),{{0,i%2}}).status==Status::Satisfiable && bad.conflict(availability()).subset_minimal;}));for(auto& j:jobs)check(j.get(),"independent concurrent queries");
    const auto copy=all;const auto witness=copy.query(availability());const auto shared_field=all.materialize(witness,availability());check(shared_field.carrier().size()==10,"copy shares immutable native owner");
    std::cout<<"finite_sheaf_sections checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
