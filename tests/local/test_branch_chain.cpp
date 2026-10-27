#include "branch_chain_fixtures.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/geometry/structural_certificate.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>

using namespace branch_chain_test;
namespace {
template<class T>concept ImplicitFloatField=requires(const Branch& b,Guard& g){b.field({T{0.5},T{0.5}},g);};
template<class T>concept ImplicitFloatSeed=requires(const Branch& b,Guard& g){b.from_initial_values({T{0.5},T{0.5}},g);};
template<class T>concept ImplicitFloatShares=requires(Guard& g){Branch::prepare({T{0.5},T{0.5}},vertices(3),relations(1),g);};
static_assert(!ImplicitFloatField<double> && !ImplicitFloatSeed<double> && !ImplicitFloatShares<double>);
static_assert(!ImplicitFloatField<float> && !ImplicitFloatSeed<float> && !ImplicitFloatShares<float>);
static_assert(!ImplicitFloatField<long double> && !ImplicitFloatSeed<long double> && !ImplicitFloatShares<long double>);
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
template<class E,class C>void rejects(C call,const char* why){bool caught=false;try{call();}catch(const E&){caught=true;}check(caught,why);}
void basics() {
    static_assert(!std::is_default_constructible_v<Branch>);
    static_assert(!std::is_default_constructible_v<Branch::Field>);
    static_assert(!std::is_default_constructible_v<Branch::ModeFamily>);
    for(const auto& shares:std::vector<V>{{1},{Q{1,2},Q{1,2}},{Q{1,4},Q{3,4}},
        {Q{1,3},Q{1,3},Q{1,3}},{Q{1,10},Q{2,10},Q{3,10},Q{4,10}},{Q{1,3},Q{5,9},Q{1,9}}}) {
        for(const std::size_t n:{1,2,10}) {
            Guard g;const auto b=prepare(shares,n,g);const auto v=n+shares.size();
            check(b.source().grade_count()==2 && b.source_rank()==n && b.nullity()==shares.size(),"branch dimension certificate differs");
            check(b.source().grade(core::Grade{0}).identities==vertices(v),"vertex order/identity changed");
            check(b.source().grade(core::Grade{1}).relation_identities==relations(n),"primary relation keys changed");
            check(b.source().grade(core::Grade{1}).incidence_count()==n*(shares.size()+1),"source occurrence count differs");
            for(std::size_t i=0;i<n;++i) {
                const core::BranchingRelationView view(b.source(),core::Grade{1},i);
                check(view.head().lower.index==i && view.tail_shares()==numeric::canonical_vector(shares),"original head/shares differ");
            }
            const field::BoundaryOperator boundary(b.source(),core::Grade{1});
            const auto cert=geometry::ChainStructureCertificate::build(b.source());
            check(cert.hodge_zero_multiplicity(core::Grade{0})==shares.size(),"native full-source nullity differs");
            for(std::size_t j=0;j<shares.size();++j) {
                V initial(shares.size());initial[j]=1;
                const auto f=b.from_initial_values(initial,g);
                check(b.initial_values(f,g)==initial,"initial recurrence coordinate roundtrip failed");
                check(numeric::is_zero(boundary.transpose_apply(f.coordinates())),"native original source residual is nonzero");
                for(std::size_t i=0;i<v;++i)check(f.coordinates()[i]==b.basis().get(i,j),"extension differs from retained frame");
            }
            const auto constants=b.from_initial_values(V(shares.size(),1),g);
            check(constants.coordinates()==V(v,1),"constant mode lost");
        }
    }
    std::cout<<"branch-recurrence-T01 explicit source identities, positive shares, full rational recurrence basis passed\n";
    std::cout<<"branch-recurrence-T02 full native boundary and Hodge/nullity cross-checks passed\n";
}
void modes() {
    Guard g;const auto rationals=F::declare_cyclotomic(1);
    for(const auto& row:std::vector<std::pair<V,Q>>{{{Q{1,2},Q{1,2}},Q{-2}},{{Q{1,4},Q{3,4}},Q{-4,3}}}) {
        const auto b=prepare(row.first,10,g);
        const auto full=b.declared_modes(rationals,{rationals.scalar(1),rationals.scalar(row.second)},g);
        full.require_complete();full.require_source(b);
        check(full.values().rows()==12 && full.values().cols()==2,"geometric carrier shape differs");
        const auto partial=b.declared_modes(rationals,{rationals.scalar(1)},g);
        check(!partial.complete(),"partial mode family marked complete");
        rejects<std::domain_error>([&]{partial.require_complete();},"partial mode family obtained complete authority");
        V wrong(12);Q p=1;for(auto& x:wrong){x=p;p*=2;}
        const auto f=b.field(wrong,g);
        check(!numeric::is_zero(b.annihilator_residual(f,g)),"2^i incorrectly treated as a mode");
        rejects<std::domain_error>([&]{(void)b.initial_values(f,g);},"nonkernel field obtained recurrence coordinates");
    }
    const auto b=prepare({Q{1,3},Q{1,3},Q{1,3}},10,g);
    const auto field=quadratic();const auto one=field.scalar(1),z=field.generator();
    const auto full=b.declared_modes(field,{one,-one+z,-one-z},g);
    check(full.complete() && full.values().cols()==3 && b.nullity()==3,"arity4 complex modes did not span the rational kernel extension");
    const auto constants=b.declared_modes(field,{one},g);
    check(!constants.complete(),"constant-only arity4 family became complete");
    std::cout<<"branch-recurrence-T03 rational and nonreal declared roots; only full independent families complete\n";
    const auto repeated=prepare({Q{1,3},Q{5,9},Q{1,9}},10,g);
    const auto partial=repeated.declared_modes(rationals,{rationals.scalar(1),rationals.scalar(-3)},g);
    check(!partial.complete() && repeated.nullity()==3,"repeated root lost a rational kernel direction");
    rejects<std::domain_error>([&]{partial.require_complete();},"ordinary repeated-root modes claimed completeness");
    rejects<std::invalid_argument>([&]{(void)repeated.declared_modes(rationals,{rationals.scalar(1),rationals.scalar(-3),rationals.scalar(-3)},g);},"duplicate ratio fabricated a new mode");
    std::cout<<"branch-recurrence-T04 repeated-root control preserves complete rational basis and partial modal status\n";
}
void refusals() {
    Guard g;
    for(const V& bad:std::vector<V>{{},{0,1},{-1,2},{Q{1,2},Q{1,3}},{2}})
        rejects<std::invalid_argument>([&]{(void)prepare(bad,2,g);},"invalid shares admitted");
    rejects<std::invalid_argument>([&]{(void)prepare({1},0,g);},"zero-relation root theorem claimed");
    rejects<std::invalid_argument>([&]{(void)Branch::prepare({1},{1,1},relations(1),g);},"duplicate vertex key admitted");
    rejects<std::invalid_argument>([&]{(void)Branch::prepare({1},{1,2,3},{core::RelationKey{1},core::RelationKey{1}},g);},"duplicate relation key admitted");
    rejects<std::invalid_argument>([&]{(void)Branch::prepare({1},{1,2},{core::RelationKey{0}},g);},"zero relation key admitted");
    rejects<std::invalid_argument>([&]{(void)Branch::prepare({1},{1,2,3},relations(1),g);},"wrong vertex count admitted");
    std::cout<<"branch-recurrence-T05 invalid shares, source dimensions and identities refused\n";
    const auto b=prepare({Q{1,2},Q{1,2}},4,g),foreign=prepare({Q{1,2},Q{1,2}},4,g);
    const auto field=F::declare_cyclotomic(1),alien=F::declare_cyclotomic(1);
    rejects<std::invalid_argument>([&]{(void)b.declared_modes(field,{field.scalar(2)},g);},"nonroot mode admitted");
    rejects<std::invalid_argument>([&]{(void)b.declared_modes(field,{field.scalar(0)},g);},"zero ratio admitted");
    rejects<std::invalid_argument>([&]{(void)b.declared_modes(field,{alien.scalar(1)},g);},"equal-valued foreign root admitted");
    rejects<std::invalid_argument>([&]{(void)b.declared_modes(field,{},g);},"empty mode family admitted");
    rejects<std::invalid_argument>([&]{(void)b.declared_modes(field,{field.scalar(1),field.scalar(-2),field.scalar(-2)},g);},"too many modes admitted");
    const auto family=b.declared_modes(field,{field.scalar(1),field.scalar(-2)},g);
    rejects<std::invalid_argument>([&]{family.require_source(foreign);},"equal source keys identified a foreign recipe");
    const auto value=b.from_initial_values({1,2},g);
    rejects<std::invalid_argument>([&]{(void)foreign.annihilator_residual(value,g);},"foreign branch field accepted");
    rejects<std::invalid_argument>([&]{(void)b.field({1},g);},"wrong full field carrier admitted");
    rejects<std::invalid_argument>([&]{(void)b.from_initial_values({1},g);},"incomplete initial coordinates admitted");
    const auto pinned=[&]{Guard a;auto branch=prepare({Q{1,3},Q{1,3},Q{1,3}},3,a);auto f=quadratic();return branch.declared_modes(f,{f.scalar(1)},a);}();
    check(pinned.values().rows()==6 && pinned.roots()[0]==pinned.field().scalar(1),"destroyed recipe/field wrappers invalidated pinned modes");
    std::cout<<"branch-recurrence-T06 invalid ratios/foreign fields/incomplete carriers refused\n";
    std::cout<<"branch-recurrence-T07 actual owners and wrapper lifetimes preserved\n";
}
void limits() {
    auto a=numeric::ExactActionLimits{};a.dimension=4;
    rejects<std::length_error>([&]{Guard g(a);(void)prepare({Q{1,2},Q{1,2}},10,g);},"dimension limit ignored");
    a={};a.entries=10;
    rejects<std::length_error>([&]{Guard g(a);(void)prepare({Q{1,2},Q{1,2}},10,g);},"complete source/frame precharge ignored");
    a={};a.integer_bits=12;
    rejects<std::length_error>([&]{Guard g(a);(void)prepare({Q{1,2},Q{1,2}},30,g);},"growing rational basis bit limit ignored");
    a={};a.integer_bits=4;
    rejects<std::length_error>([&]{Guard g(a);(void)prepare({Q{1,6},Q{1,3},Q{1,10},Q{2,5}},1,g);},"coordinated source lattice bit limit ignored");
    a={};a.steps=10;
    rejects<std::length_error>([&]{Guard g(a);(void)prepare({Q{1,2},Q{1,2}},10,g);},"recurrence work limit ignored");
    Guard complete;const auto b=prepare({Q{1,2},Q{1,2}},10,complete);
    for(const auto point:{std::size_t{1},complete.steps()/2,complete.steps()}) {
        std::size_t calls=0;std::optional<Branch> result;
        rejects<std::runtime_error>([&]{Guard g({},[&]{if(++calls==point)throw std::runtime_error("cancel");});result=prepare({Q{1,2},Q{1,2}},10,g);},"prepare cancellation ignored");
        check(!result,"cancelled preparation published a complete recipe");
    }
    const auto field=F::declare_cyclotomic(1);Guard mode_guard;
    const auto full=b.declared_modes(field,{field.scalar(1),field.scalar(-2)},mode_guard);
    for(const auto point:{std::size_t{1},mode_guard.steps()/2,mode_guard.steps()}) {
        std::size_t calls=0;std::optional<Branch::ModeFamily> result;
        rejects<std::runtime_error>([&]{Guard g({},[&]{if(++calls==point)throw std::runtime_error("cancel");});result=b.declared_modes(field,{field.scalar(1),field.scalar(-2)},g);},"mode cancellation ignored");
        check(!result,"cancelled mode construction published a family");
    }
    a={};a.steps=20;
    rejects<std::length_error>([&]{Guard g(a);(void)b.declared_modes(field,{field.scalar(1)},g);},"mode action work limit ignored");
    a={};a.entries=b.work_receipt().retained_entries_bound+1;
    rejects<std::length_error>([&]{Guard g(a);(void)b.declared_modes(field,{field.scalar(1)},g);},"full mode storage budget ignored");
    a={};a.integer_bits=128;
    rejects<std::length_error>([&]{Guard g(a);(void)b.declared_modes(field,{field.scalar(1)},g);},"receiver admitted field with broader scalar bit limits");
    numeric::AlgebraicFieldLimits fl;fl.max_integer_bits=8;
    const auto small=F::declare_cyclotomic(1,fl);
    rejects<std::length_error>([&]{Guard g;(void)b.declared_modes(small,{small.scalar(-2)},g);},"growing geometric field bits ignored");
    Guard fresh;const auto retry=b.declared_modes(field,{field.scalar(1),field.scalar(-2)},fresh);
    check(retry.complete() && full.complete(),"refusal changed original recipe or earlier certificate");
    std::cout<<"branch-recurrence-T08 receiver/work/bit budgets and early/middle/final cancellation passed\n";
}

void vector_json(const V& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void emit(const V& shares,std::size_t n,int kind,bool& first) {
    Guard g;const auto b=prepare(shares,n,g);
    if(!first)std::cout<<',';
    first=false;
    std::cout<<"{\"n\":"<<n<<",\"shares\":";vector_json(shares);
    std::cout<<",\"basis\":[";
    for(std::size_t i=0;i<b.basis().rows();++i){if(i)std::cout<<',';V row;for(std::size_t j=0;j<b.nullity();++j)row.push_back(b.basis().get(i,j));vector_json(row);}
    std::cout<<"],\"occurrences\":[";
    const auto& arena=b.source().grade(core::Grade{1});
    for(std::size_t slot=0;slot<arena.incidence_count();++slot){if(slot)std::cout<<',';
        std::cout<<'['<<arena.lower_index[slot]<<','<<arena.upper_index[slot]<<",\""<<b.source().incidence_coefficient(core::Grade{1},slot)<<"\"]";}
    std::cout<<']';
    if(kind) {
        const auto field=kind==3?quadratic():F::declare_cyclotomic(1);
        auto roots=std::vector<numeric::AlgebraicElement>{field.scalar(1)};
        if(kind==1)roots.push_back(field.scalar(-2));
        if(kind==2)roots.push_back(field.scalar(Q{-4,3}));
        if(kind==3){const auto one=field.scalar(1),z=field.generator();roots.push_back(-one+z);roots.push_back(-one-z);}
        if(kind==4)roots.push_back(field.scalar(-3));
        const auto modes=b.declared_modes(field,roots,g);
        std::cout<<",\"field_polynomial\":";vector_json(field.minimal_polynomial());
        std::cout<<",\"roots\":[";for(std::size_t i=0;i<roots.size();++i){if(i)std::cout<<',';vector_json(roots[i].coordinates());}
        std::cout<<"],\"complete\":"<<(modes.complete()?"true":"false")<<",\"modes\":[";
        for(std::size_t i=0;i<modes.values().rows();++i){if(i)std::cout<<',';std::cout<<'[';
            for(std::size_t j=0;j<roots.size();++j){if(j)std::cout<<',';vector_json(modes.values().get(i,j).coordinates());}std::cout<<']';}
        std::cout<<']';
    }
    std::cout<<'}';
}
void reference() {
    bool first=true;std::cout<<'[';
    for(const std::size_t n:{1,2,10,17}) {
        emit({1},n,0,first);emit({Q{1,2},Q{1,2}},n,1,first);
        emit({Q{1,4},Q{3,4}},n,2,first);emit({Q{1,3},Q{1,3},Q{1,3}},n,3,first);
        emit({Q{1,3},Q{5,9},Q{1,9}},n,4,first);
        emit({Q{1,10},Q{2,10},Q{3,10},Q{4,10}},n,0,first);
    }
    // Original experiment's arity-four example has twelve vertices, hence N=9.
    emit({Q{1,3},Q{1,3},Q{1,3}},9,3,first);
    std::cout<<"]\n";
}
} // namespace
int main(int argc,char** argv){try{
    if(argc==2 && std::string(argv[1])=="--reference")reference();
    else {basics();modes();refusals();limits();}
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
