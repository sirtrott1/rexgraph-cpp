#include "fixture.hpp"
#include <iostream>

int main(){try{
    using namespace native_green_example;
    auto pair=square();rfc::storage::NativeAccessionDirectory old_directory(pair.first,900),new_directory(pair.second,900);
    const auto available=rfc::inference::NativeGreenAvailability{{900,7},{900,8}};
    const auto green=rfc::inference::PreparedNativeGreenResponseDelta::prepare(request(old_directory,new_directory,M::identity(4),M::identity(1)),available);
    const auto observed=rfc::inference::PreparedNativeObservedGreenResponseDelta::prepare(green,op(old_directory,3),op(new_directory,3),
        comparison(op(old_directory,1),op(new_directory,1),30,30,M::identity(2)),available);
    const auto response=green.execute(input(old_directory),input(new_directory),available);
    const auto output=observed.execute(input(old_directory),input(new_directory),available);
    if(response.values().aggregate!=V{Q{-17,15},Q{-17,15},Q{-22,15},Q{-22,15}} || output.values().aggregate!=V{Q{-29,15},Q{-29,15}})
        throw std::logic_error("paper response identity mismatch");
    const auto print=[](const char* label,const V& value){std::cout<<label<<'=';for(const auto& q:value)std::cout<<' '<<q;std::cout<<'\n';};
    std::cout<<"namespace=900 old_sequence=7 new_sequence=8 old_occurrences="<<pair.first.source.grade(rfc::core::Grade{1}).incidence_count()
             <<" new_face_occurrences="<<pair.second.source.grade(rfc::core::Grade{2}).incidence_count()<<'\n';
    print("old_response",response.values().old_response);print("new_response",response.values().new_response);
    print("realization",response.values().realization_term);print("innovation",response.values().innovation_term);
    print("operator",response.values().operator_term);print("response_change",response.values().aggregate);
    print("accession",output.values().accession_term);print("observed_realization",output.values().realization_term);
    print("observed_innovation",output.values().innovation_term);print("observed_operator",output.values().operator_term);
    print("observed_change",output.values().aggregate);
    std::cout<<"green_preparation_steps="<<green.preparation_steps()<<" observation_preparation_steps="<<observed.preparation_steps()
             <<" execution_steps="<<output.provenance().execution_steps<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
