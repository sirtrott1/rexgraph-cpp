#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::temporal {

class OperationWord {
public:
    OperationWord() = default;
    explicit OperationWord(std::vector<numeric::ExactMatrix> operators)
        : operators_(std::move(operators)) {
        if (operators_.empty()) throw std::invalid_argument("operation word cannot be empty");
        for (std::size_t i = 1; i < operators_.size(); ++i) {
            if (operators_[i].cols() != operators_[i - 1u].rows()) throw std::invalid_argument("operation word dimensions do not compose");
        }
    }

    [[nodiscard]] std::size_t length() const noexcept { return operators_.size(); }
    [[nodiscard]] std::size_t input_dim() const noexcept { return operators_.front().cols(); }
    [[nodiscard]] std::size_t output_dim() const noexcept { return operators_.back().rows(); }
    [[nodiscard]] const numeric::ExactMatrix& op(std::size_t i) const { return operators_.at(i); }

    [[nodiscard]] numeric::ExactMatrix composite() const {
        numeric::ExactMatrix out = numeric::ExactMatrix::identity(input_dim());
        for (const auto& operation : operators_) out = operation.multiply(out);
        return out;
    }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& input) const {
        if (input.size() != input_dim()) throw std::invalid_argument("operation word input size mismatch");
        auto value = input;
        for (const auto& operation : operators_) value = operation.apply(value);
        return value;
    }

private:
    std::vector<numeric::ExactMatrix> operators_;
};

struct OperationWordDefect {
    numeric::ExactMatrix direct;
    numeric::ExactMatrix expanded;
    std::vector<numeric::ExactMatrix> position_defects;
    std::vector<numeric::ExactMatrix> contributions;

    [[nodiscard]] bool exact_identity() const noexcept { return direct.entries() == expanded.entries(); }
};

inline OperationWordDefect finite_word_defect(const OperationWord& old_word,
                                               const OperationWord& new_word,
                                               const std::vector<numeric::ExactMatrix>& correspondences) {
    if (old_word.length() != new_word.length()) throw std::invalid_argument("temporal word lengths differ");
    const auto m = old_word.length();
    if (correspondences.size() != m + 1u) throw std::invalid_argument("temporal word correspondence count mismatch");
    if (correspondences.front().cols() != old_word.input_dim() || correspondences.front().rows() != new_word.input_dim()) {
        throw std::invalid_argument("temporal input correspondence mismatch");
    }
    if (correspondences.back().cols() != old_word.output_dim() || correspondences.back().rows() != new_word.output_dim()) {
        throw std::invalid_argument("temporal output correspondence mismatch");
    }

    std::vector<numeric::ExactMatrix> defects;
    std::vector<numeric::ExactMatrix> contributions;
    defects.reserve(m); contributions.reserve(m);
    numeric::ExactMatrix expanded(new_word.output_dim(), old_word.input_dim());

    for (std::size_t i = 0; i < m; ++i) {
        const auto& old_op = old_word.op(i);
        const auto& new_op = new_word.op(i);
        if (correspondences[i].cols() != old_op.cols() || correspondences[i].rows() != new_op.cols() ||
            correspondences[i + 1u].cols() != old_op.rows() || correspondences[i + 1u].rows() != new_op.rows()) {
            throw std::invalid_argument("temporal intermediate correspondence mismatch");
        }
        const auto Ei = new_op.multiply(correspondences[i]).add(correspondences[i + 1u].multiply(old_op).scale(numeric::Rational{-1}));
        defects.push_back(Ei);

        numeric::ExactMatrix pre = numeric::ExactMatrix::identity(old_word.input_dim());
        for (std::size_t j = 0; j < i; ++j) pre = old_word.op(j).multiply(pre);

        numeric::ExactMatrix post = numeric::ExactMatrix::identity(new_op.rows());
        for (std::size_t j = i + 1u; j < m; ++j) post = new_word.op(j).multiply(post);

        auto contribution = post.multiply(Ei).multiply(pre);
        expanded = expanded.add(contribution);
        contributions.push_back(std::move(contribution));
    }

    const auto direct = new_word.composite().multiply(correspondences.front())
        .add(correspondences.back().multiply(old_word.composite()).scale(numeric::Rational{-1}));
    return {direct, expanded, std::move(defects), std::move(contributions)};
}

} // namespace rfc::temporal
