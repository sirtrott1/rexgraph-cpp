#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_composition_example;
    try {
        const auto p = plans(6).composition;
        const rfc::core::CellId vertex{{0},0};
        const auto input = p.first().source().field(carrier(0),{0},{{cell(carrier(0),{vertex,vertex,vertex}),Q{1}}},available_composition.source);
        const auto result = p.witness(input,available_composition,composition_lease());
        const auto numeric = p.numeric_plan(); const auto& factor = numeric.factors()[0][0];
        std::cout << "original_endpoint_roles=9\nfactor_contributions=" << factor.contributions.size()
            << "\ncomposed_nonzeros=" << factor.matrix.nonzeros() << "\ncomposed_supported_positions=" << factor.support.size()
            << "\nretained_output_coordinates=" << result.stages()[1]->lift().image().values().size()
            << "\nprior_receipt_retained=" << (result.stages()[1]->prior_input()==result.stages()[0])
            << "\ncoherence_routes=" << result.witness().routes().size() << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
