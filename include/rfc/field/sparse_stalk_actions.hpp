#pragma once

#include "rfc/field/exact_metric.hpp"
#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

using SparseStalkValues = std::map<std::uint32_t, numeric::SparseRationalVector>;
using SparseStalkMetrics = std::vector<std::vector<ExactMetric>>;

// Operation receipts count executed visits, not allocations or process memory.
// map_entries counts explicit transport coefficients inspected. Symbolic
// scalar-identity transports visit only the supplied nonzero coordinates.
// peak_numeric_nonzeros is the largest cardinality of a numeric field,
// accumulator, or temporary stalk observed by this layer. It is not the sum
// of all live buffers and does not inspect internal metric-action scratch.
struct SparseStalkActionWork {
    std::size_t active_stalks{}, incidences{}, map_entries{}, scalar_products{};
    std::size_t metric_apply_calls{}, metric_solve_calls{}, peak_numeric_nonzeros{};
    std::size_t checked_steps{};
};

class SparseStalkFieldActions;

// Numerical zero never removes a cell, occurrence, or coordinate from its
// immutable fiber owner. A missing map entry is an identically zero stalk
// value, and a missing coordinate in a present value is zero as well.
class SparseStalkField {
public:
    const local::OwnedLinearFiberSystem& owner() const & noexcept { return owner_; }
    const local::OwnedLinearFiberSystem& owner() const && = delete;
    core::Grade grade() const noexcept { return grade_; }
    const SparseStalkValues& values() const & noexcept { return values_; }
    const SparseStalkValues& values() const && = delete;
    std::size_t nonzeros() const noexcept { return nonzeros_; }
    std::size_t active_stalks() const noexcept { return values_.size(); }
    bool zero() const noexcept { return values_.empty(); }

private:
    friend class SparseStalkFieldActions;
    SparseStalkField(local::OwnedLinearFiberSystem owner, core::Grade grade,
                     SparseStalkValues values)
        : owner_(std::move(owner)), grade_(grade), values_(std::move(values)) {
        for (const auto& [cell, value] : values_) {
            (void)cell;
            nonzeros_ = machine::checked_size_add(nonzeros_, value.nonzeros());
        }
    }
    local::OwnedLinearFiberSystem owner_;
    core::Grade grade_{};
    SparseStalkValues values_;
    std::size_t nonzeros_{};
};

struct SparseStalkActionResult {
    SparseStalkField field;
    SparseStalkActionWork work;
};

struct SparseStalkTowerActionResult {
    std::vector<SparseStalkField> fields;
    SparseStalkActionWork work;
};

// Sparse lowering/raising on the actual retained local fiber system. The
// declared grade metric is the orthogonal direct sum of its per-cell metrics.
// General within-stalk metrics are supported through ExactMetric sparse ports;
// cross-stalk metric blocks require a separately declared grade-metric action.
// No whole-grade boundary, Hodge, projector, or amplitude array is constructed.
// A curved fiber realization is admitted for measured defects. Its two Hodge
// excursions remain defined, but strict Hodge/Dirac identities require d*d=0.
class SparseStalkFieldActions {
    using Q = numeric::Rational;
    using S = numeric::SparseRationalVector;
    using E = numeric::SparseRationalEntry;
    using Coordinates = std::map<std::size_t, Q>;
    using Accumulator = std::map<std::uint32_t, Coordinates>;
    using Guard = numeric::ExactActionGuard;
    struct State {
        local::OwnedLinearFiberSystem owner;
        SparseStalkMetrics metrics;
        numeric::ExactActionLimits limits;
    };
    std::shared_ptr<const State> state_;

