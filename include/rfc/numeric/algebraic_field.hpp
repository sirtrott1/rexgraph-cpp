#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/rational_function.hpp"
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <variant>

namespace rfc::numeric {

struct IrreducibleReduction {
    std::uint64_t prime;
    explicit IrreducibleReduction(std::uint64_t value) : prime(value) {}
    template<std::floating_point T> IrreducibleReduction(T)=delete;
};
struct EisensteinShift {
    std::uint64_t prime;
    Integer shift;
    EisensteinShift(std::uint64_t value,Integer translation) : prime(value),shift(std::move(translation)) {}
    template<class A,class B> requires (std::floating_point<A> || std::floating_point<B>)
    EisensteinShift(A,B)=delete;
};
using AlgebraicIrreducibility = std::variant<IrreducibleReduction,EisensteinShift>;
struct AlgebraicFieldLimits {
    std::size_t max_degree{32}, max_input_coefficients{256}, max_integer_bits{16384};
    std::size_t max_scalar_steps{1000000}, max_matrix_coordinates{4096};
};

namespace algebraic_detail {
using Polynomial = RationalVector;
inline void trim(Polynomial& p) { while (!p.empty() && p.back()==0) p.pop_back(); }
struct Work {
    const AlgebraicFieldLimits& limits;
    std::size_t steps{};
    void charge() {
        if (steps==limits.max_scalar_steps) throw std::length_error("algebraic scalar work budget");
        ++steps;
    }
    void admit(const Rational& q) const {
        if (mpz_sizeinbase(mpq_numref(q.get_mpq_t()),2)>limits.max_integer_bits ||
            mpz_sizeinbase(mpq_denref(q.get_mpq_t()),2)>limits.max_integer_bits)
            throw std::length_error("algebraic integer bit budget");
    }
    Rational checked(Rational q) { charge(); admit(q); return q; }
};
inline Polynomial input(const Polynomial& original, Work& work) {
    if (original.size()>work.limits.max_input_coefficients)
        throw std::length_error("algebraic input coefficient budget");
    auto p=canonical_vector(original);
    for (const auto& q:p) work.admit(q);
    trim(p); return p;
}
inline std::pair<Polynomial,Polynomial> divide(Polynomial a,const Polynomial& b,Work& work) {
    if (b.empty()) throw std::domain_error("zero algebraic polynomial divisor");
    Polynomial q(a.size()>=b.size()?a.size()-b.size()+1:0);
    while (!a.empty() && a.size()>=b.size()) {
        const auto shift=a.size()-b.size();
        const auto factor=work.checked(Rational(a.back()/b.back()));
        q[shift]=work.checked(Rational(q[shift]+factor));
        for (std::size_t j=0;j<b.size();++j)
            a[shift+j]=work.checked(Rational(a[shift+j]-factor*b[j]));
        trim(a);
    }
    trim(q); return {std::move(q),std::move(a)};
}
inline Polynomial multiply(const Polynomial& a,const Polynomial& b,Work& work) {
    if (a.empty() || b.empty()) return {};
    Polynomial out(machine::checked_size_add(a.size(),b.size())-1);
    for (std::size_t i=0;i<a.size();++i) for (std::size_t j=0;j<b.size();++j)
        out[i+j]=work.checked(Rational(out[i+j]+a[i]*b[j]));
    trim(out); return out;
}
inline Polynomial subtract(Polynomial a,const Polynomial& b,Work& work) {
    a.resize(std::max(a.size(),b.size()));
    for (std::size_t i=0;i<b.size();++i) a[i]=work.checked(Rational(a[i]-b[i]));
    trim(a); return a;
}
inline std::uint32_t prime(std::uint64_t candidate) {
    // Exact trial division on a deliberately bounded certificate domain.
    if (candidate<2 || candidate>65521) throw std::invalid_argument("algebraic certificate prime domain");
    const auto p=static_cast<std::uint32_t>(candidate);
    for (std::uint32_t d=2;d<=p/d;++d)
        if (p%d==0) throw std::invalid_argument("algebraic certificate requires a prime");
    return p;
}
using ModPolynomial=std::vector<std::uint32_t>;
inline void trim(ModPolynomial& a) { while (!a.empty() && a.back()==0) a.pop_back(); }
inline std::uint32_t mod_power(std::uint32_t a,std::uint32_t n,std::uint32_t p,Work& work) {
    std::uint64_t result=1,base=a;
    while (n) {
        if (n&1u) {work.charge(); result=result*base%p;}
        n>>=1u;
        if (n) {work.charge(); base=base*base%p;}
    }
    return static_cast<std::uint32_t>(result);
}
inline ModPolynomial remainder(ModPolynomial a,const ModPolynomial& b,std::uint32_t p,Work& work) {
    if (b.empty()) throw std::domain_error("zero modular polynomial divisor");
    const auto inv=mod_power(b.back(),p-2,p,work);
    while (!a.empty() && a.size()>=b.size()) {
        const auto offset=a.size()-b.size();
        const auto factor=static_cast<std::uint32_t>(static_cast<std::uint64_t>(a.back())*inv%p);
        for (std::size_t j=0;j<b.size();++j) {
            work.charge();
            a[offset+j]=static_cast<std::uint32_t>((a[offset+j]+p-static_cast<std::uint64_t>(factor)*b[j]%p)%p);
        }
        trim(a);
    }
    return a;
}
inline ModPolynomial mod_multiply(const ModPolynomial& a,const ModPolynomial& b,
                                  const ModPolynomial& f,std::uint32_t p,Work& work) {
    if (a.empty() || b.empty()) return {};
    ModPolynomial out(machine::checked_size_add(a.size(),b.size())-1);
    for (std::size_t i=0;i<a.size();++i) for (std::size_t j=0;j<b.size();++j) {
        work.charge(); out[i+j]=static_cast<std::uint32_t>((out[i+j]+static_cast<std::uint64_t>(a[i])*b[j])%p);
    }
    trim(out); return remainder(std::move(out),f,p,work);
}
inline ModPolynomial frobenius(ModPolynomial base,const ModPolynomial& f,std::uint32_t p,Work& work) {
    ModPolynomial result{1}; auto exponent=p;
    while (exponent) {
        if (exponent&1u) result=mod_multiply(result,base,f,p,work);
        exponent>>=1u;
        if (exponent) base=mod_multiply(base,base,f,p,work);
    }
    return result;
}
inline ModPolynomial gcd(ModPolynomial a,ModPolynomial b,std::uint32_t p,Work& work) {
    while (!b.empty()) {auto r=remainder(a,b,p,work); a=std::move(b); b=std::move(r);}
    return a; // Only its degree matters to the certificate.
}
inline void certify(const Polynomial& f,const AlgebraicIrreducibility& certificate,Work& work) {
    std::visit([&](const auto& selected) {
        const auto p=prime(selected.prime);
        if constexpr (std::same_as<std::decay_t<decltype(selected)>,IrreducibleReduction>) {
            ModPolynomial reduced; reduced.reserve(f.size());
            for (const auto& q:f) {
                const auto den=static_cast<std::uint32_t>(mpz_fdiv_ui(mpq_denref(q.get_mpq_t()),p));
                if (!den) throw std::invalid_argument("certificate prime divides a coefficient denominator");
                const auto num=mpz_fdiv_ui(mpq_numref(q.get_mpq_t()),p);
                reduced.push_back(static_cast<std::uint32_t>(num*mod_power(den,p-2,p,work)%p));
            }
            const auto degree=f.size()-1;
            const auto x=remainder(ModPolynomial{0,1},reduced,p,work);
            auto h=x;
            for (std::size_t k=1;k<=degree;++k) {
                h=frobenius(std::move(h),reduced,p,work);
                auto difference=h; difference.resize(std::max(h.size(),x.size()));
                for (std::size_t j=0;j<x.size();++j) difference[j]=(difference[j]+p-x[j])%p;
                trim(difference);
                if (k<=degree/2 && gcd(reduced,difference,p,work).size()!=1)
                    throw std::invalid_argument("polynomial fails its declared irreducible reduction certificate");
                if (k==degree && !difference.empty())
                    throw std::invalid_argument("polynomial fails its declared Frobenius certificate");
            }
        } else {
            if (mpz_sizeinbase(selected.shift.get_mpz_t(),2)>work.limits.max_integer_bits)
                throw std::length_error("Eisenstein shift integer bit budget");
            Polynomial shifted;
            for (auto i=f.rbegin();i!=f.rend();++i) {
                shifted=multiply(shifted,Polynomial{Rational(selected.shift),Rational{1}},work);
                if (shifted.empty()) shifted.resize(1);
                shifted[0]=work.checked(Rational(shifted[0]+*i));
                trim(shifted);
            }
            for (std::size_t i=0;i+1<shifted.size();++i)
                if (shifted[i].get_den()!=1 || mpz_fdiv_ui(mpq_numref(shifted[i].get_mpq_t()),p)!=0)
                    throw std::invalid_argument("polynomial fails its declared shifted Eisenstein certificate");
            const auto square=static_cast<unsigned long>(p)*p;
            if (mpz_fdiv_ui(mpq_numref(shifted[0].get_mpq_t()),square)==0)
                throw std::invalid_argument("Eisenstein constant is divisible by the prime squared");
        }
    },certificate);
}

struct FieldState {
    Polynomial polynomial;
    AlgebraicIrreducibility certificate;
    AlgebraicFieldLimits limits;
    std::size_t degree() const noexcept { return polynomial.size()-1; }
    Polynomial reduced(Polynomial a,Work& work) const {
        trim(a); a=divide(std::move(a),polynomial,work).second;
        a.resize(degree()); return a;
    }
    Polynomial product(const Polynomial& a,const Polynomial& b,Work& work) const {
        return reduced(multiply(a,b,work),work);
    }
};
} // namespace algebraic_detail

class CertifiedAlgebraicField;
// Fixed power-basis coefficients with a pinned, actually declared field owner.
// Equal polynomials or coordinate arrays do not identify different domains.
class AlgebraicElement {
public:
    AlgebraicElement(const AlgebraicElement&)=default;
    AlgebraicElement& operator=(const AlgebraicElement&)=default;
    const RationalVector& coordinates() const & noexcept { return coordinates_; }
    const RationalVector& coordinates() const && = delete;
    bool is_zero() const noexcept { return numeric::is_zero(coordinates_); }
    bool same_field(const AlgebraicElement& other) const noexcept { return state_==other.state_; }
    CertifiedAlgebraicField field() const;
    AlgebraicElement inverse() const {
        if (is_zero()) throw std::domain_error("inverse of zero algebraic element");
        algebraic_detail::Work work{state_->limits};
        auto a=state_->polynomial,b=coordinates_; algebraic_detail::trim(b);
        RationalVector u,v{Rational{1}};
        while (!b.empty()) {
            auto [q,r]=algebraic_detail::divide(a,b,work);
            auto next=algebraic_detail::subtract(u,algebraic_detail::multiply(q,v,work),work);
            a=std::move(b); b=std::move(r); u=std::move(v); v=std::move(next);
        }
        if (a.size()!=1) throw std::logic_error("certified algebraic field inverse invariant");
        for (auto& coefficient:u) coefficient=work.checked(Rational(coefficient/a[0]));
        return {state_,state_->reduced(std::move(u),work)};
    }
    AlgebraicElement power(std::uint64_t exponent) const {
        algebraic_detail::Work work{state_->limits};
        RationalVector result(state_->degree()); result[0]=1; auto base=coordinates_;
        while (exponent) {
            if (exponent&1u) result=state_->product(result,base,work);
            exponent>>=1u;
            if (exponent) base=state_->product(base,base,work);
        }
        return {state_,std::move(result)};
    }
    template<std::floating_point T> AlgebraicElement power(T) const = delete;
    ExactMatrix regular_action() const {
        const auto d=state_->degree();
        if (machine::checked_size_mul(d,d)>state_->limits.max_matrix_coordinates)
            throw std::length_error("algebraic regular action coordinate budget");
        algebraic_detail::Work work{state_->limits}; ExactMatrix result(d,d); auto column=coordinates_;
        for (std::size_t j=0;j<d;++j) {
            for (std::size_t i=0;i<d;++i) result.set(i,j,column[i]);
            if (j+1<d) {
                column.insert(column.begin(),Rational{0});
                column=state_->reduced(std::move(column),work);
            }
        }
        return result;
    }
    Rational trace() const {
        const auto action=regular_action(); Rational result{};
        algebraic_detail::Work work{state_->limits};
        for (std::size_t i=0;i<action.rows();++i) result=work.checked(Rational(result+action.get(i,i)));
        return result;
    }
    friend bool operator==(const AlgebraicElement& a,const AlgebraicElement& b) noexcept {
        return a.same_field(b) && a.coordinates_==b.coordinates_;
    }
    friend AlgebraicElement operator+(const AlgebraicElement& a,const AlgebraicElement& b) {
        require_same(a,b); algebraic_detail::Work work{a.state_->limits}; auto out=a.coordinates_;
        for (std::size_t i=0;i<out.size();++i) out[i]=work.checked(Rational(out[i]+b.coordinates_[i]));
        return {a.state_,std::move(out)};
    }
    friend AlgebraicElement operator-(const AlgebraicElement& a) {
        auto out=a.coordinates_; for (auto& q:out) q=-q; return {a.state_,std::move(out)};
    }
    friend AlgebraicElement operator-(const AlgebraicElement& a,const AlgebraicElement& b) { return a+(-b); }
    friend AlgebraicElement operator*(const AlgebraicElement& a,const AlgebraicElement& b) {
        require_same(a,b); algebraic_detail::Work work{a.state_->limits};
        return {a.state_,a.state_->product(a.coordinates_,b.coordinates_,work)};
    }
    friend AlgebraicElement operator/(const AlgebraicElement& a,const AlgebraicElement& b) {
        require_same(a,b); return a*b.inverse();
    }
private:
    friend class CertifiedAlgebraicField;
    AlgebraicElement(std::shared_ptr<const algebraic_detail::FieldState> state,RationalVector coordinates)
        : state_(std::move(state)),coordinates_(std::move(coordinates)) {}
    static void require_same(const AlgebraicElement& a,const AlgebraicElement& b) {
        if (!a.same_field(b)) throw std::invalid_argument("algebraic field owners differ");
    }
    std::shared_ptr<const algebraic_detail::FieldState> state_;
    RationalVector coordinates_;
};

// Q[x]/(f) is admitted as a field only after checking the supplied certificate.
// Its regular action is a faithful Q-linear carrier, not a selected spatial root.
class CertifiedAlgebraicField {
public:
    CertifiedAlgebraicField(const CertifiedAlgebraicField&)=default;
    CertifiedAlgebraicField& operator=(const CertifiedAlgebraicField&)=default;
    static CertifiedAlgebraicField declare(RationalVector polynomial,AlgebraicIrreducibility certificate,
                                           AlgebraicFieldLimits limits={}) {
        algebraic_detail::Work work{limits}; polynomial=algebraic_detail::input(polynomial,work);
        if (polynomial.size()<2 || polynomial.back()!=1)
            throw std::invalid_argument("algebraic field requires a monic nonconstant polynomial");
        if (polynomial.size()-1>limits.max_degree) throw std::length_error("algebraic field degree budget");
        algebraic_detail::certify(polynomial,certificate,work);
        return CertifiedAlgebraicField(std::make_shared<const algebraic_detail::FieldState>(
            algebraic_detail::FieldState{std::move(polynomial),std::move(certificate),limits}));
    }
    template<std::floating_point T>
    static CertifiedAlgebraicField declare(std::initializer_list<T>,AlgebraicIrreducibility,AlgebraicFieldLimits={})=delete;
    bool same_owner(const CertifiedAlgebraicField& other) const noexcept { return state_==other.state_; }
    bool owns(const AlgebraicElement& value) const noexcept { return state_==value.state_; }
    std::size_t degree() const noexcept { return state_->degree(); }
    const RationalVector& minimal_polynomial() const & noexcept { return state_->polynomial; }
    const AlgebraicIrreducibility& certificate() const & noexcept { return state_->certificate; }
    const AlgebraicFieldLimits& limits() const & noexcept { return state_->limits; }
    const RationalVector& minimal_polynomial() const && = delete;
    const AlgebraicIrreducibility& certificate() const && = delete;
    const AlgebraicFieldLimits& limits() const && = delete;
    AlgebraicElement element(const RationalVector& polynomial) const {
        algebraic_detail::Work work{state_->limits};
        return {state_,state_->reduced(algebraic_detail::input(polynomial,work),work)};
    }
    template<std::floating_point T> AlgebraicElement element(std::initializer_list<T>) const = delete;
    AlgebraicElement scalar(Rational value) const { return element({std::move(value)}); }
    template<std::floating_point T> AlgebraicElement scalar(T) const = delete;
    AlgebraicElement generator() const { return element({Rational{0},Rational{1}}); }
    AlgebraicElement evaluate(const RationalVector& polynomial,const AlgebraicElement& at) const {
        if (!owns(at)) throw std::invalid_argument("algebraic evaluation field owner differs");
        algebraic_detail::Work work{state_->limits};
        const auto input=algebraic_detail::input(polynomial,work); RationalVector out(degree());
        for (auto i=input.rbegin();i!=input.rend();++i) {
            out=state_->product(out,at.coordinates_,work); out[0]=work.checked(Rational(out[0]+*i));
        }
        return {state_,std::move(out)};
    }
    AlgebraicElement specialize(const RationalFunction& source,const AlgebraicElement& at) const {
        const auto denominator=evaluate(source.denominator(),at);
        if (denominator.is_zero()) throw std::domain_error("algebraic specialization is a pole");
        return evaluate(source.numerator(),at)/denominator;
    }
private:
    friend class AlgebraicElement;
    explicit CertifiedAlgebraicField(std::shared_ptr<const algebraic_detail::FieldState> state) : state_(std::move(state)) {}
    std::shared_ptr<const algebraic_detail::FieldState> state_;
};
inline CertifiedAlgebraicField AlgebraicElement::field() const { return CertifiedAlgebraicField(state_); }
} // namespace rfc::numeric
