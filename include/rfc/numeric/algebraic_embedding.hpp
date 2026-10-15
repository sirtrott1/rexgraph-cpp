#pragma once
#include "rfc/numeric/algebraic_field.hpp"

namespace rfc::numeric {
// A declared generator image defines a unital field embedding. No coordinate
// equality or spatial root choice supplies this comparison implicitly.
class AlgebraicFieldEmbedding {
    struct State {
        CertifiedAlgebraicField source,target;
        AlgebraicElement image;
        ExactMatrix action;
    };
public:
    AlgebraicFieldEmbedding(const AlgebraicFieldEmbedding&)=default;
    AlgebraicFieldEmbedding& operator=(const AlgebraicFieldEmbedding&)=default;
    static AlgebraicFieldEmbedding declare(const CertifiedAlgebraicField& source,
        const CertifiedAlgebraicField& target,const AlgebraicElement& generator_image) {
        if (!target.owns(generator_image)) throw std::invalid_argument("field embedding image owner differs");
        if (machine::checked_size_mul(source.degree(),target.degree())>target.limits().max_matrix_coordinates)
            throw std::length_error("field embedding coordinate budget");
        if (!target.evaluate(source.minimal_polynomial(),generator_image).is_zero())
            throw std::invalid_argument("field embedding image fails the source polynomial");
        ExactMatrix action(target.degree(),source.degree());
        auto column=target.scalar(Rational{1});
        for (std::size_t j=0;j<source.degree();++j) {
            for (std::size_t i=0;i<target.degree();++i) action.set(i,j,column.coordinates()[i]);
            if (j+1<source.degree()) column=column*generator_image;
        }
        if (action.rank()!=source.degree()) throw std::logic_error("field embedding faithfulness invariant");
        return AlgebraicFieldEmbedding(std::make_shared<const State>(State{source,target,generator_image,std::move(action)}));
    }
    static AlgebraicFieldEmbedding identity(const CertifiedAlgebraicField& field) {
        return declare(field,field,field.generator());
    }
    const CertifiedAlgebraicField& source() const & noexcept { return state_->source; }
    const CertifiedAlgebraicField& target() const & noexcept { return state_->target; }
    const AlgebraicElement& generator_image() const & noexcept { return state_->image; }
    const ExactMatrix& rational_action() const & noexcept { return state_->action; }
    const CertifiedAlgebraicField& source() const && = delete;
    const CertifiedAlgebraicField& target() const && = delete;
    const AlgebraicElement& generator_image() const && = delete;
    const ExactMatrix& rational_action() const && = delete;
    bool same_owner(const AlgebraicFieldEmbedding& other) const noexcept { return state_==other.state_; }
    AlgebraicElement apply(const AlgebraicElement& value) const {
        if (!state_->source.owns(value)) throw std::invalid_argument("field embedding input owner differs");
        return state_->target.evaluate(value.coordinates(),state_->image);
    }
    // Chronological composition: the next embedding acts after this one.
    AlgebraicFieldEmbedding then(const AlgebraicFieldEmbedding& next) const {
        if (!state_->target.same_owner(next.state_->source))
            throw std::invalid_argument("field embedding intermediate owners differ");
        return declare(state_->source,next.state_->target,next.apply(state_->image));
    }
    AlgebraicFieldEmbedding inverse() const {
        if (state_->source.degree()!=state_->target.degree())
            throw std::invalid_argument("rectangular field embedding has no inverse comparison");
        const auto inverse_action=state_->action.inverse();
        const auto generator=state_->target.generator();
        const auto image=state_->source.element(inverse_action.apply(generator.coordinates()));
        auto result=declare(state_->target,state_->source,image);
        if (result.rational_action()!=inverse_action)
            throw std::logic_error("inverse field embedding complete action invariant");
        return result;
    }
private:
    explicit AlgebraicFieldEmbedding(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
