#include "rfc/field/sparse_stalk_actions.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace rfc;
using Q = numeric::Rational;
using S = numeric::SparseRationalVector;
using M = numeric::ExactMatrix;
using E = numeric::SparseRationalEntry;
using F = field::SparseStalkField;
using A = field::SparseStalkFieldActions;
using Owner = local::OwnedLinearFiberSystem;
std::size_t checks{}, adjoint_pairs{}, dirac_columns{};

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function, const char* message) {
    bool caught = false;
    try { function(); } catch (const std::exception&) { caught = true; }
    check(caught, message);
}
bool same(const F& a, const F& b) {
    if (!a.owner().same_owner(b.owner()) || a.grade() != b.grade() || a.values().size() != b.values().size()) return false;
    auto left = a.values().begin(), right = b.values().begin();
    for (; left != a.values().end(); ++left, ++right)
        if (left->first != right->first || left->second.size() != right->second.size() ||
            left->second.entries() != right->second.entries()) return false;
    return true;
}
core::RelationalComplex triangle() {
    core::RelationSignature signature;
    const auto edge = signature.add_type("temporal-edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("temporal-filling", core::BoundaryPolicy::DeclaredExact, 3, 3);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{101}, edge, {
        {10, true, true, Q{1}, {}, core::OccurrenceKey{1001}},
        {20, true, false, Q{1}, {}, core::OccurrenceKey{1002}}});
    builder.add_grade_one(core::RelationKey{102}, edge, {
        {20, true, true, Q{1}, {}, core::OccurrenceKey{1003}},
        {30, true, false, Q{1}, {}, core::OccurrenceKey{1004}}});
    builder.add_grade_one(core::RelationKey{103}, edge, {
        {30, true, true, Q{1}, {}, core::OccurrenceKey{1005}},
        {10, true, false, Q{1}, {}, core::OccurrenceKey{1006}}});
    builder.add_higher(core::Grade{2}, core::RelationKey{201}, face, {
        {0, true, false, Q{1}, {}, core::OccurrenceKey{2001}},
        {1, true, false, Q{1}, {}, core::OccurrenceKey{2002}},
        {2, true, false, Q{1}, {}, core::OccurrenceKey{2003}}});
    return builder.freeze();
}
M rotation() { return M::from_rows({{Q{3,5}, Q{-4,5}}, {Q{4,5}, Q{3,5}}}); }
Owner owner(int phase = 0) {
    const auto source = triangle();
    local::LinearRelationalFiberSystem fibers(local::LinearStalkBundle(source, 2));
    if (phase == 1 || phase == 2) fibers.set_transport({1}, 1, rotation());
    if (phase == 2) fibers.set_transport({1}, 2, rotation().scale(Q{-1}));
    if (phase == 3) {
        const auto shear = M::from_rows({{1,1},{0,1}});
        fibers.set_transport({1}, 1, shear);
        fibers.set_transport({1}, 2, shear.scale(Q{-1}));
    }
    return Owner(fibers);
}
field::SparseStalkMetrics metrics(const Owner& owner) {
    field::SparseStalkMetrics result;
    const auto& fibers = owner.fibers();
    for (std::size_t k = 0; k < fibers.complex().grade_count(); ++k) {
        const core::Grade grade{static_cast<std::uint16_t>(k)};
        result.emplace_back();
        for (std::size_t i = 0; i < fibers.complex().grade(grade).cell_count(); ++i)
            result.back().push_back(field::ExactMetric::identity(fibers.stalks().dimension(
                {grade, static_cast<std::uint32_t>(i)})));
    }
    return result;
}
std::vector<F> zero_tower(const A& actions) {
    std::vector<F> result;
    for (std::size_t k = 0; k < actions.owner().fibers().complex().grade_count(); ++k)
        result.push_back(actions.bind({static_cast<std::uint16_t>(k)}));
    return result;
}

