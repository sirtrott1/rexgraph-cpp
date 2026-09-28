#include "rfc/field/low_rank_update.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::RationalVector;
    const auto A=ExactMatrix::from_rows({{3,1,0},{1,4,1},{0,1,5}});
    const auto U=ExactMatrix::from_rows({{1},{0},{1}});
    const auto C=ExactMatrix::from_rows({{2}});
    const auto V=ExactMatrix::from_rows({{0},{1},{1}});
    const RationalVector rhs{1,2,3};
    const auto update=rfc::field::low_rank_updated_matrix(A,U,C,V);
    const auto direct=update.solve(rhs);
    const auto woodbury=rfc::field::woodbury_solve(A,U,C,V,rhs);
    require(direct==woodbury,"Woodbury exact solve disagrees with direct solve");
    require(update.apply(woodbury)==rhs,"Woodbury exact residual failed");
    std::cout<<"low-rank update tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
