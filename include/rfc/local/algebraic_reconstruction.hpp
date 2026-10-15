#pragma once
#include "rfc/local/algebraic_module.hpp"
#include "rfc/numeric/algebraic_embedding.hpp"

namespace rfc::local {
// A(x) = C sigma(x). Field and carrier comparisons are explicit even when
// their coordinate presentations agree. C retains every target/source entry.
class AlgebraicCarrierComparison {
    struct State {
        numeric::AlgebraicFieldEmbedding scalars;
        AlgebraicModule source,target;
        std::vector<numeric::AlgebraicElement> coefficients;
        numeric::ExactMatrix action;
    };
public:
    static AlgebraicCarrierComparison declare(const numeric::AlgebraicFieldEmbedding& scalars,
        const AlgebraicModule& source,const AlgebraicModule& target,
        std::vector<numeric::AlgebraicElement> coefficients) {
        if (!source.field().same_owner(scalars.source()) || !target.field().same_owner(scalars.target()))
            throw std::invalid_argument("carrier comparison field owners differ");
        const auto count=machine::checked_size_mul(source.rank(),target.rank());
        if (coefficients.size()!=count) throw std::invalid_argument("carrier comparison requires complete coefficients");
        const auto ds=source.field().degree(),dt=target.field().degree();
        const auto slots=machine::checked_size_mul(count,machine::checked_size_mul(ds,dt));
        // Regular target coefficient blocks are also constructed below.
        const auto block=machine::checked_size_mul(dt,dt);
        if (slots>source.limits().max_matrix_coordinates || slots>target.limits().max_matrix_coordinates ||
            block>target.field().limits().max_matrix_coordinates)
            throw std::length_error("carrier comparison rational coordinate budget");
        for (const auto& value:coefficients)
            if (!target.field().owns(value)) throw std::invalid_argument("carrier comparison coefficient owner differs");
        numeric::ExactMatrix action(machine::checked_size_mul(target.rank(),dt),machine::checked_size_mul(source.rank(),ds));
        for (std::size_t i=0;i<target.rank();++i) for (std::size_t j=0;j<source.rank();++j) {
            const auto a=coefficients[i*source.rank()+j].regular_action().multiply(scalars.rational_action());
            for (const auto& [key,q]:a.entries()) action.set(i*dt+key.first,j*ds+key.second,q);
        }
        return AlgebraicCarrierComparison(std::make_shared<const State>(State{scalars,source,target,std::move(coefficients),std::move(action)}));
    }
    static AlgebraicCarrierComparison from_map(const AlgebraicModuleMap& map) {
        return declare(numeric::AlgebraicFieldEmbedding::identity(map.source().field()),map.source(),map.target(),map.coefficients());
    }
    const numeric::AlgebraicFieldEmbedding& scalar_comparison() const & noexcept { return state_->scalars; }
    const AlgebraicModule& source() const & noexcept { return state_->source; }
    const AlgebraicModule& target() const & noexcept { return state_->target; }
    const std::vector<numeric::AlgebraicElement>& coefficients() const & noexcept { return state_->coefficients; }
    const numeric::ExactMatrix& rational_action() const & noexcept { return state_->action; }
    const numeric::AlgebraicFieldEmbedding& scalar_comparison() const && = delete;
    const AlgebraicModule& source() const && = delete;
    const AlgebraicModule& target() const && = delete;
    const std::vector<numeric::AlgebraicElement>& coefficients() const && = delete;
    const numeric::ExactMatrix& rational_action() const && = delete;
    bool same_owner(const AlgebraicCarrierComparison& other) const noexcept { return state_==other.state_; }
    AlgebraicModuleVector apply(const AlgebraicModuleVector& value) const {
        if (!source().same_owner(value.carrier())) throw std::invalid_argument("carrier comparison input owner differs");
        std::vector<numeric::AlgebraicElement> input; input.reserve(source().rank());
        for (const auto& x:value.coordinates()) input.push_back(state_->scalars.apply(x));
        std::vector<numeric::AlgebraicElement> output(target().rank(),target().field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<target().rank();++i) for (std::size_t j=0;j<source().rank();++j)
            output[i]=output[i]+state_->coefficients[i*source().rank()+j]*input[j];
        return target().vector(std::move(output));
    }
    AlgebraicCarrierComparison then(const AlgebraicCarrierComparison& next) const {
        if (!target().same_owner(next.source())) throw std::invalid_argument("carrier comparison intermediate owner differs");
        const auto count=machine::checked_size_mul(source().rank(),next.target().rank());
        const auto slots=machine::checked_size_mul(count,machine::checked_size_mul(source().field().degree(),next.target().field().degree()));
        if (slots>source().limits().max_matrix_coordinates || slots>next.target().limits().max_matrix_coordinates)
            throw std::length_error("carrier comparison composition coordinate budget");
        std::vector<numeric::AlgebraicElement> out(count,next.target().field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<next.target().rank();++i) for (std::size_t j=0;j<source().rank();++j)
            for (std::size_t k=0;k<target().rank();++k)
                out[i*source().rank()+j]=out[i*source().rank()+j]+
                    next.coefficients()[i*target().rank()+k]*next.scalar_comparison().apply(coefficients()[k*source().rank()+j]);
        auto result=declare(state_->scalars.then(next.state_->scalars),source(),next.target(),std::move(out));
        if (result.rational_action()!=next.rational_action().multiply(rational_action()))
            throw std::logic_error("complete carrier comparison composition invariant");
        return result;
    }
private:
    explicit AlgebraicCarrierComparison(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

// A faithful physical presentation needs an explicitly declared left inverse.
// R A = I does not assert A R = I on redundant physical directions.
class AlgebraicReconstruction {
public:
    static AlgebraicReconstruction declare(const AlgebraicCarrierComparison& accession,
                                           const AlgebraicCarrierComparison& reconstruction) {
        if (!accession.target().same_owner(reconstruction.source()) ||
            !accession.source().same_owner(reconstruction.target()))
            throw std::invalid_argument("algebraic reconstruction carrier owners differ");
        const auto composite=accession.then(reconstruction);
        const auto n=machine::checked_size_mul(accession.source().rank(),accession.source().field().degree());
        if (composite.rational_action()!=numeric::ExactMatrix::identity(n))
            throw std::invalid_argument("algebraic reconstruction does not satisfy R A = I");
        return {accession,reconstruction};
    }
    const AlgebraicCarrierComparison& accession() const & noexcept { return accession_; }
    const AlgebraicCarrierComparison& reconstruction() const & noexcept { return reconstruction_; }
    const AlgebraicCarrierComparison& accession() const && = delete;
    const AlgebraicCarrierComparison& reconstruction() const && = delete;
    AlgebraicCarrierComparison physical_projector() const { return reconstruction_.then(accession_); }
    AlgebraicCarrierComparison physical_operation(const AlgebraicModuleMap& operation) const {
        if (!accession_.source().same_owner(operation.source()) || !operation.source().same_owner(operation.target()))
            throw std::invalid_argument("reconstructed operation requires the original carrier");
        return reconstruction_.then(AlgebraicCarrierComparison::from_map(operation)).then(accession_);
    }
private:
    AlgebraicReconstruction(AlgebraicCarrierComparison a,AlgebraicCarrierComparison r)
        : accession_(std::move(a)),reconstruction_(std::move(r)) {}
    AlgebraicCarrierComparison accession_,reconstruction_;
};
} // namespace rfc::local
