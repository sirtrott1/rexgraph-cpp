#include "rfc/numeric/checked_exact_actions.hpp"
#include <future>
#include <iostream>
#include <string_view>

using A = rfc::numeric::ExactMatrix;
using Q = rfc::numeric::Rational;
using V = rfc::numeric::RationalVector;
using S = rfc::numeric::PreparedExactSolve;
using G = rfc::numeric::ExactActionGuard;
using Limits = rfc::numeric::ExactActionLimits;
namespace {
std::size_t checks{}, cases{};
void check(bool ok, const char* message) {++checks; if (!ok) throw std::runtime_error(message);}
template<class Error, class F> void rejects(F f, const char* message) {
    bool caught = false; try {f();} catch (const Error&) {caught = true;} check(caught,message);
}
void vector_json(const V& v) {
    std::cout << '[';
    for (std::size_t i=0;i<v.size();++i) {if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}
    std::cout << ']';
}
void exact_case(const A& a, bool emit) {
    const S solve(a);const auto retained=solve.stored_entries(), preparation=solve.preparation_steps();
    const std::vector<std::size_t> requested{a.cols()-1,0,a.cols()-1};
    const auto selected=solve.prepare_selected(requested);
    check(selected.output_coordinates()==requested,"selected request order and repeats");
    check(selected.uses_full_solve()==(a.cols()<=2),"full requests use primal factors");
    for(unsigned sample=0;sample<3;++sample) {
        V b(a.rows());
        for(std::size_t i=0;i<b.size();++i) {b[i]=Q{static_cast<long>(i+sample)-3,static_cast<unsigned long>(i+1)};b[i].canonicalize();}
        const auto x=solve.apply(b),z=solve.apply_transpose(b),y=selected.apply(b);
        check(x==a.solve(b),"independent sparse primal solve");
        check(z==a.transpose().solve(b),"transpose operation order");
        check(y==V({x.back(),x.front(),x.back()}),"selected exact coordinates");
        check(solve.stored_entries()==retained && solve.preparation_steps()==preparation,"no repeated preparation");
        check(a.apply(x)==b && a.transpose().apply(z)==b,"complete residuals");
        if(emit) {
            std::cout<<"{\"matrix\":[";
            for(std::size_t i=0;i<a.rows();++i) {if(i)std::cout<<',';V row(a.cols());for(std::size_t j=0;j<row.size();++j)row[j]=a.get(i,j);vector_json(row);}
            std::cout<<"],\"rhs\":";vector_json(b);std::cout<<",\"full\":";vector_json(x);
            std::cout<<",\"transpose\":";vector_json(z);std::cout<<",\"outputs\":[";
            for(std::size_t i=0;i<requested.size();++i){if(i)std::cout<<',';std::cout<<requested[i];}
            std::cout<<"],\"selected\":";vector_json(y);std::cout<<"}\n";
        }
        ++cases;
    }
}
void matrices(bool emit) {
    for(const auto& a:std::vector<A>{
        A::from_rows({{0,0,2,1},{1,3,0,0},{0,1,1,0},{2,0,0,1}}),
        A::from_rows({{1,1,0},{2,2,1},{0,1,1}}),
        A::from_rows({{1,0,1},{1,1,0},{0,1,1}}),
        A::from_rows({{Q{3,4},Q{1,2},Q{1,4}},{Q{1,2},1,Q{1,2}},{Q{1,4},Q{1,2},Q{3,4}}}),
        A::from_rows({{1,16},{16,128}})}) exact_case(a,emit);
    for(std::size_t n=2;n<=13;++n)for(std::size_t sample=0;sample<8;++sample) {
        A lower=A::identity(n),upper(n,n);
        for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j) {
            if(j==i)upper.set(i,j,Q{static_cast<unsigned long>(i+2),static_cast<unsigned long>(sample+1)});
            else if(j>i && (i+2*j+sample)%3)upper.set(i,j,Q{static_cast<long>((i+j+sample)%7)-3,static_cast<unsigned long>(j%5+1)});
            else if(i>j && (2*i+j+sample)%3)lower.set(i,j,Q{static_cast<long>((i+3*j+sample)%7)-3,static_cast<unsigned long>(i%5+1)});
        }
        const auto product=lower.multiply(upper);A permuted(n,n);
        for(const auto& [key,q]:product.entries())permuted.set((key.first+sample+1)%n,n-key.second-1,q);
        exact_case(permuted,emit);
    }
    check(cases==303,"exact case count");
}
void support_and_work() {
    constexpr std::size_t n=256;Limits limits;limits.steps=6*n;
    G guard(limits);for(unsigned i=0;i<7;++i)guard.step();
    const S diagonal(A::diagonal(V(n,Q{2})),guard);const auto stats=diagonal.preparation_stats();
    check(stats.pivot_candidates==n && stats.eliminated_rows==0,"only actual diagonal supports visited");
    check(stats.index_updates==2*n && diagonal.preparation_steps()==5*n,"local preparation work count");
    check(guard.steps()==5*n+7,"shared guard is not reset");
    check(stats.peak_rational_entries==2*n && stats.peak_index_entries==n,"separate rational and index envelopes");
    check(stats.peak_integer_bits==2,"observed exact coefficient bit count");
    check(diagonal.apply(V(n,Q{2}))==V(n,Q{1}),"linear-budget preparation result");
    const S fill(A::from_rows({{1,0,1},{1,1,0},{0,1,1}}));
    const S cancel(A::from_rows({{1,1,0},{2,2,1},{0,1,1}}));
    check(fill.preparation_stats().eliminated_rows==2 && cancel.preparation_stats().eliminated_rows==1,"dynamic fill and cancellation support");
    check(fill.apply({1,2,3})==fill.matrix().solve({1,2,3}),"filled support result");
    check(cancel.apply({1,2,3})==cancel.matrix().solve({1,2,3}),"cancelled support result");
    const S identity(A::identity(n));const auto selected=identity.prepare_selected({0});
    G full_guard,selected_guard;check(identity.apply(V(n,Q{1}),full_guard)[0]==selected.apply(V(n,Q{1}),selected_guard)[0],"selected readout value");
    check(selected_guard.steps()<full_guard.steps(),"one sparse readout avoids full primal work");
    const S growth(A::from_rows({{1,16},{16,128}}));
    check(growth.preparation_stats().peak_integer_bits==9,"intermediate product bit growth recorded");
    limits={};limits.integer_bits=8;
    check(growth.apply({1,0},limits)==V({Q{-1},Q{1,8}}),"current use checks retained values, not historical peak");
    rejects<std::length_error>([&]{S bad(growth.matrix(),limits);},"intermediate product bit bound");
}
void contracts() {
    const S solve(A::identity(8));const auto selected=solve.prepare_selected({7,0,7});
    const V b{1,2,3,4,5,6,7,8};const auto expected=selected.apply(b);
    const auto empty=solve.prepare_selected({});check(empty.apply(b).empty() && !empty.uses_full_solve(),"explicit empty readout");
    const std::vector<std::size_t> all{7,6,5,4,3,2,1,0,7};const auto full=solve.prepare_selected(all);
    check(full.uses_full_solve() && full.stored_entries()==solve.stored_entries(),"complete output never caches a full inverse");
    check(full.apply(b)==V({8,7,6,5,4,3,2,1,8}),"complete request permutation and repeats");
    const S zero(A(0,0));check(zero.apply({}).empty() && zero.apply_transpose({}).empty(),"empty square operator");
    check(zero.prepare_selected({}).uses_full_solve() && zero.prepare_selected({}).apply({}).empty(),"empty complete readout");
    const S one(A::diagonal({Q{2}}));check(one.prepare_selected({0,0}).apply({3})==V({Q{3,2},Q{3,2}}),"one dimensional full path");
    rejects<std::out_of_range>([&]{solve.prepare_selected({8});},"invalid selected coordinate");
    rejects<std::out_of_range>([&]{zero.prepare_selected({0});},"empty input invalid selected coordinate");
    rejects<std::invalid_argument>([&]{S bad(A(2,3));},"rectangular solve");
    rejects<std::invalid_argument>([&]{S bad(A::from_rows({{1,1},{2,2}}));},"singular solve");
    rejects<std::invalid_argument>([&]{selected.apply({1});},"selected input dimension");
    rejects<std::invalid_argument>([&]{empty.apply({});},"empty selection still checks input");
    rejects<std::invalid_argument>([&]{solve.apply_transpose({1});},"transpose input dimension");
    V malformed=b;mpz_set_ui(mpq_denref(malformed[3].get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{selected.apply(malformed);},"unused malformed input");
    rejects<std::invalid_argument>([&]{empty.apply(malformed);},"empty selection malformed input");
    V noncanonical=b;mpz_set_ui(mpq_numref(noncanonical[0].get_mpq_t()),2);mpz_set_ui(mpq_denref(noncanonical[0].get_mpq_t()),4);
    check(selected.apply(noncanonical)==V({8,Q{1,2},8}),"canonical selected input");
    Limits limits;limits.dimension=7;
    rejects<std::length_error>([&]{selected.apply(b,limits);},"current full dimension limit");
    limits={};limits.entries=selected.stored_entries()-1;
    rejects<std::length_error>([&]{selected.apply(b,limits);},"current retained dual limit");
    limits={};limits.steps=1;
    rejects<std::length_error>([&]{selected.apply(b,limits);},"current selected work limit");
    limits={};limits.integer_bits=1;V oversized=b;oversized[3]=Q{32768};
    rejects<std::length_error>([&]{selected.apply(oversized,limits);},"unused input bit limit");
    limits={};limits.integer_bits=0;
    rejects<std::length_error>([&]{empty.apply(V(8),limits);},"empty readout validates complete owner");
    limits={};limits.entries=solve.stored_entries();
    rejects<std::length_error>([&]{solve.prepare_selected({0},limits);},"preparation retained dual limit");
    limits={};limits.dimension=8;
    rejects<std::length_error>([&]{solve.prepare_selected(all,limits);},"repeated request dimension bound");
    std::size_t calls{};
    rejects<std::runtime_error>([&]{selected.apply(b,{},[&]{if(++calls==20)throw std::runtime_error("stop");});},"selected cancellation");
    calls=0;rejects<std::runtime_error>([&]{solve.apply_transpose(b,{},[&]{if(++calls==20)throw std::runtime_error("stop");});},"transpose cancellation");
    calls=0;rejects<std::runtime_error>([&]{solve.prepare_selected({0}, {},[&]{if(++calls==20)throw std::runtime_error("stop");});},"selection preparation cancellation");
    calls=0;rejects<std::runtime_error>([&]{S bad(A::identity(8),{},[&]{if(++calls==20)throw std::runtime_error("stop");});},"index preparation cancellation");
    check(selected.apply(b)==expected,"cancelled readout leaves immutable factors usable");
    rejects<std::runtime_error>([&]{zero.apply({}, {},[]{throw std::runtime_error("stop");});},"empty solve checkpoint");
}
void ownership() {
    const auto held=[] {A a=A::from_rows({{1,0,1},{1,1,0},{0,1,1}});S solve(a);auto result=solve.prepare_selected({2,0,2});a.set(0,0,7);return result;}();
    const V b{1,2,3};const auto expected=held.apply(b);std::vector<std::future<V>> jobs;
    for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[held,b]{return held.apply(b);}));
    for(auto& job:jobs)check(job.get()==expected,"retained owner and independent concurrent scratch");
    S solve(A::identity(3));const S replacement(A(0,0));bool switched=false;
    check(solve.apply(b,{},[&]{if(!switched){switched=true;solve=replacement;}})==b,"primal owner pinned before checkpoint");
    solve=S(A::identity(3));switched=false;
    check(solve.apply_transpose(b,{},[&]{if(!switched){switched=true;solve=replacement;}})==b,"transpose owner pinned before checkpoint");
    auto selected=S(A::identity(3)).prepare_selected({2,0});const auto other=replacement.prepare_selected({});switched=false;
    check(selected.apply(b,{},[&]{if(!switched){switched=true;selected=other;}})==V({3,1}),"readout owner pinned before checkpoint");
}
}
int main(int argc,char** argv) {try {
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    matrices(emit);support_and_work();contracts();ownership();
    if(!emit)std::cout<<"sparse_selected_checks="<<checks<<" exact_cases="<<cases<<'\n';
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
