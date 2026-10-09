#pragma once
#include "rfc/composite/boundary.hpp"
#include "rfc/machine/checked_size.hpp"
#include <limits>
#include "rfc/core/occurrence.hpp"
#include "rfc/core/relational_complex.hpp"
#include "rfc/core/axioms.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rfc::storage { class MappedRfcx; }

namespace rfc::core {

class RelationalComplexBuilder {
public:
    explicit RelationalComplexBuilder(RelationSignature signature,
                                      SemanticScale semantic_scale = SemanticScale{0})
        : signature_(std::move(signature)), semantic_scale_(semantic_scale) {
        grades_.resize(1u);
    }

    // Preserve explicitly declared empty grades during exact endpoint replay.
    void declare_grade(Grade grade) { ensure_grade(grade); }

    std::uint32_t declare_witness_identity(IdentityKey identity) { return intern_grade_zero(identity); }

    RelationId add_canonical_grade_one(RelationTypeId type,
                                       const std::vector<IdentityKey>& identities,
                                       std::size_t head_position) {
        require_one_head_constructor(type);
        const auto composite = composite::canonical_one_head(identities.size(), head_position);
        const auto role_aspect = require_distinguished_role(type);
        std::vector<BoundaryOccurrenceInput> occurrences;
        occurrences.reserve(identities.size());
        for (std::size_t i = 0; i < identities.size(); ++i) {
            occurrences.push_back(BoundaryOccurrenceInput{
                identities[i], composite[i].exists, composite[i].negative, composite[i].magnitude,
                {{role_aspect, i == head_position}}});
        }
        return add_grade_one(type, occurrences);
    }

    RelationId add_weighted_branch_grade_one(RelationTypeId type,
                                              const std::vector<IdentityKey>& identities,
                                              std::size_t head_position,
                                              const numeric::RationalVector& tail_shares,
                                              bool reversed = false) {
        require_one_head_constructor(type);
        if (identities.size() < 2) throw std::invalid_argument("branch relation requires arity >= 2");
        if (head_position >= identities.size()) throw std::out_of_range("head position");
        if (tail_shares.size() + 1u != identities.size()) {
            throw std::invalid_argument("branch tail share count mismatch");
        }
        const auto role_aspect = require_distinguished_role(type);
        std::vector<BoundaryOccurrenceInput> occurrences;
        occurrences.reserve(identities.size());
        std::size_t tail = 0;
        for (std::size_t i = 0; i < identities.size(); ++i) {
            const bool is_head = i == head_position;
            const auto magnitude = is_head ? numeric::Rational{1} : tail_shares.at(tail++);
            occurrences.push_back(BoundaryOccurrenceInput{
                identities[i], true, is_head ? !reversed : reversed, magnitude,
                {{role_aspect, is_head}}});
        }
        return add_grade_one(type, occurrences);
    }

    RelationId add_grade_one(RelationTypeId type,
                             const std::vector<BoundaryOccurrenceInput>& occurrences) {
        return add_grade_one(next_relation_key(Grade{1}), type, occurrences);
    }

