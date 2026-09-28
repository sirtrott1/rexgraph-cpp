#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/composite/boundary.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::core {

struct BoundaryOccurrenceView {
    std::size_t incidence_slot{};
    CellId lower{};
    numeric::Rational coefficient{0};
    bool exists{false};
    bool negative{false};
    const composite::AspectPlanes* aspect_planes{};

    [[nodiscard]] bool aspect(composite::AspectId id) const {
        if (aspect_planes == nullptr) throw std::logic_error("occurrence view has no aspect source");
        return aspect_planes->get(id, incidence_slot);
    }
};

// A retained boundary-occurrence fiber is the direct local incidence data of
// one primary relation. It is not a pairwise expansion: all occurrence legs
// share one upper relation identity and therefore one primary field amplitude.
class BoundaryOccurrenceFiber {
public:
    BoundaryOccurrenceFiber(const RelationalComplex& complex,
                            Grade grade,
                            std::size_t relation_index)
        : complex_(&complex), grade_(grade), relation_index_(relation_index) {
        if (grade.value == 0 || grade.value >= complex.grade_count()) {
            throw std::out_of_range("boundary fiber requires a positive relation grade");
        }
        const auto& arena = complex.grade(grade);
        if (relation_index >= arena.cell_count()) throw std::out_of_range("boundary fiber relation index");
        const auto exists_id = complex.signature().existence_aspect();
        const auto orientation_id = complex.signature().orientation_aspect();
        const auto [begin, end] = arena.incidence_range(relation_index);
        occurrences_.reserve(end - begin);
        for (std::size_t slot = begin; slot < end; ++slot) {
            occurrences_.push_back(BoundaryOccurrenceView{
                slot,
                CellId{Grade{static_cast<std::uint16_t>(grade.value - 1u)}, arena.lower_index[slot]},
                complex.incidence_coefficient(grade, slot),
                arena.aspects.get(exists_id, slot),
                arena.aspects.get(orientation_id, slot),
                &arena.aspects});
        }
    }

    [[nodiscard]] CellId relation() const noexcept {
        return CellId{grade_, static_cast<std::uint32_t>(relation_index_)};
    }
    [[nodiscard]] RelationTypeId relation_type() const {
        return complex_->grade(grade_).relation_types.at(relation_index_);
    }
    [[nodiscard]] const std::vector<BoundaryOccurrenceView>& occurrences() const noexcept { return occurrences_; }
    [[nodiscard]] std::size_t size() const noexcept { return occurrences_.size(); }

    [[nodiscard]] std::vector<composite::CompositeCoefficient> composite_state() const {
        std::vector<composite::CompositeCoefficient> result;
        result.reserve(occurrences_.size());
        for (const auto& occurrence : occurrences_) {
            numeric::Rational magnitude = occurrence.coefficient < 0 ? -occurrence.coefficient : occurrence.coefficient;
            magnitude.canonicalize();
            result.push_back(composite::CompositeCoefficient{occurrence.exists, occurrence.negative, std::move(magnitude)});
        }
        return result;
    }

private:
    const RelationalComplex* complex_{};
    Grade grade_{};
    std::size_t relation_index_{};
    std::vector<BoundaryOccurrenceView> occurrences_;
};

class BranchingRelationView {
public:
    BranchingRelationView(const RelationalComplex& complex,
                          Grade grade,
                          std::size_t relation_index)
        : fiber_(complex, grade, relation_index) {
        const auto& type = complex.signature().type(fiber_.relation_type());
        if (!type.is_branching()) throw std::invalid_argument("relation type is not branching");
        const auto role = type.distinguished_role_aspect();
        if (!role) throw std::logic_error("branching relation type has no distinguished role aspect");
        role_aspect_ = *role;

        for (const auto& occurrence : fiber_.occurrences()) {
            if (!occurrence.exists) continue;
            if (occurrence.aspect(role_aspect_)) {
                if (head_) throw std::logic_error("validated branch has multiple heads");
                head_ = occurrence;
            } else {
                tails_.push_back(occurrence);
            }
        }
        if (!head_ || tails_.empty()) throw std::logic_error("validated branch fiber is incomplete");
    }

    [[nodiscard]] const BoundaryOccurrenceFiber& fiber() const noexcept { return fiber_; }
    [[nodiscard]] const BoundaryOccurrenceView& head() const noexcept { return *head_; }
    [[nodiscard]] const std::vector<BoundaryOccurrenceView>& tails() const noexcept { return tails_; }
    [[nodiscard]] composite::AspectId head_aspect() const noexcept { return role_aspect_; }
    [[nodiscard]] bool reversed() const noexcept { return !head_->negative; }

    [[nodiscard]] numeric::RationalVector tail_shares() const {
        numeric::RationalVector result;
        result.reserve(tails_.size());
        for (const auto& tail : tails_) {
            numeric::Rational share = tail.coefficient < 0 ? -tail.coefficient : tail.coefficient;
            share.canonicalize();
            result.push_back(std::move(share));
        }
        return result;
    }

    [[nodiscard]] numeric::RationalVector boundary_column() const {
        numeric::RationalVector result;
        result.reserve(1u + tails_.size());
        result.push_back(head_->coefficient);
        for (const auto& tail : tails_) result.push_back(tail.coefficient);
        return result;
    }

private:
    BoundaryOccurrenceFiber fiber_;
    composite::AspectId role_aspect_{};
    std::optional<BoundaryOccurrenceView> head_;
    std::vector<BoundaryOccurrenceView> tails_;
};

} // namespace rfc::core
