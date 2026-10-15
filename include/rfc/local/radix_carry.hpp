#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <concepts>
#include <set>

namespace rfc::local {
enum class RadixCarryProfile { Rational, GaussianMinusOnePlusI };

// A finite declared digit carrier and its actual carry relations. The value
// map is the bilinear annihilator of the carry boundary, not a chosen metric
// harmonic vector. Gaussian coordinates use the complete rational basis 1,i.
class PreparedRadixCarry {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct State {
        local::OwnedLinearFiberSystem fibers;
        RadixCarryProfile profile;
        V radix;
        M boundary,value_map;
        numeric::ExactActionLimits limits;
        std::size_t positions,dimension,retained,steps;
        State(local::OwnedLinearFiberSystem owner,RadixCarryProfile kind,V base,M b,M v,
              numeric::ExactActionLimits admitted,std::size_t count,std::size_t d,std::size_t words,std::size_t work)
            :fibers(std::move(owner)),profile(kind),radix(std::move(base)),boundary(std::move(b)),
             value_map(std::move(v)),limits(admitted),positions(count),dimension(d),retained(words),steps(work){}
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRadixCarry(std::shared_ptr<const State> state):state_(std::move(state)){}
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    void operation(Guard& guard,std::size_t scratch=0) const {
        const auto& l=guard.limits();const auto& expected=state_->limits;
        if(l.dimension!=expected.dimension || l.entries!=expected.entries ||
           l.steps!=expected.steps || l.integer_bits!=expected.integer_bits)
            throw std::invalid_argument("radix carry operation requires its original action limits");
        guard.step();guard.entries(add(state_->retained,scratch));
    }
    static PreparedRadixCarry prepare(RadixCarryProfile profile,V radix,
            const std::vector<core::IdentityKey>& positions,const std::vector<core::RelationKey>& carries,Guard& guard) {
        const auto start=guard.steps();guard.step();radix=guard.canonical_input(radix);
        const std::size_t n=positions.size(),d=profile==RadixCarryProfile::Rational?1:2;
        if(n==0 || carries.size()!=n-1)throw std::invalid_argument("explicit nonempty digit positions and every adjacent carry key required");
        if(n>std::numeric_limits<std::uint32_t>::max())throw std::length_error("radix retained position limit");
        guard.dimension(mul(n,d));guard.dimension(mul(n-1,d));
        // Admit the complete source, stalk declarations, local maps and their
        // construction copies before allocating them. These are container
        // entries, not a claim about GMP or process memory usage.
        const auto source_entries=add(64,add(mul(n,64),mul(n-1,128)));
        guard.entries(source_entries);
        std::set<core::IdentityKey> identities;
        for(const auto key:positions){guard.step();if(!identities.insert(key).second)throw std::invalid_argument("digit position identities must be distinct");}
        std::set<core::RelationKey> relations;
        for(const auto key:carries){guard.step();if(!key.value || !relations.insert(key).second)throw std::invalid_argument("carry relation keys must be distinct and nonzero");}
        core::RelationSignature signature;
        const auto type=signature.add_type("radix carry",core::BoundaryPolicy::DeclaredExact,2,2);
        core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{1});
        for(const auto key:positions){guard.step();builder.declare_witness_identity(key);}
        const Q coefficient=profile==RadixCarryProfile::Rational?radix[0]:Q{1};
        const Q magnitude=coefficient<0?guard.sub(0,coefficient):coefficient;
        for(std::size_t i=0;i+1<n;++i) {
            guard.step();builder.add_grade_one(carries[i],type,
                {{positions[i],true,coefficient<0,magnitude},{positions[i+1],true,true,1}});
        }
        const auto source=builder.freeze();
        local::LinearStalkBundle stalks(source,d);local::LinearRelationalFiberSystem fibers(stalks);
        M base(d,d),minus_identity(d,d);
        if(profile==RadixCarryProfile::Rational)base.set(0,0,radix[0]);
        else {base.set(0,0,-1);base.set(0,1,-1);base.set(1,0,1);base.set(1,1,-1);}
        for(std::size_t i=0;i<d;++i)minus_identity.set(i,i,-1);
        M boundary(mul(n,d),mul(n-1,d));
        for(std::size_t i=0;i+1<n;++i) {
            guard.step();fibers.set_transport(core::Grade{1},2*i,base);
            fibers.set_transport(core::Grade{1},2*i+1,minus_identity);
            for(const auto& [key,q]:base.entries()) {
                guard.step();boundary.set(d*i+key.first,d*i+key.second,q);
            }
            for(std::size_t j=0;j<d;++j){guard.step();boundary.set(d*(i+1)+j,d*i+j,-1);}
        }
        M value_map(d,mul(n,d)),power=M::identity(d);
        for(std::size_t i=0;i<n;++i) {
            for(const auto& [key,q]:power.entries()) {
                guard.step();value_map.set(key.first,d*i+key.second,q);
            }
            if(i+1<n)power=numeric::checked_multiply(power,base,guard);
        }
        const auto retained=add(source_entries,add(radix.size(),add(boundary.nonzeros(),value_map.nonzeros())));
        guard.entries(add(retained,mul(8,mul(n,d))));guard.matrix(boundary);guard.matrix(value_map);
        if(numeric::checked_multiply(value_map,boundary,guard).nonzeros())
            throw std::logic_error("radix value map does not annihilate its declared carry boundary");
        return PreparedRadixCarry(std::make_shared<const State>(local::OwnedLinearFiberSystem(fibers),profile,
            std::move(radix),std::move(boundary),std::move(value_map),guard.limits(),n,d,retained,guard.steps()-start));
    }
public:
    class Field {
        friend class PreparedRadixCarry;
        std::shared_ptr<const State> owner_;
        V coordinates_;
        Field(std::shared_ptr<const State> owner,V coordinates):owner_(std::move(owner)),coordinates_(std::move(coordinates)){}
    public:
        const V& coordinates() const & {return coordinates_;}
        const V& coordinates() const &&=delete;
        bool same_owner(const Field& other) const noexcept {return owner_==other.owner_;}
    };
    class Transition {
        friend class PreparedRadixCarry;
        Field before_,after_;
        V carries_,value_;
        Transition(Field before,Field after,V carries,V value)
            :before_(std::move(before)),after_(std::move(after)),carries_(std::move(carries)),value_(std::move(value)){}
    public:
        const Field& before() const & {return before_;}
        const Field& after() const & {return after_;}
        const V& carries() const & {return carries_;}
        const V& value() const & {return value_;}
        const Field& before() const &&=delete;
        const Field& after() const &&=delete;
        const V& carries() const &&=delete;
        const V& value() const &&=delete;
    };

