#pragma once

#include "rfc/storage/relational_code_types.hpp"
#include "rfc/composite/execution.hpp"
#include <stop_token>

namespace rfc::storage {

// Host policies, not serialized type declarations or execution authority.
struct RelationalBinaryPreparationLimits {
    composite::BinaryExecutionLimits binary{};
    std::size_t max_types{65536}, max_operations{65536};
    std::size_t max_type_items{1u << 20}, max_aspect_name_bytes{1u << 20};
};
struct RelationalBinaryRequest {
    // Admission units: 1 + words * (instructions + selected outputs).
    // This is deliberately NOT the reference interpreter's per-output budget.
    std::size_t max_work{1u << 26};
    std::size_t max_output_words{machine::PackedBits::default_word_budget};
    std::stop_token stop{};
};
class RelationalBinaryCancelled : public std::runtime_error {
public:
    RelationalBinaryCancelled() : std::runtime_error("prepared relational Binary execution cancelled") {}
};
struct RelationalBinaryReceipt {
    std::uint64_t namespace_key{}, operation{}, input_type{}, output_type{};
    std::size_t slots{}, words{}, instructions{}, aspect_reads{}, output_words{}, admitted_work{};
    // A successful invocation executes one complete program per domain word.
    // Empty domains execute zero programs. These are local execution counters,
    // not authenticated evidence, publication IDs or persisted cache authority.
};
struct RelationalBinaryResult {
    std::vector<machine::PackedBits> outputs;
    RelationalBinaryReceipt receipt;
};
struct RelationalBinaryValueResult {
    RelationalCodeValue value;
    RelationalBinaryReceipt receipt;
};

// Explicit prepared path for ONE Binary operation. RelationalCodeAlgebra::apply
// remains unchanged, including its historical cost and repeated-provider semantics.
// This path retains the selected operation and its reachable Boolean/product types;
// it does not claim to validate unrelated sections, interfaces or publications.
class PreparedRelationalBinaryOperation {
public:
    using word_type = composite::BinaryProgram::word_type;
    PreparedRelationalBinaryOperation(const RelationalCodeState& codes, std::uint64_t key,
                                      RelationalBinaryPreparationLimits limits = {})
        : state_(std::make_shared<const State>(snapshot(codes, key, limits), key, limits)) {}
    // Copy even from rvalues, keeping both handles usable. Callback replacement of
    // a caller's handle cannot change an active invocation's pinned state.
    PreparedRelationalBinaryOperation(const PreparedRelationalBinaryOperation&) = default;
    PreparedRelationalBinaryOperation& operator=(const PreparedRelationalBinaryOperation&) = default;

    [[nodiscard]] const RelationalCodeState& declarations() const & { return state_->codes; }
    const RelationalCodeState& declarations() const && = delete;
    [[nodiscard]] const RelationalCodeOperation& operation() const & { return state_->op(); }
    const RelationalCodeOperation& operation() const && = delete;
    [[nodiscard]] const RelationalCodeType& input_type() const & { return state_->in(); }
    const RelationalCodeType& input_type() const && = delete;
    [[nodiscard]] const RelationalCodeType& output_type() const & { return state_->out(); }
    const RelationalCodeType& output_type() const && = delete;
    [[nodiscard]] std::size_t register_words() const { return state_->plan.register_words(); }
    [[nodiscard]] std::size_t output_count() const { return state_->plan.output_count(); }
    [[nodiscard]] bool same_owner(const PreparedRelationalBinaryOperation& other) const noexcept {
        return state_ == other.state_;
    }

    // Stable-snapshot provider: (input factor ordinal, word index) -> packed word.
    // Declared factor order/grade/names are retained, never sorted by the executor.
    // All loads, including unused loads, occur once in word/instruction order.
    // Successful output order (including duplicate registers) is op.registers.
    // The span exists only during the synchronous sink call. Sink effects and
    // provider effects are NOT rolled back if an exception or cancellation occurs.
    template<class Provider, class Sink>
    RelationalBinaryReceipt for_each_word(std::uint64_t input_type_key, std::size_t factor_count,
            std::size_t slots, Provider&& provider, Sink&& sink,
            composite::BinaryExecutionWorkspace& workspace, RelationalBinaryRequest request = {}) const {
        const auto pin = state_;
        auto receipt = admit(*pin, input_type_key, factor_count, slots, request, false);
        run(*pin, receipt, provider, sink, workspace, request);
        return receipt;
    }

