#include "rfc/core/builder.hpp"
#include "rfc/inference/relational_finite_family.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace inference;
using Status = local::FiniteSearchStatus;
using Root = local::FinitePropagationStatus;
std::size_t checks{};
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
core::CellId cell(std::uint32_t i) {
    return {{0}, i};
}
OwnedRelationalFiniteSource source() {
    core::RelationSignature signature;
    signature.add_type("original ordered relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{50}, 0,
                    {{1, true, false, 1},
                     {2, true, true, 1},
                     {3, true, false, 1},
                     {1, false, true, 2},
                     {2, true, false, 0}});
    storage::NativeTemporalSeries h(storage::TemporalEndpoint(b.freeze(), 1), 28001);
    const auto pub = h.tail();
    RelationalInputUseDeclaration a;
    a.key = 1;
    a.channel = 1;
    a.encoding = "UTF-8";
    a.bytes = {std::byte{'x'}, std::byte{'x'}};
    RelationalSpanUse use;
    use.key = 1;
    use.spans = {{0, 2}};
    for (std::uint32_t i = 0; i < 3; ++i)
        use.cells.push_back(storage::stable_cell_key(pub.endpoint().source, cell(i)));
    use.cells.push_back(storage::stable_cell_key(pub.endpoint().source, {{1}, 0}));
    for (std::size_t i = 0; i < 5; ++i)
        use.occurrences.push_back({{1}, pub.endpoint().source.occurrence_identity({1}, i)});
    a.uses.push_back(use);
    RelationalFiniteSourceDeclaration d;
    d.key = 28;
    d.revision = 1;
    d.symbols = {{101, {std::byte{'a'}}}, {102, {std::byte{'a'}}}};
    for (std::uint32_t i = 0; i < 3; ++i) {
        d.variables.push_back({cell(i), {101, 102}});
        d.bindings.push_back({i + 1, 1, cell(i)});
    }
    d.charts = {{1,
                 "whole correlated symbolic field",
                 {cell(0), cell(1), cell(2), {{1}, 0}},
                 {cell(0), cell(1), cell(2)},
                 {{101, 102, 101}, {102, 101, 102}}}};
    return {OwnedRelationalInputUseAsset(h, a), d};
}
void run() {
    const auto owner = source();
    const auto packet = encode_relational_finite_source(owner);
    const auto p = *owner.prepare_global_section().prepared;
    const auto r = p.propagate();
    check(r.status == Root::FixedPoint && r.snapshot, "owning root failed");
    const auto root = *r.snapshot;
    check(root.source().same_owner(owner) && root.prepared().same_owner(p), "root lost actual owner/index");
    check(root.domains().size() == 3 && root.domains().at(cell(0)) == std::vector<std::uint64_t>{101, 102},
          "equal definitions merged or whole field lost");
    check(root.fixed().empty() && root.propagation_work().steps == r.work.steps &&
              root.retained_entries() > 0,
          "root provenance or retained cost lost");
    check(root.same_owner(root) && !root.same_owner(*p.propagate().snapshot),
          "root identity inferred by values");
    const OwnedRelationalFiniteSource::Assignment l{{cell(0), 101}}, q{{cell(0), 102}};
    const auto left = root.continue_global_section(l), right = root.continue_global_section(q);
    check(left.witness == p.global_section(l).witness && right.witness == p.global_section(q).witness,
          "owning continuation changed whole fields");
    check(left.witness->at(cell(1)) == 102 && right.witness->at(cell(1)) == 101,
          "continuation collapsed correlated directions");
    const auto conditioned = *p.propagate(l).snapshot;
    check(conditioned.fixed() == l && conditioned.domains().at(cell(1)) == std::vector<std::uint64_t>{102},
          "conditioned field/request missing");
    check(conditioned.continue_global_section({{cell(1), 101}}).status == Status::Unsatisfiable,
          "continuation erased an inherited condition");
    const auto retained = [] { return *source().prepare_global_section().prepared->propagate().snapshot; }();
    check(retained.continue_global_section(q).witness == right.witness, "root borrowed destroyed source");
    std::vector<std::future<bool>> readers;
    for (unsigned i = 0; i < 8; ++i)
        readers.push_back(std::async(std::launch::async, [root, l, q, left, right, i] {
            return root.continue_global_section(i % 2 ? l : q).witness ==
                   (i % 2 ? left.witness : right.witness);
        }));
    for (auto &reader : readers)
        check(reader.get(), "owning continuation shared mutable scratch");
    RelationalFiniteProjectionDeclaration d;
    d.key = 1;
    d.revision = 1;
    d.cells = {cell(0), cell(1), cell(2)};
    const auto image = OwnedRelationalFiniteFamily::projection(root.prepared(), d).materialize();
    check(image.image && image.image->size() == 2, "snapshot reuse changed exact correlated image");
    const auto restored = decode_relational_finite_source(packet, {});
    const auto cold = *restored.prepare_global_section().prepared->propagate().snapshot;
    check(!cold.source().same_owner(owner) && cold.domains() == root.domains() &&
              cold.continue_global_section(q).witness == right.witness,
          "cold recipe changed fields or merged owners");
    auto child_d = owner.declaration();
    ++child_d.revision;
    child_d.charts.push_back({2, "new original observation", {cell(0)}, {cell(0)}, {{102}}});
    const OwnedRelationalFiniteSource child(owner.asset(), child_d);
    const auto child_root = *child.prepare_global_section().prepared->propagate().snapshot;
    check(child_root.domains().at(cell(0)) == std::vector<std::uint64_t>{102} &&
              root.domains().at(cell(0)).size() == 2 &&
              root.continue_global_section(l).witness == left.witness,
          "child revision mutated parent snapshot");
    std::stop_source stop;
    stop.request_stop();
    check(p.propagate({}, {}, stop.get_token()).status == Root::Cancelled &&
              root.continue_global_section({}, {}, stop.get_token()).status == Status::Cancelled,
          "owning cancellation lost");
    check(p.propagate({}, {3, 1}).status == Root::BudgetExceeded &&
              root.continue_global_section({}, {3, 1}).status == Status::BudgetExceeded,
          "owning work budget lost");
    check(encode_relational_finite_source(owner) == packet &&
              encode_relational_finite_source(restored) == packet,
          "snapshot changed original bytes/flags/uses");
}
} // namespace
int main() {
    try {
        run();
        std::cout << "{\"checks\":" << checks << ",\"passed\":true}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
