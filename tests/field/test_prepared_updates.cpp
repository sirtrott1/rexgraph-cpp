#include "rfc/temporal/transfer.hpp"
#include <future>
#include <iostream>
#include <string_view>
#include <type_traits>

using M=rfc::numeric::ExactMatrix;
using V=rfc::numeric::RationalVector;
using Q=rfc::numeric::Rational;
using Guard=rfc::numeric::ExactActionGuard;
using Limits=rfc::numeric::ExactActionLimits;
using Solve=rfc::numeric::PreparedExactSolve;
using Update=rfc::field::PreparedWoodburySolve;
using Green=rfc::temporal::PreparedGreenResponseDelta;
using Observed=rfc::temporal::PreparedObservedGreenResponseDelta;
using WoodburyFunction=V(*)(const M&,const M&,const M&,const M&,const V&);
using GreenFunction=rfc::temporal::GreenResponseDelta(*)(const M&,const M&,const M&,const M&,const M&,const M&,const V&,const V&,const Q&);
using ObservedFunction=rfc::temporal::ObservedGreenResponseDelta(*)(const rfc::temporal::GreenResponseDelta&,const M&,const M&,const M&,const M&);
static_assert(std::is_same_v<decltype(&rfc::field::woodbury_solve),WoodburyFunction>);
static_assert(std::is_same_v<decltype(&rfc::temporal::green_response_delta),GreenFunction>);
static_assert(std::is_same_v<decltype(&rfc::temporal::observed_green_response_delta),ObservedFunction>);
namespace {
std::size_t checks{},woodbury_cases{},green_cases{};
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class Error,class F> void rejects(F f,const char* message){
    bool caught=false;try{f();}catch(const Error&){caught=true;}check(caught,message);
}
void json(const V& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void json(const M& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';V row(a.cols());for(std::size_t j=0;j<row.size();++j)row[j]=a.get(i,j);json(row);}std::cout<<']';}
M rectangular(std::size_t rows,std::size_t cols,std::size_t seed,unsigned denominator=1){
    M a(rows,cols);for(std::size_t i=0;i<rows;++i)for(std::size_t j=0;j<cols;++j){
        a.set(i,j,Q{static_cast<long>((3*i+5*j+seed)%7)-3,denominator});}
    return a;
}
M operator_matrix(std::size_t n,std::size_t seed){
    M a=rectangular(n,n,seed,7);for(std::size_t i=0;i<n;++i)a.set(i,i,Q{static_cast<unsigned long>(3*n+i+4)});return a;
}
void woodbury(bool emit){
    for(std::size_t n=1;n<=6;++n)for(std::size_t rank=1;rank<=3;++rank)for(std::size_t seed=0;seed<3;++seed){
        const auto a=operator_matrix(n,seed),u=rectangular(n,rank,seed+1,100),v=rectangular(n,rank,seed+4,100);
        M c=M::diagonal(V(rank,Q{2}));for(std::size_t i=1;i<rank;++i)c.set(i-1,i,Q{static_cast<unsigned long>(i+seed+1)});
        const auto full=rfc::field::low_rank_updated_matrix(a,u,c,v);const Solve base(a);const Update plan(base,u,c,v);
        check(&plan.base_solve().matrix()==&base.matrix(),"reuse authentic base factor owner");
        const auto prep=plan.preparation_steps(),entries=plan.stored_entries();
        for(std::size_t sample=0;sample<2;++sample){
            V rhs(n);for(std::size_t i=0;i<n;++i)rhs[i]=Q{static_cast<long>(i+sample)-2,3};
            const auto answer=plan.apply(rhs);
            check(answer==full.solve(rhs),"Woodbury full-system solve");check(full.apply(answer)==rfc::numeric::canonical_vector(rhs),"full updated residual");
            check(answer==rfc::field::woodbury_solve(a,u,c,v,rhs),"one-shot delegates to preparation");
            check(plan.preparation_steps()==prep && plan.stored_entries()==entries,"update factors unchanged across input changes");
            if(emit){std::cout<<"{\"kind\":\"woodbury\",\"A\":";json(a);std::cout<<",\"U\":";json(u);std::cout<<",\"C\":";json(c);std::cout<<",\"V\":";json(v);std::cout<<",\"rhs\":";json(rhs);std::cout<<",\"answer\":";json(answer);std::cout<<"}\n";}
            ++woodbury_cases;
        }
    }
    const auto b=M::from_rows({{1,0,0,-1},{-1,1,0,0},{0,-1,1,0},{0,0,-1,1}});
    const auto change=M::from_rows({{1,0,0,0},{0,0,0,0},{-1,0,0,0},{0,0,0,0}}),bp=b.add(change);
    const auto metric=M::diagonal({2,3,4,5});const auto gram=b.transpose().multiply(metric).multiply(b);
    const auto delta=bp.transpose().multiply(metric).multiply(bp).add(gram.scale(-1));
    M u(4,2),v(4,2);const auto first=b.transpose().multiply(metric).apply({1,0,-1,0});
    for(std::size_t i=0;i<4;++i){u.set(i,0,first[i]);u.set(i,1,i==0?1:0);v.set(i,0,i==0?1:0);v.set(i,1,first[i]);}
    const auto c=M::from_rows({{1,0},{6,1}});const auto a=M::identity(4).add(gram);
    check(u.multiply(c).multiply(v.transpose())==delta,"complete finite quadratic boundary interaction");
    const auto middle=v.transpose().multiply(rfc::field::solve_columns_exact(a,u));
    check(c.multiply(middle)!=middle.multiply(c),"noncommuting correction order is exercised");
    const Update boundary(a,u,c,v);check(boundary.apply({1,2,3,4})==M::identity(4).add(bp.transpose().multiply(metric).multiply(bp)).solve({1,2,3,4}),"boundary Gram update uses full interaction");
    Guard warm,cold;const auto value=boundary.apply({1,2,3,4},warm);const Update fresh(a,u,c,v,cold);
    check(fresh.apply({1,2,3,4},cold)==value && warm.steps()<cold.steps(),"retained factors avoid repeated preparation work");
    const auto actual=rfc::field::solve_columns_exact(a,u);Guard column_guard;
    check(rfc::field::solve_columns_exact(Solve(a),u,column_guard)==actual && a.multiply(actual)==u,"prepared full-column solve");
}
void green(bool emit){
    for(std::size_t old_n=1;old_n<=5;++old_n)for(std::size_t new_n=1;new_n<=6;++new_n)for(std::size_t seed=0;seed<3;++seed){
        const auto old_L=operator_matrix(old_n,seed),new_L=operator_matrix(new_n,seed+3),J=rectangular(new_n,old_n,seed+2);
        const auto old_I=rectangular(old_n,seed+1,seed+6),new_I=rectangular(new_n,4-seed,seed+8),K=rectangular(4-seed,seed+1,seed+4);
        const auto old_A=rectangular(2,old_n,seed+1),new_A=rectangular(3,new_n,seed+5),KA=rectangular(3,2,seed+6);
        const Q lambda=seed==0?Q{0}:Q{static_cast<unsigned long>(seed),3};
        const Green plan(old_L,new_L,J,old_I,new_I,K,lambda);const Observed observed(plan,old_A,new_A,KA);
        const auto prep=plan.preparation_steps(),observed_prep=observed.preparation_steps();
        for(std::size_t sample=0;sample<2;++sample){
            V u(seed+1,Q{static_cast<long>(sample)-2}),up(4-seed,Q{static_cast<long>(sample)+3});
            const auto response=plan.apply(u,up),one=rfc::temporal::green_response_delta(old_L,new_L,J,old_I,new_I,K,u,up,lambda);
            check(response.aggregate==rfc::numeric::sub(response.new_response,J.apply(response.old_response)),"full rectangular Green response");
            check(response.operator_defect==new_L.multiply(J).add(J.multiply(old_L).scale(-1)),"complete operator defect");
            check(response.injection_defect==new_I.multiply(K).add(J.multiply(old_I).scale(-1)),"complete injection defect");
            check(response.aggregate==one.aggregate && response.operator_term==one.operator_term,"canonical one-shot Green");
            const auto output=observed.apply(u,up);
            check(output.aggregate==rfc::numeric::sub(new_A.apply(response.new_response),KA.apply(old_A.apply(response.old_response))),"full observed response");
            check(output.aggregate==rfc::temporal::observed_green_response_delta(response,old_A,new_A,J,KA).aggregate,"canonical observed response");
            check(plan.preparation_steps()==prep && observed.preparation_steps()==observed_prep,"fixed Green preparation");
            if(emit){
                std::cout<<"{\"kind\":\"green\",\"old_L\":";json(old_L);std::cout<<",\"new_L\":";json(new_L);std::cout<<",\"J\":";json(J);
                std::cout<<",\"old_I\":";json(old_I);std::cout<<",\"new_I\":";json(new_I);std::cout<<",\"K\":";json(K);
                std::cout<<",\"old_A\":";json(old_A);std::cout<<",\"new_A\":";json(new_A);std::cout<<",\"KA\":";json(KA);
                std::cout<<",\"lambda\":\""<<lambda<<"\",\"u\":";json(u);std::cout<<",\"up\":";json(up);
                std::cout<<",\"old_response\":";json(response.old_response);std::cout<<",\"new_response\":";json(response.new_response);
                std::cout<<",\"EI\":";json(response.injection_defect);std::cout<<",\"EL\":";json(response.operator_defect);
                std::cout<<",\"realization\":";json(response.realization_term);std::cout<<",\"innovation\":";json(response.innovation_term);
                std::cout<<",\"operator\":";json(response.operator_term);std::cout<<",\"aggregate\":";json(response.aggregate);
                std::cout<<",\"EA\":";json(output.accession_defect);std::cout<<",\"observed\":";json(output.aggregate);std::cout<<"}\n";
            }
            ++green_cases;
        }
    }
    const auto b=M::from_rows({{1,0,0,-1},{-1,1,0,0},{0,-1,1,0},{0,0,-1,1}}),L=b.transpose().multiply(b);
    const auto c=M::from_rows({{1},{1},{1},{1}}),p=M::from_rows({{1},{1},{0},{0}}),q=M::from_rows({{0},{0},{-1},{-1}});
    const Green plan(L,L.add(c.multiply(c.transpose())),M::identity(4),p,q,M::identity(1),1);const auto response=plan.apply({2},{3});
    check(response.old_response==V({Q{4,3},Q{4,3},Q{2,3},Q{2,3}}),"paper old square response");
    check(response.new_response==V({Q{1,5},Q{1,5},Q{-4,5},Q{-4,5}}),"paper new square response");
    check(response.aggregate==V({Q{-17,15},Q{-17,15},Q{-22,15},Q{-22,15}}),"paper complete square change");
    const Observed output(plan,M::from_rows({{1,0,0,0},{0,1,0,0}}),M::from_rows({{1,0,1,0},{0,1,0,1}}),M::identity(2));
    const auto observed=output.apply({2},{3});check(observed.aggregate==V({Q{-29,15},Q{-29,15}}),"paper observed change");
    check(observed.accession_term==V({Q{2,3},Q{2,3}}) && observed.operator_term==V({Q{-8,5},Q{-8,5}}),"paper separated observation terms");
}
Green identity_green(Q lambda=0){return Green(M::identity(2),M::identity(3),M::from_rows({{1,0},{0,1},{0,0}}),
    M::from_rows({{1,0,0},{0,1,0}}),M::identity(3),M::identity(3),std::move(lambda));}
void contracts(){
    const auto identity=M::identity(2);const Update update(identity,identity,identity,identity);
    const Update empty(identity,M(2,0),M(0,0),M(2,0));check(empty.apply({2,3})==V({2,3}),"rank zero still uses full operator");
    const Update zero(M(0,0),M(0,2),identity,M(0,2));check(zero.apply({}).empty(),"empty ambient with invertible coupling");
    rejects<std::invalid_argument>([&]{Update bad(identity,identity,M(2,2),identity);},"singular C rejects even if updated A is invertible");
    rejects<std::invalid_argument>([&]{Update bad(identity,identity,identity.scale(-1),identity);},"singular updated system");
    rejects<std::invalid_argument>([&]{Update bad(M(2,2),M(2,0),M(0,0),M(2,0));},"zero update singular base");
    rejects<std::invalid_argument>([&]{Update bad(identity,M(2,1),identity,identity);},"complete update dimensions");
    rejects<std::invalid_argument>([&]{update.apply({1});},"update input dimensions");
    rejects<std::invalid_argument>([&]{rfc::field::solve_columns_exact(M(2,2),M(2,0));},"empty columns require declared invertible operator");
    const auto green=identity_green();const Observed observation(green,M(0,2),M(0,3),M(0,0));
    const V u{1,2,3},up{4,5,6};check(green.apply(u,up).aggregate==V({3,3,6}),"complete new direction preserved");
    check(observation.apply(u,up).aggregate.empty(),"explicit empty observation");
    auto malformed=u;mpz_set_ui(mpq_denref(malformed[2].get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{green.apply(malformed,up);},"unused old source coordinate denominator");
    rejects<std::invalid_argument>([&]{observation.apply(malformed,up);},"empty observation validates whole input");
    malformed={1,2};mpz_set_ui(mpq_denref(malformed[1].get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{empty.apply(malformed);},"zero update validates whole input");
    Q invalid=0;mpz_set_ui(mpq_denref(invalid.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{identity_green(invalid);},"malformed zero shift");
    rejects<std::invalid_argument>([]{Green bad(M::identity(1),M::identity(1),M::identity(1),M::identity(1),M::identity(1),M::identity(1),-1);},"singular shifted Green system");
    check(identity_green(Q{-1,2}).apply(u,up).aggregate==V({6,6,12}),"admitted nonsingular negative shift");
    rejects<std::invalid_argument>([&]{Observed bad(green,M(1,3),M(1,3),M(1,1));},"observation carrier dimensions");
    auto forged=green.apply(u,up);forged.new_response[0]+=1;
    rejects<std::logic_error>([&]{rfc::temporal::observed_green_response_delta(forged,identity,M::from_rows({{1,0,0},{0,1,0}}),green.ambient_correspondence(),identity);},"incoherent observed response rejects");
    Limits limits;limits.entries=update.stored_entries()-1;
    rejects<std::length_error>([&]{update.apply({1,2},limits);},"current retained update bound");
    limits.entries=green.stored_entries()-1;rejects<std::length_error>([&]{green.apply(u,up,limits);},"current retained Green bound");
    limits.entries=observation.stored_entries()-1;rejects<std::length_error>([&]{observation.apply(u,up,limits);},"current retained observation bound");
    limits={};limits.dimension=2;rejects<std::length_error>([&]{green.apply(u,up,limits);},"current full dimensions");
    limits={};limits.steps=1;rejects<std::length_error>([&]{update.apply({1,2},limits);},"current update work bound");
    rejects<std::length_error>([&]{green.apply(u,up,limits);},"current Green work bound");
    limits={};limits.integer_bits=3;auto large=u;large[2]=32768;
    rejects<std::length_error>([&]{green.apply(large,up,limits);},"unused source bit bound");
    const Green large_map(M::diagonal({32768,1}),M::identity(2),identity,identity,identity,identity,0);
    rejects<std::length_error>([&]{large_map.apply({0,0},{0,0},limits);},"zero shift still validates full retained L");
    rejects<std::length_error>([&]{Observed bad(large_map,M(0,2),M(0,2),M(0,0),limits);},"empty observation preparation validates complete owner");
    Q unreduced{0};mpz_set_ui(mpq_numref(unreduced.get_mpq_t()),2);mpz_set_ui(mpq_denref(unreduced.get_mpq_t()),4);
    check(update.apply({unreduced,2})==V({Q{1,4},1}),"canonical update input without mutating caller");
    check(identity_green(unreduced).apply(u,up).aggregate==V({2,2,4}),"canonical shared Green shift");
    limits={};limits.entries=1;rejects<std::length_error>([&]{Update bad(identity,identity,identity,identity,limits);},"update preparation container bound");
    rejects<std::length_error>([&]{Green bad(identity,identity,identity,identity,identity,identity,0,limits);},"Green preparation container bound");
    std::size_t calls{};auto stop=[&]{if(++calls==20)throw std::runtime_error("stop");};
    rejects<std::runtime_error>([&]{update.apply({1,2},{},stop);},"update owned cancellation");calls=0;
    rejects<std::runtime_error>([&]{green.apply(u,up,{},stop);},"Green owned cancellation");calls=0;
    rejects<std::runtime_error>([&]{Update bad(identity,identity,identity,identity,{},stop);},"update preparation cancellation");calls=0;
    rejects<std::runtime_error>([&]{Green bad(identity,identity,identity,identity,identity,identity,0,{},stop);},"Green preparation cancellation");calls=0;
    rejects<std::runtime_error>([&]{Observed bad(green,M(0,2),M(0,3),M(0,0),{},stop);},"observation preparation cancellation");calls=0;
    rejects<std::runtime_error>([&]{observation.apply(u,up,{},stop);},"observation cancellation leaves independent owner");
    check(green.apply(u,up).aggregate==V({3,3,6}),"cancelled scratch does not modify retained plan");
    rejects<std::runtime_error>([&]{zero.apply({}, {},[]{throw std::runtime_error("stop");});},"empty update checkpoint");
    const Green no_carrier(M(0,0),M(0,0),M(0,0),M(0,0),M(0,0),M(0,0),0);
    check(no_carrier.apply({},{}).aggregate.empty(),"empty Green carriers");
}
void ownership(){
    M a=M::identity(2),u=a,c=a,v=a;Update plan(a,u,c,v);a.set(0,0,9);u.set(0,0,7);c.set(0,0,5);v.set(0,0,3);
    check(plan.apply({2,4})==V({1,2}),"all update maps owned independently of mutable inputs");
    const auto green=[] {auto L=M::identity(2);Green result(L,L,M::identity(2),M::identity(2),M::identity(2),M::identity(2),1);L.set(0,0,7);return result;}();
    std::vector<std::future<V>> jobs;for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[green]{return green.apply({2,4},{6,8}).aggregate;}));
    for(auto& job:jobs)check(job.get()==V({2,2}),"immutable Green maps with independent concurrent scratch");
    const Update replacement(M::identity(1),M(1,0),M(0,0),M(1,0));bool switched=false;
    check(plan.apply({2,4},{},[&]{if(!switched){switched=true;plan=replacement;}})==V({1,2}),"update owner pinned before callback");
    auto original=identity_green();const Green empty(M(0,0),M(0,0),M(0,0),M(0,0),M(0,0),M(0,0),0);switched=false;
    check(original.apply({1,2,3},{4,5,6},{},[&]{if(!switched){switched=true;original=empty;}}).aggregate==V({3,3,6}),"Green owner pinned before callback");
    Observed observed(identity_green(),M::identity(2),M::identity(3),M::from_rows({{1,0},{0,1},{0,0}}));
    const Observed other(empty,M(0,0),M(0,0),M(0,0));switched=false;
    check(observed.apply({1,2,3},{4,5,6},{},[&]{if(!switched){switched=true;observed=other;}}).aggregate==V({3,3,6}),"observed owner pinned before callback");
}
}
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";woodbury(emit);green(emit);contracts();ownership();
    check(woodbury_cases==108 && green_cases==180,"independent exact corpus counts");
    if(!emit)std::cout<<"prepared_update_checks="<<checks<<" woodbury_cases="<<woodbury_cases<<" green_cases="<<green_cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
