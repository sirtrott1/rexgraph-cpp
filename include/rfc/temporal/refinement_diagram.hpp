#pragma once
#include "rfc/core/owned_source.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/geometry/chain_map.hpp"
#include "rfc/temporal/transfer.hpp"
#include <span>

namespace rfc::temporal {
struct RefinementRevision {
    std::uint64_t key{}, revision{};
};
struct RefinementLimits {
    std::size_t max_grades{4}, max_cells{32}, max_occurrences{512}, max_terms{1024};
    std::size_t max_levels{8}, max_maps{16}, max_paths{256}, max_depth{16};
};
namespace refinement_detail {
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
inline std::size_t add(std::size_t a, std::size_t b) {
    return machine::checked_size_add(a, b);
}
inline std::size_t mul(std::size_t a, std::size_t b) {
    return machine::checked_size_mul(a, b);
}
inline G phase(G &g, std::size_t retained) {
    g.entries(retained);
    auto l = g.limits();
    l.entries -= retained;
    return G(l, [&g] { g.step(); });
}
inline M mm(const M &a, const M &b, G &g) {
    return numeric::checked_multiply(a, b, g);
}
inline V mv(const M &a, const V &b, G &g) {
    return numeric::checked_apply(a, b, g);
}
inline M sum(const M &a, const M &b, G &g) {
    return field::update_detail::add(a, b, g);
}
inline V sum(const V &a, const V &b, G &g) {
    return field::update_detail::add(a, b, g);
}
inline M sub(const M &a, const M &b, G &g) {
    return sum(a, field::update_detail::scale(b, Q{-1}, g), g);
}
inline V sub(const V &a, const V &b, G &g) {
    return field::update_detail::sub(a, b, g);
}
inline Q dot(const V &a, const V &b, G &g) {
    if (a.size() != b.size())
        throw std::invalid_argument("refinement pairing carrier");
    Q q;
    for (std::size_t i = 0; i < a.size(); ++i)
        q = g.add(q, g.mul(a[i], b[i]));
    return q;
}
inline Q inner(const V &a, const M &m, const V &b, G &g) {
    return dot(a, mv(m, b, g), g);
}
inline void limits(RefinementLimits l) {
    if (l.max_grades > 8 || l.max_cells > 64 || l.max_occurrences > 2048 || l.max_terms > 4096 ||
        l.max_levels > 16 || l.max_maps > 64 || l.max_paths > 2048 || l.max_depth > 32)
        throw std::length_error("refinement admitted finite profile");
}
inline void revision(RefinementRevision r) {
    if (!r.key || !r.revision)
        throw std::invalid_argument("refinement declaration requires key and revision");
}
template <std::integral T>
    requires(!std::same_as<T, bool>)
inline std::size_t index(T i, std::size_t n) {
    if (!std::in_range<std::size_t>(i) || static_cast<std::size_t>(i) >= n)
        throw std::out_of_range("refinement position");
    return static_cast<std::size_t>(i);
}
inline std::size_t source_words(const core::RelationalComplex &s, G &g) {
    std::size_t out = 8;
    g.dimension(s.grade_count());
    for (std::size_t k = 0; k < s.grade_count(); ++k) {
        const auto &a = s.grade({static_cast<std::uint16_t>(k)});
        g.dimension(a.cell_count());
        out = add(out, add(mul(16, a.incidence_count()), mul(8, a.cell_count())));
        out =
            add(out, mul(a.aspects.aspect_count(), machine::PackedBits::words_for_size(a.incidence_count())));
        g.entries(out);
        for (const auto &q : a.cell_scales) {
            g.step();
            g.value(q);
        }
        for (const auto &z : a.lattice_coefficient) {
            g.step();
            g.value(Q{z});
        }
    }
    for (std::size_t i = 0; i < s.signature().type_count(); ++i) {
        g.step();
        const auto &t = s.signature().type(static_cast<core::RelationTypeId>(i));
        out = add(out, add(t.name.size(), mul(8, add(t.constraints.rules.size(), 1))));
        g.entries(out);
        for (const auto &r : t.constraints.rules) {
            g.step();
            g.value(r.value);
        }
    }
    for (std::size_t i = 0; i < s.signature().aspects().size(); ++i) {
        g.step();
        out = add(out, add(8, s.signature().aspects().at(static_cast<composite::AspectId>(i)).name.size()));
        g.entries(out);
    }
    return out;
}
inline M columns(const numeric::PreparedExactSolve &s, const M &b, G &g) {
    return field::hodge_frame_detail::solve_columns(s, b, g);
}
} // namespace refinement_detail

class RefinementField;
class PreparedRefinementMap;
class OwnedRefinementLevel {
    using Q = refinement_detail::Q;
    using V = refinement_detail::V;
    using M = refinement_detail::M;
    using G = refinement_detail::G;
    struct State {
        core::OwnedRelationalSource source;
        RefinementRevision revision;
        RefinementLimits limits;
        std::vector<M> original_metrics, frame_changes, frames, metrics, boundaries, adjoints, hodge;
        std::vector<numeric::PreparedExactSolve> frame_factors, metric_factors;
        std::vector<field::PreparedHodgeFrames> native_frames;
        std::vector<std::array<field::hodge_frame_detail::Projection, 3>> sectors;
        std::shared_ptr<const State> parent;
        std::size_t retained, depth;
    };
    std::shared_ptr<const State> state_;
    explicit OwnedRefinementLevel(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        (void)refinement_detail::source_words(s.source.source(), g);
        for (const auto *list : {&s.original_metrics, &s.frame_changes, &s.frames, &s.metrics, &s.boundaries,
                                 &s.adjoints, &s.hodge})
            for (const auto &m : *list)
                g.matrix(m);
        for (const auto &f : s.frame_factors)
            (void)f.factor_receipt(g);
        for (const auto &f : s.metric_factors)
            (void)f.factor_receipt(g);
        for (const auto &f : s.native_frames)
            f.validate(g);
        for (const auto &a : s.sectors)
            for (const auto &p : a)
                p.validate(g);
        if (s.parent)
            validate(*s.parent, g);
    }
    static OwnedRefinementLevel build(core::OwnedRelationalSource source, std::vector<M> original,
                                      std::vector<M> frames, std::vector<M> changes,
                                      RefinementRevision revision, RefinementLimits bounds,
                                      std::shared_ptr<const State> parent, G &guard) {
        using namespace refinement_detail;
        refinement_detail::limits(bounds);
        refinement_detail::revision(revision);
        guard.step();
        const auto depth = parent ? parent->depth + 1 : 1;
        if (depth > bounds.max_depth)
            throw std::length_error("refinement frame history depth");
        const auto count = source.source().grade_count();
        if (count > bounds.max_grades || original.size() != count || frames.size() != count ||
            changes.size() != count)
            throw std::invalid_argument("refinement whole-grade frame/metric coverage");
        std::size_t cells{}, flags{}, squares{};
        for (std::size_t k = 0; k < count; ++k) {
            const auto &a = source.source().grade({static_cast<std::uint16_t>(k)});
            if (a.cell_count() > bounds.max_cells)
                throw std::length_error("refinement level cell bound");
            cells = add(cells, a.cell_count());
            flags = add(flags, a.incidence_count());
            squares = add(squares, mul(add(a.cell_count(), 1), add(a.cell_count(), 1)));
        }
        if (flags > bounds.max_occurrences)
            throw std::length_error("refinement original occurrence bound");
        const auto words = source_words(source.source(), guard);
        const auto retained = add(parent ? parent->retained : 0, add(words, mul(512, squares)));
        auto g = phase(guard, retained);
        std::vector<M> metrics, boundaries, adjoints, hodge;
        std::vector<numeric::PreparedExactSolve> ff, mf;
        std::vector<field::PreparedHodgeFrames> native;
        std::vector<std::array<field::hodge_frame_detail::Projection, 3>> sectors;
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = source.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            g.matrix(frames[k]);
            g.matrix(original[k]);
            if (frames[k].rows() != n || frames[k].cols() != n || original[k].rows() != n ||
                original[k].cols() != n)
                throw std::invalid_argument("refinement source metric/frame shape");
            ff.emplace_back(frames[k], g);
            metrics.push_back(mm(frames[k].transpose(), mm(original[k], frames[k], g), g));
            mf.emplace_back(metrics.back(), g);
        }
        for (std::size_t k = 0; k < count; ++k) {
            native.emplace_back(source.shared_source(), original, core::Grade{static_cast<std::uint16_t>(k)},
                                g);
            const auto n = frames[k].rows();
            if (!k) {
                boundaries.emplace_back(0, n);
                adjoints.emplace_back(n, 0);
            } else {
                auto raw =
                    field::hodge_frame_detail::boundary(source.source(), {static_cast<std::uint16_t>(k)}, g);
                boundaries.push_back(columns(ff[k - 1], mm(raw, frames[k], g), g));
                adjoints.push_back(columns(mf[k], mm(boundaries.back().transpose(), metrics[k - 1], g), g));
            }
        }
        for (std::size_t k = 0; k < count; ++k) {
            const auto n = frames[k].rows();
            M l(n, n);
            if (k)
                l = mm(adjoints[k], boundaries[k], g);
            if (k + 1 < count)
                l = sum(l, mm(boundaries[k + 1], adjoints[k + 1], g), g);
            hodge.push_back(std::move(l));
            sectors.push_back({field::hodge_frame_detail::Projection(
                                   columns(ff[k], native[k].gradient_frame(), g), metrics[k], g),
                               field::hodge_frame_detail::Projection(
                                   columns(ff[k], native[k].curl_frame(), g), metrics[k], g),
                               field::hodge_frame_detail::Projection(
                                   columns(ff[k], native[k].harmonic_frame(), g), metrics[k], g)});
            if (k > 1 && mm(boundaries[k - 1], boundaries[k], g).nonzeros())
                throw std::logic_error("refinement complete chain family");
        }
        std::size_t actual = words;
        for (const auto *list : {&original, &changes, &frames, &metrics, &boundaries, &adjoints, &hodge})
            for (const auto &m : *list)
                actual = add(actual, m.nonzeros());
        for (const auto &f : ff)
            actual = add(actual, add(f.stored_entries(), f.stored_index_entries()));
        for (const auto &f : mf)
            actual = add(actual, add(f.stored_entries(), f.stored_index_entries()));
        for (const auto &f : native)
            actual = add(actual, add(f.stored_entries(), f.stored_index_entries()));
        for (const auto &a : sectors)
            for (const auto &p : a)
                actual = add(actual, add(p.entries(), p.indices()));
        if (actual > retained - (parent ? parent->retained : 0))
            throw std::length_error("refinement level retained envelope");
        guard.step();
        return OwnedRefinementLevel(std::make_shared<const State>(State{
            std::move(source), revision, bounds, std::move(original), std::move(changes), std::move(frames),
            std::move(metrics), std::move(boundaries), std::move(adjoints), std::move(hodge), std::move(ff),
            std::move(mf), std::move(native), std::move(sectors), std::move(parent), retained, depth}));
    }
    friend class RefinementField;
    friend class PreparedRefinementMap;

