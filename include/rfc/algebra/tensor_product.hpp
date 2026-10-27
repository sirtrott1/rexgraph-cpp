#pragma once
#include "rfc/algebra/product_field.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/index_domain.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/work_counters.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::algebra {

class RelationalTensorProduct {
public:
    // Materialized coordinate block.  This is intentionally a view for explicit
    // coordinate realization; lazy product execution uses ProductCell directly.
    struct Block {
        std::uint16_t left_grade{};
        std::uint16_t right_grade{};
        std::size_t offset{};
        std::size_t left_size{};
        std::size_t right_size{};
        [[nodiscard]] numeric::IndexDomain domain() const {
            return numeric::IndexDomain({left_size, right_size});
        }
        [[nodiscard]] std::size_t size() const { return domain().logical_size(); }
    };

    RelationalTensorProduct(const core::RelationalComplex& left,
                            const core::RelationalComplex& right)
        : left_(&left), right_(&right) {}

    [[nodiscard]] core::Grade max_grade() const noexcept {
        return core::Grade{static_cast<std::uint16_t>(left_->max_grade().value + right_->max_grade().value)};
    }

    // Native product-cell identity.  No flat Cartesian index is created.
    [[nodiscard]] ProductCell cell(core::Grade left_grade,
                                   std::size_t left_cell,
                                   core::Grade right_grade,
                                   std::size_t right_cell) const {
        if (left_cell > std::numeric_limits<std::uint32_t>::max() ||
            right_cell > std::numeric_limits<std::uint32_t>::max()) {
            throw std::overflow_error("product cell index exceeds retained cell-id width");
        }
        ProductCell result{{left_grade, static_cast<std::uint32_t>(left_cell)},
                           {right_grade, static_cast<std::uint32_t>(right_cell)}};
        validate_cell(result);
        return result;
    }

    [[nodiscard]] ProductCellIdentity source_identity(ProductCell product_cell) const {
        validate_cell(product_cell);
        return ProductCellIdentity{
            factor_identity(*left_, product_cell.left),
            factor_identity(*right_, product_cell.right)};
    }

    [[nodiscard]] SparseProductField elementary_product(core::Grade left_grade,
                                                        std::size_t left_cell,
                                                        core::Grade right_grade,
                                                        std::size_t right_cell,
                                                        numeric::Rational value = numeric::Rational{1}) const {
        return SparseProductField::basis(cell(left_grade, left_cell, right_grade, right_cell), std::move(value));
    }

    // Exposes the lazy graded-Leibniz boundary of one product cell while
    // retaining factor occurrence identity and the Koszul orientation flip.
    [[nodiscard]] std::vector<ProductBoundaryTerm> boundary_terms(ProductCell upper) const {
        validate_cell(upper);
        if (upper.grade().value == 0) throw std::invalid_argument("tensor grade zero has no boundary");
        std::vector<ProductBoundaryTerm> terms;
        const auto reserve_hint = incidence_degree(*left_, upper.left) + incidence_degree(*right_, upper.right);
        terms.reserve(reserve_hint);
        visit_boundary_terms<false>(upper, [&](ProductBoundaryTerm term) {
            terms.push_back(std::move(term));
        }, nullptr);
        return terms;
    }

    // Primary sparse product execution contract.  Neither source nor target
    // product spaces are flattened or materialized.
    [[nodiscard]] SparseProductField boundary_apply_lazy(SparseProductFieldView field) const {
        return boundary_apply_lazy_impl<false>(field, nullptr);
    }

    [[nodiscard]] SparseProductField boundary_apply_lazy(const SparseProductField& field) const {
        return boundary_apply_lazy(field.view());
    }

    [[nodiscard]] SparseProductField boundary_apply_lazy_counted(SparseProductFieldView field,
                                                                 numeric::WorkCounters& counters) const {
        return boundary_apply_lazy_impl<true>(field, &counters);
    }

    // Explicit materialized coordinate realization.

    [[nodiscard]] std::vector<Block> blocks(core::Grade total_grade) const {
        std::vector<Block> result;
        std::size_t offset = 0;
        for (std::uint16_t p = 0; p <= total_grade.value; ++p) {
            const auto q = static_cast<std::uint16_t>(total_grade.value - p);
            if (p > left_->max_grade().value || q > right_->max_grade().value) continue;
            const auto left_size = left_->grade(core::Grade{p}).cell_count();
            const auto right_size = right_->grade(core::Grade{q}).cell_count();
            const Block block{p, q, offset, left_size, right_size};
            const auto block_size = block.size();
            if (offset > std::numeric_limits<std::size_t>::max() - block_size) {
                throw std::overflow_error("tensor product coordinate size overflow");
            }
            result.push_back(block);
            offset += block_size;
        }
        return result;
    }

    [[nodiscard]] std::size_t cell_count(core::Grade total_grade) const {
        std::size_t result = 0;
        for (const auto& block : blocks(total_grade)) result += block.size();
        return result;
    }