void adjoint_certificate(const A& actions) {
    const auto& fibers = actions.owner().fibers();
    for (std::uint16_t k = 1; k < fibers.complex().grade_count(); ++k) {
        const core::Grade upper{k}, lower{static_cast<std::uint16_t>(k - 1u)};
        for (std::uint32_t u = 0; u < fibers.complex().grade(upper).cell_count(); ++u)
            for (std::size_t i = 0; i < fibers.stalks().dimension({upper,u}); ++i)
                for (std::uint32_t v = 0; v < fibers.complex().grade(lower).cell_count(); ++v)
                    for (std::size_t j = 0; j < fibers.stalks().dimension({lower,v}); ++j) {
                        const auto x = actions.bind(upper, {{u, S::basis(fibers.stalks().dimension({upper,u}), i)}});
                        const auto y = actions.bind(lower, {{v, S::basis(fibers.stalks().dimension({lower,v}), j)}});
                        const auto down = actions.boundary(x), up = actions.coboundary(upper, y);
                        check(actions.inner(down.field, y) == actions.inner(x, up.field),
                              "complete sparse stalk adjoint pairing");
                        ++adjoint_pairs;
                    }
    }
}

void strict_and_dirac() {
    const A actions(owner());
    const auto face = actions.bind({2}, {{0, S::basis(2, 0)}});
    const auto cycle = actions.boundary(face);
    check(cycle.field.active_stalks() == 3 && cycle.field.nonzeros() == 3, "face retains three coupled edge stalks");
    check(actions.chain_defect(face).field.zero(), "strict vector-stalk triangle closes");
    check(actions.inner(cycle.field, cycle.field) == 3, "triangle circulation quadrance");
    const auto zero = actions.bind({1}, {{0,S(2)}});
    check(zero.zero() && actions.owner().fibers().complex().grade({1}).cell_count() == 3,
          "zero numerical stalk does not remove its retained relation");
    const auto empty_boundary = actions.boundary(zero);
    check(empty_boundary.field.zero() && empty_boundary.work.incidences == 0, "zero field has zero incidence traversal");
    adjoint_certificate(actions);
    for (std::uint16_t k = 0; k < 3; ++k)
        for (std::uint32_t cell = 0; cell < actions.owner().fibers().complex().grade({k}).cell_count(); ++cell)
            for (std::size_t coordinate = 0; coordinate < 2; ++coordinate) {
                auto input = zero_tower(actions);
                input[k] = actions.bind({k}, {{cell,S::basis(2,coordinate)}});
                const auto first = actions.dirac(input), second = actions.dirac(first.fields);
                const auto anti = actions.dirac(input,true), anti_second = actions.dirac(anti.fields,true);
                const auto left = actions.dirac(anti.fields), right = actions.dirac(first.fields,true);
                for (std::size_t grade = 0; grade < 3; ++grade) {
                    const auto hodge = actions.hodge(input[grade]);
                    check(same(second.fields[grade],hodge.field), "D squared equals Hodge on every finite stalk basis");
                    check(same(anti_second.fields[grade],actions.scaled(hodge.field,Q{-1})), "anti Dirac square equals negative Hodge");
                    check(actions.plus(left.fields[grade],right.fields[grade]).zero(), "Dirac anti Dirac anticommutator vanishes");
                }
                ++dirac_columns;
            }
    const auto foreign = A(owner());
    rejects([&] { (void)foreign.boundary(face); }, "equal-valued foreign fiber owner rejected");
    rejects([&] { (void)actions.inner(face,foreign.bind({2})); }, "foreign field pairing rejected");
    rejects([&] { (void)actions.bind({2},{{0,S(3)}}); }, "wrong stalk value dimension rejected");
    rejects([&] { (void)actions.bind({2},{{1,S(2)}}); }, "unretained cell rejected even at zero amplitude");
    rejects([&] { (void)actions.coboundary({2},face); }, "wrong adjoint input grade rejected");
    const auto copy = actions;
    check(copy.same_metric_owner(actions), "copied action retains actual metric owner");
    const auto another_metric_owner = A(actions.owner());
    check(!another_metric_owner.same_metric_owner(actions), "equal values do not merge metric owners");
    check(same(another_metric_owner.boundary(face).field,cycle.field), "explicit alternative metric accepts the same fiber field");
}

