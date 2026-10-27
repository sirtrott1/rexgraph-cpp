#include "colimit_gluing_data.hpp"
#include <iostream>
using namespace colimit_example;
int main(){try{
    const auto d=dataset();const auto p=plan_unsplit(d);const auto r=result_unsplit(p,d);
    std::cout<<"{\"checkpoint\":\"certified-cover\",\"quotient_dimension\":"<<p.view().dimension(g1)<<",\"total\":\""<<r.total<<"\",\"self\":[";
    for(std::size_t i=0;i<r.components.size();++i){if(i)std::cout<<',';std::cout<<'"'<<r.components[i].self<<'"';}
    std::cout<<"],\"interactions\":[";for(std::size_t i=0;i<r.interactions.size();++i){if(i)std::cout<<',';std::cout<<'"'<<r.interactions[i].doubled<<'"';}
    std::cout<<"],\"input\":[";bool first=true;for(const auto& e:r.input.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"output\":[";first=true;for(const auto& e:r.aggregate.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"reference_equal\":"<<(*r.matches_reference?"true":"false")<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
