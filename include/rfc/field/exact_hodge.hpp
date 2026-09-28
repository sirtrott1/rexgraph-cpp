#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/field/boundary.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

// Explicit coordinate materialization for diagnostics, exact elimination, and
// small reference calculations. Boundary action does not require this matrix.
inline numeric::ExactMatrix materialize_boundary_matrix(const core::RelationalComplex& complex,
                                                         core::Grade upper_grade) {
    if (upper_grade.value == 0 || upper_grade.value >= complex.grade_count()) throw std::out_of_range("invalid boundary grade");
    const auto& upper = complex.grade(upper_grade);
    const auto& lower = complex.grade(core::Grade{static_cast<std::uint16_t>(upper_grade.value - 1u)});
    numeric::ExactMatrix B(lower.cell_count(), upper.cell_count());
    const auto exists = complex.signature().existence_aspect();
    for (std::size_t cell = 0; cell < upper.cell_count(); ++cell) {
        const auto [begin,end] = upper.incidence_range(cell);
        for (std::size_t p = begin; p < end; ++p) {
            if (!upper.aspects.get(exists,p)) continue;
            // Distinct retained occurrences may share a matrix coordinate.
            // Their contributions add; cancellation must not lose multiplicity.
            const auto row = upper.lower_index[p];
            B.set(row, cell, B.get(row, cell) + complex.incidence_coefficient(upper_grade,p));
        }
    }
    return B;
}

// Compatibility spelling. New production code should prefer BoundaryOperator.
inline numeric::ExactMatrix boundary_matrix(const core::RelationalComplex& complex, core::Grade upper_grade) {
    return materialize_boundary_matrix(complex, upper_grade);
}

inline numeric::ExactMatrix solve_columns(const numeric::ExactMatrix& A, const numeric::ExactMatrix& rhs) {
    if (A.rows() != A.cols() || A.rows() != rhs.rows()) throw std::invalid_argument("solve_columns dimension mismatch");
    numeric::ExactMatrix out(A.cols(), rhs.cols());
    for (std::size_t j = 0; j < rhs.cols(); ++j) {
        numeric::RationalVector column(rhs.rows(), numeric::Rational{0});
        for (std::size_t i = 0; i < rhs.rows(); ++i) column[i] = rhs.get(i,j);
        const auto solved = A.solve(column);
        for (std::size_t i = 0; i < solved.size(); ++i) if (solved[i] != 0) out.set(i,j,solved[i]);
    }
    return out;
}

inline numeric::ExactMatrix metric_adjoint_matrix(const numeric::ExactMatrix& boundary,
                                                   const ExactMetric& source_metric,
                                                   const ExactMetric& target_metric) {
    if (boundary.cols() != source_metric.size() || boundary.rows() != target_metric.size()) {
        throw std::invalid_argument("metric adjoint dimensions do not match boundary");
    }
    const auto rhs = boundary.transpose().multiply(target_metric.materialize());
    return solve_columns(source_metric.materialize(), rhs);
}

inline numeric::RationalVector metric_adjoint_apply(const numeric::ExactMatrix& boundary,
                                                     const ExactMetric& source_metric,
                                                     const ExactMetric& target_metric,
                                                     const numeric::RationalVector& y) {
    if (y.size() != boundary.rows()) throw std::invalid_argument("metric adjoint apply vector size mismatch");
    return source_metric.solve(boundary.transpose().apply(target_metric.apply(y)));
}

inline numeric::RationalVector metric_adjoint_apply(const BoundaryOperator& boundary,
                                                     const ExactMetric& source_metric,
                                                     const ExactMetric& target_metric,
                                                     const numeric::RationalVector& y) {
    if (source_metric.size() != boundary.domain_size() || target_metric.size() != boundary.codomain_size() || y.size() != boundary.codomain_size()) {
        throw std::invalid_argument("operator metric adjoint dimension mismatch");
    }
    return boundary.adjoint(y,target_metric,source_metric);
}

inline numeric::SparseRationalVector metric_adjoint_apply_sparse(const BoundaryOperator& boundary,
                                                                 const ExactMetric& source_metric,
                                                                 const ExactMetric& target_metric,
                                                                 numeric::SparseRationalView y) {
    if (!source_metric.supports_sparse_solve() || !target_metric.is_diagonal()) {
        throw std::logic_error("sparse metric adjoint requires identity/diagonal metrics");
    }
    if (source_metric.size() != boundary.domain_size() || target_metric.size() != boundary.codomain_size() || y.size() != boundary.codomain_size()) {
        throw std::invalid_argument("sparse operator metric adjoint dimension mismatch");
    }
    return boundary.adjoint(y,target_metric,source_metric);
}

