#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/field/low_rank_update.hpp"
#include <future>
#include <iostream>
#include <string_view>

using A=rfc::numeric::ExactMatrix;
using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using S=rfc::numeric::PreparedExactSolve;
using Order=rfc::numeric::ExactSolveOrdering;
using Guard=rfc::numeric::ExactActionGuard;
using Limits=rfc::numeric::ExactActionLimits;
namespace {
std::size_t checks{},cases{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F> void rejects(F action,const char* message){
    bool caught=false;try{action();}catch(const Error&){caught=true;}check(caught,message);
}
void json(const V& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void json(const std::vector<std::size_t>& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}std::cout<<']';}
void json(const A& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';V row(a.cols());for(std::size_t j=0;j<row.size();++j)row[j]=a.get(i,j);json(row);}std::cout<<']';}
void system(const A& a,bool emit){
    Order declared;for(std::size_t i=0;i<a.rows();++i){declared.rows.push_back((i+1)%a.rows());declared.columns.push_back(a.rows()-i-1);}
    Guard planning;const auto degree=rfc::numeric::minimum_degree_ordering(a,planning);
    for(unsigned mode=0;mode<3;++mode){
        const S solve=mode==0?S(a):S::prepare_ordered(a,mode==1?degree:declared);
        const auto receipt=solve.factor_receipt();const auto det=receipt.determinant();
        check(&receipt.matrix()==&solve.matrix(),"receipt authentic factor owner");check(receipt.matrix()==a,"original map retained");
        check(det==a.determinant(),"original determinant including all permutation signs");
        std::vector<std::size_t> rows,columns,pivots;V values;
        for(std::size_t i=0;i<a.rows();++i){rows.push_back(receipt.row_coordinate(i));columns.push_back(receipt.column_coordinate(i));pivots.push_back(receipt.pivot_row_slot(i));values.push_back(receipt.pivot_value(i));}
        const auto selected=solve.prepare_selected({a.rows()-1,0,a.rows()-1});
        const auto prep=solve.preparation_steps();const auto stats=solve.preparation_stats();
        for(unsigned sample=0;sample<2;++sample){
            V b(a.rows());for(std::size_t i=0;i<b.size();++i)b[i]=Q{static_cast<long>(i+sample)-2,static_cast<unsigned long>(i+1)};
            const auto x=solve.apply(b),z=solve.apply_transpose(b),observed=selected.apply(b);
            check(x==a.solve(b),"ordered primal original coordinates");check(z==a.transpose().solve(b),"ordered transpose original coordinates");
            check(observed==V({x.back(),x.front(),x.back()}),"selected original coordinates and repeats");
            check(a.apply(x)==rfc::numeric::canonical_vector(b) && a.transpose().apply(z)==rfc::numeric::canonical_vector(b),"full original residuals");
            check(solve.preparation_steps()==prep && receipt.determinant()==det,"reusable factors and receipts");
            if(emit){
                std::cout<<"{\"matrix\":";json(a);std::cout<<",\"mode\":"<<mode<<",\"rhs\":";json(b);
                std::cout<<",\"primal\":";json(x);std::cout<<",\"transpose\":";json(z);std::cout<<",\"selected\":";json(observed);
                std::cout<<",\"rows\":";json(rows);std::cout<<",\"columns\":";json(columns);std::cout<<",\"pivot_slots\":";json(pivots);
                std::cout<<",\"pivot_values\":";json(values);std::cout<<",\"determinant\":\""<<det<<"\",\"signs\":["
                    <<receipt.row_order_sign()<<','<<receipt.column_order_sign()<<','<<receipt.pivot_swap_sign()<<']';
                std::cout<<",\"prepare_steps\":"<<prep<<",\"planning_steps\":"<<(mode==1?planning.steps():0)
                    <<",\"fill\":"<<stats.fill_insertions<<",\"cancellations\":"<<stats.exact_cancellations
                    <<",\"upper_entries\":"<<stats.upper_factor_entries<<",\"elimination_entries\":"<<stats.elimination_factor_entries
                    <<",\"peak_rational_entries\":"<<stats.peak_rational_entries<<",\"peak_index_entries\":"<<stats.peak_index_entries<<"}\n";
            }
            ++cases;
        }
    }
}
void corpus(bool emit){
    for(const auto& a:std::vector<A>{A::from_rows({{0,1},{1,0}}),A::from_rows({{1,1,0},{2,2,1},{0,1,1}}),
        A::from_rows({{1,0,1},{1,1,0},{0,1,1}}),A::from_rows({{2,-1,0},{-1,2,-1},{0,-1,2}})})system(a,emit);
    for(std::size_t n=1;n<=10;++n)for(std::size_t seed=0;seed<5;++seed){
        A lower=A::identity(n),upper(n,n);
        for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j){
            if(i==j)upper.set(i,j,Q{static_cast<unsigned long>(i+2),static_cast<unsigned long>(seed+1)});
            else if(j>i && (i+j+seed)%3)upper.set(i,j,Q{static_cast<long>((i+3*j+seed)%7)-3,static_cast<unsigned long>(j+1)});
            else if(i>j && (i+2*j+seed)%3)lower.set(i,j,Q{static_cast<long>((2*i+j+seed)%7)-3,static_cast<unsigned long>(i+1)});
        }
        const auto product=lower.multiply(upper);A permuted(n,n);
        for(const auto& [key,q]:product.entries())permuted.set((key.first+seed+1)%n,n-key.second-1,q);
        system(permuted,emit);
    }
}
void fill(){
    constexpr std::size_t n=32;A star=A::identity(n);
    for(std::size_t i=1;i<n;++i){star.set(i,i,2);star.set(0,i,-1);star.set(i,0,-1);}star.set(0,0,Q{static_cast<unsigned long>(n)});
    const S natural(star);Guard planning;const auto order=rfc::numeric::minimum_degree_ordering(star,planning);
    const auto ordered=S::prepare_ordered(star,order);const auto a=natural.preparation_stats(),b=ordered.preparation_stats();
    check(natural.apply(V(n,Q{1}))==ordered.apply(V(n,Q{1})),"same complete star response");
    check(b.fill_insertions<a.fill_insertions && b.elimination_factor_entries<a.elimination_factor_entries,"measured star elimination and fill reduction");
    check(ordered.stored_index_entries()>=2*n,"coordinate maps charged in retained indices");
    check(natural.factor_receipt().determinant()==ordered.factor_receipt().determinant(),"star exact determinant preserved");
    check(S(A::from_rows({{1,1,0},{2,2,1},{0,1,1}})).preparation_stats().exact_cancellations>0,"actual numeric cancellation measured");
    const S base=S::prepare_ordered(star,order);A u(n,1);u.set(0,0,1);
    const rfc::field::PreparedWoodburySolve update(base,u,A::identity(1),u);
    check(update.apply(V(n,Q{1}))==star.add(u.multiply(u.transpose())).solve(V(n,Q{1})),"Woodbury reuses actual ordered base correctly");
}
void contracts(){
    const A a=A::identity(3);const Order reverse{{2,1,0},{2,1,0}};const auto solve=S::prepare_ordered(a,reverse);
    const auto receipt=solve.factor_receipt();check(receipt.has_declared_ordering() && !S(a).factor_receipt().has_declared_ordering(),"explicit versus natural chart");
    rejects<std::invalid_argument>([&]{S::prepare_ordered(a,{{0,1},{0,1,2}});},"partial row map");
    rejects<std::invalid_argument>([&]{S::prepare_ordered(a,{{0,1,2},{0,0,2}});},"duplicate column coordinate");
    rejects<std::invalid_argument>([&]{S::prepare_ordered(a,{{0,1,3},{0,1,2}});},"outside row coordinate");
    rejects<std::invalid_argument>([&]{S::prepare_ordered(A(2,3),{{0,1},{0,1,2}});},"rectangular ordered solve");
    rejects<std::invalid_argument>([&]{S::prepare_ordered(A(3,3),reverse);},"singular ordered map");
    rejects<std::invalid_argument>([]{rfc::numeric::minimum_degree_ordering(A(2,3));},"rectangular planning");
    rejects<std::out_of_range>([&]{receipt.pivot_value(3);},"invalid pivot coordinate");
    rejects<std::out_of_range>([&]{receipt.row_coordinate(3);},"invalid receipt row coordinate");
    const auto empty=S::prepare_ordered(A(0,0),{});check(empty.apply({}).empty() && empty.apply_transpose({}).empty(),"empty ordered system");
    check(empty.factor_receipt().determinant()==1,"empty determinant is one");
    check(rfc::numeric::minimum_degree_ordering(A(0,0)).rows.empty(),"empty ordering");
    Limits limits;limits.entries=4;
    rejects<std::length_error>([&]{S::prepare_ordered(a,reverse,limits);},"ordering scratch admission");
    rejects<std::length_error>([&]{rfc::numeric::minimum_degree_ordering(a,limits);},"symbolic index admission");
    limits.entries=5;rejects<std::length_error>([&]{solve.apply({1,2,3},limits);},"current complete ordering index admission");
    rejects<std::length_error>([&]{receipt.determinant(limits);},"receipt validates current retained indices");
    limits={};limits.steps=1;rejects<std::length_error>([&]{receipt.determinant(limits);},"determinant work admission");
    rejects<std::length_error>([&]{rfc::numeric::minimum_degree_ordering(a,limits);},"planning work admission");
    limits={};limits.integer_bits=2;const S diagonal(A::diagonal(V(16,Q{2})),limits);const auto held=diagonal.factor_receipt(limits);
    check(diagonal.apply(V(16,Q{2}),limits)==V(16,Q{1}),"factor preparation does not compute a large determinant");
    rejects<std::length_error>([&]{held.determinant(limits);},"requested determinant integer growth");
    check(held.determinant()==65536,"exact determinant from retained pivots");
    limits.integer_bits=1;rejects<std::length_error>([&]{rfc::numeric::minimum_degree_ordering(A::diagonal({2}),limits);},"unused map coefficient checked by planner");
    V bad{1,2,3};mpz_set_ui(mpq_denref(bad[2].get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{solve.apply(bad);},"ordered raw denominator");
    rejects<std::invalid_argument>([&]{solve.prepare_selected({}).apply(bad);},"empty observation raw denominator");
    std::size_t calls{};auto stop=[&]{if(++calls==7)throw std::runtime_error("stop");};
    rejects<std::runtime_error>([&]{rfc::numeric::minimum_degree_ordering(a,{},stop);},"planning cancellation");calls=0;
    rejects<std::runtime_error>([&]{S::prepare_ordered(a,reverse,{},stop);},"ordered preparation cancellation");calls=0;
    rejects<std::runtime_error>([&]{solve.apply({1,2,3},{},stop);},"ordered solve cancellation");calls=0;
    rejects<std::runtime_error>([&]{receipt.determinant({},stop);},"determinant cancellation");calls=0;
    rejects<std::runtime_error>([&]{solve.factor_receipt({},stop);},"receipt preparation cancellation");
    check(solve.apply({1,2,3})==V({1,2,3}) && receipt.determinant()==1,"cancelled work preserves immutable factors");
    rejects<std::runtime_error>([&]{empty.factor_receipt().determinant({},[]{throw std::runtime_error("stop");});},"empty determinant checkpoint");
}
void ownership(){
    A a=A::from_rows({{0,1,0},{1,3,1},{0,1,2}});const A original=a;
    Order order{{2,0,1},{1,2,0}};const auto solve=S::prepare_ordered(a,order);auto receipt=solve.factor_receipt();
    a.set(0,0,7);order.rows[0]=0;
    check(solve.matrix()==original && receipt.row_coordinate(0)==2,"original map and complete ordering owned");
    const auto expected=solve.apply({1,2,3});std::vector<std::future<V>> jobs;
    for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[solve]{return solve.apply({1,2,3});}));
    for(auto& job:jobs)check(job.get()==expected,"ordered concurrent independent scratch");
    auto changed=solve;const S replacement(A(0,0));bool switched=false;
    check(changed.apply({1,2,3},{},[&]{if(!switched){switched=true;changed=replacement;}})==expected,"ordered primal pinned before callback");
    changed=solve;switched=false;check(changed.apply_transpose({1,2,3},{},[&]{if(!switched){switched=true;changed=replacement;}})==original.transpose().solve({1,2,3}),"ordered transpose pinned before callback");
    const auto other=replacement.factor_receipt();switched=false;
    check(receipt.determinant({},[&]{if(!switched){switched=true;receipt=other;}})==original.determinant(),"receipt pinned before callback");
    auto owner=solve;switched=false;auto retained=owner.factor_receipt({},[&]{if(!switched){switched=true;owner=replacement;}});
    check(retained.matrix()==original && retained.determinant()==original.determinant(),"receipt adoption pins actual original owner");
    A input=original;switched=false;const auto planned=rfc::numeric::minimum_degree_ordering(input,{},[&]{if(!switched){switched=true;input=A(0,0);}});
    check(planned.rows.size()==3 && S::prepare_ordered(original,planned).apply({1,2,3})==expected,"planner pins map before callback");
}
}
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";corpus(emit);fill();contracts();ownership();
    check(cases==324,"exact corpus size");
    if(!emit)std::cout<<"ordered_exact_checks="<<checks<<" exact_cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
