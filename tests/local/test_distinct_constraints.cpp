#include "rfc/core/builder.hpp"
#include "rfc/local/constraint_system.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using System = local::FiniteConstraintSystem<int>;
using Status = local::FiniteSearchStatus;
std::size_t checks{};
void check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f) { bool caught = false; try { f(); } catch (const std::invalid_argument&) { caught = true; } check(caught, "unproved distinct request accepted"); }
core::RelationalComplex carrier(std::size_t n, const std::vector<std::vector<std::uint32_t>>& groups) {
    core::RelationSignature signature;
    const auto type = signature.add_type("whole original group", core::BoundaryPolicy::DeclaredExact, 0, 200);
    core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < n; ++i) builder.declare_witness_identity(i + 1);
    std::uint64_t key = 1000;
    for (const auto& support : groups) {
        std::vector<core::BoundaryOccurrenceInput> flags;
        for (const auto i : support) flags.push_back({i + 1, true, false, 1});
        builder.add_grade_one(core::RelationKey{key++}, type, flags);
    }
    return builder.freeze();
}
void pairs(System& s, std::uint32_t relation, int colors, bool omit = false, bool weak = false, bool detached = false) {
    const auto support = s.relation_fiber_support({{1},relation}, false);
    for (std::size_t a = 0; a < support.size(); ++a) for (auto b = a + 1; b < support.size(); ++b) {
        if (omit && a == 0 && b == 1) continue;
        std::vector<std::vector<int>> rows;
        for (int x = 0; x < colors; ++x) for (int y = 0; y < colors; ++y) if (x != y || weak) rows.push_back({x,y});
        auto patch = detached ? join(s.carrier().cell_patch(support[a]), s.carrier().cell_patch(support[b])) : s.carrier().cell_patch({{1},relation});
        s.add_chart("literal inequality", std::move(patch), {support[a],support[b]}, std::move(rows));
    }
}
void exhaustive() {
    const auto source = carrier(3, {{0,1,2}});
    for (unsigned a = 1; a < 16; ++a) for (unsigned b = 1; b < 16; ++b) for (unsigned c = 1; c < 16; ++c) {
        System s(source); for (std::uint32_t i = 0; i < 3; ++i) s.set_domain({{0},i}, {0,1,2,3}); pairs(s,0,4);
        const unsigned masks[]{a,b,c};
        for (std::uint32_t i = 0; i < 3; ++i) {
            std::vector<std::vector<int>> rows;
            for (unsigned v = 0; v < 4; ++v) if (masks[i] & (1u << v)) rows.push_back({static_cast<int>(v)});
            s.add_chart("original observation", s.carrier().cell_patch({{0},i}), {{{0},i}}, std::move(rows));
        }
        for (int fixed = -1; fixed < 4; ++fixed) {
            System::Assignment supplied; if (fixed >= 0) supplied.emplace(core::CellId{{0},0}, fixed);
            bool exists = false;
            for (unsigned x = 0; x < 4; ++x) for (unsigned y = 0; y < 4; ++y) for (unsigned z = 0; z < 4; ++z)
                if (x != y && x != z && y != z && (a & (1u << x)) && (b & (1u << y)) && (c & (1u << z)) && (fixed < 0 || x == static_cast<unsigned>(fixed))) exists = true;
            const auto r = s.distinct_global_section({{{1},0}}, supplied);
            check(r.status == (exists ? Status::Satisfiable : Status::Unsatisfiable), "group matching differs from exhaustive injective sections");
            check(bool(r.witness) == exists, "proof and witness disagree");
            check(r.work.distinct_groups == 1, "original relation was not verified");
            if (r.witness) { check(s.globally_valid(*r.witness), "original chart rejects group witness"); if (fixed >= 0) check(r.witness->at({{0},0}) == fixed, "fixed value changed"); }
        }
    }
}
void controls() {
    const auto source = carrier(3, {{0,1,2}}); System hall(source);
    for (std::uint32_t i = 0; i < 3; ++i) hall.set_domain({{0},i}, {0,1});
    pairs(hall,0,2);
    const auto old = hall.propagated_global_section(), result = hall.distinct_global_section({{{1},0}});
    check(old.work.value_trials > 0 && result.status == Status::Unsatisfiable && result.work.value_trials == 0 && result.work.matching_tests == 1, "Hall obstruction was missed or replaced by local SAT");
    for (const auto cap : {std::size_t{0}, std::size_t{1}, result.work.steps - 1}) {
        const auto r = hall.distinct_global_section({{{1},0}}, {}, {3,cap});
        check(r.status == Status::BudgetExceeded && !r.witness && r.work.steps <= cap, "interrupted Hall proof became UNSAT");
    }
    check(hall.distinct_global_section({{{1},0}}, {}, {3,result.work.steps}).status == Status::Unsatisfiable, "exact proof admission changed status");
    check(hall.distinct_global_section({{{1},0}}, {}, {3,100000,result.work.peak_entries - 1}).status == Status::BudgetExceeded, "group scratch limit ignored");
    std::stop_source stop; stop.request_stop();
    check(hall.distinct_global_section({{{1},0}}, {}, {}, stop.get_token()).status == Status::Cancelled, "group pre-cancel ignored");
    rejects([&] { (void)hall.distinct_global_section({{{1},0},{{1},0}}); });
    for (unsigned mode = 0; mode < 3; ++mode) {
        System missing(source); for (std::uint32_t i = 0; i < 3; ++i) missing.set_domain({{0},i}, {0,1,2});
        pairs(missing,0,3,mode==0,mode==1,mode==2);
        rejects([&] { (void)missing.distinct_global_section({{{1},0}}); });
    }
    System forced(source); for (std::uint32_t i = 0; i < 3; ++i) forced.set_domain({{0},i}, {0,1,2}); pairs(forced,0,3);
    for (std::uint32_t i = 0; i < 2; ++i) forced.add_chart("two choices",forced.carrier().cell_patch({{0},i}),{{{0},i}},{{0},{1}});
    const auto f = forced.distinct_global_section({{{1},0}});
    check(f.witness && f.witness->at({{0},2}) == 2 && f.work.value_trials == 1, "whole-group support lost correlated restrictions");
    auto one = std::async(std::launch::async,[&] { return forced.distinct_global_section({{{1},0}}); });
    auto two = std::async(std::launch::async,[&] { return forced.distinct_global_section({{{1},0}},{{{{0},0},1}}); });
    check(one.get().witness == f.witness && two.get().witness->at({{0},1}) == 0, "parallel group scratch leaked across requests");
    const auto overlap = carrier(4,{{0,1,2},{0,1,3},{0,2,3},{1,2,3}}); System inconsistent(overlap);
    for (std::uint32_t i = 0; i < 4; ++i) inconsistent.set_domain({{0},i},{0,1,2});
    for (std::uint32_t i = 0; i < 4; ++i) pairs(inconsistent,i,3);
    const auto r = inconsistent.distinct_global_section({{{1},0},{{1},1},{{1},2},{{1},3}});
    check(r.status == Status::Unsatisfiable && r.work.value_trials > 0, "compatible local matchings were mistaken for global satisfiability");
    const auto wide_source = carrier(2,{{0,1}}); System wide(wide_source); std::vector<int> colors;
    for (int i = 0; i < 70; ++i) colors.push_back(i);
    for (std::uint32_t i = 0; i < 2; ++i) wide.set_domain({{0},i},colors);
    pairs(wide,0,70);
    const auto w = wide.distinct_global_section({{{1},0}}, {{{{0},0},69}}, {2,20000000});
    check(w.witness && w.witness->at({{0},0}) == 69 && w.witness->at({{0},1}) != 69, "matching truncated the symbolic domain to a word");
}
}
int main() { try { exhaustive(); controls(); std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n"; } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
