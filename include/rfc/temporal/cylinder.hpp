#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::temporal {

inline numeric::ExactMatrix temporal_down_defect(const numeric::ExactMatrix& new_boundary,
                                                 const numeric::ExactMatrix& old_boundary,
                                                 const numeric::ExactMatrix& current_correspondence,
                                                 const numeric::ExactMatrix& lower_correspondence) {
    if(new_boundary.cols()!=current_correspondence.rows() ||
       current_correspondence.cols()!=old_boundary.cols() ||
       lower_correspondence.rows()!=new_boundary.rows() ||
       lower_correspondence.cols()!=old_boundary.rows()) {
        throw std::invalid_argument("temporal down-defect dimension mismatch");
    }
    return new_boundary.multiply(current_correspondence)
        .add(lower_correspondence.multiply(old_boundary).scale(numeric::Rational{-1}));
}

// Cylinder boundary from paper section 16.22:
// d_n(a,b,c)=(B'_n a+J_{n-1}c, B_n b-c, -B_{n-1}c).
inline numeric::ExactMatrix temporal_cylinder_boundary(
    const numeric::ExactMatrix& new_boundary_n,
    const numeric::ExactMatrix& old_boundary_n,
    const numeric::ExactMatrix& old_boundary_lower,
    const numeric::ExactMatrix& lower_correspondence) {
    if(lower_correspondence.rows()!=new_boundary_n.rows() ||
       lower_correspondence.cols()!=old_boundary_n.rows() ||
       old_boundary_lower.cols()!=old_boundary_n.rows()) {
        throw std::invalid_argument("temporal cylinder boundary dimension mismatch");
    }
    const std::size_t r_new=new_boundary_n.rows();
    const std::size_t r_old=old_boundary_n.rows();
    const std::size_t r_lower=old_boundary_lower.rows();
    const std::size_t c_new=new_boundary_n.cols();
    const std::size_t c_old=old_boundary_n.cols();
    const std::size_t c_temporal=r_old;
    numeric::ExactMatrix d(r_new+r_old+r_lower,c_new+c_old+c_temporal);
    for(const auto& [key,value]:new_boundary_n.entries()) d.set(key.first,key.second,value);
    for(const auto& [key,value]:lower_correspondence.entries()) d.set(key.first,c_new+c_old+key.second,value);
    for(const auto& [key,value]:old_boundary_n.entries()) d.set(r_new+key.first,c_new+key.second,value);
    for(std::size_t i=0;i<r_old;++i) d.set(r_new+i,c_new+c_old+i,numeric::Rational{-1});
    for(const auto& [key,value]:old_boundary_lower.entries()) d.set(r_new+r_old+key.first,c_new+c_old+key.second,-value);
    return d;
}

struct TemporalCylinderClosure {
    numeric::ExactMatrix lower_defect;
    bool closes{};
};

inline TemporalCylinderClosure temporal_cylinder_closure(
    const numeric::ExactMatrix& new_boundary_lower,
    const numeric::ExactMatrix& old_boundary_lower,
    const numeric::ExactMatrix& lower_correspondence,
    const numeric::ExactMatrix& lower_lower_correspondence) {
    auto defect=temporal_down_defect(new_boundary_lower,old_boundary_lower,lower_correspondence,lower_lower_correspondence);
    return {defect,defect.nonzeros()==0};
}

} // namespace rfc::temporal
