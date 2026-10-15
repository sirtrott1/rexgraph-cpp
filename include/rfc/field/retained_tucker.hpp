#pragma once

#include "rfc/machine/checked_size.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

// Limits cover supported scalar operations and entries in each execution
// container. Logical extents are metadata, not an allocation or iteration
// request. The bounds are not an accounting of allocator/GMP memory.
struct RetainedTuckerLimits {
    std::size_t entries{1000000};
    std::size_t steps{64000000};
    std::size_t integer_bits{1u << 24};
};

struct RetainedTuckerStats {
    std::size_t left_inverse_columns{};
    std::size_t left_inverse_products{};
    std::size_t mode_actions{};
    std::size_t coefficient_visits{};
    std::size_t routed_products{};
    std::size_t exact_cancellations{};
    std::size_t peak_intermediate_nonzeros{};
    std::size_t support_membership_checks{};
};

// The rejected source is not projected away. Its exact ambient-coordinate
// residual (reconstruction minus source) remains available to the caller.
class RetainedTuckerReconstructionError : public std::invalid_argument {
public:
    explicit RetainedTuckerReconstructionError(numeric::SparseRationalVector residual)
        : std::invalid_argument("Tucker exact reconstruction residual is nonzero"),
          residual_(std::move(residual)) {}
    [[nodiscard]] const numeric::SparseRationalVector& residual() const noexcept { return residual_; }
private:
    numeric::SparseRationalVector residual_;
};

namespace retained_tucker_detail {
using Q = numeric::Rational;
using S = numeric::SparseRationalVector;
using Coordinates = std::map<std::size_t, Q>;
struct Term { std::size_t output; Q coefficient; };
using Columns = std::map<std::size_t, std::vector<Term>>;

inline numeric::ExactActionGuard guard(RetainedTuckerLimits limits) {
    return numeric::ExactActionGuard({std::numeric_limits<std::size_t>::max(),
                                     limits.entries, limits.steps, limits.integer_bits});
}
inline void increment(std::size_t& value, std::size_t amount = 1) {
    value = machine::checked_size_add(value, amount);
}
inline void observe(std::size_t size, RetainedTuckerStats* stats) {
    if (stats) stats->peak_intermediate_nonzeros =
        std::max(stats->peak_intermediate_nonzeros, size);
}
inline void support_budget(const numeric::StructuralSupport& support,
                           numeric::ExactActionGuard& guard) {
    // Copy only the retained representation. A Full marker has constant size;
    // sparse indices and packed words already exist and are bounded explicitly.
    if (const auto* indices = support.sparse_indices()) guard.entries(indices->size());
    if (const auto* bits = support.packed_bits()) guard.entries(bits->words().size());
}
inline void accumulate(Coordinates& values, std::size_t index, Q value,
                       numeric::ExactActionGuard& guard, RetainedTuckerStats* stats) {
    guard.value(value);
    if (value == 0) return;
    const auto at = values.find(index);
    if (at == values.end()) {
        // Check before extending support; a rejected action publishes no state.
        guard.entries(machine::checked_size_add(values.size(), 1));
        values.emplace(index, std::move(value));
        observe(values.size(), stats);
    } else {
        at->second = guard.add(at->second, value);
        if (at->second == 0) {
            values.erase(at);
            if (stats) increment(stats->exact_cancellations);
        }
    }
}
inline S finish(std::size_t size, Coordinates values, numeric::ExactActionGuard& guard) {
    guard.entries(values.size());
    std::vector<numeric::SparseRationalEntry> entries;
    entries.reserve(values.size());
    for (auto& [index, value] : values) {
        guard.step();
        entries.push_back({index, std::move(value)});
    }
    return S(size, std::move(entries));
}
inline Columns columns(const numeric::ExactMatrix& map, numeric::ExactActionGuard& guard) {
    guard.entries(map.nonzeros());
    Columns result;
    for (const auto& [ij, value] : map.entries()) {
        guard.step();
        guard.value(value);
        result[ij.second].push_back({ij.first, value});
    }
    return result;
}
inline S difference(const S& left, const S& right, numeric::ExactActionGuard& guard) {
    if (left.size() != right.size()) throw std::invalid_argument("Tucker residual domain mismatch");
    Coordinates values;
    for (const auto& entry : left.entries()) {
        guard.step();
        accumulate(values, entry.index, entry.value, guard, nullptr);
    }
    for (const auto& entry : right.entries()) {
        guard.step();
        accumulate(values, entry.index, -entry.value, guard, nullptr);
    }
    return finish(left.size(), std::move(values), guard);
}
} // namespace retained_tucker_detail

