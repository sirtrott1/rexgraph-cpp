#include "rfc/inference/relational_finite_source_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace inference;
using Status = local::FiniteSearchStatus;
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
OwnedRelationalFiniteSource source() {
    core::RelationSignature signature;
    const auto type = signature.add_type("original whole relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{1000}, type,
        {{1, true, false, 1}, {2, true, true, 1}, {3, true, false, 1}, {1, false, false, 1}, {2, true, false, 0}});
    for (std::size_t i = 1; i <= 81; ++i) builder.declare_witness_identity(i);
    storage::NativeTemporalSeries history(storage::TemporalEndpoint(builder.freeze(), 1), 41001);
    const auto publication = history.tail();
    RelationalInputUseDeclaration asset; asset.key = 2; asset.channel = 3; asset.encoding = "UTF-8";
    asset.bytes = {std::byte{'a'}};
    RelationalSpanUse use; use.key = 4; use.spans = {{0,1}};
    for (std::uint32_t i = 0; i < 81; ++i) use.cells.push_back(storage::stable_cell_key(publication.endpoint().source, {{0},i}));
    use.cells.push_back(storage::stable_cell_key(publication.endpoint().source, {{1},0}));
    for (std::size_t i = 0; i < 5; ++i) use.occurrences.push_back({{1},publication.endpoint().source.occurrence_identity({1},i)});
    asset.uses.push_back(std::move(use));
    RelationalFiniteSourceDeclaration d; d.key = 5; d.revision = 1;
    d.symbols = {{101, {std::byte{'a'}}}, {102, {std::byte{'a'}}}};
    for (std::uint32_t i = 0; i < 81; ++i) { d.variables.push_back({{{0},i},{101,102}}); d.bindings.push_back({100+i,4,{{0},i}}); }
    d.charts = {{6,"whole correlated interpretation",{{{0},0},{{0},1},{{0},2},{{1},0}},{{{0},0},{{0},1},{{0},2}},{{101,102,101},{102,101,102}}}};
    return {OwnedRelationalInputUseAsset(history, asset), d};
}
}
int main() {
    try {
        const auto owner = source(); const auto archive = encode_relational_finite_source(owner);
        bool rejected = false; try { (void)owner.sections(); } catch (const std::exception&) { rejected = true; }
        check(rejected, "complete Cartesian family was unexpectedly admitted");
        const OwnedRelationalFiniteSource::Assignment fixed{{{{0},0},102}};
        const auto old = owner.bounded_global_section(fixed);
        const auto result = owner.propagated_global_section(fixed);
        check(old.status == Status::Satisfiable && result.status == Status::Satisfiable, "owning forwarders fail without Cartesian allocation");
        check(result.witness && result.witness->size() == 81 && result.witness->at({{0},1}) == 101 && result.witness->at({{0},2}) == 102, "whole symbolic correlation or equal-definition identity lost");
        check(encode_relational_finite_source(owner) == archive, "native search changed source/archive");
        const auto restored = decode_relational_finite_source(archive, {});
        check(encode_relational_finite_source(restored) == archive && restored.propagated_global_section(fixed).witness == result.witness, "cold source changes exact solve");
        auto left = std::async(std::launch::async,[owner,fixed] { return owner.propagated_global_section(fixed); });
        auto right = std::async(std::launch::async,[owner] { return owner.propagated_global_section({{{{0},0},101}}); });
        check(left.get().witness == result.witness && right.get().witness->at({{0},1}) == 102, "shared owning source changes independent requests");
        const auto temporary = source().propagated_global_section(fixed);
        check(temporary.witness == result.witness, "result borrows destroyed owner storage");
        std::stop_source stop; stop.request_stop();
        check(owner.propagated_global_section({}, {}, stop.get_token()).status == Status::Cancelled, "owning propagation loses cancellation");
        check(owner.bounded_global_section({}, {}, stop.get_token()).status == Status::Cancelled, "owning bounded forwarding loses cancellation");
        check(owner.propagated_global_section({}, {81, 1}).status == Status::BudgetExceeded, "owning forwarding loses work admission");
        check(owner.propagated_global_section({}, {81, 100000, 1}).status == Status::BudgetExceeded, "owning forwarding loses scratch admission");
        auto declaration = owner.declaration(); declaration.revision = 2; declaration.charts.clear();
        for (std::uint32_t a = 0; a < 3; ++a) for (auto b = a + 1; b < 3; ++b)
            declaration.charts.push_back({6 + declaration.charts.size(), "original complete inequality", {{{0},0},{{0},1},{{0},2},{{1},0}},
                {{{0},a},{{0},b}}, {{101,102},{102,101}}});
        const OwnedRelationalFiniteSource distinct(owner.asset(), declaration);
        const auto distinct_bytes = encode_relational_finite_source(distinct);
        const auto hall = distinct.distinct_global_section({{{1},0}});
        check(hall.status == Status::Unsatisfiable && hall.work.value_trials == 0 && hall.work.distinct_groups == 1, "owning source lost original group Hall obstruction");
        const auto cold = decode_relational_finite_source(distinct_bytes, {});
        check(cold.distinct_global_section({{{1},0}}).status == hall.status && encode_relational_finite_source(distinct) == distinct_bytes, "group execution changed canonical source");
        check(distinct.distinct_global_section({{{1},0}}, {}, {}, stop.get_token()).status == Status::Cancelled, "owning distinct forwarding lost cancellation");
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
