#pragma once

#include "rfc/composite/program.hpp"
#include <memory>
#include <span>
#include <limits>

namespace rfc::composite {

// Physical execution policy, never persisted type or operation semantics.
struct BinaryExecutionLimits {
    std::size_t max_instructions{1u << 20};
    std::size_t max_outputs{65535};
    std::size_t max_output_words{machine::PackedBits::default_word_budget};
};

// One workspace per execution/thread. No shared mutable global scratch. A sink
// or provider cannot reenter, move, clear or replace an active workspace.
class BinaryExecutionWorkspace {
public:
    using word_type = BinaryProgram::word_type;
    BinaryExecutionWorkspace() = default;
    BinaryExecutionWorkspace(const BinaryExecutionWorkspace&) = delete;
    BinaryExecutionWorkspace& operator=(const BinaryExecutionWorkspace&) = delete;
    BinaryExecutionWorkspace(BinaryExecutionWorkspace&&) = delete;
    BinaryExecutionWorkspace& operator=(BinaryExecutionWorkspace&&) = delete;
    [[nodiscard]] std::size_t growth_events() const noexcept { return growth_events_; }
    [[nodiscard]] std::size_t capacity_words() const noexcept { return registers_.capacity() + outputs_.capacity(); }
    [[nodiscard]] std::size_t completed_words() const noexcept { return completed_words_; }
    void release() {
        if (busy_) throw std::logic_error("cannot release an active binary workspace");
        std::vector<word_type>{}.swap(registers_);
        std::vector<word_type>{}.swap(outputs_);
    }
private:
    friend class BinaryExecutionPlan;
    struct Lease {
        BinaryExecutionWorkspace& workspace;
        explicit Lease(BinaryExecutionWorkspace& w) : workspace(w) {
            if (w.busy_) throw std::logic_error("binary workspace execution reentry");
            w.busy_ = true;
        }
        ~Lease() { workspace.busy_ = false; }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
    };
    void prepare(std::size_t registers, std::size_t outputs) {
        if (registers > registers_.capacity()) { registers_.reserve(registers); ++growth_events_; }
        if (outputs > outputs_.capacity()) { outputs_.reserve(outputs); ++growth_events_; }
        registers_.resize(registers);
        outputs_.resize(outputs);
    }
    std::vector<word_type> registers_, outputs_;
    bool busy_{};
    std::size_t growth_events_{}, completed_words_{};
};

// Immutable execution lowering of a BinaryProgram and its requested outputs.
// All instructions (including unused loads) retain their order. Only physical
// register labels and redundant zero initialization change. The original
// BinaryProgram remains the canonical persisted operational identity.
class BinaryExecutionPlan {
public:
    using word_type = BinaryProgram::word_type;
    explicit BinaryExecutionPlan(const BinaryProgram& program,
                                 std::vector<std::uint16_t> outputs,
                                 BinaryExecutionLimits limits = {}) {
        if (outputs.empty() || outputs.size() > limits.max_outputs)
            throw std::length_error("binary execution output budget");
        if (program.instruction_count() > limits.max_instructions)
            throw std::length_error("binary execution instruction budget");
        for (auto r : outputs) program.require_register(r);
        constexpr auto unused = std::numeric_limits<std::uint16_t>::max();
        std::vector<std::uint16_t> slots(program.register_count(), unused);
        std::uint16_t count = 0;
        const auto slot = [&](std::uint16_t reg) {
            if (slots[reg] == unused) slots[reg] = count++;
            return slots[reg];
        };
        auto lowered = program.instructions();
        for (auto& i : lowered) {
            operands(i, [&](auto& r) { r = slot(r); });
            i.dst = slot(i.dst);
        }
        for (auto& r : outputs) r = slot(r);
        BinaryProgram physical(count);
        std::vector<bool> written(count, false), needs_zero(count, false);
        for (auto i : lowered) {
            operands(i, [&](auto& r) { if (!written[r]) needs_zero[r] = true; });
            written[i.dst] = true;
            physical.append(i);
        }
        for (auto r : outputs) if (!written[r]) needs_zero[r] = true;
        std::vector<std::uint16_t> zero;
        for (std::size_t i = 0; i < count; ++i)
            if (needs_zero[i]) zero.push_back(static_cast<std::uint16_t>(i));
        state_ = std::make_shared<const State>(std::move(physical), std::move(outputs), std::move(zero), limits);
    }
    [[nodiscard]] std::size_t register_words() const { return state().program.register_count(); }
    [[nodiscard]] std::size_t zeroed_register_words() const { return state().zero.size(); }
    [[nodiscard]] std::size_t output_count() const { return state().outputs.size(); }
    [[nodiscard]] std::size_t instruction_count() const { return state().program.instruction_count(); }
    void validate_aspects(std::size_t count) const {
        for (const auto& i : state().program.instructions())
            if (i.opcode == BinaryOpcode::LoadAspect && i.aspect >= count)
                throw std::out_of_range("binary execution aspect");
    }

