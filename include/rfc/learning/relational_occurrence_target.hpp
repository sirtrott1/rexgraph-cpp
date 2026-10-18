#pragma once

#include "rfc/inference/relational_field_interface.hpp"
#include <array>

namespace rfc::learning {
struct RelationalOccurrenceTargetCoordinate {
    std::uint64_t key{};
    storage::StableOccurrenceKey occurrence;
    std::uint64_t basis{}, direction{};
};
struct RelationalOccurrenceTargetDeclaration {
    std::uint64_t key{}, revision{}, node{}, frame{};
    std::vector<RelationalOccurrenceTargetCoordinate> coordinates;
    numeric::ExactMatrix accession, reconstruction;
};
namespace occurrence_target_detail {
using namespace inference::field_interface_detail;
inline void program_values(const inference::OwnedRelationalFieldProgram &program, Guard &guard) {
    const auto p = program.publication();
    source_values(p.endpoint().source, guard);
    std::size_t retained = program.declaration().nodes.size();
    for (const auto &s : program.declaration().stalks) {
        guard.matrix(s.metric);
        retained = add(retained, add(s.metric.nonzeros(), s.directions.size()));
    }
    for (const auto &o : program.declaration().occurrences) {
        guard.matrix(o.map);
        retained = add(retained, add(3, o.map.nonzeros()));
    }
    for (const auto &n : program.declaration().nodes) {
        guard.matrix(n.map);
        retained = add(retained, n.map.nonzeros());
    }
    guard.entries(retained);
}
} // namespace occurrence_target_detail

// Coordinates retain actual flags and stalk bases. A/R reconstruct the selected
// coordinate carrier, not the complementary original occurrence field.
class OwnedRelationalOccurrenceTargetPort {
    using M = numeric::ExactMatrix;
    using V = numeric::RationalVector;
    using Guard = numeric::ExactActionGuard;
    struct State {
        inference::OwnedRelationalFieldProgram program;
        RelationalOccurrenceTargetDeclaration declaration;
        M selection, action, coordinate_metric, represented_metric;
        std::vector<std::size_t> positions;
        std::size_t retained{};
    };