    static void peak(SparseStalkActionWork& work, std::size_t size) {
        work.peak_numeric_nonzeros = std::max(work.peak_numeric_nonzeros, size);
    }
    static void add_work(SparseStalkActionWork& out, const SparseStalkActionWork& in) {
        const auto add = [](std::size_t& a, std::size_t b) { a = machine::checked_size_add(a, b); };
        add(out.active_stalks, in.active_stalks); add(out.incidences, in.incidences);
        add(out.map_entries, in.map_entries); add(out.scalar_products, in.scalar_products);
        add(out.metric_apply_calls, in.metric_apply_calls); add(out.metric_solve_calls, in.metric_solve_calls);
        add(out.checked_steps, in.checked_steps); peak(out, in.peak_numeric_nonzeros);
    }
    void require_grade(core::Grade grade) const {
        if (grade.value >= state_->owner.fibers().complex().grade_count())
            throw std::out_of_range("sparse stalk field grade is not retained");
    }
    void require_field(const SparseStalkField& field) const {
        if (!state_->owner.same_owner(field.owner_))
            throw std::invalid_argument("sparse stalk action requires the actual immutable fiber owner");
        require_grade(field.grade_);
    }
    static core::Grade lower(core::Grade grade) {
        return core::Grade{static_cast<std::uint16_t>(grade.value - 1u)};
    }
    std::size_t dimension(core::Grade grade, std::uint32_t cell) const {
        return state_->owner.fibers().stalks().dimension({grade, cell});
    }
    static S scaled_sparse(const S& value, const Q& coefficient,
                           Guard& guard, SparseStalkActionWork& work) {
        guard.sparse_vector(value); guard.value(coefficient);
        if (coefficient == 0) return S(value.size());
        std::vector<E> out;
        out.reserve(value.nonzeros());
        for (const auto& entry : value.entries()) {
            ++work.scalar_products;
            out.push_back({entry.index, guard.mul(coefficient, entry.value)});
        }
        peak(work, out.size());
        return S(value.size(), std::move(out));
    }
    static S mapped_sparse(const numeric::ExactMatrix& map, const S& value, bool transpose,
                           Guard& guard, SparseStalkActionWork& work) {
        if (value.size() != (transpose ? map.rows() : map.cols()))
            throw std::invalid_argument("sparse stalk transport input dimension");
        guard.sparse_vector(value);
        const auto output_size = transpose ? map.cols() : map.rows();
        guard.dimension(output_size);
        Coordinates out;
        for (const auto& [coordinate, coefficient] : map.entries()) {
            guard.step(); guard.value(coefficient); ++work.map_entries;
            const auto source = transpose ? coordinate.first : coordinate.second;
            const auto target = transpose ? coordinate.second : coordinate.first;
            const auto& entries = value.entries();
            const auto found = std::lower_bound(entries.begin(), entries.end(), source,
                [](const auto& entry, std::size_t index) { return entry.index < index; });
            if (found == entries.end() || found->index != source) continue;
            ++work.scalar_products;
            const auto old = out.find(target);
            Q sum = guard.add(old == out.end() ? Q{0} : old->second,
                              guard.mul(coefficient, found->value));
            sum.canonicalize();
            if (sum == 0) out.erase(target); else out[target] = std::move(sum);
            guard.entries(out.size()); peak(work, out.size());
        }
        std::vector<E> entries;
        entries.reserve(out.size());
        for (auto& [coordinate, value_out] : out) entries.push_back({coordinate, std::move(value_out)});
        return S(output_size, std::move(entries));
    }
    S transport(core::Grade grade, std::size_t slot, const S& value, bool transpose,
                Guard& guard, SparseStalkActionWork& work) const {
        const auto& fibers = state_->owner.fibers();
        if (const auto& map = fibers.transport_override(grade, slot))
            // An override is already realized, including its orientation/sign.
            return mapped_sparse(*map, value, transpose, guard, work);
        return scaled_sparse(value, fibers.complex().incidence_coefficient(grade, slot), guard, work);
    }
    static void accumulate(Accumulator& out, std::uint32_t cell, const S& value,
                           std::size_t& retained, Guard& guard, SparseStalkActionWork& work) {
        if (value.empty()) return;
        auto& target = out[cell];
        for (const auto& entry : value.entries()) {
            const auto old = target.find(entry.index);
            Q sum = guard.add(old == target.end() ? Q{0} : old->second, entry.value);
            sum.canonicalize();
            if (sum == 0) {
                if (old != target.end()) { target.erase(old); --retained; }
            } else {
                if (old == target.end()) retained = machine::checked_size_add(retained, 1);
                target[entry.index] = std::move(sum);
            }
            guard.entries(retained); peak(work, retained);
        }
        if (target.empty()) out.erase(cell);
    }
    SparseStalkField finish(core::Grade grade, Accumulator accumulated,
                           bool solve_metric, Guard& guard, SparseStalkActionWork& work) const {
        SparseStalkValues values;
        std::size_t nonzeros{};
        for (auto& [cell, coordinates] : accumulated) {
            std::vector<E> entries;
            entries.reserve(coordinates.size());
            for (auto& [coordinate, value] : coordinates) entries.push_back({coordinate, std::move(value)});
            S value(dimension(grade, cell), std::move(entries));
            if (solve_metric) {
                ++work.metric_solve_calls;
                value = metric({grade, cell}).solve(value.view());
                guard.sparse_vector(value); peak(work, value.nonzeros());
            }
            if (!value.empty()) {
                nonzeros = machine::checked_size_add(nonzeros, value.nonzeros());
                guard.entries(nonzeros);
                values.emplace(cell, std::move(value));
            }
        }
        peak(work, nonzeros);
        return SparseStalkField(state_->owner, grade, std::move(values));
    }

