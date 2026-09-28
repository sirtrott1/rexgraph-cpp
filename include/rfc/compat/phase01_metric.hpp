#pragma once
#include "rfc/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc {
class DiagonalMetric {
public:
    explicit DiagonalMetric(std::vector<Exact> diag) : diag_(std::move(diag)) {
        if (diag_.empty()) throw std::invalid_argument("metric must be nonempty");
        for (const auto& q : diag_) if (q <= 0) throw std::invalid_argument("metric must be positive");
    }
    static DiagonalMetric identity(std::size_t n) { return DiagonalMetric(std::vector<Exact>(n, Exact{1})); }
    std::size_t size() const noexcept { return diag_.size(); }
    const Exact& operator[](std::size_t i) const { return diag_.at(i); }
    ExactVector apply(const ExactVector& x) const {
        if (x.size() != diag_.size()) throw std::invalid_argument("metric apply size mismatch");
        ExactVector out(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) out[i] = diag_[i] * x[i];
        return out;
    }
    ExactVector inverse_apply(const ExactVector& x) const {
        if (x.size() != diag_.size()) throw std::invalid_argument("metric inverse size mismatch");
        ExactVector out(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) out[i] = x[i] / diag_[i];
        return out;
    }
    Exact inner(const ExactVector& a, const ExactVector& b) const {
        if (a.size() != diag_.size() || b.size() != diag_.size()) throw std::invalid_argument("metric inner size mismatch");
        Exact out = 0;
        for (std::size_t i = 0; i < a.size(); ++i) out += a[i] * diag_[i] * b[i];
        return out;
    }
private:
    std::vector<Exact> diag_;
};
} // namespace rfc
