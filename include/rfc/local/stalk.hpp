#pragma once
#include "rfc/core/identity.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>

namespace rfc::local {

// Additive rational-vector stalk.  This is a linear specialization, not the
// definition of a local carrier in the relational complex.
struct LinearStalk {
    core::CellId cell{};
    std::size_t dimension{1};
};
using Stalk = LinearStalk;

// A linear flag transport belongs to one retained incidence occurrence.  It
// carries how a vector-space field on an upper primary relation is realized in
// one lower boundary stalk.  Nonlinear/set-valued local maps are not required
// to use this representation.
struct LinearFlagMap {
    core::CellId upper{};
    core::CellId lower{};
    std::size_t incidence_slot{};
    numeric::ExactMatrix transport;
};
using FlagMap = LinearFlagMap;

} // namespace rfc::local
