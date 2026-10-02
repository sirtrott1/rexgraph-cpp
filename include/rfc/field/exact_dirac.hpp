#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/augmented_green.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::field {

// Exact state on the full graded carrier.  Grade entries are retained primary
// cell amplitudes; this object does not flatten the tower into one dense block.
struct ExactTowerField {
    std::vector<numeric::RationalVector> grades;
};

inline void require_exact_tower_shape(const core::RelationalComplex& complex,
                                      const ExactTowerField& field) {
    if (field.grades.size()!=complex.grade_count()) throw std::invalid_argument("exact tower size mismatch");
    for(std::size_t k=0;k<complex.grade_count();++k){
        const auto n=complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        if(field.grades[k].size()!=n) throw std::invalid_argument("exact tower field grade size mismatch");
    }
}

inline ExactTowerField zero_exact_tower(const core::RelationalComplex& complex) {
    ExactTowerField out;
    out.grades.reserve(complex.grade_count());
    for(std::size_t k=0;k<complex.grade_count();++k){
        out.grades.emplace_back(complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(),
                                numeric::Rational{0});
    }
    return out;
}

inline ExactTowerField exact_dirac_apply(const core::RelationalComplex& complex,
                                         const std::vector<ExactMetric>& metrics,
                                         const ExactTowerField& field) {
    if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("exact Dirac metric count mismatch");
    require_exact_tower_shape(complex,field);
    auto out=zero_exact_tower(complex);
    for(std::size_t k=1;k<complex.grade_count();++k){
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        const BoundaryOperator B(complex,grade);
        out.grades[k-1u]=numeric::add(out.grades[k-1u],B.apply(field.grades[k]));
        out.grades[k]=numeric::add(out.grades[k],metric_adjoint_apply(B,metrics[k],metrics[k-1u],field.grades[k-1u]));
    }
    return out;
}

// D_a = partial - partial^dagger.  The lowering channel has the same sign as
// Dirac; the raising channel has the opposite sign.  No tower matrix is built.
inline ExactTowerField exact_antidirac_apply(const core::RelationalComplex& complex,
                                             const std::vector<ExactMetric>& metrics,
                                             const ExactTowerField& field) {
    if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("exact anti-Dirac metric count mismatch");
    require_exact_tower_shape(complex,field);
    auto out=zero_exact_tower(complex);
    for(std::size_t k=1;k<complex.grade_count();++k){
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        const BoundaryOperator B(complex,grade);
        out.grades[k-1u]=numeric::add(out.grades[k-1u],B.apply(field.grades[k]));
        out.grades[k]=numeric::sub(out.grades[k],metric_adjoint_apply(B,metrics[k],metrics[k-1u],field.grades[k-1u]));
    }
    return out;
}

inline ExactTowerField exact_hodge_tower_apply(const core::RelationalComplex& complex,
                                               const std::vector<ExactMetric>& metrics,
                                               const ExactTowerField& field) {
    if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("exact Hodge metric count mismatch");
    require_exact_tower_shape(complex,field);
    ExactTowerField out;
    out.grades.reserve(complex.grade_count());
    for(std::size_t k=0;k<complex.grade_count();++k){
        ExactHodgeOperator L(complex,core::Grade{static_cast<std::uint16_t>(k)},metrics);
        out.grades.push_back(L.apply(field.grades[k]));
    }
    return out;
}

// K_k = L_k^down - L_k^up = 1/2 [D,D_a] on one grade.
inline numeric::RationalVector exact_hodge_orientation_apply(const ExactHodgeOperator& hodge,
                                                             const numeric::RationalVector& field) {
    return numeric::sub(hodge.apply_down(field),hodge.apply_up(field));
}

inline ExactTowerField exact_hodge_orientation_tower_apply(const core::RelationalComplex& complex,
                                                            const std::vector<ExactMetric>& metrics,
                                                            const ExactTowerField& field) {
    if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("exact orientation metric count mismatch");
    require_exact_tower_shape(complex,field);
    ExactTowerField out;
    out.grades.reserve(complex.grade_count());
    for(std::size_t k=0;k<complex.grade_count();++k){
        ExactHodgeOperator L(complex,core::Grade{static_cast<std::uint16_t>(k)},metrics);
        out.grades.push_back(exact_hodge_orientation_apply(L,field.grades[k]));
    }
    return out;
}

inline ExactTowerField exact_tower_add(const ExactTowerField& lhs,
                                       const ExactTowerField& rhs) {
    if(lhs.grades.size()!=rhs.grades.size()) throw std::invalid_argument("tower add grade mismatch");
    ExactTowerField out;
    out.grades.reserve(lhs.grades.size());
    for(std::size_t k=0;k<lhs.grades.size();++k) out.grades.push_back(numeric::add(lhs.grades[k],rhs.grades[k]));
    return out;
}

inline ExactTowerField exact_tower_sub(const ExactTowerField& lhs,
                                       const ExactTowerField& rhs) {
    if(lhs.grades.size()!=rhs.grades.size()) throw std::invalid_argument("tower subtract grade mismatch");
    ExactTowerField out;
    out.grades.reserve(lhs.grades.size());
    for(std::size_t k=0;k<lhs.grades.size();++k) out.grades.push_back(numeric::sub(lhs.grades[k],rhs.grades[k]));
    return out;
}

inline ExactTowerField exact_tower_scale(const numeric::Rational& scalar,
                                         const ExactTowerField& field) {
    ExactTowerField out;
    out.grades.reserve(field.grades.size());
    for(const auto& grade:field.grades) out.grades.push_back(numeric::scale(scalar,grade));
    return out;
}

