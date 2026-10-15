#include "rfc/core/builder.hpp"
#include "rfc/learning/graded_transport_fit.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

using namespace rfc;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using Guard = numeric::ExactActionGuard;
using Fit = learning::PreparedGradedTransportFit;

namespace {
std::size_t checks{};
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
core::Grade grade(std::size_t k) { return {static_cast<std::uint16_t>(k)}; }

numeric::ExactActionLimits limits() {
    auto out = numeric::ExactActionLimits{};
    out.dimension = 16'000'048;
    out.entries = 50'000;
    out.steps = 1'000'000;
    out.integer_bits = 256;
    return out;
}

// Every stalk has two represented axes, first and last. Unused coordinates
// alter no occurrence, parameter, coefficient, or intended chain constraint.
learning::PreparedTransportObservations fixture(std::size_t dim,
                                                std::size_t selected,
                                                Guard& guard) {
    core::RelationSignature signature;
    const auto type = signature.add_type("sparse-boundary", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(type, {{10,true,true,1}, {20,true,false,1},
        {10,true,false,Q{1,2}}, {10,true,true,Q{1,2}},
        {20,false,false,7}, {30,true,false,0}});
    builder.add_grade_one(type, {{20,true,true,1}, {30,true,false,1}});
    builder.add_grade_one(type, {{10,true,true,1}, {30,true,false,1}});
    for (int i = 0; i < 2; ++i)
        builder.add_higher(grade(2), type, {{0,true,false,1}, {1,true,false,1}, {2,true,true,1}});
    builder.add_higher(grade(3), type, {{0,true,false,1}, {1,true,true,1}});
    const auto source = builder.freeze();
    local::LinearStalkBundle stalks(source, dim);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> fixed, variables;
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const auto g = grade(k);
        const auto& arena = source.grade(g);
        for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
            M map(dim, dim);
            for (const auto axis : {std::size_t{0}, dim - 1}) {
                if (k == selected && p != 0) {
                    variables.push_back({p, axis, axis});
                } else {
                    map.set(axis, axis, source.incidence_coefficient(g, p));
                    if (k == selected) fixed.push_back({p, axis, axis});
                }
            }
            fibers.set_transport(g, p, std::move(map));
        }
    }
    return learning::PreparedTransportObservations::at_grade(
        local::OwnedLinearFiberSystem(fibers), grade(selected), fixed, variables, guard);
}

// Independent small-coordinate assembly: compare the large realization after
// selecting the two explicitly represented axes per cell with a compact one.
M selected_boundary(const local::LinearRelationalFiberSystem& fibers,
                    std::size_t k, std::size_t dim) {
    const auto& source = fibers.complex();
    const auto g = grade(k);
    const auto& arena = source.grade(g);
    M out(source.grade(grade(k-1)).cell_count()*2, arena.cell_count()*2);
    for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
        if (!arena.aspects.get(source.signature().existence_aspect(), p)) continue;
        for (const auto& [key, q] : fibers.transport_override(g,p)->entries()) {
            check(key.first == 0 || key.first == dim-1, "unexpected represented output axis");
            check(key.second == 0 || key.second == dim-1, "unexpected represented input axis");
            const auto row = arena.lower_index[p]*2 + (key.first != 0);
            const auto col = arena.upper_index[p]*2 + (key.second != 0);
            out.set(row, col, out.get(row,col) + q);
        }
    }
    return out;
}

