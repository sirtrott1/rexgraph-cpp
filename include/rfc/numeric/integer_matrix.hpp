#pragma once
#include "rfc/machine/checked_size.hpp"
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

class IntegerMatrix {
public:
    IntegerMatrix() = default;
    IntegerMatrix(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols), data_(machine::checked_size_mul(rows, cols), Integer{0}) {}

    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept { return cols_; }

    [[nodiscard]] const Integer& get(std::size_t row, std::size_t col) const {
        check(row, col);
        return data_[row * cols_ + col];
    }
    [[nodiscard]] Integer& at(std::size_t row, std::size_t col) {
        check(row, col);
        return data_[row * cols_ + col];
    }
    void set(std::size_t row, std::size_t col, Integer value) {
        check(row, col);
        data_[row * cols_ + col] = std::move(value);
    }

    [[nodiscard]] static IntegerMatrix identity(std::size_t n) {
        IntegerMatrix out(n, n);
        for (std::size_t i = 0; i < n; ++i) out.at(i, i) = 1;
        return out;
    }

    [[nodiscard]] IntegerMatrix transpose() const {
        IntegerMatrix out(cols_, rows_);
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                out.at(j, i) = get(i, j);
        return out;
    }

    [[nodiscard]] IntegerMatrix operator*(const IntegerMatrix& rhs) const {
        if (cols_ != rhs.rows_) throw std::invalid_argument("integer matrix multiply shape mismatch");
        IntegerMatrix out(rows_, rhs.cols_);
        for (std::size_t i = 0; i < rows_; ++i) {
            for (std::size_t k = 0; k < cols_; ++k) {
                const auto& a = get(i, k);
                if (a == 0) continue;
                for (std::size_t j = 0; j < rhs.cols_; ++j) {
                    const auto& b = rhs.get(k, j);
                    if (b != 0) out.at(i, j) += a * b;
                }
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<Integer> apply(const std::vector<Integer>& x) const {
        if (x.size() != cols_) throw std::invalid_argument("integer matrix vector shape mismatch");
        std::vector<Integer> out(rows_, Integer{0});
        for (std::size_t i = 0; i < rows_; ++i)
            for (std::size_t j = 0; j < cols_; ++j)
                if (get(i, j) != 0 && x[j] != 0) out[i] += get(i, j) * x[j];
        return out;
    }

    [[nodiscard]] bool operator==(const IntegerMatrix& rhs) const noexcept {
        return rows_ == rhs.rows_ && cols_ == rhs.cols_ && data_ == rhs.data_;
    }

    void swap_rows(std::size_t a, std::size_t b) {
        if (a >= rows_ || b >= rows_) throw std::out_of_range("integer matrix row swap");
        if (a == b) return;
        for (std::size_t j = 0; j < cols_; ++j) std::swap(at(a, j), at(b, j));
    }
    void swap_cols(std::size_t a, std::size_t b) {
        if (a >= cols_ || b >= cols_) throw std::out_of_range("integer matrix column swap");
        if (a == b) return;
        for (std::size_t i = 0; i < rows_; ++i) std::swap(at(i, a), at(i, b));
    }
    void row_add_multiple(std::size_t target, std::size_t source, const Integer& multiple) {
        if (target >= rows_ || source >= rows_) throw std::out_of_range("integer matrix row add");
        if (multiple == 0) return;
        for (std::size_t j = 0; j < cols_; ++j) at(target, j) += multiple * get(source, j);
    }
    void col_add_multiple(std::size_t target, std::size_t source, const Integer& multiple) {
        if (target >= cols_ || source >= cols_) throw std::out_of_range("integer matrix col add");
        if (multiple == 0) return;
        for (std::size_t i = 0; i < rows_; ++i) at(i, target) += multiple * get(i, source);
    }
    void row_scale(std::size_t row, const Integer& factor) {
        if (row >= rows_) throw std::out_of_range("integer matrix row scale");
        for (std::size_t j = 0; j < cols_; ++j) at(row, j) *= factor;
    }
    void col_scale(std::size_t col, const Integer& factor) {
        if (col >= cols_) throw std::out_of_range("integer matrix col scale");
        for (std::size_t i = 0; i < rows_; ++i) at(i, col) *= factor;
    }

private:
    std::size_t rows_{};
    std::size_t cols_{};
    std::vector<Integer> data_;

    void check(std::size_t row, std::size_t col) const {
        if (row >= rows_ || col >= cols_) throw std::out_of_range("integer matrix index");
    }
};

inline Integer abs_integer(Integer value) {
    if (value < 0) value = -value;
    return value;
}

} // namespace rfc::numeric
