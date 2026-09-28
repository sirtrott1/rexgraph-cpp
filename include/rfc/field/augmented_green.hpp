#pragma once
#include "rfc/field/exact_hodge.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rfc::field {

struct ExactGreenResult {
    numeric::RationalVector value;
    bool exact{};
    std::size_t iterations{};
};

// Production exact Green solve. The requested operator is evaluated through
// Hodge actions; no Hodge matrix, dense inverse, or eigendecomposition is built.
// For lambda > 0, I + lambda L is positive in the declared exact metric and
// exact conjugate gradients terminates in at most the grade dimension in exact
// arithmetic (absent an implementation breakdown).
inline ExactGreenResult exact_green_apply(const ExactHodgeOperator& hodge,
                                          numeric::Rational lambda,
                                          const numeric::RationalVector& source) {
    if (source.size()!=hodge.size()) throw std::invalid_argument("exact Green source size mismatch");
    if (lambda <= 0) throw std::invalid_argument("operator-first exact Green requires positive lambda");
    numeric::RationalVector x(source.size(),numeric::Rational{0});
    numeric::RationalVector residual=source;
    numeric::RationalVector direction=residual;
    auto residual_norm=hodge.metric().inner(residual,residual);
    if (residual_norm==0) return {std::move(x),true,0};

    auto apply_resolvent=[&](const numeric::RationalVector& v){
        return numeric::add(v,numeric::scale(lambda,hodge.apply(v)));
    };

    for(std::size_t iteration=0;iteration<source.size();++iteration){
        const auto applied=apply_resolvent(direction);
        const auto denominator=hodge.metric().inner(direction,applied);
        if(denominator==0) throw std::runtime_error("exact Green Krylov breakdown");
        numeric::Rational alpha=residual_norm/denominator; alpha.canonicalize();
        x=numeric::add(x,numeric::scale(alpha,direction));
        residual=numeric::sub(residual,numeric::scale(alpha,applied));
        const auto next_norm=hodge.metric().inner(residual,residual);
        if(next_norm==0) return {std::move(x),true,iteration+1u};
        numeric::Rational beta=next_norm/residual_norm; beta.canonicalize();
        direction=numeric::add(residual,numeric::scale(beta,direction));
        residual_norm=next_norm;
    }
    return {std::move(x),numeric::is_zero(numeric::sub(apply_resolvent(x),source)),source.size()};
}

inline ExactGreenResult exact_green_apply(const core::RelationalComplex& complex,
                                          core::Grade grade,
                                          const std::vector<ExactMetric>& metrics,
                                          numeric::Rational lambda,
                                          const numeric::RationalVector& source) {
    return exact_green_apply(ExactHodgeOperator(complex,grade,metrics),std::move(lambda),source);
}

// Transparent materialized reference implementation retained for validation,
// unusual nonsingular parameters, and cross-checks against the action path.
class ExactBlockSystem {
public:
    explicit ExactBlockSystem(std::size_t n) : matrix_(n,n) {}
    void put(std::size_t row, std::size_t col, const numeric::ExactMatrix& block, const numeric::Rational& scale = numeric::Rational{1}) {
        if (row + block.rows() > matrix_.rows() || col + block.cols() > matrix_.cols()) throw std::out_of_range("block system insertion");
        for (const auto& [key,v] : block.entries()) {
            const auto old = matrix_.get(row+key.first,col+key.second);
            matrix_.set(row+key.first,col+key.second,old+scale*v);
        }
    }
    [[nodiscard]] const numeric::ExactMatrix& matrix() const noexcept { return matrix_; }
private:
    numeric::ExactMatrix matrix_;
};

struct AugmentedGreenResult {
    numeric::RationalVector value;
    bool exact{};
    std::size_t augmented_dimension{};
};

