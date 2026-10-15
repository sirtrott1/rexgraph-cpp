#include "rfc/algebra/polygon.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/geometry/rational_rotation.hpp"
#include "rfc/topology/cyclic_attachment.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <concepts>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

using Q = rfc::numeric::Rational;
using M = rfc::numeric::ExactMatrix;
using V = rfc::numeric::RationalVector;
using F = rfc::numeric::RationalFunction;
using Phase = rfc::numeric::RationalPhase;
using G = rfc::core::Grade;
using Prepared = rfc::topology::PreparedCyclicAttachingRealization;

static std::size_t checks{};
static void require(bool condition,const char* reason) {
    if (!condition) throw std::runtime_error(reason);
    ++checks;
}
template<class Exception, class Action> static void rejects(Action action,const char* reason) {
    bool rejected = false;
    try { action(); } catch (const Exception&) { rejected = true; }
    require(rejected,reason);
}

static rfc::core::RelationalComplex presentation(std::size_t generators,
    const std::vector<std::vector<int>>& words,bool inactive = false) {
    rfc::core::RelationSignature signature;
    const auto loop = signature.add_type("meridian",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face = signature.add_attaching_loop_type("word",1,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < generators; ++i)
        builder.add_grade_one(loop,{{99,true,true,false,1},{99,true,false,false,1}});
    builder.declare_grade(G{2});
    for (const auto& word : words) {
        std::vector<rfc::core::HigherIncidenceInput> occurrences;
        for (const int letter : word) {
            const auto index = static_cast<std::uint32_t>((letter > 0 ? letter : -letter)-1);
            occurrences.emplace_back(index,true,letter < 0,false,Q{1});
        }
        if (inactive) occurrences.emplace_back(0,false,false,false,Q{7});
        builder.add_higher(G{2},face,occurrences);
    }
    return builder.freeze();
}

static void phases_and_geometry() {
    using namespace rfc;
    static_assert(!std::constructible_from<Phase,double,int>);
    static_assert(!std::constructible_from<numeric::CyclicPhase,int,double>);
    static_assert(!std::constructible_from<geometry::RationalUnitSpherePoint,int,double,int>);
    const auto identity = Phase::identity();
    const Phase quarter(Q{0},Q{1});
    require(quarter.power(4) == identity,"quarter-turn order");
    require(quarter.power(std::numeric_limits<std::uint64_t>::max()) == quarter.inverse(),"large exact periodic exponent");
    require(quarter*quarter == Phase::half_turn(),"Cayley chart transition");
    rejects<std::domain_error>([]{ (void)Phase::half_turn().cayley_parameter(); },"half-turn finite chart refusal");
    rejects<std::invalid_argument>([]{ (void)Phase(Q{1},Q{1}); },"nonunit phase refusal");
    rejects<std::invalid_argument>([]{ Q bad; mpz_set_ui(mpq_denref(bad.get_mpq_t()),0); (void)Phase(bad,Q{0}); },"invalid rational refusal");
    const auto fold = Phase::cayley(Q{1,2});
    require(fold.real() == Q{3,5} && fold.imaginary() == Q{4,5},"rational fold phase");
    rejects<std::length_error>([&]{ (void)fold.power(1000,64); },"aperiodic phase power budget");
    rejects<std::length_error>([&]{ (void)identity.power(0,0); },"zero power admission");
    for (int i = -6; i <= 6; ++i) {
        const Q parameter = numeric::rational(i,3);
        const auto p = Phase::cayley(parameter);
        require(p.cayley_parameter() == parameter,"phase chart reconstruction");
        require(p*p.inverse() == identity,"phase inverse");
        const auto R = p.real_action();
        require(R.transpose().multiply(R) == M::identity(2) && R.determinant() == 1,"phase exact orthogonality");
        for (int j = -3; j <= 3; ++j) {
            const auto q = Phase::cayley(Q(j,2));
            require((p*q).real_action() == R.multiply(q.real_action()),"phase action composition");
        }
    }
    for (const auto axis : {geometry::CoordinateAxis::X,geometry::CoordinateAxis::Y,geometry::CoordinateAxis::Z}) {
        const auto R = geometry::coordinate_rotation(axis,fold);
        require(R.transpose().multiply(R) == M::identity(3) && R.determinant() == 1,"axis rotation isometry");
    }
    const auto X = geometry::coordinate_rotation(geometry::CoordinateAxis::X,quarter);
    const auto Y = geometry::coordinate_rotation(geometry::CoordinateAxis::Y,quarter);
    require(X.multiply(Y) != Y.multiply(X),"noncommuting physical rotations retained");
    require(geometry::coordinate_rotation(geometry::CoordinateAxis::X,fold).apply({0,-1,0}) ==
        V({0,Q{-3,5},Q{-4,5}}),"exact panel fold point");
    rejects<std::invalid_argument>([&]{ (void)geometry::coordinate_rotation(static_cast<geometry::CoordinateAxis>(255),identity); },"axis domain refusal");
    for (int i = -3; i <= 3; ++i) for (int j = -3; j <= 3; ++j) {
        const Q u = numeric::rational(i,2), v = numeric::rational(j,3);
        const auto point = geometry::RationalUnitSpherePoint::stereographic(u,v);
        require(point.stereographic_coordinates() == std::pair<Q,Q>{u,v},"sphere chart reconstruction");
        require(numeric::dot(point.coordinates(),point.coordinates()) == 1,"sphere point quadrance");
        require(point.rotated(geometry::CoordinateAxis::Y,fold).rotated(geometry::CoordinateAxis::Y,fold.inverse()) == point,"sphere rotation inverse");
    }
    rejects<std::domain_error>([]{ (void)geometry::RationalUnitSpherePoint::excluded_pole().stereographic_coordinates(); },"sphere excluded-pole chart refusal");
    rejects<std::invalid_argument>([]{ (void)geometry::RationalUnitSpherePoint(1,1,0); },"nonunit sphere point refusal");
    for (std::uint32_t order = 1; order <= 12; ++order) {
        for (std::uint32_t step = 0; step < order; ++step) {
            const numeric::CyclicPhase phase(order,step);
            const auto action = phase.regular_action();
            require(action.transpose().multiply(action) == M::identity(order),"finite cyclic action orthogonality");
            require(phase.power(phase.element_order()).step() == 0,"finite cyclic element order");
            require((phase*phase.inverse()).step() == 0,"finite cyclic inverse");
            require((phase*phase).regular_action() == action.multiply(action),"finite cyclic group action");
        }
    }
    rejects<std::invalid_argument>([]{ (void)numeric::CyclicPhase(0,1); },"cyclic zero order refusal");
    require(numeric::CyclicPhase(7,-1).step() == 6,"signed cyclic index reduced exactly");
    const numeric::Integer huge("18446744073709551616");
    require(numeric::CyclicPhase(7,huge).step() == 2,"arbitrary integer cyclic index");
    rejects<std::invalid_argument>([]{ (void)numeric::CyclicPhase(4294967296ULL,1); },"cyclic order narrowing refusal");
    rejects<std::invalid_argument>([]{ (void)(numeric::CyclicPhase(3,1)*numeric::CyclicPhase(4,1)); },"cyclic different domain refusal");
    rejects<std::length_error>([]{ (void)numeric::CyclicPhase(1000000,1).regular_action(); },"cyclic allocation admission");
}

static void shape_topology() {
    using namespace rfc;
    for (const std::size_t n : {3u,4u,7u,12u}) {
        std::vector<core::IdentityKey> vertices;
        for (std::size_t j = 0; j < n; ++j) vertices.push_back(101+j*13);
        for (const auto filling : {algebra::PolygonFilling::BoundaryOnly,algebra::PolygonFilling::Disk,
                                  algebra::PolygonFilling::DoubledDiskSphere}) {
            const auto source = algebra::oriented_polygon(vertices,filling);
            require(source.grade(G{0}).cell_count() == n && source.grade(G{1}).cell_count() == n,"retained polygon identities");
            const auto ideal = field::ExactConnectionTower::ideal(source);
            require(ideal.flat_through(G{2}),"complete shape boundary family");
            const bool circle = filling == algebra::PolygonFilling::BoundaryOnly;
            const bool sphere = filling == algebra::PolygonFilling::DoubledDiskSphere;
            require(topology::integral_homology(source,G{0}).free_rank == 1,"shape connected H0");
            require(topology::integral_homology(source,G{1}).free_rank == (circle ? 1u : 0u),"circle versus filling H1");
            require(topology::integral_homology(source,G{2}).free_rank == (sphere ? 1u : 0u),"disk versus sphere H2");
            std::vector<field::ExactMetric> metrics{field::ExactMetric::identity(n),field::ExactMetric::identity(n),
                field::ExactMetric::identity(source.grade(G{2}).cell_count())};
            const field::ExactHodgeOperator hodge(source,G{1},metrics);
            const V cycle(n,Q{1});
            const auto reading = field::projector_apply(circle ? hodge.harmonic_frame() : hodge.curl_frame(),metrics[1],cycle);
            require(reading == cycle,"native circle harmonic-to-curl distinction");
            if (sphere) {
                const auto front = topology::attaching_word(source,0).loop.steps();
                const auto back = topology::attaching_word(source,1).loop.steps();
                require(back == topology::inverse_steps(front),"opposite ordered sphere attachments");
                require(ideal.map(G{2}).apply({1,1}) == V(n,Q{0}),"sphere top cycle");
            }
        }
    }
    rejects<std::invalid_argument>([]{ (void)algebra::oriented_polygon({1,2},algebra::PolygonFilling::BoundaryOnly); },"short polygon refusal");
    rejects<std::invalid_argument>([]{ (void)algebra::oriented_polygon({1,2,1},algebra::PolygonFilling::Disk); },"duplicate vertex identity refusal");
    rejects<std::invalid_argument>([]{ (void)algebra::oriented_polygon({1,2,3},static_cast<algebra::PolygonFilling>(255)); },"filling enum refusal");
}

static void ordered_transport() {
    using namespace rfc;
    auto source = presentation(2,{{1,2,1,-2,-1,-2}},true);
    const Prepared prepared(source);
    require(&prepared.source() != &source,"complete source independently owned");
    auto copy = prepared;
    const Prepared equal(source);
    require(prepared.same_owner(copy) && !prepared.same_owner(equal),"equal source bytes do not imply owner identity");
    const auto& face = prepared.faces().at(0);
    require(face.occurrences.size() == 7 && !face.occurrences.back().active &&
        face.occurrences.back().coefficient == 0 && face.occurrences.back().contribution.is_zero() &&
        prepared.source().grade(G{2}).incidence_count() == 7,"inactive occurrence retained independently");
    const auto z = F::parameter(); const F one(Q{1}),zero;
    const auto polynomial = z*z-z+one;
    require(face.derivatives == std::vector<F>({polynomial,zero-polynomial}),"ordered trefoil Laurent presentation");
    require(prepared.formal_flat() && face.holonomy == one,"trefoil formal flatness");
    const auto untwisted = prepared.specialize_common_transport(M::identity(1));
    require(untwisted.boundary_one().nonzeros() == 0 && untwisted.boundary_two() == M::from_rows({{1},{-1}}),"identity recovers source boundary");
    require(untwisted.first_homology_dimension() == 1,"untwisted trefoil H1");
    const auto generic = prepared.specialize_common_transport(M::from_rows({{2}}));
    require(generic.flat() && generic.first_homology_dimension() == 0,"generic twisted field");
    const auto exceptional = prepared.specialize_common_transport(M::from_rows({{0,-1},{1,1}}));
    require(exceptional.flat() && exceptional.boundary_two().nonzeros() == 0 && exceptional.first_homology_dimension() == 2,"rational exceptional phase field");
    source = presentation(1,{{1,1,1}});
    require(prepared.source().grade(G{1}).cell_count() == 2 && prepared.faces()[0].derivatives[0] == polynomial,"caller replacement preserves prepared source");
    const auto kept = [] {
        const Prepared temporary(presentation(2,{{1,2,1,-2,-1,-2}}));
        return temporary.specialize_common_transport(M::from_rows({{0,-1},{1,1}}));
    }();
    require(kept.source().grade(G{1}).cell_count() == 2 && kept.first_homology_dimension() == 2,"specialized output pins complete source");
    const Prepared cubic(source);
    require(!cubic.formal_flat() && cubic.faces()[0].curvature == z*z*z-one,"formal cyclic holonomy defect");
    const auto nonflat = cubic.specialize_common_transport(M::from_rows({{2}}));
    require(!nonflat.flat() && nonflat.curvature() == M::from_rows({{7}}),"full deformed curvature");
    rejects<std::domain_error>([&]{ (void)nonflat.first_homology_dimension(); },"curved homology refusal");
    const auto finite = cubic.specialize_common_transport(numeric::CyclicPhase(3,1).regular_action());
    require(finite.flat() && finite.first_homology_dimension() == 0,"finite cyclic flat specialization");
    const Prepared family(presentation(2,{{1,2,1,-2,-1,-2},{1,1,1}}));
    require(family.faces().size() == 2 && family.faces()[0].curvature.is_zero() &&
        !family.formal_flat(),"complete formal face family includes later defects");
    const auto family_generic = family.specialize_common_transport(M::from_rows({{2}}));
    require(family_generic.curvature() == M::from_rows({{0,7}}) && !family_generic.flat(),
        "complete specialized face family includes later defects");
    const auto family_finite = family.specialize_common_transport(numeric::CyclicPhase(3,1).regular_action());
    require(family_finite.flat(),"all finite face defects cancel under compatible specialization");
    const Prepared commutator(presentation(2,{{1,2,-1,-2}}));
    const Prepared cancellation(presentation(2,{{1,-1,2,-2}}));
    const auto ordinary_commutator = commutator.specialize_common_transport(M::identity(1));
    const auto ordinary_cancellation = cancellation.specialize_common_transport(M::identity(1));
    require(ordinary_commutator.boundary_two() == ordinary_cancellation.boundary_two(),"same ordinary realized boundary");
    require(commutator.faces()[0].derivatives != cancellation.faces()[0].derivatives,"retained order distinguishes cyclic transported boundaries");
    const Prepared negative(presentation(1,{{-1,-1,-1}}));
    require(negative.faces()[0].curvature == one/(z*z*z)-one,"negative Laurent holonomy retained");
    const auto negative_transport = negative.specialize_common_transport(M::from_rows({{2}}));
    require(negative_transport.curvature() == M::from_rows({{Q{-7,8}}}),"negative exact transport");
    const Prepared circle(presentation(1,{}));
    require(circle.formal_flat() && circle.faces().empty(),"empty face grade retained");
    require(circle.specialize_common_transport(M::identity(1)).first_homology_dimension() == 1,"circle identity-character field");
    require(circle.specialize_common_transport(Phase::cayley(Q{1,2}).real_action()).first_homology_dimension() == 0,"circle nontrivial phase field");
    rejects<std::invalid_argument>([&]{ (void)circle.specialize_common_transport(M(1,2)); },"rectangular transport refusal");
    rejects<std::invalid_argument>([&]{ (void)circle.specialize_common_transport(M(0,0)); },"empty transport refusal");
    rejects<std::exception>([&]{ (void)circle.specialize_common_transport(M(1,1)); },"singular transport refusal");
    topology::CyclicAttachmentLimits limits;
    limits.max_cells = 0;
    rejects<std::length_error>([&]{ (void)Prepared(source,limits); },"source cell admission");
    limits = {}; limits.max_occurrences = 0;
    rejects<std::length_error>([&]{ (void)Prepared(source,limits); },"source occurrence admission");
    limits = {}; limits.max_coefficient_slots = 0;
    rejects<std::length_error>([&]{ (void)Prepared(source,limits); },"coefficient output admission");
    limits = {}; limits.max_transport_dimension = 1;
    const Prepared bounded(source,limits);
    rejects<std::length_error>([&]{ (void)bounded.specialize_common_transport(M::identity(2)); },"transport dimension admission");
    limits = {}; limits.max_block_coordinate_slots = 0;
    const Prepared zero_slots(source,limits);
    rejects<std::length_error>([&]{ (void)zero_slots.specialize_common_transport(M::identity(1)); },"block slot admission");
    rejects<std::invalid_argument>([]{ const auto polygon = algebra::oriented_polygon({1,2,3},algebra::PolygonFilling::Disk); (void)Prepared(polygon); },"unsupported presentation profile refusal");
}

int main() {
    try {
        phases_and_geometry(); shape_topology(); ordered_transport();
        std::cout << "{\"checks\":" << checks << ",\"failed\":0,\"domains\":[\"Q\",\"Q(i)\",\"cyclic group\",\"retained Laurent presentation\"]}\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "failure after " << checks << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
