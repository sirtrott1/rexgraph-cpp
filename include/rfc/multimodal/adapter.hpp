#pragma once
#include "rfc/models/runtime.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rfc::multimodal {

enum class Modality { Text, Image, Audio, Video, Sensor, Other };

struct BlobView {
    Modality modality{Modality::Other};
    std::span<const std::byte> bytes;
    std::string mime_type;
};

struct EncodedObservation {
    std::string relation_type;
    std::vector<std::uint64_t> relation_ids;
    std::vector<std::byte> opaque_features;
};

class Adapter : public models::Runtime {
public:
    ~Adapter() override = default;
    virtual EncodedObservation encode(const BlobView& input) = 0;
    virtual std::string_view adapter_name() const noexcept = 0;

    std::string_view runtime_name() const noexcept final { return adapter_name(); }
    std::span<const models::Capability> capabilities() const noexcept override {
        static constexpr std::array caps{models::Capability::Multimodal};
        return caps;
    }
};

} // namespace rfc::multimodal
