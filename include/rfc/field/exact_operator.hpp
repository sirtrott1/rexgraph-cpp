#pragma once
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/field/exact_metric.hpp"
#include <concepts>
#include <cstddef>
#include <stdexcept>

namespace rfc::field {

template<class Operator>
concept ExactLinearOperator = requires(const Operator& op, const numeric::RationalVector& x) {
    { op.domain_size() } -> std::convertible_to<std::size_t>;
    { op.codomain_size() } -> std::convertible_to<std::size_t>;
    { op.apply(x) } -> std::same_as<numeric::RationalVector>;
};

template<class Operator>
concept ExactTransposeOperator = ExactLinearOperator<Operator> && requires(const Operator& op, const numeric::RationalVector& y) {
    { op.transpose_apply(y) } -> std::same_as<numeric::RationalVector>;
};

template<class Operator>
concept ExactEndomorphism = requires(const Operator& op, const numeric::RationalVector& x) {
    { op.size() } -> std::convertible_to<std::size_t>;
    { op.apply(x) } -> std::same_as<numeric::RationalVector>;
};

class MaterializedExactOperator {
public:
    explicit MaterializedExactOperator(numeric::ExactMatrix matrix) : matrix_(std::move(matrix)) {}
    [[nodiscard]] std::size_t domain_size() const noexcept { return matrix_.cols(); }
    [[nodiscard]] std::size_t codomain_size() const noexcept { return matrix_.rows(); }
    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const { return matrix_.apply(x); }
    [[nodiscard]] numeric::RationalVector transpose_apply(const numeric::RationalVector& x) const { return matrix_.transpose().apply(x); }
    [[nodiscard]] const numeric::ExactMatrix& matrix() const noexcept { return matrix_; }
private:
    numeric::ExactMatrix matrix_;
};

template<ExactLinearOperator Operator>
[[nodiscard]] numeric::ExactMatrix materialize_exact_operator(const Operator& op) {
    numeric::ExactMatrix out(op.codomain_size(),op.domain_size());
    for(std::size_t j=0;j<op.domain_size();++j){
        numeric::RationalVector e(op.domain_size(),numeric::Rational{0});
        e[j]=1;
        const auto y=op.apply(e);
        if(y.size()!=op.codomain_size()) throw std::logic_error("exact operator returned wrong codomain size");
        for(std::size_t i=0;i<y.size();++i) if(y[i]!=0) out.set(i,j,y[i]);
    }
    return out;
}

struct ExactSpdSolveResult {
    numeric::RationalVector value;
    bool exact{};
    std::size_t iterations{};
};

// Exact metric conjugate gradients for a declared positive, M-self-adjoint
// endomorphism. The mathematical operator is supplied as an action and is not
// materialized. Exact arithmetic terminates in at most the carrier dimension
// in the non-breakdown SPD case.
template<class Action>
[[nodiscard]] ExactSpdSolveResult exact_metric_spd_solve(
    const ExactMetric& metric,
    const numeric::RationalVector& source,
    Action&& action) {
    if(source.size()!=metric.size()) throw std::invalid_argument("exact SPD solve source size mismatch");
    numeric::RationalVector x(source.size(),numeric::Rational{0});
    numeric::RationalVector residual=source;
    numeric::RationalVector direction=residual;
    auto residual_norm=metric.inner(residual,residual);
    if(residual_norm==0) return {std::move(x),true,0};
    for(std::size_t iteration=0;iteration<source.size();++iteration){
        const auto applied=action(direction);
        if(applied.size()!=source.size()) throw std::logic_error("exact SPD action size mismatch");
        const auto denominator=metric.inner(direction,applied);
        if(denominator==0) throw std::runtime_error("exact SPD Krylov breakdown");
        numeric::Rational alpha=residual_norm/denominator; alpha.canonicalize();
        x=numeric::add(x,numeric::scale(alpha,direction));
        residual=numeric::sub(residual,numeric::scale(alpha,applied));
        const auto next_norm=metric.inner(residual,residual);
        if(next_norm==0) return {std::move(x),true,iteration+1u};
        numeric::Rational beta=next_norm/residual_norm; beta.canonicalize();
        direction=numeric::add(residual,numeric::scale(beta,direction));
        residual_norm=next_norm;
    }
    return {std::move(x),numeric::is_zero(numeric::sub(action(x),source)),source.size()};
}

} // namespace rfc::field
