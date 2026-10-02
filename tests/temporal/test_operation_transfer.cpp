#include "rfc/temporal/operation_word.hpp"
#include "rfc/temporal/transfer.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::Rational; using rfc::numeric::RationalVector;
    const auto O=ExactMatrix::from_rows({{1},{2}});
    const auto V=ExactMatrix::from_rows({{1,1}});
    const auto Op=ExactMatrix::from_rows({{1,0},{0,1},{1,1}});
    const auto Vp=ExactMatrix::from_rows({{1,0,1}});
    const auto J0=ExactMatrix::from_rows({{1},{0}});
    const auto J1=ExactMatrix::from_rows({{1,0},{0,1},{0,0}});
    const auto J2=ExactMatrix::from_rows({{1}});
    const rfc::temporal::OperationWord old_word({O,V});
    const rfc::temporal::OperationWord new_word({Op,Vp});
    const auto defect=rfc::temporal::finite_word_defect(old_word,new_word,{J0,J1,J2});
    require(defect.exact_identity(),"finite temporal word identity failed");
    require(defect.contributions.size()==2,"word contribution count mismatch");
    require(defect.contributions[0].get(0,0)==1 && defect.contributions[1].get(0,0)==-2,"word contribution values mismatch");
    require(defect.direct.get(0,0)==-1,"word direct defect mismatch");

    const auto I=ExactMatrix::from_rows({{1},{0}});
    const auto Ip=ExactMatrix::from_rows({{1},{1}});
    const auto J=ExactMatrix::identity(2);
    const auto K=ExactMatrix::from_rows({{1}});
    const auto source=rfc::temporal::source_injection_delta(I,Ip,J,K,RationalVector{2},RationalVector{3});
    require(source.representation_term==RationalVector({0,2}),"source representation term mismatch");
    require(source.innovation_term==RationalVector({1,1}),"source innovation term mismatch");
    require(source.aggregate==RationalVector({1,3}),"source aggregate mismatch");

    const auto A=ExactMatrix::from_rows({{1,0}});
    const auto Ap=ExactMatrix::from_rows({{0,1}});
    const auto KA=ExactMatrix::from_rows({{1}});
    const auto obs=rfc::temporal::observation_delta(A,Ap,J,KA,RationalVector{2,0},RationalVector{3,3});
    require(obs.aggregate==RationalVector{Rational{1}},"observation delta mismatch");
    require(obs.aggregate==rfc::numeric::sub(Ap.apply(RationalVector{3,3}),KA.apply(A.apply(RationalVector{2,0}))),
            "complete observation equation residual mismatch");

    const auto M=rfc::field::ExactMetric::identity(2);
    const auto Mp=rfc::field::ExactMetric::diagonal({2,3});
    const auto metric_defect=rfc::temporal::metric_pullback_defect(M,Mp,J);
    require(metric_defect.get(0,0)==1 && metric_defect.get(1,1)==2,"metric pullback defect mismatch");

    const auto L=ExactMatrix::diagonal({1,2});
    const auto Lp=ExactMatrix::diagonal({2,2});
    const auto II=ExactMatrix::identity(2);
    const auto green=rfc::temporal::green_response_delta(L,Lp,J,II,II,J,RationalVector{1,2},RationalVector{2,1},Rational{1});
    require(green.aggregate==rfc::numeric::sub(green.new_response,green.old_response),"Green response delta mismatch");
    const auto observed=rfc::temporal::observed_green_response_delta(green,A,Ap,J,KA);
    require(observed.aggregate==rfc::numeric::sub(Ap.apply(green.new_response),A.apply(green.old_response)),"observed Green delta mismatch");

    std::cout<<"operation word/transfer tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
