#include "rfc/core/builder.hpp"
#include "rfc/field/execution_tower.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using K = field::OperatorKind;
using M = field::ExactMetric;
namespace {
std::size_t checks{};
void check(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught = false; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
std::shared_ptr<const core::RelationalComplex> square(bool filled) {
    core::RelationSignature signature;
    const auto type = signature.add_type("original-square", core::BoundaryPolicy::DeclaredExact, 0, 12);
    core::RelationalComplexBuilder builder(signature); builder.declare_grade(core::Grade{2});
    builder.add_grade_one(core::RelationKey{100}, type,
        {{0, true, true, Q{1}}, {1, true, false, Q{1}}, {0, true, false, Q{2}},
         {0, true, true, Q{2}}, {2, false, false, Q{7}}});
    builder.add_grade_one(core::RelationKey{101}, type, {{1, true, true, Q{1}}, {2, true, false, Q{1}}});
    builder.add_grade_one(core::RelationKey{102}, type, {{2, true, true, Q{1}}, {3, true, false, Q{1}}});
    builder.add_grade_one(core::RelationKey{103}, type, {{3, true, true, Q{1}}, {0, true, false, Q{1}}});
    if (filled) builder.add_higher(core::Grade{2}, core::RelationKey{200}, type,
        {{0, true, false, Q{1}}, {1, true, false, Q{1}}, {2, true, false, Q{1}}, {3, true, false, Q{1}}});
    return std::make_shared<const core::RelationalComplex>(builder.freeze());
}
std::vector<M> metrics(const core::RelationalComplex& source, bool general) {
    std::vector<M> result;
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto n = source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        if (!general) { result.push_back(M::identity(n)); continue; }
        numeric::ExactMatrix form(n, n);
        for (std::size_t i = 0; i < n; ++i) {
            form.set(i, i, Q{static_cast<unsigned long>(i + 5)});
            if (i) { form.set(i - 1, i, Q{1, 3}); form.set(i, i - 1, Q{1, 3}); }
        }
        result.emplace_back(std::move(form));
    }
    return result;
}
void exact_cases() {
    for (const bool filled : {false, true}) for (const bool general : {false, true}) {
        const auto source = square(filled); const auto declared = metrics(*source, general);
        const field::PreparedExactExecutionTower tower(source, declared, field::PreparedExactProfile::GlobalActions);
        check(source->grade(core::Grade{1}).incidence_count() == 11, "retained repeated/inactive source changed");
        for (std::uint16_t k = 0; k < 3; ++k) {
            const core::Grade grade{k}; const field::ExactHodgeOperator hodge(*source, grade, declared);
            const auto projector = tower.prepare_action({K::HarmonicProjector, grade, 0});
            const auto frame = hodge.harmonic_frame();
            for (const Q& lambda : V{0, Q{1}, Q{2, 3}, Q{-1, 101}}) {
                const auto green = tower.prepare_action({K::Green, grade, lambda});
                for (unsigned sample = 0; sample < 5; ++sample) {
                    V x(hodge.size());
                    for (std::size_t i = 0; i < x.size(); ++i) {
                        x[i] = Q{static_cast<long>(i + sample) - 2, static_cast<unsigned long>(i + 1)}; x[i].canonicalize();
                    }
                    const auto value = green.apply(x);
                    check(numeric::add(value, numeric::scale(lambda, hodge.apply(value))) == x, "prepared Green exact residual");
                    check(value == field::augmented_green_apply_reference(*source, grade, declared, lambda, x).value,
                        "independent auxiliary layout comparison");
                    if (lambda > 0) check(value == field::exact_green_apply(hodge, lambda, x).value, "action Green comparison");
                    const auto harmonic = projector.apply(x);
                    check(harmonic == field::projector_apply(frame, declared[k], x), "prepared projector Gram comparison");
                    check(projector.apply(harmonic) == harmonic && numeric::is_zero(hodge.apply(harmonic)), "harmonic projection identities");
                }
            }
        }
        if (!general) {
            const auto green = tower.prepare_action({K::Green, core::Grade{1}, 1});
            check(green.apply({1, 0, 0, 0}) == (filled ? V{Q{4, 15}, 0, Q{-1, 15}, 0}
                : V{Q{7, 15}, Q{1, 5}, Q{2, 15}, Q{1, 5}}), "paper's unfilled/filled square values");
        }
    }
}
void ownership_limits() {
    const auto action = [] {
        const auto source = square(false);
        return field::PreparedExactExecutionTower(source, metrics(*source, true), field::PreparedExactProfile::GlobalActions)
            .prepare_action({K::Green, core::Grade{1}, Q{2, 4}});
    }();
    check(action.source()->grade(core::Grade{1}).cell_count() == 4 && action.operation().lambda == Q{1, 2}, "source/lambda ownership");
    const auto expected = action.apply({-1, 2, 3, 4});
    check(action.apply({Q{-2, 2}, Q{4, 2}, 3, 4}) == expected, "canonical input required");
    V invalid{1, 2, 3, 4}; mpz_set_ui(mpq_denref(invalid[0].get_mpq_t()), 0);
    rejects([&] { action.apply(invalid); }, "zero denominator accepted");
    rejects([&] { action.apply({1, 2}); }, "input dimension accepted");
    numeric::ExactActionLimits limits; limits.dimension = action.solve_dimension() - 1;
    rejects([&] { action.apply({1, 2, 3, 4}, limits); }, "current dimension limit bypassed");
    limits = {}; limits.entries = action.retained_entries() - 1;
    rejects([&] { action.apply({1, 2, 3, 4}, limits); }, "current retained-entry limit bypassed");
    limits = {}; limits.integer_bits = 0;
    rejects([&] { action.apply({0, 0, 0, 0}, limits); }, "current factor bit limit bypassed");
    limits = {}; limits.steps = 1;
    rejects([&] { action.apply({1, 2, 3, 4}, limits); }, "current work limit bypassed");
    std::size_t checkpoints{};
    rejects([&] { action.apply({1, 2, 3, 4}, {}, [&] { if (++checkpoints == 10) throw std::runtime_error("stop"); }); },
        "mid-action cancellation ignored");
    check(checkpoints == 10 && action.apply({-1, 2, 3, 4}) == expected, "cancelled application poisoned factors");
    const auto preparation = action.preparation_steps();
    std::vector<std::future<V>> work;
    for (unsigned i = 0; i < 8; ++i) work.push_back(std::async(std::launch::async, [copy = action] { return copy.apply({-1, 2, 3, 4}); }));
    for (auto& job : work) check(job.get() == expected, "shared immutable factor concurrent application");
    check(action.preparation_steps() == preparation && preparation > 0, "preparation receipt changed");
    const auto source = square(false); const auto other = square(false);
    check(action.source().get() != other.get(), "equal source values identified unrelated owners");
    const field::PreparedExactExecutionTower tower(source, metrics(*source, false), field::PreparedExactProfile::GlobalActions);
    limits = {}; limits.entries = 1;
    rejects([&] { tower.prepare_action({K::Green, core::Grade{1}, 1}, limits); }, "preparation entry limit bypassed");
    checkpoints = 0;
    rejects([&] { tower.prepare_action({K::Green, core::Grade{1}, 1}, {}, [&] { if (++checkpoints == 10) throw std::runtime_error("stop"); }); },
        "preparation cancellation ignored");
    check(tower.prepare_action({K::Green, core::Grade{1}, 1}).apply({1, 0, 0, 0})[0] == Q{7, 15}, "failed preparation changed source");
    auto replaceable = tower;
    const field::PreparedExactExecutionTower replacement(other, metrics(*other, true), field::PreparedExactProfile::GlobalActions);
    bool replaced = false;
    const auto pinned = replaceable.prepare_action({K::Green, core::Grade{1}, 1}, {}, [&] {
        if (!replaced) { replaced = true; replaceable = replacement; }
    });
    check(pinned.source().get() == source.get() && pinned.apply({1, 0, 0, 0})[0] == Q{7, 15},
        "preparation checkpoint replacement lost its original owner");
    rejects([&] { tower.prepare_action({K::Hodge, core::Grade{1}, 0}); }, "non-global solve silently prepared");
    auto external = metrics(*source, false);
    const auto identity = [](const V& x) { return x; };
    external[1] = M::certified_external(4, identity, identity, [] { return numeric::ExactMatrix::identity(4); });
    const field::PreparedExactExecutionTower external_tower(source, external, field::PreparedExactProfile::GlobalActions);
    rejects([&] { external_tower.prepare_action({K::Green, core::Grade{1}, 1}); }, "external callback silently frozen");
    check(external_tower.apply({K::Green, core::Grade{1}, 1}, {1, 0, 0, 0})[0] == Q{7, 15}, "external one-shot contract changed");
}
void endpoints_empty_singular() {
    core::RelationalComplexBuilder empty(core::RelationSignature{}); empty.declare_witness_identity(1); empty.declare_grade(core::Grade{2});
    const auto source = std::make_shared<const core::RelationalComplex>(empty.freeze());
    const field::PreparedExactExecutionTower tower(source, metrics(*source, false), field::PreparedExactProfile::GlobalActions);
    for (std::uint16_t k = 0; k < 3; ++k) {
        const V input(k ? 0 : 1, Q{3});
        check(tower.prepare_action({K::Green, core::Grade{k}, 1}).apply(input) == input, "empty tower Green");
        check(tower.prepare_action({K::HarmonicProjector, core::Grade{k}, 0}).apply(input) == input, "empty tower projector");
        if (k) rejects([&] { tower.prepare_action({K::HarmonicProjector, core::Grade{k}, 0}).apply({1}); }, "empty frame skipped dimension check");
    }
    core::RelationSignature signature; const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature); builder.add_grade_one(edge, {{0, true, true, Q{1}}, {1, true, false, Q{1}}});
    const auto pair = std::make_shared<const core::RelationalComplex>(builder.freeze());
    const field::PreparedExactExecutionTower pair_tower(pair, metrics(*pair, false), field::PreparedExactProfile::GlobalActions);
    rejects([&] { pair_tower.prepare_action({K::Green, core::Grade{0}, Q{-1, 2}}); }, "singular negative Green accepted");
    check(pair_tower.prepare_action({K::Green, core::Grade{0}, 0}).apply({3, 4}) == V({3, 4}), "zero lambda is not identity");
}
}
int main() { try { exact_cases(); ownership_limits(); endpoints_empty_singular();
    std::cout << "prepared_global_action_checks=" << checks << '\n'; return 0;
} catch (const std::exception& e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; return 1; } }
