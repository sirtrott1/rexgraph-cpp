#pragma once
#include "rfc/composite/planes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::composite {

// A tiny word-code for composite-binary logic. It executes directly on packed
// machine words; no relation is expanded to one Boolean object per slot.
enum class BinaryOpcode : std::uint8_t {
    LoadAspect,
    LoadZero,
    LoadOne,
    Not,
    And,
    Or,
    Xor,
    AndNot,
    Select
};

struct BinaryInstruction {
    BinaryOpcode opcode{BinaryOpcode::LoadZero};
    std::uint16_t dst{};
    std::uint16_t a{};
    std::uint16_t b{};
    std::uint16_t c{};
    AspectId aspect{};
    bool operator==(const BinaryInstruction&) const = default;
};

class BinaryProgram {
public:
    using word_type = machine::PackedBits::word_type;

    explicit BinaryProgram(std::uint16_t register_count = 1) : register_count_(register_count) {
        if (register_count_ == 0) throw std::invalid_argument("binary program requires at least one register");
    }

    [[nodiscard]] std::uint16_t register_count() const noexcept { return register_count_; }
    [[nodiscard]] std::size_t instruction_count() const noexcept { return instructions_.size(); }

    [[nodiscard]] const std::vector<BinaryInstruction>& instructions() const noexcept { return instructions_; }

    void append(BinaryInstruction instruction) {
        require_register(instruction.dst);
        switch (instruction.opcode) {
            case BinaryOpcode::LoadAspect:
            case BinaryOpcode::LoadZero:
            case BinaryOpcode::LoadOne:
                break;
            case BinaryOpcode::Not:
                require_register(instruction.a);
                break;
            case BinaryOpcode::And:
            case BinaryOpcode::Or:
            case BinaryOpcode::Xor:
            case BinaryOpcode::AndNot:
                require_register(instruction.a);
                require_register(instruction.b);
                break;
            case BinaryOpcode::Select:
                require_register(instruction.a);
                require_register(instruction.b);
                require_register(instruction.c);
                break;
            default:
                throw std::invalid_argument("unknown binary opcode");
        }
        // Unused operands have no semantics. Canonicalize before retaining a
        // program so padding cannot change its operational wire identity.
        if (instruction.opcode != BinaryOpcode::LoadAspect) instruction.aspect = 0;
        switch (instruction.opcode) {
            case BinaryOpcode::LoadAspect:
            case BinaryOpcode::LoadZero:
            case BinaryOpcode::LoadOne: instruction.a = instruction.b = instruction.c = 0; break;
            case BinaryOpcode::Not: instruction.b = instruction.c = 0; break;
            case BinaryOpcode::Select: break;
            default: instruction.c = 0; break;
        }
        instructions_.push_back(instruction);
    }

    [[nodiscard]] word_type execute_word(const AspectPlanes& planes,
                                         std::size_t word_index,
                                         std::uint16_t output_register) const {
        require_register(output_register);
        if (planes.aspect_count() == 0) throw std::invalid_argument("binary program requires aspect planes");
        if (word_index >= planes.plane(0).word_count()) throw std::out_of_range("binary program word index");
        std::vector<word_type> registers(register_count_, word_type{0});
        execute_word_into(planes, word_index, registers);
        return registers[output_register];
    }

    // Zero-copy execution hook for mapped/paged aspect planes.  The provider
    // must implement word_type(AspectId, std::size_t word_index).  Composite
    // semantics stay in the compiled BinaryProgram; storage merely supplies
    // packed words.
    template <class WordProvider>
    [[nodiscard]] word_type execute_word_with_provider(std::size_t word_index,
                                                       std::uint16_t output_register,
                                                       WordProvider&& provider) const {
        require_register(output_register);
        std::vector<word_type> registers(register_count_, word_type{0});
        execute_word_into_provider(word_index, registers, std::forward<WordProvider>(provider));
        return registers[output_register];
    }

    // Domain-aware provider path for persistence, hashing and final-word reads.
    // The raw provider API intentionally retains its machine-word contract.
    template <class WordProvider>
    [[nodiscard]] word_type execute_domain_word_with_provider(std::size_t slots,
                                                               std::size_t word_index,
                                                               std::uint16_t output_register,
                                                               WordProvider&& provider) const {
        const auto words = slots / 64u + (slots % 64u != 0);
        if (word_index >= words) throw std::out_of_range("binary domain word index");
        auto result = execute_word_with_provider(word_index, output_register, std::forward<WordProvider>(provider));
        if (word_index + 1u == words && slots % 64u != 0)
            result &= (word_type{1} << (slots % 64u)) - 1u;
        return result;
    }

