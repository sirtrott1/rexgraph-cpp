#pragma once
#include "rfc/database/native_query.hpp"
#include "rfc/database/native_role_join.hpp"
#include "rfc/storage/declaration_codec.hpp"

namespace rfc::database {
// Every descent through an optional, case or repeated value is declared. An
// unavailable ancestor is not an absent value of the selected declaration.
enum class NativeValueStepKind : std::uint8_t { Factor, Optional, Alternative, Repeated };
struct NativeValueStep {
    NativeValueStepKind kind{NativeValueStepKind::Factor};
    std::uint64_t key{};
    bool operator==(const NativeValueStep &) const = default;
};
using NativeValuePath = std::vector<NativeValueStep>;
struct CompiledNativeValuePath {
    composite::DeclarationId declaration;
    NativeValuePath steps;
    std::vector<std::size_t> positions;
    std::uint64_t assertion_kind{};
    composite::DeclarationId root;
};
class NativeValueObservation {
  public:
    composite::DeclarationId declaration() const {
        return declaration_;
    }
    const composite::DeclaredValue *value() const {
        return value_;
    }
    std::optional<std::size_t> unavailable_at() const {
        return unavailable_;
    }
    NativeValueObservation(composite::DeclarationId declaration,
                           std::shared_ptr<const composite::DeclaredValue> root,
                           const composite::DeclaredValue *value, std::optional<std::size_t> unavailable = {})
        : declaration_(declaration), root_(std::move(root)), value_(value), unavailable_(unavailable) {}

