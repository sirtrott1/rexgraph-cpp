#include "rfc/core/builder.hpp"
#include "rfc/topology/attaching_transport.hpp"
#include <iostream>
#include <vector>

static rfc::core::RelationalComplex presentation(const std::vector<int>& word) {
    using namespace rfc;
    core::RelationSignature signature;
    const auto loop = signature.add_type("meridian",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face = signature.add_attaching_loop_type("word",1,0);
    core::RelationalComplexBuilder builder(signature);
    for (int g = 0; g < 2; ++g)
        builder.add_grade_one(loop,{{99,true,true,false,1},{99,true,false,false,1}});
    std::vector<core::HigherIncidenceInput> occurrences;
    for (const auto letter : word)
        occurrences.emplace_back(static_cast<std::uint32_t>((letter > 0 ? letter : -letter)-1),
                                 true,letter < 0,false,numeric::Rational{1});
    builder.add_higher(core::Grade{2},face,occurrences);
    return builder.freeze();
}
int main() {
    using namespace rfc;
    using M = numeric::ExactMatrix;
    const auto A = M::from_rows({{1,1},{0,1}}), B = M::from_rows({{1,0},{-1,1}});
    const std::vector<topology::GeneratorTransport> maps{{{core::Grade{1},0},A},{{core::Grade{1},1},B}};
    for (const bool trefoil : {true,false}) {
        const auto source = presentation(trefoil ? std::vector<int>{1,2,1,-2,-1,-2} : std::vector<int>{1,2,-1,-2});
        const auto prepared = topology::PreparedAttachingTransport::common_rational_coordinates(
            source,2,maps,topology::AttachingTransportConvention::ChronologicalColumns);
        std::cout << "{\"presentation\":\"" << (trefoil ? "trefoil":"commutator")
                  << "\",\"generators_commute\":" << (A.multiply(B) == B.multiply(A) ? "true":"false")
                  << ",\"flat\":" << (prepared.flat() ? "true":"false") << ",\"curvature\":[";
        for (std::size_t i = 0; i < 2; ++i) {
            if (i != 0) std::cout << ',';
            std::cout << '[';
            for (std::size_t j = 0; j < 2; ++j) {
                if (j != 0) std::cout << ',';
                std::cout << '"' << prepared.curvature().get(i,j) << '"';
            }
            std::cout << ']';
        }
        std::cout << ']';
        if (prepared.flat()) {
            const auto h = prepared.homology_dimensions();
            std::cout << ",\"homology\":[" << h[0] << ',' << h[1] << ',' << h[2] << ']';
        }
        std::cout << "}\n";
    }
}