    [[nodiscard]] std::size_t materialize_cell(ProductCell product_cell) const {
        validate_cell(product_cell);
        const auto total = product_cell.grade();
        const auto block = find_block(total, product_cell.left.grade.value, product_cell.right.grade.value);
        return block.offset + static_cast<std::size_t>(product_cell.left.index) * block.right_size +
               static_cast<std::size_t>(product_cell.right.index);
    }

    [[nodiscard]] ProductCell dematerialize_cell(core::Grade total_grade, std::size_t flat_index) const {
        const auto materialized_blocks = blocks(total_grade);
        const auto& block = block_for_flat_index(materialized_blocks, flat_index);
        const auto local = flat_index - block.offset;
        ProductCell result{{core::Grade{block.left_grade}, static_cast<std::uint32_t>(local / block.right_size)},
                           {core::Grade{block.right_grade}, static_cast<std::uint32_t>(local % block.right_size)}};
        validate_cell(result);
        return result;
    }

    [[nodiscard]] numeric::SparseRationalVector materialize(SparseProductFieldView field) const {
        std::vector<numeric::SparseRationalEntry> entries;
        entries.reserve(field.nonzeros());
        for (const auto& entry : field.entries()) {
            if (entry.cell.grade() != field.grade()) throw std::invalid_argument("product field grade mismatch");
            entries.push_back({materialize_cell(entry.cell), entry.value});
        }
        return numeric::SparseRationalVector(cell_count(field.grade()), std::move(entries));
    }

    [[nodiscard]] numeric::SparseRationalVector materialize(const SparseProductField& field) const {
        return materialize(field.view());
    }

    [[nodiscard]] SparseProductField dematerialize(core::Grade grade,
                                                   numeric::SparseRationalView field) const {
        if (field.size() != cell_count(grade)) throw std::invalid_argument("materialized tensor field size mismatch");
        std::vector<SparseProductEntry> entries;
        entries.reserve(field.nonzeros());
        for (const auto& entry : field.entries()) {
            entries.push_back({dematerialize_cell(grade, entry.index), entry.value});
        }
        return SparseProductField(grade, std::move(entries));
    }

    [[nodiscard]] numeric::RationalVector elementary_tensor(core::Grade left_grade,
                                                             std::size_t left_cell,
                                                             core::Grade right_grade,
                                                             std::size_t right_cell) const {
        return materialize(elementary_product(left_grade, left_cell, right_grade, right_cell)).dense();
    }

    [[nodiscard]] numeric::SparseRationalVector elementary_tensor_sparse(core::Grade left_grade,
                                                                          std::size_t left_cell,
                                                                          core::Grade right_grade,
                                                                          std::size_t right_cell) const {
        return materialize(elementary_product(left_grade, left_cell, right_grade, right_cell));
    }

