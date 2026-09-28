#include "rfc/composite/aspect.hpp"
#include "rfc/composite/planes.hpp"
#include "rfc/composite/program.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::size_t slots = 1u << 20u;
    constexpr std::size_t repeats = 2000u;
    auto schema = rfc::composite::AspectSchema::canonical();
    const auto visible = schema.add("visible", rfc::composite::AspectRole::Visibility, false);
    const auto exists = schema.id("exists");
    const auto orientation = schema.id("orientation");
    rfc::composite::AspectPlanes planes(schema, slots);
    for (std::size_t i = 0; i < slots; ++i) {
        planes.set(orientation, i, (i % 5u) == 0u);
        planes.set(visible, i, (i % 3u) != 0u);
        if ((i % 17u) == 0u) planes.set(exists, i, false);
    }

    rfc::composite::BinaryProgram program(5);
    program.append({rfc::composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, exists});
    program.append({rfc::composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
    program.append({rfc::composite::BinaryOpcode::AndNot, 2, 0, 1, 0, 0});
    program.append({rfc::composite::BinaryOpcode::LoadAspect, 3, 0, 0, 0, visible});
    program.append({rfc::composite::BinaryOpcode::And, 4, 2, 3, 0, 0});

    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < repeats; ++r) checksum += program.execute(planes, 4).count();
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "slots=" << slots << " repeats=" << repeats
              << " elapsed_ns=" << elapsed << " checksum=" << checksum << '\n';
}
