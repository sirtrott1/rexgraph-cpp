#pragma once
#include "rfc/local/branch_chain.hpp"

namespace branch_chain_test {
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using Guard=numeric::ExactActionGuard;
using Branch=local::PreparedBranchChain;
using F=numeric::CertifiedAlgebraicField;
inline std::vector<core::IdentityKey> vertices(std::size_t count) {
    std::vector<core::IdentityKey> out;
    for(std::size_t i=0;i<count;++i)out.push_back(1000+(count-i)*37);
    return out;
}
inline std::vector<core::RelationKey> relations(std::size_t count) {
    std::vector<core::RelationKey> out;
    for(std::size_t i=0;i<count;++i)out.push_back(core::RelationKey{9000+i*13});
    return out;
}
inline Branch prepare(const V& shares,std::size_t count,Guard& guard) {
    return Branch::prepare(shares,vertices(count+shares.size()),relations(count),guard,core::SemanticScale{7});
}
inline F quadratic() { return F::declare({2,0,1},numeric::EisensteinShift{2,0}); }
} // namespace branch_chain_test
