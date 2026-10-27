#include "injective_gluing_data.hpp"
#include <iostream>
using namespace injective_gluing_example;
int main(){try{
    const auto data=make_dataset();const auto p=plan(data);const auto r=result(p,data);
    std::cout<<"{\"checkpoint\":\"injective-gluing\",\"shared_dimension\":1,\"local_dimensions\":[2,3],\"pushout_dimension\":"<<p.view().dimension(g1)
      <<",\"total\":\""<<r.total<<"\",\"self\":[\""<<r.components[0].self<<"\",\""<<r.components[1].self<<"\"],\"input\":[";
    bool first=true;for(const auto& e:r.input.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"output\":[";first=true;for(const auto& e:r.aggregate.sparse_values().entries()){if(!first)std::cout<<',';first=false;std::cout<<'"'<<e.value<<'"';}
    std::cout<<"],\"reference_equal\":"<<(*r.matches_reference?"true":"false")<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
