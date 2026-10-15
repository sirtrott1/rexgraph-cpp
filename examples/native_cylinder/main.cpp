#include "fixture.hpp"
#include <iostream>
int main() {
    try {
        using namespace native_cylinder_example;
        const auto plan=prepare(branch());const auto numeric=plan.numeric_plan();
        const auto input=plan.chain(native_chain(plan,grade(1),{{0,Q{1}}}),available);
        Guard guard;const auto routes=numeric.retained_prism_boundary({grade(1),0},guard);
        const auto witness=plan.homotopy_witness(input,available);
        std::cout<<"branch_relations="<<numeric.source().grade(grade(1)).cell_count()
            <<"\nprism_routes="<<routes.size()<<"\nstart_support="<<witness.witness().start().values().size()
            <<"\nfinish_support="<<witness.witness().finish().values().size()
            <<"\nhomotopy_residual="<<(zero(witness.witness().residual().values())?0:1)<<'\n';
        return zero(witness.witness().residual().values())?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