// One supplied rational frame. R reconstructs ambient coordinates from core
// coordinates; A accedes to the core. A R = I is checked exactly. The caller
// owns the retained relation identities, field/metric meaning and any Hodge
// interpretation. This class neither fits a frame nor asserts minimum rank.
class RetainedTuckerMode {
    using Columns = retained_tucker_detail::Columns;
    struct State {
        numeric::ExactMatrix reconstruction, accession;
        Columns reconstruction_columns, accession_columns;
        State(numeric::ExactMatrix r, numeric::ExactMatrix a,
              numeric::ExactActionGuard& guard, RetainedTuckerStats* stats)
            : reconstruction(std::move(r)), accession(std::move(a)),
              reconstruction_columns(retained_tucker_detail::columns(reconstruction, guard)),
              accession_columns(retained_tucker_detail::columns(accession, guard)) {
            if (accession.cols() != reconstruction.rows() ||
                accession.rows() != reconstruction.cols())
                throw std::invalid_argument("Tucker mode factor dimensions disagree");
            // An injective supplied frame needs a retained column for every
            // core direction. Count this before considering any basis loop.
            if (reconstruction_columns.size() != reconstruction.cols())
                throw std::invalid_argument("Tucker frame omits a core direction");
            for (const auto& [column, terms] : reconstruction_columns) {
                retained_tucker_detail::Coordinates image;
                for (const auto& term : terms) {
                    guard.step();
                    const auto at = accession_columns.find(term.output);
                    if (at == accession_columns.end()) continue;
                    for (const auto& dual : at->second) {
                        const auto value = guard.mul(dual.coefficient, term.coefficient);
                        retained_tucker_detail::accumulate(image, dual.output, value, guard, stats);
                        if (stats) retained_tucker_detail::increment(stats->left_inverse_products);
                    }
                }
                if (image.size() != 1 || image.begin()->first != column || image.begin()->second != 1)
                    throw std::invalid_argument("Tucker accession is not an exact left inverse");
                if (stats) retained_tucker_detail::increment(stats->left_inverse_columns);
            }
        }
    };
public:
    RetainedTuckerMode(numeric::ExactMatrix reconstruction, numeric::ExactMatrix accession,
                       RetainedTuckerLimits limits = {}, RetainedTuckerStats* stats = nullptr)
        : ambient_(reconstruction.rows()), core_(reconstruction.cols()) {
        auto guard = retained_tucker_detail::guard(limits);
        state_ = std::make_shared<const State>(std::move(reconstruction), std::move(accession), guard, stats);
    }

    // A symbolic identity has no stored diagonal and certifies every declared
    // coordinate without enumerating the extent.
    [[nodiscard]] static RetainedTuckerMode identity(std::size_t extent) {
        return RetainedTuckerMode(extent);
    }
    [[nodiscard]] std::size_t ambient_dimension() const noexcept { return ambient_; }
    [[nodiscard]] std::size_t core_dimension() const noexcept { return core_; }
    [[nodiscard]] bool is_identity() const noexcept { return !state_; }
    [[nodiscard]] bool is_complete() const noexcept { return ambient_ == core_; }
    [[nodiscard]] std::size_t stored_factor_nonzeros() const noexcept {
        return state_ ? state_->reconstruction.nonzeros() + state_->accession.nonzeros() : 0;
    }
    [[nodiscard]] const numeric::ExactMatrix* reconstruction_factor() const noexcept {
        return state_ ? &state_->reconstruction : nullptr;
    }
    [[nodiscard]] const numeric::ExactMatrix* accession_factor() const noexcept {
        return state_ ? &state_->accession : nullptr;
    }

private:
    friend class RetainedTuckerTensor;
    explicit RetainedTuckerMode(std::size_t extent) : ambient_(extent), core_(extent) {}