inline numeric::Rational exact_tower_inner(const std::vector<ExactMetric>& metrics,
                                           const ExactTowerField& lhs,
                                           const ExactTowerField& rhs) {
    if(lhs.grades.size()!=rhs.grades.size() || metrics.size()!=lhs.grades.size()) {
        throw std::invalid_argument("tower inner grade mismatch");
    }
    numeric::Rational value=0;
    for(std::size_t k=0;k<metrics.size();++k) value+=metrics[k].inner(lhs.grades[k],rhs.grades[k]);
    value.canonicalize();
    return value;
}

// Exact algebra certificates for the paper identities.  They remain action
// paths: neither D nor D_a nor L is materialized as a full tower matrix.
inline ExactTowerField exact_dirac_square_apply(const core::RelationalComplex& complex,
                                                const std::vector<ExactMetric>& metrics,
                                                const ExactTowerField& field) {
    return exact_dirac_apply(complex,metrics,exact_dirac_apply(complex,metrics,field));
}

inline ExactTowerField exact_antidirac_square_apply(const core::RelationalComplex& complex,
                                                    const std::vector<ExactMetric>& metrics,
                                                    const ExactTowerField& field) {
    return exact_antidirac_apply(complex,metrics,exact_antidirac_apply(complex,metrics,field));
}

inline ExactTowerField exact_dirac_antidirac_anticommutator_apply(
    const core::RelationalComplex& complex,
    const std::vector<ExactMetric>& metrics,
    const ExactTowerField& field) {
    const auto d_da=exact_dirac_apply(complex,metrics,exact_antidirac_apply(complex,metrics,field));
    const auto da_d=exact_antidirac_apply(complex,metrics,exact_dirac_apply(complex,metrics,field));
    return exact_tower_add(d_da,da_d);
}

inline ExactTowerField exact_dirac_antidirac_commutator_half_apply(
    const core::RelationalComplex& complex,
    const std::vector<ExactMetric>& metrics,
    const ExactTowerField& field) {
    const auto d_da=exact_dirac_apply(complex,metrics,exact_antidirac_apply(complex,metrics,field));
    const auto da_d=exact_antidirac_apply(complex,metrics,exact_dirac_apply(complex,metrics,field));
    return exact_tower_scale(numeric::Rational{1,2},exact_tower_sub(d_da,da_d));
}

// Rational anti-Dirac Cayley variant from the paper. With
// G_t=(I+t^2 L)^{-1}, U_a(t)x=(2G_t-I)x+2t D_a G_t x.
// This remains over Q and uses one Green action per grade plus one anti-Dirac
// action; the optional iD_a complex Cayley extension is deliberately separate.
inline ExactTowerField exact_antidirac_cayley_apply(
    const core::RelationalComplex& complex,
    const std::vector<ExactMetric>& metrics,
    const numeric::Rational& t,
    const ExactTowerField& field) {
    require_exact_tower_shape(complex,field);
    if(metrics.size()!=complex.grade_count()) throw std::invalid_argument("anti-Dirac Cayley metric count mismatch");
    if(t==0) return field;
    ExactTowerField y; y.grades.reserve(complex.grade_count());
    const numeric::Rational lambda=t*t;
    for(std::size_t k=0;k<complex.grade_count();++k){
        ExactHodgeOperator hodge(complex,core::Grade{static_cast<std::uint16_t>(k)},metrics);
        y.grades.push_back(exact_green_apply(hodge,lambda,field.grades[k]).value);
    }
    const auto day=exact_antidirac_apply(complex,metrics,y);
    return exact_tower_add(exact_tower_sub(exact_tower_scale(numeric::Rational{2},y),field),
                           exact_tower_scale(numeric::Rational{2}*t,day));
}

// Explicit Q(i) extension over retained real rational positive grade metrics:
// both rational components retain the full tower.
// Real tower operations never convert to this domain implicitly.
struct ExactComplexTowerField {
    ExactTowerField real,imaginary;
};

inline ExactComplexTowerField exact_dirac_cayley_apply(
    const core::RelationalComplex& complex,const std::vector<ExactMetric>& metrics,
    const numeric::Rational& t,const ExactComplexTowerField& input) {
    require_exact_tower_shape(complex,input.real);require_exact_tower_shape(complex,input.imaginary);
    if(metrics.size()!=complex.grade_count())throw std::invalid_argument("complex Cayley metric count mismatch");
    if(t==0)return input;
    ExactComplexTowerField green;
    for(std::size_t k=0;k<complex.grade_count();++k) {
        const ExactHodgeOperator hodge(complex,core::Grade{static_cast<std::uint16_t>(k)},metrics);
        const auto solve=[&](const numeric::RationalVector& x){
            const auto result=exact_green_apply(hodge,t*t,x);
            if(!result.exact)throw std::runtime_error("complex Cayley Green residual is nonzero");
            return result.value;
        };
        green.real.grades.push_back(solve(input.real.grades[k]));
        green.imaginary.grades.push_back(solve(input.imaginary.grades[k]));
    }
    const auto real_dirac=exact_dirac_apply(complex,metrics,green.real);
    const auto imaginary_dirac=exact_dirac_apply(complex,metrics,green.imaginary);
    return {exact_tower_sub(exact_tower_sub(exact_tower_scale(numeric::Rational{2},green.real),input.real),
                            exact_tower_scale(numeric::Rational{2}*t,imaginary_dirac)),
            exact_tower_add(exact_tower_sub(exact_tower_scale(numeric::Rational{2},green.imaginary),input.imaginary),
                            exact_tower_scale(numeric::Rational{2}*t,real_dirac))};
}

inline numeric::Rational exact_complex_tower_quadrance(const std::vector<ExactMetric>& metrics,
    const ExactComplexTowerField& input) {
    return exact_tower_inner(metrics,input.real,input.real)+exact_tower_inner(metrics,input.imaginary,input.imaginary);
}

} // namespace rfc::field
