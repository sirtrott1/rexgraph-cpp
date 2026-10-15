#pragma once
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/field/prepared_operator_family.hpp"

namespace rfc::field {
class PreparedHodgeInvariants {
    using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedHodgeFrames frames;
        PreparedOperatorFamily operators;
        numeric::PreparedPositiveSemidefinite stiffness;
        std::size_t preparation;
        static numeric::ExactMatrix restricted(const PreparedHodgeFrames& frames,const PreparedOperatorFamily& operators,Guard& guard) {
            if(frames.source().get()!=operators.tower().source().get())throw std::invalid_argument("Hodge invariants require the actual prepared source owner");
            if(frames.metrics().size()!=operators.tower().metrics().size())throw std::invalid_argument("Hodge invariant metric family");
            for(std::size_t k=0;k<frames.metrics().size();++k)if(frames.metrics()[k]!=operators.tower().metrics()[k].materialize())
                throw std::invalid_argument("Hodge invariant metric declarations differ");
            frames.validate(guard);
            const auto F=numeric::ExactMatrix::horizontal_concat(frames.gradient_frame(),frames.curl_frame());guard.matrix(F);
            const auto metric=operators.metric(frames.grade()),L=operators.action({OperatorKind::Hodge,frames.grade(),0},guard);
            numeric::ExactMatrix result(F.cols(),F.cols());
            std::map<std::size_t,std::vector<numeric::SparseRationalEntry>> columns;
            for(const auto& [ij,q]:F.entries()){guard.step();columns[ij.second].push_back({ij.first,q});}
            for(auto& [j,entries]:columns){
                const auto measured=metric.apply(L.apply(numeric::SparseRationalVector(F.rows(),std::move(entries)),guard),guard);
                const auto values=numeric::checked_transpose_apply(F,measured,guard);
                for(const auto& entry:values.entries()){guard.step();result.set(entry.index,j,entry.value);guard.entries(result.nonzeros());}
            }
            return result;
        }
        State(PreparedHodgeFrames f,PreparedOperatorFamily o,Guard& guard)
            :frames(std::move(f)),operators(std::move(o)),stiffness(restricted(frames,operators,guard),guard),preparation(guard.steps()) {
            if(stiffness.rank()!=stiffness.original().rows())throw std::logic_error("nonharmonic Hodge stiffness must be positive definite");
        }
    };
    std::shared_ptr<const State> state_;
public:
    PreparedHodgeInvariants(PreparedHodgeFrames frames,PreparedOperatorFamily operators,Guard& guard)
        :state_(std::make_shared<const State>(std::move(frames),std::move(operators),guard)){}
    PreparedHodgeInvariants(PreparedHodgeFrames frames,PreparedOperatorFamily operators,numeric::ExactActionLimits limits={}) {
        Guard guard(limits);state_=std::make_shared<const State>(std::move(frames),std::move(operators),guard);
    }
    const PreparedHodgeFrames& frames() const & {return state_->frames;}
    const PreparedHodgeFrames& frames() const &&=delete;
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    numeric::Rational harmonic_volume(Guard& guard) const {return state_->frames.sector_volume(2,guard);}
    numeric::Rational pseudodeterminant(Guard& guard) const {
        const auto pin=state_;pin->frames.validate(guard);guard.matrix(pin->stiffness.original());guard.matrix(pin->stiffness.unit_lower_entries());
        numeric::Rational numerator=1;for(const auto& [i,q]:pin->stiffness.diagonal_entries()){(void)i;guard.value(q);numerator=guard.mul(numerator,q);}
        const auto denominator=guard.mul(pin->frames.sector_volume(0,guard),pin->frames.sector_volume(1,guard));
        return guard.div(numerator,denominator);
    }
    numeric::Rational pseudodeterminant(numeric::ExactActionLimits limits={}) const {Guard guard(limits);return pseudodeterminant(guard);}
};

class PreparedHarmonicDeterminant {
    using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedHodgeInvariants hodge;
        numeric::PreparedExactFrames coefficient_rank;
        std::optional<numeric::PreparedExactSolve> coefficient_factor;
        State(PreparedHodgeInvariants h,numeric::ExactMatrix sigma,Guard& guard)
            :hodge(std::move(h)),coefficient_rank(sigma,guard) {
            const auto n=hodge.frames().harmonic_frame().cols();
            if(sigma.rows()!=n || sigma.cols()!=n)throw std::invalid_argument("harmonic determinant coefficient carrier dimension");
            if(coefficient_rank.rank()==n)coefficient_factor.emplace(std::move(sigma),guard);
        }
    };
    std::shared_ptr<const State> state_;
public:
    PreparedHarmonicDeterminant(PreparedHodgeInvariants hodge,numeric::ExactMatrix sigma,Guard& guard)
        :state_(std::make_shared<const State>(std::move(hodge),std::move(sigma),guard)){}
    PreparedHarmonicDeterminant(PreparedHodgeInvariants hodge,numeric::ExactMatrix sigma,numeric::ExactActionLimits limits={}) {
        Guard guard(limits);state_=std::make_shared<const State>(std::move(hodge),std::move(sigma),guard);
    }
    numeric::Rational determinant(Guard& guard) const {
        const auto pin=state_;pin->coefficient_rank.validate(guard);
        const auto pseudo=pin->hodge.pseudodeterminant(guard),volume=pin->hodge.harmonic_volume(guard);
        const auto coefficient=pin->coefficient_factor?pin->coefficient_factor->factor_receipt(guard).determinant(guard):numeric::Rational{0};
        return guard.mul(guard.mul(pseudo,volume),coefficient);
    }
    numeric::Rational determinant(numeric::ExactActionLimits limits={}) const {Guard guard(limits);return determinant(guard);}
};

