#include "chart_gluing_data.hpp"
#include <iostream>
using namespace chart_gluing_example;
int main(){try{
    const auto a=changed(make(100,10,20),0,10900),c=changed(make(300,20,30),2,10902);
    std::vector<Fixture> fs{a.field,make(200,30,40),c.field};auto requests=a.charts;requests.insert(requests.end(),c.charts.begin(),c.charts.end());
    const auto p=plan(fs,{vertex_interface(fs,0,2,20),vertex_interface(fs,1,2,30)},requests);
    const auto m=moments(p,fs);const auto r=third(p,fs);
    Guard g;const auto Smap=coordinate_change(2),inverse=inverse_change(2);const S x(2,{{0,1},{1,2}});
    const auto local=numeric::checked_apply(Smap,x,g);const auto back=numeric::checked_apply(inverse,local,g);
    std::cout<<"{\"checkpoint\":\"chart-gluing\",\"moments\":[\""<<m[0]<<"\",\""<<m[1]<<"\",\""<<m[2]
      <<"\"],\"contributions\":[\""<<r.components[0].self<<"\",\""<<r.components[1].self<<"\",\""<<r.components[2].self
      <<"\"],\"reference_equal\":"<<(*r.matches_reference?"true":"false")
      <<",\"local_shear\":[\""<<local.entries()[0].value<<"\",\""<<local.entries()[1].value
      <<"\"],\"recovered\":[\""<<back.entries()[0].value<<"\",\""<<back.entries()[1].value<<"\"]}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
