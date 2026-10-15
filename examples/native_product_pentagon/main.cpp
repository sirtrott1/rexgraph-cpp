#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_pentagon_example;
    const auto p = plan(); const auto n = p.numeric_plan();
    const rfc::core::CellId e{{1},0}; const Cell top = rfc::algebra::LeftPentagonCell{{{e,e},e},e};
    const auto cell = p.cell_witness(top,available);
    const auto input = p.field(Vertex::Left,{4},{{top,Q{1}}},available);
    const auto w = p.witness(input,available);
    std::cout << "ordered_factors=4\nparenthesizations=5\ncomplete_type_quadruples=" << n.declaration().edges[0].size()
        << "\nretained_top_flags=" << cell.witness().flags()[0].size()
        << "\nexact_pentagon=" << (w.witness().long_path().back().values() == w.witness().short_path().back().values())
        << "\nindependent_cutoffs=4\n";
}
