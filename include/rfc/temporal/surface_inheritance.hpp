#pragma once
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/temporal/surface_holonomy.hpp"

namespace rfc::temporal {
// Exact theorem certificate. Finite phase observations are separate Sample
// objects; convergence of a finite set never grants this authority.
class PreparedSurfaceInheritance {
    using Q = surface_holonomy_detail::Q;
    using I = surface_holonomy_detail::I;
    using V = surface_holonomy_detail::V;
    using M = surface_holonomy_detail::M;
    using G = surface_holonomy_detail::G;
    struct State {
        PreparedSurfaceHolonomy holonomy;
        field::PreparedHodgeFrames base;
        numeric::PreparedExactFrames closed;
        M fan_cup, square_cup;
        V harmonic_chi, harmonic_psi;
        std::array<M, 3> frames;
        std::array<field::hodge_frame_detail::Projection, 3> projections;
        std::array<std::size_t, 3> dimensions;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedSurfaceInheritance(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static M append(const M &a, const V &x, G &g) {
        if (a.rows() != x.size())
            throw std::logic_error("surface inherited frame carrier");
        M b(a.rows(), a.cols() + 1, a.entries());
        for (std::size_t i = 0; i < x.size(); ++i) {
            g.step();
            b.set(i, a.cols(), x[i]);
        }
        return b;
    }
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.holonomy.validate_receiving(g);
        s.base.validate(g);
        s.closed.validate(g);
        g.vector(s.harmonic_chi);
        g.vector(s.harmonic_psi);
        g.matrix(s.fan_cup);
        g.matrix(s.square_cup);
        for (const auto &m : s.frames)
            g.matrix(m);
        for (const auto &p : s.projections)
            p.validate(g);
    }