  private:
    composite::DeclarationId declaration_;
    std::shared_ptr<const composite::DeclaredValue> root_;
    const composite::DeclaredValue *value_{};
    std::optional<std::size_t> unavailable_;
};
inline CompiledNativeValuePath compile_native_value_path(const InterpretationSchema &schema,
                                                         std::uint64_t kind, NativeValuePath path) {
    using namespace composite;
    using interpretation_detail::require;
    const auto assertion = std::find_if(schema.assertions.begin(), schema.assertions.end(),
                                        [&](const auto &a) { return a.key == kind; });
    require(assertion != schema.assertions.end(), "native path assertion kind absent");
    require(path.size() <= 64, "native path depth budget");
    auto type = assertion->payload;
    std::vector<std::size_t> positions;
    for (const auto step : path) {
        const auto &d = schema.values.at(type);
        std::size_t position{};
        if (step.kind == NativeValueStepKind::Factor || step.kind == NativeValueStepKind::Alternative) {
            require(d.kind == (step.kind == NativeValueStepKind::Factor ? DeclarationKind::Product
                                                                        : DeclarationKind::Alternative),
                    "native path requires the declared product/case constructor");
            const auto f = std::find_if(d.factors.begin(), d.factors.end(),
                                        [&](const auto &x) { return x.key == step.key; });
            require(f != d.factors.end(), "native path factor/case absent");
            position = static_cast<std::size_t>(f - d.factors.begin());
            type = f->type;
        } else if (step.kind == NativeValueStepKind::Optional) {
            require(d.kind == DeclarationKind::Optional && step.key == 0,
                    "native path requires explicit optional descent");
            type = d.element;
        } else if (step.kind == NativeValueStepKind::Repeated) {
            require(d.kind == DeclarationKind::Repeated && step.key < d.max_count,
                    "native repeated path exceeds declared bounds");
            position = static_cast<std::size_t>(step.key);
            type = d.element;
        } else
            throw std::invalid_argument("unknown native value path step");
        positions.push_back(position);
    }
    return {type, std::move(path), std::move(positions), kind, assertion->payload};
}
inline NativeValueObservation observe_native_value(const InterpretedSnapshot &source, std::uint64_t key,
                                                   const CompiledNativeValuePath &path) {
    using V = composite::DeclaredValue;
    interpretation_detail::require(path.steps.size() == path.positions.size(),
                                   "compiled path positions absent");
    const auto verified = compile_native_value_path(source.schema(), path.assertion_kind, path.steps);
    interpretation_detail::require(verified.root == path.root && verified.declaration == path.declaration &&
                                       verified.positions == path.positions,
                                   "compiled path differs from the declared source carrier");
    const auto cell = source.source().cell({core::Grade{1}, key});
    interpretation_detail::require(cell.type().has_value() &&
                                       cell.source_schema()->type(*cell.type()).name ==
                                           interpretation_detail::source_type(path.assertion_kind),
                                   "value lies outside the named assertion carrier");
    const auto pin = source.payload(key);
    interpretation_detail::require(pin.declaration()->id == path.root,
                                   "value lies outside the compiled path payload carrier");
    const auto root = pin.declared_value();
    const V *value = root.get();
    for (std::size_t i = 0; i < path.steps.size(); ++i) {
        const auto step = path.steps[i];
        const auto position = path.positions[i];
        if (step.kind == NativeValueStepKind::Factor)
            value = &std::get<V::Product>(value->data).elements.at(position);
        else if (step.kind == NativeValueStepKind::Optional) {
            const auto &elements = std::get<V::Optional>(value->data).element;
            if (elements.empty())
                return {path.declaration, root, nullptr, i};
            value = &elements.front();
        } else if (step.kind == NativeValueStepKind::Alternative) {
            const auto &alternative = std::get<V::Alternative>(value->data);
            if (alternative.key != step.key)
                return {path.declaration, root, nullptr, i};
            value = &alternative.element.at(0);
        } else {
            const auto &elements = std::get<V::Repeated>(value->data).elements;
            if (position >= elements.size())
                return {path.declaration, root, nullptr, i};
            value = &elements[position];
        }
    }
    return {path.declaration, root, value};
}
enum class NativeConditionKind : std::uint8_t { Value, RoleState, RoleCount };
struct NativeCondition {
    NativeConditionKind kind{NativeConditionKind::Value};
    NativeValuePath path;
    ExactPredicateKind operation{ExactPredicateKind::Equal};
    composite::DeclaredValue literal;
    std::uint64_t role{};
    ReferenceState state{ReferenceState::Present};
    NativeRoleActivity activity{NativeRoleActivity::Retained};
};
enum class NativeExpressionKind : std::uint8_t { Condition, All, Any, Not };
struct NativeQueryExpression {
    NativeExpressionKind kind{NativeExpressionKind::All};
    NativeCondition condition;
    std::vector<NativeQueryExpression> children;
};
namespace native_algebra_detail {
using interpretation_detail::require;
using V = composite::DeclaredValue;
using D = composite::DeclarationKind;
struct ConditionPlan {
    NativeCondition request;
    std::optional<CompiledNativeValuePath> path;
    native_query_detail::Scalar scalar;
};
struct ExpressionPlan {
    NativeExpressionKind kind;
    std::optional<ConditionPlan> condition;
    std::vector<ExpressionPlan> children;
};
inline ExpressionPlan compile(const InterpretationSchema &schema, std::uint64_t kind,
                              const NativeQueryExpression &expression, std::size_t &nodes,
                              std::size_t depth = 0) {
    require(++nodes <= 256 && depth <= 32, "native boolean expression budget");
    ExpressionPlan plan{expression.kind, {}, {}};
    if (expression.kind != NativeExpressionKind::Condition) {
        require(expression.kind == NativeExpressionKind::All ||
                    expression.kind == NativeExpressionKind::Any ||
                    (expression.kind == NativeExpressionKind::Not && expression.children.size() == 1),
                "invalid native boolean constructor");
        for (const auto &child : expression.children)
            plan.children.push_back(compile(schema, kind, child, nodes, depth + 1));
        return plan;
    }
    require(expression.children.empty(), "condition has boolean children");
    ConditionPlan c{expression.condition, {}, {}};
    require(c.request.operation <= ExactPredicateKind::IsNotNull, "unknown native comparison");
    if (c.request.kind == NativeConditionKind::Value) {
        c.path = compile_native_value_path(schema, kind, c.request.path);
        const auto &selected = schema.values.at(c.path->declaration);
        if (c.request.operation == ExactPredicateKind::IsNull ||
            c.request.operation == ExactPredicateKind::IsNotNull)
            require(selected.kind == D::Optional,
                    "null predicate requires the selected optional constructor");
        else {
            const auto element = selected.kind == D::Optional ? selected.element : selected.id;
            c.scalar = {selected.id, element, schema.values.at(element).kind, selected.kind == D::Optional};
            require(c.scalar.kind == D::Integer || c.scalar.kind == D::Rational ||
                        c.scalar.kind == D::Boolean || c.scalar.kind == D::Finite,
                    "scalar predicate needs an explicit path to a scalar");
            native_query_detail::validate(schema, c.scalar, {{}, c.request.operation, c.request.literal});
        }
    } else {
        const auto a = std::find_if(schema.assertions.begin(), schema.assertions.end(),
                                    [&](const auto &x) { return x.key == kind; });
        require(a != schema.assertions.end() &&
                    std::any_of(a->roles.begin(), a->roles.end(),
                                [&](const auto &r) { return r.key == c.request.role; }),
                "predicate role absent");
        if (c.request.kind == NativeConditionKind::RoleState)
            require(c.request.state <= ReferenceState::Present &&
                        (c.request.operation == ExactPredicateKind::Equal ||
                         c.request.operation == ExactPredicateKind::NotEqual),
                    "invalid reference-state predicate");
        else {
            require(c.request.kind == NativeConditionKind::RoleCount &&
                        (c.request.activity == NativeRoleActivity::Retained ||
                         c.request.activity == NativeRoleActivity::ActiveOnly) &&
                        c.request.operation < ExactPredicateKind::IsNull &&
                        std::holds_alternative<numeric::Integer>(c.request.literal.data),
                    "invalid role-count predicate");
            c.scalar = {{}, {}, D::Integer, false};
        }
    }
    plan.condition = std::move(c);
    return plan;
}
inline std::size_t expression_work(const InterpretedSnapshot &source, std::uint64_t key,
                                   const ExpressionPlan &plan) {
    std::size_t work = 1;
    if (plan.condition) {
        const auto &c = *plan.condition;
        if (c.path)
            work = machine::checked_size_add(work, c.path->steps.size() + 1);
        else if (c.request.kind == NativeConditionKind::RoleCount)
            work = machine::checked_size_add(work, source.participants(key, c.request.role).size());
    }
    for (const auto &child : plan.children)
        work = machine::checked_size_add(work, expression_work(source, key, child));
    return work;
}
inline NativeTruth evaluate(const InterpretedSnapshot &source, std::uint64_t key,
                            const ExpressionPlan &plan) {
    if (plan.kind == NativeExpressionKind::Condition) {
        const auto &c = *plan.condition;
        if (c.request.kind == NativeConditionKind::RoleState) {
            const bool equal = source.reference_state(key, c.request.role) == c.request.state;
            return equal == (c.request.operation == ExactPredicateKind::Equal) ? NativeTruth::True
                                                                               : NativeTruth::False;
        }
        if (c.request.kind == NativeConditionKind::RoleCount) {
            const auto count =
                source.participants(key, c.request.role, c.request.activity == NativeRoleActivity::ActiveOnly)
                    .size();
            return native_query_detail::evaluate(c.scalar, {{}, c.request.operation, c.request.literal},
                                                 V::integer(numeric::Integer(std::to_string(count))));
        }
        const auto observed = observe_native_value(source, key, *c.path);
        if (!observed.value())
            return NativeTruth::Unknown;
        if (c.request.operation == ExactPredicateKind::IsNull ||
            c.request.operation == ExactPredicateKind::IsNotNull) {
            const bool absent = std::get<V::Optional>(observed.value()->data).element.empty();
            return absent == (c.request.operation == ExactPredicateKind::IsNull) ? NativeTruth::True
                                                                                 : NativeTruth::False;
        }
        return native_query_detail::evaluate(c.scalar, {{}, c.request.operation, c.request.literal},
                                             *observed.value());
    }
    if (plan.kind == NativeExpressionKind::Not) {
        const auto value = evaluate(source, key, plan.children.front());
        return value == NativeTruth::Unknown ? value
               : value == NativeTruth::True  ? NativeTruth::False
                                             : NativeTruth::True;
    }
    bool unknown{};
    for (const auto &child : plan.children) {
        const auto value = evaluate(source, key, child);
        if (plan.kind == NativeExpressionKind::All && value == NativeTruth::False)
            return NativeTruth::False;
        if (plan.kind == NativeExpressionKind::Any && value == NativeTruth::True)
            return NativeTruth::True;
        unknown |= value == NativeTruth::Unknown;
    }
    return unknown                                  ? NativeTruth::Unknown
           : plan.kind == NativeExpressionKind::All ? NativeTruth::True
                                                    : NativeTruth::False;
}
} // namespace native_algebra_detail
struct NativeAlgebraSelection {
    InterpretedSnapshot source;
    runtime::WorkAvailability availability;
    std::vector<std::uint64_t> keys, unknown_keys;
};
class PreparedNativeQueryAlgebra {
  public:
    static PreparedNativeQueryAlgebra prepare(InterpretedSnapshot source, std::uint64_t kind,
                                              NativeQueryExpression expression,
                                              runtime::WorkAvailability available) {
        runtime::admit_work_availability(source.schema().source_namespace, source.sequence(), available);
        std::size_t nodes{};
        auto compiled = native_algebra_detail::compile(source.schema(), kind, expression, nodes);
        const auto count = source.assertions(kind).size();
        interpretation_detail::bound(machine::checked_size_mul(count, nodes + 1), source.limits().max_work,
                                     "boolean selection work budget");
        return PreparedNativeQueryAlgebra(std::make_shared<const Plan>(
            Plan{std::move(source), kind, available, std::move(expression), std::move(compiled)}));
    }
    NativeAlgebraSelection execute(runtime::WorkAvailability available,
                                   const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) const {
        runtime::admit_work_availability(plan_->source.schema().source_namespace, plan_->source.sequence(),
                                         available);
        lease.check_cancelled();
        runtime::ExecutionRequirements required;
        required.records = plan_->source.assertions(plan_->kind).size();
        for (const auto key : plan_->source.assertions(plan_->kind)) {
            lease.check_cancelled();
            required.scalar_work = machine::checked_size_add(
                required.scalar_work,
                native_algebra_detail::expression_work(plan_->source, key, plan_->compiled));
            interpretation_detail::bound(required.scalar_work, plan_->source.limits().max_work,
                                         "boolean evaluation work budget");
        }
        runtime::admit_execution(required, lease.policy());
        NativeAlgebraSelection out{plan_->source, available, {}, {}};
        for (const auto key : plan_->source.assertions(plan_->kind)) {
            lease.check_cancelled();
            const auto truth = native_algebra_detail::evaluate(plan_->source, key, plan_->compiled);
            if (truth == NativeTruth::True)
                out.keys.push_back(key);
            else if (truth == NativeTruth::Unknown)
                out.unknown_keys.push_back(key);
        }
        return out;
    }