    SparseStalkActionResult sum_result(const SparseStalkField& a, const SparseStalkField& b) const {
        require_field(a); require_field(b);
        if (a.grade() != b.grade()) throw std::invalid_argument("sparse stalk sum grade mismatch");
        Guard guard(state_->limits); SparseStalkActionWork work; Accumulator out; std::size_t retained{};
        peak(work, a.nonzeros()); peak(work, b.nonzeros());
        for (const auto* field : {&a, &b})
            for (const auto& [cell, value] : field->values()) {
                guard.sparse_vector(value);
                accumulate(out, cell, value, retained, guard, work);
            }
        auto result = finish(a.grade(), std::move(out), false, guard, work);
        work.checked_steps = guard.steps();
        return {std::move(result), work};
    }
    SparseStalkActionResult scale_result(const SparseStalkField& input, Q coefficient) const {
        require_field(input); Guard guard(state_->limits); guard.value(coefficient); coefficient.canonicalize();
        SparseStalkActionWork work; SparseStalkValues values;
        peak(work, input.nonzeros());
        if (coefficient != 0) for (const auto& [cell, value] : input.values()) {
            auto result = scaled_sparse(value, coefficient, guard, work);
            if (!result.empty()) values.emplace(cell, std::move(result));
        }
        auto result = SparseStalkField(state_->owner, input.grade(), std::move(values));
        peak(work, result.nonzeros()); work.checked_steps = guard.steps();
        return {std::move(result), work};
    }

public:
    // Logical dimensions are declarations, not an allocation request. Numeric
    // support, work, and integer growth retain their ordinary exact limits.
    static numeric::ExactActionLimits default_limits() {
        numeric::ExactActionLimits limits;
        limits.dimension = std::numeric_limits<std::size_t>::max();
        return limits;
    }
    explicit SparseStalkFieldActions(local::OwnedLinearFiberSystem owner,
                                    SparseStalkMetrics metrics = {},
                                    numeric::ExactActionLimits limits = default_limits()) {
        const auto& fibers = owner.fibers();
        const auto grade_count = fibers.complex().grade_count();
        Guard guard(limits);
        if (metrics.empty()) {
            metrics.resize(grade_count);
            for (std::size_t k = 0; k < grade_count; ++k) {
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                for (std::size_t i = 0; i < fibers.complex().grade(grade).cell_count(); ++i)
                    metrics[k].push_back(ExactMetric::identity(fibers.stalks().dimension(
                        {grade, static_cast<std::uint32_t>(i)})));
            }
        }
        if (metrics.size() != grade_count) throw std::invalid_argument("sparse stalk metric grade count");
        for (std::size_t k = 0; k < grade_count; ++k) {
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            const auto& arena = fibers.complex().grade(grade);
            if (metrics[k].size() != arena.cell_count()) throw std::invalid_argument("sparse stalk metric cell count");
            for (std::size_t i = 0; i < arena.cell_count(); ++i) {
                guard.step();
                const auto n = fibers.stalks().dimension({grade, static_cast<std::uint32_t>(i)});
                guard.dimension(n);
                if (metrics[k][i].size() != n || !metrics[k][i].supports_sparse_apply() || !metrics[k][i].supports_sparse_solve())
                    throw std::invalid_argument("sparse stalk metric requires matching native sparse apply/solve ports");
            }
            // All retained transports are admitted, even when a field is zero
            // or an occurrence is currently inactive. No identity map is made.
            if (k) for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
                guard.step(); fibers.validate_transport(grade, slot);
            }
        }
        state_ = std::make_shared<const State>(State{std::move(owner), std::move(metrics), limits});
    }

    const local::OwnedLinearFiberSystem& owner() const & { return state_->owner; }
    const local::OwnedLinearFiberSystem& owner() const && = delete;
    const SparseStalkMetrics& metrics() const & { return state_->metrics; }
    const SparseStalkMetrics& metrics() const && = delete;
    const ExactMetric& metric(core::CellId cell) const & {
        require_grade(cell.grade); return state_->metrics.at(cell.grade.value).at(cell.index);
    }
    const ExactMetric& metric(core::CellId) const && = delete;
    numeric::ExactActionLimits limits() const noexcept { return state_->limits; }
    bool same_metric_owner(const SparseStalkFieldActions& other) const noexcept { return state_ == other.state_; }

    SparseStalkField bind(core::Grade grade, SparseStalkValues values = {}) const {
        require_grade(grade); Guard guard(state_->limits); std::size_t retained{};
        for (auto it = values.begin(); it != values.end();) {
            if (it->second.size() != dimension(grade, it->first))
                throw std::invalid_argument("sparse stalk field value dimension");
            guard.sparse_vector(it->second);
            if (it->second.empty()) it = values.erase(it);
            else {
                retained = machine::checked_size_add(retained, it->second.nonzeros());
                guard.entries(retained); ++it;
            }
        }
        return SparseStalkField(state_->owner, grade, std::move(values));
    }

    SparseStalkActionResult boundary(const SparseStalkField& input) const {
        require_field(input);
        const auto grade = input.grade();
        if (!grade.value) throw std::out_of_range("grade-zero stalk field has no retained lower boundary");
        const auto& fibers = state_->owner.fibers();
        const auto& arena = fibers.complex().grade(grade);
        const auto exists = fibers.complex().signature().existence_aspect();
        Guard guard(state_->limits); SparseStalkActionWork work;
        peak(work, input.nonzeros()); Accumulator out; std::size_t retained{};
        for (const auto& [cell, value] : input.values()) {
            guard.step(); ++work.active_stalks; guard.sparse_vector(value);
            const auto [begin, end] = arena.incidence_range(cell);
            for (auto slot = begin; slot < end; ++slot) {
                guard.step(); ++work.incidences;
                if (!arena.aspects.get(exists, slot)) continue;
                const auto local = transport(grade, slot, value, false, guard, work);
                accumulate(out, arena.lower_index[slot], local, retained, guard, work);
            }
        }
        auto result = finish(lower(grade), std::move(out), false, guard, work);
        work.checked_steps = guard.steps();
        return {std::move(result), work};
    }

    SparseStalkActionResult coboundary(core::Grade boundary_grade, const SparseStalkField& input) const {
        require_field(input); require_grade(boundary_grade);
        if (!boundary_grade.value || input.grade() != lower(boundary_grade))
            throw std::invalid_argument("sparse stalk adjoint requires the declared lower grade");
        const auto& fibers = state_->owner.fibers();
        const auto& arena = fibers.complex().grade(boundary_grade);
        const auto exists = fibers.complex().signature().existence_aspect();
        Guard guard(state_->limits); SparseStalkActionWork work;
        peak(work, input.nonzeros()); Accumulator out; std::size_t retained{};
        for (const auto& [cell, value] : input.values()) {
            guard.step(); ++work.active_stalks; ++work.metric_apply_calls;
            const auto weighted = metric({input.grade(), cell}).apply(value.view());
            guard.sparse_vector(weighted); peak(work, weighted.nonzeros());
            const auto [begin, end] = arena.coface_incidence_range(cell);
            for (auto index = begin; index < end; ++index) {
                guard.step(); ++work.incidences;
                const auto slot = arena.incidence_by_lower[index];
                if (!arena.aspects.get(exists, slot)) continue;
                const auto local = transport(boundary_grade, slot, weighted, true, guard, work);
                accumulate(out, arena.upper_index[slot], local, retained, guard, work);
            }
        }
        auto result = finish(boundary_grade, std::move(out), true, guard, work);
        work.checked_steps = guard.steps();
        return {std::move(result), work};
    }

    Q inner(const SparseStalkField& a, const SparseStalkField& b) const {
        require_field(a); require_field(b);
        if (a.grade() != b.grade()) throw std::invalid_argument("sparse stalk pairing grade mismatch");
        Guard guard(state_->limits); Q result{0};
        for (const auto& [cell, x] : a.values()) {
            const auto found = b.values().find(cell);
            if (found == b.values().end()) continue;
            guard.sparse_vector(x); guard.sparse_vector(found->second);
            result = guard.add(result, metric({a.grade(), cell}).inner_sparse(x.view(), found->second.view()));
        }
        result.canonicalize(); return result;
    }

    SparseStalkField plus(const SparseStalkField& a, const SparseStalkField& b) const {
        return sum_result(a, b).field;
    }
    SparseStalkField scaled(const SparseStalkField& input, Q coefficient) const {
        return scale_result(input, std::move(coefficient)).field;
    }

    SparseStalkActionResult chain_defect(const SparseStalkField& input) const {
        require_field(input);
        if (input.grade().value < 2) throw std::invalid_argument("stalk chain defect requires grade at least two");
        auto first = boundary(input); auto second = boundary(first.field);
        add_work(second.work, first.work); return second;
    }

    SparseStalkActionResult hodge(const SparseStalkField& input) const {
        require_field(input); auto result = bind(input.grade()); SparseStalkActionWork work;
        peak(work, input.nonzeros());
        if (input.grade().value) {
            const auto down = boundary(input);
            const auto back = coboundary(input.grade(), down.field);
            auto combined = sum_result(result, back.field); result = std::move(combined.field);
            add_work(work, down.work); add_work(work, back.work); add_work(work, combined.work);
        }
        if (input.grade().value + 1u < state_->owner.fibers().complex().grade_count()) {
            const auto up = coboundary({static_cast<std::uint16_t>(input.grade().value + 1u)}, input);
            const auto back = boundary(up.field);
            auto combined = sum_result(result, back.field); result = std::move(combined.field);
            add_work(work, up.work); add_work(work, back.work); add_work(work, combined.work);
        }
        peak(work, result.nonzeros()); return {std::move(result), work};
    }

    SparseStalkTowerActionResult dirac(const std::vector<SparseStalkField>& input, bool anti = false) const {
        const auto count = state_->owner.fibers().complex().grade_count();
        if (input.size() != count) throw std::invalid_argument("sparse stalk Dirac requires every retained grade");
        std::vector<SparseStalkField> result; result.reserve(count); SparseStalkActionWork work;
        for (std::size_t k = 0; k < count; ++k) {
            require_field(input[k]);
            if (input[k].grade().value != k) throw std::invalid_argument("sparse stalk Dirac grade order");
            result.push_back(bind({static_cast<std::uint16_t>(k)}));
        }
        for (std::size_t k = 1; k < count; ++k) {
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            const auto down = boundary(input[k]); const auto up = coboundary(grade, input[k - 1u]);
            auto lower_sum = sum_result(result[k - 1u], down.field);
            result[k - 1u] = std::move(lower_sum.field); add_work(work, lower_sum.work);
            if (anti) {
                auto negative = scale_result(up.field, Q{-1}); add_work(work, negative.work);
                auto upper_sum = sum_result(result[k], negative.field);
                result[k] = std::move(upper_sum.field); add_work(work, upper_sum.work);
            } else {
                auto upper_sum = sum_result(result[k], up.field);
                result[k] = std::move(upper_sum.field); add_work(work, upper_sum.work);
            }
            add_work(work, down.work); add_work(work, up.work);
        }
        for (const auto& field : result) peak(work, field.nonzeros());
        return {std::move(result), work};
    }
};

} // namespace rfc::field
