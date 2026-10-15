#include "fixture.hpp"
#include <iostream>

int main() {
    using namespace native_homotopy_sequence_example;
    const auto sequence=prepare(1); const auto domain=sequence.domain(); const auto a=sequence.preparation_availability();
    const auto input=domain.chain(native_chain(domain,grade(2),{{0,Q{3,5}}}),a.stages.back().domain);
    const auto result=sequence.witness(input,a);
    std::cout << "exact_sequenced_identity=" << zero(result.witness().witness().residual().values()) << '\n';
    std::cout << "retained_stages=" << result.plan().stage_count() << '\n';
    std::cout << "retained_root=" << result.plan().root().same_owner(sequence.root()) << '\n';
    std::cout << "publication_sequences=";
    for(std::size_t i=0;i<result.provenance().sequences.size();++i) {
        if(i)std::cout << ',';
        std::cout << result.provenance().sequences[i];
    }
    std::cout << '\n';
}