    // The materialized result is owning and published only on successful return.
    // Budgets and cancellation are checked before output allocation. One workspace
    // per concurrent/reentrant execution; never share a workspace between threads.
    template<class Provider>
    [[nodiscard]] RelationalBinaryResult materialize(std::uint64_t input_type_key,
            std::size_t factor_count, std::size_t slots, Provider&& provider,
            composite::BinaryExecutionWorkspace& workspace, RelationalBinaryRequest request = {}) const {
        const auto pin = state_;
        RelationalBinaryResult result;
        result.receipt = admit(*pin, input_type_key, factor_count, slots, request, true);
        result.outputs.reserve(pin->plan.output_count());
        for (std::size_t i = 0; i < pin->plan.output_count(); ++i)
            result.outputs.emplace_back(slots, false, request.max_output_words);
        auto sink = [&](std::size_t word, std::span<const word_type> values) {
            for (std::size_t i = 0; i < values.size(); ++i)
                result.outputs[i].assign_word(word, values[i]);
        };
        run(*pin, result.receipt, provider, sink, workspace, request);
        return result;
    }

    [[nodiscard]] RelationalBinaryResult materialize(std::uint64_t input_type_key,
            std::size_t slots, std::span<const machine::PackedBits> factors,
            composite::BinaryExecutionWorkspace& workspace, RelationalBinaryRequest request = {}) const {
        // Validate unused input columns too, even for an empty domain.
        for (const auto& column : factors)
            if (column.size() != slots) throw std::invalid_argument("prepared Binary input domain mismatch");
        return materialize(input_type_key, factors.size(), slots,
            [&](composite::AspectId a, std::size_t w) { return factors[a].word(w); }, workspace, request);
    }

