#include "fixture.hpp"
#include <iostream>

int main() {
    using namespace native_homotopy_composition_example;
    const auto plan = prepare(1); const auto domain = plan.domain();
    const auto input = domain.chain(native_chain(domain,grade(2),{{0,Q{3,5}}}),available.domain);
    const auto result = plan.witness(input,available); const auto& w = result.witness().witness();
    std::cout << "exact_composed_identity=" << zero(w.residual().values()) << '\n';
    std::cout << "target_homotopy_grade=" << w.homotopy().grade().value << '\n';
    std::cout << "retained_original=" << result.plan().original().same_owner(plan.original()) << '\n';
    std::cout << "original_sequences=";
    for (std::size_t i = 0; i < result.provenance().sequences.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << result.provenance().sequences[i];
    }
    std::cout << '\n';
}
