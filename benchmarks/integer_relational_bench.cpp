#include "rfc/core/builder.hpp"
#include "rfc/field/lattice_field.hpp"
#include "rfc/learning/integer_relational_layer.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    constexpr std::size_t vertices = 50000;
    constexpr std::size_t repeats = 100;
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < vertices; ++i) {
        const auto j = (i + 1u) % vertices;
        builder.add_grade_one(edge, {
            {static_cast<rfc::core::IdentityKey>(i), true, true, false, 1, {}},
            {static_cast<rfc::core::IdentityKey>(j), true, false, false, 1, {}}
        });
    }
    const auto complex = builder.freeze();
    std::vector<std::int64_t> values(vertices);
    for (std::size_t i = 0; i < vertices; ++i) values[i] = static_cast<std::int64_t>((i % 11u)) - 5;
    rfc::field::MachineLatticeField field(complex, rfc::core::Grade{1}, values);
    rfc::learning::MachineIntegerRelationalLayer layer(complex, rfc::core::Grade{1}, 1, 1, 0, 0);

    std::int64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < repeats; ++r) {
        const auto output = layer.forward(field);
        checksum += output.values()[r % output.values().size()];
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    std::cout << "relations=" << vertices << " incidences=" << (2u * vertices)
              << " repeats=" << repeats << " elapsed_ns=" << ns
              << " checksum=" << checksum << '\n';
    return 0;
}