  public:
    static OwnedRefinementLevel prepare(core::OwnedRelationalSource source, std::vector<M> metrics,
                                        RefinementRevision id, G &g, RefinementLimits limits = {}) {
        refinement_detail::limits(limits);
        if (source.source().grade_count() > limits.max_grades)
            throw std::length_error("refinement grade bound");
        std::vector<M> frames;
        for (std::size_t k = 0; k < source.source().grade_count(); ++k) {
            g.step();
            const auto n = source.source().grade({static_cast<std::uint16_t>(k)}).cell_count();
            if (n > limits.max_cells)
                throw std::length_error("refinement cell bound");
            g.entries(n);
            frames.push_back(M::identity(n));
        }
        auto changes = frames;
        return build(std::move(source), std::move(metrics), std::move(frames), std::move(changes), id, limits,
                     {}, g);
    }
    OwnedRefinementLevel reframe(std::vector<M> changes, RefinementRevision id, G &g) const {
        validate_receiving(g);
        if (changes.size() != grades())
            throw std::invalid_argument("refinement reframe grade coverage");
        auto declared_changes = changes;
        auto work = refinement_detail::phase(g, state_->retained);
        for (std::size_t k = 0; k < grades(); ++k) {
            if (changes[k].rows() != size(k) || changes[k].cols() != size(k))
                throw std::invalid_argument("refinement frame carrier");
            changes[k] = refinement_detail::mm(state_->frames[k], changes[k], work);
        }
        return build(state_->source, state_->original_metrics, std::move(changes),
                     std::move(declared_changes), id, state_->limits, state_, g);
    }
    std::size_t grades() const noexcept {
        return state_->metrics.size();
    }
    std::size_t size(std::size_t k) const {
        return state_->metrics.at(k).rows();
    }
    template <std::floating_point T> std::size_t size(T) const = delete;
    RefinementRevision revision() const noexcept {
        return state_->revision;
    }
    const core::OwnedRelationalSource &source() const & noexcept {
        return state_->source;
    }
    const core::OwnedRelationalSource &source() const && = delete;
    const std::vector<M> &original_metrics() const & noexcept {
        return state_->original_metrics;
    }
    const std::vector<M> &original_metrics() const && = delete;
    const std::vector<M> &frame_changes() const & noexcept {
        return state_->frame_changes;
    }
    const std::vector<M> &frame_changes() const && = delete;
    std::optional<OwnedRefinementLevel> frame_parent() const {
        if (state_->parent)
            return OwnedRefinementLevel(state_->parent);
        return {};
    }
    RefinementLimits limits() const noexcept {
        return state_->limits;
    }
    const std::vector<M> &metrics() const & noexcept {
        return state_->metrics;
    }
    const std::vector<M> &metrics() const && = delete;
    const std::vector<M> &frames() const & noexcept {
        return state_->frames;
    }
    const std::vector<M> &frames() const && = delete;
    const std::vector<M> &boundaries() const & noexcept {
        return state_->boundaries;
    }
    const std::vector<M> &boundaries() const && = delete;
    const std::vector<M> &adjoints() const & noexcept {
        return state_->adjoints;
    }
    const std::vector<M> &adjoints() const && = delete;
    const std::vector<M> &hodge() const & noexcept {
        return state_->hodge;
    }
    const std::vector<M> &hodge() const && = delete;
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    bool same_owner(const OwnedRefinementLevel &n) const noexcept {
        return state_ == n.state_;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    RefinementField field(core::Grade grade, std::span<const Q> values, G &g) const;
    RefinementField from_parent(const RefinementField &field, G &g) const;
    RefinementField project(std::size_t sector, const RefinementField &field, G &g) const;
    template <std::floating_point T> RefinementField project(T, const RefinementField &, G &) const = delete;
    M source_selector(core::CellId cell, G &guard) const {
        validate_receiving(guard);
        const auto k = cell.grade.value;
        if (k >= grades() || cell.index >= size(k))
            throw std::out_of_range("refinement original cell selector");
        auto g = refinement_detail::phase(guard, state_->retained);
        M selected(size(k), size(k));
        for (std::size_t j = 0; j < size(k); ++j) {
            g.step();
            selected.set(cell.index, j, state_->frames[k].get(cell.index, j));
        }
        return refinement_detail::columns(state_->frame_factors[k], selected, g);
    }
};
class RefinementField {
    using V = refinement_detail::V;
    using G = refinement_detail::G;
    OwnedRefinementLevel level_;
    core::Grade grade_;
    V values_;
    friend class OwnedRefinementLevel;
    RefinementField(OwnedRefinementLevel n, core::Grade k, V v)
        : level_(std::move(n)), grade_(k), values_(std::move(v)) {}

