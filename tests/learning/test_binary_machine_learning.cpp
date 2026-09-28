#include "rfc/composite/aspect.hpp"
#include "rfc/composite/planes.hpp"
#include "rfc/composite/program.hpp"
#include "rfc/learning/binary_gate.hpp"
#include "rfc/learning/composite_features.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr std::size_t n = 1024;
        rfc::machine::PackedBits a(n, false), b(n, false), target(n, false);
        for (std::size_t i = 0; i < n; ++i) {
            const bool av = (i & 1u) != 0;
            const bool bv = (i & 2u) != 0;
            a.set(i, av);
            b.set(i, bv);
            target.set(i, av != bv);
        }
        const auto fit = rfc::learning::fit_binary_truth_gate2(a, b, target);
        require(fit.hamming_error == 0u, "machine binary gate failed to learn XOR exactly");
        require(fit.gate.truth_table() == 0x6u, "learned XOR truth table mismatch");
        require((fit.gate.forward(a, b) ^ target).none(), "trained binary gate prediction mismatch");

        auto schema = rfc::composite::AspectSchema::canonical();
        const auto visible = schema.add("visible", rfc::composite::AspectRole::Visibility, false);
        rfc::composite::AspectPlanes planes(schema, n);
        const auto exists = schema.id("exists");
        const auto orientation = schema.id("orientation");
        for (std::size_t i = 0; i < n; ++i) {
            planes.set(orientation, i, (i & 1u) != 0);
            planes.set(visible, i, (i % 3u) == 0u);
        }
        rfc::composite::BinaryProgram feature_program(4);
        feature_program.append({rfc::composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, exists});
        feature_program.append({rfc::composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
        feature_program.append({rfc::composite::BinaryOpcode::AndNot, 2, 0, 1, 0, 0});
        feature_program.append({rfc::composite::BinaryOpcode::LoadAspect, 3, 0, 0, 0, visible});
        rfc::composite::BinaryProgram combined(5);
        combined.append({rfc::composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, exists});
        combined.append({rfc::composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
        combined.append({rfc::composite::BinaryOpcode::AndNot, 2, 0, 1, 0, 0});
        combined.append({rfc::composite::BinaryOpcode::LoadAspect, 3, 0, 0, 0, visible});
        combined.append({rfc::composite::BinaryOpcode::And, 4, 2, 3, 0, 0});
        rfc::learning::CompositeFeatureExtractor extractor;
        extractor.add("visible_positive", std::move(combined), 4);
        const auto features = extractor.evaluate(planes);
        require(features.size() == 1u, "composite feature count mismatch");
        for (std::size_t i = 0; i < n; ++i) {
            require(features[0].values.get(i) == (((i % 3u) == 0u) && ((i & 1u) == 0u)),
                    "composite packed feature mismatch");
        }

        std::cout << "machine-instruction learning primitive tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
