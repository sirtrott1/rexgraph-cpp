#include "rfc/core/builder.hpp"
#include "rfc/field/exact_dirac.hpp"
#include "rfc/geometry/chain_homotopy.hpp"
#include "rfc/numeric/rational_function.hpp"
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"expected rejection");}
int main(){try{
    core::RelationSignature s;const auto edge=s.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(s);b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    const auto source=std::make_shared<const core::RelationalComplex>(b.freeze());
    std::vector<field::ExactMetric> metrics{field::ExactMetric::diagonal({Q{2},Q{3}}),field::ExactMetric::diagonal({Q{5}})};
    field::ExactComplexTowerField x{{{{Q{1,2},Q{-3}},{Q{7}}}},{{{Q{4},Q{2,3}},{Q{-5}}}}};
    for(const auto& t:numeric::RationalVector{Q{0},Q{1,2},Q{-2,3},Q{1}}){
        const auto y=field::exact_dirac_cayley_apply(*source,metrics,t,x);
        require(field::exact_complex_tower_quadrance(metrics,y)==field::exact_complex_tower_quadrance(metrics,x),"complex Cayley changed Hermitian quadrance");
        const auto z=field::exact_dirac_cayley_apply(*source,metrics,-t,y);
        require(z.real.grades==x.real.grades && z.imaginary.grades==x.imaginary.grades,"complex Cayley inverse failed");
        const auto dy_real=field::exact_dirac_apply(*source,metrics,y.real),dy_imag=field::exact_dirac_apply(*source,metrics,y.imaginary);
        const auto dx_real=field::exact_dirac_apply(*source,metrics,x.real),dx_imag=field::exact_dirac_apply(*source,metrics,x.imaginary);
        require(field::exact_tower_add(y.real,field::exact_tower_scale(t,dy_imag)).grades==
            field::exact_tower_sub(x.real,field::exact_tower_scale(t,dx_imag)).grades,"complex Cayley real solve residual");
        require(field::exact_tower_sub(y.imaginary,field::exact_tower_scale(t,dy_real)).grades==
            field::exact_tower_add(x.imaginary,field::exact_tower_scale(t,dx_real)).grades,"complex Cayley imaginary solve residual");
    }
    using F=numeric::RationalFunction;const auto t=F::parameter(),one=F(Q{1});
    require((t*t-one)/(t-one)==t+one,"symbolic cancellation failed");
    const auto f=one/(t*t);
    require(f.derivative()==F({Q{-2}},{Q{0},Q{0},Q{0},Q{1}}),"symbolic derivative failed");
    reject([&]{f.evaluate(Q{0});});reject([&]{one/F();});
    numeric::RationalFunctionMatrix matrix(2,2);matrix.set(0,0,t);matrix.set(0,1,one);matrix.set(1,0,one);matrix.set(1,1,t);
    const auto solved=matrix.solve({one,F()});
    require(solved[0]==t/(t*t-one) && solved[1]==F(Q{-1})/(t*t-one),"Q(t) solve mismatch");
    for(int value=-5;value<=5;++value)if(value!=1 && value!=-1){
        const numeric::ExactMatrix m=numeric::ExactMatrix::from_rows({{Q{value},Q{1}},{Q{1},Q{value}}});
        const auto expected=m.solve({Q{1},Q{0}});
        require(solved[0].evaluate(Q{value})==expected[0] && solved[1].evaluate(Q{value})==expected[1],"symbolic exact evaluation mismatch");
    }
    reject([&]{solved[0].evaluate(Q{1});});
    reject([&]{matrix.solve({one,F()},{1,262144,1000000});});
    reject([&]{matrix.solve({one,F()},{32,262144,0});});
    numeric::RationalFunctionMatrix singular(2,2);singular.set(0,0,t);reject([&]{singular.solve({one,F()});});
    geometry::LinearGradeMap f0(2),g0(2),f1(1),g1(1);f0[0]={{0,Q{1}}};f0[1]={{0,Q{1}}};g0[0]={{1,Q{1}}};g0[1]={{1,Q{1}}};
    geometry::PreparedChainHomotopy homotopy(source,source,{f0,f1},{g0,g1},{numeric::ExactMatrix::from_rows({{1,1}}),numeric::ExactMatrix(0,1)});
    require(homotopy.certificate().exact_identity,"owning chain homotopy failed");
    auto held=homotopy;homotopy=held;require(held.first().source().grade_count()==2,"homotopy owner lost");
    reject([&]{geometry::PreparedChainHomotopy(source,source,{f0,f1},{g0,g1},{numeric::ExactMatrix::from_rows({{1,1}}),numeric::ExactMatrix(0,1)},0);});
    std::cout<<"declared Q(i), Q(t) and owning chain homotopy checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