    // Dense materialization remains a small-reference path.  Production sparse
    // execution should use boundary_apply_lazy().
    [[nodiscard]] numeric::RationalVector boundary_apply(core::Grade total_grade,
                                                          const numeric::RationalVector& field) const {
        if (total_grade.value == 0) throw std::invalid_argument("tensor grade zero has no boundary");
        if (field.size() != cell_count(total_grade)) throw std::invalid_argument("tensor field size mismatch");
        std::vector<numeric::SparseRationalEntry> active;
        active.reserve(field.size());
        for (std::size_t i = 0; i < field.size(); ++i) {
            if (field[i] != 0) active.push_back({i, field[i]});
        }
        const numeric::SparseRationalVector sparse(field.size(), std::move(active));
        return boundary_apply_sparse(total_grade, sparse).dense();
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse(
        core::Grade total_grade,
        numeric::SparseRationalView field) const {
        const auto lazy = dematerialize(total_grade, field);
        return materialize(boundary_apply_lazy(lazy));
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse(
        core::Grade total_grade,
        const numeric::SparseRationalVector& field) const {
        return boundary_apply_sparse(total_grade, field.view());
    }

    [[nodiscard]] numeric::SparseRationalVector boundary_apply_sparse_counted(
        core::Grade total_grade,
        numeric::SparseRationalView field,
        numeric::WorkCounters& counters) const {
        const auto lazy = dematerialize(total_grade, field);
        return materialize(boundary_apply_lazy_counted(lazy.view(), counters));
    }

private:
    [[nodiscard]] static ProductFactorIdentity factor_identity(const core::RelationalComplex& complex,
                                                               core::CellId cell_id) {
        if (cell_id.grade.value > complex.max_grade().value) throw std::out_of_range("product factor grade");
        const auto& arena = complex.grade(cell_id.grade);
        if (cell_id.index >= arena.cell_count()) throw std::out_of_range("product factor cell");
        if (cell_id.grade.value == 0) {
            return ProductFactorIdentity{cell_id.grade, arena.identities.at(cell_id.index)};
        }
        return ProductFactorIdentity{cell_id.grade, arena.relation_identities.at(cell_id.index).value};
    }

    void validate_cell(ProductCell product_cell) const {
        if (product_cell.left.grade.value > left_->max_grade().value ||
            product_cell.right.grade.value > right_->max_grade().value) {
            throw std::out_of_range("product cell grade");
        }
        if (product_cell.left.index >= left_->grade(product_cell.left.grade).cell_count() ||
            product_cell.right.index >= right_->grade(product_cell.right.grade).cell_count()) {
            throw std::out_of_range("product cell index");
        }
        (void)product_cell.grade();
    }

    [[nodiscard]] static std::size_t incidence_degree(const core::RelationalComplex& complex,
                                                      core::CellId cell_id) {
        if (cell_id.grade.value == 0) return 0;
        const auto [begin, end] = complex.grade(cell_id.grade).incidence_range(cell_id.index);
        return end - begin;
    }

    template <bool Count, class Visitor>
    void visit_boundary_terms(ProductCell upper,
                              Visitor&& visitor,
                              numeric::WorkCounters* counters) const {
        if (upper.left.grade.value > 0) {
            const auto& arena = left_->grade(upper.left.grade);
            const auto exists = left_->signature().existence_aspect();
            const auto [begin, end] = arena.incidence_range(upper.left.index);
            for (std::size_t p = begin; p < end; ++p) {
                if constexpr (Count) ++counters->incidences_touched;
                if (!arena.aspects.get(exists, p)) continue;
                const auto coefficient = left_->incidence_coefficient(upper.left.grade, p);
                if (coefficient == 0) continue;
                visitor(ProductBoundaryTerm{
                    ProductCell{{core::Grade{static_cast<std::uint16_t>(upper.left.grade.value - 1u)},
                                 arena.lower_index[p]},
                                upper.right},
                    coefficient,
                    ProductBoundarySide::Left,
                    arena.occurrence_identities.at(p),
                    false});
            }
        }

        if (upper.right.grade.value > 0) {
            const auto& arena = right_->grade(upper.right.grade);
            const auto exists = right_->signature().existence_aspect();
            const auto [begin, end] = arena.incidence_range(upper.right.index);
            const bool koszul_flip = (upper.left.grade.value % 2u) != 0u;
            const auto sign = koszul_flip ? numeric::Rational{-1} : numeric::Rational{1};
            for (std::size_t p = begin; p < end; ++p) {
                if constexpr (Count) ++counters->incidences_touched;
                if (!arena.aspects.get(exists, p)) continue;
                numeric::Rational coefficient = sign * right_->incidence_coefficient(upper.right.grade, p);
                coefficient.canonicalize();
                if (coefficient == 0) continue;
                visitor(ProductBoundaryTerm{
                    ProductCell{upper.left,
                                {core::Grade{static_cast<std::uint16_t>(upper.right.grade.value - 1u)},
                                 arena.lower_index[p]}},
                    std::move(coefficient),
                    ProductBoundarySide::Right,
                    arena.occurrence_identities.at(p),
                    koszul_flip});
            }
        }
    }

    template <bool Count>
    [[nodiscard]] SparseProductField boundary_apply_lazy_impl(
        SparseProductFieldView field,
        numeric::WorkCounters* counters) const {
        if (field.grade().value == 0) throw std::invalid_argument("tensor grade zero has no boundary");
        const auto target_grade = core::Grade{static_cast<std::uint16_t>(field.grade().value - 1u)};
        std::vector<SparseProductEntry> contributions;
        contributions.reserve(field.nonzeros() * 4u);

        for (const auto& active : field.entries()) {
            if (active.cell.grade() != field.grade()) throw std::invalid_argument("product field grade mismatch");
            validate_cell(active.cell);
            if constexpr (Count) ++counters->active_coordinates;
            visit_boundary_terms<Count>(active.cell, [&](ProductBoundaryTerm term) {
                contributions.push_back({term.lower, active.value * term.coefficient});
            }, counters);
        }
        return SparseProductField(target_grade, std::move(contributions));
    }

    [[nodiscard]] static const Block& block_for_flat_index(const std::vector<Block>& candidate_blocks,
                                                            std::size_t index) {
        for (const auto& block : candidate_blocks) {
            if (index >= block.offset && index < block.offset + block.size()) return block;
        }
        throw std::out_of_range("tensor flat index");
    }

    [[nodiscard]] static Block find_block(const std::vector<Block>& candidate_blocks,
                                          std::uint16_t left_grade,
                                          std::uint16_t right_grade) {
        for (const auto& block : candidate_blocks) {
            if (block.left_grade == left_grade && block.right_grade == right_grade) return block;
        }
        throw std::out_of_range("tensor block not present");
    }

    [[nodiscard]] Block find_block(core::Grade total_grade,
                                   std::uint16_t left_grade,
                                   std::uint16_t right_grade) const {
        return find_block(blocks(total_grade), left_grade, right_grade);
    }

    const core::RelationalComplex* left_{};
    const core::RelationalComplex* right_{};
};

} // namespace rfc::algebra
