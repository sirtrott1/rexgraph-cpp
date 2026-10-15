#include "fixture.hpp"
#include <iostream>
int main(){
    try{
        using namespace native_relative_example;
        const auto plan=prepare();const auto numeric=plan.numeric_plan();
        std::cout<<"quotient_cells=";
        for(std::uint16_t k=0;k<numeric.ambient().grade_count();++k){if(k)std::cout<<',';std::cout<<numeric.coordinates(Space::Quotient,grade(k)).size();}
        const auto relative=plan.project(native_chain(plan,grade(2),{{0,Q{1}}}),available);
        const auto d=plan.boundary(relative,available);const auto witness=plan.connecting_witness(relative,available);
        Guard guard;const auto dd=numeric.boundary(witness.witness().sub_boundary(),guard);
        std::cout<<"\nrelative_face_support="<<relative.chain().values().size()<<"\nrelative_boundary_support="<<d.chain().values().size()
                 <<"\nconnecting_boundary_support="<<witness.witness().sub_boundary().values().size()<<"\nboundary_squared="<<(zero(dd.values())?0:1)<<'\n';
        return zero(dd.values())?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