    [[nodiscard]] machine::PackedBits execute(const AspectPlanes& planes,
                                               std::uint16_t output_register) const {
        require_register(output_register);
        if (planes.aspect_count() == 0) throw std::invalid_argument("binary program requires aspect planes");
        machine::PackedBits result(planes.slots(), false);
        std::vector<word_type> registers(register_count_, word_type{0});
        for (std::size_t word = 0; word < result.word_count(); ++word) {
            std::fill(registers.begin(), registers.end(), word_type{0});
            execute_word_into(planes, word, registers);
            result.assign_word(word, registers[output_register]);
        }
        return result;
    }

private:
    friend class BinaryExecutionPlan;
    void execute_word_into(const AspectPlanes& planes,
                           std::size_t word_index,
                           std::vector<word_type>& registers) const {
        for (const auto& instruction : instructions_) {
            switch (instruction.opcode) {
                case BinaryOpcode::LoadAspect:
                    registers[instruction.dst] = planes.plane(instruction.aspect).word(word_index);
                    break;
                case BinaryOpcode::LoadZero:
                    registers[instruction.dst] = word_type{0};
                    break;
                case BinaryOpcode::LoadOne:
                    registers[instruction.dst] = ~word_type{0};
                    break;
                case BinaryOpcode::Not:
                    registers[instruction.dst] = ~registers[instruction.a];
                    break;
                case BinaryOpcode::And:
                    registers[instruction.dst] = registers[instruction.a] & registers[instruction.b];
                    break;
                case BinaryOpcode::Or:
                    registers[instruction.dst] = registers[instruction.a] | registers[instruction.b];
                    break;
                case BinaryOpcode::Xor:
                    registers[instruction.dst] = registers[instruction.a] ^ registers[instruction.b];
                    break;
                case BinaryOpcode::AndNot:
                    registers[instruction.dst] = registers[instruction.a] & ~registers[instruction.b];
                    break;
                case BinaryOpcode::Select:
                    registers[instruction.dst] =
                        (registers[instruction.a] & registers[instruction.b]) |
                        (~registers[instruction.a] & registers[instruction.c]);
                    break;
                default: throw std::invalid_argument("unknown binary opcode");
            }
        }
    }

    template <class WordProvider>
    void execute_word_into_provider(std::size_t word_index,
                                    std::vector<word_type>& registers,
                                    WordProvider&& provider) const {
        for (const auto& instruction : instructions_) {
            switch (instruction.opcode) {
                case BinaryOpcode::LoadAspect:
                    registers[instruction.dst] = provider(instruction.aspect, word_index);
                    break;
                case BinaryOpcode::LoadZero:
                    registers[instruction.dst] = word_type{0};
                    break;
                case BinaryOpcode::LoadOne:
                    registers[instruction.dst] = ~word_type{0};
                    break;
                case BinaryOpcode::Not:
                    registers[instruction.dst] = ~registers[instruction.a];
                    break;
                case BinaryOpcode::And:
                    registers[instruction.dst] = registers[instruction.a] & registers[instruction.b];
                    break;
                case BinaryOpcode::Or:
                    registers[instruction.dst] = registers[instruction.a] | registers[instruction.b];
                    break;
                case BinaryOpcode::Xor:
                    registers[instruction.dst] = registers[instruction.a] ^ registers[instruction.b];
                    break;
                case BinaryOpcode::AndNot:
                    registers[instruction.dst] = registers[instruction.a] & ~registers[instruction.b];
                    break;
                case BinaryOpcode::Select:
                    registers[instruction.dst] =
                        (registers[instruction.a] & registers[instruction.b]) |
                        (~registers[instruction.a] & registers[instruction.c]);
                    break;
                default: throw std::invalid_argument("unknown binary opcode");
            }
        }
    }

    void require_register(std::uint16_t index) const {
        if (index >= register_count_) throw std::out_of_range("binary program register");
    }

    std::uint16_t register_count_{};
    std::vector<BinaryInstruction> instructions_;
};

// Canonical existence/orientation realization at machine-word level.
struct SignedMasks {
    machine::PackedBits positive;
    machine::PackedBits negative;
};

inline SignedMasks signed_masks(const AspectPlanes& planes,
                                AspectId existence,
                                AspectId orientation) {
    const auto& exists = planes.plane(existence);
    const auto& negative = planes.plane(orientation);
    return SignedMasks{exists.and_not(negative), exists & negative};
}

inline BinaryProgram compile_active_orientation_query(AspectId existence,
                                                      AspectId orientation,
                                                      bool want_negative) {
    // r0 = existence, r1 = orientation, r2 = exists & (orientation or !orientation)
    BinaryProgram program(3);
    program.append({BinaryOpcode::LoadAspect, 0, 0, 0, 0, existence});
    program.append({BinaryOpcode::LoadAspect, 1, 0, 0, 0, orientation});
    program.append({want_negative ? BinaryOpcode::And : BinaryOpcode::AndNot, 2, 0, 1, 0, 0});
    return program;
}

} // namespace rfc::composite
