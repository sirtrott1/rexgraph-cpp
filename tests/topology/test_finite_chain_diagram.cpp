#include "rfc/topology/finite_chain_diagram.hpp"
#include <iostream>

using namespace rfc;
using I=numeric::IntegerMatrix;
using Q=numeric::ExactMatrix;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static topology::IntegralChainVertex torsion(long order){I boundary(1,1);boundary.set(0,0,order);return {{I(0,1),std::move(boundary)}};}
int main(){try{
    const numeric::ExactActionLimits policy{1000,100000,1000000,100000};
    for(const auto& data:std::vector<std::vector<long>>{{4,2,1,2},{2,4,2,1},{4,4,2,2},{4,2,0,0}}){
        numeric::ExactActionGuard guard(policy);I f0(1,1),f1(1,1);f0.set(0,0,data[2]);f1.set(0,0,data[3]);
        topology::PreparedIntegralModuleDiagram diagram({torsion(data[0]),torsion(data[1])},{{true,{f0,f1}}},0,guard);
        require(diagram.modules()[0].torsion==std::vector<numeric::Integer>{data[0]} && diagram.modules()[1].torsion==std::vector<numeric::Integer>{data[1]},"torsion presentations lost");
        require(diagram.induced()[0].smith_map.get(0,0)==data[2],"Smith induced map differs");
        for(long x=0;x<data[0];++x){require((diagram.induced()[0].smith_map.get(0,0)*x)%data[1]==(data[2]*x)%data[1],"residue representative differs");
            require((data[2]*(x+data[0]))%data[1]==(data[2]*x)%data[1],"boundary equivalence differs");}
    }
    numeric::ExactActionGuard guard(policy);
    bool refused=false;I one=I::identity(1);try{topology::PreparedIntegralModuleDiagram bad({torsion(4),torsion(2)},{{true,{one,one}}},0,guard);(void)bad;}catch(const std::invalid_argument&){refused=true;}
    require(refused,"invalid integral chain map accepted");
    I lower_source(1,3),lower_target(1,3),upper_source(3,1),upper_target(3,1),mixed_map(3,3),upper_map(1,1);
    lower_source.set(0,0,1);lower_source.set(0,1,1);lower_target.set(0,0,1);lower_target.set(0,2,1);
    upper_source.set(0,0,-4);upper_source.set(1,0,4);upper_target.set(0,0,-2);upper_target.set(2,0,2);
    mixed_map.set(0,0,1);mixed_map.set(2,1,1);mixed_map.set(0,2,-3);mixed_map.set(1,2,1);mixed_map.set(2,2,3);upper_map.set(0,0,2);
    topology::PreparedIntegralModuleDiagram mixed({{{I(0,1),lower_source,upper_source}},{{I(0,1),lower_target,upper_target}}},
        {{true,{I::identity(1),mixed_map,upper_map}}},1,guard);
    require(mixed.modules()[0].free_rank==1 && mixed.modules()[0].torsion==std::vector<numeric::Integer>{4} &&
        mixed.modules()[1].free_rank==1 && mixed.modules()[1].torsion==std::vector<numeric::Integer>{2},"mixed free/torsion modules differ");
    I expected_mixed(2,2);expected_mixed.set(0,0,1);expected_mixed.set(0,1,3);expected_mixed.set(1,1,1);
    require(mixed.induced()[0].smith_map==expected_mixed,"induced map lost Smith basis transport or free to torsion contribution");
    Q d0(0,2),d1(2,1);d1.set(0,0,-1);d1.set(1,0,1);
    topology::RationalChainVertex point{{Q(0,1),Q(1,0)}},edge{{d0,d1}};
    Q inclusion(2,1);inclusion.set(0,0,1);
    topology::PreparedRationalChainDiagram fork({point,edge,point},{{true,{inclusion,Q(1,0)}},{false,{inclusion,Q(1,0)}}},0,guard);
    require(fork.zigzag().intervals().size()==1 && fork.zigzag().intervals()[0].begin==0 && fork.zigzag().intervals()[0].end==2,"induced H0 fork differs");
    topology::PreparedRationalChainDiagram acyclic({edge},{},1,guard);require(acyclic.zigzag().intervals().empty(),"acyclic edge H1");
    Q b0(0,1),b1(1,1),b2(1,1);b1.set(0,0,1);b2.set(0,0,1);
    refused=false;try{topology::PreparedRationalChainDiagram bad({{{b0,b1,b2}}},{},1,guard);(void)bad;}catch(const std::invalid_argument&){refused=true;}
    require(refused,"failed boundary square accepted");
    std::cout<<"chain quotients, torsion, induced maps and refusals passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
