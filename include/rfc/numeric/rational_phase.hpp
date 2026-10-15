#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace rfc::numeric {

// A declared unit in Q(i), in its standard real/imaginary basis. It carries
// phase, not an unwrapped angle, winding history, or a Hodge-sector label.
class RationalPhase {
public:
    RationalPhase(Rational real, Rational imaginary)
        : real_(canonical_rational(std::move(real))), imaginary_(canonical_rational(std::move(imaginary))) {
        if (real_*real_+imaginary_*imaginary_ != 1)
            throw std::invalid_argument("rational phase must have unit norm");
    }
    template<class A, class B> requires (std::floating_point<A> || std::floating_point<B>)
    RationalPhase(A, B) = delete;
    static RationalPhase identity() { return {Rational{1}, Rational{0}}; }
    static RationalPhase half_turn() { return {Rational{-1}, Rational{0}}; }
    static RationalPhase cayley(Rational parameter) {
        parameter = canonical_rational(std::move(parameter));
        const Rational square = parameter*parameter;
        const Rational denominator = 1+square;
        return {Rational((1-square)/denominator), Rational(2*parameter/denominator)};
    }
    template<std::floating_point T> static RationalPhase cayley(T) = delete;
    const Rational& real() const & noexcept { return real_; }
    const Rational& imaginary() const & noexcept { return imaginary_; }
    const Rational& real() const && = delete;
    const Rational& imaginary() const && = delete;
    Rational cayley_parameter() const {
        if (real_ == -1) throw std::domain_error("half turn is outside the finite Cayley chart");
        return Rational(imaginary_/(1+real_));
    }
    RationalPhase inverse() const { return {real_, Rational(-imaginary_)}; }
    RationalPhase power(std::uint64_t exponent, std::size_t max_integer_bits = 262144) const {
        const auto admit = [max_integer_bits](const RationalPhase& phase) {
            for (const auto* value : {&phase.real_, &phase.imaginary_})
                if (mpz_sizeinbase(mpq_numref(value->get_mpq_t()),2) > max_integer_bits ||
                    mpz_sizeinbase(mpq_denref(value->get_mpq_t()),2) > max_integer_bits)
                    throw std::length_error("rational phase power integer bit budget");
        };
        admit(*this);
        auto result = identity();
        auto base = *this;
        while (exponent != 0) {
            if ((exponent & 1u) != 0) { result = result*base; admit(result); }
            exponent >>= 1u;
            if (exponent != 0) { base = base*base; admit(base); }
        }
        return result;
    }
    template<std::floating_point T> RationalPhase power(T, std::size_t = 262144) const = delete;
    ExactMatrix real_action() const {
        return ExactMatrix::from_rows({{real_, Rational(-imaginary_)}, {imaginary_, real_}});
    }
    friend RationalPhase operator*(const RationalPhase& a, const RationalPhase& b) {
        return {Rational(a.real_*b.real_-a.imaginary_*b.imaginary_),
                Rational(a.real_*b.imaginary_+a.imaginary_*b.real_)};
    }
    friend bool operator==(const RationalPhase&, const RationalPhase&) = default;
private:
    Rational real_, imaginary_;
};

// An element of the explicitly chosen cyclic group. The regular action is an
// n-dimensional permutation action; it does not assert a rational plane root.
class CyclicPhase {
public:
    CyclicPhase(std::uint64_t order, const Integer& step) {
        if (order == 0 || order > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("cyclic phase order exceeds its positive retained index domain");
        order_ = static_cast<std::uint32_t>(order);
        step_ = static_cast<std::uint32_t>(mpz_fdiv_ui(step.get_mpz_t(),static_cast<unsigned long>(order_)));
    }
    template<class A, class B> requires (std::floating_point<A> || std::floating_point<B>)
    CyclicPhase(A,B) = delete;
    std::uint32_t order() const noexcept { return order_; }
    std::uint32_t step() const noexcept { return step_; }
    std::uint32_t element_order() const noexcept { return order_/std::gcd(order_,step_); }
    CyclicPhase inverse() const { return {order_, step_ == 0 ? 0u : order_-step_}; }
    CyclicPhase power(std::uint64_t exponent) const {
        const auto step = static_cast<std::uint32_t>((static_cast<std::uint64_t>(step_)*(exponent % order_)) % order_);
        return {order_,Integer(step)};
    }
    template<std::floating_point T> CyclicPhase power(T) const = delete;
    ExactMatrix regular_action(std::size_t max_coordinate_slots = 1000000) const {
        const auto slots = machine::checked_size_mul(order_,order_);
        if (slots > max_coordinate_slots) throw std::length_error("cyclic regular action coordinate budget");
        ExactMatrix result(order_,order_);
        for (std::uint32_t j = 0; j < order_; ++j)
            result.set(static_cast<std::size_t>((static_cast<std::uint64_t>(j)+step_) % order_),j,Rational{1});
        return result;
    }
    friend CyclicPhase operator*(const CyclicPhase& a, const CyclicPhase& b) {
        if (a.order_ != b.order_) throw std::invalid_argument("cyclic phase domains differ");
        const auto step = static_cast<std::uint32_t>((static_cast<std::uint64_t>(a.step_)+b.step_) % a.order_);
        return {a.order_,Integer(step)};
    }
    friend bool operator==(const CyclicPhase&, const CyclicPhase&) = default;
private:
    std::uint32_t order_, step_{};
};
} // namespace rfc::numeric
