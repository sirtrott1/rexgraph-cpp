#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <stdexcept>

namespace rfc::temporal {

enum class HodgeSector { Gradient, Curl, Harmonic };

inline numeric::RationalVector project_sector(const field::HodgeProjectors& p,
                                              HodgeSector sector,
                                              const numeric::RationalVector& x){
    switch(sector){
        case HodgeSector::Gradient:return p.project_gradient(x);
        case HodgeSector::Curl:return p.project_curl(x);
        case HodgeSector::Harmonic:return p.project_harmonic(x);
    }
    throw std::logic_error("unknown Hodge sector");
}

inline numeric::RationalVector sector_transport_apply(
    const field::HodgeProjectors& old_projectors,
    const field::HodgeProjectors& new_projectors,
    const numeric::ExactMatrix& correspondence,
    HodgeSector destination,
    HodgeSector source,
    const numeric::RationalVector& field_value){
    if(correspondence.cols()!=old_projectors.metric.size() || correspondence.rows()!=new_projectors.metric.size()) {
        throw std::invalid_argument("sector transport correspondence mismatch");
    }
    return project_sector(new_projectors,destination,
                          correspondence.apply(project_sector(old_projectors,source,field_value)));
}

inline numeric::RationalVector hodge_defect_apply(
    const field::ExactHodgeOperator& old_hodge,
    const field::ExactHodgeOperator& new_hodge,
    const numeric::ExactMatrix& correspondence,
    const numeric::RationalVector& old_field){
    if(correspondence.cols()!=old_hodge.size() || correspondence.rows()!=new_hodge.size() || old_field.size()!=old_hodge.size()) {
        throw std::invalid_argument("Hodge defect dimension mismatch");
    }
    return numeric::sub(new_hodge.apply(correspondence.apply(old_field)),
                        correspondence.apply(old_hodge.apply(old_field)));
}

inline numeric::RationalVector typed_sector_transport_apply(
    const numeric::ExactMatrix& destination_accession,
    const field::HodgeProjectors& new_projectors,
    const numeric::ExactMatrix& correspondence,
    const field::HodgeProjectors& old_projectors,
    const numeric::ExactMatrix& source_reconstruction,
    HodgeSector destination,
    HodgeSector source,
    const numeric::RationalVector& source_view){
    if(source_reconstruction.cols()!=source_view.size() || source_reconstruction.rows()!=old_projectors.metric.size() ||
       destination_accession.cols()!=new_projectors.metric.size()) {
        throw std::invalid_argument("typed sector transport dimension mismatch");
    }
    const auto ambient=source_reconstruction.apply(source_view);
    return destination_accession.apply(sector_transport_apply(old_projectors,new_projectors,correspondence,destination,source,ambient));
}

} // namespace rfc::temporal