// Arbitrary exact volume factorizations reuse their Gram factors. The defining
// frame hypotheses, including independent Z and invertible R, are checked.
class PreparedMetricVolumeFactorization {
    using Guard=numeric::ExactActionGuard;
    struct State {
        numeric::ExactMatrix closed,filled,complement,coordinates,metric;
        numeric::PreparedExactSolve closed_gram,filled_gram,coordinate_factor;
        numeric::ExactMatrix harmonic_frame;
        numeric::PreparedExactSolve harmonic_gram;
        VolumeFactorizationResult result;
        State(numeric::ExactMatrix Z,numeric::ExactMatrix F,numeric::ExactMatrix W,numeric::ExactMatrix R,numeric::ExactMatrix M,Guard& guard)
            :closed(std::move(Z)),filled(std::move(F)),complement(std::move(W)),coordinates(std::move(R)),metric(std::move(M)),
             closed_gram(gram(closed,metric,guard),guard),filled_gram(gram(filled,metric,guard),guard),coordinate_factor(coordinates,guard),
             harmonic_frame(harmonic(filled,complement,metric,filled_gram,guard)),harmonic_gram(gram(harmonic_frame,metric,guard),guard) {
            hodge_frame_detail::positive(metric,guard);
            if(closed.rows()!=filled.rows() || closed.rows()!=complement.rows() || filled.cols()+complement.cols()!=closed.cols() ||
               coordinates.rows()!=closed.cols() || coordinates.cols()!=closed.cols() ||
               numeric::checked_multiply(closed,coordinates,guard)!=numeric::ExactMatrix::horizontal_concat(filled,complement))
                throw std::invalid_argument("volume factorization requires [F W]=Z R and complete independent coordinates");
            const auto gf=filled_gram.factor_receipt(guard).determinant(guard),gh=harmonic_gram.factor_receipt(guard).determinant(guard),
                       gz=closed_gram.factor_receipt(guard).determinant(guard),r=coordinate_factor.factor_receipt(guard).determinant(guard);
            result={harmonic_frame,gf,gh,gz,guard.mul(r,r),guard.mul(gf,gh)==guard.mul(guard.mul(r,r),gz)};
            if(!result.exact_identity)throw std::logic_error("prepared metric volume identity failed");
        }
        static numeric::ExactMatrix gram(const numeric::ExactMatrix& f,const numeric::ExactMatrix& m,Guard& g) {
            return numeric::checked_multiply(f.transpose(),numeric::checked_multiply(m,f,g),g);
        }
        static numeric::ExactMatrix harmonic(const numeric::ExactMatrix& f,const numeric::ExactMatrix& w,const numeric::ExactMatrix& m,
                                             const numeric::PreparedExactSolve& solve,Guard& g) {
            const auto rhs=numeric::checked_multiply(f.transpose(),numeric::checked_multiply(m,w,g),g);
            const auto coordinates=hodge_frame_detail::solve_columns(solve,rhs,g);
            auto result=w;const auto projected=numeric::checked_multiply(f,coordinates,g);
            for(const auto& [ij,q]:projected.entries()){result.set(ij.first,ij.second,g.sub(result.get(ij.first,ij.second),q));g.entries(result.nonzeros());}
            return result;
        }
    };
    std::shared_ptr<const State> state_;
public:
    PreparedMetricVolumeFactorization(numeric::ExactMatrix Z,numeric::ExactMatrix F,numeric::ExactMatrix W,
        numeric::ExactMatrix R,numeric::ExactMatrix M,Guard& guard)
        :state_(std::make_shared<const State>(std::move(Z),std::move(F),std::move(W),std::move(R),std::move(M),guard)){}
    PreparedMetricVolumeFactorization(numeric::ExactMatrix Z,numeric::ExactMatrix F,numeric::ExactMatrix W,
        numeric::ExactMatrix R,numeric::ExactMatrix M,numeric::ExactActionLimits limits={}) {
        Guard guard(limits);state_=std::make_shared<const State>(std::move(Z),std::move(F),std::move(W),std::move(R),std::move(M),guard);
    }
    VolumeFactorizationResult result(Guard& guard) const {
        const auto pin=state_;for(const auto* m:{&pin->closed,&pin->filled,&pin->complement,&pin->coordinates,&pin->metric})guard.matrix(*m);
        for(const auto* solve:{&pin->closed_gram,&pin->filled_gram,&pin->coordinate_factor,&pin->harmonic_gram})(void)solve->factor_receipt(guard);
        guard.matrix(pin->result.harmonic_frame);
        for(const auto& q:{pin->result.filled_volume,pin->result.harmonic_volume,pin->result.closed_volume,pin->result.coordinate_volume})guard.value(q);
        return pin->result;
    }
    VolumeFactorizationResult result(numeric::ExactActionLimits limits={}) const {Guard guard(limits);return result(guard);}
};
} // namespace rfc::field
