#include "rfc/core/builder.hpp"
#include "rfc/temporal/rotating_coefficient.hpp"
#include <iostream>

using namespace rfc;
using namespace temporal;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
using System = OwnedConsistentLinearSystem;
using Rotation = PreparedRotatingCoefficient;
static std::size_t checks{};
static void require(bool yes, const char *why) {
    ++checks;
    if (!yes)
        throw std::runtime_error(why);
}
template <class Error = std::invalid_argument, class F> static void rejects(F f, const char *why) {
    bool bad = false;
    try {
        f();
    } catch (const Error &) {
        bad = true;
    }
    require(bad, why);
}
static core::RelationalComplex matrix_source(const M &matrix, bool retained = false, bool empty_upper = false,
                                             bool face = false) {
    core::RelationSignature signature;
    const auto equation = signature.add_type("equation", core::BoundaryPolicy::DeclaredExact);
    const auto syzygy = signature.add_type("syzygy", core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder b(signature);
    for (std::size_t v = 0; v < matrix.cols(); ++v)
        b.declare_witness_identity(v);
    for (std::size_t k = 0; k < matrix.rows(); ++k) {
        std::vector<core::BoundaryOccurrenceInput> row;
        for (std::size_t v = 0; v < matrix.cols(); ++v) {
            const auto a = matrix.get(k, v);
            if (a != 0)
                row.emplace_back(v, true, a < 0, Q{a < 0 ? -a : a});
        }
        if (retained && k == 0) {
            row.clear();
            row.emplace_back(0, true, false, Q{1});
            row.emplace_back(0, true, false, Q{1});
            row.emplace_back(0, true, false, Q{0});
            row.emplace_back(0, false, false, Q{11});
            row.emplace_back(0, true, false, Q{2});
            row.emplace_back(0, true, true, Q{2});
        }
        b.add_grade_one(equation, row);
    }
    if (empty_upper)
        b.declare_grade({2});
    if (face)
        b.add_higher({2}, syzygy, {{0, true, false, false, 1}, {0, true, true, false, 1}});
    return b.freeze();
}
static geometry::UnivariateDomain domain(G &g, std::vector<V> holes = {}, bool closed = true) {
    return geometry::UnivariateDomain::declare(geometry::UnivariateCoordinate::declare("s", 12), Q{-16},
                                               Q{16}, closed, closed, std::move(holes), g);
}
static M original_matrix() {
    return M::from_rows({{-1, 1, 0, 0, 0, 0},
                         {0, -1, 1, 0, 0, 0},
                         {0, 0, Q{1, 2}, Q{1, 2}, -1, 0},
                         {0, 0, 0, -1, 0, 1},
                         {0, 0, 0, 0, 1, -1},
                         {0, 2, 3, 0, -1, 0},
                         {1, 0, 0, 0, 0, 0},
                         {-1, 0, 0, 1, 0, 0}});
}
static core::RelationalComplex original_source() {
    core::RelationSignature signature;
    auto equation = signature.add_type("original-equation", core::BoundaryPolicy::DeclaredExact);
    core::RelationalComplexBuilder b(signature);
    for (std::size_t v = 0; v < 6; ++v)
        b.declare_witness_identity(v);
    const std::vector<std::vector<std::pair<std::size_t, Q>>> rows{
        {{0, -1}, {1, 1}}, {{1, -1}, {2, 1}}, {{4, -1}, {2, Q{1, 2}}, {3, Q{1, 2}}},
        {{3, -1}, {5, 1}}, {{5, -1}, {4, 1}}, {{1, 2}, {2, 3}, {4, -1}},
        {{0, 1}},          {{0, -1}, {3, 1}}};
    for (const auto &r : rows) {
        std::vector<core::BoundaryOccurrenceInput> flags;
        for (const auto &[v, a] : r)
            flags.emplace_back(v, true, a < 0, Q{a < 0 ? -a : a});
        b.add_grade_one(equation, flags);
    }
    return b.freeze();
}
static void qj(const Q &q) {
    std::cout << '"' << q << '"';
}
static void vj(const V &v) {
    std::cout << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            std::cout << ',';
        qj(v[i]);
    }
    std::cout << ']';
}
static void mj(const M &m) {
    std::cout << '[';
    for (std::size_t i = 0; i < m.rows(); ++i) {
        if (i)
            std::cout << ',';
        V row;
        for (std::size_t j = 0; j < m.cols(); ++j)
            row.push_back(m.get(i, j));
        vj(row);
    }
    std::cout << ']';
}
static void sample_json(const Rotation::Sample &p) {
    std::cout << "{\"s\":";
    if (p.point())
        qj(p.point()->value());
    else
        std::cout << "null";
    std::cout << ",\"h\":[";
    qj(p.phase_real());
    std::cout << ',';
    qj(p.phase_imaginary());
    std::cout << "],\"d\":";
    qj(p.distance_squared());
    std::cout << ",\"denominator\":";
    qj(p.denominator());
    std::cout << ",\"auxiliary\":[";
    qj(p.auxiliary_real());
    std::cout << ',';
    qj(p.auxiliary_imaginary());
    std::cout << "],\"Q\":";
    qj(p.obstruction());
    std::cout << ",\"u_re\":";
    vj(p.solution_real());
    std::cout << ",\"u_im\":";
    vj(p.solution_imaginary());
    std::cout << ",\"r_re\":";
    vj(p.residual_real());
    std::cout << ",\"r_im\":";
    vj(p.residual_imaginary());
    std::cout << ",\"solvable\":" << (p.solvable() ? "true" : "false") << '}';
}
static void run_case(const char *name, M matrix, V truth, std::size_t row, std::size_t local, bool dump,
                     bool retained = false, bool empty_upper = false) {
    G g;
    core::OwnedRelationalSource owner(std::string_view(name) == "original-eight-equations"
                                          ? original_source()
                                          : matrix_source(matrix, retained, empty_upper));
    auto rhs = numeric::checked_apply(matrix, truth, g);
    const auto system = System::declare(owner, rhs, g);
    auto selected = system.coefficient(system.row(row, g), local, g);
    auto d = domain(g);
    auto plan = Rotation::prepare(system, selected, d, g);
    const auto &p = plan.parameters();
    require(system.solution() == truth, "original unique real solution");
    require(system.owner().same_owner(owner), "actual retained source owner");
    require(system.frames().gradient_frame().cols() == truth.size(), "complete column rank");
    require(system.dual_response().kernel().cols() == rhs.size() - truth.size(),
            "complete potential adjoint-kernel gauge");
    require(p.kappa > 0 && p.g_vv > 0 && p.minimum_denominator > 0, "positive theorem directions");
    require(system.owner().source().grade({1}).incidence_count() ==
                owner.source().grade({1}).incidence_count(),
            "all original flags retained");
    std::vector<Q> points;
    if (std::string_view(name) == "original-eight-equations") {
        for (const char *s :
             {"-16",   "-8",   "-4",   "-3",    "-2",    "-3/2",   "-1",      "-2/3", "-1/2",
              "-1/3",  "-1/4", "-1/8", "-1/16", "-1/64", "-1/256", "-1/1024", "0",    "1/1024",
              "1/256", "1/64", "1/16", "1/8",   "1/4",   "1/3",    "1/2",     "2/3",  "1",
              "3/2",   "2",    "3",    "4",     "8",     "16"})
            points.emplace_back(s);
        require(p.a == -1 && p.kappa == Q{15, 641} && p.m_v == Q{80, 641} && p.g_vv == Q{1069, 641},
                "original native coefficients");
        require(p.q == Q{115, 641} && p.numerator == Q{375, 641}, "original rational closed form");
    } else
        points = {Q{-16}, Q{-3, 2}, Q{-1}, Q{-1, 4}, Q{0}, Q{1, 4}, Q{1, 2}, Q{1}, Q{3, 2}, Q{16}};
    if (dump) {
        std::cout << "{\"name\":\"" << name << "\",\"M\":";
        mj(matrix);
        std::cout << ",\"source_flags\":[";
        const auto &arena = owner.source().grade({1});
        for (std::size_t j = 0; j < arena.incidence_count(); ++j) {
            if (j)
                std::cout << ',';
            std::cout << '[' << arena.upper_index[j] << ',' << arena.lower_index[j] << ','
                      << (arena.aspects.get(owner.source().signature().existence_aspect(), j) ? "true"
                                                                                              : "false")
                      << ',';
            const Q raw = owner.source().grade({0}).cell_scales[arena.lower_index[j]] *
                          Q{arena.lattice_coefficient[j]} / arena.cell_scales[arena.upper_index[j]];
            qj(raw);
            std::cout << ']';
        }
        std::cout << ']';
        std::cout << ",\"rhs\":";
        vj(rhs);
        std::cout << ",\"truth\":";
        vj(truth);
        std::cout << ",\"row\":" << row << ",\"variable\":" << selected.variable() << ",\"a\":";
        qj(p.a);
        std::cout << ",\"kernel\":";
        mj(system.dual_response().kernel());
        std::cout << ",\"kappa\":";
        qj(p.kappa);
        std::cout << ",\"m\":";
        vj(plan.minimal_potential());
        std::cout << ",\"G_column\":";
        vj(plan.inverse_column());
        std::cout << ",\"harmonic\":";
        vj(plan.harmonic_row());
        std::cout << ",\"x\":";
        qj(p.x);
        std::cout << ",\"q\":";
        qj(p.q);
        std::cout << ",\"minimum_denominator\":";
        qj(p.minimum_denominator);
        std::cout << ",\"numerator\":";
        qj(p.numerator);
        std::cout << ",\"samples\":[";
    }
    bool first = true;
    for (const auto &s : points) {
        auto sample = plan.sample(d.at(s, g), g);
        plan.validate_sample(sample, g);
        require(sample.denominator() > 0 && !sample.half_turn(),
                "finite chart regularity including closed endpoints");
        require(sample.solvable() == (p.x == 0 || s == 0), "whole joint solvability criterion");
        if (s == 0)
            require(sample.solution_real() == truth && numeric::is_zero(sample.solution_imaginary()),
                    "trivial phase exact solution");
        if (dump) {
            if (!first)
                std::cout << ',';
            sample_json(sample);
            first = false;
        }
    }
    auto end = plan.half_turn(g);
    require(end.half_turn() && numeric::is_zero(end.solution_imaginary()) &&
                numeric::is_zero(end.residual_imaginary()),
            "separately declared real half-turn");
    if (std::string_view(name) == "original-eight-equations")
        require(end.obstruction() == Q{500, 367}, "original half-turn500/367");
    if (std::string_view(name) == "auxiliary-zero")
        require(end.auxiliary_real() == 0 && end.denominator() == 1 && end.obstruction() == 2,
                "removable auxiliary zero crossed");
    if (std::string_view(name) == "negative-q")
        require(p.q == Q{-1, 6} && end.denominator() == Q{1, 3},
                "valid negative q with positive denominator");
    if (dump) {
        if (!first)
            std::cout << ',';
        sample_json(end);
        std::cout << "]}";
    }
}
static void refusal_checks() {
    G g;
    auto matrix = M::from_rows({{1}, {1}});
    core::OwnedRelationalSource owner(matrix_source(matrix));
    V c{1, 1};
    auto sys = System::declare(owner, c, g);
    auto row = sys.row(0, g);
    auto coefficient = sys.coefficient(row, 0, g);
    auto d = domain(g);
    auto plan = Rotation::prepare(sys, coefficient, d, g);
    rejects([&] { (void)System::declare(owner, V{1}, g); }, "partial right-hand side rejected");
    rejects([&] { (void)System::declare(owner, V{1, 2}, g); }, "inconsistent original system rejected");
    rejects(
        [&] {
            core::OwnedRelationalSource s(matrix_source(M::from_rows({{1, 1}, {2, 2}})));
            (void)System::declare(s, V{1, 2}, g);
        },
        "full-column rank required");
    rejects(
        [&] {
            core::OwnedRelationalSource s(matrix_source(M::from_rows({{1, 2}})));
            (void)System::declare(s, V{1}, g);
        },
        "underdetermined potential refused");
    rejects(
        [&] {
            core::OwnedRelationalSource s(matrix_source(M::identity(2)));
            auto x = System::declare(s, V{1, 2}, g);
            (void)Rotation::prepare(x, x.coefficient(x.row(0, g), 0, g), d, g);
        },
        "kappa zero theorem refused");
    rejects([&] { (void)System::declare(owner, c, std::vector<M>{M::identity(1)}, g); },
            "partial metrics refused");
    rejects([&] { (void)System::declare(owner, c, std::vector<M>{M::from_rows({{2}}), M::identity(2)}, g); },
            "nonidentity unknown metric refused");
    rejects(
        [&] {
            (void)System::declare(owner, c, std::vector<M>{M::identity(1), M::from_rows({{1, 0}, {0, 2}})},
                                  g);
        },
        "nonidentity equation metric refused");
    rejects(
        [&] {
            core::OwnedRelationalSource s(matrix_source(matrix, false, false, true));
            (void)System::declare(s, c, g);
        },
        "grade-two cells refused");
    rejects<std::out_of_range>([&] { (void)sys.row(-1, g); }, "negative row refused");
    rejects<std::out_of_range>([&] { (void)sys.row(2, g); }, "outside row refused");
    rejects<std::out_of_range>([&] { (void)sys.coefficient(row, 1, g); }, "outside occurrence refused");
    rejects<std::out_of_range>([&] { (void)sys.coefficient(row, -1, g); }, "negative occurrence refused");
    auto same = System::declare(owner, c, g);
    rejects([&] { (void)sys.coefficient(same.row(0, g), 0, g); }, "foreign joint-system row refused");
    rejects([&] { (void)Rotation::prepare(same, coefficient, d, g); }, "foreign coefficient owner refused");
    core::OwnedRelationalSource other(owner.source());
    auto foreign = System::declare(other, c, g);
    rejects([&] { (void)Rotation::prepare(foreign, coefficient, d, g); },
            "equal source data is not authority");
    auto other_plan = Rotation::prepare(sys, coefficient, d, g);
    auto receipt = plan.half_turn(g);
    rejects([&] { other_plan.validate_sample(receipt, g); }, "foreign result parent refused");
    rejects([&] { (void)plan.sample(domain(g).at(0, g), g); }, "foreign parameter domain refused");
    rejects<std::domain_error>([&] { (void)plan.sample(d.at(17, g), g); }, "out of domain sample refused");
    auto holes = domain(g, {{0, 1}});
    auto punctured = Rotation::prepare(sys, coefficient, holes, g);
    rejects<std::domain_error>([&] { (void)punctured.sample(holes.at(0, g), g); }, "original hole retained");
    auto punctured_end = punctured.half_turn(g);
    require(punctured_end.obstruction() == 2, "endpoint has separate declared domain");
    auto open = domain(g, {}, false);
    auto open_plan = Rotation::prepare(sys, coefficient, open, g);
    rejects<std::domain_error>([&] { (void)open_plan.sample(open.at(16, g), g); },
                               "open interval endpoint retained");
    auto empty = domain(g, {{}});
    auto empty_plan = Rotation::prepare(sys, coefficient, empty, g);
    rejects<std::domain_error>([&] { (void)empty_plan.sample(empty.at(0, g), g); },
                               "identically excluded finite chart refused");
    auto empty_end = empty_plan.half_turn(g);
    require(empty_end.obstruction() == 2, "empty finite chart does not erase separate endpoint");
    core::OwnedRelationalSource flags(matrix_source(M::from_rows({{2}, {1}}), true));
    auto flag_system = System::declare(flags, V{2, 1}, g);
    rejects([&] { (void)flag_system.coefficient(flag_system.row(0, g), 3, g); },
            "absent coefficient is not present zero");
    auto zero = flag_system.coefficient(flag_system.row(0, g), 2, g);
    auto zero_plan = Rotation::prepare(flag_system, zero, d, g);
    auto zero_end = zero_plan.half_turn(g);
    require(zero.value() == 0 && zero_end.obstruction() == 0, "present zero coefficient retained");
    for (const RotatingSystemLimits limits : {RotatingSystemLimits{0, 64, 512},
                                              {32, 0, 512},
                                              {32, 64, 1},
                                              {65, 64, 512},
                                              {32, 129, 512},
                                              {32, 64, 1025}})
        rejects<std::length_error>([&] { (void)System::declare(owner, c, g, limits); },
                                   "declared source cap refused");
    for (auto limits : {numeric::ExactActionLimits{1, 16000000, 64000000, 1u << 24},
                        {4096, 1, 64000000, 1u << 24},
                        {4096, 16000000, 0, 1u << 24},
                        {4096, 16000000, 64000000, 1}})
        rejects<std::length_error>(
            [&] {
                G small(limits);
                (void)System::declare(owner, c, small);
            },
            "construction receiving policy enforced");
    for (auto limits : {numeric::ExactActionLimits{1, 16000000, 64000000, 1u << 24},
                        {4096, 1, 64000000, 1u << 24},
                        {4096, 16000000, 0, 1u << 24},
                        {4096, 16000000, 64000000, 1}})
        rejects<std::length_error>(
            [&] {
                G small(limits);
                (void)plan.half_turn(small);
            },
            "prepared plan receiving policy enforced");
    for (auto limits : {numeric::ExactActionLimits{1, 16000000, 64000000, 1u << 24},
                        {4096, 1, 64000000, 1u << 24},
                        {4096, 16000000, 0, 1u << 24},
                        {4096, 16000000, 64000000, 1}})
        rejects<std::length_error>(
            [&] {
                G small(limits);
                receipt.validate_receiving(small);
            },
            "result receiving policy enforced");
    G prep;
    (void)System::declare(owner, c, prep);
    for (auto cutoff : {std::size_t{1}, prep.steps() / 2, prep.steps()}) {
        std::size_t count{};
        G cancelled({}, [&] {
            if (++count == cutoff)
                throw std::runtime_error("cancel");
        });
        rejects<std::runtime_error>([&] { (void)System::declare(owner, c, cancelled); },
                                    "source construction cancellation");
        require(count == cutoff, "cancellation reaches exact source work boundary");
    }
    G build;
    (void)Rotation::prepare(sys, coefficient, d, build);
    for (auto cutoff : {std::size_t{1}, build.steps() / 2, build.steps()}) {
        std::size_t count{};
        G cancelled({}, [&] {
            if (++count == cutoff)
                throw std::runtime_error("cancel");
        });
        rejects<std::runtime_error>([&] { (void)Rotation::prepare(sys, coefficient, d, cancelled); },
                                    "theorem construction cancellation");
        require(count == cutoff, "cancellation reaches exact theorem work boundary");
    }
    G sample;
    (void)plan.half_turn(sample);
    for (auto cutoff : {std::size_t{1}, sample.steps() / 2, sample.steps()}) {
        std::size_t count{};
        G cancelled({}, [&] {
            if (++count == cutoff)
                throw std::runtime_error("cancel");
        });
        rejects<std::runtime_error>([&] { (void)plan.half_turn(cancelled); }, "observation cancellation");
        require(count == cutoff, "cancellation reaches exact observation work boundary");
    }
    auto saved = [&] {
        auto temporary = plan;
        G local_guard;
        return temporary.sample(d.at(Q{1, 2}, local_guard), local_guard);
    }();
    saved.validate_receiving(g);
    require(saved.obstruction() > 0, "result retains source and theorem beyond temporary plan lifetime");
}
int main(int argc, char **argv) {
    try {
        const bool dump = argc > 1 && std::string_view(argv[1]) == "--dump";
        if (dump)
            std::cout << '[';
        run_case("original-eight-equations", original_matrix(), {2, -1, 3, 0, 5, 1}, 5, 2, dump);
        if (dump)
            std::cout << ',';
        run_case("auxiliary-zero", M::from_rows({{1}, {1}}), {1}, 0, 0, dump);
        if (dump)
            std::cout << ',';
        run_case("zero-obstruction", M::from_rows({{1, 1}, {1, -1}, {1, 0}}), {0, 2}, 0, 0, dump);
        if (dump)
            std::cout << ',';
        run_case("negative-q", M::from_rows({{1, 1}, {1, -1}, {1, 0}}), {1, 0}, 0, 0, dump);
        if (dump)
            std::cout << ',';
        run_case("positive-scaled-coefficient", M::from_rows({{2}, {3}, {-1}}), {5}, 0, 0, dump);
        if (dump)
            std::cout << ',';
        run_case("rational-joint-system", M::from_rows({{Q{1, 2}, -2}, {3, 1}, {-1, Q{5, 2}}, {0, 2}}),
                 {Q{2, 3}, Q{-7, 5}}, 0, 0, dump);
        if (dump)
            std::cout << ',';
        run_case("repeated-present-zero-inactive-cancelling-flags", M::from_rows({{2}, {1}}), {1}, 0, 0, dump,
                 true);
        if (dump)
            std::cout << ',';
        run_case("negative-cancelling-coefficient", M::from_rows({{2}, {1}}), {1}, 0, 5, dump, true);
        if (dump)
            std::cout << ',';
        run_case("present-zero-coefficient", M::from_rows({{2}, {1}}), {1}, 0, 2, dump, true);
        if (dump)
            std::cout << ',';
        run_case("empty-upper-grade", M::from_rows({{1}, {1}}), {2}, 0, 0, dump, false, true);
        if (dump)
            std::cout << "]\n";
        else {
            refusal_checks();
            std::cout << checks << " rotating coefficient, full potential and obstruction checks passed\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
