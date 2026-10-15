#pragma once
#include "rfc/numeric/algebraic_embedding.hpp"
#include <optional>

namespace rfc::geometry {
struct ExactRealInterval { numeric::Rational lower,upper; };
struct RealRootLimits { std::size_t max_bisections{4096}; };
struct RealRootCertificate {
    ExactRealInterval interval;
    std::vector<numeric::RationalVector> sturm_sequence;
    std::size_t lower_variations{},upper_variations{};
};
namespace real_root_detail {
using Q=numeric::Rational;
using P=numeric::RationalVector;
using Work=numeric::algebraic_detail::Work;
inline Q evaluate(const P& p,const Q& x,Work& work) {
    Q result{};
    for (auto i=p.rbegin();i!=p.rend();++i) result=work.checked(Q(result*x+*i));
    return result;
}
inline std::vector<P> sturm(const P& f,Work& work) {
    P derivative(f.size()-1);
    for (std::size_t i=1;i<f.size();++i)
        derivative[i-1]=work.checked(Q(f[i]*numeric::Integer(std::to_string(i))));
    std::vector<P> result{f,std::move(derivative)};
    while (result.back().size()>1) {
        auto remainder=numeric::algebraic_detail::divide(result[result.size()-2],result.back(),work).second;
        if (remainder.empty()) throw std::logic_error("certified field polynomial is not squarefree");
        for (auto& value:remainder) value=-value;
        result.push_back(std::move(remainder));
    }
    return result;
}
inline std::size_t variations(const std::vector<P>& sequence,const Q& x,Work& work) {
    int previous{}; std::size_t changes{};
    for (const auto& p:sequence) {
        const auto value=evaluate(p,x,work); const int sign=mpq_sgn(value.get_mpq_t());
        if (!sign) continue;
        if (previous && sign!=previous) ++changes;
        previous=sign;
    }
    return changes;
}
inline RealRootCertificate certificate(const numeric::CertifiedAlgebraicField& field,Q lower,Q upper,Work& work) {
    lower=numeric::canonical_rational(std::move(lower)); upper=numeric::canonical_rational(std::move(upper));
    work.admit(lower); work.admit(upper);
    if (lower>=upper) throw std::invalid_argument("real root interval must be ordered and nonempty");
    const auto& f=field.minimal_polynomial();
    if (evaluate(f,lower,work)==0 || evaluate(f,upper,work)==0)
        throw std::invalid_argument("real root interval endpoints must not be roots");
    auto sequence=sturm(f,work);
    const auto left=variations(sequence,lower,work),right=variations(sequence,upper,work);
    if (left<right) throw std::logic_error("Sturm variation monotonicity invariant");
    return {{std::move(lower),std::move(upper)},std::move(sequence),left,right};
}
inline ExactRealInterval enclosure(const P& p,const ExactRealInterval& x,Work& work) {
    ExactRealInterval out{Q{0},Q{0}};
    for (auto i=p.rbegin();i!=p.rend();++i) {
        const Q a=work.checked(Q(out.lower*x.lower)),b=work.checked(Q(out.lower*x.upper));
        const Q c=work.checked(Q(out.upper*x.lower)),d=work.checked(Q(out.upper*x.upper));
        out={work.checked(Q(std::min({a,b,c,d})+*i)),work.checked(Q(std::max({a,b,c,d})+*i))};
    }
    return out;
}
inline ExactRealInterval bisect(const P& f,ExactRealInterval interval,Work& work) {
    if (interval.lower==interval.upper) return interval;
    const auto middle=work.checked(Q((interval.lower+interval.upper)/2));
    const auto value=evaluate(f,middle,work);
    if (value==0) return {middle,middle}; // Degree-one fields can have a rational midpoint root.
    if ((value>0)==(evaluate(f,interval.lower,work)>0)) interval.lower=middle;
    else interval.upper=middle;
    return interval;
}
} // namespace real_root_detail

// This is a selected real embedding of an already certified field. Its rational
// interval is a root certificate, never a replacement floating coefficient.
class SelectedRealAlgebraicEmbedding {
    struct State { numeric::CertifiedAlgebraicField field; RealRootCertificate certificate; RealRootLimits limits; };
public:
    SelectedRealAlgebraicEmbedding(const SelectedRealAlgebraicEmbedding&)=default;
    SelectedRealAlgebraicEmbedding& operator=(const SelectedRealAlgebraicEmbedding&)=default;
    static SelectedRealAlgebraicEmbedding isolate(const numeric::CertifiedAlgebraicField& field,
        numeric::Rational lower,numeric::Rational upper,RealRootLimits limits={}) {
        numeric::algebraic_detail::Work work{field.limits()};
        auto certificate=real_root_detail::certificate(field,std::move(lower),std::move(upper),work);
        if (certificate.lower_variations-certificate.upper_variations!=1)
            throw std::invalid_argument("selected real interval must contain exactly one root");
        return SelectedRealAlgebraicEmbedding(std::make_shared<const State>(State{field,std::move(certificate),limits}));
    }
    template<class A,class B> requires (std::floating_point<A> || std::floating_point<B>)
    static SelectedRealAlgebraicEmbedding isolate(const numeric::CertifiedAlgebraicField&,A,B,RealRootLimits={})=delete;
    static std::size_t root_count(const numeric::CertifiedAlgebraicField& field,numeric::Rational lower,numeric::Rational upper) {
        numeric::algebraic_detail::Work work{field.limits()};
        const auto proof=real_root_detail::certificate(field,std::move(lower),std::move(upper),work);
        return proof.lower_variations-proof.upper_variations;
    }
    template<class A,class B> requires (std::floating_point<A> || std::floating_point<B>)
    static std::size_t root_count(const numeric::CertifiedAlgebraicField&,A,B)=delete;
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return state_->field; }
    const RealRootCertificate& certificate() const & noexcept { return state_->certificate; }
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const RealRootCertificate& certificate() const && = delete;
    bool same_owner(const SelectedRealAlgebraicEmbedding& other) const noexcept { return state_==other.state_; }
    ExactRealInterval refined_interval(std::size_t bisections) const {
        if (bisections>state_->limits.max_bisections) throw std::length_error("real root bisection budget");
        numeric::algebraic_detail::Work work{state_->field.limits()}; auto result=state_->certificate.interval;
        for (std::size_t i=0;i<bisections && result.lower!=result.upper;++i)
            result=real_root_detail::bisect(state_->field.minimal_polynomial(),std::move(result),work);
        return result;
    }
    template<std::floating_point T> ExactRealInterval refined_interval(T) const = delete;
    int sign(const numeric::AlgebraicElement& value) const {
        if (!state_->field.owns(value)) throw std::invalid_argument("selected real value owner differs");
        if (value.is_zero()) return 0;
        numeric::algebraic_detail::Work work{state_->field.limits()}; auto interval=state_->certificate.interval;
        for (std::size_t i=0;;++i) {
            const auto enclosure=real_root_detail::enclosure(value.coordinates(),interval,work);
            if (enclosure.lower>0) return 1;
            if (enclosure.upper<0) return -1;
            if (i==state_->limits.max_bisections) throw std::length_error("real sign isolation budget exhausted");
            interval=real_root_detail::bisect(state_->field.minimal_polynomial(),std::move(interval),work);
        }
    }
    int compare(const numeric::AlgebraicElement& a,const numeric::AlgebraicElement& b) const { return sign(a-b); }
    bool agrees_with(const numeric::AlgebraicFieldEmbedding& comparison,
                     const SelectedRealAlgebraicEmbedding& target) const {
        if (!state_->field.same_owner(comparison.source()) || !target.field().same_owner(comparison.target()))
            throw std::invalid_argument("selected embedding comparison owners differ");
        const auto& interval=state_->certificate.interval;
        const auto& image=comparison.generator_image();
        return target.sign(image-target.field().scalar(interval.lower))>0 &&
               target.sign(target.field().scalar(interval.upper)-image)>0;
    }
private:
    explicit SelectedRealAlgebraicEmbedding(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::geometry