  public:
    const OwnedRefinementLevel &level() const & noexcept {
        return level_;
    }
    const OwnedRefinementLevel &level() const && = delete;
    core::Grade grade() const noexcept {
        return grade_;
    }
    const V &values() const & noexcept {
        return values_;
    }
    const V &values() const && = delete;
    void validate_receiving(G &g) const {
        g.entries(refinement_detail::add(level_.retained_allowance(), values_.size()));
        level_.validate_receiving(g);
        g.vector(values_);
    }
};
inline RefinementField OwnedRefinementLevel::field(core::Grade k, std::span<const Q> v, G &g) const {
    validate_receiving(g);
    if (k.value >= grades() || v.size() != size(k.value))
        throw std::invalid_argument("refinement complete owned field carrier");
    g.entries(refinement_detail::add(state_->retained, v.size()));
    V out;
    out.reserve(v.size());
    for (const auto &q : v) {
        g.step();
        g.value(q);
        out.push_back(numeric::canonical_rational(q));
    }
    return RefinementField(*this, k, std::move(out));
}
inline RefinementField OwnedRefinementLevel::from_parent(const RefinementField &f, G &g) const {
    f.validate_receiving(g);
    validate_receiving(g);
    if (!state_->parent || f.level().state_ != state_->parent)
        throw std::invalid_argument("refinement actual frame parent differs");
    auto work = refinement_detail::phase(g, refinement_detail::add(state_->retained, f.values().size()));
    auto raw = refinement_detail::mv(state_->parent->frames[f.grade().value], f.values(), work);
    auto value = state_->frame_factors[f.grade().value].apply(raw, work);
    return field(f.grade(), value, g);
}
inline RefinementField OwnedRefinementLevel::project(std::size_t sector, const RefinementField &f,
                                                     G &g) const {
    f.validate_receiving(g);
    if (!same_owner(f.level()) || sector >= 3)
        throw std::invalid_argument("refinement sector field owner");
    auto work = refinement_detail::phase(g, state_->retained);
    auto value = state_->sectors[f.grade().value][sector].apply(f.values(), work).first;
    return field(f.grade(), value, g);
}

struct RefinementMapDeclaration {
    RefinementRevision revision;
    std::vector<geometry::LinearGradeMap> grades;
};
enum class RefinementProperty { Chain, MetricIsometry, AdjointIntertwining, HodgeIntertwining };
class PreparedRefinementMap {
    using Q = refinement_detail::Q;
    using V = refinement_detail::V;
    using M = refinement_detail::M;
    using G = refinement_detail::G;
    struct State {
        OwnedRefinementLevel source, target;
        RefinementMapDeclaration declaration;
        geometry::RelationalChainMap native;
        std::vector<M> maps, down, metric, adjoint, hodge;
        std::vector<std::array<M, 4>> hodge_positions;
        std::vector<std::array<M, 2>> composition_metric, composition_adjoint, composition_hodge;
        std::array<std::shared_ptr<const State>, 2> parents;
        RefinementLimits limits;
        std::size_t retained, depth;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRefinementMap(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.source.validate_receiving(g);
        s.target.validate_receiving(g);
        for (const auto &grade : s.declaration.grades)
            for (const auto &column : grade)
                for (const auto &t : column) {
                    g.step();
                    g.value(t.coefficient);
                }
        for (const auto *list : {&s.maps, &s.down, &s.metric, &s.adjoint, &s.hodge})
            for (const auto &m : *list)
                g.matrix(m);
        for (const auto &a : s.hodge_positions)
            for (const auto &m : a)
                g.matrix(m);
        for (const auto *list : {&s.composition_metric, &s.composition_adjoint, &s.composition_hodge})
            for (const auto &a : *list)
                for (const auto &m : a)
                    g.matrix(m);
        for (const auto &p : s.parents)
            if (p)
                validate(*p, g);
    }

