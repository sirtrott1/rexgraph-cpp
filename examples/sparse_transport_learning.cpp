#include "sparse_transport_data.hpp"
#include <iostream>
int main(){try{
    using namespace sparse_transport_example;
    std::cout<<"{\"cases\":[";bool first=true;
    for(const std::size_t n:{7ULL,1000003ULL,1000000000000000000ULL}) {
        const auto l=limits(n);Guard g(l);Fit base(model(n,g),{},g);const auto obs=samples(n);
        Guard g1(l);const auto one=base.append_sparse({obs[0]},g1);
        Guard g2(l);const auto two=one.append_sparse({obs[1]},g2);
        Guard replay(l);const auto all=base.append_sparse(obs,replay);
        if(two.particular()!=all.particular() || two.kernel()!=all.kernel())throw std::logic_error("replay differs");
        if(!first)std::cout<<',';
        first=false;
        std::cout<<"{\"extent\":"<<n<<",\"dimensions\":["<<base.kernel().cols()<<','<<one.kernel().cols()<<','<<two.kernel().cols()
          <<"],\"rows\":"<<two.design().rows()<<",\"retained\":"<<two.retained_entries()<<",\"prepare_steps\":"<<g.steps()
          <<",\"append_steps\":["<<g1.steps()<<','<<g2.steps()<<"],\"particular\":[";
        for(std::size_t i=0;i<two.particular().size();++i){if(i)std::cout<<',';std::cout<<'"'<<two.particular()[i]<<'"';}
        std::cout<<"],\"free_parameters\":[";
        for(std::size_t i=0;i<two.free_columns().size();++i){if(i)std::cout<<',';std::cout<<two.free_columns()[i];}
        std::cout<<"]}";
    }
    std::cout<<"]}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
