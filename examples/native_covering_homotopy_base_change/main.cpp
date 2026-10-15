#include "fixture.hpp"
#include <iostream>
int main() {
    using namespace native_covering_base_change_example;
    try {
        const auto f = fixture(6,0,1); const auto p = BaseChange::prepare(f.original,f.changed,f.pullback,f.request,f.available);
        const auto c = p.domain(); const auto input = c.chain(grade(0),{{0,Q{1}}},f.available.original.original.cylinder);
        const auto result = p.witness(input,f.available); bool exact = true;
        for (const auto& r : result.witness().residuals()) exact = exact && zero(r);
        const auto numeric = p.numeric_plan();
        std::cout << "exact_base_change=" << exact << '\n'
                  << "retained_lifts=" << (p.original().same_owner(f.original) && p.changed().same_owner(f.changed)) << '\n'
                  << "complete_mediators=" << numeric.certificate().mediators.size() << '\n'
                  << "independent_roles=" << result.provenance().sequences.size() << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
