#include "rfc/local/radix_carry.hpp"
#include <iostream>

int main() {
    using namespace rfc;
    numeric::ExactActionGuard guard;
    const std::vector<core::IdentityKey> positions{10,20,30,40,50,60};
    const std::vector<core::RelationKey> carries{{100},{200},{300},{400},{500}};
    const auto radix=local::PreparedRadixCarry::gaussian_minus_one_plus_i(positions,carries,guard);
    const auto original=radix.field({5,-3,0,0,0,0,0,0,0,0,0,0},guard);
    const auto normalized=radix.normalize_integer(original,guard);
    const auto& coordinates=normalized.after().coordinates();
    if(normalized.value()!=numeric::RationalVector({5,-3}) ||
       coordinates!=numeric::RationalVector({0,0,1,0,1,0,1,0,0,0,1,0}))return 1;
    std::cout<<"5 - 3i = ";
    for(std::size_t i=radix.position_count();i-->0;)std::cout<<coordinates[2*i];
    std::cout<<" in base -1+i, with six retained positions.\n";
    std::cout<<"Exact carry coordinates:";
    for(const auto& coefficient:normalized.carries())std::cout<<' '<<coefficient;
    std::cout<<'\n';
}