  public:
    static PreparedRefinementMap prepare(OwnedRefinementLevel source, OwnedRefinementLevel target,
                                         RefinementMapDeclaration declaration, G &guard,
                                         RefinementLimits bounds = {}) {
        using namespace refinement_detail;
        refinement_detail::limits(bounds);
        revision(declaration.revision);
        guard.step();
        source.validate_receiving(guard);
        target.validate_receiving(guard);
        if (!bounds.max_depth)
            throw std::length_error("refinement map history bound");
        const auto count = source.grades();
        if (count > bounds.max_grades)
            throw std::length_error("refinement map grade bound");
        if (target.grades() != count || declaration.grades.size() != count)
            throw std::invalid_argument("refinement map must retain every grade, including empty grades");
        std::size_t terms{}, squares{}, source_flags{}, target_flags{};
        for (std::size_t k = 0; k < count; ++k) {
            if (source.size(k) > bounds.max_cells || target.size(k) > bounds.max_cells)
                throw std::length_error("refinement map cell bound");
            source_flags =
                add(source_flags,
                    source.source().source().grade({static_cast<std::uint16_t>(k)}).incidence_count());
            target_flags =
                add(target_flags,
                    target.source().source().grade({static_cast<std::uint16_t>(k)}).incidence_count());
            if (declaration.grades[k].size() != source.size(k))
                throw std::invalid_argument("refinement map complete source columns");
            squares = add(squares, mul(add(add(source.size(k), target.size(k)), 1),
                                       add(add(source.size(k), target.size(k)), 1)));
            for (const auto &c : declaration.grades[k])
                terms = add(terms, c.size());
        }
        if (source_flags > bounds.max_occurrences || target_flags > bounds.max_occurrences)
            throw std::length_error("refinement map source occurrence bound");
        if (terms > bounds.max_terms)
            throw std::length_error("refinement retained map term bound");
        const auto retained = add(add(source.retained_allowance(), target.retained_allowance()),
                                  add(mul(8, terms), mul(128, squares)));
        auto g = phase(guard, retained);
        geometry::RelationalChainMap native(source.source().source(), target.source().source(),
                                            declaration.grades);
        std::vector<M> maps, down, metric, adjoint, hodge;
        std::vector<std::array<M, 4>> positions;
        for (std::size_t k = 0; k < count; ++k) {
            M raw(target.size(k), source.size(k));
            for (std::size_t j = 0; j < source.size(k); ++j)
                for (const auto &t : native.image({static_cast<std::uint16_t>(k)}, j)) {
                    g.step();
                    g.value(t.coefficient);
                    raw.set(t.target, j, g.add(raw.get(t.target, j), t.coefficient));
                }
            maps.push_back(columns(target.state_->frame_factors[k], mm(raw, source.frames()[k], g), g));
        }
        for (std::size_t k = 0; k < count; ++k) {
            if (k) {
                down.push_back(sub(mm(target.boundaries()[k], maps[k], g),
                                   mm(maps[k - 1], source.boundaries()[k], g), g));
                if (down.back().nonzeros())
                    throw std::invalid_argument("refinement full native chain identity failed");
                adjoint.push_back(
                    sub(mm(target.adjoints()[k], maps[k - 1], g), mm(maps[k], source.adjoints()[k], g), g));
            } else {
                down.emplace_back(0, source.size(0));
                adjoint.emplace_back(target.size(0), 0);
            }
            metric.push_back(
                sub(mm(maps[k].transpose(), mm(target.metrics()[k], maps[k], g), g), source.metrics()[k], g));
            hodge.push_back(sub(mm(target.hodge()[k], maps[k], g), mm(maps[k], source.hodge()[k], g), g));
        }
        for (std::size_t k = 0; k < count; ++k) {
            const auto m = target.size(k), n = source.size(k);
            std::array<M, 4> p{M(m, n), M(m, n), M(m, n), M(m, n)};
            if (k) {
                p[0] = mm(target.adjoints()[k], down[k], g);
                p[1] = mm(adjoint[k], source.boundaries()[k], g);
            }
            if (k + 1 < count) {
                p[2] = mm(target.boundaries()[k + 1], adjoint[k + 1], g);
                p[3] = mm(down[k + 1], source.adjoints()[k + 1], g);
            }
            if (sum(sum(p[0], p[1], g), sum(p[2], p[3], g), g) != hodge[k])
                throw std::logic_error("refinement four-position Hodge identity");
            positions.push_back(std::move(p));
        }
        guard.step();
        return PreparedRefinementMap(std::make_shared<const State>(State{std::move(source),
                                                                         std::move(target),
                                                                         std::move(declaration),
                                                                         std::move(native),
                                                                         std::move(maps),
                                                                         std::move(down),
                                                                         std::move(metric),
                                                                         std::move(adjoint),
                                                                         std::move(hodge),
                                                                         std::move(positions),
                                                                         {},
                                                                         {},
                                                                         {},
                                                                         {},
                                                                         bounds,
                                                                         retained,
                                                                         1}));
    }
    PreparedRefinementMap then(const PreparedRefinementMap &second, RefinementRevision id, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        second.validate_receiving(guard);
        if (!target().same_owner(second.source()))
            throw std::invalid_argument("refinement composition actual middle owner differs");
        const auto depth = add(state_->depth, second.state_->depth);
        if (depth > state_->limits.max_depth)
            throw std::length_error("refinement map history depth");
        std::size_t pairs{};
        for (std::size_t k = 0; k < source().grades(); ++k)
            for (const auto &c : declaration().grades[k])
                for (const auto &t : c)
                    pairs = add(pairs, second.declaration().grades[k][t.target].size());
        if (pairs > state_->limits.max_terms)
            throw std::length_error("complete refinement map contribution bound");
        const auto retained = add(add(state_->retained, second.state_->retained), mul(32, add(pairs, 1)));
        auto g = phase(guard, retained);
        RefinementMapDeclaration d{id, {}};
        for (std::size_t k = 0; k < source().grades(); ++k) {
            geometry::LinearGradeMap grade(source().size(k));
            for (std::size_t j = 0; j < grade.size(); ++j)
                for (const auto &a : declaration().grades[k][j])
                    for (const auto &b : second.declaration().grades[k][a.target])
                        grade[j].push_back({b.target, g.mul(b.coefficient, a.coefficient)});
            d.grades.push_back(std::move(grade));
        }
        std::size_t squares{};
        for (std::size_t k = 0; k < source().grades(); ++k) {
            const auto width = add(add(source().size(k), second.target().size(k)), 1);
            squares = add(squares, mul(width, width));
        }
        const auto result_envelope =
            add(add(source().retained_allowance(), second.target().retained_allowance()),
                add(mul(8, pairs), mul(128, squares)));
        guard.entries(add(retained, result_envelope));
        auto result = prepare(source(), second.target(), std::move(d), guard, state_->limits);
        guard.entries(add(result.state_->retained, retained));
        auto next = std::make_shared<State>(*result.state_);
        next->parents = {state_, second.state_};
        next->depth = depth;
        next->retained = add(next->retained, retained);
        for (std::size_t k = 0; k < source().grades(); ++k) {
            auto p =
                std::array<M, 2>{mm(maps()[k].transpose(), mm(second.metric_defects()[k], maps()[k], g), g),
                                 metric_defects()[k]};
            if (sum(p[0], p[1], g) != next->metric[k])
                throw std::logic_error("composed metric defect identity");
            next->composition_metric.push_back(std::move(p));
            auto h = std::array<M, 2>{mm(second.hodge_defects()[k], maps()[k], g),
                                      mm(second.maps()[k], hodge_defects()[k], g)};
            if (sum(h[0], h[1], g) != next->hodge[k])
                throw std::logic_error("composed Hodge defect identity");
            next->composition_hodge.push_back(std::move(h));
            std::array<M, 2> a{M(second.target().size(k), k ? source().size(k - 1) : 0),
                               M(second.target().size(k), k ? source().size(k - 1) : 0)};
            if (k) {
                a[0] = mm(second.adjoint_defects()[k], maps()[k - 1], g);
                a[1] = mm(second.maps()[k], adjoint_defects()[k], g);
            }
            if (sum(a[0], a[1], g) != next->adjoint[k])
                throw std::logic_error("composed adjoint defect identity");
            next->composition_adjoint.push_back(std::move(a));
        }
        guard.step();
        return PreparedRefinementMap(std::move(next));
    }
    const OwnedRefinementLevel &source() const & noexcept {
        return state_->source;
    }
    const OwnedRefinementLevel &source() const && = delete;
    const OwnedRefinementLevel &target() const & noexcept {
        return state_->target;
    }
    const OwnedRefinementLevel &target() const && = delete;
    std::vector<PreparedRefinementMap> composition_inputs() const {
        std::vector<PreparedRefinementMap> out;
        for (const auto &p : state_->parents)
            if (p)
                out.push_back(PreparedRefinementMap(p));
        return out;
    }
    RefinementLimits limits() const noexcept {
        return state_->limits;
    }
    const RefinementMapDeclaration &declaration() const & noexcept {
        return state_->declaration;
    }
    const RefinementMapDeclaration &declaration() const && = delete;
    const geometry::RelationalChainMap &native() const & noexcept {
        return state_->native;
    }
    const geometry::RelationalChainMap &native() const && = delete;
    const std::vector<M> &maps() const & noexcept {
        return state_->maps;
    }
    const std::vector<M> &maps() const && = delete;
    const std::vector<M> &boundary_defects() const & noexcept {
        return state_->down;
    }
    const std::vector<M> &boundary_defects() const && = delete;
    const std::vector<M> &metric_defects() const & noexcept {
        return state_->metric;
    }
    const std::vector<M> &metric_defects() const && = delete;
    const std::vector<M> &adjoint_defects() const & noexcept {
        return state_->adjoint;
    }
    const std::vector<M> &adjoint_defects() const && = delete;
    const std::vector<M> &hodge_defects() const & noexcept {
        return state_->hodge;
    }
    const std::vector<M> &hodge_defects() const && = delete;
    const auto &hodge_positions() const & noexcept {
        return state_->hodge_positions;
    }
    const auto &hodge_positions() const && = delete;
    const auto &composition_metric() const & noexcept {
        return state_->composition_metric;
    }
    const auto &composition_metric() const && = delete;
    const auto &composition_adjoint() const & noexcept {
        return state_->composition_adjoint;
    }
    const auto &composition_adjoint() const && = delete;
    const auto &composition_hodge() const & noexcept {
        return state_->composition_hodge;
    }
    const auto &composition_hodge() const && = delete;
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    bool same_owner(const PreparedRefinementMap &m) const noexcept {
        return state_ == m.state_;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    bool satisfies(RefinementProperty p) const {
        const std::vector<M> *values{};
        switch (p) {
        case RefinementProperty::Chain:
            values = &state_->down;
            break;
        case RefinementProperty::MetricIsometry:
            values = &state_->metric;
            break;
        case RefinementProperty::AdjointIntertwining:
            values = &state_->adjoint;
            break;
        case RefinementProperty::HodgeIntertwining:
            values = &state_->hodge;
            break;
        default:
            throw std::invalid_argument("unknown refinement property");
        }
        return std::all_of(values->begin(), values->end(), [](const M &m) { return !m.nonzeros(); });
    }
    void require(RefinementProperty p, G &g) const {
        validate_receiving(g);
        if (!satisfies(p))
            throw std::invalid_argument("requested refinement property has a nonzero complete residual");
    }
    M sector_defect(std::size_t sector, core::Grade grade, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        const auto k = grade.value;
        if (sector >= 3 || k >= source().grades())
            throw std::out_of_range("refinement sector comparison");
        const auto n = source().size(k), m = target().size(k);
        auto g = phase(guard, add(state_->retained, mul(32, mul(add(n, 1), add(m, 1)))));
        M out(m, n);
        for (std::size_t j = 0; j < n; ++j) {
            V axis(n);
            axis[j] = 1;
            auto old = state_->source.state_->sectors[k][sector].apply(axis, g).first;
            auto image = mv(maps()[k], axis, g);
            auto next = state_->target.state_->sectors[k][sector].apply(image, g).first;
            auto residual = sub(next, mv(maps()[k], old, g), g);
            for (std::size_t i = 0; i < m; ++i) {
                g.step();
                out.set(i, j, residual[i]);
            }
        }
        return out;
    }
    template <std::floating_point T> M sector_defect(T, core::Grade, G &) const = delete;
    RefinementField push(const RefinementField &f, G &guard) const {
        validate_receiving(guard);
        f.validate_receiving(guard);
        if (!source().same_owner(f.level()))
            throw std::invalid_argument("refinement field actual source differs");
        auto g = refinement_detail::phase(
            guard, refinement_detail::add(
                       state_->retained,
                       refinement_detail::mul(
                           8, refinement_detail::add(f.values().size(), target().size(f.grade().value)))));
        auto v = refinement_detail::mv(maps()[f.grade().value], f.values(), g);
        return target().field(f.grade(), v, guard);
    }
    class EnergyReceipt {
        friend class PreparedRefinementMap;
        std::shared_ptr<const State> owner_;
        RefinementField old_, next_, transported_, innovation_;
        std::array<Q, 3> terms_;
        Q total_;
        std::size_t retained_;
        EnergyReceipt(std::shared_ptr<const State> s, RefinementField a, RefinementField b, RefinementField t,
                      RefinementField i, std::array<Q, 3> q, Q total, std::size_t r)
            : owner_(std::move(s)), old_(std::move(a)), next_(std::move(b)), transported_(std::move(t)),
              innovation_(std::move(i)), terms_(std::move(q)), total_(std::move(total)), retained_(r) {}

      public:
        const RefinementField &old_field() const & noexcept {
            return old_;
        }
        const RefinementField &old_field() const && = delete;
        const RefinementField &new_field() const & noexcept {
            return next_;
        }
        const RefinementField &new_field() const && = delete;
        const auto &terms() const & noexcept {
            return terms_;
        }
        const auto &terms() const && = delete;
        const Q &total() const & noexcept {
            return total_;
        }
        const Q &total() const && = delete;
        const RefinementField &transported() const & noexcept {
            return transported_;
        }
        const RefinementField &transported() const && = delete;
        const RefinementField &innovation() const & noexcept {
            return innovation_;
        }
        const RefinementField &innovation() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(retained_);
            validate(*owner_, g);
            for (const auto *f : {&old_, &next_, &transported_, &innovation_})
                f->validate_receiving(g);
            for (const auto &q : terms_)
                g.value(q);
            g.value(total_);
        }
    };
    EnergyReceipt energy_change(RefinementField old, RefinementField next, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        old.validate_receiving(guard);
        next.validate_receiving(guard);
        if (!old.level().same_owner(source()) || !next.level().same_owner(target()) ||
            old.grade() != next.grade())
            throw std::invalid_argument("refinement energy actual endpoints/grade");
        const std::size_t k = old.grade().value,
                          retained = add(state_->retained, mul(64, add(source().size(k), target().size(k))));
        auto g = phase(guard, retained);
        auto transported = mv(maps()[k], old.values(), g), eta = sub(next.values(), transported, g);
        std::array<Q, 3> terms{inner(old.values(), metric_defects()[k], old.values(), g),
                               g.mul(Q{2}, inner(transported, target().metrics()[k], eta, g)),
                               inner(eta, target().metrics()[k], eta, g)};
        auto total = g.add(g.add(terms[0], terms[1]), terms[2]);
        if (total != g.sub(inner(next.values(), target().metrics()[k], next.values(), g),
                           inner(old.values(), source().metrics()[k], old.values(), g)))
            throw std::logic_error("complete finite refinement energy identity");
        guard.step();
        return EnergyReceipt(state_, std::move(old), std::move(next),
                             target().field({static_cast<std::uint16_t>(k)}, transported, guard),
                             target().field({static_cast<std::uint16_t>(k)}, eta, guard), std::move(terms),
                             std::move(total), retained);
    }
};

class PreparedRefinementRetraction {
    using Q = refinement_detail::Q;
    using V = refinement_detail::V;
    using M = refinement_detail::M;
    using G = refinement_detail::G;
    struct State {
        PreparedRefinementMap forward, reverse;
        std::vector<numeric::PreparedExactFrames> private_frames;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRefinementRetraction(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.forward.validate_receiving(g);
        s.reverse.validate_receiving(g);
        for (const auto &f : s.private_frames)
            f.validate(g);
    }

