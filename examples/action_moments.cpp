#include "action_moment_data.hpp"
#include <iostream>
using namespace action_moment_example;
int main() { try {
    const auto e=make(); storage::NativeAccessionDirectory directory(e,900);
    const auto x=input(directory); const auto p=plans(directory);
    const NativeActionAvailability available{{900,7}};
    const auto before=p.old_third.execute(x,available), after=p.new_third.execute(x,available);
    std::cout<<"{\"old_third\":\""<<before.total<<"\",\"new_third\":\""<<after.total
        <<"\",\"components\":[";
    for(std::size_t i=0;i<after.components.size();++i) {
        const auto& row=after.components[i]; if(i)std::cout<<',';
        std::cout<<"{\"label\":\""<<row.label<<"\",\"self\":\""<<row.self
            <<"\",\"interaction_with_previous\":\""<<row.with_previous<<"\",\"increment\":\""<<row.increment<<"\"}";
    }
    std::cout<<"],\"matches_direct\":"<<(*after.matches_reference?"true":"false")
        <<",\"steps\":"<<after.execution_steps<<",\"retained_sparse_entries\":"<<after.retained_sparse_entries<<"}\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
