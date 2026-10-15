#pragma once
#include "../algebraic_graph/model.hpp"
#include "rfc/storage/algebraic_graph_snapshot_codec.hpp"

namespace graph_history_example {
using Snapshot=rfc::storage::AlgebraicGraphSnapshot;
using Graph=rfc::storage::AlgebraicArchiveGraph;
using Q=rfc::numeric::Rational;
inline Graph graph(std::uint64_t value) {
    auto original=algebraic_graph_example::make();auto c=original.contents();
    const auto& carrier=std::get<rfc::local::AlgebraicModuleVector>(original.archive(1).at(10)).carrier();
    const auto x=carrier.vector({carrier.field().scalar(Q{std::to_string(value)})+carrier.field().generator(),carrier.field().scalar(0)});
    for (auto& row:c.archives) {
        auto contents=row.value.contents();
        if (row.key==1 || row.key==4) contents.records.front().value=x;
        else if (row.key==2) contents.records.front().value=original.carrier(1).apply(x);
        else if (row.key==3) contents.records.front().value=original.carrier(2).apply(original.carrier(1).apply(x));
        else if (row.key==5) contents.records.front().value=original.carrier(3).apply(x);
        row.value=rfc::storage::AlgebraicArchive::declare(row.value.bundle(),std::move(contents));
    }
    return Graph::declare(std::move(c));
}
inline Snapshot snapshot(std::uint64_t value,bool mapped=false) {
    auto result=Snapshot::anchor_only(graph(value),1);
    if (mapped) result=result.with_source_morphism(2,{{0,1},{0}});
    return result;
}
} // namespace graph_history_example
