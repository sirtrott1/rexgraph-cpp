#include "finite_cover_data.hpp"
#include <iostream>
using namespace finite_cover_example;
int main(){try{
    const auto d=make_cover();const auto p=plan_cover(d);const auto r=cover_result(p,d);
    std::cout<<"{\"checkpoint\":\"finite-cover\",\"local_dimensions\":[3,3,3],\"interface_dimensions\":[2,1,1],\"glued_dimension\":"<<p.view().dimension(g1)<<",\"total\":\""<<r.total<<"\",\"self\":[";
    for(std::size_t i=0;i<r.components.size();++i){if(i)std::cout<<',';std::cout<<'"'<<r.components[i].self<<'"';}
    std::cout<<"],\"input\":[";bool first=true;for(const auto& e:r.input.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"output\":[";first=true;for(const auto& e:r.aggregate.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"reference_equal\":"<<(*r.matches_reference?"true":"false")<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