  public:
    static PreparedRefinementRetraction prepare(PreparedRefinementMap forward, PreparedRefinementMap reverse,
                                                G &guard) {
        using namespace refinement_detail;
        forward.validate_receiving(guard);
        reverse.validate_receiving(guard);
        if (!forward.source().same_owner(reverse.target()) || !forward.target().same_owner(reverse.source()))
            throw std::invalid_argument("retraction actual reverse endpoints differ");
        std::size_t extra{};
        for (std::size_t k = 0; k < forward.source().grades(); ++k)
            extra =
                add(extra, mul(32, mul(add(forward.target().size(k), 1), add(forward.target().size(k), 1))));
        const auto retained = add(add(forward.retained_allowance(), reverse.retained_allowance()), extra);
        auto g = phase(guard, retained);
        std::vector<numeric::PreparedExactFrames> kernels;
        for (std::size_t k = 0; k < forward.source().grades(); ++k) {
            if (mm(reverse.maps()[k], forward.maps()[k], g) != M::identity(forward.source().size(k)))
                throw std::invalid_argument("reconstruction must be a complete left inverse");
            kernels.emplace_back(reverse.maps()[k], g);
            if (kernels.back().rank() != forward.source().size(k) ||
                kernels.back().kernel().cols() != forward.target().size(k) - forward.source().size(k))
                throw std::logic_error("complete private refinement directions");
        }
        guard.step();
        return PreparedRefinementRetraction(std::make_shared<const State>(
            State{std::move(forward), std::move(reverse), std::move(kernels), retained}));
    }
    const PreparedRefinementMap &forward() const & noexcept {
        return state_->forward;
    }
    const PreparedRefinementMap &forward() const && = delete;
    const PreparedRefinementMap &reverse() const & noexcept {
        return state_->reverse;
    }
    const PreparedRefinementMap &reverse() const && = delete;
    const M &private_frame(core::Grade k) const & {
        return state_->private_frames.at(k.value).kernel();
    }
    const M &private_frame(core::Grade) const && = delete;
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    class Split {
        friend class PreparedRefinementRetraction;
        std::shared_ptr<const State> owner_;
        RefinementField input_, coarse_, represented_, private_;
        M gram_;
        std::size_t retained_;
        Split(std::shared_ptr<const State> s, RefinementField i, RefinementField c, RefinementField r,
              RefinementField p, M gram, std::size_t n)
            : owner_(std::move(s)), input_(std::move(i)), coarse_(std::move(c)), represented_(std::move(r)),
              private_(std::move(p)), gram_(std::move(gram)), retained_(n) {}

      public:
        const RefinementField &input() const & noexcept {
            return input_;
        }
        const RefinementField &input() const && = delete;
        const RefinementField &coarse() const & noexcept {
            return coarse_;
        }
        const RefinementField &coarse() const && = delete;
        const RefinementField &represented() const & noexcept {
            return represented_;
        }
        const RefinementField &represented() const && = delete;
        const RefinementField &private_field() const & noexcept {
            return private_;
        }
        const RefinementField &private_field() const && = delete;
        const M &gram() const & noexcept {
            return gram_;
        }
        const M &gram() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(retained_);
            validate(*owner_, g);
            for (const auto *f : {&input_, &coarse_, &represented_, &private_})
                f->validate_receiving(g);
            g.matrix(gram_);
        }
    };
    Split split(RefinementField input, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        input.validate_receiving(guard);
        if (!input.level().same_owner(forward().target()))
            throw std::invalid_argument("private split actual fine owner");
        const std::size_t k = input.grade().value,
                          retained = add(state_->retained, mul(64, add(input.values().size(), 1)));
        auto g = phase(guard, retained);
        auto coarse = mv(reverse().maps()[k], input.values(), g),
             represented = mv(forward().maps()[k], coarse, g),
             private_part = sub(input.values(), represented, g);
        if (!numeric::is_zero(mv(reverse().maps()[k], private_part, g)) ||
            sum(represented, private_part, g) != input.values())
            throw std::logic_error("refinement complete private split residual");
        M gram(2, 2);
        const std::array<V, 2> channels{represented, private_part};
        for (std::size_t i = 0; i < 2; ++i)
            for (std::size_t j = 0; j < 2; ++j)
                gram.set(i, j, inner(channels[i], input.level().metrics()[k], channels[j], g));
        Q total;
        for (const auto &[ij, q] : gram.entries()) {
            (void)ij;
            total = g.add(total, q);
        }
        if (total != inner(input.values(), input.level().metrics()[k], input.values(), g))
            throw std::logic_error("private split cross terms lost");
        guard.step();
        return Split(state_, input, forward().source().field(input.grade(), coarse, guard),
                     forward().target().field(input.grade(), represented, guard),
                     forward().target().field(input.grade(), private_part, guard), std::move(gram), retained);
    }
};

class PreparedRefinementResponse {
    using Q = refinement_detail::Q;
    using V = refinement_detail::V;
    using M = refinement_detail::M;
    using G = refinement_detail::G;
    struct State {
        PreparedRefinementMap map;
        core::Grade grade;
        Q lambda;
        PreparedGreenResponseDelta native;
        M defect;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRefinementResponse(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        s.map.validate_receiving(g);
        g.value(s.lambda);
        g.matrix(s.defect);
        auto scratch = refinement_detail::phase(g, s.retained);
        (void)s.native.apply(V(s.map.source().size(s.grade.value)), V(s.map.target().size(s.grade.value)),
                             scratch);
    }

