#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <array>
#include <memory>
#include <set>

namespace rfc::field {

enum class RelationalChannel : std::uint8_t { Topology, RawGeometry, Frustration, Coparticipation };
enum class CoparticipationReading : std::uint8_t { Share, Count };

struct RelationalChannelResponse {
    std::array<numeric::RationalVector, 4> fields;
    std::array<numeric::Rational, 4> energies;
    std::size_t steps{};
};

// Euclidean scalar forms on the retained grade-one carrier. Raw G is the
// unsigned weighted Gram. F always uses raw G, including signed weights.
class PreparedRelationalChannels {
    using Q = numeric::Rational;
    using V = numeric::RationalVector;
    using Guard = numeric::ExactActionGuard;
    std::shared_ptr<const core::RelationalComplex> source_;
    V weights_, signed_, participation_, coparticipation_mass_;
    std::array<V, 4> diagonals_;
    std::array<Q, 4> traces_;
    CoparticipationReading reading_;
    numeric::ExactActionLimits limits_;
    std::size_t retained_{}, preparation_steps_{};

    static std::size_t index(RelationalChannel channel) {
        switch (channel) {
            case RelationalChannel::Topology: return 0;
            case RelationalChannel::RawGeometry: return 1;
            case RelationalChannel::Frustration: return 2;
            case RelationalChannel::Coparticipation: return 3;
        }
        throw std::invalid_argument("unknown relational channel");
    }
    static Q magnitude(const Q& q) { return q < 0 ? Q{-q} : q; }
    void workspace(std::size_t upper_vectors, std::size_t lower_vectors, Guard& guard) const {
        const auto slots = machine::checked_size_add(retained_, machine::checked_size_add(
            machine::checked_size_mul(size(), upper_vectors),
            machine::checked_size_mul(lower_size(), lower_vectors)));
        guard.entries(slots);
    }
    RelationalChannelResponse execute(const V& input, bool normalized, Guard& guard) const {
        if (input.size() != size()) throw std::invalid_argument("relational channel carrier size mismatch");
        workspace(5, 3, guard);
        const auto x = guard.canonical_input(input);
        V t(lower_size()), g(lower_size()), c(lower_size());
        const auto& arena = source_->grade(core::Grade{1});
        for (std::size_t p = 0; p < signed_.size(); ++p) {
            guard.step();
            const auto row = arena.lower_index[p], column = arena.upper_index[p];
            if (signed_[p] != 0) {
                const auto weighted = guard.mul(weights_[column], x[column]);
                t[row] = guard.add(t[row], guard.mul(signed_[p], weighted));
                g[row] = guard.add(g[row], guard.mul(magnitude(signed_[p]), weighted));
                c[row] = guard.add(c[row], guard.mul(participation_[p], x[column]));
            }
        }
        RelationalChannelResponse out;
        for (auto& field : out.fields) field.resize(size());
        for (std::size_t p = 0; p < signed_.size(); ++p) {
            guard.step();
            const auto row = arena.lower_index[p], column = arena.upper_index[p];
            if (signed_[p] != 0) {
                const auto weighted = guard.mul(weights_[column], signed_[p]);
                out.fields[0][column] = guard.add(out.fields[0][column], guard.mul(weighted, t[row]));
                out.fields[1][column] = guard.add(out.fields[1][column],
                    guard.mul(guard.mul(weights_[column], magnitude(signed_[p])), g[row]));
                out.fields[3][column] = guard.sub(out.fields[3][column], guard.mul(participation_[p], c[row]));
            }
        }
        for (std::size_t i = 0; i < size(); ++i) {
            out.fields[2][i] = guard.add(guard.sub(out.fields[0][i], out.fields[1][i]),
                guard.mul(diagonals_[2][i], x[i]));
            out.fields[3][i] = guard.add(out.fields[3][i], guard.mul(coparticipation_mass_[i], x[i]));
            for (std::size_t k = 0; k < 4; ++k) {
                if (normalized && traces_[k] > 0) out.fields[k][i] = guard.div(out.fields[k][i], traces_[k]);
                out.energies[k] = guard.add(out.energies[k], guard.mul(x[i], out.fields[k][i]));
            }
        }
        out.steps = guard.steps();
        return out;
    }
public:
    PreparedRelationalChannels(std::shared_ptr<const core::RelationalComplex> source, V weights,
        CoparticipationReading reading = CoparticipationReading::Share,
        numeric::ExactActionLimits limits = {}, std::function<void()> checkpoint = {})
        : source_(std::move(source)), reading_(reading), limits_(limits) {
        if (!source_ || source_->grade_count() < 2) throw std::invalid_argument("grade-one relational source required");
        if (reading != CoparticipationReading::Share && reading != CoparticipationReading::Count)
            throw std::invalid_argument("unknown coparticipation reading");
        Guard guard(limits_, std::move(checkpoint));
        guard.dimension(size()); guard.dimension(lower_size());
        if (weights.size() != size()) throw std::invalid_argument("complete original relation weights required");
        const auto& arena = source_->grade(core::Grade{1});
        retained_ = machine::checked_size_add(machine::checked_size_mul(arena.incidence_count(), 2),
            machine::checked_size_add(machine::checked_size_mul(size(), 6), 4));
        workspace(1, 3, guard);
        weights_ = guard.canonical_input(weights);
        signed_.resize(arena.incidence_count()); participation_.resize(arena.incidence_count());
        coparticipation_mass_.resize(size());
        for (auto& diagonal : diagonals_) diagonal.resize(size());
        V positive(lower_size()), negative(lower_size()), c_mass(lower_size());
        const auto exists = source_->signature().existence_aspect();
        for (std::size_t i = 0; i < size(); ++i) {
            const auto [begin, end] = arena.incidence_range(i);
            std::set<std::uint32_t> participants;
            for (auto p = begin; p < end; ++p) {
                guard.step();
                const auto row = arena.lower_index[p];
                if (!participants.insert(row).second)
                    throw std::invalid_argument("relational channels require distinct participants per relation");
                guard.entries(machine::checked_size_add(retained_, machine::checked_size_add(
                    machine::checked_size_mul(lower_size(), 3), participants.size())));
                if (!arena.aspects.get(exists, p)) continue;
                const auto& lower_scale = source_->grade(core::Grade{0}).cell_scales[row];
                const auto& upper_scale = arena.cell_scales[i];
                const Q lattice{arena.lattice_coefficient[p]};
                guard.value(lower_scale); guard.value(upper_scale); guard.value(lattice);
                const auto q = guard.div(guard.mul(lower_scale, lattice), upper_scale);
                signed_[p] = q;
                if (q == 0) continue;
                const auto a = magnitude(q);
                participation_[p] = reading_ == CoparticipationReading::Share ? a : Q{1};
                auto& oriented = q < 0 ? negative[row] : positive[row];
                oriented = guard.add(oriented, guard.mul(a, magnitude(weights_[i])));
                c_mass[row] = guard.add(c_mass[row], participation_[p]);
                const auto weighted = guard.mul(weights_[i], q);
                diagonals_[0][i] = guard.add(diagonals_[0][i], guard.mul(weighted, weighted));
            }
            diagonals_[1][i] = diagonals_[0][i];
        }
        for (std::size_t p = 0; p < signed_.size(); ++p) {
            guard.step(); if (signed_[p] == 0) continue;
            const auto row = arena.lower_index[p], column = arena.upper_index[p];
            const auto& opposite = signed_[p] < 0 ? positive[row] : negative[row];
            diagonals_[2][column] = guard.add(diagonals_[2][column], guard.mul(Q{2},
                guard.mul(guard.mul(magnitude(weights_[column]), magnitude(signed_[p])), opposite)));
            diagonals_[3][column] = guard.add(diagonals_[3][column],
                guard.mul(participation_[p], guard.sub(c_mass[row], participation_[p])));
            coparticipation_mass_[column] = guard.add(coparticipation_mass_[column],
                guard.mul(participation_[p], c_mass[row]));
        }
        for (std::size_t k = 0; k < 4; ++k)
            for (const auto& q : diagonals_[k]) traces_[k] = guard.add(traces_[k], q);
        preparation_steps_ = guard.steps();
    }
    PreparedRelationalChannels(core::RelationalComplex source, V weights,
        CoparticipationReading reading = CoparticipationReading::Share,
        numeric::ExactActionLimits limits = {}, std::function<void()> checkpoint = {})
        : PreparedRelationalChannels(std::make_shared<const core::RelationalComplex>(std::move(source)),
            std::move(weights), reading, limits, std::move(checkpoint)) {}
    [[nodiscard]] std::size_t size() const { return source_->grade(core::Grade{1}).cell_count(); }
    [[nodiscard]] std::size_t lower_size() const { return source_->grade(core::Grade{0}).cell_count(); }
    [[nodiscard]] std::size_t retained_entries() const noexcept { return retained_; }
    [[nodiscard]] std::size_t preparation_steps() const noexcept { return preparation_steps_; }
    [[nodiscard]] CoparticipationReading reading() const noexcept { return reading_; }
    [[nodiscard]] const core::RelationalComplex& source() const & { return *source_; }
    const core::RelationalComplex& source() const && = delete;
    [[nodiscard]] const V& weights() const & { return weights_; }
    const V& weights() const && = delete;
    [[nodiscard]] const V& realized_incidence() const & { return signed_; }
    const V& realized_incidence() const && = delete;
    [[nodiscard]] const std::array<V, 4>& diagonals() const & { return diagonals_; }
    const std::array<V, 4>& diagonals() const && = delete;
    [[nodiscard]] const std::array<Q, 4>& traces() const & { return traces_; }
    const std::array<Q, 4>& traces() const && = delete;
    [[nodiscard]] RelationalChannelResponse apply_all(const V& input, bool trace_normalized = false,
        std::function<void()> checkpoint = {}) const {
        Guard guard(limits_, std::move(checkpoint)); return execute(input, trace_normalized, guard);
    }
    [[nodiscard]] V apply(RelationalChannel channel, const V& input, bool trace_normalized = false,
        std::function<void()> checkpoint = {}) const {
        const auto k = index(channel);
        Guard guard(limits_, std::move(checkpoint));
        if (input.size() != size()) throw std::invalid_argument("relational channel carrier size mismatch");
        workspace(2, k == 2 ? 2 : 1, guard);
        const auto x = guard.canonical_input(input);
        V rows(lower_size()), opposite(k == 2 ? lower_size() : 0), out(size());
        const auto& arena = source_->grade(core::Grade{1});
        for (std::size_t p = 0; p < signed_.size(); ++p) {
            guard.step(); if (signed_[p] == 0) continue;
            const auto row = arena.lower_index[p], column = arena.upper_index[p];
            const auto coefficient = k == 3 ? participation_[p] :
                guard.mul(weights_[column], k == 0 ? signed_[p] : magnitude(signed_[p]));
            auto& target = k == 2 && signed_[p] < 0 ? opposite[row] : rows[row];
            target = guard.add(target, guard.mul(coefficient, x[column]));
        }
        for (std::size_t p = 0; p < signed_.size(); ++p) {
            guard.step(); if (signed_[p] == 0) continue;
            const auto row = arena.lower_index[p], column = arena.upper_index[p];
            const auto coefficient = k == 3 ? participation_[p] :
                guard.mul(weights_[column], k == 0 ? signed_[p] : magnitude(signed_[p]));
            if (k == 2) {
                const auto& value = signed_[p] < 0 ? rows[row] : opposite[row];
                out[column] = guard.sub(out[column], guard.mul(Q{2}, guard.mul(coefficient,value)));
            } else {
                const auto value = guard.mul(coefficient,rows[row]);
                out[column] = k == 3 ? guard.sub(out[column],value) : guard.add(out[column],value);
            }
        }
        for (std::size_t i = 0; i < size(); ++i) {
            if (k == 2 || k == 3) out[i] = guard.add(out[i],
                guard.mul(k == 2 ? diagonals_[2][i] : coparticipation_mass_[i],x[i]));
            if (trace_normalized && traces_[k] > 0) out[i] = guard.div(out[i],traces_[k]);
        }
        return out;
    }
};
} // namespace rfc::field
