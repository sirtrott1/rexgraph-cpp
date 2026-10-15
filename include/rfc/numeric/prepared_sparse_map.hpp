#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include <functional>
#include <exception>
#include <memory>
#include <set>
#include <unordered_map>

namespace rfc::numeric {

// Numerical map execution only. The consumer retains the actual carrier,
// source publication and measurement meanings; equal dimensions do not bind them.
using SparseMapCheck = std::function<void()>;

inline StructuralSupport intersect_support(const StructuralSupport& left,
                                            const StructuralSupport& right,
                                            const SparseMapCheck& check = {}) {
    if (left.size() != right.size()) throw std::invalid_argument("support intersection size");
    if (left.storage() == StructuralSupport::Storage::Full) return right;
    if (right.storage() == StructuralSupport::Storage::Full) return left;
    const auto& smaller = left.count() <= right.count() ? left : right;
    const auto& larger = left.count() <= right.count() ? right : left;
    std::vector<std::size_t> active;
    smaller.for_each_active([&](std::size_t i) {
        if (check) check();
        if (larger.contains(i)) active.push_back(i);
    });
    return StructuralSupport::sparse(left.size(), std::move(active));
}

namespace sparse_map_detail {
struct Mutation {
    bool& failed;
    int exceptions = std::uncaught_exceptions();
    explicit Mutation(bool& value) : failed(value) {
        if (failed) throw std::logic_error("failed sparse accumulator cannot be reused");
    }
    ~Mutation() { if (std::uncaught_exceptions() > exceptions) failed = true; }
};
inline void require_valid(bool failed) {
    if (failed) throw std::logic_error("failed sparse accumulator cannot be published");
}
}

class SparseExactFieldSum {
public:
    explicit SparseExactFieldSum(std::size_t size) : size_(size), support_(StructuralSupport::full(size)) {}
    void add(const SparseRationalTensor& field, const Rational& coefficient = Rational{1},
             const SparseMapCheck& check = {}) {
        sparse_map_detail::Mutation mutation(failed_);
        if (field.domain().extents() != std::vector<std::size_t>{size_})
            throw std::invalid_argument("field sum domain");
        if (coefficient == 0) return;
        support_ = intersect_support(support_, field.support(), check);
        for (const auto& entry : field.values().entries()) {
            if (check) check();
            if (coefficient == 1) add_value(entry.index, entry.value);
            else add_value(entry.index, coefficient * entry.value);
        }
    }
    void merge(const SparseExactFieldSum& other, const SparseMapCheck& check = {}) {
        sparse_map_detail::Mutation mutation(failed_);
        sparse_map_detail::require_valid(other.failed_);
        if (size_ != other.size_) throw std::invalid_argument("field sum merge domain");
        support_ = intersect_support(support_, other.support_, check);
        for (const auto& [i, value] : other.values_) {
            if (check) check();
            add_value(i, value);
        }
    }
    [[nodiscard]] SparseRationalTensor finish(const SparseMapCheck& check = {}) const {
        sparse_map_detail::require_valid(failed_);
        std::vector<SparseRationalEntry> values;
        values.reserve(values_.size());
        for (const auto& [i, value] : values_) {
            if (check) check();
            if (support_.contains(i)) values.push_back({i, value});
        }
        return SparseRationalTensor(IndexDomain({size_}), support_, SparseRationalVector(size_, std::move(values)));
    }
private:
    void add_value(std::size_t i, const Rational& value) {
        auto [it, inserted] = values_.try_emplace(i, value);
        if (!inserted) it->second += value;
        it->second.canonicalize();
        if (it->second == 0) values_.erase(it);
    }
    std::size_t size_;
    bool failed_{};
    StructuralSupport support_;
    std::unordered_map<std::size_t, Rational> values_;
};

class PreparedSparseExactMap {
    struct State {
        struct Coefficient { std::size_t row; Rational value; };
        ExactMatrix map;
        std::map<std::size_t, std::vector<Coefficient>> columns;
        bool identity;
        explicit State(ExactMatrix value) : map(std::move(value)),
            identity(map.is_identity()) {
            for (const auto& [ij, coefficient] : map.entries()) {
                columns[ij.second].push_back({ij.first, coefficient});
            }
        }
    };
public:
    class FieldAccumulator {
    public:
        void add_column(std::size_t input, const SparseRationalTensor& field,
                        const SparseMapCheck& check = {}) {
            sparse_map_detail::Mutation mutation(failed_);
            if (input >= state_->map.cols()) throw std::out_of_range("field map input");
            if (field.domain().extents() != std::vector<std::size_t>{size_})
                throw std::invalid_argument("field map domain");
            const auto it = state_->columns.find(input);
            if (it == state_->columns.end()) return;
            if (!inputs_.insert(input).second)
                throw std::invalid_argument("field map input supplied twice");
            for (const auto& coefficient : it->second) {
                if (check) check();
                auto [target, inserted] = sums_.try_emplace(coefficient.row, size_);
                (void)inserted;
                target->second.add(field, coefficient.value, check);
            }
        }
        void merge(const FieldAccumulator& other, const SparseMapCheck& check = {}) {
            sparse_map_detail::Mutation mutation(failed_);
            sparse_map_detail::require_valid(other.failed_);
            if (state_ != other.state_ || size_ != other.size_)
                throw std::invalid_argument("field map merge owner/domain");
            for (const auto input : other.inputs_) {
                if (check) check();
                if (!inputs_.insert(input).second)
                    throw std::invalid_argument("field map merge repeats an input");
            }
            for (const auto& [row, field] : other.sums_) {
                if (check) check();
                auto [target, inserted] = sums_.try_emplace(row, size_);
                (void)inserted;
                target->second.merge(field, check);
            }
        }
        [[nodiscard]] SparseRationalTensor finish_column(std::size_t output,
                                                         const SparseMapCheck& check = {}) const {
            sparse_map_detail::require_valid(failed_);
            if (output >= state_->map.rows()) throw std::out_of_range("field map output");
            const auto& entries = state_->map.entries();
            for (auto it = entries.lower_bound({output, 0}); it != entries.end() && it->first.first == output; ++it) {
                if (check) check();
                if (!inputs_.contains(it->first.second))
                    throw std::invalid_argument("field map output has an unsupplied required input");
            }
            const auto it = sums_.find(output);
            if (it == sums_.end())
                return SparseRationalTensor::zero(IndexDomain({size_}), StructuralSupport::full(size_));
            return it->second.finish(check);
        }
    private:
        friend class PreparedSparseExactMap;
        FieldAccumulator(std::shared_ptr<const State> state, std::size_t size)
            : state_(std::move(state)), size_(size) {}
        std::shared_ptr<const State> state_;
        std::size_t size_;
        bool failed_{};
        std::set<std::size_t> inputs_;
        std::map<std::size_t, SparseExactFieldSum> sums_;
    };