  public:
    static PreparedRefinementResponse prepare(PreparedRefinementMap map, core::Grade grade, Q lambda,
                                              G &guard) {
        using namespace refinement_detail;
        map.validate_receiving(guard);
        guard.value(lambda);
        lambda = numeric::canonical_rational(lambda);
        if (lambda < 0 || grade.value >= map.source().grades())
            throw std::invalid_argument(
                "refinement Green requires nonnegative common shift and actual grade");
        const std::size_t k = grade.value, n = map.source().size(k), m = map.target().size(k);
        const auto retained =
            add(map.retained_allowance(), mul(128, mul(add(add(n, m), 1), add(add(n, m), 1))));
        auto g = phase(guard, retained);
        PreparedGreenResponseDelta native(map.source().hodge()[k], map.target().hodge()[k], map.maps()[k],
                                          M::identity(n), M::identity(m), map.maps()[k], lambda, g);
        M defect(m, n);
        for (std::size_t j = 0; j < n; ++j) {
            V x(n);
            x[j] = 1;
            const auto y = mv(map.maps()[k], x, g);
            auto result = native.apply(x, y, g);
            for (std::size_t i = 0; i < m; ++i) {
                g.step();
                defect.set(i, j, result.aggregate[i]);
            }
            if (!numeric::is_zero(result.realization_term) || !numeric::is_zero(result.innovation_term) ||
                result.aggregate != result.operator_term)
                throw std::logic_error("whole-carrier Green intertwining defect");
        }
        guard.step();
        return PreparedRefinementResponse(std::make_shared<const State>(
            State{std::move(map), grade, std::move(lambda), std::move(native), std::move(defect), retained}));
    }
    template <std::floating_point T>
    static PreparedRefinementResponse prepare(PreparedRefinementMap, core::Grade, T, G &) = delete;
    const M &defect() const & noexcept {
        return state_->defect;
    }
    const M &defect() const && = delete;
    const Q &lambda() const & noexcept {
        return state_->lambda;
    }
    const Q &lambda() const && = delete;
    const PreparedRefinementMap &map() const & noexcept {
        return state_->map;
    }
    const PreparedRefinementMap &map() const && = delete;
    core::Grade grade() const noexcept {
        return state_->grade;
    }
    void require_intertwining(G &g) const {
        validate_receiving(g);
        if (state_->defect.nonzeros())
            throw std::invalid_argument("selected Green shift has a nonzero complete defect");
    }
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    class Result {
        friend class PreparedRefinementResponse;
        std::shared_ptr<const State> owner_;
        RefinementField old_, next_;
        GreenResponseDelta data_;
        std::size_t retained_;
        Result(std::shared_ptr<const State> s, RefinementField a, RefinementField b, GreenResponseDelta d,
               std::size_t n)
            : owner_(std::move(s)), old_(std::move(a)), next_(std::move(b)), data_(std::move(d)),
              retained_(n) {}

      public:
        const RefinementField &old_input() const & noexcept {
            return old_;
        }
        const RefinementField &old_input() const && = delete;
        const RefinementField &new_input() const & noexcept {
            return next_;
        }
        const RefinementField &new_input() const && = delete;
        const GreenResponseDelta &data() const & noexcept {
            return data_;
        }
        const GreenResponseDelta &data() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(retained_);
            validate(*owner_, g);
            old_.validate_receiving(g);
            next_.validate_receiving(g);
            for (const auto *v :
                 {&data_.old_response, &data_.new_response, &data_.coordinate_innovation,
                  &data_.realization_term, &data_.innovation_term, &data_.operator_term, &data_.aggregate})
                g.vector(*v);
            g.matrix(data_.injection_defect);
            g.matrix(data_.operator_defect);
        }
    };
    Result apply(RefinementField old, RefinementField next, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        old.validate_receiving(guard);
        next.validate_receiving(guard);
        if (!old.level().same_owner(map().source()) || !next.level().same_owner(map().target()) ||
            old.grade() != grade() || next.grade() != grade())
            throw std::invalid_argument("refinement Green actual source/target fields");
        const auto retained =
            add(state_->retained, mul(64, add(add(old.values().size(), next.values().size()), 1)));
        auto g = phase(guard, retained);
        auto result = state_->native.apply(old.values(), next.values(), g);
        guard.step();
        return Result(state_, std::move(old), std::move(next), std::move(result), retained);
    }
};

// Finite forward DAG. All paths (including identities) are retained and checked;
// coarsening/retraction legs remain separately owned, not silently inverted.
class PreparedRefinementDiagram {
    using Q = refinement_detail::Q;
    using V = refinement_detail::V;
    using M = refinement_detail::M;
    using G = refinement_detail::G;

  public:
    struct Path {
        std::size_t source, target, anchor;
        std::vector<std::size_t> maps;
        std::vector<M> composite, residual;
    };

