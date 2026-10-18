#include "rfc/topology/rational_zigzag.hpp"
#include <iostream>
#include <limits>

using namespace rfc;
using Matrix=numeric::ExactMatrix;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    try{
        const numeric::ExactActionLimits policy{10000,10000000,100000000,1000000};
        const std::vector<std::pair<std::size_t,std::size_t>> expected{{0,0},{0,2},{1,3},{3,3},{0,3},{1,2},{1,2}};
        for(unsigned orientation=0;orientation<8;++orientation){
            numeric::ExactActionGuard guard(policy);std::vector<std::vector<std::size_t>> active(4);
            std::vector<std::size_t> dimensions;std::vector<Matrix> bases;
            for(std::size_t i=0;i<4;++i){
                for(std::size_t j=0;j<expected.size();++j)if(expected[j].first<=i && i<=expected[j].second)active[i].push_back(j);
                dimensions.push_back(active[i].size());auto b=Matrix::identity(dimensions.back());
                for(std::size_t j=1;j<b.rows();++j)b.set(j,j-1,numeric::Rational{static_cast<long>(j+orientation+1),2});
                bases.push_back(std::move(b));
            }
            std::vector<topology::RationalZigzagArrow> arrows;
            for(std::size_t i=0;i<3;++i){const bool forward=((orientation>>i)&1)!=0;const auto s=forward?i:i+1,t=forward?i+1:i;
                Matrix d(dimensions[t],dimensions[s]);
                for(std::size_t p=0;p<active[t].size();++p)for(std::size_t q=0;q<active[s].size();++q)if(active[t][p]==active[s][q])d.set(p,q,1);
                numeric::PreparedExactSolve solve(bases[s],guard);
                auto inverse=topology::zigzag_detail::solve_columns(solve,Matrix::identity(dimensions[s]),guard);
                auto map=numeric::checked_multiply(numeric::checked_multiply(bases[t],d,guard),inverse,guard);
                arrows.push_back({forward,std::move(map)});
            }
            topology::PreparedRationalZigzag diagram(dimensions,arrows,guard);diagram.validate(guard);
            std::map<std::pair<std::size_t,std::size_t>,std::size_t> wanted,actual;
            for(const auto& interval:expected)++wanted[interval];
            for(const auto& interval:diagram.intervals())actual[{interval.begin,interval.end}]=interval.generators.front().cols();
            require(actual==wanted,"interval multiplicities differ under orientation/basis changes");
        }
        for(bool forward:{false,true}){
            numeric::ExactActionGuard guard(policy);
            topology::PreparedRationalZigzag zero({2,3},{{forward,Matrix(forward?3:2,forward?2:3)}},guard);
            require(zero.intervals().size()==2,"zero map must retain both independent births");
        }
        numeric::ExactActionGuard guard(policy);
        topology::PreparedRationalZigzag empty({0,0},{{false,Matrix(0,0)}},guard);
        require(empty.intervals().empty(),"zero diagram");
        topology::PreparedRationalZigzag single({3},{},guard);
        require(single.intervals().size()==1 && single.intervals()[0].generators[0].cols()==3,"single vertex");
        topology::PreparedRationalZigzag large({5000},{},guard);
        require(large.intervals()[0].generators[0].nonzeros()==5000,"actual caller dimension beyond the default action profile");
        bool refused=false;try{topology::PreparedRationalZigzag bad({2,3},{{true,Matrix(2,3)}},guard);(void)bad;}catch(const std::invalid_argument&){refused=true;}
        require(refused,"malformed arrow accepted");
        refused=false;try{numeric::ExactActionGuard tiny({10,0,100,100});topology::PreparedRationalZigzag bad({2},{},tiny);(void)bad;}catch(const std::length_error&){refused=true;}
        require(refused,"zero entry policy accepted");
        refused=false;try{numeric::ExactActionGuard cancelled(policy,[]{throw std::runtime_error("cancelled");});topology::PreparedRationalZigzag bad({2},{},cancelled);(void)bad;}catch(const std::runtime_error&){refused=true;}
        require(refused,"cancellation ignored");
        std::cout<<"rational zigzag orientations, reconstruction and refusals passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
