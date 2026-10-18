#pragma once

#include "rfc/inference/relational_known_tensor_graph.hpp"
#include "rfc/learning/relational_field_family.hpp"
#include <variant>

namespace rfc::learning {
using RelationalLiteralFamilyInput = std::variant<inference::OwnedRelationalLiteralInput,
    inference::RelationalKnownTensorGraphInput>;
class RelationalLiteralJointEvaluation;

// Keep the byte predicates and any known-input graph beside the complete joint
// fields. The existing family owns the equations and performs their execution.
class RelationalLiteralJointResult {
    struct State {
        std::vector<RelationalLiteralFamilyInput> inputs;
        RelationalJointFieldResult result;
    };
public:
    RelationalLiteralJointResult(PreparedRelationalFieldFamily family, std::size_t member,
        std::vector<RelationalLiteralFamilyInput> inputs, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) {
        using namespace relational_evidence_detail;
        Guard guard(limits,std::move(check)); same_limits(family.limits(),limits);
        const auto program = family.members().at(member).program();
        guard.dimension(inputs.size()); std::vector<inference::RelationalAccessionInput> supplied;
        for (const auto& input : inputs) {
            guard.step();
            const auto actual = std::visit([&](const auto& value) -> inference::RelationalAccessionInput {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value,inference::RelationalKnownTensorGraphInput>) {
                    const auto graph = value.graph(); same_limits(graph.limits(),limits);
                    require(graph.literal_parent().has_value(),"literal joint input requires its actual literal parent");
                }
                return value.input();
            },input);
            require(actual.accession().program().same_owner(program),"literal joint input requires the actual member program");
            supplied.push_back(actual);
        }
        auto original = inference::execute_relational_inputs(program,std::move(supplied),limits,[&]{guard.step();});
        auto result = family.execute(member,std::move(original),limits,[&]{guard.step();});
        state_ = std::make_shared<const State>(State{std::move(inputs),std::move(result)});
    }
    RelationalJointFieldResult raw_result() const { return state_->result; }
    PreparedRelationalFieldFamily family() const { return state_->result.family(); }
    std::size_t member() const noexcept { return state_->result.member(); }
    const std::vector<RelationalLiteralFamilyInput>& inputs() const & { return state_->inputs; }
    const std::vector<RelationalLiteralFamilyInput>& inputs() const && = delete;
    const RelationalAffineField& field(std::uint64_t node) const & { return state_->result.field(node); }
    const RelationalAffineField& field(std::uint64_t) const && = delete;
    const RelationalAffineField& occurrence_field(std::uint64_t node) const & { return state_->result.occurrence_field(node); }
    const RelationalAffineField& occurrence_field(std::uint64_t) const && = delete;
    bool identified(std::uint64_t node,std::size_t coordinate) const { return state_->result.identified(node,coordinate); }
    RelationalLiteralJointEvaluation evaluate(numeric::RationalVector free,numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class RelationalLiteralJointEvaluation {
public:
    RelationalLiteralJointResult parent() const { return parent_; }
    RelationalJointFieldEvaluation raw_evaluation() const { return evaluation_; }
    inference::OwnedAccessionFieldResult result() const { return evaluation_.result(); }
    const numeric::RationalVector& free_values() const & { return evaluation_.free_values(); }
    const numeric::RationalVector& free_values() const && = delete;
private:
    friend class RelationalLiteralJointResult;
    RelationalLiteralJointEvaluation(RelationalLiteralJointResult parent,RelationalJointFieldEvaluation evaluation)
        : parent_(std::move(parent)),evaluation_(std::move(evaluation)) {}
    RelationalLiteralJointResult parent_;
    RelationalJointFieldEvaluation evaluation_;
};
inline RelationalLiteralJointEvaluation RelationalLiteralJointResult::evaluate(numeric::RationalVector free,
    numeric::ExactActionLimits limits,std::function<void()> check) const {
    const auto pin = *this; return {pin,pin.raw_result().evaluate(std::move(free),limits,std::move(check))};
}
} // namespace rfc::learning
