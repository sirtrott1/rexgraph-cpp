#include "rfc/core/builder.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace inference;
using Status = local::FiniteSearchStatus;
using Ready = local::FinitePreparationStatus;
std::size_t checks{};
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
OwnedRelationalFiniteSource source() {
    core::RelationSignature signature;
    const auto type =
        signature.add_type("original ordered relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{50}, type,
                    {{1, true, false, 1},
                     {2, true, true, 1},
                     {3, true, false, 1},
                     {1, false, true, 2},
                     {2, true, false, 0}});
    for (std::uint64_t i = 1; i <= 81; ++i)
        b.declare_witness_identity(i);
    storage::NativeTemporalSeries h(storage::TemporalEndpoint(b.freeze(), 1), 25001);
    const auto pub = h.tail();
    RelationalInputUseDeclaration a;
    a.key = 1;
    a.channel = 1;
    a.encoding = "UTF-8";
    a.bytes = {std::byte{'x'}, std::byte{'x'}};
    RelationalSpanUse use;
    use.key = 1;
    use.spans = {{0, 2}};
    for (std::uint32_t i = 0; i < 81; ++i)
        use.cells.push_back(storage::stable_cell_key(pub.endpoint().source, {{0}, i}));
    use.cells.push_back(storage::stable_cell_key(pub.endpoint().source, {{1}, 0}));
    for (std::size_t i = 0; i < 5; ++i)
        use.occurrences.push_back({{1}, pub.endpoint().source.occurrence_identity({1}, i)});
    a.uses.push_back(use);
    RelationalFiniteSourceDeclaration d;
    d.key = 25;
    d.revision = 1;
    d.symbols = {{101, {std::byte{'a'}}}, {102, {std::byte{'a'}}}};
    for (std::uint32_t i = 0; i < 81; ++i) {
        d.variables.push_back({{{0}, i}, {101, 102}});
        d.bindings.push_back({i + 1, 1, {{0}, i}});
    }
    d.charts = {{1,
                 "whole correlated symbolic field",
                 {{{0}, 0}, {{0}, 1}, {{0}, 2}, {{1}, 0}},
                 {{{0}, 0}, {{0}, 1}, {{0}, 2}},
                 {{101, 102, 101}, {102, 101, 102}}}};
    return {OwnedRelationalInputUseAsset(h, a), d};
}
} // namespace
int main() {
    try {
        const auto owner = source();
        const auto packet = encode_relational_finite_source(owner);
        const auto preparation = owner.prepare_global_section();
        check(preparation.status == Ready::Ready && preparation.prepared.has_value(),
              "owning preparation failed");
        const auto p = *preparation.prepared;
        check(p.source().same_owner(owner) && p.preparation_work().steps == preparation.work.steps,
              "prepared search lost actual source owner or work");
        const auto other = owner.prepare_global_section();
        check(!p.same_owner(*other.prepared), "independent prepared indexes were silently identified");
        const auto copied = p;
        check(p.same_owner(copied), "prepared copy lost shared immutable index");
        const OwnedRelationalFiniteSource::Assignment left{{{{0}, 0}, 101}}, right{{{{0}, 0}, 102}};
        const auto l = p.global_section(left), r = p.global_section(right);
        check(l.witness == owner.propagated_global_section(left).witness &&
                  r.witness == owner.propagated_global_section(right).witness,
              "prepared owner changed complete response");
        check(l.witness->size() == 81 && r.witness->at({{0}, 1}) == 101 && r.witness->at({{0}, 2}) == 102,
              "equal symbol definitions merged or correlations were lost");
        const auto retained = [] { return *source().prepare_global_section().prepared; }();
        check(retained.global_section(right).witness == r.witness, "prepared owner borrows destroyed source");
        std::vector<std::future<void>> readers;
        for (unsigned i = 0; i < 12; ++i)
            readers.push_back(std::async(std::launch::async, [p, left, right, l, r, i] {
                const auto &fixed = (i % 2) ? left : right;
                const auto &expected = (i % 2) ? l : r;
                for (unsigned j = 0; j < 8; ++j)
                    if (p.global_section(fixed).witness != expected.witness)
                        throw std::runtime_error("shared scratch or cross-request rollback");
            }));
        for (auto &reader : readers)
            reader.get();
        check(true, "concurrent readers");
        check(encode_relational_finite_source(p.source()) == packet,
              "prepared execution changed source bytes/flags/uses");
        auto restored = decode_relational_finite_source(packet, {});
        const auto cold = restored.prepare_global_section();
        check(!cold.prepared->source().same_owner(owner) &&
                  cold.prepared->global_section(right).witness == r.witness &&
                  encode_relational_finite_source(cold.prepared->source()) == packet,
              "cold preparation inferred owner identity or changed source");
        auto d = owner.declaration();
        ++d.revision;
        d.charts.push_back({2, "new named observation", {{{0}, 0}}, {{{0}, 0}}, {{102}}});
        const OwnedRelationalFiniteSource child(owner.asset(), d);
        const auto child_prepared = child.prepare_global_section();
        check(child_prepared.prepared->global_section(left).status == Status::Unsatisfiable &&
                  p.global_section(left).witness == l.witness &&
                  encode_relational_finite_source(owner) == packet,
              "child revision changed parent prepared state");
        std::stop_source stop;
        stop.request_stop();
        const auto cancelled = owner.prepare_global_section({}, {}, stop.get_token());
        check(cancelled.status == Ready::Cancelled && !cancelled.prepared,
              "owning preparation cancellation lost");
        check(p.global_section({}, {}, stop.get_token()).status == Status::Cancelled,
              "owning prepared solve cancellation lost");
        check(owner.prepare_global_section({}, {81, 1}).status == Ready::BudgetExceeded,
              "owning preparation budget lost");
        check(p.global_section({}, {81, 1}).status == Status::BudgetExceeded,
              "owning prepared solve budget lost");
        check(p.global_section({}, {81, 100000, 1}).status == Status::BudgetExceeded,
              "owning prepared workspace admission lost");
        std::cout << "{\"checks\":" << checks << ",\"concurrent_solves\":96,\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
