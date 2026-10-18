#pragma once

#include "rfc/learning/relational_field_evidence.hpp"
#include "rfc/learning/transport_family_gluing.hpp"

namespace rfc::learning {
// A common publication and explicit reconstructed views declare the interface.
// The owner retains evidence and lineage; external availability remains a
// separate provider obligation.
struct RelationalFieldFamilyInterface {
    std::uint64_t key{}, revision{};
    std::size_t left{}, right{};
    storage::NativeSeriesEndpoint common;
    algebra::RetainedSourceMap common_left, common_right;
    std::vector<std::vector<TransportContextView>> left_views, right_views;
};
struct RelationalFieldFamilyDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalFieldFamilyInterface> interfaces;
};
struct RelationalFieldFamilyCoordinates {
    numeric::RationalVector shift;
    numeric::ExactMatrix map;
};
class RelationalJointFieldResult;
class RelationalJointFieldEvaluation;
class RelationalJointOccurrenceMap;

class PreparedRelationalFieldFamily {
    struct State {
        std::vector<PreparedRelationalFieldEvidence> members;
        RelationalFieldFamilyDeclaration declaration;
        PreparedTransportFamilyGluing gluing;
        std::vector<RelationalFieldFamilyCoordinates> coordinates;
        numeric::ExactActionLimits limits;
        std::size_t retained{};
        State(std::vector<PreparedRelationalFieldEvidence> m, RelationalFieldFamilyDeclaration d,
            PreparedTransportFamilyGluing g, numeric::ExactActionLimits l)
            : members(std::move(m)), declaration(std::move(d)), gluing(std::move(g)), limits(l) {}
    };
public:
    PreparedRelationalFieldFamily(RelationalFieldFamilyDeclaration declaration,
        std::vector<PreparedRelationalFieldEvidence> members, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) {
        using namespace relational_evidence_detail;
        Guard guard(limits, std::move(check)); guard.step();
        require(declaration.key && declaration.revision && !members.empty(), "identified nonempty field family required");
        guard.dimension(members.size()); guard.dimension(declaration.interfaces.size());
        std::vector<PreparedGradedTransportFit> fits;
        std::size_t retained{};
        for (const auto& member : members) {
            guard.step(); same_limits(member.limits(), limits);
            const auto program = member.program(); program_values(program, guard);
            for (const auto& o : program.declaration().occurrences) {
                guard.step();
                if (o.grade == member.state_->fixed->grade)
                    require(o.map.nonzeros() == 0, "field family requires zero original selected-grade Phi");
            }
            // Reuse the already prepared member. No observations are fitted
            // again to convert a field-evidence owner into a transport owner.
            fits.push_back(member.state_->fit);
            const auto& system = member.observation_system();
            retained = add(retained, add(system.design.nonzeros(), add(system.augmented.nonzeros(), system.target.size())));
            guard.entries(retained);
        }
        std::vector<TransportFamilyInterface> interfaces; std::set<std::uint64_t> keys;
        for (const auto& d : declaration.interfaces) {
            guard.step(); require(d.key && d.revision && keys.insert(d.key).second, "field family interface identity");
            interfaces.push_back({d.key, d.revision, d.left, d.right, d.common.endpoint().source,
                d.common_left, d.common_right, d.left_views, d.right_views});
            source_values(d.common.endpoint().source, guard);
        }
        PreparedTransportFamilyGluing gluing(std::move(fits), std::move(interfaces), guard);
        // The existing solver compares realized c*Phi. On a shared variable
        // flag with c=0 that does not compare intrinsic maps. Refuse this scope
        // explicitly; unshared inactive alternatives remain in the family.
        for (const auto& d : declaration.interfaces) for (const bool left : {true, false}) {
            const auto& member = members.at(left ? d.left : d.right);
            const auto k = member.state_->fixed->grade;
            const auto& source = d.common.endpoint().source;
            const auto& map = left ? d.common_left : d.common_right;
            if (k.value >= source.grade_count()) continue;
            for (std::size_t p = 0; p < source.grade(k).incidence_count(); ++p) {
                guard.step(); const auto actual = map.occurrences.at(k.value).at(p);
                const auto coefficient = algebra::owned_gluing_detail::coefficient(source, k, p, guard);
                if (coefficient != 0) continue;
                for (const auto& c : member.declaration().variables) {
                    guard.step(); require(c.occurrence != actual, "shared variable zero/inactive Phi needs an intrinsic comparison contract");
                }
            }
        }
        auto state = std::make_shared<State>(std::move(members), std::move(declaration), std::move(gluing), limits);
        state->retained = add(retained, state->gluing.retained_entries());
        if (state->gluing.consistent()) {
            const auto& offsets = state->gluing.member_free_offsets();
            for (std::size_t i = 0; i < state->members.size(); ++i) {
                const auto first = offsets[i], last = offsets[i + 1];
                const auto& origin = state->gluing.coordinate_origin();
                RelationalFieldFamilyCoordinates c{V(origin.begin() + static_cast<std::ptrdiff_t>(first),
                    origin.begin() + static_cast<std::ptrdiff_t>(last)), Matrix(last - first, state->gluing.coordinate_kernel().cols())};
                for (const auto& [ij, q] : state->gluing.coordinate_kernel().entries()) {
                    guard.step(); if (ij.first >= first && ij.first < last) c.map.set(ij.first - first, ij.second, q);
                }
                state->retained = add(state->retained, add(c.shift.size(), c.map.nonzeros()));
                guard.entries(state->retained); state->coordinates.push_back(std::move(c));
            }
        }
        guard.entries(state->retained); guard.step(); state_ = std::move(state);
    }
    bool same_owner(const PreparedRelationalFieldFamily& other) const noexcept { return state_ == other.state_; }
    numeric::ExactActionLimits limits() const { return state_->limits; }
    const RelationalFieldFamilyDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldFamilyDeclaration& declaration() const && = delete;
    const std::vector<PreparedRelationalFieldEvidence>& members() const & { return state_->members; }
    const std::vector<PreparedRelationalFieldEvidence>& members() const && = delete;
    const PreparedTransportFamilyGluing& gluing() const & { return state_->gluing; }
    const PreparedTransportFamilyGluing& gluing() const && = delete;
    const RelationalFieldFamilyCoordinates& member_coordinates(std::size_t member) const & {
        inference::field_program_detail::require(gluing().consistent(), "inconsistent family has no member coordinates");
        return state_->coordinates.at(member);
    }
    const RelationalFieldFamilyCoordinates& member_coordinates(std::size_t) const && = delete;
    std::size_t retained_entries() const noexcept { return state_->retained; }
    RelationalJointFieldResult execute(std::size_t member, inference::OwnedAccessionFieldResult input,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    RelationalJointOccurrenceMap coefficient_independent_map(std::size_t member, core::Grade grade, std::size_t occurrence,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};

class RelationalJointFieldResult {
    struct State {
        PreparedRelationalFieldFamily family;
        std::size_t member;
        RelationalAffineProgramResult original;
        std::vector<relational_evidence_detail::ResultNode> nodes;
    };
public:
    PreparedRelationalFieldFamily family() const { return state_->family; }
    std::size_t member() const noexcept { return state_->member; }
    RelationalAffineProgramResult original() const { return state_->original; }
    inference::OwnedAccessionFieldResult input() const { return state_->original.input(); }
    const RelationalAffineField& field(std::uint64_t node) const & { return find(node).field; }
    const RelationalAffineField& field(std::uint64_t) const && = delete;
    const RelationalAffineField& occurrence_field(std::uint64_t node) const & {
        const auto& value = find(node); inference::field_program_detail::require(value.occurrences.has_value(), "joint node has no flag field");
        return *value.occurrences;
    }
    const RelationalAffineField& occurrence_field(std::uint64_t) const && = delete;
    bool identified(std::uint64_t node, std::size_t coordinate) const {
        const auto& directions = find(node).field.directions;
        inference::field_program_detail::require(coordinate < directions.rows(), "joint field identification coordinate");
        for (const auto& [ij, q] : directions.entries()) { (void)q; if (ij.first == coordinate) return false; }
        return true;
    }
    RelationalJointFieldEvaluation evaluate(numeric::RationalVector free, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const;
private:
    friend class PreparedRelationalFieldFamily;
    explicit RelationalJointFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    const relational_evidence_detail::ResultNode& find(std::uint64_t key) const {
        const auto it = std::find_if(state_->nodes.begin(), state_->nodes.end(), [&](const auto& n) { return n.node == key; });
        inference::field_program_detail::require(it != state_->nodes.end(), "unknown joint program node"); return *it;
    }
    std::shared_ptr<const State> state_;
};
inline RelationalJointFieldResult PreparedRelationalFieldFamily::execute(std::size_t member,
    inference::OwnedAccessionFieldResult input, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; Guard guard(limits, std::move(check)); same_limits(pin.limits(), limits);
    guard.entries(pin.retained_entries()); const auto& c = pin.member_coordinates(member);
    auto original = pin.members().at(member).execute(std::move(input), limits, [&] { guard.step(); });
    const auto program = pin.members().at(member).program(); std::vector<ResultNode> nodes; std::size_t retained = pin.retained_entries();
    const auto transform = [&](const RelationalAffineField& f) {
        return RelationalAffineField{tensor(sum(f.particular.values().dense(), numeric::checked_apply(f.directions, c.shift, guard), guard), guard),
            numeric::checked_multiply(f.directions, c.map, guard)};
    };
    for (const auto& n : program.declaration().nodes) {
        guard.step(); ResultNode value{n.key, n.output_grade, transform(original.field(n.key)), {}, false};
        if (n.kind == inference::FieldProgramNodeKind::Boundary) value.occurrences = transform(original.occurrence_field(n.key));
        retained = add(retained, add(value.field.particular.values().nonzeros(), value.field.directions.nonzeros()));
        if (value.occurrences) retained = add(retained, add(value.occurrences->particular.values().nonzeros(), value.occurrences->directions.nonzeros()));
        guard.entries(retained); nodes.push_back(std::move(value));
    }
    return RelationalJointFieldResult({pin, member, std::move(original), std::move(nodes)});
}
class RelationalJointFieldEvaluation {
public:
    RelationalJointFieldResult family() const { return family_; }
    inference::OwnedAccessionFieldResult result() const { return result_; }
    const numeric::RationalVector& free_values() const & { return free_; }
    const numeric::RationalVector& free_values() const && = delete;
private:
    friend class RelationalJointFieldResult;
    RelationalJointFieldEvaluation(RelationalJointFieldResult family, inference::OwnedAccessionFieldResult result, numeric::RationalVector free)
        : family_(std::move(family)), result_(std::move(result)), free_(std::move(free)) {}
    RelationalJointFieldResult family_;
    inference::OwnedAccessionFieldResult result_;
    numeric::RationalVector free_;
};
inline RelationalJointFieldEvaluation RelationalJointFieldResult::evaluate(numeric::RationalVector free,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; const auto owner = pin.family(); Guard guard(limits, std::move(check)); same_limits(owner.limits(), limits);
    guard.entries(owner.retained_entries()); const auto values = guard.canonical_input(free); const auto& c = owner.member_coordinates(pin.member());
    const auto local = sum(c.shift, numeric::checked_apply(c.map, values, guard), guard);
    const auto evaluated = pin.original().evaluate(local, limits, [&] { guard.step(); }); const auto result = evaluated.result();
    const auto actual = result.result(); const auto program = actual.program();
    for (const auto& n : program.declaration().nodes) {
        const auto verify = [&](const RelationalAffineField& f, const Tensor& field) {
            guard.step(); require(equal(tensor(sum(f.particular.values().dense(), numeric::checked_apply(f.directions, values, guard), guard), guard), field),
                "joint materialization differs from complete field");
        };
        verify(pin.field(n.key), actual.field(n.key));
        if (n.kind == inference::FieldProgramNodeKind::Boundary) verify(pin.occurrence_field(n.key), actual.occurrence_field(n.key));
    }
    return {pin, result, values};
}
class RelationalJointOccurrenceMap {
public:
    PreparedRelationalFieldFamily family() const { return family_; }
    std::size_t member() const noexcept { return member_; }
    core::Grade grade() const noexcept { return grade_; }
    std::size_t occurrence() const noexcept { return occurrence_; }
    const numeric::ExactMatrix& particular() const & { return particular_; }
    const numeric::ExactMatrix& particular() const && = delete;
    const std::vector<numeric::ExactMatrix>& directions() const & { return directions_; }
    const std::vector<numeric::ExactMatrix>& directions() const && = delete;
private:
    friend class PreparedRelationalFieldFamily;
    RelationalJointOccurrenceMap(PreparedRelationalFieldFamily family, std::size_t member, core::Grade grade, std::size_t occurrence,
        numeric::ExactMatrix particular, std::vector<numeric::ExactMatrix> directions)
        : family_(std::move(family)), member_(member), grade_(grade), occurrence_(occurrence),
          particular_(std::move(particular)), directions_(std::move(directions)) {}
    PreparedRelationalFieldFamily family_;
    std::size_t member_;
    core::Grade grade_;
    std::size_t occurrence_;
    numeric::ExactMatrix particular_;
    std::vector<numeric::ExactMatrix> directions_;
};
inline RelationalJointOccurrenceMap PreparedRelationalFieldFamily::coefficient_independent_map(std::size_t member,
    core::Grade grade, std::size_t occurrence, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_evidence_detail;
    const auto pin = *this; Guard guard(limits, std::move(check)); same_limits(pin.limits(), limits); guard.entries(pin.retained_entries());
    const auto& c = pin.member_coordinates(member);
    const auto original = pin.members().at(member).coefficient_independent_map(grade, occurrence, limits, [&] { guard.step(); });
    auto p = original.particular(); const auto& local = original.directions();
    std::vector<Matrix> directions(c.map.cols(), Matrix(p.rows(), p.cols()));
    std::size_t retained = add(pin.retained_entries(), p.nonzeros()); guard.entries(add(retained, directions.size()));
    const auto accumulate = [&](Matrix& target, const Matrix& source, const numeric::Rational& scale) {
        if (scale == 0) return;
        for (const auto& [ij, q] : source.entries()) {
            guard.step(); const bool existed = target.get(ij.first, ij.second) != 0;
            const auto value = guard.add(target.get(ij.first, ij.second), guard.mul(q, scale));
            target.set(ij.first, ij.second, value);
            if (!existed && value != 0) retained = add(retained, 1);
            if (existed && value == 0) --retained;
            guard.entries(add(retained, directions.size()));
        }
    };
    for (std::size_t i = 0; i < c.shift.size(); ++i) { guard.step(); accumulate(p, local.at(i), c.shift[i]); }
    for (const auto& [ij, q] : c.map.entries()) { guard.step(); accumulate(directions.at(ij.second), local.at(ij.first), q); }
    return {pin, member, grade, occurrence, std::move(p), std::move(directions)};
}
} // namespace rfc::learning
