#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include "rfc/field/factored_action.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::statistics {

struct EffectiveDimension2 {
    numeric::Rational variance{0};
    numeric::Rational second_contraction{0};
    numeric::Rational dimension{0};
    numeric::Rational collision_probability{0};
};

inline numeric::Rational trace_product(const numeric::ExactMatrix& lhs,
                                       const numeric::ExactMatrix& rhs) {
    if (lhs.cols()!=rhs.rows() || lhs.rows()!=rhs.cols()) throw std::invalid_argument("exact trace product shape mismatch");
    numeric::Rational result=0;
    for(const auto& [key,value]:lhs.entries()) {
        const auto other=rhs.get(key.second,key.first);
        if(other!=0) result+=value*other;
    }
    return result;
}

// The hypotheses of the dimension/coupling theorems are checked once and
// retained. Scalar contractions use action chains, not covariance eigenvectors
// or full products G Sigma, G Sigma G Sigma.
class PreparedEnsembleMoments {
    using Guard=numeric::ExactActionGuard;
    struct State {
        std::size_t start;
        numeric::PreparedPositiveSemidefinite metric,covariance;
        EffectiveDimension2 result;
        std::size_t preparation{};
        State(numeric::ExactMatrix g,numeric::ExactMatrix s,Guard& guard):start(guard.steps()),metric(std::move(g),guard),covariance(std::move(s),guard) {
            const auto n=metric.original().rows();
            if(metric.rank()!=n || covariance.original().rows()!=n)throw std::invalid_argument("ensemble requires SPD metric and same-carrier PSD covariance");
            const auto action=field::FactoredAction::matrix(covariance.original()).then(field::FactoredAction::matrix(metric.original()));
            numeric::Rational variance=0,second=0;
            for(const auto& [key,q]:metric.original().entries())variance=guard.add(variance,guard.mul(q,covariance.original().get(key.second,key.first)));
            std::set<std::size_t> active;
            for(const auto& [key,q]:covariance.original().entries()){(void)q;guard.step();active.insert(key.second);}
            for(const auto i:active){
                const auto twice=action.apply(action.apply(numeric::SparseRationalVector::basis(n,i),guard),guard);
                for(const auto& entry:twice.entries())if(entry.index==i)second=guard.add(second,entry.value);
            }
            result.variance=variance;result.second_contraction=second;
            if(variance!=0){
                if(second<=0)throw std::logic_error("nonzero PSD covariance has nonpositive second contraction");
                result.dimension=guard.div(guard.mul(variance,variance),second);
                result.collision_probability=guard.div(1,result.dimension);
                if(result.dimension<1 || result.dimension>numeric::Rational{static_cast<unsigned long>(covariance.rank())})
                    throw std::logic_error("certified effective dimension rank bound failed");
            }
            preparation=guard.steps()-start;
        }
    };
    std::shared_ptr<const State> state_;
    static void validate(const numeric::PreparedPositiveSemidefinite& p,Guard& guard) {
        guard.matrix(p.original());
        guard.matrix(p.unit_lower_entries());
        for(const auto& [key,q]:p.diagonal_entries()){(void)key;guard.step();guard.value(q);}
    }
public:
    PreparedEnsembleMoments(numeric::ExactMatrix metric,numeric::ExactMatrix covariance,Guard& guard)
        :state_(std::make_shared<const State>(std::move(metric),std::move(covariance),guard)){}
    PreparedEnsembleMoments(numeric::ExactMatrix metric,numeric::ExactMatrix covariance,numeric::ExactActionLimits limits={}) {
        Guard guard(limits);state_=std::make_shared<const State>(std::move(metric),std::move(covariance),guard);
    }
    std::size_t covariance_rank() const noexcept {return state_->covariance.rank();}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    EffectiveDimension2 contractions(Guard& guard) const {
        const auto pin=state_;validate(pin->metric,guard);validate(pin->covariance,guard);
        guard.value(pin->result.variance);guard.value(pin->result.second_contraction);guard.value(pin->result.dimension);guard.value(pin->result.collision_probability);
        if(pin->result.variance==0)throw std::invalid_argument("effective dimension requires nonzero covariance");
        return pin->result;
    }
    EffectiveDimension2 contractions(numeric::ExactActionLimits limits={}) const {Guard guard(limits);return contractions(guard);}
};

inline EffectiveDimension2 effective_dimension_2(const numeric::ExactMatrix& metric,
                                                  const numeric::ExactMatrix& covariance) {
    return PreparedEnsembleMoments(metric,covariance).contractions();
}

inline numeric::Rational cross_sector_coupling(const numeric::ExactMatrix& metric_s,
                                               const numeric::ExactMatrix& sigma_ss,
                                               const numeric::ExactMatrix& sigma_st,
                                               const numeric::ExactMatrix& metric_t,
                                               const numeric::ExactMatrix& sigma_tt,
                                               const numeric::ExactMatrix& sigma_ts) {
    if(metric_s.rows()!=metric_s.cols() || metric_t.rows()!=metric_t.cols() ||
       sigma_ss.rows()!=metric_s.rows() || sigma_ss.cols()!=metric_s.cols() ||
       sigma_tt.rows()!=metric_t.rows() || sigma_tt.cols()!=metric_t.cols() ||
       sigma_st.rows()!=metric_s.rows() || sigma_st.cols()!=metric_t.rows() ||
       sigma_ts.rows()!=metric_t.rows() || sigma_ts.cols()!=metric_s.rows()) {
        throw std::invalid_argument("cross-sector coupling shape mismatch");
    }
    if(sigma_ts!=sigma_st.transpose())throw std::invalid_argument("joint covariance cross blocks must be transposes");
    numeric::ExactActionGuard guard;
    const auto vs=PreparedEnsembleMoments(metric_s,sigma_ss,guard).contractions(guard).variance;
    const auto vt=PreparedEnsembleMoments(metric_t,sigma_tt,guard).contractions(guard).variance;
    const auto n=metric_s.rows(),m=metric_t.rows(),dimension=machine::checked_size_add(n,m);numeric::ExactMatrix joint(dimension,dimension);
    const auto put=[&](const numeric::ExactMatrix& a,std::size_t r,std::size_t c){for(const auto& [key,q]:a.entries())joint.set(r+key.first,c+key.second,q);};
    put(sigma_ss,0,0);put(sigma_st,0,n);put(sigma_ts,n,0);put(sigma_tt,n,n);
    const numeric::PreparedPositiveSemidefinite joint_certificate(std::move(joint),guard);
    (void)joint_certificate;
    const auto product=field::FactoredAction::matrix(sigma_ts).then(field::FactoredAction::matrix(metric_t))
        .then(field::FactoredAction::matrix(sigma_st)).then(field::FactoredAction::matrix(metric_s));
    numeric::Rational numerator=0;
    std::set<std::size_t> active;for(const auto& [key,q]:sigma_ts.entries()){(void)q;guard.step();active.insert(key.second);}
    for(const auto i:active){const auto value=product.apply(numeric::SparseRationalVector::basis(n,i),guard);
        for(const auto& entry:value.entries())if(entry.index==i)numerator=guard.add(numerator,entry.value);}
    const auto result=guard.div(numerator,guard.mul(vs,vt));
    if(result<0 || result>1)throw std::logic_error("certified cross-sector coupling bound failed");
    return result;
}

} // namespace rfc::statistics