    static PreparedRadixCarry rational(Q base,const std::vector<core::IdentityKey>& positions,
            const std::vector<core::RelationKey>& carries,Guard& guard) {
        return prepare(RadixCarryProfile::Rational,{std::move(base)},positions,carries,guard);
    }
    template<std::floating_point T> static PreparedRadixCarry rational(T,
        const std::vector<core::IdentityKey>&,const std::vector<core::RelationKey>&,Guard&)=delete;
    static PreparedRadixCarry gaussian_minus_one_plus_i(const std::vector<core::IdentityKey>& positions,
            const std::vector<core::RelationKey>& carries,Guard& guard) {
        return prepare(RadixCarryProfile::GaussianMinusOnePlusI,{-1,1},positions,carries,guard);
    }
    const local::OwnedLinearFiberSystem& owner() const & {return state_->fibers;}
    const core::RelationalComplex& source() const & {return state_->fibers.fibers().complex();}
    const M& boundary() const & {return state_->boundary;}
    const M& value_map() const & {return state_->value_map;}
    const V& radix() const & {return state_->radix;}
    const local::OwnedLinearFiberSystem& owner() const &&=delete;
    const core::RelationalComplex& source() const &&=delete;
    const M& boundary() const &&=delete;
    const M& value_map() const &&=delete;
    const V& radix() const &&=delete;
    RadixCarryProfile profile() const noexcept {return state_->profile;}
    std::size_t position_count() const noexcept {return state_->positions;}
    std::size_t coordinates_per_position() const noexcept {return state_->dimension;}
    std::size_t retained_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}
    bool same_owner(const PreparedRadixCarry& other) const noexcept {return state_==other.state_;}

