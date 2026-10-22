#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include <iostream>
#include <memory>

namespace {
using namespace rfc;
using System = local::FiniteConstraintSystem<int>;
using Status = local::FiniteSearchStatus;
using Ready = local::FinitePreparationStatus;
std::size_t checks{};
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
template <class F> void rejects(F action) {
    bool caught = false;
    try {
        action();
    } catch (const std::invalid_argument &) {
        caught = true;
    }
    check(caught, "invalid prepared index or unproved group accepted");
}
core::CellId cell(std::uint32_t i) {
    return {{0}, i};
}
core::RelationalComplex source() {
    core::RelationSignature signature;
    const auto type = signature.add_type("retained whole group", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{90}, type,
                    {{1, true, false, 1},
                     {2, true, true, 1},
                     {3, true, false, 1},
                     {1, false, true, 2},
                     {2, true, false, 0}});
    return b.freeze();
}
void equivalent(const System &s, const System::PreparedIndex &index, const System::Assignment &fixed,
                bool grouped) {
    const auto old =
        grouped ? s.distinct_global_section({{{1}, 0}}, fixed) : s.propagated_global_section(fixed);
    const auto now = s.prepared_global_section(index, fixed);
    check(old.status == now.status && old.witness == now.witness, "prepared complete result differs");
    check(old.work.value_trials == now.work.value_trials && old.work.search_nodes == now.work.search_nodes &&
              old.work.domain_reductions == now.work.domain_reductions &&
              old.work.chart_tests == now.work.chart_tests &&
              old.work.matching_tests == now.work.matching_tests &&
              old.work.peak_assigned == now.work.peak_assigned,
          "reuse changed propagation, branch or final original predicate work");
    if (now.witness)
        check(s.globally_valid(*now.witness), "prepared witness violates original charts");
}
void table_families() {
    const auto c = source();
    for (unsigned mask = 1; mask < 256; ++mask) {
        System s(c);
        s.set_domain(cell(0), {1, 0});
        s.set_domain(cell(1), {0, 1});
        s.set_domain(cell(2), {1, 0});
        std::vector<std::vector<int>> allowed;
        for (unsigned bits = 0; bits < 8; ++bits)
            if (mask & (1u << bits))
                allowed.push_back({static_cast<int>(bits & 1u), static_cast<int>((bits >> 1u) & 1u),
                                   static_cast<int>((bits >> 2u) & 1u)});
        allowed.push_back(allowed.front());
        s.add_chart("complete correlated field", s.carrier().cell_patch({{1}, 0}),
                    {cell(0), cell(1), cell(2)}, allowed);
        const auto prepared = s.prepare_global_section();
        check(prepared.status == Ready::Ready && prepared.prepared.has_value(),
              "finite family did not prepare");
        equivalent(s, *prepared.prepared, {}, false);
        for (unsigned bits = 0; bits < 8; ++bits) {
            System::Assignment fixed;
            for (std::uint32_t i = 0; i < 3; ++i)
                fixed.emplace(cell(i), static_cast<int>((bits >> i) & 1u));
            const auto result = s.prepared_global_section(*prepared.prepared, fixed);
            check(result.status == ((mask & (1u << bits)) ? Status::Satisfiable : Status::Unsatisfiable),
                  "prepared family differs from independently enumerated original tuples");
            equivalent(s, *prepared.prepared, fixed, false);
        }
    }
}
void add_pairs(System &s, unsigned mode = 0) {
    for (std::uint32_t a = 0; a < 3; ++a)
        for (auto b = a + 1; b < 3; ++b) {
            if (mode == 1 && a == 0 && b == 1)
                continue;
            std::vector<std::vector<int>> rows;
            for (int x = 0; x < 3; ++x)
                for (int y = 0; y < 3; ++y)
                    if (x != y || mode == 2)
                        rows.push_back({x, y});
            rows.push_back(rows.front());
            auto patch = mode == 3 ? join(s.carrier().cell_patch(cell(a)), s.carrier().cell_patch(cell(b)))
                                   : s.carrier().cell_patch({{1}, 0});
            s.add_chart("original pair predicate", std::move(patch), {cell(a), cell(b)}, rows);
        }
}
void group_families() {
    const auto c = source();
    for (unsigned a = 1; a < 8; ++a)
        for (unsigned b = 1; b < 8; ++b)
            for (unsigned d = 1; d < 8; ++d) {
                System s(c);
                s.set_domain(cell(0), {2, 0, 1});
                s.set_domain(cell(1), {1, 2, 0});
                s.set_domain(cell(2), {0, 2, 1});
                add_pairs(s);
                const unsigned masks[]{a, b, d};
                for (std::uint32_t i = 0; i < 3; ++i) {
                    std::vector<std::vector<int>> rows;
                    for (unsigned v = 0; v < 3; ++v)
                        if (masks[i] & (1u << v))
                            rows.push_back({static_cast<int>(v)});
                    s.add_chart("actual observation", s.carrier().cell_patch(cell(i)), {cell(i)}, rows);
                }
                const auto p = s.prepare_global_section({{{1}, 0}});
                check(p.prepared && p.work.distinct_groups == 1, "group proof was not retained");
                equivalent(s, *p.prepared, {}, true);
                for (int fixed = 0; fixed < 3; ++fixed) {
                    const System::Assignment assignment{{cell(0), fixed}};
                    const auto r = s.prepared_global_section(*p.prepared, assignment);
                    bool exists = false;
                    for (unsigned x = 0; x < 3; ++x)
                        for (unsigned y = 0; y < 3; ++y)
                            for (unsigned z = 0; z < 3; ++z)
                                if (x != y && x != z && y != z && (a & (1u << x)) && (b & (1u << y)) &&
                                    (d & (1u << z)) && x == static_cast<unsigned>(fixed))
                                    exists = true;
                    check(r.status == (exists ? Status::Satisfiable : Status::Unsatisfiable),
                          "prepared matching differs from full injective family");
                    equivalent(s, *p.prepared, assignment, true);
                }
            }
}
void mutation_and_limits() {
    const auto c = source();
    System s(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        s.set_domain(cell(i), {0, 1, 2});
    add_pairs(s);
    const auto p = s.prepare_global_section({{{1}, 0}});
    const auto result = s.prepared_global_section(*p.prepared);
    check(p.prepared->same_index(*p.prepared) &&
              result.work.steps < s.distinct_global_section({{{1}, 0}}).work.steps,
          "prepared execution did not reuse completed compilation/proof");
    check(p.prepared->relations() == std::vector<core::RelationId>{{{1}, 0}} &&
              p.prepared->preparation_work().steps == p.work.steps,
          "prepared provenance or work lost");
    auto copy = s;
    s.set_domain(cell(0), {0, 1});
    rejects([&] { (void)s.prepared_global_section(*p.prepared); });
    check(copy.prepared_global_section(*p.prepared).witness == result.witness,
          "mutation changed independent system copy");
    auto revised = copy;
    revised.add_chart("new observation", revised.carrier().cell_patch(cell(0)), {cell(0)}, {{1}});
    rejects([&] { (void)revised.prepared_global_section(*p.prepared); });
    const auto rp = revised.prepare_global_section({{{1}, 0}});
    check(revised.prepared_global_section(*rp.prepared).witness->at(cell(0)) == 1,
          "reprepared source ignored its new chart");
    System foreign(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        foreign.set_domain(cell(i), {0, 1, 2});
    add_pairs(foreign);
    rejects([&] { (void)foreign.prepared_global_section(*p.prepared); });
    for (unsigned mode = 1; mode < 4; ++mode) {
        System bad(c);
        for (std::uint32_t i = 0; i < 3; ++i)
            bad.set_domain(cell(i), {0, 1, 2});
        add_pairs(bad, mode);
        rejects([&] { (void)bad.prepare_global_section({{{1}, 0}}); });
    }
    rejects([&] { (void)copy.prepare_global_section({{{1}, 0}, {{1}, 0}}); });
    rejects([&] { (void)copy.prepared_global_section(*p.prepared, {{cell(0), 5}}); });
    for (const auto cap : {std::size_t{0}, std::size_t{1}, p.work.steps - 1}) {
        const auto r = copy.prepare_global_section({{{1}, 0}}, {3, cap});
        check(r.status == Ready::BudgetExceeded && !r.prepared && r.work.steps <= cap,
              "partial preparation escaped admission");
    }
    check(copy.prepare_global_section({{{1}, 0}}, {3, p.work.steps, p.work.peak_entries}).status ==
              Ready::Ready,
          "exact preparation limits were rejected");
    check(copy.prepare_global_section({{{1}, 0}}, {3, p.work.steps, p.work.peak_entries - 1}).status ==
              Ready::BudgetExceeded,
          "preparation entry cap ignored");
    check(copy.prepare_global_section({{{1}, 0}}, {2, 1000000}).status == Ready::BudgetExceeded,
          "preparation variable cap ignored");
    for (const auto cap : {std::size_t{0}, std::size_t{1}, result.work.steps - 1}) {
        const auto r = copy.prepared_global_section(*p.prepared, {}, {3, cap});
        check(r.status == Status::BudgetExceeded && !r.witness && r.work.steps <= cap,
              "partial prepared solve became SAT/UNSAT");
    }
    check(copy.prepared_global_section(*p.prepared, {}, {3, result.work.steps, result.work.peak_entries})
                  .witness == result.witness,
          "exact prepared workspace boundary changed witness");
    check(copy.prepared_global_section(*p.prepared, {}, {3, result.work.steps, result.work.peak_entries - 1})
                  .status == Status::BudgetExceeded,
          "prepared workspace cap ignored");
    std::stop_source stop;
    stop.request_stop();
    check(copy.prepare_global_section({{{1}, 0}}, {}, stop.get_token()).status == Ready::Cancelled,
          "preparation pre-cancel ignored");
    check(copy.prepared_global_section(*p.prepared, {}, {}, stop.get_token()).status == Status::Cancelled,
          "prepared solve pre-cancel ignored");
    auto hall = copy;
    hall.set_domain(cell(0), {0, 1});
    hall.set_domain(cell(1), {0, 1});
    hall.set_domain(cell(2), {0, 1});
    const auto h = hall.prepare_global_section({{{1}, 0}});
    check(h.status == Ready::Ready && h.prepared &&
              hall.prepared_global_section(*h.prepared).status == Status::Unsatisfiable,
          "Ready was confused with satisfiable");
}
struct Probe {
    std::stop_source stop;
    std::size_t comparisons{}, trigger{};
    bool fail{};
};
struct Symbol {
    unsigned value;
    std::shared_ptr<Probe> probe;
    bool operator==(const Symbol &other) const {
        if (probe->trigger && ++probe->comparisons == probe->trigger) {
            if (probe->fail)
                throw std::runtime_error("original Value equality failure");
            probe->stop.request_stop();
        }
        return value == other.value;
    }
};
void interruption_and_symbols() {
    const auto c = source();
    auto probe = std::make_shared<Probe>();
    local::FiniteConstraintSystem<Symbol> s(c);
    const Symbol a{3, probe}, b{7, probe};
    for (std::uint32_t i = 0; i < 3; ++i)
        s.set_domain(cell(i), {a, b});
    s.add_chart("symbolic correlated state", s.carrier().cell_patch({{1}, 0}), {cell(0), cell(1), cell(2)},
                {{a, b, a}, {b, a, b}});
    probe->trigger = 2;
    auto interrupted = s.prepare_global_section({}, {}, probe->stop.get_token());
    check(interrupted.status == Ready::Cancelled && !interrupted.prepared,
          "mid-preparation cancellation escaped");
    probe->trigger = 0;
    probe->stop = std::stop_source{};
    const auto p = s.prepare_global_section();
    probe->comparisons = 0;
    probe->trigger = 1;
    const auto r = s.prepared_global_section(*p.prepared, {{cell(0), a}}, {}, probe->stop.get_token());
    check(r.status == Status::Cancelled && !r.witness, "mid-solve cancellation escaped");
    probe->comparisons = 0;
    probe->fail = true;
    bool propagated = false;
    try {
        (void)s.prepared_global_section(*p.prepared, {{cell(0), a}});
    } catch (const std::runtime_error &) {
        propagated = true;
    }
    check(propagated, "original Value exception was reclassified as a mathematical result");
    probe->trigger = 0;
    const auto good = s.prepared_global_section(*p.prepared, {{cell(0), b}});
    check(good.witness && good.witness->at(cell(1)).value == a.value &&
              good.witness->at(cell(2)).value == b.value,
          "symbolic whole-field correlation changed after interruption");
}
} // namespace
int main() {
    try {
        table_families();
        group_families();
        mutation_and_limits();
        interruption_and_symbols();
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