inline numeric::RationalVector matrix_column(const numeric::ExactMatrix& frame, std::size_t column) {
    if (column >= frame.cols()) throw std::out_of_range("frame column");
    numeric::RationalVector out(frame.rows(), numeric::Rational{0});
    for (std::size_t i = 0; i < frame.rows(); ++i) out[i] = frame.get(i,column);
    return out;
}

inline numeric::ExactMatrix metric_gram(const numeric::ExactMatrix& lhs,
                                        const ExactMetric& metric,
                                        const numeric::ExactMatrix& rhs) {
    if (lhs.rows() != metric.size() || rhs.rows() != metric.size()) throw std::invalid_argument("metric Gram row mismatch");
    numeric::ExactMatrix out(lhs.cols(), rhs.cols());
    std::vector<numeric::RationalVector> right_columns;
    right_columns.reserve(rhs.cols());
    std::vector<numeric::RationalVector> measured_right;
    measured_right.reserve(rhs.cols());
    for (std::size_t j = 0; j < rhs.cols(); ++j) {
        right_columns.push_back(matrix_column(rhs,j));
        measured_right.push_back(metric.apply(right_columns.back()));
    }
    for (std::size_t i = 0; i < lhs.cols(); ++i) {
        const auto left = matrix_column(lhs,i);
        for (std::size_t j = 0; j < rhs.cols(); ++j) {
            const auto value = numeric::dot(left, measured_right[j]);
            if (value != 0) out.set(i,j,value);
        }
    }
    return out;
}

inline numeric::RationalVector metric_frame_dual_apply(const numeric::ExactMatrix& frame,
                                                        const ExactMetric& metric,
                                                        const numeric::RationalVector& x) {
    if (frame.rows() != metric.size() || x.size() != metric.size()) throw std::invalid_argument("metric frame dual size mismatch");
    const auto measured = metric.apply(x);
    return frame.transpose().apply(measured);
}

class ExactHodgeOperator {
public:
    ExactHodgeOperator(const core::RelationalComplex& complex,
                       core::Grade grade,
                       const std::vector<ExactMetric>& metrics)
        : complex_(&complex), grade_(grade), metrics_(&metrics) {
        if (grade.value >= complex.grade_count()) throw std::out_of_range("Hodge grade outside complex");
        if (metrics.size() != complex.grade_count()) throw std::invalid_argument("one exact metric required per grade");
        for (std::size_t k = 0; k < metrics.size(); ++k) {
            if (metrics[k].size() != complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()) {
                throw std::invalid_argument("exact metric grade size mismatch");
            }
        }
        if (grade_.value > 0) lower_boundary_.emplace(complex, grade_);
        if (grade_.value < complex.max_grade().value) {
            upper_boundary_.emplace(complex, core::Grade{static_cast<std::uint16_t>(grade_.value + 1u)});
        }
    }

