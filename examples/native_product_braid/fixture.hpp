#pragma once
#include "rfc/inference/native_product_braid.hpp"
#include "../native_product_hexagon/fixture.hpp"

namespace native_braid_example {
using namespace native_hexagon_example;
using Braid = rfc::inference::PreparedNativeProductBraid;
using BraidNumeric = rfc::algebra::OwnedProductBraid;
using Generator = rfc::algebra::ProductBraidGenerator;
// Higher-grade stress fixtures declare a larger logical workspace explicitly.
// The small standalone example and public runtime defaults stay unchanged.
inline rfc::runtime::ExecutionLease sample_lease(unsigned sample) {
    rfc::runtime::ExecutionHostPolicy policy;
    if (sample>=14) policy.limits.rational_slots=policy.limits.sparse_entries=64000000;
    return rfc::runtime::ExecutionLease(policy);
}
inline Braid braid(unsigned sample=0) { return Braid::adopt(plan(sample),{1900,1},available,sample_lease(sample)); }
} // namespace native_braid_example
