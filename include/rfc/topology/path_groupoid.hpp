#pragma once
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/core/relational_complex.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::topology {

struct PairwiseEndpoints {
    std::uint32_t tail{};
    std::uint32_t head{};
};

inline PairwiseEndpoints pairwise_endpoints(const core::RelationalComplex& complex,
                                            std::size_t relation_index) {
    if (complex.grade_count() < 2u || relation_index >= complex.grade(core::Grade{1}).cell_count()) {
        throw std::out_of_range("pairwise relation index");
    }
    const core::BoundaryOccurrenceFiber fiber(complex, core::Grade{1}, relation_index);
    const core::BoundaryOccurrenceView* negative = nullptr;
    const core::BoundaryOccurrenceView* positive = nullptr;
    std::size_t active = 0;
    numeric::Rational negative_magnitude{0};
    numeric::Rational positive_magnitude{0};
    for (const auto& occurrence : fiber.occurrences()) {
        if (!occurrence.exists) continue;
        ++active;
        if (occurrence.coefficient < 0) {
            if (negative != nullptr) throw std::invalid_argument("pairwise path relation has multiple negative endpoints");
            negative = &occurrence;
            negative_magnitude = -occurrence.coefficient;
        } else if (occurrence.coefficient > 0) {
            if (positive != nullptr) throw std::invalid_argument("pairwise path relation has multiple positive endpoints");
            positive = &occurrence;
            positive_magnitude = occurrence.coefficient;
        }
    }
    if (active != 2u || negative == nullptr || positive == nullptr || negative_magnitude != positive_magnitude) {
        throw std::invalid_argument("path-groupoid relation must be an oriented pairwise zero-sum relation");
    }
    return PairwiseEndpoints{negative->lower.index, positive->lower.index};
}

struct EdgeStep {
    std::uint32_t relation{};
    bool forward{true};
    constexpr auto operator<=>(const EdgeStep&) const = default;
};

class PairwisePath {
public:
    PairwisePath() = default;

    PairwisePath(const core::RelationalComplex& complex,
                 std::uint32_t source,
                 std::vector<EdgeStep> steps)
        : complex_(&complex), source_(source), steps_(std::move(steps)) {
        if (complex.grade_count() < 2u || source_ >= complex.grade(core::Grade{0}).cell_count()) {
            throw std::out_of_range("path source vertex");
        }
        target_ = validate_and_target(source_, steps_);
    }

    [[nodiscard]] const core::RelationalComplex& complex() const {
        if (complex_ == nullptr) throw std::logic_error("path has no declared source owner");
        return *complex_;
    }
    [[nodiscard]] std::uint32_t source() const noexcept { return source_; }
    [[nodiscard]] std::uint32_t target() const noexcept { return target_; }
    [[nodiscard]] const std::vector<EdgeStep>& steps() const noexcept { return steps_; }
    [[nodiscard]] bool is_loop() const noexcept { return source_ == target_; }

    [[nodiscard]] PairwisePath inverse() const {
        std::vector<EdgeStep> reverse;
        reverse.reserve(steps_.size());
        for (auto it = steps_.rbegin(); it != steps_.rend(); ++it) reverse.push_back(EdgeStep{it->relation, !it->forward});
        return PairwisePath(complex(), target_, std::move(reverse));
    }

    [[nodiscard]] PairwisePath reduced() const {
        std::vector<EdgeStep> reduced_steps;
        reduced_steps.reserve(steps_.size());
        for (const auto& step : steps_) {
            if (!reduced_steps.empty() && reduced_steps.back().relation == step.relation &&
                reduced_steps.back().forward != step.forward) {
                reduced_steps.pop_back();
            } else {
                reduced_steps.push_back(step);
            }
        }
        return PairwisePath(complex(), source_, std::move(reduced_steps));
    }

    [[nodiscard]] static PairwisePath compose(const PairwisePath& first, const PairwisePath& second) {
        if (first.complex_ != second.complex_) throw std::invalid_argument("path composition complex mismatch");
        if (first.target_ != second.source_) throw std::invalid_argument("path composition endpoint mismatch");
        std::vector<EdgeStep> steps = first.steps_;
        steps.insert(steps.end(), second.steps_.begin(), second.steps_.end());
        return PairwisePath(first.complex(), first.source_, std::move(steps)).reduced();
    }

private:
    [[nodiscard]] std::uint32_t validate_and_target(std::uint32_t current,
                                                    const std::vector<EdgeStep>& steps) const {
        for (const auto& step : steps) {
            const auto endpoints = pairwise_endpoints(*complex_, step.relation);
            const auto from = step.forward ? endpoints.tail : endpoints.head;
            const auto to = step.forward ? endpoints.head : endpoints.tail;
            if (current != from) throw std::invalid_argument("path step is not incident to current endpoint");
            current = to;
        }
        return current;
    }

    const core::RelationalComplex* complex_{};
    std::uint32_t source_{};
    std::uint32_t target_{};
    std::vector<EdgeStep> steps_;
};

class PairwisePathGroupoid {
public:
    explicit PairwisePathGroupoid(const core::RelationalComplex& complex) : complex_(&complex) {}

    [[nodiscard]] PairwisePath identity(std::uint32_t object) const {
        return PairwisePath(*complex_, object, {});
    }
    [[nodiscard]] PairwisePath generator(std::uint32_t relation, bool forward = true) const {
        const auto endpoints = pairwise_endpoints(*complex_, relation);
        return PairwisePath(*complex_, forward ? endpoints.tail : endpoints.head,
                            {EdgeStep{relation, forward}});
    }
    [[nodiscard]] PairwisePath compose(const PairwisePath& first, const PairwisePath& second) const {
        return PairwisePath::compose(first, second);
    }

private:
    const core::RelationalComplex* complex_{};
};

} // namespace rfc::topology
