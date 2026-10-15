#pragma once
#include "rfc/geometry/complex_algebraic_embedding.hpp"
#include "rfc/geometry/real_algebraic_embedding.hpp"
#include "rfc/local/algebraic_dual.hpp"
#include "rfc/numeric/algebraic_phase.hpp"
#include "rfc/field/exact_metric.hpp"

namespace rfc::field {
struct AlgebraicMetricLimits {
    std::size_t max_sign_refinements{128},max_factor_steps{1000000};
};
struct AlgebraicSignCertificate {
    numeric::AlgebraicElement value;
    geometry::ExactRealInterval interval;
    std::size_t refinements;
    int sign;
};

// Compatibility is a separate declaration: a field involution need not be
// conjugation at the selected embedding. Equal declarations have distinct owners.
class AlgebraicMetricDomain {
    using Embedding=std::variant<geometry::SelectedRealAlgebraicEmbedding,geometry::SelectedComplexAlgebraicEmbedding>;
    struct State { Embedding embedding; numeric::AlgebraicInvolution involution; AlgebraicMetricLimits limits; };
public:
    static AlgebraicMetricDomain real(const geometry::SelectedRealAlgebraicEmbedding& embedding,
        const numeric::AlgebraicInvolution& involution,AlgebraicMetricLimits limits={}) {
        if (!embedding.field().same_owner(involution.field())) throw std::invalid_argument("real metric domain field owners differ");
        if (involution.generator_image()!=embedding.field().generator())
            throw std::invalid_argument("real metric requires the identity field involution");
        return AlgebraicMetricDomain(std::make_shared<const State>(State{embedding,involution,limits}));
    }
    static AlgebraicMetricDomain hermitian(const geometry::SelectedComplexAlgebraicEmbedding& embedding,
        const numeric::AlgebraicInvolution& involution,AlgebraicMetricLimits limits={}) {
        if (!embedding.field().same_owner(involution.field())) throw std::invalid_argument("Hermitian metric domain field owners differ");
        const auto comparison=numeric::AlgebraicFieldEmbedding::declare(embedding.field(),embedding.field(),involution.generator_image());
        if (!embedding.agrees_with(comparison,embedding.conjugated()))
            throw std::invalid_argument("involution is not conjugation at the selected complex embedding");
        return AlgebraicMetricDomain(std::make_shared<const State>(State{embedding,involution,limits}));
    }
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return state_->involution.field(); }
    const numeric::AlgebraicInvolution& involution() const & noexcept { return state_->involution; }
    const AlgebraicMetricLimits& limits() const & noexcept { return state_->limits; }
    const Embedding& embedding() const & noexcept { return state_->embedding; }
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const numeric::AlgebraicInvolution& involution() const && = delete;
    const AlgebraicMetricLimits& limits() const && = delete;
    const Embedding& embedding() const && = delete;
    bool same_owner(const AlgebraicMetricDomain& other) const noexcept { return state_==other.state_; }
    bool is_real() const noexcept { return std::holds_alternative<geometry::SelectedRealAlgebraicEmbedding>(state_->embedding); }
    AlgebraicSignCertificate certify_sign(const numeric::AlgebraicElement& value) const {
        if (!field().owns(value)) throw std::invalid_argument("metric sign field owner differs");
        if (involution().apply(value)!=value) throw std::invalid_argument("metric sign requires an involution-fixed scalar");
        if (value.is_zero()) return {value,{numeric::Rational{0},numeric::Rational{0}},0,0};
        numeric::algebraic_detail::Work work{field().limits()};
        std::size_t n{};
        for (;;) {
            geometry::ExactRealInterval bounds{numeric::Rational{0},numeric::Rational{0}};
            if (const auto* real=std::get_if<geometry::SelectedRealAlgebraicEmbedding>(&state_->embedding))
                bounds=geometry::real_root_detail::enclosure(value.coordinates(),real->refined_interval(n),work);
            else {
                const auto& complex=std::get<geometry::SelectedComplexAlgebraicEmbedding>(state_->embedding);
                const auto observation=complex.observe(value); const auto refinement=complex.refine(n);
                const auto enclosure=observation.enclosure(refinement);
                bounds={work.checked(numeric::Rational(enclosure.center.real-enclosure.radius)),
                        work.checked(numeric::Rational(enclosure.center.real+enclosure.radius))};
            }
            if (bounds.lower>0) return {value,std::move(bounds),n,1};
            if (bounds.upper<0) return {value,std::move(bounds),n,-1};
            if (n==limits().max_sign_refinements) throw std::length_error("metric fixed-scalar sign budget exhausted");
            n=std::min(limits().max_sign_refinements,n ? machine::checked_size_mul(n,2) : std::size_t{1});
        }
    }
private:
    explicit AlgebraicMetricDomain(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

// Hermitian coefficients alone do not establish positivity or an inverse.
class AlgebraicMeasurementForm {
    struct State { AlgebraicMetricDomain domain; local::AlgebraicModuleMap form; };
public:
    static AlgebraicMeasurementForm declare(const AlgebraicMetricDomain& domain,const local::AlgebraicModuleMap& form) {
        if (!form.source().same_owner(form.target())) throw std::invalid_argument("measurement form requires one actual carrier");
        if (!domain.field().same_owner(form.source().field())) throw std::invalid_argument("measurement form field owner differs");
        const auto n=form.source().rank(); const auto& h=form.coefficients();
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j)
            if (h[i*n+j]!=domain.involution().apply(h[j*n+i])) throw std::invalid_argument("measurement form is not Hermitian");
        return AlgebraicMeasurementForm(std::make_shared<const State>(State{domain,form}));
    }
    const AlgebraicMetricDomain& domain() const & noexcept { return state_->domain; }
    const local::AlgebraicModule& carrier() const & noexcept { return state_->form.source(); }
    const local::AlgebraicModuleMap& coefficient_form() const & noexcept { return state_->form; }
    const AlgebraicMetricDomain& domain() const && = delete;
    const local::AlgebraicModule& carrier() const && = delete;
    const local::AlgebraicModuleMap& coefficient_form() const && = delete;
    numeric::AlgebraicElement inner(const local::AlgebraicModuleVector& x,const local::AlgebraicModuleVector& y) const {
        if (!carrier().same_owner(x.carrier())) throw std::invalid_argument("measurement input carrier owner differs");
        const auto image=state_->form.apply(y); auto result=domain().field().scalar(numeric::Rational{0});
        for (std::size_t i=0;i<carrier().rank();++i)
            result=result+domain().involution().apply(x.coordinates()[i])*image.coordinates()[i];
        return result;
    }
    numeric::AlgebraicElement quadrance(const local::AlgebraicModuleVector& x) const { return inner(x,x); }
    local::AlgebraicCovector as_covector(const local::AlgebraicModuleVector& x) const {
        if (!carrier().same_owner(x.carrier())) throw std::invalid_argument("metric dual input carrier owner differs");
        // m(x,.) is K-linear in its second slot, conjugate-linear in x.
        const auto image=state_->form.apply(x); std::vector<numeric::AlgebraicElement> row;
        row.reserve(carrier().rank());
        for (const auto& value:image.coordinates()) row.push_back(domain().involution().apply(value));
        return local::AlgebraicCovector::declare(carrier(),std::move(row));
    }
    AlgebraicMeasurementForm add(const AlgebraicMeasurementForm& other) const {
        if (!domain().same_owner(other.domain())) throw std::invalid_argument("measurement domain owners differ");
        return declare(domain(),state_->form.add(other.state_->form));
    }
    AlgebraicMeasurementForm pullback(const local::AlgebraicModuleMap& map) const {
        if (!carrier().same_owner(map.target())) throw std::invalid_argument("measurement pullback target owner differs");
        const auto slots=machine::checked_size_mul(machine::checked_size_mul(map.source().rank(),map.source().rank()),
            machine::checked_size_mul(domain().field().degree(),domain().field().degree()));
        if (slots>map.source().limits().max_matrix_coordinates) throw std::length_error("measurement pullback coordinate budget");
        const auto weighted=map.then(state_->form); const auto n=map.source().rank(),m=map.target().rank();
        std::vector<numeric::AlgebraicElement> out(machine::checked_size_mul(n,n),domain().field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) for (std::size_t k=0;k<m;++k)
            out[i*n+j]=out[i*n+j]+domain().involution().apply(map.coefficients()[k*n+i])*weighted.coefficients()[k*n+j];
        return declare(domain(),local::AlgebraicModuleMap::declare(map.source(),map.source(),std::move(out)));
    }
    // Explicit scalar restriction by Tr(bar(x) H y)/[K:Q]. It is generally
    // indefinite even when H is positive at one selected embedding.
    numeric::ExactMatrix rational_trace_form() const {
        const auto& field=domain().field(); const auto d=field.degree(),n=carrier().rank();
        const auto size=machine::checked_size_mul(n,d); numeric::ExactMatrix out(size,size);
        std::vector<numeric::AlgebraicElement> basis; basis.reserve(d);
        auto next=field.scalar(numeric::Rational{1});
        for (std::size_t i=0;i<d;++i) {basis.push_back(next); if (i+1<d) next=next*field.generator();}
        numeric::algebraic_detail::Work work{field.limits()};
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j)
            for (std::size_t a=0;a<d;++a) for (std::size_t b=0;b<d;++b)
                out.set(i*d+a,j*d+b,work.checked(numeric::Rational(
                    (domain().involution().apply(basis[a])*state_->form.coefficients()[i*n+j]*basis[b]).trace()/numeric::Rational(numeric::Integer(d)))));
        return out;
    }
private:
    explicit AlgebraicMeasurementForm(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

class AlgebraicModuleMetric {
    struct State {
        AlgebraicMeasurementForm form;
        std::vector<numeric::AlgebraicElement> lower;
        std::vector<AlgebraicSignCertificate> pivots;
    };
public:
    static AlgebraicModuleMetric certify(const AlgebraicMeasurementForm& form) {
        const auto n=form.carrier().rank(),count=machine::checked_size_mul(n,n);
        if (machine::checked_size_mul(count,n)>form.domain().limits().max_factor_steps)
            throw std::length_error("algebraic metric factor work budget");
        const auto& field=form.domain().field(); const auto& involution=form.domain().involution();
        std::vector<numeric::AlgebraicElement> lower(count,field.scalar(numeric::Rational{0}));
        std::vector<AlgebraicSignCertificate> pivots; pivots.reserve(n);
        const auto& h=form.coefficient_form().coefficients();
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<=i;++j) {
            auto value=h[i*n+j];
            for (std::size_t k=0;k<j;++k)
                value=value-lower[i*n+k]*pivots[k].value*involution.apply(lower[j*n+k]);
            if (i==j) {
                auto certificate=form.domain().certify_sign(value);
                if (certificate.sign!=1) throw std::invalid_argument("algebraic metric is not positive at its selected embedding");
                pivots.push_back(std::move(certificate)); lower[i*n+i]=field.scalar(numeric::Rational{1});
            } else lower[i*n+j]=value/pivots[j].value;
        }
        return AlgebraicModuleMetric(std::make_shared<const State>(State{form,std::move(lower),std::move(pivots)}));
    }
    static AlgebraicModuleMetric identity(const AlgebraicMetricDomain& domain,const local::AlgebraicModule& carrier) {
        return certify(AlgebraicMeasurementForm::declare(domain,local::AlgebraicModuleMap::identity(carrier)));
    }
    const AlgebraicMeasurementForm& measurement() const & noexcept { return state_->form; }
    const AlgebraicMetricDomain& domain() const & noexcept { return state_->form.domain(); }
    const local::AlgebraicModule& carrier() const & noexcept { return state_->form.carrier(); }
    const std::vector<numeric::AlgebraicElement>& lower_factor() const & noexcept { return state_->lower; }
    const std::vector<AlgebraicSignCertificate>& pivot_certificates() const & noexcept { return state_->pivots; }
    const AlgebraicMeasurementForm& measurement() const && = delete;
    const AlgebraicMetricDomain& domain() const && = delete;
    const local::AlgebraicModule& carrier() const && = delete;
    const std::vector<numeric::AlgebraicElement>& lower_factor() const && = delete;
    const std::vector<AlgebraicSignCertificate>& pivot_certificates() const && = delete;
    bool same_owner(const AlgebraicModuleMetric& other) const noexcept { return state_==other.state_; }
    numeric::AlgebraicElement inner(const local::AlgebraicModuleVector& x,const local::AlgebraicModuleVector& y) const { return measurement().inner(x,y); }
    numeric::AlgebraicElement quadrance(const local::AlgebraicModuleVector& x) const { return inner(x,x); }
    local::AlgebraicModuleVector apply(const local::AlgebraicModuleVector& value) const { return measurement().coefficient_form().apply(value); }
    // Native K triangular solves reuse the certified factors; no inverse or
    // selected-embedding projection is constructed on this execution path.
    local::AlgebraicModuleVector solve(const local::AlgebraicModuleVector& rhs) const {
        if (!carrier().same_owner(rhs.carrier())) throw std::invalid_argument("metric solve carrier owner differs");
        const auto n=carrier().rank(); auto out=rhs.coordinates(); const auto& l=state_->lower;
        for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<i;++j) out[i]=out[i]-l[i*n+j]*out[j];
        for (std::size_t i=0;i<n;++i) out[i]=out[i]/state_->pivots[i].value;
        for (std::size_t i=n;i-->0;) for (std::size_t j=i+1;j<n;++j)
            out[i]=out[i]-domain().involution().apply(l[j*n+i])*out[j];
        auto result=carrier().vector(std::move(out));
        const auto residual=apply(result);
        if (residual.coordinates()!=rhs.coordinates()) throw std::logic_error("algebraic metric solve residual invariant");
        return result;
    }
    local::AlgebraicModuleVector from_covector(const local::AlgebraicCovector& value) const {
        if (!carrier().same_owner(value.carrier())) throw std::invalid_argument("metric dual inverse carrier owner differs");
        std::vector<numeric::AlgebraicElement> coefficients; coefficients.reserve(carrier().rank());
        for (const auto& coefficient:value.coefficients()) coefficients.push_back(domain().involution().apply(coefficient));
        return solve(carrier().vector(std::move(coefficients)));
    }
    ExactMetric certify_rational_trace_metric() const { return ExactMetric(measurement().rational_trace_form()); }
private:
    explicit AlgebraicModuleMetric(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

inline local::AlgebraicModuleVector algebraic_adjoint_apply(const local::AlgebraicModuleMap& map,
    const AlgebraicModuleMetric& source,const AlgebraicModuleMetric& target,const local::AlgebraicModuleVector& value) {
    if (!source.carrier().same_owner(map.source()) || !target.carrier().same_owner(map.target()))
        throw std::invalid_argument("metric adjoint map carrier owners differ");
    if (!source.domain().same_owner(target.domain())) throw std::invalid_argument("metric adjoint domain owners differ");
    const auto dual=local::AlgebraicDualMap(map).apply(target.measurement().as_covector(value));
    return source.from_covector(dual);
}
inline local::AlgebraicModuleMap algebraic_metric_adjoint(const local::AlgebraicModuleMap& map,
    const AlgebraicModuleMetric& source,const AlgebraicModuleMetric& target) {
    if (!source.carrier().same_owner(map.source()) || !target.carrier().same_owner(map.target()))
        throw std::invalid_argument("metric adjoint map carrier owners differ");
    if (!source.domain().same_owner(target.domain())) throw std::invalid_argument("metric adjoint domain owners differ");
    const auto n=map.source().rank(),m=map.target().rank(); const auto& field=source.domain().field();
    std::vector<numeric::AlgebraicElement> coefficients(machine::checked_size_mul(n,m),field.scalar(numeric::Rational{0}));
    for (std::size_t j=0;j<m;++j) {
        std::vector<numeric::AlgebraicElement> basis(m,field.scalar(numeric::Rational{0})); basis[j]=field.scalar(numeric::Rational{1});
        const auto column=algebraic_adjoint_apply(map,source,target,map.target().vector(std::move(basis)));
        for (std::size_t i=0;i<n;++i) coefficients[i*m+j]=column.coordinates()[i];
    }
    return local::AlgebraicModuleMap::declare(map.target(),map.source(),std::move(coefficients));
}
} // namespace rfc::field
