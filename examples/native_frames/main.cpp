#include "fixture.hpp"
#include <iostream>

int main(){try{
    using namespace native_frame_example;
    const auto square=native_green_example::square();
    for(const auto* source:{&square.first.source,&square.second.source}){
        rfc::storage::NativeAccessionDirectory directory(endpoint(*source,0),900);
        const rfc::runtime::WorkAvailability available{900,7};const auto field=input(directory,rfc::core::Grade{1});
        const auto plan=rfc::inference::PreparedNativeHodgeFrames::prepare(field,request(directory,rfc::core::Grade{1},0),available);
        const auto result=plan.execute(field,available);const auto& report=result.provenance();
        std::cout<<"face_count="<<source->grade(rfc::core::Grade{2}).cell_count()<<" gradient="<<report.gradient_dimension
                 <<" curl="<<report.curl_dimension<<" harmonic="<<report.harmonic_dimension
                 <<" occurrences="<<source->grade(rfc::core::Grade{1}).incidence_count()<<'\n';
        const auto print=[](const char* name,const Vector& values){std::cout<<name<<'=';for(const auto& value:values)std::cout<<' '<<value;std::cout<<'\n';};
        print("gradient_field",result.values().gradient);print("curl_field",result.values().curl);
        print("harmonic_field",result.values().harmonic);print("reconstructed",result.values().reconstructed);
        if(result.values().reconstructed!=Vector{1,0,0,0})throw std::logic_error("square reconstruction failed");
    }
    rfc::storage::NativeAccessionDirectory directory(endpoint(higher(),2),900);
    const auto field=input(directory,rfc::core::Grade{2});const rfc::runtime::WorkAvailability available{900,7};
    const auto plan=rfc::inference::PreparedNativeHodgeFrames::prepare(field,request(directory,rfc::core::Grade{2},2),available);
    const auto result=plan.execute(field,available);
    std::cout<<"higher_chain_interfaces="<<result.provenance().chain_interfaces_checked
             <<" preparation_steps="<<plan.preparation_steps()<<" execution_steps="<<result.provenance().execution_steps<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