  public:
    OwnedRelationalOccurrenceTargetPort(inference::OwnedRelationalFieldProgram program,
                                        RelationalOccurrenceTargetDeclaration declaration,
                                        numeric::ExactActionLimits limits = {},
                                        std::function<void()> check = {}) {
        using namespace occurrence_target_detail;
        Guard guard(limits, std::move(check));
        const auto &d = declaration;
        program_values(program, guard);
        const auto &n = node(program, d.node);
        require(d.key && d.revision && d.frame && !d.coordinates.empty() &&
                    n.kind == inference::FieldProgramNodeKind::Boundary,
                "occurrence target port requires identity, original coordinates and a Boundary node");
        const auto g = node(program, *n.input).output_grade;
        const auto publication = program.publication();
        const auto &source = publication.endpoint().source;
        const auto &arena = source.grade(g);
        const auto fibers = program.fibers();
        const auto &stalks = fibers.fibers().stalks();
        guard.dimension(d.coordinates.size());
        guard.dimension(machine::checked_size_add(arena.incidence_count(), 1));
        std::size_t retained =
            add(machine::checked_size_mul(d.coordinates.size(), 7), add(arena.incidence_count(), 1));
        guard.entries(retained);
        std::vector<std::size_t> offsets{0};
        for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
            guard.step();
            offsets.push_back(add(offsets.back(), stalks.dimension({n.output_grade, arena.lower_index[p]})));
            guard.dimension(offsets.back());
        }
        const storage::StableSourceIndex index(source);
        std::set<std::uint64_t> keys;
        std::set<std::size_t> selected;
        std::vector<std::size_t> positions, flags, axes;
        std::vector<const inference::StalkBasisDeclaration *> bases;
        M selection(d.coordinates.size(), offsets.back()), metric(d.coordinates.size(), d.coordinates.size());
        for (std::size_t i = 0; i < d.coordinates.size(); ++i) {
            guard.step();
            const auto &coordinate = d.coordinates[i];
            require(coordinate.key && keys.insert(coordinate.key).second &&
                        coordinate.occurrence.upper_grade == g,
                    "target coordinates need distinct identities at their actual original grade");
            const auto flag = index.occurrence(coordinate.occurrence);
            require(flag.has_value(), "target occurrence is absent from actual source");
            const core::CellId lower{n.output_grade, arena.lower_index[*flag]};
            const auto &declarations = program.declaration().stalks;
            const auto basis = std::find_if(declarations.begin(), declarations.end(),
                                            [&](const auto &s) { return s.cell == lower; });
            require(basis != declarations.end() && basis->basis == coordinate.basis,
                    "target coordinate names a foreign original stalk basis");
            const auto axis =
                std::find(basis->directions.begin(), basis->directions.end(), coordinate.direction);
            require(axis != basis->directions.end(), "target direction is absent from actual stalk basis");
            const auto slot = static_cast<std::size_t>(axis - basis->directions.begin()),
                       position = add(offsets[*flag], slot);
            require(selected.insert(position).second, "target port repeats an original flag coordinate");
            selection.set(i, position, 1);
            positions.push_back(position);
            flags.push_back(*flag);
            axes.push_back(slot);
            bases.push_back(&*basis);
        }
        for (std::size_t i = 0; i < d.coordinates.size(); ++i)
            for (std::size_t j = 0; j < d.coordinates.size(); ++j) {
                guard.step();
                if (flags[i] == flags[j])
                    metric.set(i, j, bases[i]->metric.get(axes[i], axes[j]));
            }
        guard.matrix(d.accession);
        guard.matrix(d.reconstruction);
        require(d.accession.cols() == d.coordinates.size() &&
                    d.reconstruction.rows() == d.coordinates.size() &&
                    d.accession.rows() == d.reconstruction.cols(),
                "target A/R must cover the complete selected coordinate carrier");
        require(numeric::checked_multiply(d.reconstruction, d.accession, guard) ==
                    M::identity(d.coordinates.size()),
                "target reconstruction must retain every selected original coordinate");
        field::hodge_frame_detail::positive(metric, guard);
        auto action = numeric::checked_multiply(d.accession, selection, guard);
        auto represented = numeric::checked_multiply(
            numeric::checked_multiply(d.reconstruction.transpose(), metric, guard), d.reconstruction, guard);
        for (const auto *matrix : std::array<const M *, 6>{&selection, &metric, &action, &represented,
                                                           &d.accession, &d.reconstruction}) {
            guard.matrix(*matrix);
            retained = add(retained, matrix->nonzeros());
            guard.entries(retained);
        }
        guard.step();
        state_ = std::make_shared<const State>(
            State{std::move(program), std::move(declaration), std::move(selection), std::move(action),
                  std::move(metric), std::move(represented), std::move(positions), retained});
    }
    inference::OwnedRelationalFieldProgram program() const {
        return state_->program;
    }
    bool same_owner(const OwnedRelationalOccurrenceTargetPort &other) const noexcept {
        return state_ == other.state_;
    }
    const RelationalOccurrenceTargetDeclaration &declaration() const & {
        return state_->declaration;
    }
    const RelationalOccurrenceTargetDeclaration &declaration() const && = delete;
    const M &selection() const & {
        return state_->selection;
    }
    const M &selection() const && = delete;
    const M &action() const & {
        return state_->action;
    }
    const M &action() const && = delete;
    const M &coordinate_metric() const & {
        return state_->coordinate_metric;
    }
    const M &coordinate_metric() const && = delete;
    const M &represented_metric() const & {
        return state_->represented_metric;
    }
    const M &represented_metric() const && = delete;
    std::size_t retained_entries() const noexcept {
        return state_->retained;
    }
    void validate(Guard &guard) const {
        const auto pin = state_;
        occurrence_target_detail::program_values(pin->program, guard);
        guard.entries(pin->retained);
        for (const auto *m :
             {&pin->selection, &pin->action, &pin->coordinate_metric, &pin->represented_metric,
              &pin->declaration.accession, &pin->declaration.reconstruction})
            guard.matrix(*m);
    }
    std::vector<std::size_t> observed_indices(const std::vector<std::uint64_t> &observed,
                                              Guard &guard) const {
        const auto pin = state_;
        validate(guard);
        guard.dimension(observed.size());
        guard.entries(inference::field_interface_detail::add(pin->retained, observed.size()));
        inference::field_program_detail::require(
            !observed.empty(), "missing target coordinates contribute no observation append");
        std::set<std::uint64_t> seen;
        std::vector<std::size_t> out;
        for (const auto key : observed) {
            guard.step();
            const auto &coordinates = pin->declaration.coordinates;
            const auto found = std::find_if(coordinates.begin(), coordinates.end(),
                                            [&](const auto &c) { return c.key == key; });
            inference::field_program_detail::require(found != coordinates.end() && seen.insert(key).second,
                                                     "observed target coordinate absent or repeated");
            out.push_back(static_cast<std::size_t>(found - coordinates.begin()));
        }
        return out;
    }
    M observation_map(const std::vector<std::size_t> &observed, Guard &guard) const {
        const auto pin = state_;
        validate(guard);
        guard.dimension(observed.size());
        M out(observed.size(), pin->selection.cols());
        for (std::size_t row = 0; row < observed.size(); ++row) {
            guard.step();
            inference::field_program_detail::require(observed[row] < pin->positions.size(),
                                                     "observed port coordinate index");
            out.set(row, pin->positions[observed[row]], 1);
            guard.entries(inference::field_interface_detail::add(pin->retained, out.nonzeros()));
        }
        return out;
    }
    V target_coordinates(const numeric::SparseRationalTensor &target,
                         const std::vector<std::size_t> &observed, Guard &guard) const {
        using namespace occurrence_target_detail;
        const auto pin = state_;
        validate(guard);
        admit_field(target, pin->declaration.accession.rows(), guard);
        const auto y = target.values().dense();
        auto coordinates = numeric::checked_apply(pin->declaration.reconstruction, y, guard);
        require(numeric::checked_apply(pin->declaration.accession, coordinates, guard) == y,
                "target is outside declared represented image");
        std::set<std::size_t> seen;
        for (const auto i : observed) {
            guard.step();
            require(i < coordinates.size() && seen.insert(i).second,
                    "target mask coordinate absent or repeated");
        }
        // The supplied tensor is a complete admitted representative. Only
        // named original coordinates constrain the model; other values remain
        // retained data and never become inferred zero observations.
        return coordinates;
    }

  private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::learning
