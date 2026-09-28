#include "rfc/statistics/operation_moments.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::Rational; using rfc::numeric::RationalVector;
    const RationalVector d0{Rational{-1,2},Rational{1,4},Rational{1,4}};
    const RationalVector d1{Rational{1,2},Rational{-1,2},0};
    const auto gram=rfc::statistics::channel_field_gram({d0,d1},ExactMatrix::identity(3));
    require(gram.get(0,0)==Rational{3,8},"operation moment d0 diagonal mismatch");
    require(gram.get(0,1)==Rational{-3,8} && gram.get(1,0)==Rational{-3,8},"operation moment interaction mismatch");
    require(gram.get(1,1)==Rational{1,2},"operation moment d1 diagonal mismatch");
    Rational all=0, diagonal=0;
    for(std::size_t i=0;i<2;++i) for(std::size_t j=0;j<2;++j) all+=gram.get(i,j);
    for(std::size_t i=0;i<2;++i) diagonal+=gram.get(i,i);
    require(all==Rational{1,8} && diagonal==Rational{7,8},"operation moment contraction mismatch");
    std::cout<<"operation moment tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
