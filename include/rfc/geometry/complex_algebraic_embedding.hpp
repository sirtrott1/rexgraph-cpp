#pragma once
#include "rfc/numeric/algebraic_embedding.hpp"

namespace rfc::geometry {
// Rational coordinates of an observation, not coefficients replacing a field.
struct ExactComplexCoordinates {
    numeric::Rational real,imaginary;
    ExactComplexCoordinates(numeric::Rational x,numeric::Rational y) : real(std::move(x)),imaginary(std::move(y)) {}
    template<class A,class B> requires (std::floating_point<A> || std::floating_point<B>)
    ExactComplexCoordinates(A,B)=delete;
    friend bool operator==(const ExactComplexCoordinates&,const ExactComplexCoordinates&)=default;
};
struct ExactComplexDiamond {
    ExactComplexCoordinates center;
    numeric::Rational radius; // |Re(z-center)| + |Im(z-center)| <= radius
    friend bool operator==(const ExactComplexDiamond&,const ExactComplexDiamond&)=default;
};
struct ComplexRootLimits {
    std::size_t max_refinements{256},max_rounding_bits{4096},max_trace_coordinates{8192};
};
struct ComplexRootCertificate {
    ExactComplexDiamond region;
    std::vector<ExactComplexCoordinates> taylor_coefficients,fixed_point_coefficients;
    numeric::Rational center_step_bound,contraction_bound,selfmap_bound,margin;
    std::size_t root_count{1};
};
struct ComplexRefinementStep {
    ExactComplexDiamond before,after;
    ExactComplexCoordinates local_image;
    numeric::Rational slack,rounding_error;
    std::size_t grid_bits;
};
namespace complex_root_detail {
using Q=numeric::Rational;
using Z=ExactComplexCoordinates;
using Work=numeric::algebraic_detail::Work;
inline Z canonical(Z value,Work& work) {
    value.real=numeric::canonical_rational(std::move(value.real));
    value.imaginary=numeric::canonical_rational(std::move(value.imaginary));
    work.admit(value.real); work.admit(value.imaginary); return value;
}
inline Q norm(const Z& value,Work& work) {
    return work.checked(Q(abs(value.real)+abs(value.imaginary)));
}
inline Z add(const Z& a,const Z& b,Work& work) {
    return {work.checked(Q(a.real+b.real)),work.checked(Q(a.imaginary+b.imaginary))};
}
inline Z subtract(const Z& a,const Z& b,Work& work) {
    return {work.checked(Q(a.real-b.real)),work.checked(Q(a.imaginary-b.imaginary))};
}
inline Z multiply(const Z& a,const Z& b,Work& work) {
    const auto ac=work.checked(Q(a.real*b.real)),bd=work.checked(Q(a.imaginary*b.imaginary));
    const auto ad=work.checked(Q(a.real*b.imaginary)),bc=work.checked(Q(a.imaginary*b.real));
    return {work.checked(Q(ac-bd)),work.checked(Q(ad+bc))};
}
inline Z negative_quotient(const Z& a,const Z& b,Work& work) {
    const auto divisor=work.checked(Q(b.real*b.real+b.imaginary*b.imaginary));
    if (!divisor) throw std::invalid_argument("complex root center derivative must be nonzero");
    const auto x=work.checked(Q(a.real*b.real+a.imaginary*b.imaginary));
    const auto y=work.checked(Q(a.imaginary*b.real-a.real*b.imaginary));
    return {work.checked(Q(-x/divisor)),work.checked(Q(-y/divisor))};
}
inline std::vector<Z> taylor(const numeric::RationalVector& polynomial,const Z& center,Work& work) {
    // Exact Horner translation p(center+w), retaining every coefficient.
    std::vector<Z> result;
    for (auto i=polynomial.rbegin();i!=polynomial.rend();++i) {
        std::vector<Z> next(result.size()+1,Z{Q{0},Q{0}});
        for (std::size_t j=0;j<result.size();++j) {
            next[j]=add(next[j],multiply(result[j],center,work),work);
            next[j+1]=add(next[j+1],result[j],work);
        }
        next[0]=add(next[0],Z{*i,Q{0}},work); result=std::move(next);
    }
    return result;
}
inline Z evaluate(const std::vector<Z>& polynomial,const Z& point,Work& work) {
    Z result{Q{0},Q{0}};
    for (auto i=polynomial.rbegin();i!=polynomial.rend();++i) result=add(multiply(result,point,work),*i,work);
    return result;
}
inline ComplexRootCertificate certificate(const numeric::CertifiedAlgebraicField& field,ExactComplexDiamond region,Work& work) {
    region.center=canonical(std::move(region.center),work);
    region.radius=numeric::canonical_rational(std::move(region.radius)); work.admit(region.radius);
    if (field.degree()<2 || region.radius<=0 || abs(region.center.imaginary)<=region.radius)
        throw std::invalid_argument("selected complex region must have positive radius and lie strictly off the real axis");
    auto coefficients=taylor(field.minimal_polynomial(),region.center,work);
    std::vector<Z> fixed; fixed.reserve(coefficients.size());
    for (std::size_t i=0;i<coefficients.size();++i)
        fixed.push_back(i==1?Z{Q{0},Q{0}}:negative_quotient(coefficients[i],coefficients[1],work));
    const auto beta=norm(fixed[0],work); Q contraction{},power=region.radius;
    for (std::size_t k=2;k<fixed.size();++k) {
        const auto factor=work.checked(Q(numeric::Integer(std::to_string(k))*norm(fixed[k],work)));
        contraction=work.checked(Q(contraction+factor*power));
        if (k+1<fixed.size()) power=work.checked(Q(power*region.radius));
    }
    const auto bound=work.checked(Q(beta+contraction*region.radius));
    const auto margin=work.checked(Q(region.radius-bound));
    if (contraction<0 || contraction>=1 || margin<=0)
        throw std::invalid_argument("complex region fails its strict contraction and self-map certificate");
    return {std::move(region),std::move(coefficients),std::move(fixed),beta,contraction,bound,margin,1};
}
inline Q rounded(const Q& coordinate,const Q& unit,Work& work) {
    const auto scaled=work.checked(Q(coordinate/unit)); numeric::Integer floor;
    mpz_fdiv_q(floor.get_mpz_t(),mpq_numref(scaled.get_mpq_t()),mpq_denref(scaled.get_mpq_t()));
    return work.checked(Q(Q(floor)*unit));
}
inline ComplexRefinementStep step(const ComplexRootCertificate& proof,const ExactComplexDiamond& before,
                                  const ComplexRootLimits& limits,Work& work) {
    const auto local=subtract(before.center,proof.region.center,work);
    const auto image=evaluate(proof.fixed_point_coefficients,local,work);
    const auto difference=norm(subtract(image,local,work),work);
    const auto slack=work.checked(Q((1-proof.contraction_bound)*before.radius-difference));
    if (slack<=0) throw std::logic_error("complex refinement strict slack invariant");
    Q unit{1}; std::size_t bits{};
    while (work.checked(Q(8*unit))>slack) {
        if (bits==limits.max_rounding_bits) throw std::length_error("complex enclosure grid bit budget");
        unit=work.checked(Q(unit/2)); ++bits;
    }
    const Z next{rounded(image.real,unit,work),rounded(image.imaginary,unit,work)};
    const auto error=norm(subtract(next,image,work),work);
    ExactComplexDiamond after{add(proof.region.center,next,work),work.checked(Q(proof.contraction_bound*before.radius+error))};
    if (error>work.checked(Q(slack/4)) || after.radius<=0 || after.radius>=before.radius ||
        work.checked(Q(norm(subtract(after.center,before.center,work),work)+after.radius))>=before.radius)
        throw std::logic_error("complex enclosure nested refinement invariant");
    return {before,std::move(after),image,slack,error,bits};
}
inline ExactComplexDiamond enclosure(const numeric::AlgebraicElement& value,const ExactComplexDiamond& region,Work& work) {
    const auto coefficients=taylor(value.coordinates(),region.center,work);
    Q radius{},power=region.radius;
    for (std::size_t k=1;k<coefficients.size();++k) {
        radius=work.checked(Q(radius+norm(coefficients[k],work)*power));
        if (k+1<coefficients.size()) power=work.checked(Q(power*region.radius));
    }
    return {coefficients[0],std::move(radius)};
}
inline bool contained(const ExactComplexDiamond& inner,const ExactComplexDiamond& outer,Work& work) {
    return work.checked(Q(norm(subtract(inner.center,outer.center,work),work)+inner.radius))<=outer.radius;
}
inline bool disjoint(const ExactComplexDiamond& a,const ExactComplexDiamond& b,Work& work) {
    return norm(subtract(a.center,b.center,work),work)>work.checked(Q(a.radius+b.radius));
}
} // namespace complex_root_detail

// The exact root is selected by a finite rational contraction certificate.
// Grid rounding changes only certified enclosure centers, never field values.
class SelectedComplexAlgebraicEmbedding {
    struct State { numeric::CertifiedAlgebraicField field; ComplexRootCertificate certificate; ComplexRootLimits limits; };
public:
    class Observation;
    class Refinement {
    public:
        Refinement(const Refinement&)=default;
        Refinement& operator=(const Refinement&)=default;
        const ExactComplexDiamond& region() const & noexcept { return region_; }
        const std::vector<ComplexRefinementStep>& steps() const & noexcept { return steps_; }
        const ComplexRootCertificate& root_certificate() const & noexcept { return owner_->certificate; }
        const ExactComplexDiamond& region() const && = delete;
        const std::vector<ComplexRefinementStep>& steps() const && = delete;
        const ComplexRootCertificate& root_certificate() const && = delete;
    private:
        friend class SelectedComplexAlgebraicEmbedding;
        friend class Observation;
        Refinement(std::shared_ptr<const State> owner,ExactComplexDiamond region,std::vector<ComplexRefinementStep> steps)
            : owner_(std::move(owner)),region_(std::move(region)),steps_(std::move(steps)) {}
        std::shared_ptr<const State> owner_;
        ExactComplexDiamond region_;
        std::vector<ComplexRefinementStep> steps_;
    };
    class Observation {
    public:
        Observation(const Observation&)=default;
        Observation& operator=(const Observation&)=default;
        const numeric::AlgebraicElement& value() const & noexcept { return value_; }
        const numeric::CertifiedAlgebraicField& field() const & noexcept { return owner_->field; }
        const numeric::AlgebraicElement& value() const && = delete;
        const numeric::CertifiedAlgebraicField& field() const && = delete;
        bool is_zero() const noexcept { return value_.is_zero(); }
        ExactComplexDiamond enclosure(const Refinement& refinement) const {
            if (owner_!=refinement.owner_) throw std::invalid_argument("complex observation refinement embedding owner differs");
            numeric::algebraic_detail::Work work{owner_->field.limits()};
            return complex_root_detail::enclosure(value_,refinement.region_,work);
        }
    private:
        friend class SelectedComplexAlgebraicEmbedding;
        Observation(std::shared_ptr<const State> owner,numeric::AlgebraicElement value) : owner_(std::move(owner)),value_(std::move(value)) {}
        std::shared_ptr<const State> owner_;
        numeric::AlgebraicElement value_;
    };
    SelectedComplexAlgebraicEmbedding(const SelectedComplexAlgebraicEmbedding&)=default;
    SelectedComplexAlgebraicEmbedding& operator=(const SelectedComplexAlgebraicEmbedding&)=default;
    static SelectedComplexAlgebraicEmbedding isolate(const numeric::CertifiedAlgebraicField& field,
        ExactComplexCoordinates center,numeric::Rational radius,ComplexRootLimits limits={}) {
        if (machine::checked_size_add(machine::checked_size_mul(4,field.degree()+1),16)>limits.max_trace_coordinates)
            throw std::length_error("complex root certificate coordinate budget");
        numeric::algebraic_detail::Work work{field.limits()};
        auto proof=complex_root_detail::certificate(field,{std::move(center),std::move(radius)},work);
        return SelectedComplexAlgebraicEmbedding(std::make_shared<const State>(State{field,std::move(proof),limits}));
    }
    template<std::floating_point T>
    static SelectedComplexAlgebraicEmbedding isolate(const numeric::CertifiedAlgebraicField&,ExactComplexCoordinates,T,ComplexRootLimits={})=delete;
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return state_->field; }
    const ComplexRootCertificate& certificate() const & noexcept { return state_->certificate; }
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const ComplexRootCertificate& certificate() const && = delete;
    bool same_owner(const SelectedComplexAlgebraicEmbedding& other) const noexcept { return state_==other.state_; }
    Refinement refine(std::size_t iterations) const {
        if (iterations>state_->limits.max_refinements ||
            machine::checked_size_add(machine::checked_size_mul(16,iterations),
                machine::checked_size_add(machine::checked_size_mul(4,state_->field.degree()+1),16))>state_->limits.max_trace_coordinates)
            throw std::length_error("complex refinement step or trace coordinate budget");
        numeric::algebraic_detail::Work work{state_->field.limits()}; auto region=state_->certificate.region;
        std::vector<ComplexRefinementStep> steps; steps.reserve(iterations);
        for (std::size_t i=0;i<iterations;++i) {
            auto next=complex_root_detail::step(state_->certificate,region,state_->limits,work);
            region=next.after; steps.push_back(std::move(next));
        }
        return Refinement(state_,std::move(region),std::move(steps));
    }
    template<std::floating_point T> Refinement refine(T) const = delete;
    Observation observe(const numeric::AlgebraicElement& value) const {
        if (!state_->field.owns(value)) throw std::invalid_argument("complex observation value field owner differs");
        return Observation(state_,value);
    }
    SelectedComplexAlgebraicEmbedding conjugated() const {
        const auto& region=state_->certificate.region;
        return isolate(state_->field,{region.center.real,numeric::Rational(-region.center.imaginary)},region.radius,state_->limits);
    }
    bool agrees_with(const numeric::AlgebraicFieldEmbedding& comparison,const SelectedComplexAlgebraicEmbedding& target) const {
        if (!state_->field.same_owner(comparison.source()) || !target.field().same_owner(comparison.target()))
            throw std::invalid_argument("complex embedding comparison field owners differ");
        numeric::algebraic_detail::Work work{target.field().limits()}; auto region=target.certificate().region;
        const auto& original=state_->certificate.region;
        work.admit(original.center.real); work.admit(original.center.imaginary); work.admit(original.radius);
        const auto count=std::min(state_->limits.max_refinements,target.state_->limits.max_refinements);
        for (std::size_t i=0;;++i) {
            const auto image=complex_root_detail::enclosure(comparison.generator_image(),region,work);
            if (complex_root_detail::contained(image,original,work)) return true;
            if (complex_root_detail::disjoint(image,original,work)) return false;
            if (i==count) throw std::length_error("complex selected-root comparison budget exhausted");
            region=complex_root_detail::step(target.certificate(),region,target.state_->limits,work).after;
        }
    }
    bool same_selected_root(const SelectedComplexAlgebraicEmbedding& other) const {
        if (!state_->field.same_owner(other.field())) throw std::invalid_argument("complex selected-root field owners differ");
        if (same_owner(other)) return true;
        return agrees_with(numeric::AlgebraicFieldEmbedding::identity(state_->field),other);
    }
private:
    explicit SelectedComplexAlgebraicEmbedding(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::geometry