  public:
    static PreparedSurfaceInheritance prepare(PreparedSurfaceHolonomy holonomy, G &guard) {
        using namespace surface_holonomy_detail;
        holonomy.validate_receiving(guard);
        const auto n = holonomy.surface().work().edges;
        const auto retained = add(holonomy.state_->retained, mul(256, mul(add(n, 1), add(n, 1))));
        auto g = phase(guard, retained);
        auto original =
            std::shared_ptr<const core::RelationalComplex>(holonomy.state_, &holonomy.surface().source());
        std::vector<M> metrics;
        for (std::uint16_t k = 0; k < 3; ++k)
            metrics.push_back(M::identity(holonomy.surface().source().grade({k}).cell_count()));
        field::PreparedHodgeFrames base(original, std::move(metrics), {1}, g);
        auto b1 = field::hodge_frame_detail::boundary(*original, {1}, g),
             b2 = field::hodge_frame_detail::boundary(*original, {2}, g);
        numeric::PreparedExactFrames closed(b2.transpose(), g);
        const auto &C = closed.kernel();
        V chi;
        for (const auto &z : holonomy.cocycle().values())
            chi.emplace_back(z);
        const auto &psi = holonomy.psi();
        if (!numeric::is_zero(numeric::checked_apply(b1, psi, g)))
            throw std::logic_error("differentiated fundamental face sum is not a cycle");
        const auto &faces = original->grade({2});
        M fan(n, n), square(n, n);
        for (std::size_t f = 0; f < faces.cell_count(); ++f) {
            const auto [begin, end] = faces.incidence_range(f);
            const Q F{holonomy.surface().fundamental_cycle().at(f)};
            for (std::size_t j = begin; j < end; ++j)
                for (std::size_t i = begin; i < j; ++i) {
                    const auto a = faces.lower_index[i], b = faces.lower_index[j];
                    auto q = g.mul(g.mul(F, Q{1, 2}),
                                   Q{faces.lattice_coefficient[i] * faces.lattice_coefficient[j]});
                    fan.set(a, b, g.add(fan.get(a, b), q));
                    fan.set(b, a, g.sub(fan.get(b, a), q));
                }
        }
        for (const auto &term : holonomy.surface().cup_terms()) {
            const auto i = term.first_slot, j = term.second_slot;
            auto q = g.mul(Q{holonomy.surface().fundamental_cycle().at(term.face)}, term.coefficient);
            q = g.mul(q, Q{faces.lattice_coefficient[i] * faces.lattice_coefficient[j]});
            const auto a = faces.lower_index[i], b = faces.lower_index[j];
            square.set(a, b, g.add(square.get(a, b), q));
        }
        auto expected = numeric::checked_apply(fan.transpose(), chi, g);
        if (numeric::checked_apply(C.transpose(), psi, g) !=
            numeric::checked_apply(C.transpose(), expected, g))
            throw std::logic_error("full closed-cochain cup/psi duality failed");
        const auto fan_form =
                       numeric::checked_multiply(C.transpose(), numeric::checked_multiply(fan, C, g), g),
                   square_form =
                       numeric::checked_multiply(C.transpose(), numeric::checked_multiply(square, C, g), g);
        if (fan_form != square_form)
            throw std::logic_error(
                "declared cellular cup and oriented fan disagree on complete closed cochains");
        auto hc = base.project_sector(2, chi, g), hp = base.project_sector(2, psi, g);
        auto cc = dot(hc, hc, g), pp = dot(hp, hp, g);
        if (cc <= 0 || pp <= 0 || dot(hc, hp, g) != 0 || dot(chi, psi, g) != 0)
            throw std::logic_error("surface inherited harmonic directions are not nonzero orthogonal duals");
        const auto &H = base.harmonic_frame();
        M constraints(2, H.cols());
        for (std::size_t j = 0; j < H.cols(); ++j) {
            auto h = column(H, j, g);
            constraints.set(0, j, dot(hc, h, g));
            constraints.set(1, j, dot(hp, h, g));
        }
        numeric::PreparedExactFrames complement(constraints, g);
        if (complement.rank() != 2)
            throw std::logic_error("surface two inherited harmonic directions are not independent");
        std::array<M, 3> frames{append(base.gradient_frame(), hc, g), append(base.curl_frame(), hp, g),
                                numeric::checked_multiply(H, complement.kernel(), g)};
        const auto genus = holonomy.surface().work().genus;
        std::array<std::size_t, 3> dims{
            {holonomy.surface().work().vertices, holonomy.surface().work().faces, 2 * genus - 2}};
        std::size_t sum = 0;
        for (std::size_t i = 0; i < 3; ++i) {
            numeric::PreparedExactFrames rank(frames[i], g);
            if (rank.rank() != dims[i] || rank.rank() != frames[i].cols())
                throw std::logic_error("surface complete inherited sector dimensions");
            sum = add(sum, rank.rank());
            for (std::size_t j = 0; j < i; ++j)
                if (numeric::checked_multiply(frames[j].transpose(), frames[i], g).nonzeros())
                    throw std::logic_error("surface limit sectors are not orthogonal");
        }
        if (sum != n || H.cols() != 2 * genus)
            throw std::logic_error("surface inherited sectors omit directions");
        const auto unit = M::identity(n);
        std::array<field::hodge_frame_detail::Projection, 3> projections{
            {field::hodge_frame_detail::Projection(frames[0], unit, g),
             field::hodge_frame_detail::Projection(frames[1], unit, g),
             field::hodge_frame_detail::Projection(frames[2], unit, g)}};
        guard.step();
        return PreparedSurfaceInheritance(std::make_shared<const State>(
            State{std::move(holonomy), std::move(base), std::move(closed), std::move(fan), std::move(square),
                  std::move(hc), std::move(hp), std::move(frames), std::move(projections), dims, retained}));
    }
    const PreparedSurfaceHolonomy &holonomy() const & noexcept {
        return state_->holonomy;
    }
    const PreparedSurfaceHolonomy &holonomy() const && = delete;
    const V &harmonic_chi() const & noexcept {
        return state_->harmonic_chi;
    }
    const V &harmonic_chi() const && = delete;
    const V &harmonic_psi() const & noexcept {
        return state_->harmonic_psi;
    }
    const V &harmonic_psi() const && = delete;
    const numeric::PreparedExactFrames &closed_cochains() const & noexcept {
        return state_->closed;
    }
    const numeric::PreparedExactFrames &closed_cochains() const && = delete;
    const std::array<M, 3> &frames() const & noexcept {
        return state_->frames;
    }
    const std::array<M, 3> &frames() const && = delete;
    const std::array<std::size_t, 3> &dimensions() const & noexcept {
        return state_->dimensions;
    }
    const std::array<std::size_t, 3> &dimensions() const && = delete;
    const M &fan_cup() const & noexcept {
        return state_->fan_cup;
    }
    const M &fan_cup() const && = delete;
    const M &cellular_cup() const & noexcept {
        return state_->square_cup;
    }
    const M &cellular_cup() const && = delete;
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    PreparedSurfaceHolonomy::Sectors decompose(PreparedSurfaceHolonomy::Field field, G &guard) const {
        using namespace surface_holonomy_detail;
        if (field.owner_ != state_->holonomy.state_)
            throw std::invalid_argument("surface limit field has a foreign actual owner");
        validate(*state_, guard);
        auto g = phase(guard, add(state_->retained, mul(32, field.values().size())));
        g.vector(field.values());
        auto re = part(field.values(), 0, g), im = part(field.values(), 1, g);
        std::array<V, 3> values;
        for (std::size_t i = 0; i < 3; ++i)
            values[i] =
                lift(state_->projections[i].apply(re, g).first, state_->projections[i].apply(im, g).first, g);
        guard.step();
        return PreparedSurfaceHolonomy::Sectors(state_, std::move(field), std::move(values), g);
    }
};
} // namespace rfc::temporal