  private:
    struct Plan {
        InterpretedSnapshot source;
        std::uint64_t kind;
        runtime::WorkAvailability available;
        NativeQueryExpression request;
        native_algebra_detail::ExpressionPlan compiled;
    };
    explicit PreparedNativeQueryAlgebra(std::shared_ptr<const Plan> plan) : plan_(std::move(plan)) {}
    std::shared_ptr<const Plan> plan_;
};
enum class NativeQuerySide : std::uint8_t { Left, Right };
struct NativeValueProjection {
    NativeQuerySide side{NativeQuerySide::Left};
    NativeValuePath path;
};
enum class NativeAggregateKind : std::uint8_t { Count, Sum, Mean, Minimum, Maximum };
struct NativeAggregateRequest {
    NativeAggregateKind operation{NativeAggregateKind::Count};
    NativeValueProjection value;
    bool has_value{}, unique_assertions{};
};
struct NativeQueryRow {
    std::uint64_t left{}, right{};
    std::size_t occurrence{};
};
struct NativeAggregateValue {
    std::size_t supported{};
    std::optional<numeric::Rational> value;
};
struct NativeQueryGroup {
    std::vector<NativeValueObservation> values;
    std::vector<std::size_t> members;
    std::vector<NativeAggregateValue> aggregates;
};
struct DerivedNativeGroups {
    InterpretedSnapshot left;
    std::optional<InterpretedSnapshot> right;
    std::vector<NativeQueryRow> rows;
    std::vector<NativeQueryGroup> groups;
};
// Grouping is a derived observation over retained assertion or occurrence rows.
// Equal group values do not identify their source cells or discard membership.
inline DerivedNativeGroups
group_native_rows(const InterpretedSnapshot &left, std::uint64_t left_kind, const InterpretedSnapshot *right,
                  std::uint64_t right_kind, std::vector<NativeQueryRow> rows,
                  const std::vector<NativeValueProjection> &grouping,
                  const std::vector<NativeAggregateRequest> &aggregates, runtime::WorkAvailability available,
                  std::optional<runtime::WorkAvailability> right_available = {},
                  const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) {
    using namespace native_algebra_detail;
    runtime::admit_work_availability(left.schema().source_namespace, left.sequence(), available);
    if (right) {
        require(right_available.has_value(), "right grouping cutoff absent");
        runtime::admit_work_availability(right->schema().source_namespace, right->sequence(),
                                         *right_available);
    }
    require(grouping.size() <= 32 && aggregates.size() <= 32, "group projection budget");
    interpretation_detail::bound(rows.size(), left.limits().max_records, "group row budget");
    interpretation_detail::bound(
        machine::checked_size_mul(rows.size(), grouping.size() + aggregates.size() + 1),
        left.limits().max_work, "group work budget");
    const auto source_for = [&](NativeQuerySide side) -> const InterpretedSnapshot & {
        require(side == NativeQuerySide::Left || (side == NativeQuerySide::Right && right),
                "right projection requires a declared join");
        return side == NativeQuerySide::Left ? left : *right;
    };
    const auto compile_projection = [&](const NativeValueProjection &p) {
        return compile_native_value_path(source_for(p.side).schema(),
                                         p.side == NativeQuerySide::Left ? left_kind : right_kind, p.path);
    };
    std::vector<CompiledNativeValuePath> paths;
    std::vector<composite::CompiledLayout> layouts;
    for (const auto &p : grouping) {
        paths.push_back(compile_projection(p));
        layouts.emplace_back(source_for(p.side).schema().values, paths.back().declaration,
                             composite::BooleanLayoutProfile::Lsb0OctetsV1,
                             source_for(p.side).limits().declarations);
    }
    std::vector<std::optional<CompiledNativeValuePath>> operands;
    for (const auto &a : aggregates) {
        require(a.operation <= NativeAggregateKind::Maximum, "unknown native aggregate");
        (void)source_for(a.value.side);
        operands.push_back(a.has_value ? std::optional(compile_projection(a.value)) : std::nullopt);
        require(a.has_value || a.operation == NativeAggregateKind::Count, "numeric aggregate operand absent");
        if (a.operation != NativeAggregateKind::Count) {
            const auto &source = source_for(a.value.side);
            const auto &selected = source.schema().values.at(operands.back()->declaration);
            const auto element = selected.kind == D::Optional ? selected.element : selected.id;
            const auto type = source.schema().values.at(element).kind;
            require(type == D::Integer || type == D::Rational,
                    "numeric aggregate requires an explicit numeric scalar path");
        }
    }
    const std::set<std::uint64_t> valid_left(left.assertions(left_kind).begin(),
                                             left.assertions(left_kind).end());
    std::set<std::uint64_t> valid_right;
    if (right)
        valid_right.insert(right->assertions(right_kind).begin(), right->assertions(right_kind).end());
    for (const auto &row : rows)
        require(valid_left.contains(row.left) && (!right || valid_right.contains(row.right)),
                "group row lies outside the declared assertion carriers");
    runtime::ExecutionRequirements required;
    required.records = machine::checked_size_mul(rows.size(), grouping.size() + aggregates.size() + 1);
    required.rational_slots = machine::checked_size_mul(rows.size() + 1, aggregates.size());
    required.scalar_work = required.records;
    runtime::admit_execution(required, lease.policy());
    lease.check_cancelled();
    DerivedNativeGroups result{left, right ? std::optional(*right) : std::nullopt, std::move(rows), {}};
    std::map<std::vector<std::byte>, std::size_t> index;
    std::size_t encoded_bytes{};
    const auto append = [](std::vector<std::byte> &bytes, std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i)
            bytes.push_back(static_cast<std::byte>((value >> (8u * i)) & 255u));
    };
    if (grouping.empty())
        result.groups.push_back({{}, {}, {}});
    for (std::size_t i = 0; i < result.rows.size(); ++i) {
        lease.check_cancelled();
        const auto &row = result.rows[i];
        std::vector<std::byte> key;
        std::vector<NativeValueObservation> values;
        for (std::size_t j = 0; j < grouping.size(); ++j) {
            const auto &p = grouping[j];
            const auto &source = source_for(p.side);
            auto value = observe_native_value(source, p.side == NativeQuerySide::Left ? row.left : row.right,
                                              paths[j]);
            const auto id = value.declaration();
            append(key, id.name_space);
            append(key, id.key);
            append(key, id.version);
            append(key, value.unavailable_at() ? static_cast<std::uint64_t>(*value.unavailable_at()) + 1 : 0);
            if (value.value()) {
                const auto bytes = storage::encode_declared_bits(layouts[j], *value.value());
                append(key, bytes.bit_count);
                append(key, bytes.bytes.size());
                key.insert(key.end(), bytes.bytes.begin(), bytes.bytes.end());
            } else {
                append(key, 0);
                append(key, 0);
            }
            values.push_back(std::move(value));
        }
        std::size_t group{};
        if (!grouping.empty()) {
            const auto existing = index.find(key);
            if (existing == index.end()) {
                encoded_bytes = machine::checked_size_add(encoded_bytes, key.size());
                interpretation_detail::bound(encoded_bytes, left.limits().max_metadata_bytes,
                                             "group key byte budget");
                group = result.groups.size();
                index.emplace(std::move(key), group);
                result.groups.push_back({std::move(values), {}, {}});
            } else
                group = existing->second;
        }
        result.groups[group].members.push_back(i);
    }
    for (auto &group : result.groups)
        for (std::size_t j = 0; j < aggregates.size(); ++j) {
            const auto &a = aggregates[j];
            const auto &source = source_for(a.value.side);
            NativeAggregateValue output;
            std::set<std::uint64_t> seen;
            for (const auto member : group.members) {
                lease.check_cancelled();
                const auto &row = result.rows[member];
                const auto key = a.value.side == NativeQuerySide::Left ? row.left : row.right;
                if (a.unique_assertions && !seen.insert(key).second)
                    continue;
                const V *value{};
                std::optional<NativeValueObservation> observed;
                if (operands[j]) {
                    observed = observe_native_value(source, key, *operands[j]);
                    value = observed->value();
                    if (!value)
                        continue;
                    if (const auto *optional = std::get_if<V::Optional>(&value->data)) {
                        if (optional->element.empty())
                            continue;
                        value = &optional->element.front();
                    }
                }
                ++output.supported;
                if (a.operation == NativeAggregateKind::Count)
                    continue;
                const numeric::Rational q = std::holds_alternative<numeric::Integer>(value->data)
                                                ? numeric::Rational(std::get<numeric::Integer>(value->data))
                                                : std::get<numeric::Rational>(value->data);
                if (!output.value)
                    output.value = q;
                else if (a.operation == NativeAggregateKind::Minimum) {
                    if (q < *output.value)
                        output.value = q;
                } else if (a.operation == NativeAggregateKind::Maximum) {
                    if (q > *output.value)
                        output.value = q;
                } else
                    *output.value += q;
                interpretation_detail::bound(
                    composite::declaration_detail::integer_bits(output.value->get_num()),
                    source.limits().declarations.max_integer_bits, "aggregate intermediate numerator budget");
                interpretation_detail::bound(
                    composite::declaration_detail::integer_bits(output.value->get_den()),
                    source.limits().declarations.max_integer_bits,
                    "aggregate intermediate denominator budget");
            }
            if (a.operation == NativeAggregateKind::Count)
                output.value = numeric::Rational(numeric::Integer(std::to_string(output.supported)));
            else if (a.operation == NativeAggregateKind::Mean && output.value)
                *output.value /= numeric::Integer(std::to_string(output.supported));
            if (output.value) {
                output.value->canonicalize();
                interpretation_detail::bound(
                    composite::declaration_detail::integer_bits(output.value->get_num()),
                    left.limits().declarations.max_integer_bits, "aggregate numerator budget");
                interpretation_detail::bound(
                    composite::declaration_detail::integer_bits(output.value->get_den()),
                    left.limits().declarations.max_integer_bits, "aggregate denominator budget");
            }
            group.aggregates.push_back(std::move(output));
        }
    return result;
}
} // namespace rfc::database