    [[nodiscard]] numeric::SparseRationalVector act(
        const numeric::SparseRationalVector& input, const numeric::IndexDomain& domain,
        std::size_t axis, bool accession, numeric::ExactActionGuard& guard,
        RetainedTuckerStats* stats) const {
        const auto source_extent = accession ? ambient_ : core_;
        const auto target_extent = accession ? core_ : ambient_;
        if (axis >= domain.rank() || domain.extents()[axis] != source_extent ||
            input.size() != domain.logical_size())
            throw std::invalid_argument("Tucker mode action domain mismatch");
        guard.sparse_vector(input);
        if (stats) {
            retained_tucker_detail::increment(stats->mode_actions);
            retained_tucker_detail::increment(stats->coefficient_visits, input.nonzeros());
            retained_tucker_detail::observe(input.nonzeros(), stats);
        }
        if (is_identity()) return input;
        auto extents = domain.extents();
        extents[axis] = target_extent;
        const numeric::IndexDomain output_domain(std::move(extents));
        const auto& routes = accession ? state_->accession_columns : state_->reconstruction_columns;
        retained_tucker_detail::Coordinates values;
        for (const auto& entry : input.entries()) {
            // An entry cannot exist in a zero-size domain, so each divisor
            // below is nonzero. Only this entry's mode coordinate is decoded.
            const auto stride = domain.strides()[axis];
            const auto source_block = machine::checked_size_mul(source_extent, stride);
            const auto coordinate = (entry.index / stride) % source_extent;
            const auto at = routes.find(coordinate);
            if (at == routes.end()) continue;
            const auto prefix = entry.index / source_block;
            const auto suffix = entry.index % stride;
            const auto target_block = machine::checked_size_mul(target_extent, stride);
            for (const auto& term : at->second) {
                const auto index = machine::checked_size_add(
                    machine::checked_size_add(machine::checked_size_mul(prefix, target_block),
                                              machine::checked_size_mul(term.output, stride)), suffix);
                const auto value = guard.mul(term.coefficient, entry.value);
                retained_tucker_detail::accumulate(values, index, value, guard, stats);
                if (stats) retained_tucker_detail::increment(stats->routed_products);
            }
        }
        return retained_tucker_detail::finish(output_domain.logical_size(), std::move(values), guard);
    }

    std::size_t ambient_{}, core_{};
    std::shared_ptr<const State> state_;
};

// T = (R_0 tensor ... tensor R_d) core, with supplied mode frames. Core slot
// existence belongs to its declared coordinate space. Source existence stays
// in the original StructuralSupport and is never inferred from a factor or
// numeric coefficient. In particular an existing source zero is preserved.
//
// encode() certifies exact reconstruction of the supplied source; a reduced
// frame that loses any source value is rejected. This is a certificate for
// that tensor in those frames, not a preservation theorem for other fields or
// cohomology. No full product tensor, operator or Cartesian support is formed.
class RetainedTuckerTensor {
public:
    [[nodiscard]] static RetainedTuckerTensor encode(
        const numeric::SparseRationalTensor& source, std::vector<RetainedTuckerMode> modes,
        RetainedTuckerLimits limits = {}, RetainedTuckerStats* stats = nullptr) {
        if (modes.size() != source.domain().rank())
            throw std::invalid_argument("Tucker factor count differs from tensor rank");
        for (std::size_t axis = 0; axis < modes.size(); ++axis)
            if (modes[axis].ambient_dimension() != source.domain().extents()[axis])
                throw std::invalid_argument("Tucker ambient mode extent mismatch");
        auto guard = retained_tucker_detail::guard(limits);
        guard.entries(modes.size());
        retained_tucker_detail::support_budget(source.support(), guard);
        guard.sparse_vector(source.values());
        auto core_domain = source.domain();
        auto core_values = source.values();
        for (std::size_t axis = 0; axis < modes.size(); ++axis) {
            core_values = modes[axis].act(core_values, core_domain, axis, true, guard, stats);
            auto extents = core_domain.extents();
            extents[axis] = modes[axis].core_dimension();
            core_domain = numeric::IndexDomain(std::move(extents));
        }
        const auto core_size = core_domain.logical_size();
        RetainedTuckerTensor result(source.domain(), source.support(), std::move(modes),
            numeric::SparseRationalTensor(std::move(core_domain),
                numeric::StructuralSupport::full(core_size), std::move(core_values)));
        const auto reconstructed = result.reconstruct_values(guard, stats);
        auto residual = retained_tucker_detail::difference(reconstructed, source.values(), guard);
        if (!residual.empty()) throw RetainedTuckerReconstructionError(std::move(residual));
        return result;
    }