void directional_defect_and_repair() {
    const A broken(owner(1)), repaired(owner(2));
    const auto input = broken.bind({2},{{0,S::basis(2,0)}});
    const auto defect = broken.chain_defect(input);
    const auto expected = broken.bind({0},{{1,S(2,{{0,Q{-2,5}},{1,Q{4,5}}})}});
    check(same(defect.field,expected), "rational rotation produces exact directional defect");
    check(broken.inner(defect.field,defect.field) == Q{4,5}, "directional defect quadrance is four fifths");
    const auto other = broken.chain_defect(broken.bind({2},{{0,S::basis(2,1)}}));
    check(broken.inner(other.field,other.field) == Q{4,5}, "second direction witnesses complete operator defect");
    check(broken.inner(defect.field,defect.field) + broken.inner(other.field,other.field) == Q{8,5},
          "full two-column identity-metric defect quadrance");
    auto broken_tower = zero_tower(broken); broken_tower[2] = input;
    const auto first = broken.dirac(broken_tower), second = broken.dirac(first.fields);
    check(same(second.fields[0],defect.field), "curved Dirac square retains the grade-two chain defect instead of claiming a strict Hodge identity");
    check(repaired.chain_defect(repaired.bind({2},input.values())).field.zero(), "one additional signed incidence block repairs closure");
    const auto r = rotation();
    check(r.transpose().multiply(r) == M::identity(2), "injected rotation preserves local magnitudes");
    check(defect.work.map_entries == 4, "only active explicit transport entries are visited");
    const auto& bad = broken.owner().fibers();
    const auto& fixed = repaired.owner().fibers();
    std::size_t changed{};
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto a = bad.flag_map({1},slot).transport;
        const auto b = fixed.flag_map({1},slot).transport;
        if (a != b) ++changed;
    }
    check(changed == 1, "exactly one further incidence block changes in the repair");
    adjoint_certificate(repaired);
}

void metric_pullback_and_rectangular_stalk() {
    const A old(owner());
    const auto moved_owner = owner(3);
    auto local_metrics = metrics(moved_owner);
    local_metrics[0][1] = field::ExactMetric(M::from_rows({{1,-1},{-1,2}}));
    const A moved(moved_owner,std::move(local_metrics));
    const auto x_old = old.bind({1},{{0,S(2,{{0,2},{1,3}})}});
    const auto x_new = moved.bind({1},x_old.values());
    const auto old_boundary = old.boundary(x_old), new_boundary = moved.boundary(x_new);
    check(old.inner(old_boundary.field,old_boundary.field) == 26, "old two endpoint energy");
    check(moved.inner(new_boundary.field,new_boundary.field) == 26, "pullback metric preserves boundary quadrance under shear chart");
    const auto old_up = old.coboundary({1},old_boundary.field);
    const auto new_up = moved.coboundary({1},new_boundary.field);
    check(same(moved.bind({1},old_up.field.values()),new_up.field), "aligned coboundary survives chart and metric pullback");
    const auto stale = A(moved_owner);
    check(stale.inner(new_boundary.field,new_boundary.field) == 47, "stale metric has a distinct exact measurement");
    adjoint_certificate(moved);

    const auto source = triangle();
    local::LinearStalkBundle bundle(source,2); bundle.set_dimension({{0},1},3);
    local::LinearRelationalFiberSystem fibers(std::move(bundle));
    rejects([&] { (void)A(Owner(fibers)); }, "rectangular incidence requires explicit transport even before an active field");
    M embedding(3,2); embedding.set(0,0,1); embedding.set(1,1,1);
    fibers.set_transport({1},1,embedding); fibers.set_transport({1},2,embedding.scale(-1));
    const A expanded{Owner(fibers)};
    const auto face = expanded.bind({2},{{0,S(2,{{0,2},{1,3}})}});
    check(expanded.chain_defect(face).field.zero(), "stalk expansion preserves the full directional chain");
    const auto new_direction = expanded.bind({0},{{1,S::basis(3,2)}});
    check(expanded.coboundary({1},new_direction).field.zero(), "new unresolved stalk direction has no invented boundary coupling");
    check(expanded.inner(new_direction,new_direction) == 1, "unresolved direction still has a declared metric");
    adjoint_certificate(expanded);

    auto unsupported = metrics(expanded.owner());
    unsupported[0][1] = field::ExactMetric::certified_external(3,
        [](const numeric::RationalVector& x) { return x; },
        [](const numeric::RationalVector& x) { return x; });
    rejects([&] { (void)A(expanded.owner(),unsupported); }, "metric without sparse ports cannot trigger a dense fallback");
}

