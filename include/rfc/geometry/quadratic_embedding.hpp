#pragma once
#include "rfc/numeric/algebraic_phase.hpp"
#include <optional>

namespace rfc::geometry {
enum class QuadraticRealRoot { Smaller, Larger };
enum class QuadraticHalfPlane { Upper, Lower };
enum class RadicalAxis { Real, Imaginary };
struct QuadraticRootInterval { numeric::Rational lower,upper; };
struct ExactRadicalCoordinates {
    numeric::Rational rational_part,radical_coefficient,radicand;
    RadicalAxis axis;
};

// Selects one quadratic root by an exact isolating interval or half-plane.
// Values remain a+b*sqrt(r) or a+i*b*sqrt(r), never approximate root samples.
class SelectedQuadraticEmbedding {
    using Q=numeric::Rational;
public:
    SelectedQuadraticEmbedding(const SelectedQuadraticEmbedding&)=default;
    SelectedQuadraticEmbedding& operator=(const SelectedQuadraticEmbedding&)=default;
    static SelectedQuadraticEmbedding real_root(const numeric::CertifiedAlgebraicField& field,
        QuadraticRealRoot side,Q lower,Q upper) {
        const auto discriminant=discriminant_of(field);
        if (discriminant<=0) throw std::invalid_argument("real embedding requires positive discriminant");
        if (side!=QuadraticRealRoot::Smaller && side!=QuadraticRealRoot::Larger)
            throw std::invalid_argument("quadratic real root selection");
        lower=numeric::canonical_rational(std::move(lower)); upper=numeric::canonical_rational(std::move(upper));
        algebraic_detail_check(field,lower); algebraic_detail_check(field,upper);
        if (lower>=upper) throw std::invalid_argument("quadratic root interval order");
        const auto& f=field.minimal_polynomial();
        const Q left=lower*lower+f[1]*lower+f[0],right=upper*upper+f[1]*upper+f[0];
        algebraic_detail_check(field,left); algebraic_detail_check(field,right);
        const bool smaller=side==QuadraticRealRoot::Smaller;
        if (smaller ? !(left>0 && right<0) : !(left<0 && right>0))
            throw std::invalid_argument("interval does not isolate the selected quadratic root");
        return {field,RadicalAxis::Real,smaller?-1:1,discriminant,QuadraticRootInterval{lower,upper}};
    }
    template<class A,class B> requires (std::floating_point<A> || std::floating_point<B>)
    static SelectedQuadraticEmbedding real_root(const numeric::CertifiedAlgebraicField&,QuadraticRealRoot,A,B)=delete;
    static SelectedQuadraticEmbedding complex_root(const numeric::CertifiedAlgebraicField& field,QuadraticHalfPlane side) {
        const auto discriminant=discriminant_of(field);
        if (discriminant>=0) throw std::invalid_argument("complex embedding requires negative discriminant");
        if (side!=QuadraticHalfPlane::Upper && side!=QuadraticHalfPlane::Lower)
            throw std::invalid_argument("quadratic half-plane selection");
        return {field,RadicalAxis::Imaginary,side==QuadraticHalfPlane::Upper?1:-1,discriminant,{}};
    }
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return field_; }
    const std::optional<QuadraticRootInterval>& interval() const & noexcept { return interval_; }
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const std::optional<QuadraticRootInterval>& interval() const && = delete;
    RadicalAxis axis() const noexcept { return axis_; }
    int root_sign() const noexcept { return sign_; }
    ExactRadicalCoordinates coordinates(const numeric::AlgebraicElement& value) const {
        if (!field_.owns(value)) throw std::invalid_argument("quadratic embedding field owner differs");
        const auto& coefficients=value.coordinates();
        Q rational=coefficients[0]+coefficients[1]*center_;
        Q radical=sign_*coefficients[1];
        algebraic_detail_check(field_,rational); algebraic_detail_check(field_,radical);
        return {std::move(rational),std::move(radical),radicand_,axis_};
    }
    numeric::AlgebraicInvolution conjugation() const {
        if (axis_==RadicalAxis::Real) return numeric::AlgebraicInvolution::identity(field_);
        return numeric::AlgebraicInvolution::declare(field_,field_.element({-field_.minimal_polynomial()[1],Q{-1}}));
    }
    numeric::ExactMatrix complex_norm_form() const {
        if (axis_!=RadicalAxis::Imaginary) throw std::domain_error("complex norm form requires a nonreal quadratic embedding");
        const Q off=center_, diagonal=center_*center_+radicand_;
        algebraic_detail_check(field_,diagonal);
        return numeric::ExactMatrix::from_rows({{Q{1},off},{off,diagonal}});
    }
    ExactRadicalCoordinates phase_coordinates(const numeric::AlgebraicUnitPhase& phase) const {
        if (axis_!=RadicalAxis::Imaginary) throw std::domain_error("complex phase coordinates require a nonreal embedding");
        const auto expected=field_.element({-field_.minimal_polynomial()[1],Q{-1}});
        if (!field_.same_owner(phase.involution().field()) || phase.involution().generator_image()!=expected)
            throw std::invalid_argument("declared phase involution is incompatible with this complex embedding");
        return coordinates(phase.value());
    }
private:
    static void algebraic_detail_check(const numeric::CertifiedAlgebraicField& field,const Q& q) {
        numeric::algebraic_detail::Work work{field.limits()}; work.admit(q);
    }
    static Q discriminant_of(const numeric::CertifiedAlgebraicField& field) {
        if (field.degree()!=2) throw std::invalid_argument("selected quadratic embedding requires degree two");
        const auto& f=field.minimal_polynomial(); Q out=f[1]*f[1]-4*f[0];
        algebraic_detail_check(field,out); return out;
    }
    SelectedQuadraticEmbedding(numeric::CertifiedAlgebraicField field,RadicalAxis axis,int sign,
                               const Q& discriminant,std::optional<QuadraticRootInterval> interval)
        : field_(std::move(field)),axis_(axis),sign_(sign),interval_(std::move(interval)),
          center_(-field_.minimal_polynomial()[1]/2),radicand_((discriminant>0?discriminant:-discriminant)/4) {
        algebraic_detail_check(field_,center_); algebraic_detail_check(field_,radicand_);
    }
    numeric::CertifiedAlgebraicField field_;
    RadicalAxis axis_;
    int sign_;
    std::optional<QuadraticRootInterval> interval_;
    Q center_,radicand_;
};
} // namespace rfc::geometry
