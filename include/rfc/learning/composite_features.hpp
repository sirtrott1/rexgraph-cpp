#pragma once
#include "rfc/composite/program.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rfc::learning {

struct PackedFeature {
    std::string name;
    machine::PackedBits values;
};

class CompositeFeatureExtractor {
public:
    void add(std::string name, composite::BinaryProgram program, std::uint16_t output_register) {
        features_.push_back(FeatureSpec{std::move(name), std::move(program), output_register});
    }

    [[nodiscard]] std::vector<PackedFeature> evaluate(const composite::AspectPlanes& planes) const {
        std::vector<PackedFeature> result;
        result.reserve(features_.size());
        for (const auto& feature : features_) {
            result.push_back(PackedFeature{feature.name, feature.program.execute(planes, feature.output_register)});
        }
        return result;
    }

private:
    struct FeatureSpec {
        std::string name;
        composite::BinaryProgram program;
        std::uint16_t output_register{};
    };
    std::vector<FeatureSpec> features_;
};

} // namespace rfc::learning
