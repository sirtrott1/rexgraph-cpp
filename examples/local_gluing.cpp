#include "local_gluing_data.hpp"
#include <iostream>
int main(){try{
    using namespace local_gluing_example;
    std::vector<Fixture> fs{make(100,10,20),make(200,30,40),make(300,20,30)};
    const auto all=make_plan(fs,{vertex_interface(fs,0,2,20),vertex_interface(fs,1,2,30)});
    const std::vector<Fixture> ab{fs[0],fs[1]};const auto before=make_plan(ab,{});
    const auto m=moments(all,fs),old=moments(before,ab);const auto result=third(all,fs);
    std::cout<<"{\"before\":["<<old[0]<<','<<old[1]<<','<<old[2]<<"],\"after\":["<<m[0]<<','<<m[1]<<','<<m[2]<<"],\"components\":[";
    for(std::size_t i=0;i<result.components.size();++i){if(i)std::cout<<',';std::cout<<result.components[i].self;}
    std::cout<<"],\"total\":"<<result.total<<",\"matches\":"<<(result.matches_reference==true?"true":"false")<<",\"vertices\":"<<all.view().cell_count(g0)<<",\"relations\":"<<all.view().cell_count(g1)<<",\"occurrences\":"<<all.view().occurrence_count(g1)<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
