#pragma once
#include "rfc/field/algebraic_metric.hpp"
#include "rfc/local/algebraic_reconstruction.hpp"

namespace rfc::field {
// A scalar comparison does not by itself compare selected metric domains.
// Both the selected root and the involution must intertwine along that map.
class AlgebraicMetricDomainComparison {
    struct State {
        numeric::AlgebraicFieldEmbedding scalars;
        AlgebraicMetricDomain source,target;
    };
public:
    static AlgebraicMetricDomainComparison declare(const numeric::AlgebraicFieldEmbedding& scalars,
        const AlgebraicMetricDomain& source,const AlgebraicMetricDomain& target) {
        if (!scalars.source().same_owner(source.field()) || !scalars.target().same_owner(target.field()))
            throw std::invalid_argument("metric comparison field owners differ");
        if (scalars.apply(source.involution().generator_image())!=target.involution().apply(scalars.generator_image()))
            throw std::invalid_argument("metric comparison involutions do not intertwine");
        const bool agrees=std::visit([&](const auto& selected) {
            using T=std::decay_t<decltype(selected)>;
            if (const auto* next=std::get_if<T>(&target.embedding())) return selected.agrees_with(scalars,*next);
            if constexpr (std::same_as<T,geometry::SelectedRealAlgebraicEmbedding>) {
                // Intertwining fixes the generator image under target conjugation.
                // Its two exact signs select the original isolated real root.
                const auto& interval=selected.certificate().interval;
                return target.certify_sign(scalars.generator_image()-target.field().scalar(interval.lower)).sign>0 &&
                       target.certify_sign(target.field().scalar(interval.upper)-scalars.generator_image()).sign>0;
            } else return false;
        },source.embedding());
        if (!agrees) throw std::invalid_argument("metric comparison selected roots disagree");
        return AlgebraicMetricDomainComparison(std::make_shared<const State>(State{scalars,source,target}));
    }
    const numeric::AlgebraicFieldEmbedding& scalar_comparison() const & noexcept { return state_->scalars; }
    const AlgebraicMetricDomain& source() const & noexcept { return state_->source; }
    const AlgebraicMetricDomain& target() const & noexcept { return state_->target; }
    const numeric::AlgebraicFieldEmbedding& scalar_comparison() const && = delete;
    const AlgebraicMetricDomain& source() const && = delete;
    const AlgebraicMetricDomain& target() const && = delete;
    bool same_owner(const AlgebraicMetricDomainComparison& other) const noexcept { return state_==other.state_; }
private:
    explicit AlgebraicMetricDomainComparison(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

// This additional receipt verifies C* H_target C = sigma(H_source) on the
// complete source carrier. It does not infer an inverse or a target projection.
struct AlgebraicMeasurementComparisonLimits { std::size_t max_coefficient_products{1000000}; };
class AlgebraicMeasurementComparison {
    struct State {
        AlgebraicMetricDomainComparison domains;
        local::AlgebraicCarrierComparison carrier;
        AlgebraicMeasurementForm source,target;
    };
public:
    static AlgebraicMeasurementComparison declare(const AlgebraicMetricDomainComparison& domains,
        const local::AlgebraicCarrierComparison& carrier,
        const AlgebraicMeasurementForm& source,const AlgebraicMeasurementForm& target,
        AlgebraicMeasurementComparisonLimits limits={}) {
        if (!carrier.scalar_comparison().same_owner(domains.scalar_comparison()) ||
            !source.domain().same_owner(domains.source()) || !target.domain().same_owner(domains.target()) ||
            !carrier.source().same_owner(source.carrier()) || !carrier.target().same_owner(target.carrier()))
            throw std::invalid_argument("measurement comparison declared owners differ");
        const auto n=carrier.source().rank(),m=carrier.target().rank(),d=target.domain().field().degree();
        if (machine::checked_size_mul(machine::checked_size_mul(n,m),machine::checked_size_add(n,m))>limits.max_coefficient_products)
            throw std::length_error("measurement comparison coefficient product budget");
        const auto count=machine::checked_size_mul(n,machine::checked_size_add(n,m));
        if (machine::checked_size_mul(count,machine::checked_size_mul(d,d))>carrier.target().limits().max_matrix_coordinates)
            throw std::length_error("measurement comparison coordinate budget");
        const auto& c=carrier.coefficients(); const auto& h=target.coefficient_form().coefficients();
        const auto zero=target.domain().field().scalar(numeric::Rational{0});
        std::vector<numeric::AlgebraicElement> weighted(machine::checked_size_mul(m,n),zero);
        for (std::size_t i=0;i<m;++i) for (std::size_t j=0;j<n;++j) for (std::size_t k=0;k<m;++k)
            weighted[i*n+j]=weighted[i*n+j]+h[i*m+k]*c[k*n+j];
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) {
            auto value=zero;
            for (std::size_t k=0;k<m;++k)
                value=value+domains.target().involution().apply(c[k*n+i])*weighted[k*n+j];
            if (value!=domains.scalar_comparison().apply(source.coefficient_form().coefficients()[i*n+j]))
                throw std::invalid_argument("complete measurement forms do not intertwine");
        }
        return AlgebraicMeasurementComparison(std::make_shared<const State>(State{domains,carrier,source,target}));
    }
    const AlgebraicMetricDomainComparison& domains() const & noexcept { return state_->domains; }
    const local::AlgebraicCarrierComparison& carrier() const & noexcept { return state_->carrier; }
    const AlgebraicMeasurementForm& source() const & noexcept { return state_->source; }
    const AlgebraicMeasurementForm& target() const & noexcept { return state_->target; }
    const AlgebraicMetricDomainComparison& domains() const && = delete;
    const local::AlgebraicCarrierComparison& carrier() const && = delete;
    const AlgebraicMeasurementForm& source() const && = delete;
    const AlgebraicMeasurementForm& target() const && = delete;
private:
    explicit AlgebraicMeasurementComparison(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
