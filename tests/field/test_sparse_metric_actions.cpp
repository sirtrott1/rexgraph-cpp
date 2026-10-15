#include "rfc/field/exact_hodge.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using S=numeric::SparseRationalVector;
using M=numeric::ExactMatrix;
using Metric=field::ExactMetric;
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F fn,const char* message){bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit,message);}
S sparse(const V& values){std::vector<numeric::SparseRationalEntry> out;for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)out.push_back({i,values[i]});return S(values.size(),std::move(out));}
}
int main(){try{
    for(std::size_t n=1;n<=9;++n)for(unsigned pattern=0;pattern<4;++pattern){
        M lower=M::identity(n),diagonal(n,n);
        for(std::size_t i=0;i<n;++i){
            diagonal.set(i,i,Q{static_cast<long>(i+2),static_cast<long>(i+1)});
            for(std::size_t j=0;j<i;++j)if((i+j+pattern)%3==0)
                lower.set(i,j,Q{static_cast<long>((i+pattern)%5)-2,static_cast<long>(j+2)});
        }
        const auto form=lower.multiply(diagonal).multiply(lower.transpose());const Metric metric(form);
        check(metric.supports_sparse_apply() && metric.supports_sparse_solve(),"general rational form has native sparse actions");
        check(metric.determinant()==diagonal.determinant(),"metric determinant reuses positive pivots");
        for(std::size_t selected=0;selected<n;++selected){
            V input(n);input[selected]=Q{3,2};if(selected+1<n)input[selected+1]=Q{-7,3};
            const auto x=sparse(input),applied=metric.apply(x.view()),solved=metric.solve(x.view());
            check(applied.dense()==form.apply(input),"sparse form action matches independent map");
            check(solved.dense()==form.solve(input),"sparse triangular solve matches independent elimination");
            check(metric.solve(applied.view()).entries()==x.entries(),"sparse metric round trip");
            check(metric.inner_sparse(x.view(),x.view())==numeric::dot(input,form.apply(input)),"sparse metric pairing");
        }
        check(metric.solve(S(n).view()).empty(),"zero sparse input does not expand the carrier");
    }
    const M coupled=M::from_rows({{2,1},{1,3}});const Metric m(coupled);
    const auto external=Metric::certified_external(2,[m](const V& x){return m.apply(x);},[m](const V& x){return m.solve(x);});
    check(!external.supports_sparse_apply() && !external.supports_sparse_solve(),"undeclared external sparse capability is absent");
    rejects([&]{(void)external.apply(S::basis(2,0).view());},"external metric must not silently densify sparse apply");
    rejects([&]{(void)external.solve(S::basis(2,0).view());},"external metric must not silently densify sparse solve");
    const auto native_external=Metric::certified_external(2,[m](const V& x){return m.apply(x);},[m](const V& x){return m.solve(x);},{},
        [m](auto x){return m.apply(x);},[m](auto x){return m.solve(x);});
    check(native_external.solve(S::basis(2,0).view()).dense()==V{Q{3,5},Q{-1,5}},"declared external sparse action retained");
    if(std::numeric_limits<std::size_t>::digits>34){
        const auto n=std::size_t{1}<<34;
        const auto block=Metric::block_diagonal({Metric::identity(n),m,Metric::identity(0)});
        const S input(n+2,{{n,Q{1}}});const auto solution=block.solve(input.view());
        check(solution.entries()==std::vector<numeric::SparseRationalEntry>{{n,Q{3,5}},{n+1,Q{-1,5}}},"huge declared block carrier retains only two solution entries");
        check(block.apply(solution.view()).entries()==input.entries(),"huge sparse block action certificate");
        check(block.determinant()==5,"block determinant does not materialize the huge identity");
        check(block.inner_sparse(input.view(),input.view())==2,"huge sparse block energy");
        M a(n,n),b(n,n);a.set(n-2,n-1,3);b.set(n-1,n-3,Q{2,7});
        const auto product=a.multiply(b);check(product.nonzeros()==1 && product.get(n-2,n-3)==Q{6,7},"unbounded sparse logical matrix multiplication");
        numeric::ExactActionLimits limits;limits.dimension=n;limits.entries=32;limits.steps=100;
        numeric::ExactActionGuard guard(limits);
        check(numeric::checked_multiply(a,b,guard)==product,"guarded multiplication has no carrier-sized row index");
        rejects([&]{(void)Metric(a);},"huge nonsymmetric form rejects without carrier allocation");
    }
    core::RelationSignature signature;const auto edge=signature.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);builder.add_grade_one(edge,{{10,true,true,1},{20,true,false,1}});
    const auto source=builder.freeze();const field::BoundaryOperator boundary(source,core::Grade{1});
    const auto upper=Metric::diagonal({Q{7,3}});const S input=S::basis(2,0,Q{2});
    const auto adjoint=boundary.adjoint(input.view(),m,upper);
    check(adjoint.dense()==boundary.adjoint(input.dense(),m,upper),"source boundary sparse adjoint accepts general rational lower metric");
    std::cout<<"sparse metric actions: "<<checks<<" checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
