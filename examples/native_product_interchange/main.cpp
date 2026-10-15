#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_interchange_example;
    const auto p=plan(); const auto n=p.numeric_plan(); const rfc::core::CellId e{{1},0}; const Cell top{e,e};
    const auto c=p.cell_witness(Order::AB,top,available);
    const auto input=p.field(Order::AB,{2},{{top,Q{1}}},available);
    const auto swapped=p.interchange(input,available), restored=p.interchange(swapped,available);
    const auto w=p.witness(input,available);
    std::cout << "ordered_factors=2\nowned_products=2\ncomplete_type_pairs=" << n.declaration().types.size()
        << "\nretained_top_flags=" << c.witness().flags().size()
        << "\nodd_pair_coefficient=" << swapped.field().values().at(top)
        << "\nexact_round_trip=" << (restored.field().values() == input.field().values())
        << "\nexact_boundary_naturality=" << (w.witness().mapped_boundary()->values() == w.witness().image_boundary()->values())
        << "\nindependent_cutoffs=2\n";
}