inline AugmentedGreenResult augmented_green_apply_reference(const core::RelationalComplex& complex,
                                                             core::Grade grade,
                                                             const std::vector<ExactMetric>& metrics,
                                                             numeric::Rational lambda,
                                                             const numeric::RationalVector& source) {
    if (grade.value >= complex.grade_count() || metrics.size()!=complex.grade_count()) throw std::invalid_argument("augmented Green grade/metric mismatch");
    const std::size_t n = complex.grade(grade).cell_count();
    if (source.size()!=n || metrics[grade.value].size()!=n) throw std::invalid_argument("augmented Green source size mismatch");
    const std::size_t l = grade.value > 0 ? complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value-1u)}).cell_count() : 0u;
    const std::size_t u = grade.value < complex.max_grade().value ? complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value+1u)}).cell_count() : 0u;
    const numeric::ExactMatrix lower = grade.value > 0 ? materialize_boundary_matrix(complex,grade) : numeric::ExactMatrix(0,n);
    const numeric::ExactMatrix upper = grade.value < complex.max_grade().value ? materialize_boundary_matrix(complex,core::Grade{static_cast<std::uint16_t>(grade.value+1u)}) : numeric::ExactMatrix(n,0);

    const std::vector<std::size_t> sizes{n,n,l,l,u,u,n,n};
    std::vector<std::size_t> off; off.reserve(sizes.size());
    std::size_t total=0; for (auto s:sizes){off.push_back(total); total+=s;}
    const auto ix=off[0], ia=off[1], ib=off[2], ic=off[3], id=off[4], ie=off[5], iff=off[6], ig=off[7];
    ExactBlockSystem block(total);
    std::size_t row=0;
    block.put(row,ia,numeric::ExactMatrix::identity(n)); block.put(row,ix,metrics[grade.value].materialize(),numeric::Rational{-1}); row+=n;
    if (l) {
        block.put(row,ib,numeric::ExactMatrix::identity(l)); block.put(row,ix,lower,numeric::Rational{-1}); row+=l;
        block.put(row,ic,numeric::ExactMatrix::identity(l)); block.put(row,ib,metrics[grade.value-1u].materialize(),numeric::Rational{-1}); row+=l;
    }
    if (u) {
        block.put(row,id,numeric::ExactMatrix::identity(u)); block.put(row,ia,upper.transpose(),numeric::Rational{-1}); row+=u;
        block.put(row,ie,metrics[grade.value+1u].materialize()); block.put(row,id,numeric::ExactMatrix::identity(u),numeric::Rational{-1}); row+=u;
        block.put(row,iff,numeric::ExactMatrix::identity(n)); block.put(row,ie,upper,numeric::Rational{-1}); row+=n;
        block.put(row,ig,numeric::ExactMatrix::identity(n)); block.put(row,iff,metrics[grade.value].materialize(),numeric::Rational{-1}); row+=n;
    } else {
        block.put(row,iff,numeric::ExactMatrix::identity(n)); row+=n;
        block.put(row,ig,numeric::ExactMatrix::identity(n)); row+=n;
    }
    block.put(row,ia,numeric::ExactMatrix::identity(n));
    if (l) block.put(row,ic,lower.transpose(),lambda);
    block.put(row,ig,numeric::ExactMatrix::identity(n),lambda);

    numeric::RationalVector rhs(total,numeric::Rational{0});
    const auto metric_source=metrics[grade.value].apply(source);
    for (std::size_t i=0;i<n;++i) rhs[row+i]=metric_source[i];
    const auto solution=block.matrix().solve(rhs);
    numeric::RationalVector x(n); for (std::size_t i=0;i<n;++i) x[i]=solution[ix+i];

    ExactHodgeOperator hodge(complex,grade,metrics);
    const auto residual=numeric::add(x,numeric::scale(lambda,hodge.apply(x)));
    return {std::move(x), residual==source, total};
}

// Compatibility spelling retained as the explicit augmented reference path.
inline AugmentedGreenResult augmented_green_apply(const core::RelationalComplex& complex,
                                                   core::Grade grade,
                                                   const std::vector<ExactMetric>& metrics,
                                                   numeric::Rational lambda,
                                                   const numeric::RationalVector& source) {
    return augmented_green_apply_reference(complex,grade,metrics,std::move(lambda),source);
}

} // namespace rfc::field
