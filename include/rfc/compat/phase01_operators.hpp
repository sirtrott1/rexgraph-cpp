#pragma once
#include "rfc/compat/phase01_relational.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc {

class HodgeOperator {
public:
    HodgeOperator(const BoundaryStore& down,
                  const DiagonalMetric& metric_below,
                  const DiagonalMetric& metric_here,
                  const BoundaryStore* up = nullptr,
                  const DiagonalMetric* metric_above = nullptr)
        : down_(down), metric_below_(metric_below), metric_here_(metric_here),
          up_(up), metric_above_(metric_above) {
        if (down_.lower_count() != metric_below_.size() || down_.upper_count() != metric_here_.size()) {
            throw std::invalid_argument("HodgeOperator: down metric mismatch");
        }
        if ((up_ == nullptr) != (metric_above_ == nullptr)) throw std::invalid_argument("HodgeOperator: up metric missing");
        if (up_ != nullptr) {
            if (up_->lower_count() != size() || up_->upper_count() != metric_above_->size()) {
                throw std::invalid_argument("HodgeOperator: up metric mismatch");
            }
        }
    }

    std::size_t size() const noexcept { return metric_here_.size(); }
    const DiagonalMetric& metric() const noexcept { return metric_here_; }

    ExactVector apply_down(const ExactVector& x) const {
        return down_.adjoint(down_.apply(x), metric_below_, metric_here_);
    }
    ExactVector apply_up(const ExactVector& x) const {
        if (up_ == nullptr) return ExactVector(size(), Exact{0});
        return up_->apply(up_->adjoint(x, metric_here_, *metric_above_));
    }
    ExactVector apply(const ExactVector& x) const { return add(apply_down(x), apply_up(x)); }

private:
    const BoundaryStore& down_;
    const DiagonalMetric& metric_below_;
    const DiagonalMetric& metric_here_;
    const BoundaryStore* up_{};
    const DiagonalMetric* metric_above_{};
};

class ResolventOperator {
public:
    ResolventOperator(const HodgeOperator& L, Exact lambda) : L_(L), lambda_(std::move(lambda)) {}
    std::size_t size() const noexcept { return L_.size(); }
    const DiagonalMetric& metric() const noexcept { return L_.metric(); }
    ExactVector apply(const ExactVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("ResolventOperator size mismatch");
        return add(x, scale(lambda_, L_.apply(x)));
    }
private:
    const HodgeOperator& L_;
    Exact lambda_;
};

struct CGResult { ExactVector x; std::size_t iterations{}; bool exact{}; };

inline CGResult exact_cg(const ResolventOperator& A, const ExactVector& b) {
    if (b.size() != A.size()) throw std::invalid_argument("exact_cg size mismatch");
    ExactVector x(b.size(), Exact{0});
    ExactVector r = b;
    ExactVector p = r;
    Exact rr = A.metric().inner(r, r);
    if (rr == 0) return {std::move(x), 0u, true};
    for (std::size_t iter = 0; iter < b.size(); ++iter) {
        const auto Ap = A.apply(p);
        const Exact denom = A.metric().inner(p, Ap);
        if (denom == 0) throw std::runtime_error("exact_cg breakdown");
        const Exact alpha = rr / denom;
        x = add(x, scale(alpha, p));
        r = sub(r, scale(alpha, Ap));
        const Exact rr_new = A.metric().inner(r, r);
        if (rr_new == 0) return {std::move(x), iter + 1u, true};
        const Exact beta = rr_new / rr;
        p = add(r, scale(beta, p));
        rr = rr_new;
    }
    return {std::move(x), b.size(), is_zero(sub(A.apply(x), b))};
}

struct TowerField3 { ExactVector c0; ExactVector c1; ExactVector c2; };

inline TowerField3 dirac_apply(const BoundaryStore& B1, const BoundaryStore& B2,
                               const DiagonalMetric& M0, const DiagonalMetric& M1,
                               const DiagonalMetric& M2, const TowerField3& x) {
    if (x.c0.size() != M0.size() || x.c1.size() != M1.size() || x.c2.size() != M2.size()) {
        throw std::invalid_argument("dirac_apply field size mismatch");
    }
    TowerField3 out;
    out.c0 = B1.apply(x.c1);
    out.c1 = add(B1.adjoint(x.c0, M0, M1), B2.apply(x.c2));
    out.c2 = B2.adjoint(x.c1, M1, M2);
    return out;
}

} // namespace rfc
