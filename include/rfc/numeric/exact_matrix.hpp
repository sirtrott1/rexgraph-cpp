#pragma once
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

class ExactMatrix {
public:
    using Key = std::pair<std::size_t, std::size_t>;

    ExactMatrix() = default;
    ExactMatrix(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols) {}
    ExactMatrix(std::size_t rows, std::size_t cols, std::map<Key, Rational> entries)
        : rows_(rows), cols_(cols), entries_(std::move(entries)) {
        normalize();
    }

    [[nodiscard]] bool is_identity() const {
        if (rows_ != cols_ || entries_.size() != rows_) return false;
        for (const auto& [ij, value] : entries_)
            if (ij.first != ij.second || value != 1) return false;
        return true;
    }

    static ExactMatrix identity(std::size_t n) {
        ExactMatrix out(n, n);
        for (std::size_t i = 0; i < n; ++i) out.set(i, i, Rational{1});
        return out;
    }

    static ExactMatrix diagonal(const RationalVector& d) {
        ExactMatrix out(d.size(), d.size());
        for (std::size_t i = 0; i < d.size(); ++i) if (d[i] != 0) out.set(i, i, d[i]);
        return out;
    }

    static ExactMatrix from_rows(const std::vector<RationalVector>& rows) {
        const std::size_t m = rows.size();
        const std::size_t n = m ? rows.front().size() : 0u;
        for (const auto& row : rows) if (row.size() != n) throw std::invalid_argument("matrix rows have unequal length");
        ExactMatrix out(m, n);
        for (std::size_t i = 0; i < m; ++i) {
            for (std::size_t j = 0; j < n; ++j) if (rows[i][j] != 0) out.set(i, j, rows[i][j]);
        }
        return out;
    }

    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept { return cols_; }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return entries_.size(); }
    [[nodiscard]] bool operator==(const ExactMatrix& other) const noexcept {
        return rows_ == other.rows_ && cols_ == other.cols_ && entries_ == other.entries_;
    }
    [[nodiscard]] bool operator!=(const ExactMatrix& other) const noexcept { return !(*this == other); }
    [[nodiscard]] const std::map<Key, Rational>& entries() const noexcept { return entries_; }

    [[nodiscard]] Rational get(std::size_t i, std::size_t j) const {
        check_index(i, j);
        const auto it = entries_.find({i,j});
        return it == entries_.end() ? Rational{0} : it->second;
    }

    void set(std::size_t i, std::size_t j, Rational value) {
        check_index(i, j);
        value.canonicalize();
        if (value == 0) entries_.erase({i,j});
        else entries_[{i,j}] = std::move(value);
    }

    [[nodiscard]] ExactMatrix transpose() const {
        ExactMatrix out(cols_, rows_);
        for (const auto& [key, value] : entries_) out.entries_[{key.second, key.first}] = value;
        return out;
    }

    [[nodiscard]] RationalVector apply(const RationalVector& x) const {
        if (x.size() != cols_) throw std::invalid_argument("matrix apply size mismatch");
        const auto input = canonical_vector(x);
        RationalVector out(rows_, Rational{0});
        for (const auto& [key, value] : entries_) out[key.first] += value * input[key.second];
        return out;
    }

    [[nodiscard]] ExactMatrix add(const ExactMatrix& other) const {
        if (rows_ != other.rows_ || cols_ != other.cols_) throw std::invalid_argument("matrix add dimension mismatch");
        ExactMatrix out = *this;
        for (const auto& [key, value] : other.entries_) out.set(key.first, key.second, out.get(key.first, key.second) + value);
        return out;
    }

    [[nodiscard]] ExactMatrix scale(const Rational& scalar) const {
        ExactMatrix out(rows_, cols_);
        if (scalar == 0) return out;
        for (const auto& [key, value] : entries_) out.set(key.first, key.second, scalar * value);
        return out;
    }

    [[nodiscard]] ExactMatrix multiply(const ExactMatrix& other) const {
        if (cols_ != other.rows_) throw std::invalid_argument("matrix multiply dimension mismatch");
        ExactMatrix out(rows_, other.cols_);
        // Sparse row/column index. This is a transparent exact reference kernel,
        // not the production sparse backend.
        std::vector<std::vector<std::pair<std::size_t,Rational>>> right_rows(other.rows_);
        for (const auto& [key, value] : other.entries_) right_rows[key.first].push_back({key.second, value});
        for (const auto& [key, left] : entries_) {
            for (const auto& [j, right] : right_rows[key.second]) {
                const auto current = out.get(key.first, j);
                out.set(key.first, j, current + left * right);
            }
        }
        return out;
    }

    [[nodiscard]] RationalVector solve(const RationalVector& rhs) const {
        if (rows_ != cols_ || rhs.size() != rows_) throw std::invalid_argument("exact solve requires square matrix and matching rhs");
        const std::size_t n = rows_;
        auto rows = sparse_rows();
        const auto input = canonical_vector(rhs);
        RationalVector b = input;
        for (std::size_t k = 0; k < n; ++k) {
            std::size_t pivot = n;
            std::size_t best_nnz = static_cast<std::size_t>(-1);
            for (std::size_t i = k; i < n; ++i) {
                const auto it = rows[i].find(k);
                if (it != rows[i].end() && it->second != 0 && rows[i].size() < best_nnz) {
                    pivot = i; best_nnz = rows[i].size();
                }
            }
            if (pivot == n) throw std::runtime_error("singular exact system");
            if (pivot != k) { std::swap(rows[pivot], rows[k]); std::swap(b[pivot], b[k]); }
            const Rational p = rows[k].at(k);
            for (std::size_t i = k + 1; i < n; ++i) {
                auto it = rows[i].find(k);
                if (it == rows[i].end() || it->second == 0) continue;
                const Rational ratio = it->second / p;
                rows[i].erase(k);
                for (const auto& [j, v] : rows[k]) {
                    if (j <= k) continue;
                    const auto jt = rows[i].find(j);
                    const Rational old = jt == rows[i].end() ? Rational{0} : jt->second;
                    Rational updated = old - ratio * v;
                    updated.canonicalize();
                    if (updated == 0) rows[i].erase(j); else rows[i][j] = std::move(updated);
                }
                b[i] -= ratio * b[k];
            }
        }
        RationalVector x(n, Rational{0});
        for (std::size_t ii = n; ii-- > 0;) {
            Rational sum = 0;
            for (const auto& [j,v] : rows[ii]) if (j > ii) sum += v * x[j];
            const auto it = rows[ii].find(ii);
            if (it == rows[ii].end() || it->second == 0) throw std::runtime_error("singular exact system in back substitution");
            x[ii] = (b[ii] - sum) / it->second;
            x[ii].canonicalize();
        }
        if (apply(x) != input) throw std::runtime_error("exact solve residual is nonzero");
        return x;
    }

    [[nodiscard]] ExactMatrix inverse() const {
        if (rows_ != cols_) throw std::invalid_argument("inverse requires square matrix");
        ExactMatrix out(rows_, cols_);
        for (std::size_t column = 0; column < cols_; ++column) {
            RationalVector basis(rows_, Rational{0});
            basis[column] = Rational{1};
            const auto solution = solve(basis);
            for (std::size_t row = 0; row < rows_; ++row) {
                if (solution[row] != 0) out.set(row, column, solution[row]);
            }
        }
        return out;
    }

    [[nodiscard]] ExactMatrix rref(std::vector<std::size_t>& pivots) const {
        auto rows = sparse_rows();
        pivots.clear();
        std::size_t k = 0;
        for (std::size_t j = 0; j < cols_ && k < rows_; ++j) {
            std::size_t pivot = rows_;
            std::size_t best_nnz = static_cast<std::size_t>(-1);
            for (std::size_t i = k; i < rows_; ++i) {
                const auto it = rows[i].find(j);
                if (it != rows[i].end() && it->second != 0 && rows[i].size() < best_nnz) {
                    pivot = i; best_nnz = rows[i].size();
                }
            }
            if (pivot == rows_) continue;
            if (pivot != k) std::swap(rows[pivot], rows[k]);
            const Rational scale_value = rows[k].at(j);
            for (auto& [col, value] : rows[k]) value /= scale_value;
            for (std::size_t i = 0; i < rows_; ++i) {
                if (i == k) continue;
                const auto it = rows[i].find(j);
                if (it == rows[i].end() || it->second == 0) continue;
                const Rational ratio = it->second;
                for (const auto& [col, value] : rows[k]) {
                    const auto jt = rows[i].find(col);
                    const Rational old = jt == rows[i].end() ? Rational{0} : jt->second;
                    Rational updated = old - ratio * value;
                    updated.canonicalize();
                    if (updated == 0) rows[i].erase(col); else rows[i][col] = std::move(updated);
                }
            }
            pivots.push_back(j);
            ++k;
        }
        ExactMatrix result(rows_, cols_);
        for (std::size_t i = 0; i < rows_; ++i) for (const auto& [j,v] : rows[i]) result.set(i,j,v);
        return result;
    }

    [[nodiscard]] std::size_t rank() const {
        std::vector<std::size_t> pivots;
        (void)rref(pivots);
        return pivots.size();
    }

    [[nodiscard]] ExactMatrix kernel_frame() const {
        std::vector<std::size_t> pivots;
        const auto reduced = rref(pivots);
        std::vector<bool> is_pivot(cols_, false);
        for (const auto p : pivots) is_pivot[p] = true;
        std::vector<std::size_t> free;
        for (std::size_t j = 0; j < cols_; ++j) if (!is_pivot[j]) free.push_back(j);
        ExactMatrix frame(cols_, free.size());
        for (std::size_t a = 0; a < free.size(); ++a) {
            const auto j = free[a];
            frame.set(j, a, Rational{1});
            for (std::size_t i = 0; i < pivots.size(); ++i) {
                const auto p = pivots[i];
                Rational v = -reduced.get(i,j);
                if (v != 0) frame.set(p,a,v);
            }
        }
        return frame;
    }

    [[nodiscard]] ExactMatrix image_frame() const {
        std::vector<std::size_t> pivots;
        (void)rref(pivots);
        ExactMatrix frame(rows_, pivots.size());
        for (std::size_t a = 0; a < pivots.size(); ++a) {
            const auto source_col = pivots[a];
            for (std::size_t i = 0; i < rows_; ++i) {
                const auto v = get(i, source_col);
                if (v != 0) frame.set(i,a,v);
            }
        }
        return frame;
    }

    [[nodiscard]] Rational determinant() const {
        if (rows_ != cols_) throw std::invalid_argument("determinant requires square matrix");
        const std::size_t n = rows_;
        if (n == 0) return Rational{1};
        auto rows = sparse_rows();
        Rational det = 1;
        int sign = 1;
        for (std::size_t k = 0; k < n; ++k) {
            std::size_t pivot = n;
            for (std::size_t i = k; i < n; ++i) {
                const auto it = rows[i].find(k);
                if (it != rows[i].end() && it->second != 0) { pivot = i; break; }
            }
            if (pivot == n) return Rational{0};
            if (pivot != k) { std::swap(rows[pivot], rows[k]); sign = -sign; }
            const Rational p = rows[k].at(k);
            det *= p;
            for (std::size_t i = k + 1; i < n; ++i) {
                auto it = rows[i].find(k);
                if (it == rows[i].end() || it->second == 0) continue;
                const Rational ratio = it->second / p;
                rows[i].erase(k);
                for (const auto& [j,v] : rows[k]) {
                    if (j <= k) continue;
                    const auto jt = rows[i].find(j);
                    const Rational old = jt == rows[i].end() ? Rational{0} : jt->second;
                    Rational updated = old - ratio * v;
                    updated.canonicalize();
                    if (updated == 0) rows[i].erase(j); else rows[i][j] = std::move(updated);
                }
            }
        }
        if (sign < 0) det = -det;
        det.canonicalize();
        return det;
    }

    [[nodiscard]] ExactMatrix select_columns(const std::vector<std::size_t>& columns) const {
        ExactMatrix out(rows_, columns.size());
        for (std::size_t a = 0; a < columns.size(); ++a) {
            if (columns[a] >= cols_) throw std::out_of_range("select column index");
            for (std::size_t i = 0; i < rows_; ++i) {
                const auto v = get(i, columns[a]);
                if (v != 0) out.set(i,a,v);
            }
        }
        return out;
    }

    static ExactMatrix horizontal_concat(const ExactMatrix& left, const ExactMatrix& right) {
        if (left.rows_ != right.rows_) throw std::invalid_argument("horizontal concat row mismatch");
        ExactMatrix out(left.rows_, left.cols_ + right.cols_);
        for (const auto& [key,v] : left.entries_) out.set(key.first,key.second,v);
        for (const auto& [key,v] : right.entries_) out.set(key.first,left.cols_+key.second,v);
        return out;
    }

    static ExactMatrix vertical_concat(const ExactMatrix& top, const ExactMatrix& bottom) {
        if (top.cols_ != bottom.cols_) throw std::invalid_argument("vertical concat column mismatch");
        ExactMatrix out(top.rows_ + bottom.rows_, top.cols_);
        for (const auto& [key,v] : top.entries_) out.set(key.first,key.second,v);
        for (const auto& [key,v] : bottom.entries_) out.set(top.rows_+key.first,key.second,v);
        return out;
    }

private:
    using SparseRow = std::map<std::size_t, Rational>;
    std::size_t rows_{};
    std::size_t cols_{};
    std::map<Key, Rational> entries_;

    void check_index(std::size_t i, std::size_t j) const {
        if (i >= rows_ || j >= cols_) throw std::out_of_range("matrix index");
    }
    void normalize() {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->first.first >= rows_ || it->first.second >= cols_) throw std::out_of_range("matrix entry index");
            it->second.canonicalize();
            if (it->second == 0) it = entries_.erase(it); else ++it;
        }
    }
    [[nodiscard]] std::vector<SparseRow> sparse_rows() const {
        std::vector<SparseRow> rows(rows_);
        for (const auto& [key,v] : entries_) rows[key.first][key.second] = v;
        return rows;
    }
};

inline ExactMatrix outer_product(const RationalVector& lhs, const RationalVector& rhs) {
    ExactMatrix out(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) if (lhs[i] != 0) {
        for (std::size_t j = 0; j < rhs.size(); ++j) if (rhs[j] != 0) out.set(i,j,lhs[i]*rhs[j]);
    }
    return out;
}

} // namespace rfc::numeric
