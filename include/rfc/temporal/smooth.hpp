#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <stdexcept>

namespace rfc::temporal {

inline numeric::ExactMatrix first_order_boundary_defect(
    const numeric::ExactMatrix& boundary_dot,
    const numeric::ExactMatrix& boundary,
    const numeric::ExactMatrix& current_connection,
    const numeric::ExactMatrix& lower_connection) {
    if(boundary_dot.rows()!=boundary.rows() || boundary_dot.cols()!=boundary.cols() ||
       current_connection.rows()!=current_connection.cols() || current_connection.rows()!=boundary.cols() ||
       lower_connection.rows()!=lower_connection.cols() || lower_connection.rows()!=boundary.rows()) {
        throw std::invalid_argument("smooth boundary-defect dimension mismatch");
    }
    return boundary_dot.add(boundary.multiply(current_connection))
        .add(lower_connection.multiply(boundary).scale(numeric::Rational{-1}));
}

inline numeric::ExactMatrix metric_compatibility_residual(
    const numeric::ExactMatrix& metric_dot,
    const numeric::ExactMatrix& connection,
    const numeric::ExactMatrix& metric) {
    if(metric.rows()!=metric.cols() || metric_dot.rows()!=metric.rows() || metric_dot.cols()!=metric.cols() ||
       connection.rows()!=metric.rows() || connection.cols()!=metric.cols()) {
        throw std::invalid_argument("smooth metric-compatibility dimension mismatch");
    }
    return metric_dot.add(connection.transpose().multiply(metric)).add(metric.multiply(connection));
}

inline numeric::RationalVector covariant_field_derivative(
    const numeric::RationalVector& field_dot,
    const numeric::ExactMatrix& connection,
    const numeric::RationalVector& field) {
    if(connection.rows()!=connection.cols() || connection.cols()!=field.size() || field_dot.size()!=field.size()) {
        throw std::invalid_argument("covariant field derivative dimension mismatch");
    }
    return numeric::sub(field_dot,connection.apply(field));
}

inline numeric::ExactMatrix covariant_operator_derivative(
    const numeric::ExactMatrix& operator_dot,
    const numeric::ExactMatrix& op,
    const numeric::ExactMatrix& connection) {
    if(op.rows()!=op.cols() || operator_dot.rows()!=op.rows() || operator_dot.cols()!=op.cols() ||
       connection.rows()!=op.rows() || connection.cols()!=op.cols()) {
        throw std::invalid_argument("covariant operator derivative dimension mismatch");
    }
    return operator_dot.add(op.multiply(connection)).add(connection.multiply(op).scale(numeric::Rational{-1}));
}

} // namespace rfc::temporal
