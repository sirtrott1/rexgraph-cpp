#pragma once
#include "rfc/numeric/exact.hpp"
#include <algorithm>
#include <concepts>
#include <map>
#include <utility>
#include <vector>

namespace rfc::numeric {
// One formal parameter over Q. This domain is selected explicitly and does not
// assert that every real evaluation is rational or that a pole is admissible.
class RationalFunction {
  public:
    using Polynomial = RationalVector; // ascending powers of the formal parameter
    static constexpr std::size_t max_coefficients = 4096;
    RationalFunction() : denominator_{Rational{1}} {}
    explicit RationalFunction(Rational scalar) : numerator_{std::move(scalar)}, denominator_{Rational{1}} {
        normalize();
    }
    template <std::floating_point T> RationalFunction(T) = delete;
    RationalFunction(Polynomial numerator, Polynomial denominator)
        : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {
        normalize();
    }
    static RationalFunction parameter() { return {{Rational{0}, Rational{1}}, {Rational{1}}}; }
    const Polynomial &numerator() const & noexcept { return numerator_; }
    const Polynomial &denominator() const & noexcept { return denominator_; }
    const Polynomial &numerator() const && = delete;
    const Polynomial &denominator() const && = delete;
    bool is_zero() const noexcept { return numerator_.empty(); }
    std::size_t coefficient_count() const {
        return machine::checked_size_add(numerator_.size(), denominator_.size());
    }
    Rational evaluate(const Rational &parameter_value) const {
        const auto d = evaluate_polynomial(denominator_, parameter_value);
        if (d == 0)
            throw std::domain_error("rational function evaluated at a pole");
        return evaluate_polynomial(numerator_, parameter_value) / d;
    }
    RationalFunction derivative() const {
        return {
            subtract(multiply(derive(numerator_), denominator_), multiply(numerator_, derive(denominator_))),
            multiply(denominator_, denominator_)};
    }
    friend bool operator==(const RationalFunction &, const RationalFunction &) = default;
    friend RationalFunction operator+(const RationalFunction &a, const RationalFunction &b) {
        return {add(multiply(a.numerator_, b.denominator_), multiply(b.numerator_, a.denominator_)),
                multiply(a.denominator_, b.denominator_)};
    }
    friend RationalFunction operator-(const RationalFunction &a, const RationalFunction &b) {
        return {subtract(multiply(a.numerator_, b.denominator_), multiply(b.numerator_, a.denominator_)),
                multiply(a.denominator_, b.denominator_)};
    }
    friend RationalFunction operator*(const RationalFunction &a, const RationalFunction &b) {
        return {multiply(a.numerator_, b.numerator_), multiply(a.denominator_, b.denominator_)};
    }
    friend RationalFunction operator/(const RationalFunction &a, const RationalFunction &b) {
        if (b.is_zero())
            throw std::domain_error("division by zero rational function");
        return {multiply(a.numerator_, b.denominator_), multiply(a.denominator_, b.numerator_)};
    }

  private:
    Polynomial numerator_, denominator_;
    static void trim(Polynomial &p) {
        while (!p.empty() && p.back() == 0)
            p.pop_back();
    }
    static void validate(const Polynomial &p) {
        if (p.size() > max_coefficients)
            throw std::length_error("rational function degree limit");
        for (const auto &q : p) {
            if (q.get_den() <= 0)
                throw std::invalid_argument("rational function coefficient denominator");
            Integer gcd;
            mpz_gcd(gcd.get_mpz_t(), mpq_numref(q.get_mpq_t()), mpq_denref(q.get_mpq_t()));
            if (gcd != 1)
                throw std::invalid_argument("noncanonical rational function coefficient");
        }
    }
    static Polynomial add(Polynomial a, const Polynomial &b) {
        a.resize(std::max(a.size(), b.size()));
        for (std::size_t i = 0; i < b.size(); ++i)
            a[i] += b[i];
        trim(a);
        return a;
    }
    static Polynomial subtract(Polynomial a, const Polynomial &b) {
        a.resize(std::max(a.size(), b.size()));
        for (std::size_t i = 0; i < b.size(); ++i)
            a[i] -= b[i];
        trim(a);
        return a;
    }
    static Polynomial multiply(const Polynomial &a, const Polynomial &b) {
        if (a.empty() || b.empty())
            return {};
        const auto size = machine::checked_size_add(a.size(), b.size()) - 1;
        if (size > max_coefficients)
            throw std::length_error("rational function intermediate degree limit");
        Polynomial out(size);
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = 0; j < b.size(); ++j)
                out[i + j] += a[i] * b[j];
        trim(out);
        return out;
    }
    static std::pair<Polynomial, Polynomial> divide(Polynomial a, const Polynomial &b) {
        if (b.empty())
            throw std::domain_error("zero polynomial divisor");
        Polynomial q(a.size() >= b.size() ? a.size() - b.size() + 1 : 0);
        while (!a.empty() && a.size() >= b.size()) {
            const auto shift = a.size() - b.size();
            const Rational factor = a.back() / b.back();
            q[shift] += factor;
            for (std::size_t i = 0; i < b.size(); ++i)
                a[shift + i] -= factor * b[i];
            trim(a);
        }
        trim(q);
        return {std::move(q), std::move(a)};
    }
    static Polynomial derive(const Polynomial &p) {
        Polynomial out(p.empty() ? 0 : p.size() - 1);
        for (std::size_t i = 1; i < p.size(); ++i)
            out[i - 1] = p[i] * Rational{static_cast<unsigned long>(i)};
        trim(out);
        return out;
    }
    static Rational evaluate_polynomial(const Polynomial &p, const Rational &x) {
        Rational out = 0;
        for (auto i = p.rbegin(); i != p.rend(); ++i)
            out = out * x + *i;
        return out;
    }
    void normalize() {
        validate(numerator_);
        validate(denominator_);
        trim(numerator_);
        trim(denominator_);
        if (denominator_.empty())
            throw std::invalid_argument("zero rational function denominator");
        if (numerator_.empty()) {
            denominator_ = {Rational{1}};
            return;
        }
        auto a = numerator_, b = denominator_;
        while (!b.empty()) {
            auto remainder = divide(a, b).second;
            a = std::move(b);
            b = std::move(remainder);
        }
        const Rational lead = a.back();
        for (auto &value : a)
            value /= lead;
        numerator_ = divide(numerator_, a).first;
        denominator_ = divide(denominator_, a).first;
        const Rational scale = denominator_.back();
        for (auto &value : numerator_)
            value /= scale;
        for (auto &value : denominator_)
            value /= scale;
    }
};

