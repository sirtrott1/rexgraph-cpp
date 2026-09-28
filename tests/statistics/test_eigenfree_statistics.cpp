#include "rfc/statistics/ensemble.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

int main(){
    try{
        using rfc::numeric::ExactMatrix;
        const auto G=ExactMatrix::from_rows({{2,1},{1,3}});
        const auto S=ExactMatrix::from_rows({{rfc::numeric::rational(1,2),0},{0,rfc::numeric::rational(1,2)}});
        const auto stats=rfc::statistics::effective_dimension_2(G,S);
        require(stats.variance==rfc::numeric::rational(5,2),"variance mismatch");
        require(stats.second_contraction==rfc::numeric::rational(15,4),"second contraction mismatch");
        require(stats.dimension==rfc::numeric::rational(5,3),"effective dimension mismatch");
        require(stats.collision_probability==rfc::numeric::rational(3,5),"collision probability mismatch");
        std::cout<<"eigenfree statistics tests passed\n";
        return EXIT_SUCCESS;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