    [[nodiscard]] std::size_t size() const { return complex_->grade(grade_).cell_count(); }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const ExactMetric& metric() const { return metrics_->at(grade_.value); }
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }

    [[nodiscard]] numeric::RationalVector apply_down(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("Hodge down size mismatch");
        if (!lower_boundary_) return numeric::RationalVector(size(), numeric::Rational{0});
        const auto bx = lower_boundary_->apply(x);
        return metric_adjoint_apply(*lower_boundary_, metric(), metrics_->at(grade_.value-1u), bx);
    }

    [[nodiscard]] numeric::RationalVector apply_up(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("Hodge up size mismatch");
        if (!upper_boundary_) return numeric::RationalVector(size(), numeric::Rational{0});
        const auto adj = metric_adjoint_apply(*upper_boundary_, metrics_->at(grade_.value+1u), metric(), x);
        return upper_boundary_->apply(adj);
    }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const {
        return numeric::add(apply_down(x), apply_up(x));
    }

    [[nodiscard]] bool supports_sparse_action() const noexcept {
        if (!metric().is_diagonal()) return false;
        if (lower_boundary_ && !metrics_->at(grade_.value-1u).is_diagonal()) return false;
        if (upper_boundary_ && !metrics_->at(grade_.value+1u).is_diagonal()) return false;
        return true;
    }

    [[nodiscard]] numeric::SparseRationalVector apply_down(numeric::SparseRationalView x) const {
        if (x.size() != size()) throw std::invalid_argument("sparse Hodge down size mismatch");
        if (!supports_sparse_action()) throw std::logic_error("sparse Hodge action requires identity/diagonal adjacent metrics");
        if (!lower_boundary_) return numeric::SparseRationalVector(size());
        const auto bx = lower_boundary_->apply(x);
        return metric_adjoint_apply_sparse(*lower_boundary_, metric(), metrics_->at(grade_.value-1u), bx.view());
    }

    [[nodiscard]] numeric::SparseRationalVector apply_up(numeric::SparseRationalView x) const {
        if (x.size() != size()) throw std::invalid_argument("sparse Hodge up size mismatch");
        if (!supports_sparse_action()) throw std::logic_error("sparse Hodge action requires identity/diagonal adjacent metrics");
        if (!upper_boundary_) return numeric::SparseRationalVector(size());
        const auto adj = metric_adjoint_apply_sparse(*upper_boundary_, metrics_->at(grade_.value+1u), metric(), x);
        return upper_boundary_->apply(adj.view());
    }

    [[nodiscard]] numeric::SparseRationalVector apply(numeric::SparseRationalView x) const {
        const auto down = apply_down(x);
        const auto up = apply_up(x);
        std::vector<numeric::SparseRationalEntry> entries;
        entries.reserve(down.nonzeros()+up.nonzeros());
        entries.insert(entries.end(), down.entries().begin(), down.entries().end());
        entries.insert(entries.end(), up.entries().begin(), up.entries().end());
        return numeric::SparseRationalVector(size(), std::move(entries));
    }

    // Explicit materialization for diagnostics/reference cross-checks.
    [[nodiscard]] numeric::ExactMatrix materialize() const {
        numeric::ExactMatrix out(size(), size());
        for (std::size_t j = 0; j < size(); ++j) {
            numeric::RationalVector e(size(), numeric::Rational{0}); e[j] = 1;
            const auto y = apply(e);
            for (std::size_t i = 0; i < size(); ++i) if (y[i] != 0) out.set(i,j,y[i]);
        }
        return out;
    }

    [[nodiscard]] numeric::ExactMatrix action_matrix() const { return materialize(); }

    [[nodiscard]] numeric::ExactMatrix gradient_frame() const {
        if (!lower_boundary_) return numeric::ExactMatrix(size(), 0);
        const auto B = materialize_boundary_matrix(*complex_, grade_);
        return metric_adjoint_matrix(B, metric(), metrics_->at(grade_.value-1u)).image_frame();
    }

    [[nodiscard]] numeric::ExactMatrix curl_frame() const {
        if (!upper_boundary_) return numeric::ExactMatrix(size(), 0);
        return materialize_boundary_matrix(*complex_, upper_boundary_->upper_grade()).image_frame();
    }

    [[nodiscard]] numeric::ExactMatrix harmonic_frame() const {
        numeric::ExactMatrix constraints(0, size());
        bool initialized = false;
        if (lower_boundary_) {
            constraints = materialize_boundary_matrix(*complex_, grade_);
            initialized = true;
        }
        if (upper_boundary_) {
            const auto U = materialize_boundary_matrix(*complex_, upper_boundary_->upper_grade());
            const auto upper_constraint = U.transpose().multiply(metric().materialize());
            constraints = initialized ? numeric::ExactMatrix::vertical_concat(constraints, upper_constraint) : upper_constraint;
            initialized = true;
        }
        if (!initialized) return numeric::ExactMatrix::identity(size());
        return constraints.kernel_frame();
    }

private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    const std::vector<ExactMetric>* metrics_{};
    std::optional<BoundaryOperator> lower_boundary_;
    std::optional<BoundaryOperator> upper_boundary_;
};

