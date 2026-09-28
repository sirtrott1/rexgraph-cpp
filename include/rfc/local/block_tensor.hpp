#pragma once
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

struct LocalTensorBlock {
    core::CellId target{};
    std::size_t incidence_slot{};
    std::size_t row_offset{};
    numeric::ExactMatrix map;
};

// A local relation tensor realizes one primary upper stalk into the direct sum
// of its retained boundary-occurrence stalks.  A branching relation therefore
// remains one tensor with one source amplitude/state and several coupled legs.
class LocalRelationTensor {
public:
    LocalRelationTensor(core::CellId source,
                        std::size_t source_dimension,
                        std::vector<LocalTensorBlock> blocks)
        : source_(source), source_dimension_(source_dimension), blocks_(std::move(blocks)) {
        if (source_dimension_ == 0) throw std::invalid_argument("local tensor source dimension must be positive");
        std::size_t expected_offset = 0;
        for (const auto& block : blocks_) {
            if (block.row_offset != expected_offset) throw std::invalid_argument("local tensor block offsets must be contiguous");
            if (block.map.cols() != source_dimension_) throw std::invalid_argument("local tensor block source dimension mismatch");
            expected_offset += block.map.rows();
        }
        rows_ = expected_offset;
    }

    [[nodiscard]] core::CellId source() const noexcept { return source_; }
    [[nodiscard]] std::size_t source_dimension() const noexcept { return source_dimension_; }
    [[nodiscard]] std::size_t boundary_dimension() const noexcept { return rows_; }
    [[nodiscard]] const std::vector<LocalTensorBlock>& blocks() const noexcept { return blocks_; }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& source_value) const {
        if (source_value.size() != source_dimension_) throw std::invalid_argument("local tensor source value mismatch");
        numeric::RationalVector out(rows_, numeric::Rational{0});
        for (const auto& block : blocks_) {
            const auto part = block.map.apply(source_value);
            for (std::size_t i = 0; i < part.size(); ++i) out[block.row_offset + i] = part[i];
        }
        return out;
    }

    [[nodiscard]] numeric::ExactMatrix matrix() const {
        numeric::ExactMatrix out(rows_, source_dimension_);
        for (const auto& block : blocks_) {
            for (const auto& [key, value] : block.map.entries()) out.set(block.row_offset + key.first, key.second, value);
        }
        return out;
    }

private:
    core::CellId source_{};
    std::size_t source_dimension_{};
    std::size_t rows_{};
    std::vector<LocalTensorBlock> blocks_;
};

} // namespace rfc::local
