#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_braid_example;
    const auto p=braid(); const auto h=p.hexagon(); const auto n=p.numeric_plan();
    const rfc::core::CellId e{{1},0}; const std::array ids{e,e,e};
    const auto c=p.cell_witness(ids,available);
    const auto input=h.field(carrier(0),{3},{{cell(carrier(0),ids),Q{1}}},available);
    const auto w=p.witness(input,available); bool boundary=true;
    for (const auto& route : w.witness().routes()) for (const auto& step : route.steps) {
        const auto& d=step.data(); boundary=boundary && d.boundary_path.back().values()==d.image_boundary->values();
    }
    std::cout << "ordered_factors=3\nowned_nested_carriers=" << c.witness().hexagon().carriers().size()
        << "\ncomplete_type_witnesses=" << n.type_witnesses().size()
        << "\nretained_top_flags=" << c.witness().hexagon().carriers()[0].flags.size()
        << "\nfirst_route_edges=" << w.native_paths()[0].size()-1
        << "\nsecond_route_edges=" << w.native_paths()[1].size()-1
        << "\nexact_yang_baxter=" << (w.native_paths()[0].back().field().values()==w.native_paths()[1].back().field().values())
        << "\nexact_boundary_naturality=" << boundary
        << "\nindependent_cutoffs=3\n";
}
