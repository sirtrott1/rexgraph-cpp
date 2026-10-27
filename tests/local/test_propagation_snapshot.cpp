#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using System = local::FiniteConstraintSystem<int>;
using Status = local::FiniteSearchStatus;
using Root = local::FinitePropagationStatus;
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
    check(caught, "foreign or stale propagation state accepted");
}
core::CellId cell(std::uint32_t i) {
    return {{0}, i};
}
core::RelationalComplex source() {
    core::RelationSignature signature;
    signature.add_type("whole original relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{90}, 0,
                    {{1, true, false, 1},
                     {2, true, true, 1},
                     {3, true, false, 1},
                     {1, false, true, 2},
                     {2, true, false, 0}});
    return b.freeze();
}
void table_families() {
    const auto c = source();
    for (unsigned mask = 1; mask < 256; ++mask) {
        System s(c);
        s.set_domain(cell(0), {1, 0});
        s.set_domain(cell(1), {0, 1});
        s.set_domain(cell(2), {1, 0});
        std::vector<std::vector<int>> rows;
        for (unsigned bits = 0; bits < 8; ++bits)
            if (mask & (1u << bits))
                rows.push_back({static_cast<int>(bits & 1u), static_cast<int>((bits >> 1u) & 1u),
                                static_cast<int>((bits >> 2u) & 1u)});
        rows.push_back(rows.front());
        s.add_chart("complete joint predicate", s.carrier().cell_patch({{1}, 0}), {cell(0), cell(1), cell(2)},
                    rows);
        const auto p = *s.prepare_global_section().prepared;
        for (int observation = -1; observation < 2; ++observation) {
            System::Assignment fixed;
            if (observation >= 0)
                fixed.emplace(cell(0), observation);
            std::vector<std::vector<int>> admitted;
            for (const auto &row : rows)
                if (observation < 0 || row[0] == observation)
                    admitted.push_back(row);
            const auto result = s.prepared_propagation(p, fixed);
            check(result.work.value_trials == 0 && result.work.search_nodes == 0,
                  "snapshot made a search decision");
            if (admitted.empty()) {
                check(result.status == Root::Unsatisfiable && !result.snapshot,
                      "root refutation lost or published a partial field");
                continue;
            }
            check(result.status == Root::FixedPoint && result.snapshot.has_value(), "root did not complete");
            const auto root = *result.snapshot;
            const auto saved_domains = root.domains();
            check(root.fixed() == fixed && root.prepared_index().same_index(p), "root request/index lost");
            check(root.domains().size() == 3 && root.retained_entries() > 0 &&
                      root.propagation_work().steps == result.work.steps,
                  "incomplete field or receipt");
            for (std::uint32_t vi = 0; vi < 3; ++vi) {
                std::vector<int> expected;
                for (const auto v : s.domain(cell(vi)))
                    if (std::any_of(admitted.begin(), admitted.end(),
                                    [&](const auto &row) { return row[vi] == v; }))
                        expected.push_back(v);
                check(root.domains().at(cell(vi)) == expected,
                      "root differs from independent full joint image");
            }
            const auto old = s.prepared_global_section(p, fixed);
            const auto now = s.continue_global_section(root);
            check(now.status == old.status && now.witness == old.witness && s.globally_valid(*now.witness),
                  "continuation changed complete source solve");
            for (unsigned bits = 0; bits < 8; ++bits) {
                System::Assignment condition;
                for (std::uint32_t vi = 0; vi < 3; ++vi)
                    condition.emplace(cell(vi), static_cast<int>((bits >> vi) & 1u));
                const bool exists =
                    (mask & (1u << bits)) && (observation < 0 || observation == static_cast<int>(bits & 1u));
                const auto answer = s.continue_global_section(root, condition);
                check(answer.status == (exists ? Status::Satisfiable : Status::Unsatisfiable),
                      "conditional continuation lost original correlation or observation");
                check(answer.work.value_trials == 0 && answer.work.search_nodes == 0,
                      "complete conditional field branched");
                if (answer.witness)
                    check(s.globally_valid(*answer.witness), "original predicate not checked");
            }
            check(root.domains() == saved_domains, "continuation changed immutable root");
        }
    }
}
void add_pairs(System &s, int size) {
    for (std::uint32_t a = 0; a < 3; ++a)
        for (auto b = a + 1; b < 3; ++b) {
            std::vector<std::vector<int>> rows;
            for (int x = 0; x < size; ++x)
                for (int y = 0; y < size; ++y)
                    if (x != y)
                        rows.push_back({x, y});
            s.add_chart("original inequality", s.carrier().cell_patch({{1}, 0}), {cell(a), cell(b)}, rows);
        }
}
void correlations_and_matching() {
    const auto c = source();
    System odd(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        odd.set_domain(cell(i), {0, 1});
    add_pairs(odd, 2);
    const auto op = *odd.prepare_global_section().prepared;
    const auto root = odd.prepared_propagation(op);
    check(root.status == Root::FixedPoint && root.snapshot->domains().at(cell(0)).size() == 2,
          "arc-consistent alternatives were called a global solution");
    check(odd.continue_global_section(*root.snapshot).status == Status::Unsatisfiable,
          "snapshot discarded correlated global refutation");
    const auto grouped = *odd.prepare_global_section({{{1}, 0}}).prepared;
    const auto obstruction = odd.prepared_propagation(grouped);
    check(obstruction.status == Root::Unsatisfiable && !obstruction.snapshot &&
              obstruction.work.value_trials == 0 && obstruction.work.matching_tests > 0,
          "existing matching refutation was lost");
    System s(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        s.set_domain(cell(i), {2, 0, 1});
    add_pairs(s, 3);
    s.add_chart("first actual observation", s.carrier().cell_patch(cell(0)), {cell(0)}, {{0}, {1}});
    s.add_chart("second actual observation", s.carrier().cell_patch(cell(1)), {cell(1)}, {{0}, {1}});
    const auto p = *s.prepare_global_section({{{1}, 0}}).prepared;
    const auto matching = s.prepared_propagation(p);
    check(matching.snapshot && matching.work.matching_tests > 0 &&
              matching.snapshot->domains().at(cell(2)) == std::vector<int>{2},
          "existing Hall filtering not exposed");
    const auto left = s.continue_global_section(*matching.snapshot, {{cell(0), 0}});
    const auto right = s.continue_global_section(*matching.snapshot, {{cell(0), 1}});
    check(left.witness->at(cell(1)) == 1 && right.witness->at(cell(1)) == 0,
          "conditioned matching incorrect");
    check(s.continue_global_section(*matching.snapshot, {{cell(2), 0}}).status == Status::Unsatisfiable,
          "a removed original carrier value was restored");
    std::vector<std::future<bool>> readers;
    for (unsigned i = 0; i < 8; ++i)
        readers.push_back(std::async(std::launch::async, [&, snapshot = *matching.snapshot, i] {
            const auto r = s.continue_global_section(snapshot, {{cell(0), static_cast<int>(i % 2)}});
            return r.witness == ((i % 2) ? right.witness : left.witness);
        }));
    for (auto &reader : readers)
        check(reader.get(), "concurrent continuations shared scratch");
    auto copy = s;
    check(copy.continue_global_section(*matching.snapshot).witness.has_value(),
          "same-generation copy rejected");
    s.set_domain(cell(0), {0});
    rejects([&] { (void)s.continue_global_section(*matching.snapshot); });
    rejects([&] { (void)s.prepared_propagation(p); });
    System foreign(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        foreign.set_domain(cell(i), {2, 0, 1});
    add_pairs(foreign, 3);
    rejects([&] { (void)foreign.continue_global_section(*matching.snapshot); });
    rejects([&] { (void)copy.continue_global_section(*matching.snapshot, {{cell(0), 8}}); });
    rejects([&] { (void)copy.prepared_propagation(p, {{cell(9), 0}}); });
}
void limits() {
    const auto c = source();
    System s(c);
    for (std::uint32_t i = 0; i < 3; ++i)
        s.set_domain(cell(i), {0, 1});
    s.add_chart("whole field", s.carrier().cell_patch({{1}, 0}), {cell(0), cell(1), cell(2)},
                {{0, 1, 0}, {1, 0, 1}});
    const auto p = *s.prepare_global_section().prepared;
    const auto full = s.prepared_propagation(p);
    for (const auto cap : {std::size_t{0}, std::size_t{1}, full.work.steps - 1}) {
        const auto r = s.prepared_propagation(p, {}, {3, cap});
        check(r.status == Root::BudgetExceeded && !r.snapshot && r.work.steps <= cap,
              "snapshot work bound published a partial fixed point");
    }
    const auto exact = s.prepared_propagation(p, {}, {3, full.work.steps, full.work.peak_entries});
    check(exact.snapshot && exact.snapshot->domains() == full.snapshot->domains(),
          "exact admission rejected");
    const auto memory = s.prepared_propagation(p, {}, {3, full.work.steps, full.work.peak_entries - 1});
    check(memory.status == Root::BudgetExceeded && !memory.snapshot, "snapshot output admission omitted");
    check(s.prepared_propagation(p, {}, {2}).status == Root::BudgetExceeded,
          "snapshot dimension bound omitted");
    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = s.prepared_propagation(p, {}, {}, stop.get_token());
    check(cancelled.status == Root::Cancelled && !cancelled.snapshot, "snapshot cancellation lost");
    check(s.continue_global_section(*full.snapshot, {}, {}, stop.get_token()).status == Status::Cancelled,
          "continuation cancellation lost");
    const auto terminal = s.continue_global_section(*full.snapshot);
    const auto bounded = s.continue_global_section(*full.snapshot, {}, {3, terminal.work.steps - 1});
    check(bounded.status == Status::BudgetExceeded && !bounded.witness,
          "incomplete continuation published witness");
    check(s.continue_global_section(*full.snapshot, {}, {3, terminal.work.steps}).witness == terminal.witness,
          "exact continuation work limit changed result");
    check(s.continue_global_section(*full.snapshot, {}, {3, 100000, 1}).status == Status::BudgetExceeded,
          "continuation workspace bound omitted");
    System empty(c);
    const auto ep = *empty.prepare_global_section().prepared;
    const auto e = empty.prepared_propagation(ep);
    check(e.snapshot && e.snapshot->domains().empty() && empty.continue_global_section(*e.snapshot).witness,
          "empty complete field confused with absence");
}
void whole_values() {
    const auto c = source();
    local::FiniteConstraintSystem<std::vector<int>> s(c);
    const std::vector<int> a{4, 1}, b{4, 2};
    s.set_domain(cell(0), {a, b});
    s.set_domain(cell(1), {a, b});
    s.add_chart("whole literal values", s.carrier().cell_patch({{1}, 0}), {cell(0), cell(1)},
                {{a, b}, {b, a}});
    const auto p = *s.prepare_global_section().prepared;
    const auto root = s.prepared_propagation(p);
    check(root.snapshot && root.snapshot->domains().at(cell(0)) == std::vector<std::vector<int>>{a, b},
          "whole finite values were numerically reencoded");
    const auto answer = s.continue_global_section(*root.snapshot, {{cell(0), a}});
    check(answer.witness && answer.witness->at(cell(1)) == b, "whole-value correlation was lost");
}
} // namespace
int main() {
    try {
        table_families();
        correlations_and_matching();
        limits();
        whole_values();
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
