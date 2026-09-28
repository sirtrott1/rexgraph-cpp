#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/temporal/change.hpp"
#include "rfc/temporal/operation_word.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::temporal {

struct FieldTransferDelta {
    numeric::RationalVector transported_old;
    numeric::RationalVector innovation;
    numeric::RationalVector delta;
};

inline FieldTransferDelta field_delta(const numeric::ExactMatrix& correspondence,
                                      const numeric::RationalVector& old_field,
                                      const numeric::RationalVector& new_field) {
    const auto transported = correspondence.apply(old_field);
    if (transported.size() != new_field.size()) throw std::invalid_argument("field transfer size mismatch");
    return {transported, numeric::sub(new_field, transported), numeric::sub(new_field, transported)};
}

struct SourceInjectionDelta {
    numeric::ExactMatrix realization_defect;
    numeric::RationalVector coordinate_innovation;
    numeric::RationalVector representation_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector aggregate;
};

inline SourceInjectionDelta source_injection_delta(const numeric::ExactMatrix& old_injection,
                                                   const numeric::ExactMatrix& new_injection,
                                                   const numeric::ExactMatrix& ambient_correspondence,
                                                   const numeric::ExactMatrix& source_correspondence,
                                                   const numeric::RationalVector& old_coordinates,
                                                   const numeric::RationalVector& new_coordinates) {
    if (old_injection.cols() != old_coordinates.size() || new_injection.cols() != new_coordinates.size()) throw std::invalid_argument("injection coordinate size mismatch");
    const auto EI = new_injection.multiply(source_correspondence)
        .add(ambient_correspondence.multiply(old_injection).scale(numeric::Rational{-1}));
    const auto zeta = numeric::sub(new_coordinates, source_correspondence.apply(old_coordinates));
    const auto representation = EI.apply(old_coordinates);
    const auto innovation = new_injection.apply(zeta);
    return {EI, zeta, representation, innovation, numeric::add(representation, innovation)};
}

struct ObservationDelta {
    numeric::ExactMatrix accession_defect;
    numeric::RationalVector accession_term;
    numeric::RationalVector field_term;
    numeric::RationalVector aggregate;
};

inline ObservationDelta observation_delta(const numeric::ExactMatrix& old_accession,
                                          const numeric::ExactMatrix& new_accession,
                                          const numeric::ExactMatrix& ambient_correspondence,
                                          const numeric::ExactMatrix& output_correspondence,
                                          const numeric::RationalVector& old_field,
                                          const numeric::RationalVector& new_field) {
    const auto EA = new_accession.multiply(ambient_correspondence)
        .add(output_correspondence.multiply(old_accession).scale(numeric::Rational{-1}));
    const auto eta = numeric::sub(new_field, ambient_correspondence.apply(old_field));
    const auto accession_term = EA.apply(old_field);
    const auto field_term = new_accession.apply(eta);
    return {EA, accession_term, field_term, numeric::add(accession_term, field_term)};
}

inline numeric::ExactMatrix metric_pullback_defect(const field::ExactMetric& old_metric,
                                                   const field::ExactMetric& new_metric,
                                                   const numeric::ExactMatrix& correspondence) {
    if (correspondence.cols() != old_metric.size() || correspondence.rows() != new_metric.size()) throw std::invalid_argument("metric correspondence shape mismatch");
    return correspondence.transpose().multiply(new_metric.materialize()).multiply(correspondence)
        .add(old_metric.materialize().scale(numeric::Rational{-1}));
}

inline numeric::RationalVector shifted_solve(const numeric::ExactMatrix& L,
                                             const numeric::Rational& lambda,
                                             const numeric::RationalVector& rhs) {
    if (L.rows() != L.cols() || rhs.size() != L.rows()) throw std::invalid_argument("shifted solve dimensions");
    const auto A = numeric::ExactMatrix::identity(L.rows()).add(L.scale(lambda));
    return A.solve(rhs);
}

struct GreenResponseDelta {
    numeric::RationalVector old_response;
    numeric::RationalVector new_response;
    numeric::ExactMatrix injection_defect;
    numeric::ExactMatrix operator_defect;
    numeric::RationalVector coordinate_innovation;
    numeric::RationalVector realization_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector operator_term;
    numeric::RationalVector aggregate;
};

inline GreenResponseDelta green_response_delta(const numeric::ExactMatrix& old_L,
                                               const numeric::ExactMatrix& new_L,
                                               const numeric::ExactMatrix& ambient_correspondence,
                                               const numeric::ExactMatrix& old_injection,
                                               const numeric::ExactMatrix& new_injection,
                                               const numeric::ExactMatrix& source_correspondence,
                                               const numeric::RationalVector& old_coordinates,
                                               const numeric::RationalVector& new_coordinates,
                                               const numeric::Rational& lambda) {
    const auto source = source_injection_delta(old_injection, new_injection, ambient_correspondence,
                                               source_correspondence, old_coordinates, new_coordinates);
    const auto old_field = old_injection.apply(old_coordinates);
    const auto new_field = new_injection.apply(new_coordinates);
    const auto g = shifted_solve(old_L, lambda, old_field);
    const auto gp = shifted_solve(new_L, lambda, new_field);
    const auto EL = new_L.multiply(ambient_correspondence)
        .add(ambient_correspondence.multiply(old_L).scale(numeric::Rational{-1}));
    const auto realization = shifted_solve(new_L, lambda, source.realization_defect.apply(old_coordinates));
    const auto innovation = shifted_solve(new_L, lambda, new_injection.apply(source.coordinate_innovation));
    const auto operator_term = numeric::scale(-lambda, shifted_solve(new_L, lambda, EL.apply(g)));
    const auto aggregate = numeric::add(numeric::add(realization, innovation), operator_term);
    const auto direct = numeric::sub(gp, ambient_correspondence.apply(g));
    if (aggregate != direct) throw std::logic_error("Green response delta identity failed");
    return {g, gp, source.realization_defect, EL, source.coordinate_innovation,
            realization, innovation, operator_term, aggregate};
}

struct ObservedGreenResponseDelta {
    numeric::ExactMatrix accession_defect;
    numeric::RationalVector accession_term;
    numeric::RationalVector realization_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector operator_term;
    numeric::RationalVector aggregate;
};

inline ObservedGreenResponseDelta observed_green_response_delta(const GreenResponseDelta& green,
                                                                const numeric::ExactMatrix& old_accession,
                                                                const numeric::ExactMatrix& new_accession,
                                                                const numeric::ExactMatrix& ambient_correspondence,
                                                                const numeric::ExactMatrix& output_correspondence) {
    const auto EA = new_accession.multiply(ambient_correspondence)
        .add(output_correspondence.multiply(old_accession).scale(numeric::Rational{-1}));
    const auto accession_term = EA.apply(green.old_response);
    const auto realization_term = new_accession.apply(green.realization_term);
    const auto innovation_term = new_accession.apply(green.innovation_term);
    const auto operator_term = new_accession.apply(green.operator_term);
    auto aggregate = numeric::add(accession_term, realization_term);
    aggregate = numeric::add(aggregate, innovation_term);
    aggregate = numeric::add(aggregate, operator_term);
    return {EA, accession_term, realization_term, innovation_term, operator_term, aggregate};
}

} // namespace rfc::temporal