inline numeric::RationalVector projector_apply(const numeric::ExactMatrix& frame,
                                                const ExactMetric& metric,
                                                const numeric::RationalVector& x) {
    if (frame.rows() != metric.size() || x.size() != metric.size()) throw std::invalid_argument("projector dimension mismatch");
    if (frame.cols() == 0) return numeric::RationalVector(metric.size(), numeric::Rational{0});
    const auto gram = metric_gram(frame, metric, frame);
    const auto rhs = metric_frame_dual_apply(frame, metric, x);
    const auto coeff = gram.solve(rhs);
    return frame.apply(coeff);
}

struct HodgeProjectors {
    numeric::ExactMatrix gradient;
    numeric::ExactMatrix curl;
    numeric::ExactMatrix harmonic;
    ExactMetric metric;

    [[nodiscard]] numeric::RationalVector project_gradient(const numeric::RationalVector& x) const { return projector_apply(gradient,metric,x); }
    [[nodiscard]] numeric::RationalVector project_curl(const numeric::RationalVector& x) const { return projector_apply(curl,metric,x); }
    [[nodiscard]] numeric::RationalVector project_harmonic(const numeric::RationalVector& x) const { return projector_apply(harmonic,metric,x); }
};

inline HodgeProjectors exact_hodge_projectors(const ExactHodgeOperator& hodge) {
    return {hodge.gradient_frame(), hodge.curl_frame(), hodge.harmonic_frame(), hodge.metric()};
}

inline numeric::ExactMatrix materialize_action_matrix(const ExactMetric& metric,
                                                       const auto& action) {
    numeric::ExactMatrix out(metric.size(), metric.size());
    for (std::size_t j = 0; j < metric.size(); ++j) {
        numeric::RationalVector e(metric.size(), numeric::Rational{0}); e[j]=1;
        const auto y = action(e);
        for (std::size_t i = 0; i < y.size(); ++i) if (y[i] != 0) out.set(i,j,y[i]);
    }
    return out;
}

inline numeric::ExactMatrix action_matrix_from(const ExactMetric& metric, const auto& action) {
    return materialize_action_matrix(metric, action);
}

inline numeric::ExactMatrix harmonic_projector_matrix(const HodgeProjectors& projectors) {
    return materialize_action_matrix(projectors.metric, [&](const numeric::RationalVector& x){ return projectors.project_harmonic(x); });
}

inline numeric::ExactMatrix hodge_nonharmonic_frame(const ExactHodgeOperator& hodge) {
    const auto gradient = hodge.gradient_frame();
    const auto curl = hodge.curl_frame();
    return numeric::ExactMatrix::horizontal_concat(gradient,curl);
}

inline numeric::ExactMatrix restricted_hodge_stiffness(const ExactHodgeOperator& hodge,
                                                        const numeric::ExactMatrix& frame) {
    if (frame.rows() != hodge.size()) throw std::invalid_argument("restricted Hodge frame size mismatch");
    numeric::ExactMatrix out(frame.cols(), frame.cols());
    std::vector<numeric::RationalVector> columns;
    columns.reserve(frame.cols());
    std::vector<numeric::RationalVector> applied;
    applied.reserve(frame.cols());
    for (std::size_t j = 0; j < frame.cols(); ++j) {
        columns.push_back(matrix_column(frame,j));
        applied.push_back(hodge.apply(columns.back()));
    }
    for (std::size_t i = 0; i < frame.cols(); ++i) {
        for (std::size_t j = 0; j < frame.cols(); ++j) {
            const auto value = hodge.metric().inner(columns[i], applied[j]);
            if (value != 0) out.set(i,j,value);
        }
    }
    return out;
}

inline numeric::Rational pseudodeterminant_via_harmonic_completion(const ExactHodgeOperator& hodge) {
    const auto frame = hodge_nonharmonic_frame(hodge);
    if (frame.cols() == 0) return numeric::Rational{1};
    const auto gram = metric_gram(frame,hodge.metric(),frame);
    const auto stiffness = restricted_hodge_stiffness(hodge,frame);
    numeric::Rational result = stiffness.determinant() / gram.determinant();
    result.canonicalize();
    return result;
}

struct VolumeFactorizationResult {
    numeric::ExactMatrix harmonic_frame;
    numeric::Rational filled_volume;
    numeric::Rational harmonic_volume;
    numeric::Rational closed_volume;
    numeric::Rational coordinate_volume;
    bool exact_identity{};
};

