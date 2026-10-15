#include "fixture.hpp"
#include <iostream>

int main() {
    try {
        using namespace rfc;
        using namespace native_product_example;
        storage::NativeAccessionDirectory a(storage::TemporalEndpoint(interval(), 7), 900),
            b(storage::TemporalEndpoint(interval(), 8), 901);
        const auto product = Plan::prepare(request(a, b), available);
        const auto input = product.field(core::Grade{2}, {{cell(1, 0, 1, 0), Q{1}}}, available);
        const auto first = product.boundary(input, available), second = product.boundary(first, available);
        Guard guard;
        const auto occurrences = product.product().retained_boundary(cell(1, 0, 1, 0), guard);
        std::size_t inactive{}, supported_zero{};
        for (const auto& o : occurrences) { inactive += !o.exists; supported_zero += o.exists && o.coefficient == 0; }
        if (occurrences.size() != 12 || first.field().values().size() != 4 || second.field().values().size() != 4) return 1;
        for (const auto& [id, value] : second.field().values()) { (void)id; if (value != 0) return 1; }
        std::cout << "square_cells=" << product.product().cell_count(core::Grade{0}) << ','
                  << product.product().cell_count(core::Grade{1}) << ',' << product.product().cell_count(core::Grade{2})
                  << "\nretained_top_occurrences=" << occurrences.size() << "\ninactive_occurrences=" << inactive
                  << "\nactive_zero_occurrences=" << supported_zero << '\n';
        for (const auto& [id, value] : first.field().values())
            std::cout << "boundary=(" << id.left.grade.value << ',' << id.left.index << ";"
                      << id.right.grade.value << ',' << id.right.index << ") " << value << '\n';
        auto left = factor_map(1, 1, 2, 1, 3), right = factor_map(1, 1, -1, 2, 1);
        left.pop_back(); right.pop_back();
        const auto map = Map::prepare(product, product, left, right, {available, available});
        const auto mapped = map.execute(input, {available, available});
        if (mapped.field().values().at(cell(1, 0, 1, 0)) != -2 || !mapped.chain_map()) return 1;
        std::cout << "mapped_top_coefficient=" << mapped.field().values().at(cell(1, 0, 1, 0))
                  << "\nboundary_squared=0\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
