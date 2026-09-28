#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rfc::learning {

template <class Scalar>
struct FieldChannel {
    std::string name;
    std::uint32_t chain_grade{};
    std::uint32_t semantic_scale{};
    std::vector<Scalar> values;
};

template <class Scalar>
struct RelationalBatch {
    std::size_t batch_size{1};
    std::vector<FieldChannel<Scalar>> channels;
    std::vector<std::uint64_t> relation_ids;
    std::vector<std::uint64_t> provenance_ids;
};

} // namespace rfc::learning
