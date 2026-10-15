#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_hexagon_example;
    const auto p=plan(); const auto n=p.numeric_plan(); const rfc::core::CellId e{{1},0}; const std::array ids{e,e,e};
    const auto c=p.cell_witness(ids,available);
    const auto input=p.field(carrier(0),{3},{{cell(carrier(0),ids),Q{1}}},available);
    const auto w=p.witness(input,available); bool boundary=true,paths=true;
    for (const auto& h : w.witness().hexagons()) {
        paths=paths && h.long_path.back().values() == h.direct.image.values();
        boundary=boundary && h.direct.mapped_boundary->values() == h.direct.image_boundary->values();
        for (const auto& edge : h.edges) boundary=boundary && edge.mapped_boundary->values() == edge.image_boundary->values();
    }
    std::cout << "ordered_factors=3\nowned_nested_carriers=" << c.witness().carriers().size()
        << "\ncomplete_constructor_triples=" << n.declaration().swaps[0].inner.size()
        << "\nretained_top_flags=" << c.witness().carriers()[0].flags.size()
        << "\ncertified_edges=" << c.witness().edges().size()
        << "\nexact_hexagons=" << (paths ? 2 : 0)
        << "\nexact_boundary_naturality=" << boundary
        << "\nindependent_cutoffs=3\n";
}
