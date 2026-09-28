#pragma once

#include "rfc/core/relational_complex.hpp"
#include "rfc/machine/packed_bits.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/temporal/composite_change.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::temporal {

// Stable source identity across time. Grade zero uses IdentityKey; positive
// grades use RelationKey. The grade disambiguates equal numeric key values.
struct StableCellIdentity {
    core::Grade grade{};
    std::uint64_t value{};
    constexpr auto operator<=>(const StableCellIdentity&) const = default;
};

struct RelationTypeChange {
    core::RelationKey relation{};
    core::RelationTypeId old_type{};
    core::RelationTypeId new_type{};
};

struct AttachingOrderChange {
    core::RelationKey relation{};
    std::vector<core::OccurrenceKey> old_order;
    std::vector<core::OccurrenceKey> new_order;
};

struct PersistentOccurrenceChange {
    core::OccurrenceKey occurrence{};
    std::size_t old_slot{};
    std::size_t new_slot{};
    StableCellIdentity old_lower{};
    StableCellIdentity new_lower{};
    core::RelationKey old_parent{};
    core::RelationKey new_parent{};
    numeric::Rational old_coefficient{0};
    numeric::Rational new_coefficient{0};
    CompositeOccurrenceChange composite;
    std::vector<composite::AspectId> changed_aspects;

    [[nodiscard]] bool attachment_changed() const noexcept {
        return old_lower != new_lower || old_parent != new_parent;
    }
    [[nodiscard]] bool coefficient_changed() const noexcept {
        return old_coefficient != new_coefficient;
    }
    [[nodiscard]] bool changed() const noexcept {
        return attachment_changed() || coefficient_changed() || !changed_aspects.empty();
    }
};

struct GradeSourceDelta {
    core::Grade grade{};

    // Old/new coordinate correspondence induced only by stable source identity.
    // absent_index means the source cell did not persist to the other endpoint.
    static constexpr std::uint32_t absent_index = static_cast<std::uint32_t>(-1);
    std::vector<std::uint32_t> old_to_new;
    std::vector<std::uint32_t> new_to_old;
    std::vector<StableCellIdentity> born_cells;
    std::vector<StableCellIdentity> removed_cells;
    std::vector<RelationTypeChange> relation_type_changes;
    // Order within an attaching fiber is semantic; arena order is not.
    std::vector<AttachingOrderChange> attaching_order_changes;

    // Positive grades retain occurrence identity independently from aggregate
    // coordinates. Persistent occurrence keys are sorted and provide the
    // alignment on which aspect XOR planes are defined.
    std::vector<core::OccurrenceKey> persistent_occurrences;
    std::vector<std::uint32_t> old_occurrence_slots;
    std::vector<std::uint32_t> new_occurrence_slots;
    std::vector<core::OccurrenceKey> born_occurrences;
    std::vector<core::OccurrenceKey> removed_occurrences;
    std::vector<machine::PackedBits> aspect_xor;
    std::vector<PersistentOccurrenceChange> occurrence_changes;

    [[nodiscard]] bool source_structure_unchanged() const noexcept {
        if (!born_cells.empty() || !removed_cells.empty() || !relation_type_changes.empty() ||
            !born_occurrences.empty() || !removed_occurrences.empty() ||
            !attaching_order_changes.empty()) return false;
        for (const auto& bits : aspect_xor) if (bits.any()) return false;
        for (const auto& change : occurrence_changes) {
            if (change.attachment_changed() || change.coefficient_changed()) return false;
        }
        return true;
    }
};

