#include "rfc/core/builder.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"
#include "rfc/storage/file_io.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected fixture destination");
        using namespace rfc;
        core::RelationSignature signature;
        const auto type = signature.add_type("distinct pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(core::RelationKey{50}, type,
                              {{10, true, false, 1}, {20, true, true, 1}});
        storage::NativeTemporalSeries history(storage::TemporalEndpoint(builder.freeze(), 1), 91001);
        const auto publication = history.tail();
        const auto& complex = publication.endpoint().source;
        const core::CellId left{{0}, 0}, right{{0}, 1}, pair{{1}, 0};
        inference::RelationalInputUseDeclaration input;
        input.key = 1; input.channel = 1; input.encoding = "UTF-8";
        input.bytes = {std::byte{'a'}, std::byte{'b'}};
        inference::RelationalSpanUse use;
        use.key = 1; use.spans = {{0, 2}};
        for (const auto cell : {left, right, pair})
            use.cells.push_back(storage::stable_cell_key(complex, cell));
        input.uses.push_back(use);
        inference::RelationalFiniteSourceDeclaration declaration;
        declaration.key = 1; declaration.revision = 1;
        declaration.symbols = {{11, {std::byte{'a'}}}, {12, {std::byte{'b'}}}};
        declaration.variables = {{left, {11, 12}}, {right, {11, 12}}};
        declaration.bindings = {{1, 1, left}, {2, 1, right}};
        declaration.charts = {{1, "complete distinct pair", {left, right, pair},
                               {left, right}, {{11, 12}, {12, 11}}}};
        const inference::OwnedRelationalFiniteSource source(
            inference::OwnedRelationalInputUseAsset(history, input), declaration);
        storage::write_file_exact(argv[1], inference::encode_relational_finite_source(source));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
