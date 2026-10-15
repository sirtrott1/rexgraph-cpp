#pragma once
#include "rfc/numeric/rational_phase.hpp"
#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace rfc::geometry {
enum class CoordinateAxis : std::uint8_t { X, Y, Z };

// Right-handed action in an explicitly chosen standard three-coordinate frame.
// Applying it to a relational stalk requires a declared realization of that frame.
inline numeric::ExactMatrix coordinate_rotation(CoordinateAxis axis, const numeric::RationalPhase& phase) {
    using Q = numeric::Rational;
    const auto& c = phase.real();
    const auto& s = phase.imaginary();
    switch (axis) {
        case CoordinateAxis::X: return numeric::ExactMatrix::from_rows({{1,0,0},{0,c,Q(-s)},{0,s,c}});
        case CoordinateAxis::Y: return numeric::ExactMatrix::from_rows({{c,0,s},{0,1,0},{Q(-s),0,c}});
        case CoordinateAxis::Z: return numeric::ExactMatrix::from_rows({{c,Q(-s),0},{s,c,0},{0,0,1}});
        default: throw std::invalid_argument("unknown coordinate rotation axis");
    }
}

// A point of the declared standard Euclidean unit sphere over Q. This is a
// geometric realization, not a cell-complex topology derived from a point cloud.
class RationalUnitSpherePoint {
public:
    RationalUnitSpherePoint(numeric::Rational x, numeric::Rational y, numeric::Rational z)
        : values_(numeric::canonical_vector({std::move(x),std::move(y),std::move(z)})) {
        if (numeric::dot(values_,values_) != 1)
            throw std::invalid_argument("rational unit sphere point has nonunit quadrance");
    }
    template<class A, class B, class C> requires
        (std::floating_point<A> || std::floating_point<B> || std::floating_point<C>)
    RationalUnitSpherePoint(A,B,C) = delete;
    static RationalUnitSpherePoint stereographic(numeric::Rational u, numeric::Rational v) {
        u = numeric::canonical_rational(std::move(u)); v = numeric::canonical_rational(std::move(v));
        const numeric::Rational square = u*u+v*v;
        const numeric::Rational denominator = 1+square;
        return {numeric::Rational(2*u/denominator),numeric::Rational(2*v/denominator),
                numeric::Rational((1-square)/denominator)};
    }
    template<class A, class B> requires (std::floating_point<A> || std::floating_point<B>)
    static RationalUnitSpherePoint stereographic(A,B) = delete;
    static RationalUnitSpherePoint excluded_pole() { return {0,0,-1}; }
    const numeric::RationalVector& coordinates() const & noexcept { return values_; }
    const numeric::RationalVector& coordinates() const && = delete;
    std::pair<numeric::Rational,numeric::Rational> stereographic_coordinates() const {
        const numeric::Rational denominator = 1+values_[2];
        if (denominator == 0) throw std::domain_error("pole is outside the finite stereographic chart");
        return {numeric::Rational(values_[0]/denominator),numeric::Rational(values_[1]/denominator)};
    }
    RationalUnitSpherePoint rotated(CoordinateAxis axis, const numeric::RationalPhase& phase) const {
        const auto values = coordinate_rotation(axis,phase).apply(values_);
        return {values[0],values[1],values[2]};
    }
    friend bool operator==(const RationalUnitSpherePoint&,const RationalUnitSpherePoint&) = default;
private:
    numeric::RationalVector values_;
};
} // namespace rfc::geometry
