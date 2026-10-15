#pragma once
#include "rfc/numeric/algebraic_field.hpp"

namespace rfc::numeric {

// An explicitly declared Q-linear field involution. This need not be complex
// conjugation in any selected embedding, and its trace form need not be positive.
class AlgebraicInvolution {
    struct State {
        CertifiedAlgebraicField field;
        AlgebraicElement image;
        ExactMatrix action;
    };
public:
    AlgebraicInvolution(const AlgebraicInvolution&)=default;
    AlgebraicInvolution& operator=(const AlgebraicInvolution&)=default;
    static AlgebraicInvolution declare(const CertifiedAlgebraicField& field,const AlgebraicElement& generator_image) {
        if (!field.owns(generator_image)) throw std::invalid_argument("involution image field owner differs");
        if (!field.evaluate(field.minimal_polynomial(),generator_image).is_zero())
            throw std::invalid_argument("involution image does not respect the minimal polynomial");
        const auto d=field.degree();
        if (machine::checked_size_mul(d,d)>field.limits().max_matrix_coordinates)
            throw std::length_error("involution coordinate budget");
        ExactMatrix action(d,d); auto column=field.scalar(Rational{1});
        for (std::size_t j=0;j<d;++j) {
            for (std::size_t i=0;i<d;++i) action.set(i,j,column.coordinates()[i]);
            if (j+1<d) column=column*generator_image;
        }
        if (action.multiply(action)!=ExactMatrix::identity(d))
            throw std::invalid_argument("declared algebraic map is not an involution");
        return AlgebraicInvolution(std::make_shared<const State>(State{field,generator_image,std::move(action)}));
    }
    static AlgebraicInvolution identity(const CertifiedAlgebraicField& field) { return declare(field,field.generator()); }
    const CertifiedAlgebraicField& field() const & noexcept { return state_->field; }
    const AlgebraicElement& generator_image() const & noexcept { return state_->image; }
    const ExactMatrix& action() const & noexcept { return state_->action; }
    const CertifiedAlgebraicField& field() const && = delete;
    const AlgebraicElement& generator_image() const && = delete;
    const ExactMatrix& action() const && = delete;
    bool same_owner(const AlgebraicInvolution& other) const noexcept { return state_==other.state_; }
    AlgebraicElement apply(const AlgebraicElement& value) const {
        if (!state_->field.owns(value)) throw std::invalid_argument("involution operand field owner differs");
        return state_->field.element(state_->action.apply(value.coordinates()));
    }
    ExactMatrix trace_form() const {
        // Tr(bar(x)*y)/[K:Q] in the declared power basis. Positivity must be
        // certified by the existing metric API before any Hodge interpretation.
        const auto& field=state_->field; const auto d=field.degree();
        ExactMatrix form(d,d); std::vector<AlgebraicElement> basis;
        basis.reserve(d); auto next=field.scalar(Rational{1});
        for (std::size_t i=0;i<d;++i) {basis.push_back(next); if(i+1<d) next=next*field.generator();}
        algebraic_detail::Work work{field.limits()};
        for (std::size_t i=0;i<d;++i) {
            const auto conjugate=apply(basis[i]);
            for (std::size_t j=0;j<d;++j)
                form.set(i,j,work.checked(Rational((conjugate*basis[j]).trace()/Rational(Integer(d)))));
        }
        return form;
    }
private:
    explicit AlgebraicInvolution(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

// A norm-one unit for the actually declared field/involution. A geometric
// complex-phase interpretation additionally requires a compatible embedding.
class AlgebraicUnitPhase {
public:
    AlgebraicUnitPhase(AlgebraicInvolution involution,AlgebraicElement value)
        : involution_(std::move(involution)),value_(std::move(value)) {
        if (!involution_.field().owns(value_)) throw std::invalid_argument("algebraic phase field owner differs");
        if (value_*involution_.apply(value_)!=involution_.field().scalar(Rational{1}))
            throw std::invalid_argument("algebraic phase requires a declared norm-one unit");
    }
    const AlgebraicInvolution& involution() const & noexcept { return involution_; }
    const AlgebraicElement& value() const & noexcept { return value_; }
    const AlgebraicInvolution& involution() const && = delete;
    const AlgebraicElement& value() const && = delete;
    AlgebraicUnitPhase inverse() const { return {involution_,involution_.apply(value_)}; }
    AlgebraicUnitPhase power(std::uint64_t exponent) const { return {involution_,value_.power(exponent)}; }
    template<std::floating_point T> AlgebraicUnitPhase power(T) const = delete;
    ExactMatrix regular_action() const { return value_.regular_action(); }
    friend AlgebraicUnitPhase operator*(const AlgebraicUnitPhase& a,const AlgebraicUnitPhase& b) {
        if (!a.involution_.same_owner(b.involution_)) throw std::invalid_argument("algebraic phase involution owners differ");
        return {a.involution_,a.value_*b.value_};
    }
    friend bool operator==(const AlgebraicUnitPhase& a,const AlgebraicUnitPhase& b) noexcept {
        return a.involution_.same_owner(b.involution_) && a.value_==b.value_;
    }
private:
    AlgebraicInvolution involution_;
    AlgebraicElement value_;
};
} // namespace rfc::numeric
