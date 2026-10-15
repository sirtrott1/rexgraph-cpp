#pragma once
#include "rfc/field/prepared_hodge_frames.hpp"

namespace rfc::field {
struct MetricRelativeNovelty {
    numeric::RationalVector transported_old,innovation,persistent_coordinates,persistent,complement,reconstructed;
    numeric::ExactMatrix pairings;
    numeric::Rational quadrance;
};
struct MetricCausePairings {
    std::vector<numeric::RationalVector> channels;
    numeric::RationalVector aggregate;
    numeric::ExactMatrix pairings;
    numeric::Rational quadrance;
};
namespace metric_novelty_detail {
using Matrix=numeric::ExactMatrix;
using Vector=numeric::RationalVector;
using Q=numeric::Rational;
using Guard=numeric::ExactActionGuard;
inline std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
inline std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
// Conservative envelopes for named rational containers, including copies and
// factor scratch. They do not measure allocator metadata, GMP limbs or RSS.
inline std::size_t preparation_workspace(std::size_t n,std::size_t m) {
    return add(1,add(mul(6,add(mul(n,n),mul(m,m))),add(mul(4,mul(n,m)),mul(8,add(n,m)))));
}
inline std::size_t action_workspace(std::size_t n,std::size_t m,std::size_t channels) {
    return add(1,add(mul(18,n),add(mul(add(24,mul(3,channels)),m),mul(channels,channels))));
}
inline Vector sub(const Vector& a,const Vector& b,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("metric novelty vector dimensions");
    Vector out(a.size());for(std::size_t i=0;i<a.size();++i)out[i]=guard.sub(a[i],b[i]);return out;
}
inline Q dot(const Vector& a,const Vector& b,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("metric pairing vector dimensions");
    Q out;for(std::size_t i=0;i<a.size();++i)out=guard.add(out,guard.mul(a[i],b[i]));
    guard.value(out);return out;
}
inline Matrix gram(const Matrix& metric,const std::vector<Vector>& channels,Guard& guard) {
    guard.dimension(channels.size());std::vector<Vector> measured;measured.reserve(channels.size());
    for(const auto& channel:channels)measured.push_back(numeric::checked_apply(metric,channel,guard));
    Matrix result(channels.size(),channels.size());
    for(std::size_t i=0;i<channels.size();++i)for(std::size_t j=0;j<=i;++j){
        const auto value=dot(channels[i],measured[j],guard);result.set(i,j,value);result.set(j,i,value);
    }
    return result;
}
} // namespace metric_novelty_detail

// The complete declared J is injective and M is positive definite. An exact
// Gram factor certifies independence; left-inverse actions use solves. No old
// metric, adjoint, source birth, projector matrix or inverse is inferred.
class PreparedMetricNovelty {
    using Matrix=numeric::ExactMatrix;
    using Vector=numeric::RationalVector;
    using Guard=numeric::ExactActionGuard;
    struct State {
        Matrix correspondence,metric,dual;
        std::optional<numeric::PreparedExactSolve> gram;
        std::size_t retained{},indices{},preparation{};
        State(Matrix j,Matrix m,Guard& guard):correspondence(std::move(j)),metric(std::move(m)) {
            using namespace metric_novelty_detail;
            const auto start=guard.steps();guard.step();
            const auto n=correspondence.cols(),size=correspondence.rows();
            if(metric.rows()!=size || metric.cols()!=size)throw std::invalid_argument("metric novelty target metric dimensions");
            if(n>size)throw std::invalid_argument("metric novelty correspondence must be injective");
            guard.matrix(correspondence);guard.matrix(metric);
            guard.entries(add(add(correspondence.nonzeros(),metric.nonzeros()),preparation_workspace(n,size)));
            hodge_frame_detail::positive(metric,guard);
            dual=numeric::checked_multiply(correspondence.transpose(),metric,guard);
            // Under M > 0 this matrix is nonsingular exactly when J is injective.
            gram.emplace(numeric::checked_multiply(dual,correspondence,guard),guard);
            retained=add(add(correspondence.nonzeros(),metric.nonzeros()),add(dual.nonzeros(),gram->stored_entries()));
            indices=gram->stored_index_entries();guard.entries(add(retained,preparation_workspace(n,size)));guard.entries(indices);
            preparation=guard.steps()-start;
        }
    };
public:
    PreparedMetricNovelty(const PreparedMetricNovelty&)=default;
    PreparedMetricNovelty& operator=(const PreparedMetricNovelty&)=default;
    PreparedMetricNovelty(Matrix correspondence,Matrix metric,Guard& guard)
        :state_(std::make_shared<const State>(std::move(correspondence),std::move(metric),guard)){}
    PreparedMetricNovelty(Matrix correspondence,Matrix metric,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) {
        Guard guard(limits,std::move(checkpoint));state_=std::make_shared<const State>(std::move(correspondence),std::move(metric),guard);
    }
    const Matrix& correspondence() const & {return state_->correspondence;}
    const Matrix& metric() const & {return state_->metric;}
    const Matrix& pulled_back_metric() const & {return state_->gram->matrix();}
    const Matrix& correspondence() const &&=delete;
    const Matrix& metric() const &&=delete;
    const Matrix& pulled_back_metric() const &&=delete;
    std::size_t old_dimension() const noexcept {return state_->correspondence.cols();}
    std::size_t new_dimension() const noexcept {return state_->correspondence.rows();}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t stored_index_entries() const noexcept {return state_->indices;}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    std::size_t workspace_slots(std::size_t channels=2) const {
        return metric_novelty_detail::action_workspace(old_dimension(),new_dimension(),channels);
    }
    numeric::RationalVector left_inverse(Vector input,Guard& guard) const {
        const auto pin=state_;validate(*pin,0,guard);
        return coordinates(*pin,std::move(input),guard);
    }
    numeric::RationalVector left_inverse(Vector input,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) const {
        Guard guard(limits,std::move(checkpoint));return left_inverse(std::move(input),guard);
    }
    MetricRelativeNovelty decompose(Vector old_field,Vector new_field,Guard& guard) const {
        using namespace metric_novelty_detail;
        const auto pin=state_;validate(*pin,2,guard);
        if(old_field.size()!=pin->correspondence.cols() || new_field.size()!=pin->correspondence.rows())
            throw std::invalid_argument("metric novelty complete field dimensions");
        const auto x=guard.canonical_input(old_field),xp=guard.canonical_input(new_field);
        auto transported=numeric::checked_apply(pin->correspondence,x,guard);
        auto innovation=sub(xp,transported,guard);auto lifted=coordinates(*pin,xp,guard);
        auto change=sub(lifted,x,guard);auto persistent=numeric::checked_apply(pin->correspondence,change,guard);
        auto complement=sub(xp,numeric::checked_apply(pin->correspondence,lifted,guard),guard);
        Vector reconstructed(xp.size());for(std::size_t i=0;i<xp.size();++i)reconstructed[i]=guard.add(persistent[i],complement[i]);
        if(reconstructed!=innovation)throw std::logic_error("metric novelty reconstruction failed");
        auto pairings=metric_novelty_detail::gram(pin->metric,{persistent,complement},guard);
        const auto total=dot(innovation,numeric::checked_apply(pin->metric,innovation,guard),guard);
        if(pairings.get(0,1)!=0 || guard.add(pairings.get(0,0),pairings.get(1,1))!=total)
            throw std::logic_error("metric novelty orthogonal budget failed");
        for(const auto* value:{&transported,&innovation,&change,&persistent,&complement,&reconstructed})guard.vector(*value);
        guard.value(total);
        return {std::move(transported),std::move(innovation),std::move(change),std::move(persistent),std::move(complement),
                std::move(reconstructed),std::move(pairings),total};
    }
    MetricRelativeNovelty decompose(Vector old_field,Vector new_field,numeric::ExactActionLimits limits={},std::function<void()> checkpoint={}) const {
        Guard guard(limits,std::move(checkpoint));return decompose(std::move(old_field),std::move(new_field),guard);
    }
    MetricCausePairings pair_causes(std::vector<Vector> channels,Vector expected,Guard& guard) const {
        using namespace metric_novelty_detail;
        const auto pin=state_;validate(*pin,channels.size(),guard);
        if(expected.size()!=pin->metric.rows())throw std::invalid_argument("metric cause aggregate dimensions");
        expected=guard.canonical_input(expected);Vector aggregate(expected.size());
        for(auto& value:channels){
            if(value.size()!=expected.size())throw std::invalid_argument("metric cause complete channel dimensions");
            value=guard.canonical_input(value);
            for(std::size_t i=0;i<value.size();++i)aggregate[i]=guard.add(aggregate[i],value[i]);
        }
        if(aggregate!=expected)throw std::invalid_argument("declared cause channels must sum to the complete innovation");
        auto pairings=metric_novelty_detail::gram(pin->metric,channels,guard);
        const auto total=dot(aggregate,numeric::checked_apply(pin->metric,aggregate,guard),guard);
        numeric::Rational contraction;for(std::size_t i=0;i<channels.size();++i)for(std::size_t j=0;j<channels.size();++j)
            contraction=guard.add(contraction,pairings.get(i,j));
        guard.value(contraction);if(contraction!=total)throw std::logic_error("complete cause Gram contraction failed");
        guard.vector(aggregate);return {std::move(channels),std::move(aggregate),std::move(pairings),total};
    }
    MetricCausePairings pair_causes(std::vector<Vector> channels,Vector expected,numeric::ExactActionLimits limits={},
                                  std::function<void()> checkpoint={}) const {
        Guard guard(limits,std::move(checkpoint));return pair_causes(std::move(channels),std::move(expected),guard);
    }
private:
    static void validate(const State& state,std::size_t channels,Guard& guard) {
        using namespace metric_novelty_detail;
        guard.step();guard.dimension(channels);
        guard.entries(add(state.retained,action_workspace(state.correspondence.cols(),state.correspondence.rows(),channels)));
        guard.entries(state.indices);guard.matrix(state.correspondence);guard.matrix(state.metric);guard.matrix(state.dual);
        (void)state.gram->factor_receipt(guard);
    }
    static Vector coordinates(const State& state,Vector input,Guard& guard) {
        if(input.size()!=state.metric.rows())throw std::invalid_argument("metric left inverse complete input dimensions");
        const auto x=guard.canonical_input(input);
        auto result=state.gram->apply(numeric::checked_apply(state.dual,x,guard),guard);guard.vector(result);return result;
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
