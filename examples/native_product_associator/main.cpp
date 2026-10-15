#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_associator_example;
    const auto p = plan(); const auto numeric = p.numeric_plan();
    const auto& c = numeric.cells().back();
    const auto flags = p.cell_witness(c.left, available);
    const auto input = p.field(Association::Left, c.left.grade(), {{c.left,Q{1}}}, available);
    const auto witness = p.witness(input, available);
    if (witness.witness().round_trip().values() != input.field().values() ||
        witness.witness().mapped_boundary()->values() != witness.witness().image_boundary()->values()) return 1;
    std::cout << "ordered_factors=3\ncomplete_type_pairs=" << numeric.declaration().types.size()
        << "\nretained_top_flags=" << flags.witness().flags().size()
        << "\nexact_chain_naturality=1\nindependent_cutoffs=3\n";
}
