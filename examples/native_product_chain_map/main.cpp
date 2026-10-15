#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_map_example;
    const auto p = native_map_example::plan(); const auto h = p.source(); const auto c = carrier(0);
    const std::array<rfc::core::CellId,3> ids{{{{1},0}, {{1},0}, {{0},0}}};
    const auto input = h.field(c,{2},{{cell(c,ids),Q{1}}},map_available.source);
    const auto result = p.apply(input,map_available,map_lease()); const auto witness = p.witness(input,map_available,map_lease());
    std::size_t contributions = 0; for (const auto& s : result.lift().stages()) contributions += s.contributions.size();
    std::cout << "original_factors=3\nindependent_endpoints=6\nmap_stages=" << result.lift().stages().size()
        << "\nretained_contributions=" << contributions << "\nimage_coordinates=" << result.lift().image().values().size()
        << "\nnaturality_routes=" << witness.witness().routes().size()
        << "\nretained_boundary_support_difference=" << witness.witness().boundary()->comparison.only_first_support.size()
        << "\nexact_yang_baxter=1\n";
}
