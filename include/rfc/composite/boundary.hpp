#pragma once
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rfc::composite {

// Signed-incidence realization only. Relation-role aspects such as "head" are
// independent binary fields in the relation signature and do not belong to the
// scalar coefficient itself.
struct CompositeCoefficient {
    bool exists{true};
    bool negative{false};
    numeric::Rational magnitude{1};

    [[nodiscard]] numeric::Rational realize() const {
        if (!exists) return 0;
        if (magnitude < 0) throw std::invalid_argument("composite magnitude must be nonnegative");
        return negative ? -magnitude : magnitude;
    }
};

inline int signed_support(bool exists, bool negative) noexcept {
    if (!exists) return 0;
    return negative ? -1 : 1;
}

inline std::vector<CompositeCoefficient> canonical_one_head(std::size_t arity, std::size_t head_position) {
    if (arity < 2) throw std::invalid_argument("canonical composite relation requires arity >= 2");
    if (head_position >= arity) throw std::out_of_range("head position");
    std::vector<CompositeCoefficient> result;
    result.reserve(arity);
    numeric::Rational tail_share = numeric::Rational{1} / numeric::Rational{static_cast<unsigned long>(arity - 1u)};
    tail_share.canonicalize();
    for (std::size_t i = 0; i < arity; ++i) {
        if (i == head_position) result.push_back({true, true, numeric::Rational{1}});
        else result.push_back({true, false, tail_share});
    }
    return result;
}

} // namespace rfc::composite
