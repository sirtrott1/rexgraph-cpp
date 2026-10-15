#include "fixture.hpp"
#include <iostream>

namespace {
using namespace native_metric_example;
void print(const char* name,const Vector& values) {
    std::cout<<name<<'=';for(const auto& value:values)std::cout<<' '<<value;std::cout<<'\n';
}
}
int main(){try{
    using namespace rfc;
    using namespace native_metric_example;
    const auto J=Matrix::from_rows({{1,0},{0,1},{1,1}}),M=Matrix::from_rows({{2,0,0},{0,3,0},{0,0,4}});
    storage::NativeAccessionDirectory old_directory(endpoint(Matrix::identity(2),{2,3}),900),
        new_directory(endpoint(M,{3,2,8},8),901);
    const auto result=inference::PreparedNativeMetricNovelty::prepare(request(old_directory,new_directory,J),available)
        .execute(input(old_directory),input(new_directory),available);
    const auto& values=result.values();
    if(values.persistent!=Vector{Q{31,13},Q{-1,13},Q{30,13}} ||
       values.complement!=Vector{Q{-18,13},Q{-12,13},Q{9,13}} || values.quadrance!=41)return 1;
    print("innovation",values.innovation);print("persistent",values.persistent);print("complement",values.complement);
    std::cout<<"persistent_quadrance="<<values.pairings.get(0,0)<<"\ncomplement_quadrance="<<values.pairings.get(1,1)
             <<"\ncross_pairing="<<values.pairings.get(0,1)<<"\ntotal_quadrance="<<values.quadrance<<'\n';
    const auto endpoints=square();
    storage::NativeAccessionDirectory old_cause(endpoints.first,900),new_cause(endpoints.second,901);
    const auto causes=inference::PreparedNativeMetricNovelty::prepare(request(old_cause,new_cause,Matrix::identity(4)),available)
        .execute_causes(input(old_cause),input(new_cause),{input(new_cause,200),input(new_cause,201)},available);
    const auto& pairs=*causes.cause_pairings();
    if(pairs.pairings!=Matrix::from_rows({{16,4},{4,2}}) || pairs.quadrance!=26)return 1;
    std::cout<<"cause_gram="<<pairs.pairings.get(0,0)<<' '<<pairs.pairings.get(0,1)<<' '
             <<pairs.pairings.get(1,0)<<' '<<pairs.pairings.get(1,1)<<"\ncause_total_quadrance="<<pairs.quadrance<<'\n';
    std::cout<<"source_grade="<<causes.provenance().new_endpoint.grade.value
             <<" source_occurrences="<<causes.new_input().endpoint()->source.grade(core::Grade{1}).incidence_count()<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
