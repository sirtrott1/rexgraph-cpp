#include "fixture.hpp"
#include <iostream>

int main() {
    using namespace native_covering_homotopy_example;
    const auto p = prepare(6); const auto a = p.preparation_availability(); const auto c = p.domain();
    const auto input = c.chain(native_chain(c,grade(0),{{0,Q{1}}}),a.original.cylinder);
    const auto result = p.witness(input,a); const auto& w = result.witness().witness();
    std::cout << "exact_lifted_identity=" << zero(w.residual().values()) << '\n';
    std::cout << "start_vertex=" << w.first().values().begin()->first << '\n';
    std::cout << "finish_vertex=" << w.second().values().begin()->first << '\n';
    std::cout << "retained_original=" << p.original().same_owner(result.plan().original()) << '\n';
    std::cout << "publication_sequences=";
    for (std::size_t i = 0; i < result.provenance().sequences.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << result.provenance().sequences[i];
    }
    std::cout << '\n';
}