  private:
    struct State {
        RefinementRevision revision;
        std::vector<OwnedRefinementLevel> levels;
        std::vector<PreparedRefinementMap> maps;
        std::vector<PreparedRefinementRetraction> retractions;
        std::vector<Path> paths;
        RefinementLimits limits;
        std::size_t retained;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedRefinementDiagram(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static void validate(const State &s, G &g) {
        g.entries(s.retained);
        for (const auto &n : s.levels)
            n.validate_receiving(g);
        for (const auto &m : s.maps)
            m.validate_receiving(g);
        for (const auto &r : s.retractions)
            r.validate_receiving(g);
        for (const auto &p : s.paths) {
            for (const auto i : p.maps) {
                g.step();
                if (i >= s.maps.size())
                    throw std::logic_error("retained diagram map position");
            }
            for (const auto &m : p.composite)
                g.matrix(m);
            for (const auto &m : p.residual)
                g.matrix(m);
        }
    }

  public:
    static PreparedRefinementDiagram prepare(std::vector<OwnedRefinementLevel> levels,
                                             std::vector<PreparedRefinementMap> maps,
                                             std::vector<PreparedRefinementRetraction> retractions,
                                             RefinementRevision id, G &guard, RefinementLimits bounds = {}) {
        using namespace refinement_detail;
        refinement_detail::limits(bounds);
        refinement_detail::revision(id);
        guard.step();
        if (levels.empty() || levels.size() > bounds.max_levels || maps.size() > bounds.max_maps ||
            retractions.size() > bounds.max_maps)
            throw std::length_error("refinement finite diagram size");
        const auto N = levels.size();
        std::vector<std::pair<std::size_t, std::size_t>> ends;
        std::vector<std::vector<std::size_t>> out(N);
        std::vector<std::size_t> indegree(N);
        auto position = [&](const OwnedRefinementLevel &n) {
            for (std::size_t i = 0; i < N; ++i) {
                guard.step();
                if (levels[i].same_owner(n))
                    return i;
            }
            throw std::invalid_argument("diagram omits an actual map endpoint");
        };
        std::size_t retained = 64, dimension{};
        for (std::size_t i = 0; i < N; ++i) {
            levels[i].validate_receiving(guard);
            if (levels[i].grades() > bounds.max_grades)
                throw std::length_error("diagram level grade bound");
            std::size_t flags{};
            for (std::size_t k = 0; k < levels[i].grades(); ++k) {
                if (levels[i].size(k) > bounds.max_cells)
                    throw std::length_error("diagram level cell bound");
                flags =
                    add(flags,
                        levels[i].source().source().grade({static_cast<std::uint16_t>(k)}).incidence_count());
            }
            if (flags > bounds.max_occurrences)
                throw std::length_error("diagram level occurrence bound");
            retained = add(retained, levels[i].retained_allowance());
            for (std::size_t j = 0; j < i; ++j)
                if (levels[i].same_owner(levels[j]))
                    throw std::invalid_argument("duplicate actual diagram level");
            for (std::size_t k = 0; k < levels[i].grades(); ++k)
                dimension = add(dimension, mul(add(levels[i].size(k), 1), add(levels[i].size(k), 1)));
        }
        for (std::size_t e = 0; e < maps.size(); ++e) {
            maps[e].validate_receiving(guard);
            std::size_t terms{};
            for (const auto &grade : maps[e].declaration().grades)
                for (const auto &column : grade)
                    terms = add(terms, column.size());
            if (terms > bounds.max_terms)
                throw std::length_error("diagram map term bound");
            retained = add(retained, maps[e].retained_allowance());
            const auto a = position(maps[e].source()), b = position(maps[e].target());
            ends.push_back({a, b});
            out[a].push_back(e);
            ++indegree[b];
        }
        for (const auto &r : retractions) {
            r.validate_receiving(guard);
            retained = add(retained, r.retained_allowance());
            if (std::none_of(maps.begin(), maps.end(),
                             [&](const auto &m) { return m.same_owner(r.forward()); }))
                throw std::invalid_argument("diagram retraction has no actual forward leg");
        }
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < N; ++i)
            if (!indegree[i])
                order.push_back(i);
        for (std::size_t q = 0; q < order.size(); ++q)
            for (const auto e : out[order[q]]) {
                guard.step();
                if (!--indegree[ends[e].second])
                    order.push_back(ends[e].second);
            }
        if (order.size() != N)
            throw std::invalid_argument("initial refinement diagram requires an acyclic forward graph");
        std::size_t total{};
        for (std::size_t start = 0; start < N; ++start) {
            std::vector<std::size_t> counts(N);
            counts[start] = 1;
            for (const auto n : order)
                for (const auto e : out[n]) {
                    guard.step();
                    counts[ends[e].second] = add(counts[ends[e].second], counts[n]);
                    if (counts[ends[e].second] > bounds.max_paths)
                        throw std::length_error("complete refinement path bound");
                }
            for (const auto n : counts)
                total = add(total, n);
        }
        if (total > bounds.max_paths)
            throw std::length_error("complete diagram path family exceeds bound");
        retained = add(retained, mul(16, mul(add(total, 1), add(dimension, 1))));
        auto g = phase(guard, retained);
        std::vector<Path> paths;
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> anchors;
        for (std::size_t start = 0; start < N; ++start) {
            const auto first = paths.size();
            std::vector<M> identity;
            for (std::size_t k = 0; k < levels[start].grades(); ++k)
                identity.push_back(M::identity(levels[start].size(k)));
            paths.push_back({start, start, first, {}, std::move(identity), {}});
            anchors[{start, start}] = first;
            for (std::size_t q = first; q < paths.size(); ++q) {
                if (paths[q].source != start)
                    break;
                const auto current = paths[q];
                for (const auto e : out[current.target]) {
                    if (current.maps.size() >= bounds.max_depth)
                        throw std::length_error("refinement path depth");
                    auto word = current.maps;
                    word.push_back(e);
                    std::vector<M> composite, residual;
                    for (std::size_t k = 0; k < current.composite.size(); ++k)
                        composite.push_back(mm(maps[e].maps()[k], current.composite[k], g));
                    const auto target = ends[e].second;
                    auto [at, inserted] = anchors.try_emplace({start, target}, paths.size());
                    for (std::size_t k = 0; k < composite.size(); ++k) {
                        residual.push_back(inserted ? M(composite[k].rows(), composite[k].cols())
                                                    : sub(composite[k], paths[at->second].composite[k], g));
                        if (residual.back().nonzeros())
                            throw std::invalid_argument("complete refinement diagram does not commute");
                    }
                    paths.push_back({start, target, at->second, std::move(word), std::move(composite),
                                     std::move(residual)});
                }
            }
        }
        if (paths.size() != total)
            throw std::logic_error("incomplete refinement path census");
        guard.step();
        return PreparedRefinementDiagram(
            std::make_shared<const State>(State{id, std::move(levels), std::move(maps),
                                                std::move(retractions), std::move(paths), bounds, retained}));
    }
    class Route {
        friend class PreparedRefinementDiagram;
        std::shared_ptr<const State> owner_;
        std::size_t index_;
        Route(std::shared_ptr<const State> s, std::size_t i) : owner_(std::move(s)), index_(i) {}

      public:
        std::size_t index() const noexcept {
            return index_;
        }
    };
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    Route route(T i, G &g) const {
        validate_receiving(g);
        return Route(state_, refinement_detail::index(i, state_->paths.size()));
    }
    template <std::floating_point T> Route route(T, G &) const = delete;
    RefinementRevision revision() const noexcept {
        return state_->revision;
    }
    RefinementLimits limits() const noexcept {
        return state_->limits;
    }
    const std::vector<Path> &paths() const & noexcept {
        return state_->paths;
    }
    const std::vector<Path> &paths() const && = delete;
    const std::vector<OwnedRefinementLevel> &levels() const & noexcept {
        return state_->levels;
    }
    const std::vector<OwnedRefinementLevel> &levels() const && = delete;
    const std::vector<PreparedRefinementMap> &maps() const & noexcept {
        return state_->maps;
    }
    const std::vector<PreparedRefinementMap> &maps() const && = delete;
    const std::vector<PreparedRefinementRetraction> &retractions() const & noexcept {
        return state_->retractions;
    }
    const std::vector<PreparedRefinementRetraction> &retractions() const && = delete;
    void validate_receiving(G &g) const {
        validate(*state_, g);
    }
    class Accumulation {
        friend class PreparedRefinementDiagram;
        std::shared_ptr<const State> owner_;
        Route route_;
        RefinementField initial_;
        std::vector<RefinementField> innovations_, prefixes_, channels_;
        M gram_;
        std::size_t retained_;
        Accumulation(std::shared_ptr<const State> s, Route r, RefinementField i,
                     std::vector<RefinementField> eta, std::vector<RefinementField> prefix,
                     std::vector<RefinementField> channels, M gram, std::size_t n)
            : owner_(std::move(s)), route_(std::move(r)), initial_(std::move(i)),
              innovations_(std::move(eta)), prefixes_(std::move(prefix)), channels_(std::move(channels)),
              gram_(std::move(gram)), retained_(n) {}