namespace detail {

inline bool same_signature_for_temporal_alignment(const core::RelationSignature& a,
                                                  const core::RelationSignature& b) {
    if (a.aspects().size() != b.aspects().size() || a.type_count() != b.type_count()) return false;
    for (std::size_t i = 0; i < a.aspects().size(); ++i) {
        const auto id = static_cast<composite::AspectId>(i);
        const auto& x = a.aspects().at(id);
        const auto& y = b.aspects().at(id);
        if (x.name != y.name || x.role != y.role || x.default_value != y.default_value) return false;
    }
    for (std::size_t i = 0; i < a.type_count(); ++i) {
        const auto& x = a.type(static_cast<core::RelationTypeId>(i));
        const auto& y = b.type(static_cast<core::RelationTypeId>(i));
        if (x.name != y.name || x.boundary_policy != y.boundary_policy ||
            x.min_arity != y.min_arity || x.max_arity != y.max_arity ||
            x.constraints.shape != y.constraints.shape ||
            x.constraints.distinguished_role != y.constraints.distinguished_role ||
            x.constructor != y.constructor || x.constraints.rules.size() != y.constraints.rules.size()) return false;
        for (std::size_t r = 0; r < x.constraints.rules.size(); ++r) {
            const auto& xr = x.constraints.rules[r];
            const auto& yr = y.constraints.rules[r];
            if (xr.kind != yr.kind || xr.aspect != yr.aspect || xr.count != yr.count || xr.value != yr.value) return false;
        }
    }
    return true;
}

inline StableCellIdentity stable_cell_identity(const core::RelationalComplex& complex,
                                                core::Grade grade,
                                                std::size_t index) {
    const auto& arena = complex.grade(grade);
    if (index >= arena.cell_count()) throw std::out_of_range("stable cell identity index");
    if (grade.value == 0) return {grade, arena.identities.at(index)};
    return {grade, arena.relation_identities.at(index).value};
}

inline std::vector<StableCellIdentity> grade_keys(const core::RelationalComplex& complex,
                                                  core::Grade grade) {
    if (grade.value >= complex.grade_count()) return {};
    const auto count = complex.grade(grade).cell_count();
    std::vector<StableCellIdentity> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) out.push_back(stable_cell_identity(complex, grade, i));
    return out;
}

inline std::unordered_map<std::uint64_t, std::uint32_t> key_index(
    const std::vector<StableCellIdentity>& keys) {
    std::unordered_map<std::uint64_t, std::uint32_t> result;
    result.reserve(keys.size() * 2u + 1u);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (!result.emplace(keys[i].value, static_cast<std::uint32_t>(i)).second) {
            throw std::logic_error("duplicate stable cell identity within grade");
        }
    }
    return result;
}

inline std::unordered_map<std::uint64_t, std::uint32_t> occurrence_index(
    const core::RelationalComplex& complex, core::Grade grade) {
    std::unordered_map<std::uint64_t, std::uint32_t> result;
    if (grade.value == 0 || grade.value >= complex.grade_count()) return result;
    const auto& arena = complex.grade(grade);
    result.reserve(arena.incidence_count() * 2u + 1u);
    for (std::size_t i = 0; i < arena.incidence_count(); ++i) {
        if (!result.emplace(arena.occurrence_identities[i].value, static_cast<std::uint32_t>(i)).second) {
            throw std::logic_error("duplicate occurrence identity within grade");
        }
    }
    return result;
}

inline numeric::Rational magnitude_of(numeric::Rational value) {
    if (value < 0) value = -value;
    value.canonicalize();
    return value;
}

inline composite::CompositeCoefficient coefficient_state(const core::RelationalComplex& complex,
                                                         core::Grade grade,
                                                         std::size_t slot) {
    const auto& arena = complex.grade(grade);
    const auto exists = complex.signature().existence_aspect();
    const auto orientation = complex.signature().orientation_aspect();
    return composite::CompositeCoefficient{
        arena.aspects.get(exists, slot),
        arena.aspects.get(orientation, slot),
        magnitude_of(complex.incidence_coefficient(grade, slot))};
}

} // namespace detail

