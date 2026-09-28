#include "rfc/core/builder.hpp"
#include "rfc/local/cellular_sheaf.hpp"
#include "rfc/local/covering.hpp"
#include "rfc/topology/local_system_transport.hpp"
#include "rfc/topology/monodromy.hpp"
#include "rfc/topology/sheaf_cohomology.hpp"
#include <iostream>
#include <vector>

static rfc::core::RelationalComplex make_cycle(std::size_t n) {
    rfc::core::RelationSignature sig;
    const auto edge = sig.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder b(sig);
    for (std::size_t i = 0; i < n; ++i) {
        const auto j = (i + 1u) % n;
        b.add_grade_one(edge, {{static_cast<rfc::core::IdentityKey>(i),true,true,false,1},
                               {static_cast<rfc::core::IdentityKey>(j),true,false,false,1}});
    }
    return b.freeze();
}

int main() {
    using namespace rfc;
    const auto c3 = make_cycle(3u);
    local::StalkBundle stalks(c3);
    local::LinearCellularSheaf local_system(stalks);
    const auto& arena = c3.grade(core::Grade{1});
    const auto [begin,end] = arena.incidence_range(2u);
    for (std::size_t slot = begin; slot < end; ++slot) {
        if (arena.lower_index[slot] == 0u) {
            local_system.set_restriction(core::Grade{1}, slot,
                numeric::ExactMatrix::from_rows({{numeric::Rational{2}}}));
        }
    }
    const topology::PairwisePath loop(c3, 0u, {{0u,true},{1u,true},{2u,true}});
    const auto holonomy = topology::local_system_transport(local_system, loop);
    const auto H0 = topology::sheaf_cohomology(local_system, core::Grade{0});
    const auto H1 = topology::sheaf_cohomology(local_system, core::Grade{1});

    const auto c6 = make_cycle(6u);
    std::vector<std::vector<std::uint32_t>> maps(2, std::vector<std::uint32_t>(6u));
    for (std::uint32_t i = 0; i < 6u; ++i) { maps[0][i] = i % 3u; maps[1][i] = i % 3u; }
    const local::StrictRelationalCovering cover(c6, c3, maps);
    const auto action = topology::monodromy(cover, loop);

    std::cout << "twisted_H0=" << H0.dimension << " twisted_H1=" << H1.dimension << '\n';
    std::cout << "local_system_holonomy=" << numeric::str(holonomy.get(0u,0u)) << '\n';
    std::cout << "cover_monodromy=";
    for (const auto i : action.image) std::cout << ' ' << i;
    std::cout << '\n';
    return 0;
}