      public:
        const RefinementField &initial() const & noexcept {
            return initial_;
        }
        const RefinementField &initial() const && = delete;
        const auto &innovations() const & noexcept {
            return innovations_;
        }
        const auto &innovations() const && = delete;
        Route route() const {
            return route_;
        }
        const auto &prefixes() const & noexcept {
            return prefixes_;
        }
        const auto &prefixes() const && = delete;
        const auto &channels() const & noexcept {
            return channels_;
        }
        const auto &channels() const && = delete;
        const RefinementField &terminal() const & noexcept {
            return prefixes_.back();
        }
        const RefinementField &terminal() const && = delete;
        const M &gram() const & noexcept {
            return gram_;
        }
        const M &gram() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(retained_);
            validate(*owner_, g);
            initial_.validate_receiving(g);
            for (const auto *list : {&innovations_, &prefixes_, &channels_})
                for (const auto &f : *list)
                    f.validate_receiving(g);
            g.matrix(gram_);
        }
    };
    Accumulation accumulate(Route route, RefinementField initial, std::vector<RefinementField> innovations,
                            G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        if (route.owner_ != state_)
            throw std::invalid_argument("foreign actual refinement route");
        const auto &p = state_->paths[route.index_];
        if (!initial.level().same_owner(levels()[p.source]) || innovations.size() != p.maps.size())
            throw std::invalid_argument("refinement complete joint innovation family");
        initial.validate_receiving(guard);
        const auto k = initial.grade().value;
        std::size_t widths = initial.values().size();
        for (const auto e : p.maps)
            widths = add(widths, maps()[e].target().size(k));
        std::size_t local = mul(128, mul(add(p.maps.size(), 1), add(widths, 1)));
        const auto retained = add(state_->retained, local);
        auto g = phase(guard, retained);
        std::vector<RefinementField> prefixes{initial};
        for (std::size_t j = 0; j < p.maps.size(); ++j) {
            const auto &m = maps()[p.maps[j]];
            innovations[j].validate_receiving(guard);
            if (!innovations[j].level().same_owner(m.target()) || innovations[j].grade() != initial.grade())
                throw std::invalid_argument("refinement innovation actual stage owner/grade");
            auto value = sum(mv(m.maps()[k], prefixes.back().values(), g), innovations[j].values(), g);
            prefixes.push_back(m.target().field(initial.grade(), value, guard));
        }
        std::vector<RefinementField> channels;
        auto base = mv(p.composite[k], initial.values(), g);
        channels.push_back(levels()[p.target].field(initial.grade(), base, guard));
        for (std::size_t j = 0; j < innovations.size(); ++j) {
            auto value = innovations[j].values();
            for (std::size_t t = j + 1; t < p.maps.size(); ++t)
                value = mv(maps()[p.maps[t]].maps()[k], value, g);
            channels.push_back(levels()[p.target].field(initial.grade(), value, guard));
        }
        V sum_value(levels()[p.target].size(k));
        for (const auto &c : channels)
            sum_value = sum(sum_value, c.values(), g);
        if (sum_value != prefixes.back().values())
            throw std::logic_error("complete refinement innovation identity");
        M gram(channels.size(), channels.size());
        Q total;
        for (std::size_t i = 0; i < channels.size(); ++i)
            for (std::size_t j = 0; j < channels.size(); ++j) {
                auto q =
                    inner(channels[i].values(), levels()[p.target].metrics()[k], channels[j].values(), g);
                gram.set(i, j, q);
                total = g.add(total, q);
            }
        if (total != inner(sum_value, levels()[p.target].metrics()[k], sum_value, g))
            throw std::logic_error("refinement joint channel interaction lost");
        guard.step();
        return Accumulation(state_, route, initial, std::move(innovations), std::move(prefixes),
                            std::move(channels), std::move(gram), retained);
    }
    class LimitContract {
        friend class PreparedRefinementDiagram;
        std::shared_ptr<const State> owner_;
        std::string name_, theorem_;
        std::vector<RefinementProperty> checked_;
        std::vector<std::string> hypotheses_, observations_;
        std::size_t retained_;
        LimitContract(std::shared_ptr<const State> s, std::string name, std::string theorem,
                      std::vector<RefinementProperty> checked, std::vector<std::string> hypotheses,
                      std::vector<std::string> observations, std::size_t n)
            : owner_(std::move(s)), name_(std::move(name)), theorem_(std::move(theorem)),
              checked_(std::move(checked)), hypotheses_(std::move(hypotheses)),
              observations_(std::move(observations)), retained_(n) {}

      public:
        bool certifies_continuum_limit() const noexcept {
            return false;
        }
        const std::string &name() const & noexcept {
            return name_;
        }
        const std::string &name() const && = delete;
        const std::string &theorem() const & noexcept {
            return theorem_;
        }
        const std::string &theorem() const && = delete;
        const auto &checked_finite_properties() const & noexcept {
            return checked_;
        }
        const auto &checked_finite_properties() const && = delete;
        const auto &unresolved_hypotheses() const & noexcept {
            return hypotheses_;
        }
        const auto &unresolved_hypotheses() const && = delete;
        const auto &observations() const & noexcept {
            return observations_;
        }
        const auto &observations() const && = delete;
        void validate_receiving(G &g) const {
            g.entries(retained_);
            validate(*owner_, g);
            for (const auto *s : {&name_, &theorem_})
                for (char c : *s) {
                    (void)c;
                    g.step();
                }
            for (const auto *list : {&hypotheses_, &observations_})
                for (const auto &s : *list)
                    for (char c : s) {
                        (void)c;
                        g.step();
                    }
        }
        void require_continuum_limit(G &g) const {
            validate_receiving(g);
            throw std::domain_error(
                "finite refinement evidence does not discharge an infinite limiting theorem");
        }
    };
    LimitContract limit_contract(std::string name, std::string theorem,
                                 std::vector<RefinementProperty> checked, std::vector<std::string> unresolved,
                                 std::vector<std::string> observations, G &guard) const {
        using namespace refinement_detail;
        validate_receiving(guard);
        if (name.empty() || theorem.empty() || unresolved.empty())
            throw std::invalid_argument(
                "limiting contract requires theorem text and explicit unresolved infinite hypotheses");
        std::size_t text = add(name.size(), theorem.size());
        for (const auto p : checked) {
            switch (p) {
            case RefinementProperty::Chain:
            case RefinementProperty::MetricIsometry:
            case RefinementProperty::AdjointIntertwining:
            case RefinementProperty::HodgeIntertwining:
                break;
            default:
                throw std::invalid_argument("unknown finite limiting premise");
            }
            for (const auto &m : maps())
                m.require(p, guard);
        }
        for (const auto *list : {&unresolved, &observations})
            for (const auto &s : *list) {
                if (s.empty())
                    throw std::invalid_argument("empty limiting metadata item");
                text = add(text, s.size());
            }
        const auto retained = add(state_->retained, add(text, checked.size()));
        guard.entries(retained);
        for (std::size_t i = 0; i < text; ++i)
            guard.step();
        return LimitContract(state_, std::move(name), std::move(theorem), std::move(checked),
                             std::move(unresolved), std::move(observations), retained);
    }
};
} // namespace rfc::temporal
