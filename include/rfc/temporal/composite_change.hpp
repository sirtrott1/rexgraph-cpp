#pragma once
#include "rfc/composite/boundary.hpp"
#include <stdexcept>
#include <vector>

namespace rfc::temporal {

struct CompositeOccurrenceChange {
    numeric::Rational existence{0};
    numeric::Rational orientation{0};
    numeric::Rational magnitude{0};
    numeric::Rational aggregate{0};
};

inline CompositeOccurrenceChange composite_coefficient_change(
    const composite::CompositeCoefficient& old_state,
    const composite::CompositeCoefficient& new_state) {
    if (old_state.magnitude < 0 || new_state.magnitude < 0) {
        throw std::invalid_argument("composite magnitudes must be nonnegative");
    }
    const numeric::Rational e = old_state.exists ? 1 : 0;
    const numeric::Rational ep = new_state.exists ? 1 : 0;
    const numeric::Rational s = old_state.negative ? -1 : 1;
    const numeric::Rational sp = new_state.negative ? -1 : 1;
    CompositeOccurrenceChange out;
    out.existence = (ep - e) * s * old_state.magnitude;
    out.orientation = ep * (sp - s) * old_state.magnitude;
    out.magnitude = ep * sp * (new_state.magnitude - old_state.magnitude);
    out.aggregate = out.existence + out.orientation + out.magnitude;
    out.existence.canonicalize();
    out.orientation.canonicalize();
    out.magnitude.canonicalize();
    out.aggregate.canonicalize();
    numeric::Rational direct = new_state.realize() - old_state.realize();
    direct.canonicalize();
    if (out.aggregate != direct) throw std::logic_error("composite finite-change identity failed");
    return out;
}

struct CompositeBoundaryChange {
    numeric::RationalVector existence;
    numeric::RationalVector orientation;
    numeric::RationalVector magnitude;
    numeric::RationalVector aggregate;
};

inline CompositeBoundaryChange composite_boundary_change(const std::vector<composite::CompositeCoefficient>& old_state,
                                                          const std::vector<composite::CompositeCoefficient>& new_state) {
    if (old_state.size() != new_state.size()) throw std::invalid_argument("composite boundary change size mismatch");
    CompositeBoundaryChange out;
    out.existence.resize(old_state.size(),0);
    out.orientation.resize(old_state.size(),0);
    out.magnitude.resize(old_state.size(),0);
    out.aggregate.resize(old_state.size(),0);
    for (std::size_t i=0;i<old_state.size();++i) {
        const auto item = composite_coefficient_change(old_state[i], new_state[i]);
        out.existence[i] = item.existence;
        out.orientation[i] = item.orientation;
        out.magnitude[i] = item.magnitude;
        out.aggregate[i] = item.aggregate;
    }
    return out;
}

} // namespace rfc::temporal
