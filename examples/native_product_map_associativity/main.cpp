#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_associativity_example;
    try {
        const auto p = native_associativity_example::plans(6).associativity;
        const rfc::core::CellId vertex{{0},0};
        const auto input = p.prefix().first().source().field(carrier(0),{0},
            {{cell(carrier(0),{vertex,vertex,vertex}),Q{1}}},available_associativity[0]);
        const auto prior = std::make_shared<const rfc::inference::NativeProductCompositionLift>(
            p.prefix().apply(input,{available_associativity[0],available_associativity[1],available_associativity[2]},associativity_lease()));
        const auto result = p.continue_from(prior,available_associativity,associativity_lease());
        const auto numeric = p.numeric_plan();
        std::cout << "original_endpoint_roles=12\nparenthesizations=2\ntriple_factor_paths=" << numeric.factors()[0][0].contributions.size()
            << "\nsupported_zero_output_coordinates=" << result.last_stage()->lift().image().values().size()
            << "\noriginal_receipt_adopted=" << (result.prefix_input()==prior)
            << "\nprior_map_receipt_retained=" << (result.last_stage()->prior_input()==prior->stages()[1]) << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