    RelationId add_grade_one(RelationKey relation_key,
                             RelationTypeId type,
                             const std::vector<BoundaryOccurrenceInput>& occurrences) {
        validate_type_arity(type, occurrences.size());
        ensure_grade(Grade{1});
        claim_relation_key(Grade{1}, relation_key);
        PendingRelation relation;
        relation.identity = relation_key;
        relation.type = type;
        relation.incidences.reserve(occurrences.size());
        for (const auto& occurrence : occurrences) {
            const auto lower = intern_grade_zero(occurrence.identity);
            auto aspects = resolve_aspect_values(type, occurrence.aspects);
            relation.incidences.push_back(make_incidence(lower, occurrence.exists, occurrence.negative,
                                                         occurrence.magnitude, std::move(aspects),
                                                         occurrence.occurrence_key));
        }
        validate_relation_semantics(type, relation.incidences);
        auto& grade = grades_[1];
        if (grade.relations.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("retained cell index overflow");
        const auto index = static_cast<std::uint32_t>(grade.relations.size());
        grade.relations.push_back(std::move(relation));
        return RelationId{Grade{1}, index};
    }

    RelationId add_canonical_higher(Grade grade,
                                    RelationTypeId type,
                                    const std::vector<std::uint32_t>& lower_indices,
                                    std::size_t head_position) {
        require_one_head_constructor(type);
        const auto composite = composite::canonical_one_head(lower_indices.size(), head_position);
        const auto role_aspect = require_distinguished_role(type);
        std::vector<HigherIncidenceInput> incidences;
        incidences.reserve(lower_indices.size());
        for (std::size_t i = 0; i < lower_indices.size(); ++i) {
            incidences.emplace_back(lower_indices[i], composite[i].exists, composite[i].negative,
                                    composite[i].magnitude,
                                    std::vector<AspectValue>{{role_aspect, i == head_position}});
        }
        return add_higher(grade, type, incidences);
    }

    RelationId add_weighted_branch_higher(Grade grade,
                                          RelationTypeId type,
                                          const std::vector<std::uint32_t>& lower_indices,
                                          std::size_t head_position,
                                          const numeric::RationalVector& tail_shares,
                                          bool reversed = false) {
        require_one_head_constructor(type);
        if (lower_indices.size() < 2) throw std::invalid_argument("branch relation requires arity >= 2");
        if (head_position >= lower_indices.size()) throw std::out_of_range("head position");
        if (tail_shares.size() + 1u != lower_indices.size()) {
            throw std::invalid_argument("branch tail share count mismatch");
        }
        const auto role_aspect = require_distinguished_role(type);
        std::vector<HigherIncidenceInput> incidences;
        incidences.reserve(lower_indices.size());
        std::size_t tail = 0;
        for (std::size_t i = 0; i < lower_indices.size(); ++i) {
            const bool is_head = i == head_position;
            const auto magnitude = is_head ? numeric::Rational{1} : tail_shares.at(tail++);
            incidences.emplace_back(lower_indices[i], true, is_head ? !reversed : reversed,
                                    magnitude, std::vector<AspectValue>{{role_aspect, is_head}});
        }
        return add_higher(grade, type, incidences);
    }

    RelationId add_higher(Grade grade,
                          RelationTypeId type,
                          const std::vector<HigherIncidenceInput>& incidences) {
        return add_higher(grade, next_relation_key(grade), type, incidences);
    }

    RelationId add_higher(Grade grade,
                          RelationKey relation_key,
                          RelationTypeId type,
                          const std::vector<HigherIncidenceInput>& incidences) {
        if (grade.value < 2) throw std::invalid_argument("add_higher requires grade >= 2");
        ensure_grade(grade);
        claim_relation_key(grade, relation_key);
        validate_type_arity(type, incidences.size());
        const auto lower_count = grades_.at(grade.value - 1u).relations.size();
        PendingRelation relation;
        relation.identity = relation_key;
        relation.type = type;
        relation.incidences.reserve(incidences.size());
        for (const auto& incidence : incidences) {
            if (incidence.lower_index >= lower_count) throw std::out_of_range("higher incidence lower relation index");
            auto aspects = resolve_aspect_values(type, incidence.aspects);
            relation.incidences.push_back(make_incidence(incidence.lower_index, incidence.exists, incidence.negative,
                                                         incidence.magnitude, std::move(aspects),
                                                         incidence.occurrence_key));
        }
        validate_relation_semantics(type, relation.incidences);
        validate_higher_closure(grade, relation.incidences);
        auto& pending_grade = grades_[grade.value];
        if (pending_grade.relations.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("retained cell index overflow");
        const auto index = static_cast<std::uint32_t>(pending_grade.relations.size());
        pending_grade.relations.push_back(std::move(relation));
        return RelationId{grade, index};
    }

    [[nodiscard]] RelationalComplex freeze() const {
        std::vector<GradeArena> frozen;
        frozen.reserve(grades_.size());

        GradeArena grade_zero;
        grade_zero.grade = Grade{0};
        grade_zero.identities = grade_zero_identities_;
        grade_zero.relation_types.resize(grade_zero_identities_.size(), 0u);
        grade_zero.cell_scales.resize(grade_zero_identities_.size(), numeric::Rational{1});
        grade_zero.offsets = {0u};
        grade_zero.aspects = composite::AspectPlanes(signature_.aspects(), 0u);
        frozen.push_back(std::move(grade_zero));

        for (std::size_t grade_index = 1; grade_index < grades_.size(); ++grade_index) {
            const auto& pending = grades_[grade_index];
            const auto& lower = frozen[grade_index - 1u];
            GradeArena arena;
            arena.grade = Grade{static_cast<std::uint16_t>(grade_index)};
            arena.offsets.reserve(pending.relations.size() + 1u);
            arena.offsets.push_back(0u);
            arena.relation_types.reserve(pending.relations.size());
            arena.relation_identities.reserve(pending.relations.size());
            arena.cell_scales.reserve(pending.relations.size());

            std::vector<std::vector<numeric::Integer>> integer_columns;
            integer_columns.reserve(pending.relations.size());
            std::size_t total_incidences = 0;

            for (const auto& relation : pending.relations) {
                numeric::Integer upper_scale = 1;
                for (const auto& incidence : relation.incidences) {
                    if (!incidence.exists) continue;
                    numeric::Rational ratio = incidence.coefficient / lower.cell_scales.at(incidence.lower_index);
                    ratio.canonicalize();
                    upper_scale = numeric::lcm_positive(upper_scale, numeric::denominator(ratio));
                }
                if (upper_scale == 0) upper_scale = 1;
                arena.cell_scales.emplace_back(upper_scale);
                arena.relation_types.push_back(relation.type);
                arena.relation_identities.push_back(relation.identity);

                std::vector<numeric::Integer> column;
                column.reserve(relation.incidences.size());
                for (const auto& incidence : relation.incidences) {
                    numeric::Rational integer_value = incidence.coefficient * arena.cell_scales.back()
                                                     / lower.cell_scales.at(incidence.lower_index);
                    integer_value.canonicalize();
                    if (integer_value.get_den() != 1) throw std::logic_error("coordinated integralization failed");
                    column.push_back(integer_value.get_num());
                }
                total_incidences = machine::checked_size_add(total_incidences, relation.incidences.size());
                arena.offsets.push_back(total_incidences);
                integer_columns.push_back(std::move(column));
            }

            arena.lower_index.reserve(total_incidences);
            arena.upper_index.reserve(total_incidences);
            arena.occurrence_identities.reserve(total_incidences);
            arena.lattice_coefficient.reserve(total_incidences);
            arena.aspects = composite::AspectPlanes(signature_.aspects(), total_incidences);
            const auto exists_id = signature_.existence_aspect();
            const auto orientation_id = signature_.orientation_aspect();

            std::size_t slot = 0;
            for (std::size_t relation_index = 0; relation_index < pending.relations.size(); ++relation_index) {
                const auto& relation = pending.relations[relation_index];
                for (std::size_t local = 0; local < relation.incidences.size(); ++local, ++slot) {
                    const auto& incidence = relation.incidences[local];
                    arena.lower_index.push_back(incidence.lower_index);
                    arena.upper_index.push_back(static_cast<std::uint32_t>(relation_index));
                    arena.occurrence_identities.push_back(incidence.identity);
                    arena.lattice_coefficient.push_back(integer_columns[relation_index][local]);
                    arena.aspects.set(exists_id, slot, incidence.exists);
                    arena.aspects.set(orientation_id, slot, incidence.negative);
                    for (const auto aspect : incidence.aspects) arena.aspects.set(aspect.aspect, slot, aspect.value);
                }
            }

            arena.lower_offsets.assign(lower.cell_count() + 1u, 0u);
            for (const auto lower_cell : arena.lower_index) ++arena.lower_offsets[static_cast<std::size_t>(lower_cell) + 1u];
            for (std::size_t i = 1; i < arena.lower_offsets.size(); ++i) {
                arena.lower_offsets[i] += arena.lower_offsets[i - 1u];
            }
            arena.incidence_by_lower.resize(total_incidences);
            auto cursor = arena.lower_offsets;
            for (std::size_t p = 0; p < arena.lower_index.size(); ++p) {
                const auto lower_cell = arena.lower_index[p];
                arena.incidence_by_lower[cursor[lower_cell]++] = p;
            }
            frozen.push_back(std::move(arena));
        }

        return RelationalComplex(signature_, semantic_scale_, std::move(frozen));
    }

private:
    friend class storage::MappedRfcx;

    // The codec supplies fresh arenas after checking all structural sections.
    RelationalComplex freeze_retained(std::vector<GradeArena> grades) const {
        RelationalComplex source(signature_, semantic_scale_, std::move(grades));
        for (std::size_t k = 1; k < source.grade_count(); ++k) {
            const auto grade = Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = source.grade(grade);
            for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
                const auto type = arena.relation_types[cell];
                const auto [begin, end] = arena.incidence_range(cell);
                validate_type_arity(type, end - begin);
                if (signature_.type(type).constraints.rules.empty()) continue;
                std::vector<PendingIncidence> incidences;
                incidences.reserve(end - begin);
                for (auto slot = begin; slot < end; ++slot) {
                    std::vector<AspectValue> aspects;
                    for (std::size_t a = 0; a < signature_.aspects().size(); ++a) {
                        const auto aspect = static_cast<composite::AspectId>(a);
                        if (aspect != signature_.existence_aspect() && aspect != signature_.orientation_aspect())
                            aspects.push_back({aspect, arena.aspects.get(aspect, slot)});
                    }
                    incidences.push_back({arena.lower_index[slot], arena.occurrence_identities[slot],
                        source.incidence_coefficient(grade, slot),
                        arena.aspects.get(signature_.existence_aspect(), slot),
                        arena.aspects.get(signature_.orientation_aspect(), slot), std::move(aspects)});
                }
                validate_relation_semantics(type, incidences);
            }
        }
        if (!bianchi_through(source, source.max_grade()).satisfied())
            throw std::invalid_argument("higher relation boundary is not closed: adjacent chain axiom violated");
        return source;
    }

    struct PendingIncidence {
        std::uint32_t lower_index{};
        OccurrenceKey identity{};
        numeric::Rational coefficient{0};
        bool exists{true};
        bool negative{false};
        std::vector<AspectValue> aspects;
    };
    struct PendingRelation {
        RelationKey identity{};
        RelationTypeId type{};
        std::vector<PendingIncidence> incidences;
    };
    struct PendingGrade { std::vector<PendingRelation> relations; };

    PendingIncidence make_incidence(std::uint32_t lower,
                                    bool exists,
                                    bool negative,
                                    numeric::Rational magnitude,
                                    std::vector<AspectValue> aspects,
                                    std::optional<OccurrenceKey> requested_key) {
        if (magnitude < 0) throw std::invalid_argument("magnitude must be nonnegative; orientation owns sign");
        validate_aspect_values(aspects);
        numeric::Rational coefficient = exists ? magnitude : numeric::Rational{0};
        if (negative) coefficient = -coefficient;
        coefficient.canonicalize();
        const auto key = requested_key ? claim_occurrence_key(*requested_key) : next_occurrence_key();
        return PendingIncidence{lower, key, coefficient, exists, negative, std::move(aspects)};
    }

    RelationKey next_relation_key(Grade grade) {
        ensure_grade(grade);
        ensure_relation_key_state(grade);
        auto& next = next_relation_keys_[grade.value];
        auto& used = used_relation_keys_[grade.value];
        while (used.contains(next)) ++next;
        return RelationKey{next++};
    }

    void claim_relation_key(Grade grade, RelationKey key) {
        ensure_relation_key_state(grade);
        if (!used_relation_keys_[grade.value].insert(key.value).second) {
            throw std::invalid_argument("duplicate primary relation identity within grade");
        }
    }

    void ensure_relation_key_state(Grade grade) {
        const auto count = static_cast<std::size_t>(grade.value) + 1u;
        if (used_relation_keys_.size() < count) used_relation_keys_.resize(count);
        if (next_relation_keys_.size() < count) next_relation_keys_.resize(count, 0u);
    }

    OccurrenceKey next_occurrence_key() {
        while (used_occurrence_keys_.contains(next_occurrence_key_)) ++next_occurrence_key_;
        const auto value = next_occurrence_key_++;
        used_occurrence_keys_.insert(value);
        return OccurrenceKey{value};
    }

    OccurrenceKey claim_occurrence_key(OccurrenceKey key) {
        if (!used_occurrence_keys_.insert(key.value).second) {
            throw std::invalid_argument("duplicate boundary occurrence identity");
        }
        return key;
    }

    std::uint32_t intern_grade_zero(IdentityKey identity) {
        if (const auto it = grade_zero_index_.find(identity); it != grade_zero_index_.end()) return it->second;
        if (grade_zero_identities_.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("retained cell index overflow");
        const auto index = static_cast<std::uint32_t>(grade_zero_identities_.size());
        grade_zero_identities_.push_back(identity);
        grade_zero_index_.emplace(identity, index);
        return index;
    }

    void ensure_grade(Grade grade) {
        if (grades_.size() <= grade.value) grades_.resize(static_cast<std::size_t>(grade.value) + 1u);
    }

    void validate_type_arity(RelationTypeId type_id, std::size_t arity) const {
        const auto& type = signature_.type(type_id);
        if (arity < type.min_arity) throw std::invalid_argument("relation arity below type minimum");
        if (type.max_arity != 0 && arity > type.max_arity) throw std::invalid_argument("relation arity above type maximum");
    }

    void validate_relation_semantics(RelationTypeId type_id,
                                     const std::vector<PendingIncidence>& incidences) const {
        const auto& constraints = signature_.type(type_id).constraints;
        const auto abs_coeff = [](const numeric::Rational& value) {
            numeric::Rational result = value < 0 ? -value : value;
            result.canonicalize();
            return result;
        };

        for (const auto& rule : constraints.rules) {
            switch (rule.kind) {
                case RelationRuleKind::AspectImpliesExistence:
                    for (const auto& incidence : incidences) {
                        if (aspect_value(incidence, rule.aspect) && !incidence.exists) {
                            throw std::invalid_argument("relation-role aspect cannot be active on an absent incidence");
                        }
                    }
                    break;
                case RelationRuleKind::ActiveAspectCountEquals: {
                    std::size_t count = 0;
                    for (const auto& incidence : incidences) {
                        if (incidence.exists && aspect_value(incidence, rule.aspect)) ++count;
                    }
                    if (count != rule.count) throw std::invalid_argument("relation aspect active-count rule violated");
                    break;
                }
                case RelationRuleKind::AspectOrientationOpposesActiveComplement: {
                    const PendingIncidence* role = nullptr;
                    for (const auto& incidence : incidences) {
                        if (incidence.exists && aspect_value(incidence, rule.aspect)) {
                            if (role != nullptr) throw std::invalid_argument("orientation-opposition rule requires one active role incidence");
                            role = &incidence;
                        }
                    }
                    if (role == nullptr) throw std::invalid_argument("orientation-opposition rule has no active role incidence");
                    for (const auto& incidence : incidences) {
                        if (!incidence.exists || aspect_value(incidence, rule.aspect)) continue;
                        if (incidence.negative == role->negative) {
                            throw std::invalid_argument("relation role orientation must oppose active complement");
                        }
                    }
                    break;
                }
                case RelationRuleKind::AspectMagnitudeEquals:
                    for (const auto& incidence : incidences) {
                        if (!incidence.exists || !aspect_value(incidence, rule.aspect)) continue;
                        if (abs_coeff(incidence.coefficient) != rule.value) {
                            throw std::invalid_argument("relation role magnitude rule violated");
                        }
                    }
                    break;
                case RelationRuleKind::ActiveComplementMagnitudePositive:
                    for (const auto& incidence : incidences) {
                        if (!incidence.exists || aspect_value(incidence, rule.aspect)) continue;
                        if (abs_coeff(incidence.coefficient) <= 0) {
                            throw std::invalid_argument("relation complement magnitude must be positive");
                        }
                    }
                    break;
                case RelationRuleKind::ActiveComplementMagnitudeSumEquals: {
                    numeric::Rational sum = 0;
                    std::size_t count = 0;
                    for (const auto& incidence : incidences) {
                        if (!incidence.exists || aspect_value(incidence, rule.aspect)) continue;
                        sum += abs_coeff(incidence.coefficient);
                        ++count;
                    }
                    sum.canonicalize();
                    if (count == 0u || sum != rule.value) {
                        throw std::invalid_argument("relation complement magnitude-sum rule violated");
                    }
                    break;
                }
                default: throw std::invalid_argument("unknown executable relation rule");
            }
        }
    }

    [[nodiscard]] bool aspect_value(const PendingIncidence& incidence,
                                    composite::AspectId aspect) const {
        const auto exists_id = signature_.existence_aspect();
        const auto orientation_id = signature_.orientation_aspect();
        if (aspect == exists_id) return incidence.exists;
        if (aspect == orientation_id) return incidence.negative;
        for (const auto& value : incidence.aspects) {
            if (value.aspect == aspect) return value.value;
        }
        return signature_.aspects().at(aspect).default_value;
    }

    void validate_aspect_values(const std::vector<AspectValue>& aspects) const {
        const auto exists_id = signature_.existence_aspect();
        const auto orientation_id = signature_.orientation_aspect();
        std::unordered_set<composite::AspectId> seen;
        seen.reserve(aspects.size());
        for (const auto& value : aspects) {
            (void)signature_.aspects().at(value.aspect);
            if (value.aspect == exists_id || value.aspect == orientation_id) {
                throw std::invalid_argument("existence/orientation must use dedicated incidence fields");
            }
            if (!seen.insert(value.aspect).second) {
                throw std::invalid_argument("duplicate aspect assignment on incidence");
            }
        }
    }

    [[nodiscard]] std::vector<AspectValue> resolve_aspect_values(RelationTypeId type_id,
                                                                 const std::vector<AspectValue>& aspects) const {
        std::vector<AspectValue> resolved;
        resolved.reserve(aspects.size());
        for (auto value : aspects) {
            if (value.aspect == type_distinguished_role_aspect) {
                const auto role = signature_.type(type_id).distinguished_role_aspect();
                if (!role) throw std::invalid_argument("relation type has no declared distinguished role");
                value.aspect = *role;
            }
            resolved.push_back(value);
        }
        return resolved;
    }

    void require_one_head_constructor(RelationTypeId type_id) const {
        const auto& type = signature_.type(type_id);
        if (type.constructor != CompositeConstructorKind::NormalizedOneHead) {
            throw std::invalid_argument("relation type has no normalized one-head constructor contract");
        }
    }

    [[nodiscard]] composite::AspectId require_distinguished_role(RelationTypeId type_id) const {
        const auto role = signature_.type(type_id).distinguished_role_aspect();
        if (!role) throw std::invalid_argument("relation type has no distinguished relation-role aspect");
        return *role;
    }

    // Enforce the adjacent chain axiom at assertion time.  This walks only the
    // active two-step incidence routes touched by the candidate higher cell;
    // it never materializes either boundary matrix or a dense residual vector.
    void validate_higher_closure(Grade grade,
                                 const std::vector<PendingIncidence>& incidences) const {
        if (grade.value < 2) return;
        const auto& lower_grade = grades_.at(grade.value - 1u);

        std::unordered_map<std::uint32_t, numeric::Rational> residual;
        std::size_t route_hint = 0;
        for (const auto& incidence : incidences) {
            if (!incidence.exists || incidence.coefficient == 0) continue;
            route_hint = machine::checked_size_add(route_hint, lower_grade.relations.at(incidence.lower_index).incidences.size());
        }
        residual.reserve(route_hint);

        for (const auto& upper_incidence : incidences) {
            if (!upper_incidence.exists || upper_incidence.coefficient == 0) continue;
            const auto& lower_relation = lower_grade.relations.at(upper_incidence.lower_index);
            for (const auto& lower_incidence : lower_relation.incidences) {
                if (!lower_incidence.exists || lower_incidence.coefficient == 0) continue;
                const auto contribution = upper_incidence.coefficient * lower_incidence.coefficient;
                auto [it, inserted] = residual.try_emplace(lower_incidence.lower_index, contribution);
                if (!inserted) it->second += contribution;
                it->second.canonicalize();
                if (it->second == 0) residual.erase(it);
            }
        }

        if (!residual.empty()) {
            throw std::invalid_argument("higher relation boundary is not closed: adjacent chain axiom violated");
        }
    }

    RelationSignature signature_;
    SemanticScale semantic_scale_{};
    std::vector<PendingGrade> grades_;
    std::vector<IdentityKey> grade_zero_identities_;
    std::unordered_map<IdentityKey, std::uint32_t> grade_zero_index_;
    std::vector<std::unordered_set<std::uint64_t>> used_relation_keys_;
    std::vector<std::uint64_t> next_relation_keys_;
    std::unordered_set<std::uint64_t> used_occurrence_keys_;
    std::uint64_t next_occurrence_key_{};
};

} // namespace rfc::core