    [[nodiscard]] const numeric::IndexDomain& domain() const noexcept { return domain_; }
    [[nodiscard]] const numeric::StructuralSupport& support() const noexcept { return support_; }
    [[nodiscard]] const numeric::SparseRationalTensor& core() const noexcept { return core_; }
    [[nodiscard]] const std::vector<RetainedTuckerMode>& modes() const noexcept { return modes_; }

    [[nodiscard]] numeric::SparseRationalTensor reconstruct(
        RetainedTuckerLimits limits = {}, RetainedTuckerStats* stats = nullptr) const {
        auto guard = retained_tucker_detail::guard(limits);
        retained_tucker_detail::support_budget(support_, guard);
        auto values = reconstruct_values(guard, stats);
        require_source_support(values, guard, stats);
        return numeric::SparseRationalTensor(domain_, support_, std::move(values));
    }

    // Numerical residual against a state on the same ambient domain. Original
    // structural existence remains available separately through support().
    [[nodiscard]] numeric::SparseRationalVector reconstruction_residual(
        const numeric::SparseRationalVector& expected, RetainedTuckerLimits limits = {},
        RetainedTuckerStats* stats = nullptr) const {
        if (expected.size() != domain_.logical_size())
            throw std::invalid_argument("Tucker expected state domain mismatch");
        auto guard = retained_tucker_detail::guard(limits);
        guard.sparse_vector(expected);
        return retained_tucker_detail::difference(reconstruct_values(guard, stats), expected, guard);
    }

    // An exact update in the declared coordinates. Updates may change numeric
    // state but cannot silently create source incidences outside retained support.
    [[nodiscard]] RetainedTuckerTensor with_core(
        numeric::SparseRationalVector values, RetainedTuckerLimits limits = {},
        RetainedTuckerStats* stats = nullptr) const {
        if (values.size() != core_.domain().logical_size())
            throw std::invalid_argument("Tucker updated core domain mismatch");
        auto guard = retained_tucker_detail::guard(limits);
        retained_tucker_detail::support_budget(support_, guard);
        guard.sparse_vector(values);
        RetainedTuckerTensor result(domain_, support_, modes_,
            numeric::SparseRationalTensor(core_.domain(), core_.support(), std::move(values)));
        const auto reconstructed = result.reconstruct_values(guard, stats);
        result.require_source_support(reconstructed, guard, stats);
        return result;
    }

    [[nodiscard]] RetainedTuckerTensor reframe(
        std::vector<RetainedTuckerMode> modes, RetainedTuckerLimits limits = {},
        RetainedTuckerStats* stats = nullptr) const {
        return encode(reconstruct(limits, stats), std::move(modes), limits, stats);
    }

private:
    RetainedTuckerTensor(numeric::IndexDomain domain, numeric::StructuralSupport support,
                         std::vector<RetainedTuckerMode> modes, numeric::SparseRationalTensor core)
        : domain_(std::move(domain)), support_(std::move(support)),
          modes_(std::move(modes)), core_(std::move(core)) {}

    [[nodiscard]] numeric::SparseRationalVector reconstruct_values(
        numeric::ExactActionGuard& guard, RetainedTuckerStats* stats) const {
        auto domain = core_.domain();
        auto values = core_.values();
        guard.sparse_vector(values);
        for (std::size_t axis = 0; axis < modes_.size(); ++axis) {
            values = modes_[axis].act(values, domain, axis, false, guard, stats);
            auto extents = domain.extents();
            extents[axis] = modes_[axis].ambient_dimension();
            domain = numeric::IndexDomain(std::move(extents));
        }
        return values;
    }
    void require_source_support(const numeric::SparseRationalVector& values,
                                numeric::ExactActionGuard& guard, RetainedTuckerStats* stats) const {
        for (const auto& entry : values.entries()) {
            guard.step();
            if (stats) retained_tucker_detail::increment(stats->support_membership_checks);
            if (!support_.contains(entry.index))
                throw std::invalid_argument("Tucker reconstruction creates an absent source coordinate");
        }
    }

    numeric::IndexDomain domain_;
    numeric::StructuralSupport support_;
    std::vector<RetainedTuckerMode> modes_;
    numeric::SparseRationalTensor core_;
};

} // namespace rfc::field