class RelationalSourceDelta {
public:
    RelationalSourceDelta(const core::RelationalComplex& old_source,
                          const core::RelationalComplex& new_source)
        : old_(&old_source), new_(&new_source) {
        if (!detail::same_signature_for_temporal_alignment(old_source.signature(), new_source.signature())) {
            throw std::invalid_argument("temporal source delta requires the same declared relation signature; use an explicit typed correspondence for signature changes");
        }
        build();
    }

    [[nodiscard]] const core::RelationalComplex& old_source() const noexcept { return *old_; }
    [[nodiscard]] const core::RelationalComplex& new_source() const noexcept { return *new_; }
    [[nodiscard]] const std::vector<GradeSourceDelta>& grades() const noexcept { return grades_; }
    [[nodiscard]] const GradeSourceDelta& grade(core::Grade grade) const { return grades_.at(grade.value); }

    [[nodiscard]] numeric::ExactMatrix correspondence(core::Grade grade) const {
        const auto& delta = grades_.at(grade.value);
        numeric::ExactMatrix J(delta.new_to_old.size(), delta.old_to_new.size());
        for (std::size_t old_index = 0; old_index < delta.old_to_new.size(); ++old_index) {
            const auto mapped = delta.old_to_new[old_index];
            if (mapped != GradeSourceDelta::absent_index) J.set(mapped, old_index, numeric::Rational{1});
        }
        return J;
    }

    [[nodiscard]] numeric::RationalVector field_innovation(core::Grade grade,
                                                            const numeric::RationalVector& old_field,
                                                            const numeric::RationalVector& new_field) const {
        const auto J = correspondence(grade);
        if (old_field.size() != J.cols() || new_field.size() != J.rows()) {
            throw std::invalid_argument("temporal field innovation dimension mismatch");
        }
        return numeric::sub(new_field, J.apply(old_field));
    }

    [[nodiscard]] bool source_structure_unchanged() const noexcept {
        for (const auto& delta : grades_) if (!delta.source_structure_unchanged()) return false;
        return true;
    }

private:
    void build() {
        const auto grade_count = std::max(old_->grade_count(), new_->grade_count());
        grades_.reserve(grade_count);
        for (std::size_t k = 0; k < grade_count; ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            GradeSourceDelta delta;
            delta.grade = grade;

            const auto old_keys = detail::grade_keys(*old_, grade);
            const auto new_keys = detail::grade_keys(*new_, grade);
            const auto old_index = detail::key_index(old_keys);
            const auto new_index = detail::key_index(new_keys);
            delta.old_to_new.assign(old_keys.size(), GradeSourceDelta::absent_index);
            delta.new_to_old.assign(new_keys.size(), GradeSourceDelta::absent_index);

            for (std::size_t i = 0; i < old_keys.size(); ++i) {
                if (const auto it = new_index.find(old_keys[i].value); it != new_index.end()) {
                    delta.old_to_new[i] = it->second;
                    delta.new_to_old[it->second] = static_cast<std::uint32_t>(i);
                    if (grade.value > 0) {
                        const auto& oa = old_->grade(grade);
                        const auto& na = new_->grade(grade);
                        const auto [ob, oe] = oa.incidence_range(i);
                        const auto [nb, ne] = na.incidence_range(it->second);
                        std::vector<core::OccurrenceKey> old_order(oa.occurrence_identities.begin() + static_cast<std::ptrdiff_t>(ob),
                                                                  oa.occurrence_identities.begin() + static_cast<std::ptrdiff_t>(oe));
                        std::vector<core::OccurrenceKey> new_order(na.occurrence_identities.begin() + static_cast<std::ptrdiff_t>(nb),
                                                                  na.occurrence_identities.begin() + static_cast<std::ptrdiff_t>(ne));
                        if (old_order != new_order) {
                            delta.attaching_order_changes.push_back({oa.relation_identities[i], std::move(old_order), std::move(new_order)});
                        }
                        if (oa.relation_types[i] != na.relation_types[it->second]) {
                            delta.relation_type_changes.push_back({oa.relation_identities[i], oa.relation_types[i], na.relation_types[it->second]});
                        }
                    }
                } else {
                    delta.removed_cells.push_back(old_keys[i]);
                }
            }
            for (std::size_t i = 0; i < new_keys.size(); ++i) {
                if (!old_index.contains(new_keys[i].value)) delta.born_cells.push_back(new_keys[i]);
            }

            if (grade.value > 0) build_occurrences(delta);
            grades_.push_back(std::move(delta));
        }
    }

