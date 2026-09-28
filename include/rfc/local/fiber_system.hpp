#pragma once
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/local/block_tensor.hpp"
#include "rfc/local/stalk_bundle.hpp"
#include "rfc/local/stalk.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

struct LocalOccurrenceValue {
    core::CellId lower{};
    std::size_t incidence_slot{};
    numeric::RationalVector value;
};

struct RelationLocalSection {
    core::CellId relation{};
    numeric::RationalVector upper_value;
    std::vector<LocalOccurrenceValue> boundary_values;
};

class LinearRelationalFiberSystem {
public:
    explicit LinearRelationalFiberSystem(LinearStalkBundle stalks)
        : stalks_(std::move(stalks)) {
        overrides_.resize(stalks_.complex().grade_count());
        for (std::size_t k = 1; k < stalks_.complex().grade_count(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            overrides_[k].resize(stalks_.complex().grade(grade).incidence_count());
        }
    }

    [[nodiscard]] const LinearStalkBundle& stalks() const noexcept { return stalks_; }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return stalks_.complex(); }

    void set_transport(core::Grade grade, std::size_t incidence_slot, numeric::ExactMatrix map) {
        if (grade.value == 0 || grade.value >= complex().grade_count()) throw std::out_of_range("flag transport grade");
        const auto& arena = complex().grade(grade);
        if (incidence_slot >= arena.incidence_count()) throw std::out_of_range("flag transport incidence");
        const auto upper = core::CellId{grade, arena.upper_index[incidence_slot]};
        const auto lower = core::CellId{core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}, arena.lower_index[incidence_slot]};
        if (map.cols() != stalks_.dimension(upper) || map.rows() != stalks_.dimension(lower)) {
            throw std::invalid_argument("flag transport stalk dimensions do not match");
        }
        overrides_[grade.value][incidence_slot] = std::move(map);
    }

    [[nodiscard]] LinearFlagMap flag_map(core::Grade grade, std::size_t incidence_slot) const {
        if (grade.value == 0 || grade.value >= complex().grade_count()) throw std::out_of_range("flag map grade");
        const auto& arena = complex().grade(grade);
        if (incidence_slot >= arena.incidence_count()) throw std::out_of_range("flag map incidence");
        const auto upper = core::CellId{grade, arena.upper_index[incidence_slot]};
        const auto lower = core::CellId{core::Grade{static_cast<std::uint16_t>(grade.value - 1u)}, arena.lower_index[incidence_slot]};
        if (overrides_[grade.value][incidence_slot]) {
            return LinearFlagMap{upper, lower, incidence_slot, *overrides_[grade.value][incidence_slot]};
        }
        const auto upper_dim = stalks_.dimension(upper);
        const auto lower_dim = stalks_.dimension(lower);
        if (upper_dim != lower_dim) {
            throw std::logic_error("non-equal stalk dimensions require an explicit flag transport");
        }
        numeric::ExactMatrix transport(lower_dim, upper_dim);
        const auto coefficient = complex().incidence_coefficient(grade, incidence_slot);
        for (std::size_t i = 0; i < lower_dim; ++i) if (coefficient != 0) transport.set(i, i, coefficient);
        return LinearFlagMap{upper, lower, incidence_slot, std::move(transport)};
    }

    [[nodiscard]] LocalRelationTensor relation_tensor(core::Grade grade, std::size_t relation_index) const {
        const core::BoundaryOccurrenceFiber fiber(complex(), grade, relation_index);
        const auto source = fiber.relation();
        std::vector<LocalTensorBlock> blocks;
        std::size_t offset = 0;
        for (const auto& occurrence : fiber.occurrences()) {
            if (!occurrence.exists) continue;
            auto flag = flag_map(grade, occurrence.incidence_slot);
            blocks.push_back(LocalTensorBlock{flag.lower, flag.incidence_slot, offset, std::move(flag.transport)});
            offset += blocks.back().map.rows();
        }
        return LocalRelationTensor(source, stalks_.dimension(source), std::move(blocks));
    }

    [[nodiscard]] LocalRelationTensor branch_tensor(core::Grade grade, std::size_t relation_index) const {
        (void)core::BranchingRelationView(complex(), grade, relation_index);
        return relation_tensor(grade, relation_index);
    }

    [[nodiscard]] RelationLocalSection realize_relation(core::Grade grade,
                                                        std::size_t relation_index,
                                                        const numeric::RationalVector& upper_value) const {
        const auto tensor = relation_tensor(grade, relation_index);
        if (upper_value.size() != tensor.source_dimension()) throw std::invalid_argument("relation local section upper value size");
        RelationLocalSection section;
        section.relation = tensor.source();
        section.upper_value = upper_value;
        section.boundary_values.reserve(tensor.blocks().size());
        for (const auto& block : tensor.blocks()) {
            section.boundary_values.push_back(LocalOccurrenceValue{block.target, block.incidence_slot, block.map.apply(upper_value)});
        }
        return section;
    }

private:
    LinearStalkBundle stalks_;
    std::vector<std::vector<std::optional<numeric::ExactMatrix>>> overrides_;
};

// Compatibility alias.  Prefer LinearRelationalFiberSystem in new code.
using RelationalFiberSystem = LinearRelationalFiberSystem;

} // namespace rfc::local
