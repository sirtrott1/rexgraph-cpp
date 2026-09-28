#pragma once
#include "rfc/core/identity.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/database/provenance.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::database {

struct CellIdHash {
    std::size_t operator()(const core::CellId& cell) const noexcept {
        return (static_cast<std::size_t>(cell.grade.value) << 32u) ^ cell.index;
    }
};

struct ExactRecord {
    numeric::Rational value{0};
    std::vector<ProvenanceId> provenance;
};

class ExactRelationalMemory {
public:
    explicit ExactRelationalMemory(const core::RelationalComplex& complex) : complex_(&complex) {}

    void assert_exact(core::CellId cell, ExactRecord record) {
        validate_cell(cell);
        records_.insert_or_assign(cell, std::move(record));
    }

    [[nodiscard]] std::optional<ExactRecord> lookup(core::CellId cell) const {
        validate_cell(cell);
        const auto it = records_.find(cell);
        if (it == records_.end()) return std::nullopt;
        return it->second;
    }

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }

private:
    void validate_cell(core::CellId cell) const {
        if (cell.grade.value >= complex_->grade_count() || cell.index >= complex_->grade(cell.grade).cell_count()) {
            throw std::out_of_range("database cell id");
        }
    }

    const core::RelationalComplex* complex_{};
    std::unordered_map<core::CellId, ExactRecord, CellIdHash> records_;
};

} // namespace rfc::database