    // Provider reads are in word/instruction order, once per instruction per
    // word, irrespective of output count. It must provide a stable snapshot.
    // The output span is borrowed only for the synchronous sink invocation.
    // Sink side effects are not rolled back on exceptions; materialize() below
    // instead publishes only a complete owning result.
    template <class Provider, class Sink>
    void for_each_word(std::size_t slots, Provider&& provider, Sink&& sink,
                       BinaryExecutionWorkspace& workspace) const {
        const auto pin = checked_pin(); // callback may replace the caller's plan
        typename BinaryExecutionWorkspace::Lease lease(workspace);
        execute(*pin, slots, provider, sink, workspace);
    }

    template <class Provider>
    [[nodiscard]] std::vector<machine::PackedBits> materialize(
        std::size_t slots, Provider&& provider, BinaryExecutionWorkspace& workspace) const {
        const auto pin = checked_pin();
        typename BinaryExecutionWorkspace::Lease lease(workspace);
        const auto words = machine::PackedBits::words_for_size(slots);
        const auto total = machine::checked_size_mul(words, pin->outputs.size());
        if (total > pin->limits.max_output_words)
            throw std::length_error("binary execution aggregate output budget");
        std::vector<machine::PackedBits> result;
        result.reserve(pin->outputs.size());
        for (std::size_t i = 0; i < pin->outputs.size(); ++i)
            result.emplace_back(slots, false, pin->limits.max_output_words);
        const auto sink = [&](std::size_t word, std::span<const word_type> values) {
            for (std::size_t i = 0; i < values.size(); ++i) result[i].assign_word(word, values[i]);
        };
        execute(*pin, slots, provider, sink, workspace);
        return result;
    }
    [[nodiscard]] std::vector<machine::PackedBits> materialize(
        const AspectPlanes& planes, BinaryExecutionWorkspace& workspace) const {
        if (planes.aspect_count() == 0) throw std::invalid_argument("binary execution requires aspect planes");
        validate_aspects(planes.aspect_count()); // including an empty domain
        return materialize(planes.slots(), [&](AspectId a, std::size_t w) { return planes.plane(a).word(w); }, workspace);
    }
private:
    template <class Fn> static void operands(BinaryInstruction& i, Fn&& fn) {
        switch (i.opcode) {
            case BinaryOpcode::LoadAspect: case BinaryOpcode::LoadZero: case BinaryOpcode::LoadOne: break;
            case BinaryOpcode::Not: fn(i.a); break;
            case BinaryOpcode::And: case BinaryOpcode::Or: case BinaryOpcode::Xor: case BinaryOpcode::AndNot: fn(i.a); fn(i.b); break;
            case BinaryOpcode::Select: fn(i.a); fn(i.b); fn(i.c); break;
            default: throw std::invalid_argument("unknown binary execution opcode");
        }
    }
    struct State {
        BinaryProgram program;
        std::vector<std::uint16_t> outputs, zero;
        BinaryExecutionLimits limits;
        State(BinaryProgram p, std::vector<std::uint16_t> out, std::vector<std::uint16_t> z, BinaryExecutionLimits l)
            : program(std::move(p)), outputs(std::move(out)), zero(std::move(z)), limits(l) {}
    };
    [[nodiscard]] const State& state() const {
        if (!state_) throw std::logic_error("moved-from binary execution plan");
        return *state_;
    }
    [[nodiscard]] std::shared_ptr<const State> checked_pin() const { (void)state(); return state_; }
    template <class Provider, class Sink>
    static void execute(const State& s, std::size_t slots, Provider& provider, Sink& sink,
                        BinaryExecutionWorkspace& workspace) {
        const auto words = machine::PackedBits::words_for_size(slots);
        if (!words) return;
        workspace.prepare(s.program.register_count(), s.outputs.size());
        for (std::size_t word = 0; word < words; ++word) {
            for (auto r : s.zero) workspace.registers_[r] = 0;
            s.program.execute_word_into_provider(word, workspace.registers_, provider);
            const auto mask = word + 1u == words && slots % 64u
                ? (word_type{1} << (slots % 64u)) - 1u : ~word_type{0};
            for (std::size_t i = 0; i < s.outputs.size(); ++i)
                workspace.outputs_[i] = workspace.registers_[s.outputs[i]] & mask;
            sink(word, std::span<const word_type>(workspace.outputs_));
            if (workspace.completed_words_ != std::numeric_limits<std::size_t>::max()) ++workspace.completed_words_;
        }
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::composite
