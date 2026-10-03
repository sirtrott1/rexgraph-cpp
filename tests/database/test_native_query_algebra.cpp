#include "interpretation_fixture.hpp"
#include "rfc/database/cross_native_role_join.hpp"
#include "rfc/database/native_query_algebra.hpp"
#include <iostream>

using namespace rfc;
using namespace rfc::database;
using V = composite::DeclaredValue;
using Q = numeric::Rational;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "invalid algebra operation accepted");
}
runtime::WorkAvailability available(const InterpretedSnapshot &source) {
    return {source.schema().source_namespace, source.sequence()};
}
NativeQueryExpression atom(NativeValuePath path, ExactPredicateKind operation, V literal = {}) {
    NativeQueryExpression expression;
    expression.kind = NativeExpressionKind::Condition;
    expression.condition.path = std::move(path);
    expression.condition.operation = operation;
    expression.condition.literal = std::move(literal);
    return expression;
}
int main() {
    try {
        const auto source = InterpretedSnapshot::interpret(f5_fixture::schema(), f5_fixture::snapshot());
        const auto bytes = encode_interpreted_snapshot(source);
        const NativeValuePath outer{{NativeValueStepKind::Factor, 1}},
            inner{{NativeValueStepKind::Factor, 1}, {NativeValueStepKind::Optional, 0}};
        const auto positive = atom(inner, ExactPredicateKind::Greater, V::rational(Q{0}));
        const auto select = [&](NativeQueryExpression e) {
            return PreparedNativeQueryAlgebra::prepare(source, 10, std::move(e), available(source))
                .execute(available(source));
        };
        auto result = select(positive);
        check(result.keys == std::vector<std::uint64_t>{1, 2} &&
                  result.unknown_keys == std::vector<std::uint64_t>{3, 4},
              "optional ancestor truth changed");
        check(select(atom(outer, ExactPredicateKind::IsNull)).keys == std::vector<std::uint64_t>{3},
              "outer absence flattened");
        check(select(atom(inner, ExactPredicateKind::IsNull)).keys == std::vector<std::uint64_t>{4},
              "inner absence flattened");
        NativeQueryExpression any{
            NativeExpressionKind::Any, {}, {positive, atom(outer, ExactPredicateKind::IsNull)}};
        check(select(any).keys == std::vector<std::uint64_t>{1, 2, 3} &&
                  select(any).unknown_keys == std::vector<std::uint64_t>{4},
              "three-valued disjunction");
        check(select({NativeExpressionKind::Not, {}, {positive}}).unknown_keys ==
                  std::vector<std::uint64_t>{3, 4},
              "NOT changed unknown to false");
        auto role = atom({}, ExactPredicateKind::Equal);
        role.condition.kind = NativeConditionKind::RoleState;
        role.condition.role = 11;
        role.condition.state = ReferenceState::Null;
        check(select(role).keys == std::vector<std::uint64_t>{2},
              "null role confused with missing/present-empty");
        role.condition.kind = NativeConditionKind::RoleCount;
        role.condition.literal = V::integer(2);
        role.condition.activity = NativeRoleActivity::Retained;
        check(select(role).keys == std::vector<std::uint64_t>{1}, "retained role count omitted inactive");
        role.condition.activity = NativeRoleActivity::ActiveOnly;
        check(select(role).keys.empty(), "active role count retained inactive");
        rejects([&] {
            (void)select(atom({{NativeValueStepKind::Factor, 2}}, ExactPredicateKind::Greater, V::symbol(7)));
        });
        rejects(
            [&] { (void)select(atom({{NativeValueStepKind::Optional, 0}}, ExactPredicateKind::IsNull)); });
        auto invalid = positive;
        invalid.condition.path = {{NativeValueStepKind::Factor, 99}};
        rejects([&] {
            (void)select({NativeExpressionKind::Any, {}, {{NativeExpressionKind::All, {}, {}}, invalid}});
        });
        std::vector<NativeQueryRow> rows{{1, 0, 0}, {2, 0, 1}, {3, 0, 2}, {4, 0, 3}};
        NativeAggregateRequest sum{NativeAggregateKind::Sum, {NativeQuerySide::Left, inner}, true, false};
        auto grouped = group_native_rows(source, 10, nullptr, 0, rows, {{NativeQuerySide::Left, outer}},
                                         {sum}, available(source));
        check(grouped.groups.size() == 3 && grouped.groups[0].members.size() == 2 &&
                  grouped.groups[0].aggregates[0].value == Q{10},
              "grouping collapsed optional layers or identities");
        check(!grouped.groups[1].aggregates[0].value && !grouped.groups[2].aggregates[0].value,
              "absent numeric groups became zero");
        auto mean = sum;
        mean.operation = NativeAggregateKind::Mean;
        check(group_native_rows(source, 10, nullptr, 0, rows, {}, {mean}, available(source))
                      .groups[0]
                      .aggregates[0]
                      .value == Q{5},
              "mean included absent payload");
        auto count = NativeAggregateRequest{};
        check(group_native_rows(source, 10, nullptr, 0, {}, {}, {count, sum}, available(source))
                      .groups[0]
                      .aggregates[0]
                      .value == Q{0},
              "empty count identity");
        const auto joined = PreparedNativeRoleJoin::prepare(source, {10, 11, 10, 12}, available(source))
                                .execute(available(source));
        rows.clear();
        for (std::size_t i = 0; i < joined.matches().size(); ++i) {
            const auto &m = joined.matches()[i];
            rows.push_back({m.left_assertion, m.right_assertion, i});
        }
        sum.value.side = NativeQuerySide::Right;
        auto unique = sum;
        unique.unique_assertions = true;
        const auto totals = group_native_rows(source, 10, &source, 10, rows, {}, {sum, unique},
                                              available(source), available(source));
        check(totals.groups[0].aggregates[0].value == Q{10} && totals.groups[0].aggregates[1].value == Q{5},
              "joined aggregate inferred identity from equal values");
        auto foreign_schema = f5_fixture::schema();
        foreign_schema.source_namespace = 901;
        foreign_schema.binding_namespace = 902;
        const auto foreign = InterpretedSnapshot::interpret(foreign_schema, f5_fixture::snapshot());
        NativeParticipantCorrespondence correspondence{
            501, 0, 901, 0, {{1, {core::Grade{0}, 1}, {core::Grade{0}, 1}}}};
        auto cross = PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                          available(source), available(foreign))
                         .execute(available(source), available(foreign));
        check(cross.matches.size() == 2 && cross.unmapped_left_occurrences == 1 &&
                  cross.matches[0].channel == 1 && cross.matches[1].left_position == 1,
              "cross-source composition lost channels");
        correspondence.links.push_back({2, {core::Grade{0}, 1}, {core::Grade{0}, 1}});
        check(PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                   available(source), available(foreign))
                      .execute(available(source), available(foreign))
                      .matches.size() == 4,
              "duplicate mapped values merged distinct channels");
        correspondence.left_sequence = 1;
        rejects([&] {
            (void)PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                       available(source), available(foreign));
        });
        correspondence.left_sequence = 0;
        correspondence.links[0].left.key = 4;
        rejects([&] {
            (void)PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                       available(source), available(foreign));
        });
        auto policy = runtime::ExecutionHostPolicy{};
        policy.limits.records = 0;
        rejects([&] {
            (void)group_native_rows(source, 10, nullptr, 0, {{1, 0, 0}}, {}, {count}, available(source), {},
                                    runtime::ExecutionLease(policy));
        });
        const auto cancellation = std::make_shared<std::atomic_bool>(true);
        const runtime::ExecutionLease cancelled({}, cancellation);
        auto empty_logical = f5_fixture::snapshot();
        empty_logical.assertions.clear();
        const auto empty_source = InterpretedSnapshot::interpret(f5_fixture::schema(), empty_logical);
        const auto empty_join = PreparedNativeCrossRoleJoin::prepare(
            empty_source, foreign, {10, 11, 10, 12}, {501, 0, 901, 0, {}},
            available(empty_source), available(foreign));
        rejects([&] { (void)empty_join.execute(available(empty_source), available(foreign), cancelled); });
        const auto empty_foreign = InterpretedSnapshot::interpret(foreign_schema, empty_logical);
        const auto unmatched_join = PreparedNativeCrossRoleJoin::prepare(
            source, empty_foreign, {10, 11, 10, 12},
            {501, 0, 901, 0, {{1, {core::Grade{0}, 1}, {core::Grade{0}, 1}}}},
            available(source), available(empty_foreign));
        auto lookup_policy = runtime::ExecutionHostPolicy{};
        lookup_policy.limits.scalar_work = 16;
        rejects([&] { (void)unmatched_join.execute(available(source), available(empty_foreign),
                                                  runtime::ExecutionLease(lookup_policy)); });
        rejects([&] {
            (void)PreparedNativeQueryAlgebra::prepare(source, 10, positive, available(source))
                .execute(available(source), cancelled);
        });
        rejects([&] {
            (void)group_native_rows(source, 10, nullptr, 0, {{1, 0, 0}}, {}, {count}, available(source), {},
                                    cancelled);
        });
        correspondence.links = {{1, {core::Grade{0}, 1}, {core::Grade{0}, 1}}};
        rejects([&] {
            (void)PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                       available(source), available(foreign), cancelled);
        });
        rejects([&] {
            (void)PreparedNativeCrossRoleJoin::prepare(source, foreign, {10, 11, 10, 12}, correspondence,
                                                       available(source), available(foreign),
                                                       runtime::ExecutionLease(policy));
        });
        auto broken = compile_native_value_path(source.schema(), 10, inner);
        broken.positions.clear();
        rejects([&] { (void)observe_native_value(source, 1, broken); });
        const NativeValuePath repeated{{NativeValueStepKind::Factor, 3}, {NativeValueStepKind::Repeated, 1}};
        check(select(atom(repeated, ExactPredicateKind::Equal, V::rational(Q{0}))).keys.size() == 4,
              "repeated scalar descent changed supported zero");
        auto cancelling = source.export_snapshot();
        cancelling.revision = "cancellation";
        cancelling.assertions[0].payload = f5_fixture::payload(2, Q{5});
        cancelling.assertions[1].payload = f5_fixture::payload(2, Q{-5});
        const auto cancelling_source = source.replace(cancelling);
        auto cancellation_sum = sum;
        cancellation_sum.value.side = NativeQuerySide::Left;
        const auto cancelled_total =
            group_native_rows(cancelling_source, 10, nullptr, 0, {{1, 0, 0}, {2, 0, 1}}, {},
                              {cancellation_sum}, available(cancelling_source));
        check(cancelled_total.groups[0].aggregates[0].supported == 2 &&
                  cancelled_total.groups[0].aggregates[0].value == Q{0},
              "cancellation lost retained support");
        auto dead_logical = source.export_snapshot();
        dead_logical.revision = "deleted-subject";
        dead_logical.subjects.erase(dead_logical.subjects.begin() + 2);
        const auto dead_source = source.replace(dead_logical);
        correspondence.left_sequence = dead_source.sequence();
        correspondence.links[0].left.key = 3;
        rejects([&] {
            (void)PreparedNativeCrossRoleJoin::prepare(dead_source, foreign, {10, 11, 10, 12}, correspondence,
                                                       available(dead_source), available(foreign));
        });
        auto future = source.export_snapshot();
        future.revision = "future";
        const auto next = source.replace(future);
        rejects([&] { (void)PreparedNativeQueryAlgebra::prepare(next, 10, positive, available(source)); });
        check(encode_interpreted_snapshot(source) == bytes, "derived query rewrote native source");
        std::cout << "PASS native boolean/path/group/cross-source query algebra\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
