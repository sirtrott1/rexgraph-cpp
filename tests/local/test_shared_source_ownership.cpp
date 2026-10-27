#include "rfc/core/owned_source.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/sparse_stalk_actions.hpp"
#include "rfc/local/sparse_sheaf_actions.hpp"
#include <iostream>
#include <type_traits>
#include <utility>

namespace {
using namespace rfc;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
using S = numeric::SparseRationalVector;
using O = core::OwnedRelationalSource;
using F = local::OwnedLinearFiberSystem;
using H = local::OwnedLinearCellularSheaf;
constexpr core::Grade g0{0}, g1{1};
std::size_t checks{};
void check(bool ok, const char* msg) {
    ++checks;
    if (!ok) throw std::runtime_error(msg);
}
template<class Fn> void reject(Fn&& fn, const char* msg) {
    bool caught = false;
    try { fn(); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, msg);
}
core::RelationalComplex source() {
    core::RelationSignature sig;
    const auto gate = sig.aspects().add("gate", composite::AspectRole::RelationRole);
    const auto type = sig.add_type("shared-source-fixture", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(core::RelationKey{101}, type, {
        {10,true,true,3,{},core::OccurrenceKey{1001}},
        {20,true,false,1,{},core::OccurrenceKey{1002}},
        {30,true,false,0,{{gate,true}},core::OccurrenceKey{1003}},
        {20,false,false,7,{},core::OccurrenceKey{1004}},
        {10,true,false,Q{1,2},{},core::OccurrenceKey{1005}},
        {10,true,true,Q{1,2},{},core::OccurrenceKey{1006}}});
    return b.freeze();
}
std::pair<F,H> heterogeneous() {
    const auto raw = source();
    O owner(raw), equal_but_foreign(raw);
    check(&owner.source() != &raw, "snapshot must not borrow mutable input");
    check(!owner.same_owner(equal_but_foreign), "equal snapshots must differ");
    check(owner.same_owner(O(owner)), "source copy must share");
    local::LinearStalkBundle bs(owner.source(),2);
    bs.set_dimension({g0,0},1); bs.set_dimension({g0,2},3);
    local::LinearRelationalFiberSystem fibers(bs);
    fibers.set_transport(g1,0,M::from_rows({{2,0}}));
    fibers.set_transport(g1,2,M(3,2));
    fibers.set_transport(g1,3,M(2,2));
    fibers.set_transport(g1,4,M::from_rows({{Q{1,2},0}}));
    fibers.set_transport(g1,5,M::from_rows({{Q{-1,2},0}}));
    F down(owner,fibers);
    reject([&]{ F bad(equal_but_foreign,fibers); },"foreign equal source accepted for fibers");
    reject([&]{ fibers.set_transport(g1,0,M(2,1)); },"reversed rectangular map accepted");
    fibers.set_transport(g1,0,M::from_rows({{99,0}}));
    check(down.fibers().transport_override(g1,0)->get(0,0)==2,"mutable map was not snapshotted");
    check(!down.fibers().transport_override(g1,1),"default must stay symbolic");
    check(down.fibers().transport_override(g1,2).has_value(),"explicit zero lost");
    check(down.fibers().transport_override(g1,3).has_value(),"inactive map lost");
    check(down.fibers().complex().grade(g1).incidence_count()==6,"occurrence lost");
    for (std::size_t i=0;i<6;++i) {
        check(down.fibers().complex().occurrence_identity(g1,i)==raw.occurrence_identity(g1,i),"occurrence key changed");
        check(down.fibers().complex().incidence_coefficient(g1,i)==raw.incidence_coefficient(g1,i),"coefficient changed");
    }
    const auto gate=owner.source().signature().aspects().id("gate");
    check(owner.source().grade(g1).aspects.get(gate,2),"zero role lost");
    check(!owner.source().grade(g1).aspects.get(owner.source().signature().existence_aspect(),3),"absence lost");

    // Observation dimensions are independent of the physical family.
    local::LinearStalkBundle obs(owner.source(),3);
    obs.set_dimension({g0,0},2); obs.set_dimension({g0,1},1);
    local::LinearCellularSheaf sheaf(obs);
    sheaf.set_restriction(g1,0,M::from_rows({{2,0},{0,0},{0,5}}));
    sheaf.set_restriction(g1,1,M(3,1));
    sheaf.set_restriction(g1,3,M(3,1));
    sheaf.set_restriction(g1,4,M(3,2));
    sheaf.set_restriction(g1,5,M(3,2));
    H up(owner,sheaf);
    reject([&]{ H bad(equal_but_foreign,sheaf); },"foreign equal source accepted for sheaf");
    reject([&]{ sheaf.set_restriction(g1,1,M(1,3)); },"reversed restriction accepted");
    sheaf.set_restriction(g1,0,M(3,2));
    check(up.sheaf().restriction_override(g1,0)->get(0,0)==2,"restriction was not snapshotted");
    check(&down.fibers().complex()==&up.sheaf().complex(),"shared source address differs");
    check(down.source_owner().same_owner(up.source_owner()),"source anchor differs");
    check(down.fibers().stalks().dimension({g1,0})==2,"physical dimension changed");
    check(up.sheaf().stalks().dimension({g1,0})==3,"observation dimension conflated");
    check(!up.sheaf().restriction_override(g1,2),"upward default materialized");
    F distinct_field(owner,fibers);
    H distinct_sheaf(owner,sheaf);
    check(!down.same_owner(distinct_field),"source sharing identified field declarations");
    check(!up.same_owner(distinct_sheaf),"source sharing identified restrictions");
    check(down.same_owner(F(down)),"field handle copy differs");
    check(up.same_owner(H(up)),"sheaf handle copy differs");
    F old_down(fibers); H old_up(sheaf);
    check(&old_down.fibers().complex()!=&old_up.sheaf().complex(),"legacy constructors unexpectedly share");
    check(!old_down.source_owner().same_owner(owner),"legacy snapshot unexpectedly aliases input");
    return {down,up};
}
void actions_after_inputs_destroyed() {
    auto [down,up] = heterogeneous();
    field::SparseStalkFieldActions a(down);
    local::SparseSheafActions b(up);
    const auto output=a.boundary(a.bind(g1,{{0,S(2,{{0,3},{1,4}})}}));
    check(output.field.values().at(0).entries()==S(1,{{0,6}}).entries(),"downward override got a second coefficient");
    check(output.field.values().at(1).entries()==S(2,{{0,3},{1,4}}).entries(),"default down result differs");
    check(output.field.nonzeros()==3,"zero/inactive contributions leaked");
    const auto reading=b.coboundary(b.bind(g0,{{0,S(2,{{0,3},{1,4}})}}));
    check(reading.field.values().at(0).entries()==S(3,{{0,-18},{2,-60}}).entries(),"upward coefficient not applied once");
    check(reading.field.nonzeros()==2,"upward sparse count differs");
    check(down.source_owner().same_owner(up.source_owner()),"anchor lifetime lost");
    const auto another=[&] {
        local::LinearRelationalFiberSystem f(down.fibers().stalks());
        f.set_transport(g1,0,M(1,2)); f.set_transport(g1,2,M(3,2));
        f.set_transport(g1,4,M(1,2)); f.set_transport(g1,5,M(1,2));
        return F(down.source_owner(),f);
    }();
    field::SparseStalkFieldActions c(another);
    reject([&]{ (void)c.boundary(a.bind(g1)); },"same source accepted foreign field owner");
}
struct Counts { std::size_t down,up,down_nonzeros,up_nonzeros; };
Counts sparse(std::size_t n) {
    core::RelationSignature sig;
    const auto type=sig.add_type("pair",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(sig);
    builder.add_grade_one(type,{{10,true,true,1},{20,true,false,1}});
    O owner(builder.freeze());
    local::LinearStalkBundle bundle(owner.source(),n);
    local::LinearRelationalFiberSystem f(bundle);
    local::LinearCellularSheaf h(bundle);
    F down(owner,f); H up(owner,h);
    field::SparseStalkFieldActions da(down); local::SparseSheafActions ua(up);
    auto d=da.boundary(da.bind(g1,{{0,S(n,{{0,Q{-2,3}},{n-1,Q{4,7}}})}}));
    auto u=ua.coboundary(ua.bind(g0,{{0,S(n,{{0,Q{-2,3}},{n-1,Q{4,7}}})}}));
    check(!down.fibers().transport_override(g1,0),"large default materialized");
    check(!up.sheaf().restriction_override(g1,0),"large restriction materialized");
    check(d.field.values().at(0).entries()==S(n,{{0,Q{2,3}},{n-1,Q{-4,7}}}).entries(),"large down exact values");
    check(u.field.values().at(0).entries()==S(n,{{0,Q{2,3}},{n-1,Q{-4,7}}}).entries(),"large up exact values");
    return {d.work.scalar_products,u.work.scalar_products,d.field.nonzeros(),u.field.nonzeros()};
}
template<class T> concept RvalueSource = requires(T&& x) { std::move(x).source(); };
template<class T> concept RvalueAnchor = requires(T&& x) { std::move(x).source_owner(); };
static_assert(!RvalueSource<O> && !RvalueAnchor<F> && !RvalueAnchor<H>);
static_assert(!std::is_default_constructible_v<O>);
static_assert(!std::is_constructible_v<O,std::shared_ptr<core::RelationalComplex>>);
}
int main() {
    try {
        actions_after_inputs_destroyed();
        const auto small=sparse(7), large=sparse(1000003);
        check(small.down==large.down && small.up==large.up,"work scales with unused extent");
        check(small.down_nonzeros==large.down_nonzeros && small.up_nonzeros==large.up_nonzeros,"sparse support grew");
        std::cout<<"shared_source_ownership checks="<<checks<<" passed; million-coordinate products="<<large.down<<","<<large.up<<"\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
