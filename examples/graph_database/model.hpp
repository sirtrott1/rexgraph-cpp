#pragma once
#include "../graph_history/model.hpp"

namespace graph_database_example {
inline rfc::storage::AlgebraicGraphSnapshot snapshot(std::uint64_t value) {
    auto graph=graph_history_example::graph(value);auto contents=graph.contents();
    for (auto& row:contents.archives) {
        const auto& bundle=row.value.bundle();const auto& field=bundle.field();auto archive=row.value.contents();
        const auto lower=bundle.stalk({rfc::core::Grade{0},1}),upper=bundle.stalk({rfc::core::Grade{1},0});
        std::vector<rfc::numeric::AlgebraicElement> lower_values(lower.rank(),field.scalar(0));lower_values[0]=field.scalar(1);
        std::vector<rfc::numeric::AlgebraicElement> upper_values;
        upper_values.push_back(field.scalar(rfc::numeric::Rational{std::to_string(value)})+field.generator());
        for (std::size_t i=1;i<upper.rank();++i) upper_values.push_back(field.scalar(rfc::numeric::Rational{std::to_string(i)}));
        archive.records.push_back({11,lower.vector(std::move(lower_values))});
        archive.records.push_back({30,upper.vector(std::move(upper_values))});
        row.value=rfc::storage::AlgebraicArchive::declare(bundle,std::move(archive));
    }
    auto result=rfc::storage::AlgebraicGraphSnapshot::anchor_only(rfc::storage::AlgebraicArchiveGraph::declare(std::move(contents)),1);
    return result.with_source_morphism(2,{{0,1},{0}});
}
} // namespace graph_database_example