    void build_occurrences(GradeSourceDelta& delta) {
        const auto grade = delta.grade;
        const auto old_map = detail::occurrence_index(*old_, grade);
        const auto new_map = detail::occurrence_index(*new_, grade);

        std::vector<std::uint64_t> persistent;
        persistent.reserve(std::min(old_map.size(), new_map.size()));
        for (const auto& [key, slot] : old_map) {
            (void)slot;
            if (new_map.contains(key)) persistent.push_back(key);
            else delta.removed_occurrences.push_back(core::OccurrenceKey{key});
        }
        for (const auto& [key, slot] : new_map) {
            (void)slot;
            if (!old_map.contains(key)) delta.born_occurrences.push_back(core::OccurrenceKey{key});
        }
        std::sort(persistent.begin(), persistent.end());
        std::sort(delta.born_occurrences.begin(), delta.born_occurrences.end());
        std::sort(delta.removed_occurrences.begin(), delta.removed_occurrences.end());

        delta.persistent_occurrences.reserve(persistent.size());
        delta.old_occurrence_slots.reserve(persistent.size());
        delta.new_occurrence_slots.reserve(persistent.size());
        const auto aspect_count = old_->signature().aspects().size();
        delta.aspect_xor.reserve(aspect_count);
        for (std::size_t a = 0; a < aspect_count; ++a) delta.aspect_xor.emplace_back(persistent.size(), false);

        if (grade.value >= old_->grade_count() || grade.value >= new_->grade_count()) return;
        const auto& oa = old_->grade(grade);
        const auto& na = new_->grade(grade);
        const auto lower_grade = core::Grade{static_cast<std::uint16_t>(grade.value - 1u)};

        for (std::size_t aligned = 0; aligned < persistent.size(); ++aligned) {
            const auto key = persistent[aligned];
            const auto os = old_map.at(key);
            const auto ns = new_map.at(key);
            delta.persistent_occurrences.push_back(core::OccurrenceKey{key});
            delta.old_occurrence_slots.push_back(os);
            delta.new_occurrence_slots.push_back(ns);

            PersistentOccurrenceChange change;
            change.occurrence = core::OccurrenceKey{key};
            change.old_slot = os;
            change.new_slot = ns;
            change.old_lower = detail::stable_cell_identity(*old_, lower_grade, oa.lower_index[os]);
            change.new_lower = detail::stable_cell_identity(*new_, lower_grade, na.lower_index[ns]);
            change.old_parent = oa.relation_identities.at(oa.upper_index[os]);
            change.new_parent = na.relation_identities.at(na.upper_index[ns]);
            change.old_coefficient = old_->incidence_coefficient(grade, os);
            change.new_coefficient = new_->incidence_coefficient(grade, ns);
            change.composite = composite_coefficient_change(
                detail::coefficient_state(*old_, grade, os),
                detail::coefficient_state(*new_, grade, ns));

            for (std::size_t a = 0; a < aspect_count; ++a) {
                const auto aspect = static_cast<composite::AspectId>(a);
                const bool ov = oa.aspects.get(aspect, os);
                const bool nv = na.aspects.get(aspect, ns);
                if (ov != nv) {
                    delta.aspect_xor[a].set(aligned);
                    change.changed_aspects.push_back(aspect);
                }
            }
            if (change.changed()) delta.occurrence_changes.push_back(std::move(change));
        }
    }

    const core::RelationalComplex* old_{};
    const core::RelationalComplex* new_{};
    std::vector<GradeSourceDelta> grades_;
};

} // namespace rfc::temporal
