#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include <future>
#include <iostream>
#include <string>

namespace {
using namespace rfc;
using System = local::FiniteConstraintSystem<int>;
using Status = local::FiniteSearchStatus;
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
core::RelationalComplex vertices(std::size_t n) {
    core::RelationalComplexBuilder builder(core::RelationSignature{});
    for (std::size_t i = 0; i < n; ++i) builder.declare_witness_identity(i + 1);
    return builder.freeze();
}
void exhaustive() {
    const auto source = vertices(3);
    for (unsigned a = 1; a < 16; ++a) for (unsigned b = 1; b < 16; ++b) for (unsigned c = 1; c < 16; ++c) {
        System s(source); for (std::uint32_t i = 0; i < 3; ++i) s.set_boolean_domain({{0}, i});
        const unsigned tables[]{a, b, c};
        for (std::uint32_t i = 0; i < 3; ++i) {
            const core::CellId x{{0}, i}, y{{0}, (i + 1) % 3};
            s.add_boolean_truth_chart("original chart", join(s.carrier().cell_patch(x), s.carrier().cell_patch(y)), {x, y}, tables[i]);
        }
        for (unsigned fixed = 0; fixed < 9; ++fixed) {
            System::Assignment assignment;
            if (fixed < 8) for (std::uint32_t i = 0; i < 3; ++i) assignment.emplace(core::CellId{{0}, i}, static_cast<int>((fixed >> i) & 1u));
            bool exists = false;
            for (unsigned bits = 0; bits < 8; ++bits) {
                if (fixed < 8 && bits != fixed) continue;
                bool good = true;
                for (unsigned i = 0; i < 3; ++i) {
                    const auto row = ((bits >> i) & 1u) | (((bits >> ((i + 1) % 3)) & 1u) << 1u);
                    good = good && (((tables[i] >> row) & 1u) != 0);
                }
                exists = exists || good;
            }
            const auto result = s.propagated_global_section(assignment);
            check(result.status == (exists ? Status::Satisfiable : Status::Unsatisfiable), "propagation differs from exhaustive original truth tables");
            check(bool(result.witness) == exists, "proof status and witness disagree");
            if (result.witness) {
                check(s.globally_valid(*result.witness), "witness violates original charts");
                for (const auto& [cell, value] : assignment) check(result.witness->at(cell) == value, "fixed original assignment changed");
            }
        }
    }
}
struct Observed {
    int value{};
    static inline std::stop_source* stop{};
    friend bool operator==(Observed a, Observed b) { if (stop) stop->request_stop(); return a.value == b.value; }
};
void controls() {
    const auto source = vertices(3); System odd(source);
    for (std::uint32_t i = 0; i < 3; ++i) odd.set_boolean_domain({{0}, i});
    for (std::uint32_t i = 0; i < 3; ++i) {
        const core::CellId x{{0}, i}, y{{0}, (i + 1) % 3};
        odd.add_chart("different", join(odd.carrier().cell_patch(x), odd.carrier().cell_patch(y)), {x, y}, {{0, 1}, {1, 0}});
    }
    const auto contradiction = odd.propagated_global_section();
    check(contradiction.status == Status::Unsatisfiable && contradiction.work.value_trials > 0, "arc-consistent odd cycle incorrectly identified or never searched");
    for (const auto steps : {std::size_t{0}, std::size_t{1}, contradiction.work.steps - 1}) {
        const auto r = odd.propagated_global_section({}, {3, steps});
        check(r.status == Status::BudgetExceeded && !r.witness && r.work.steps <= steps, "unfinished proof called unsatisfiable");
    }
    check(odd.propagated_global_section({}, {3, contradiction.work.steps}).status == Status::Unsatisfiable, "exact work limit changed proof");
    check(odd.propagated_global_section({}, {2, 10000}).status == Status::BudgetExceeded, "variable admission ignored");
    check(odd.propagated_global_section({}, {3, 10000, contradiction.work.peak_entries - 1}).status == Status::BudgetExceeded, "scratch admission ignored");
    check(odd.propagated_global_section({}, {3, 10000, contradiction.work.peak_entries}).status == Status::Unsatisfiable, "admitted scratch failed");
    std::stop_source stop; stop.request_stop();
    check(odd.propagated_global_section({}, {}, stop.get_token()).status == Status::Cancelled, "pre-cancel ignored");
    bool threw = false; try { (void)odd.propagated_global_section({{{{0}, 0}, 9}}); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "out-of-domain assumption accepted");
    threw = false; try { (void)odd.propagated_global_section({{{{0}, 8}, 0}}); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "foreign variable accepted");
    local::FiniteConstraintSystem<Observed> observed(source);
    observed.set_domain({{0}, 0}, {{0}, {1}});
    observed.add_chart("observed", observed.carrier().cell_patch({{0}, 0}), {{{0}, 0}}, {{{1}}});
    std::stop_source during; Observed::stop = &during;
    const auto cancelled = observed.propagated_global_section({}, {}, during.get_token()); Observed::stop = nullptr;
    check(cancelled.status == Status::Cancelled && !cancelled.witness, "mid-comparison cancellation ignored");
    const auto before = odd.domains(); const auto charts = odd.charts().size();
    auto one = std::async(std::launch::async, [&] { return odd.propagated_global_section(); });
    auto two = std::async(std::launch::async, [&] { return odd.propagated_global_section(); });
    check(one.get().status == Status::Unsatisfiable && two.get().status == Status::Unsatisfiable, "parallel solves share mutable scratch");
    check(odd.domains() == before && odd.charts().size() == charts, "search mutated retained model");
    System mutable_domain(source); mutable_domain.set_boolean_domain({{0}, 0});
    mutable_domain.add_chart("old value", mutable_domain.carrier().cell_patch({{0}, 0}), {{{0}, 0}}, {{0}});
    mutable_domain.set_domain({{0}, 0}, {1});
    check(mutable_domain.propagated_global_section().status == Status::Unsatisfiable, "removed native domain value resurrected");
    const auto empty = vertices(0); System none(empty);
    check(none.propagated_global_section().witness->empty(), "empty source loses its empty global section");
    local::FiniteConstraintSystem<std::string> symbolic(source);
    symbolic.set_domain({{0}, 0}, {"literal", "other"});
    symbolic.add_chart("literal bytes", symbolic.carrier().cell_patch({{0}, 0}), {{{0}, 0}}, {{"other"}, {"other"}});
    check(symbolic.propagated_global_section().witness->at({{0}, 0}) == "other", "symbolic carrier or repeated tuple lost");
    System wide(source); std::vector<int> values; for (int i = 0; i < 130; ++i) values.push_back(i);
    wide.set_domain({{0}, 0}, values); wide.add_chart("last value", wide.carrier().cell_patch({{0}, 0}), {{{0}, 0}}, {{129}});
    check(wide.propagated_global_section().witness->at({{0}, 0}) == 129, "domain truncated to machine-word mask");
    System ternary(source); for (std::uint32_t i = 0; i < 3; ++i) ternary.set_boolean_domain({{0}, i});
    const auto patch = join(join(ternary.carrier().cell_patch({{0}, 0}), ternary.carrier().cell_patch({{0}, 1})), ternary.carrier().cell_patch({{0}, 2}));
    ternary.add_chart("whole parity", patch, {{{0}, 0}, {{0}, 1}, {{0}, 2}}, {{0,0,0}, {0,1,1}, {1,0,1}, {1,1,0}});
    const auto t = ternary.propagated_global_section({{{{0}, 0}, 1}, {{{0}, 1}, 0}});
    check(t.witness && t.witness->at({{0}, 2}) == 1 && ternary.charts().front().support.size() == 3, "whole non-pairwise correlation lost");
}
}
int main() { try { exhaustive(); controls(); std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n"; } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