    explicit PreparedSparseExactMap(ExactMatrix map) : state_(std::make_shared<const State>(std::move(map))) {}
    [[nodiscard]] const ExactMatrix& matrix() const & { return state_->map; }
    const ExactMatrix& matrix() const && = delete;
    [[nodiscard]] FieldAccumulator field_accumulator(std::size_t field_size) const {
        return FieldAccumulator(state_, field_size);
    }
    [[nodiscard]] SparseRationalVector apply(SparseRationalView input,
                                             const SparseMapCheck& check = {}) const {
        std::optional<std::size_t> previous;
        for (const auto& entry : input.entries()) {
            if (check) check();
            if (entry.index >= input.size() || (previous && entry.index <= *previous) || entry.value == 0)
                throw std::invalid_argument("prepared map requires canonical sparse coordinates");
            previous = entry.index;
        }
        return apply_canonical(input, check);
    }
    [[nodiscard]] SparseRationalVector apply(const SparseRationalVector& input,
                                             const SparseMapCheck& check = {}) const {
        return apply_canonical(input.view(), check);
    }
private:
    [[nodiscard]] SparseRationalVector apply_canonical(SparseRationalView input,
                                             const SparseMapCheck& check = {}) const {
        if (input.size() != state_->map.cols()) throw std::invalid_argument("prepared sparse map input size");
        if (state_->identity)
            return SparseRationalVector(input.size(), {input.entries().begin(), input.entries().end()});
        std::vector<SparseRationalEntry> values;
        if (state_->map.nonzeros() <= input.nonzeros()) {
            for (const auto& [ij, coefficient] : state_->map.entries()) {
                if (check) check();
                const auto xs = input.entries();
                const auto it = std::lower_bound(xs.begin(), xs.end(), ij.second,
                    [](const auto& entry, std::size_t index) { return entry.index < index; });
                if (it != xs.end() && it->index == ij.second)
                    values.push_back({ij.first, coefficient * it->value});
            }
        } else {
            for (const auto& entry : input.entries()) {
                if (check) check();
                const auto it = state_->columns.find(entry.index);
                if (it == state_->columns.end()) continue;
                for (const auto& coefficient : it->second) {
                    if (check) check();
                    values.push_back({coefficient.row, coefficient.value * entry.value});
                }
            }
        }
        return SparseRationalVector(state_->map.rows(), std::move(values));
    }
public:
    [[nodiscard]] SparseRationalTensor apply(const SparseRationalTensor& input,
                                             const SparseMapCheck& check = {}) const {
        if (input.domain().extents() != std::vector<std::size_t>{state_->map.cols()})
            throw std::invalid_argument("prepared sparse map tensor domain");
        if (state_->identity) { if (check) check(); return input; }
        auto values = apply_canonical(input.values().view(), check);
        auto support = StructuralSupport::full(state_->map.rows());
        if (input.support().storage() != StructuralSupport::Storage::Full) {
            std::set<std::size_t> missing;
            for (const auto& [ij, coefficient] : state_->map.entries()) {
                (void)coefficient;
                if (check) check();
                if (!input.support().contains(ij.second)) missing.insert(ij.first);
            }
            if (!missing.empty()) {
                std::vector<std::size_t> active;
                for (std::size_t row = 0; row < state_->map.rows(); ++row) {
                    if (check) check();
                    if (!missing.contains(row)) active.push_back(row);
                }
                support = StructuralSupport::sparse(state_->map.rows(), std::move(active));
                std::vector<SparseRationalEntry> retained;
                for (const auto& entry : values.entries())
                    if (support.contains(entry.index)) retained.push_back(entry);
                values = SparseRationalVector(state_->map.rows(), std::move(retained));
            }
        }
        return SparseRationalTensor(IndexDomain({state_->map.rows()}), std::move(support), std::move(values));
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