inline VolumeFactorizationResult metric_volume_factorization(const numeric::ExactMatrix& Z,
                                                              const numeric::ExactMatrix& F,
                                                              const numeric::ExactMatrix& W,
                                                              const numeric::ExactMatrix& R,
                                                              const ExactMetric& metric) {
    if (Z.rows()!=metric.size() || F.rows()!=metric.size() || W.rows()!=metric.size()) throw std::invalid_argument("volume frame row mismatch");
    if (F.cols()+W.cols()!=Z.cols() || R.rows()!=Z.cols() || R.cols()!=Z.cols()) throw std::invalid_argument("volume coordinate dimension mismatch");
    const auto FW = numeric::ExactMatrix::horizontal_concat(F,W);
    if (Z.multiply(R).entries() != FW.entries()) throw std::invalid_argument("[F W] must equal Z R exactly");

    const auto gramF = metric_gram(F,metric,F);
    const auto fMw = metric_gram(F,metric,W);
    numeric::ExactMatrix H = W;
    if (F.cols() != 0) {
        const auto A = solve_columns(gramF, fMw);
        H = W.add(F.multiply(A).scale(numeric::Rational{-1}));
    }
    const auto gramH = metric_gram(H,metric,H);
    const auto gramZ = metric_gram(Z,metric,Z);
    const auto detR = R.determinant();
    const auto filled = gramF.determinant();
    const auto harmonic = gramH.determinant();
    const auto closed = gramZ.determinant();
    numeric::Rational coordinate = detR * detR;
    return {std::move(H), filled, harmonic, closed, coordinate, filled*harmonic == coordinate*closed};
}

inline numeric::ExactMatrix harmonic_supported_operator(const numeric::ExactMatrix& H,
                                                         const numeric::ExactMatrix& sigma,
                                                         const ExactMetric& metric) {
    if (H.rows()!=metric.size() || sigma.rows()!=H.cols() || sigma.cols()!=H.cols()) throw std::invalid_argument("harmonic supported operator dimensions");
    return H.multiply(sigma).multiply(H.transpose()).multiply(metric.materialize());
}

inline numeric::Rational determinant_harmonic_completion(const ExactHodgeOperator& hodge,
                                                          const numeric::ExactMatrix& sigma) {
    const auto H = hodge.harmonic_frame();
    if (sigma.rows()!=H.cols() || sigma.cols()!=H.cols()) throw std::invalid_argument("harmonic completion sigma size mismatch");
    const auto GH = metric_gram(H,hodge.metric(),H);
    return pseudodeterminant_via_harmonic_completion(hodge) * GH.determinant() * sigma.determinant();
}

inline numeric::ExactMatrix inverse_by_solves(const numeric::ExactMatrix& A) {
    return solve_columns(A, numeric::ExactMatrix::identity(A.rows()));
}

inline numeric::Rational determinant_projector_completion(const ExactHodgeOperator& hodge) {
    return pseudodeterminant_via_harmonic_completion(hodge);
}

// Explicit reference materialization of L + Pi_h.
inline numeric::ExactMatrix harmonic_completion_matrix(const ExactHodgeOperator& hodge) {
    const auto projectors=exact_hodge_projectors(hodge);
    return hodge.materialize().add(harmonic_projector_matrix(projectors));
}

// Operator-first exact solve of (L + Pi_h)x = rhs. Only the nonharmonic
// restricted stiffness matrix is factored; no full Hodge matrix is required.
inline numeric::RationalVector harmonic_completion_solve(const ExactHodgeOperator& hodge,
                                                          const numeric::RationalVector& rhs) {
    if (rhs.size()!=hodge.size()) throw std::invalid_argument("harmonic completion rhs size mismatch");
    const auto projectors=exact_hodge_projectors(hodge);
    const auto harmonic = projectors.project_harmonic(rhs);
    const auto frame = numeric::ExactMatrix::horizontal_concat(projectors.gradient,projectors.curl);
    if (frame.cols()==0) return harmonic;
    const auto stiffness = restricted_hodge_stiffness(hodge,frame);
    const auto reduced_rhs = metric_frame_dual_apply(frame,hodge.metric(),rhs);
    const auto coeff = stiffness.solve(reduced_rhs);
    return numeric::add(frame.apply(coeff),harmonic);
}