    Field field(const V& coordinates,Guard& guard) const {
        operation(guard,coordinates.size());
        if(coordinates.size()!=state_->boundary.rows())throw std::invalid_argument("every declared digit coordinate required");
        return Field(state_,guard.canonical_input(coordinates));
    }
    V value(const Field& digits,Guard& guard) const {
        operation(guard,digits.coordinates_.size());require_owner(digits);
        return numeric::checked_apply(state_->value_map,digits.coordinates_,guard);
    }
    Transition apply_carries(const Field& digits,const V& carries,Guard& guard) const {
        operation(guard,add(mul(4,digits.coordinates_.size()),mul(2,carries.size())));require_owner(digits);
        if(carries.size()!=state_->boundary.cols())throw std::invalid_argument("every declared carry coordinate required");
        auto accepted=guard.canonical_input(carries);
        auto shifted=numeric::checked_apply(state_->boundary,accepted,guard);
        for(std::size_t i=0;i<shifted.size();++i)shifted[i]=guard.add(shifted[i],digits.coordinates_[i]);
        auto represented=value(digits,guard);
        if(numeric::checked_apply(state_->value_map,shifted,guard)!=represented)
            throw std::logic_error("carry transition changed its exact represented value");
        return Transition(digits,Field(state_,std::move(shifted)),std::move(accepted),std::move(represented));
    }

    // Rational profile: integral base |b|>=2 and integral digits, canonical
    // digits 0..|b|-1. Gaussian profile: Gaussian integers, digits 0 or 1.
    // The highest declared position must suffice. Overflow is rejected without
    // changing the original field or inventing an extra source position.
    Transition normalize_integer(const Field& digits,Guard& guard) const {
        operation(guard,add(mul(5,digits.coordinates_.size()),mul(3,state_->boundary.cols())));require_owner(digits);
        V normalized=guard.canonical_input(digits.coordinates_),carries(state_->boundary.cols());
        for(const auto& q:normalized){guard.step();require_integer(q);}
        if(state_->profile==RadixCarryProfile::Rational) {
            const auto& base=state_->radix[0];require_integer(base);
            const Q absolute=base<0?guard.sub(0,base):base;
            if(absolute<2)throw std::invalid_argument("integer radix normalization requires integral absolute base at least two");
            for(std::size_t i=0;i+1<state_->positions;++i) {
                const auto digit=remainder(normalized[i],absolute,guard);
                const auto quotient=guard.div(guard.sub(normalized[i],digit),base);require_integer(quotient);
                carries[i]=guard.sub(0,quotient);normalized[i]=digit;
                normalized[i+1]=guard.add(normalized[i+1],quotient);
            }
            if(normalized.back()<0 || normalized.back()>=absolute)
                throw std::overflow_error("canonical radix digits exceed the declared finite carrier");
        } else {
            for(std::size_t i=0;i+1<state_->positions;++i) {
                const Q a=normalized[2*i],b=normalized[2*i+1];
                const auto digit=remainder(guard.add(a,b),Q{2},guard);
                const auto real=guard.div(guard.add(guard.sub(b,a),digit),2);
                const auto imaginary=guard.div(guard.sub(guard.sub(digit,a),b),2);
                require_integer(real);require_integer(imaginary);
                carries[2*i]=guard.sub(0,real);carries[2*i+1]=guard.sub(0,imaginary);
                normalized[2*i]=digit;normalized[2*i+1]=0;
                normalized[2*i+2]=guard.add(normalized[2*i+2],real);
                normalized[2*i+3]=guard.add(normalized[2*i+3],imaginary);
            }
            if(normalized.back()!=0 || (normalized[normalized.size()-2]!=0 && normalized[normalized.size()-2]!=1))
                throw std::overflow_error("canonical Gaussian digits exceed the declared finite carrier");
        }
        auto result=apply_carries(digits,carries,guard);
        if(result.after().coordinates()!=normalized)throw std::logic_error("canonical digits do not reconstruct from the retained carries");
        return result;
    }
private:
    void require_owner(const Field& field) const {
        if(field.owner_!=state_)throw std::invalid_argument("digit field belongs to a different declared radix carrier");
    }
    static void require_integer(const Q& q) {
        if(mpz_cmp_ui(mpq_denref(q.get_mpq_t()),1)!=0)throw std::invalid_argument("integral radix coordinates required");
    }
    static Q remainder(const Q& integer,const Q& positive_modulus,Guard& guard) {
        guard.step();mpz_class result;
        mpz_fdiv_r(result.get_mpz_t(),mpq_numref(integer.get_mpq_t()),mpq_numref(positive_modulus.get_mpq_t()));
        Q out{result};guard.value(out);return out;
    }
};
} // namespace rfc::local