struct RationalFunctionSolveLimits {
    std::size_t max_dimension{32}, max_coefficients{262144}, max_function_steps{1000000};
};

// Bounded exact sparse elimination over Q(t). No sampled rank or implicit CAS
// dependency is used. Singular formal systems and invalid evaluations reject.
class RationalFunctionMatrix {
  public:
    RationalFunctionMatrix(std::size_t rows, std::size_t columns) : rows_(rows), columns_(columns) {}
    void set(std::size_t row, std::size_t column, RationalFunction value) {
        if (row >= rows_ || column >= columns_)
            throw std::out_of_range("rational function matrix index");
        if (value.is_zero())
            entries_.erase({row, column});
        else
            entries_[{row, column}] = std::move(value);
    }
    std::vector<RationalFunction> apply(const std::vector<RationalFunction> &x) const {
        if (x.size() != columns_)
            throw std::invalid_argument("symbolic action dimension mismatch");
        std::vector<RationalFunction> out(rows_);
        for (const auto &[key, value] : entries_)
            out[key.first] = out[key.first] + value * x[key.second];
        return out;
    }
    std::vector<RationalFunction> solve(const std::vector<RationalFunction> &rhs,
                                        const RationalFunctionSolveLimits &limits = {}) const {
        if (rows_ != columns_ || rhs.size() != rows_)
            throw std::invalid_argument("symbolic solve requires a square matching system");
        if (rows_ > limits.max_dimension)
            throw std::length_error("symbolic solve dimension limit");
        std::size_t initial{};
        for (const auto &[key, value] : entries_) {
            (void)key;
            initial = machine::checked_size_add(initial, value.coefficient_count());
        }
        for (const auto &value : rhs)
            initial = machine::checked_size_add(initial, value.coefficient_count());
        if (initial > limits.max_coefficients)
            throw std::length_error("symbolic solve input coefficient limit");
        std::vector<std::map<std::size_t, RationalFunction>> rows(rows_);
        auto b = rhs;
        std::vector<RationalFunction> out(rows_);
        for (const auto &[key, value] : entries_)
            rows[key.first].emplace(key.second, value);
        std::size_t steps{};
        const auto charge = [&] {
            if (++steps > limits.max_function_steps)
                throw std::length_error("symbolic solve work limit");
        };
        const auto retained = [&] {
            std::size_t count{};
            for (const auto &row : rows)
                for (const auto &[key, value] : row) {
                    (void)key;
                    count = machine::checked_size_add(count, value.coefficient_count());
                }
            for (const auto &value : b)
                count = machine::checked_size_add(count, value.coefficient_count());
            for (const auto &value : out)
                count = machine::checked_size_add(count, value.coefficient_count());
            if (count > limits.max_coefficients)
                throw std::length_error("symbolic solve coefficient limit");
        };
        retained();
        for (std::size_t k = 0; k < rows_; ++k) {
            std::size_t pivot = k;
            while (pivot < rows_ && !rows[pivot].contains(k))
                ++pivot;
            if (pivot == rows_)
                throw std::domain_error("singular rational function system");
            std::swap(rows[k], rows[pivot]);
            std::swap(b[k], b[pivot]);
            const auto p = rows[k].at(k);
            for (std::size_t i = k + 1; i < rows_; ++i)
                if (rows[i].contains(k)) {
                    charge();
                    const auto ratio = rows[i].at(k) / p;
                    rows[i].erase(k);
                    for (const auto &[j, value] : rows[k])
                        if (j > k) {
                            charge();
                            auto next = rows[i][j] - ratio * value;
                            if (next.is_zero())
                                rows[i].erase(j);
                            else
                                rows[i][j] = std::move(next);
                            retained();
                        }
                    charge();
                    b[i] = b[i] - ratio * b[k];
                    retained();
                }
        }
        for (std::size_t i = rows_; i-- > 0;) {
            auto sum = b[i];
            for (const auto &[j, value] : rows[i])
                if (j > i) {
                    charge();
                    sum = sum - value * out[j];
                }
            charge();
            out[i] = sum / rows[i].at(i);
            retained();
        }
        if (apply(out) != rhs)
            throw std::runtime_error("symbolic solve residual is nonzero");
        return out;
    }

  private:
    std::size_t rows_, columns_;
    std::map<std::pair<std::size_t, std::size_t>, RationalFunction> entries_;
};
} // namespace rfc::numeric
