#include "rfc/composite/aspect.hpp"
#include "rfc/composite/planes.hpp"
#include "rfc/composite/program.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        auto schema = rfc::composite::AspectSchema::canonical();
        const auto visible = schema.add("visible", rfc::composite::AspectRole::Visibility, false);
        rfc::composite::AspectPlanes planes(schema, 130);
        const auto exists = schema.id("exists");
        const auto orientation = schema.id("orientation");

        // canonical schema defaults exists=true. Give three slots negative orientation.
        planes.set(orientation, 1, true);
        planes.set(orientation, 64, true);
        planes.set(orientation, 129, true);
        planes.set(visible, 0, true);
        planes.set(visible, 1, true);
        planes.set(visible, 64, true);
        planes.set(exists, 5, false);

        const auto masks = rfc::composite::signed_masks(planes, exists, orientation);
        require(masks.negative.count() == 3u, "negative signed mask mismatch");
        require(masks.positive.count() == 126u, "positive signed mask mismatch");
        require(!masks.positive.get(5), "absent slot appeared positive");

        const auto negative_program = rfc::composite::compile_active_orientation_query(exists, orientation, true);
        const auto queried_negative = negative_program.execute(planes, 2);
        require(queried_negative.count() == 3u, "compiled negative query mismatch");

        // Exact machine-word program: visible AND active AND NOT negative.
        rfc::composite::BinaryProgram program(5);
        program.append({rfc::composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, exists});
        program.append({rfc::composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
        program.append({rfc::composite::BinaryOpcode::AndNot, 2, 0, 1, 0, 0});
        program.append({rfc::composite::BinaryOpcode::LoadAspect, 3, 0, 0, 0, visible});
        program.append({rfc::composite::BinaryOpcode::And, 4, 2, 3, 0, 0});
        const auto result = program.execute(planes, 4);
        require(result.count() == 1u && result.get(0), "word-code aspect program mismatch");

        // XOR is native orientation composition: two flips cancel.
        rfc::composite::BinaryProgram xor_program(3);
        xor_program.append({rfc::composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, orientation});
        xor_program.append({rfc::composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
        xor_program.append({rfc::composite::BinaryOpcode::Xor, 2, 0, 1, 0, 0});
        require(xor_program.execute(planes, 2).none(), "orientation XOR cancellation failed");

        std::cout << "machine composite-binary algebra tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