void compare_constraints(const Fit& compact, const Fit& expanded, std::size_t dim) {
    check(compact.chain() == expanded.chain(), "chain coefficients depend on unused axes");
    check(compact.chain_target() == expanded.chain_target(), "affine defect target depends on unused axes");
    check(compact.design() == expanded.design(), "design depends on unused axes");
    check(compact.target() == expanded.target(), "target depends on unused axes");
    check(compact.particular() == expanded.particular(), "particular solution depends on unused axes");
    check(compact.kernel() == expanded.kernel(), "complete kernel depends on unused axes");
    check(compact.pivots() == expanded.pivots(), "pivot identity changed");
    check(compact.free_columns() == expanded.free_columns(), "free parameter identity changed");
    check(compact.retained_entries() == expanded.retained_entries(), "retained entries scale with unused axes");
    check(compact.preparation_steps() == expanded.preparation_steps(), "preparation steps scale with unused axes");
    check(compact.update().execution_steps == expanded.update().execution_steps, "execution steps scale with unused axes");
    check(compact.chain_coordinates().size() == expanded.chain_coordinates().size(), "constraint identities lost");
    for (std::size_t i = 0; i < compact.chain_coordinates().size(); ++i) {
        const auto& a = compact.chain_coordinates()[i];
        const auto& b = expanded.chain_coordinates()[i];
        const auto mapped = [dim](std::size_t coordinate) {
            return (coordinate/2)*dim + (coordinate%2 ? dim-1 : 0);
        };
        check(a.grade == b.grade && mapped(a.row) == b.row && mapped(a.column) == b.column,
            "retained chain coordinate was compressed or renamed");
    }
}

void run() {
    for (std::size_t selected = 1; selected <= 3; ++selected) {
        Guard cg(limits());
        const Fit compact(fixture(2, selected, cg), {}, cg);
        const auto compact_owner = compact.materialize(V(compact.free_columns().size(), Q{2,3}), cg);
        for (const auto dim : {std::size_t{257}, std::size_t{1'000'003}}) {
            Guard g(limits());
            const Fit expanded(fixture(dim, selected, g), {}, g);
            compare_constraints(compact, expanded, dim);
            check(expanded.model().owner().fibers().stalks().dimension({grade(0),0}) == dim,
                "declared stalk dimension was replaced");
            const auto& a = expanded.model().owner().fibers().complex().grade(grade(1));
            check(a.incidence_count() == 10, "source occurrences changed");
            check(!a.aspects.get(expanded.model().owner().fibers().complex().signature().existence_aspect(),4),
                "inactive occurrence no longer retained as inactive");
            check(expanded.model().owner().fibers().complex().incidence_coefficient(grade(1),5) == 0,
                "supported zero occurrence lost");
            const auto owner = expanded.materialize(V(expanded.free_columns().size(), Q{2,3}), g);
            M previous;
            for (std::size_t k = 1; k < owner.fibers().complex().grade_count(); ++k) {
                const auto b = selected_boundary(owner.fibers(), k, dim);
                const auto reference = local::GradeTensorField(compact_owner.fibers(), grade(k)).matrix();
                check(b == reference, "sparse materialization differs from compact native reference");
                if (k > 1) check(previous.multiply(b).nonzeros() == 0, "repaired realization is not closed");
                previous = b;
            }
            std::cout << "grade=" << selected << " dimension=" << dim
                      << " retained=" << expanded.retained_entries()
                      << " preparation_steps=" << expanded.preparation_steps()
                      << " chain_rows=" << expanded.chain().rows()
                      << " parameters=" << expanded.model().parameter_count() << '\n';
        }
        // Tight entry limits must still reject genuinely insufficient budgets.
        auto narrow = limits(); narrow.entries = 1;
        bool rejected = false;
        try { Guard g(narrow); const Fit bad(compact.model(), {}, g); }
        catch (const std::length_error&) { rejected = true; }
        check(rejected, "stored-entry admission disabled by sparse refactor");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--admission-probe") {
            Guard g(limits());
            const Fit fit(fixture(1'000'003, 2, g), {}, g);
            std::cout << "large sparse family admitted: retained=" << fit.retained_entries()
                      << " preparation_steps=" << fit.preparation_steps() << '\n';
        } else if (argc == 1) {
            run();
            std::cout << checks << " sparse graded transport checks passed\n";
        } else {
            throw std::invalid_argument("usage: test_graded_transport_sparse [--admission-probe]");
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