    [[nodiscard]] RelationalBinaryValueResult apply(const RelationalCodeValue& value,
            composite::BinaryExecutionWorkspace& workspace, RelationalBinaryRequest request = {}) const {
        const auto pin = state_;
        check_stop(request);
        pin->algebra.validate_value(pin->op().input, value);
        const auto& factors = std::get<RelationalCodeValue::Product>(value.data);
        // No caller callback can replace the plan on this concrete value path.
        auto packed = materialize(pin->op().input, factors.size(), 1,
            [&](composite::AspectId a, std::size_t) { return std::get<std::uint64_t>(factors[a].data); },
            workspace, request);
        RelationalCodeValue::Product outputs;
        outputs.reserve(packed.outputs.size());
        for (const auto& bits : packed.outputs)
            outputs.push_back(RelationalCodeValue{std::uint64_t(bits.get(0))});
        RelationalCodeValue out = pin->out().kind == RelationalCodeTypeKind::Product
            ? RelationalCodeValue{std::move(outputs)} : std::move(outputs.front());
        pin->algebra.validate_value(pin->op().output, out);
        check_stop(request);
        return {std::move(out), packed.receipt};
    }
private:
    static void bound(std::size_t n, std::size_t limit, const char* message) {
        if (n > limit) throw std::length_error(message);
    }
    static void check_stop(const RelationalBinaryRequest& r) {
        if (r.stop.stop_requested()) throw RelationalBinaryCancelled();
    }
    static RelationalCodeState snapshot(const RelationalCodeState& source, std::uint64_t key,
                                        const RelationalBinaryPreparationLimits& limits) {
        using relational_code_detail::require;
        require(source.namespace_key != 0, "prepared Binary requires code namespace");
        bound(source.types.size(), limits.max_types, "prepared Binary type index budget");
        bound(source.operations.size(), limits.max_operations, "prepared Binary operation index budget");
        const auto types = relational_code_detail::index(source.types);
        const auto ops = relational_code_detail::index(source.operations);
        const auto& op = *ops.at(key);
        require(op.opcode == RelationalCodeOpcode::Binary && op.binary.has_value(),
                "prepared Binary requires a Binary operation");
        require(!op.kernel && op.word.empty() && op.legs.empty(), "prepared Binary has foreign operands");
        bound(op.binary->instruction_count(), limits.binary.max_instructions, "prepared Binary instruction budget");
        bound(op.registers.size(), limits.binary.max_outputs, "prepared Binary output budget");
        require(!op.registers.empty(), "prepared Binary requires a nonempty output selection");
        RelationalCodeState result; result.namespace_key = source.namespace_key;
        std::set<std::uint64_t> visited;
        std::size_t items = 0, names = 0;
        // Binary signatures admit a product of Boolean leaves, not nested products.
        const auto leaf = [&](std::uint64_t id) {
            const auto& t = *types.at(id);
            require(relational_code_detail::boolean(t) && t.factors.empty(), "prepared Binary factor is not Boolean");
            if (visited.insert(id).second) {
                items = machine::checked_size_add(items, 2);
                bound(items, limits.max_type_items, "prepared Binary type item budget");
                result.types.push_back(t);
            }
        };
        const auto root = [&](std::uint64_t id, bool input) {
            const auto& t = *types.at(id);
            if (!input && relational_code_detail::boolean(t)) { leaf(id); return; }
            require(t.kind == RelationalCodeTypeKind::Product && t.atoms.empty(), "prepared Binary signature requires product");
            if (!visited.insert(id).second) return;
            items = machine::checked_size_add(items, t.factors.size());
            bound(items, limits.max_type_items, "prepared Binary type item budget");
            for (const auto& f : t.factors) {
                names = machine::checked_size_add(names, f.source_aspect.size());
                bound(names, limits.max_aspect_name_bytes, "prepared Binary aspect-name budget");
                leaf(f.type);
            }
            result.types.push_back(t);
        };
        root(op.input, true); root(op.output, false);
        result.operations.push_back(op);
        return result;
    }
    struct State {
        RelationalCodeState codes;
        RelationalCodeAlgebra algebra;
        std::uint64_t key;
        RelationalBinaryPreparationLimits limits;
        composite::BinaryExecutionPlan plan;
        State(RelationalCodeState c, std::uint64_t k, RelationalBinaryPreparationLimits l)
            : codes(std::move(c)), algebra(codes), key(k), limits(l),
              plan(*algebra.operation(k).binary, algebra.operation(k).registers, l.binary) {
            plan.validate_aspects(in().factors.size());
        }
        const RelationalCodeOperation& op() const { return algebra.operation(key); }
        const RelationalCodeType& in() const { return algebra.type(op().input); }
        const RelationalCodeType& out() const { return algebra.type(op().output); }
    };
    static RelationalBinaryReceipt admit(const State& s, std::uint64_t input_type_key,
            std::size_t factors, std::size_t slots, const RelationalBinaryRequest& r, bool materialize) {
        check_stop(r);
        if (input_type_key != s.op().input || factors != s.in().factors.size())
            throw std::invalid_argument("prepared Binary input type/factor mismatch");
        const auto words = machine::PackedBits::words_for_size(slots);
        const auto outputs = machine::checked_size_mul(words, s.plan.output_count());
        const auto instructions = machine::checked_size_mul(words, s.plan.instruction_count());
        const auto work = machine::checked_size_add(1, machine::checked_size_add(instructions, outputs));
        bound(work, r.max_work, "prepared Binary execution work budget");
        if (materialize) {
            bound(outputs, r.max_output_words, "prepared Binary request output budget");
            bound(outputs, s.limits.binary.max_output_words, "prepared Binary plan output budget");
        }
        return {s.codes.namespace_key, s.key, s.op().input, s.op().output, slots, 0, 0, 0, 0, work};
    }
    template<class Provider, class Sink>
    static void run(const State& s, RelationalBinaryReceipt& receipt, Provider& provider, Sink& sink,
                    composite::BinaryExecutionWorkspace& workspace, const RelationalBinaryRequest& r) {
        auto read = [&](composite::AspectId a, std::size_t w) {
            check_stop(r);
            const word_type value = provider(a, w);
            ++receipt.aspect_reads;
            return value;
        };
        auto emit = [&](std::size_t w, std::span<const word_type> values) {
            // Poll even when the program contains no aspect loads. Cancellation
            // latency is at most one bounded word program, excluding callbacks.
            check_stop(r);
            sink(w, values);
            ++receipt.words;
            receipt.instructions += s.plan.instruction_count();
            receipt.output_words += values.size();
            check_stop(r);
        };
        s.plan.for_each_word(receipt.slots, read, emit, workspace);
        check_stop(r);
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::storage
