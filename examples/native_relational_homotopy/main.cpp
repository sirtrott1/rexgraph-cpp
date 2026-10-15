#include "fixture.hpp"
#include <iostream>

int main() {
    try {
        using namespace native_relational_homotopy_example;
        const auto plan = prepare(1); const auto cylinder = plan.cylinder();
        const auto input = cylinder.chain(native_chain(cylinder,grade(1),{{0,Q{3,5}},{1,Q{-3,5}}}),available.cylinder);
        const auto result = plan.witness(input,available); const auto& witness = result.witness();
        std::cout << "source_relations=" << cylinder.factor(false).endpoint()->source.grade(grade(1)).cell_count() << '\n'
                  << "carrier_cells=" << plan.declaration().realization.cells.size() << '\n'
                  << "carrier_occurrences=" << plan.declaration().realization.occurrences.size() << '\n'
                  << "target_homotopy_grade=" << witness.homotopy().grade().value << '\n'
                  << "residual_support=" << witness.residual().values().size() << '\n'
                  << "exact_identity=" << zero(witness.residual().values()) << '\n'
                  << "original_sequences=";
        for(std::size_t i=0;i<4;++i)std::cout<<(i?",":"")<<result.provenance().sequences[i];
        std::cout<<'\n'; return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