void huge_symbolic_stalk() {
    constexpr std::size_t n = 1000003;
    core::RelationSignature signature;
    const auto edge = signature.add_type("large-stalk-edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge,{{10,true,true,1},{20,true,false,1}});
    const auto source = builder.freeze();
    const local::LinearRelationalFiberSystem fibers(local::LinearStalkBundle(source,n));
    const A actions{Owner(fibers)};
    const auto input = actions.bind({1},{{0,S(n,{{3,Q{3,2}},{n-1,Q{-7,3}}})}});
    const auto down = actions.boundary(input);
    check(down.field.nonzeros() == 4 && down.field.active_stalks() == 2, "million-coordinate stalk visits only its four realized values");
    check(down.work.active_stalks == 1 && down.work.incidences == 2 && down.work.map_entries == 0 && down.work.scalar_products == 4,
          "symbolic identity boundary work is independent of declared stalk dimension");
    check(down.work.peak_numeric_nonzeros == 4, "largest observed numeric support remains four");
    const auto up = actions.coboundary({1},down.field);
    check(same(up.field,actions.scaled(input,Q{2})), "huge sparse adjoint has exact two times input");
    check(up.work.active_stalks == 2 && up.work.incidences == 2 && up.work.map_entries == 0 &&
          up.work.scalar_products == 4 && up.work.metric_apply_calls == 2 && up.work.metric_solve_calls == 1,
          "huge sparse adjoint traverses only actual cofaces and metric ports");
    check(up.work.peak_numeric_nonzeros == 4, "huge adjoint produces bounded numeric support");
    check(same(actions.hodge(input).field,actions.scaled(input,Q{2})), "huge stalk Hodge composes native sparse actions");
}

void retained_inactive_occurrences() {
    core::RelationSignature signature;
    const auto edge = signature.add_type("retained-occurrence-edge",core::BoundaryPolicy::DeclaredExact,2,0);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge,{{10,true,true,1},{20,true,false,1},{10,true,false,0},{10,false,false,1}});
    const auto source = builder.freeze();
    local::LinearRelationalFiberSystem fibers(local::LinearStalkBundle(source,2));
    fibers.set_transport({1},3,M::identity(2).scale(Q{100}));
    const A actions{Owner(fibers)};
    const auto input = actions.bind({1},{{0,S::basis(2,1,Q{3})}});
    const auto down = actions.boundary(input);
    const auto expected = actions.bind({0},{{0,S::basis(2,1,Q{-3})},{1,S::basis(2,1,Q{3})}});
    check(same(down.field,expected), "inactive occurrence cannot activate its declared override");
    check(down.work.incidences == 4 && down.work.map_entries == 0 && down.work.scalar_products == 2,
          "existing-zero and absent occurrences retain distinct source records without arithmetic contributions");
    check(actions.owner().fibers().complex().grade({1}).incidence_count() == 4,
          "sparse action retains every original occurrence");
    const auto up = actions.coboundary({1},down.field);
    check(same(up.field,actions.scaled(input,Q{2})), "inactive override is also skipped in reverse coface action");
    adjoint_certificate(actions);
}
}

int main() {
    try {
        strict_and_dirac();
        directional_defect_and_repair();
        metric_pullback_and_rectangular_stalk();
        huge_symbolic_stalk();
        retained_inactive_occurrences();
        std::cout << "sparse stalk actions: " << checks << " checks passed; adjoint basis pairs=" << adjoint_pairs
                  << "; complete Dirac basis columns=" << dirac_columns << "; largest stalk=1000003\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
