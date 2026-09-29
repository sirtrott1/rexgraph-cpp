#include "engine_math.hpp"
#include <iostream>
#include <stdexcept>
#include <utility>
int main() {
    try {
        EngineMath engine; engine.set_amplitude(3, 7);
        const auto value = engine.boundary();
        if (value.size() != 2 || value[0].identity != 10 || value[1].identity != 11 ||
            value[0].numerator != "-3" || value[1].numerator != "3" || value[0].denominator != "7")
            throw std::runtime_error("native exact boundary mismatch");
        bool rejected = false; try { engine.set_amplitude(1, 0); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected || engine.boundary()[0].numerator != "-3") throw std::runtime_error("failed update changed state");
        engine.set_amplitude(0, 1);
        if (engine.retained_relations() != 1 || engine.boundary()[0].numerator != "0") throw std::runtime_error("zero erased assertion");
        auto moved = std::move(engine);
        if (moved.retained_relations() != 1) throw std::runtime_error("move lost source");
        std::cout << "external_cpp20_consumer=true exact_boundary=true zero_preserves_relation=true\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
