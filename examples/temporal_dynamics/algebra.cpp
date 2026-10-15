#include "common.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/field/prepared_metric_novelty.hpp"
#include "rfc/numeric/algebraic_phase.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/temporal/chronological_prefix.hpp"
#include "rfc/temporal/factor_attribution.hpp"
#include "rfc/temporal/operation_word.hpp"
#include <map>

namespace temporal_demo {
namespace {
using namespace rfc;
void variation(const Frame &frame, bool duration) {
    std::vector<const Event *> samples;
    std::map<std::uint64_t, std::size_t> groups;
    for (const auto &e : frame.events)
        if (e.subtype != 7) {
            samples.push_back(&e);
            if (!groups.contains(e.category))
                groups.emplace(e.category, groups.size());
        }
    const auto n = samples.size();
    M constant(n, 1), group(n, groups.size()), metric(n, n);
    V x(n);
    for (std::size_t i = 0; i < n; ++i) {
        constant.set(i, 0, 1);
        group.set(i, groups.at(samples[i]->category), 1);
        x[i] = samples[i]->forcing;
        metric.set(i, i, duration ? Q{samples[i]->end - samples[i]->start} : Q{1});
    }
    const auto mean = field::PreparedMetricNovelty(constant, metric).decompose({0}, x);
    const auto partition = field::PreparedMetricNovelty(group, metric).decompose(V(groups.size()), x);
    const auto between = numeric::sub(partition.persistent, mean.persistent), within = partition.complement;
    const auto centered = numeric::sub(x, mean.persistent);
    const Q ss_between = numeric::dot(between, metric.apply(between)),
            ss_within = numeric::dot(within, metric.apply(within)),
            ss_total = numeric::dot(centered, metric.apply(centered));
    require(ss_between + ss_within == ss_total && numeric::dot(between, metric.apply(within)) == 0,
            "declared group decomposition failed");
    emit({{"chapter", quote("variation")},
          {"sequence", std::to_string(frame.sequence)},
          {"metric", quote(duration ? "duration" : "unit")},
          {"observations", vector(x)},
          {"group_frame", matrix(group)},
          {"global_mean", quote(mean.persistent.front().get_str())},
          {"group_mean_field", vector(partition.persistent)},
          {"between", vector(between)},
          {"within", vector(within)},
          {"between_quadrance", quote(ss_between.get_str())},
          {"within_quadrance", quote(ss_within.get_str())},
          {"total_quadrance", quote(ss_total.get_str())},
          {"cross_pairing", quote("0")},
          {"statistical_test", "null"}});
}
void ranking_cases() {
    core::RelationSignature signature;
    const auto edge = signature.add_type("reversible-conductance", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{101}, edge, {{10, true, true, 1}, {20, true, false, 1}});
    b.add_grade_one(core::RelationKey{102}, edge, {{20, true, true, 1}, {30, true, false, 1}});
    const auto source = b.freeze();
    const M transition = M::from_rows({{0, Q{1, 3}, 0}, {1, 0, 1}, {0, Q{2, 3}, 0}});
    const std::vector<field::ExactMetric> metrics{field::ExactMetric::diagonal({1, Q{1, 3}, Q{1, 2}}),
                                                  field::ExactMetric::diagonal({1, Q{1, 2}})};
    const field::ExactHodgeOperator hodge(source, core::Grade{0}, metrics);
    const auto l = hodge.materialize();
    require(l.entries() == M::identity(3).add(transition.scale(-1)).entries(),
            "reversible PageRank metric identity differs");
    const numeric::PreparedExactSolve markov(M::identity(3).add(transition.scale(Q{-1, 2}))),
        green(M::identity(3).add(l));
    const auto rank = markov.apply({Q{1, 2}, 0, 0}), response = green.apply({1, 0, 0});
    require(rank == response && rank == V{Q{5, 9}, Q{1, 3}, Q{1, 9}},
            "PageRank and its declared Hodge realization differ");
    require(numeric::add(response, hodge.apply(response)) == V{1, 0, 0},
            "PageRank Hodge action residual differs");
    emit({{"chapter", quote("pagerank")},
          {"scope", quote("canonical reversible reference case")},
          {"transition", matrix(transition)},
          {"hodge_operator", matrix(l)},
          {"object_metric", matrix(metrics[0].materialize())},
          {"relation_metric", matrix(metrics[1].materialize())},
          {"rank", vector(rank)},
          {"green_response", vector(response)},
          {"alpha", quote("1/2")},
          {"lambda", quote("1")},
          {"exact_equal", "true"}});
    core::RelationSignature document_signature;
    const auto sentence =
        document_signature.add_type("declared-sentence", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder document(document_signature);
    const std::vector<std::vector<std::uint64_t>> terms{{1, 2}, {1, 3, 4, 5}, {1, 6, 7, 8, 9, 10, 11, 12}};
    for (std::size_t j = 0; j < terms.size(); ++j) {
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for (const auto word : terms[j])
            occurrences.push_back({word, true, false, 1});
        document.add_grade_one(core::RelationKey{201 + j}, sentence, occurrences);
    }
    const auto sentences = document.freeze();
    const auto &arena = sentences.grade(core::Grade{1});
    M participation(sentences.grade(core::Grade{0}).cell_count(), 3);
    for (std::size_t p = 0; p < arena.incidence_count(); ++p)
        if (arena.aspects.get(sentences.signature().existence_aspect(), p))
            participation.set(arena.lower_index[p], arena.upper_index[p], 1);
    auto overlap = participation.transpose().multiply(participation);
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            overlap.set(i, j, i == j ? Q{0} : Q{overlap.get(i, j) / Q{std::to_string(i + j + 2)}});
    require(overlap == M::from_rows({{0, Q{1, 3}, Q{1, 4}}, {Q{1, 3}, 0, Q{1, 5}}, {Q{1, 4}, Q{1, 5}, 0}}),
            "declared sentence participation differs");
    M t(3, 3);
    for (std::size_t j = 0; j < 3; ++j) {
        Q degree{};
        for (std::size_t i = 0; i < 3; ++i)
            degree += overlap.get(i, j);
        for (std::size_t i = 0; i < 3; ++i)
            t.set(i, j, Q{overlap.get(i, j) / degree});
    }
    const numeric::PreparedExactSolve solve(M::identity(3).add(t.scale(Q{-1, 2})));
    const auto tr = solve.apply({Q{1, 6}, Q{1, 6}, Q{1, 6}});
    require(tr == V{rational("3367/9432"), rational("3176/9432"), rational("2889/9432")},
            "exact common-base TextRank case differs");
    emit({{"chapter", quote("textrank")},
          {"scope", quote("canonical lengths 2, 4, 8 reference case")},
          {"lengths", numbers({2, 4, 8})},
          {"length_exponents", numbers({1, 2, 3})},
          {"retained_sentence_terms", matrix(participation)},
          {"overlap_normalization", matrix(overlap)},
          {"transition", matrix(t)},
          {"rank", vector(tr)},
          {"raw_overlap_rank", vector({Q{1, 3}, Q{1, 3}, Q{1, 3}})},
          {"published_normalization_exact", "true"},
          {"general_unequal_length_approximation", "false"}});
}
} // namespace

void algebra_demo(const std::vector<Frame> &frames) {
    using namespace rfc;
    const auto contact = M::from_rows({{0, 0, 0}, {1, 1, 0}, {0, 0, 1}}),
               repair = M::from_rows({{1, 0, 0}, {0, 0, 0}, {0, 1, 1}});
    for (const auto &frame : frames) {
        variation(frame, false);
        variation(frame, true);
        std::vector<M> maps;
        std::vector<V> injections;
        if (frame.sequence >= 1) {
            maps.push_back(contact);
            injections.push_back(V(3));
        }
        if (frame.sequence >= 3) {
            maps.push_back(repair);
            injections.push_back(V(3));
        }
        const auto prefix = temporal::ChronologicalPrefix::capture({1, 0, 0}, maps, injections, maps.size());
        const auto value = prefix.terminal();
        emit({{"chapter", quote("chronology")},
              {"sequence", std::to_string(frame.sequence)},
              {"assets", strings({"pump", "valve", "controller"})},
              {"state", vector(value)},
              {"accepted_contacts",
               std::to_string((frame.sequence >= 1 ? 1 : 0) + (frame.sequence >= 3 ? 1 : 0))},
              {"scope", quote("declared pulse-transfer observation of the alarm and repair roles")}});
    }
    if (frames.back().sequence >= 3) {
        const temporal::OperationWord forward({contact, repair}), reverse({repair, contact});
        const V source{1, 0, 0};
        const auto x = forward.apply(source), y = reverse.apply(source);
        require(x != y, "chronological word control unexpectedly commutes");
        emit({{"chapter", quote("order_control")},
              {"available_from_sequence", "3"},
              {"forward", vector(x)},
              {"reverse_counterfactual", vector(y)},
              {"first_action", matrix(contact)},
              {"second_action", matrix(repair)},
              {"commutes", "false"}});
    }
    const auto a = M::from_rows({{1, 1}, {0, 1}}), b = M::from_rows({{1, 0}, {1, 1}});
    const auto da = M::from_rows({{0, 1}, {0, 0}}), db = M::from_rows({{0, 0}, {1, 0}});
    const temporal::OperationWord old_word({a, b}), new_word({a.add(da), b.add(db)});
    const auto defect =
        temporal::finite_word_defect(old_word, new_word, {M::identity(2), M::identity(2), M::identity(2)});
    require(defect.exact_identity(), "finite word defect identity differs");
    const auto single_a = b.multiply(da), single_b = db.multiply(a), interaction = db.multiply(da);
    require(single_a.add(single_b).add(interaction).entries() == defect.direct.entries() &&
                interaction.nonzeros() > 0,
            "finite interaction term omitted");
    emit({{"chapter", quote("finite_interaction")},
          {"scope", quote("declared two-factor reference case")},
          {"old_first", matrix(a)},
          {"old_second", matrix(b)},
          {"delta_first", matrix(da)},
          {"delta_second", matrix(db)},
          {"first_term", matrix(single_a)},
          {"second_term", matrix(single_b)},
          {"interaction", matrix(interaction)},
          {"complete_delta", matrix(defect.direct)},
          {"exact_identity", "true"}});
    const auto field =
        numeric::CertifiedAlgebraicField::declare({1, -1, 1}, numeric::IrreducibleReduction{2});
    const auto involution = numeric::AlgebraicInvolution::declare(field, field.scalar(1) - field.generator());
    const numeric::AlgebraicUnitPhase phase(involution, field.generator());
    std::vector<std::string> cycle;
    for (std::uint64_t i = 0; i <= 6; ++i) {
        const auto step = phase.power(i);
        cycle.push_back(vector(step.value().coordinates()));
    }
    const auto closing = phase.power(6);
    require(closing.value() == field.scalar(1), "declared phase cycle did not close exactly");
    emit({{"chapter", quote("phase")},
          {"scope", quote("exact periodic reference carrier")},
          {"minimal_polynomial", vector(field.minimal_polynomial())},
          {"coordinates", array(cycle)},
          {"action", matrix(phase.regular_action())},
          {"trace_pairing", matrix(involution.trace_form())},
          {"period", "6"},
          {"pi_evaluation_required", "false"},
          {"closing_identity", "true"}});
    ranking_cases();
}
} // namespace temporal_demo
