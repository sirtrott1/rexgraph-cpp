#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace rfc::numeric;
namespace {
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class E,class F> void rejects(F&& fn,const char* message){
    bool rejected=false;try{fn();}catch(const E&){rejected=true;}check(rejected,message);
}
Rational pairing(const ExactMatrix& a,const RationalVector& x){
    Rational result{0};for(const auto& [key,q]:a.entries())result+=x[key.first]*q*x[key.second];return result;
}
}
int main(){
    try {
        for(std::size_t n=1;n<=7;++n)for(std::size_t rank=0;rank<=n;++rank)for(unsigned variant=0;variant<4;++variant){
            ExactMatrix factor(n,rank),diagonal(rank,rank);
            for(std::size_t j=0;j<rank;++j){
                factor.set(j,j,1);diagonal.set(j,j,Rational{static_cast<long>(j+1),static_cast<long>(j+2)});
                for(std::size_t i=j+1;i<n;++i){
                    const auto numerator=static_cast<long>((i+2*j+variant)%5)-2;
                    factor.set(i,j,Rational{numerator,static_cast<long>(i+j+2)});
                }
            }
            if(variant&1u) {
                ExactMatrix permuted(n,rank);
                for(const auto& [key,q]:factor.entries())permuted.set(n-1-key.first,key.second,q);
                factor=std::move(permuted);
            }
            auto form=factor.multiply(diagonal).multiply(factor.transpose());
            ExactActionGuard guard;
            PreparedPositiveSemidefinite certificate(form,guard);
            check(certificate.rank()==rank,"PSD rank");
            ExactMatrix d(n,n);
            for(const auto& [i,q]:certificate.diagonal_entries())d.set(i,i,q);
            const auto lower=ExactMatrix::identity(n).add(certificate.unit_lower_entries());
            check(lower.multiply(d).multiply(lower.transpose())==form,"square-root-free factor reconstruction");
            RationalVector x(n);
            std::vector<SparseRationalEntry> sparse;
            for(std::size_t i=0;i<n;++i){x[i]=Rational{static_cast<long>((i+variant)%4)-2,3};if(x[i]!=0)sparse.push_back({i,x[i]});}
            const auto expected=pairing(form,x);
            check(certificate.quadratic_form(x,guard)==expected,"PSD full quadratic form");
            check(certificate.quadratic_form(SparseRationalVector(n,std::move(sparse)),guard)==expected,"PSD sparse quadratic form");
            check(expected>=0,"certified nonnegative energy");
            if(n){form.set(0,0,-1);check(certificate.original().get(0,0)!=-1,"PSD owner must retain original snapshot");}
        }
        rejects<std::invalid_argument>([]{ExactMatrix a(2,2);a.set(0,1,1);(void)PreparedPositiveSemidefinite(a);},"asymmetric form");
        rejects<std::invalid_argument>([]{ExactMatrix a(2,2);a.set(0,1,1);a.set(1,0,1);(void)PreparedPositiveSemidefinite(a);},"zero diagonal with coupling");
        rejects<std::invalid_argument>([]{ExactMatrix a(2,2);a.set(0,0,1);a.set(1,1,1);a.set(0,1,2);a.set(1,0,2);(void)PreparedPositiveSemidefinite(a);},"negative Schur pivot");
        rejects<std::invalid_argument>([]{(void)PreparedPositiveSemidefinite(ExactMatrix(2,3));},"rectangular PSD form");
        const PreparedPositiveSemidefinite retained(ExactMatrix::diagonal(RationalVector{8}));
        rejects<std::exception>([]{ExactActionLimits limits;limits.entries=2;
            (void)PreparedPositiveSemidefinite(ExactMatrix::diagonal(RationalVector{1}),limits);},
            "PSD preparation admits the simultaneous source, working and saved pivot coefficients");
        rejects<std::exception>([&]{ExactActionLimits limits;limits.entries=0;ExactActionGuard guard(limits);
            (void)retained.quadratic_form(SparseRationalVector(1),guard);},"empty sparse input still admits retained PSD factors");
        rejects<std::exception>([&]{ExactActionLimits limits;limits.integer_bits=2;ExactActionGuard guard(limits);
            (void)retained.quadratic_form(SparseRationalVector(1),guard);},"current bit limit applies to retained PSD factors");
        rejects<std::exception>([&]{ExactActionGuard guard({},[]{throw std::runtime_error("cancel");});
            (void)retained.quadratic_form(SparseRationalVector(1),guard);},"zero-input PSD action checks cancellation");
        if(std::numeric_limits<std::size_t>::digits>33){
            const std::size_t n=std::size_t{1}<<33;
            ExactActionLimits limits;limits.dimension=n;limits.entries=16;limits.steps=64;
            ExactActionGuard guard(limits);ExactMatrix huge(n,n);huge.set(n-1,n-1,Rational{7,3});
            const PreparedPositiveSemidefinite certificate(huge,guard);
            check(certificate.rank()==1 && certificate.stats().factor_nonzeros==1,"huge logical carrier retains one factor");
            check(certificate.stats().peak_rational_entries<=4,"no whole-carrier PSD scratch");
            check(certificate.quadratic_form(SparseRationalVector(n,{{n-1,Rational{3,2}}}),guard)==Rational{21,4},"huge carrier sparse energy");
            const PreparedPositiveSemidefinite zero(ExactMatrix(n,n),limits);
            check(zero.rank()==0 && zero.stats().factor_nonzeros==0,"zero PSD carrier is implicit");
        }
        std::cout<<"positive semidefinite: "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
