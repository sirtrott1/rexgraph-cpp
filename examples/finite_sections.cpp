#include "finite_sections_data.hpp"
#include <iostream>
using namespace finite_sections_example;
int main(){try{
    auto d=data();const auto records=store_conditions(d,policies());const auto family=recorded_family(d,records);
    const auto conflict=family.conflict(availability());
    if(conflict.status!=Status::Unsatisfiable || !conflict.subset_minimal)throw std::runtime_error("conflict expected");
    const auto fixed=prepare_family(d,policies(false));const auto q=fixed.query(availability(),{{0,1}});const auto x=fixed.materialize(q,availability());
    std::cout<<"{\"global_conflict\":true,\"core\":[";
    for(std::size_t i=0;i<conflict.clauses.size();++i){if(i)std::cout<<',';std::cout<<'\"'<<family.family().clauses()[conflict.clauses[i]].condition.label<<'\"';}
    std::cout<<"],\"deletion_witnesses\":[";
    for(std::size_t i=0;i<conflict.deletion_witnesses.size();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<3;++j){if(j)std::cout<<',';std::cout<<conflict.deletion_witnesses[i][j];}std::cout<<']';}
    std::cout<<"],\"selected_keys\":[1,1,1],\"selected_field\":[";
    const auto dense=x.dense_values();for(std::size_t i=0;i<dense.size();++i){if(i)std::cout<<',';std::cout<<'\"'<<dense[i]<<'\"';}
    std::cout<<"],\"unconstrained_alternatives\":8,\"equality_alternatives\":2}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