inline numeric::RationalVector generalized_hodge_inverse_apply(const ExactHodgeOperator& hodge,
                                                                const numeric::RationalVector& rhs) {
    const auto projectors=exact_hodge_projectors(hodge);
    return numeric::sub(harmonic_completion_solve(hodge,rhs),projectors.project_harmonic(rhs));
}

struct CompleteFieldCoordinates {
    numeric::RationalVector lower_boundary;
    numeric::RationalVector upper_adjoint;
    numeric::RationalVector harmonic;
};

inline CompleteFieldCoordinates complete_field_coordinates(const core::RelationalComplex& complex,
                                                            core::Grade grade,
                                                            const std::vector<ExactMetric>& metrics,
                                                            const numeric::RationalVector& x) {
    ExactHodgeOperator hodge(complex,grade,metrics);
    const auto projectors=exact_hodge_projectors(hodge);
    numeric::RationalVector lower;
    if (grade.value>0) {
        const BoundaryOperator B(complex,grade);
        lower=B.apply(x);
    }
    numeric::RationalVector upper;
    if (grade.value<complex.max_grade().value) {
        const auto ug=core::Grade{static_cast<std::uint16_t>(grade.value+1u)};
        const BoundaryOperator U(complex,ug);
        upper=metric_adjoint_apply(U,metrics[ug.value],metrics[grade.value],x);
    }
    return {std::move(lower),std::move(upper),projectors.project_harmonic(x)};
}

inline numeric::RationalVector reconstruct_complete_field(const core::RelationalComplex& complex,
                                                           core::Grade grade,
                                                           const std::vector<ExactMetric>& metrics,
                                                           const CompleteFieldCoordinates& coordinates) {
    ExactHodgeOperator hodge(complex,grade,metrics);
    numeric::RationalVector rhs(hodge.size(),numeric::Rational{0});
    if (grade.value>0) {
        const BoundaryOperator B(complex,grade);
        if (coordinates.lower_boundary.size()!=B.codomain_size()) throw std::invalid_argument("complete coordinate lower size mismatch");
        rhs=numeric::add(rhs,metric_adjoint_apply(B,metrics[grade.value],metrics[grade.value-1u],coordinates.lower_boundary));
    } else if (!coordinates.lower_boundary.empty()) throw std::invalid_argument("grade zero has no lower boundary coordinates");
    if (grade.value<complex.max_grade().value) {
        const BoundaryOperator U(complex,core::Grade{static_cast<std::uint16_t>(grade.value+1u)});
        if (coordinates.upper_adjoint.size()!=U.domain_size()) throw std::invalid_argument("complete coordinate upper size mismatch");
        rhs=numeric::add(rhs,U.apply(coordinates.upper_adjoint));
    } else if (!coordinates.upper_adjoint.empty()) throw std::invalid_argument("top grade has no upper adjoint coordinates");
    if (coordinates.harmonic.size()!=hodge.size()) throw std::invalid_argument("complete coordinate harmonic size mismatch");
    rhs=numeric::add(rhs,coordinates.harmonic);
    return harmonic_completion_solve(hodge,rhs);
}

inline numeric::ExactMatrix subspace_join_frame(const numeric::ExactMatrix& F,
                                                 const numeric::ExactMatrix& G) {
    if (F.rows()!=G.rows()) throw std::invalid_argument("subspace join row mismatch");
    return numeric::ExactMatrix::horizontal_concat(F,G).image_frame();
}

inline numeric::ExactMatrix subspace_intersection_frame(const numeric::ExactMatrix& F,
                                                         const numeric::ExactMatrix& G) {
    if (F.rows()!=G.rows()) throw std::invalid_argument("subspace intersection row mismatch");
    const auto FG=numeric::ExactMatrix::horizontal_concat(F,G.scale(numeric::Rational{-1}));
    const auto kernel=FG.kernel_frame();
    if (kernel.cols()==0) return numeric::ExactMatrix(F.rows(),0);
    numeric::ExactMatrix common(F.rows(),kernel.cols());
    for (std::size_t a=0;a<kernel.cols();++a) {
        numeric::RationalVector coeff(F.cols(),numeric::Rational{0});
        for (std::size_t i=0;i<F.cols();++i) coeff[i]=kernel.get(i,a);
        const auto v=F.apply(coeff);
        for (std::size_t i=0;i<v.size();++i) if(v[i]!=0) common.set(i,a,v[i]);
    }
    return common.image_frame();
}

} // namespace rfc::field
