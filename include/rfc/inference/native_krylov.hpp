#pragma once
#include "rfc/inference/native_factored_action.hpp"
#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include <concepts>

namespace rfc::inference {
struct NativeKrylovLimits {
    std::size_t max_degree{64};
};
namespace krylov_detail {
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
inline void admit(G &g, std::size_t base, std::size_t n, std::size_t d) {
    g.entries(add(base, add(mul(128, mul(add(n, 1), add(d, 2))), mul(128, mul(add(d, 2), add(d, 2))))));
}
inline Q dot(const V &a, const V &b, G &g) {
    if (a.size() != b.size())
        throw std::invalid_argument("Krylov pairing shape");
    Q q = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        q = g.add(q, g.mul(a[i], b[i]));
    return q;
}
inline std::size_t source_allowance(const storage::PinnedNativeAccession &p) {
    std::size_t n = 0;
    const auto e = p.endpoint();
    for (std::size_t k = 0; k < e->source.grade_count(); ++k) {
        const auto &a = e->source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        n = add(n, add(add(a.cell_count(), a.lower_index.size()),
                       mul(add(a.cell_count(), 1), add(a.cell_count(), 1))));
    }
    return mul(32, add(n, 1));
}
inline bool equal(const NativeFieldValue &a, const NativeFieldValue &b) {
    return a.carrier().same(b.carrier()) &&
           field::factored_action_detail::equal(a.sparse_values(), b.sparse_values());
}
} // namespace krylov_detail
// Field-specific exact recurrence. The rectangular Krylov table and a small
// positive Gram certificate prove minimality without materializing the operator.
class PreparedNativeKrylov {
    friend class PreparedNativeCayleyOrbit;
    using Q = krylov_detail::Q;
    using V = krylov_detail::V;
    using M = krylov_detail::M;
    using G = krylov_detail::G;
    struct State {
        NativeFactoredAction action;
        NativeFieldValue start;
        std::vector<NativeFieldValue> powers;
        V polynomial;
        numeric::PreparedExactFrames reduction;
        numeric::PreparedPositiveSemidefinite independence;
        std::vector<storage::PinnedNativeAccession> pins;
        std::size_t retained, preparation;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeKrylov(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static std::vector<storage::PinnedNativeAccession> dependencies(const NativeFactoredAction &a,
                                                                    const NativeFieldValue &x) {
        auto pins = a.state_->pins;
        for (const auto &p : x.publications())
            native_action_detail::append_pin(pins, p);
        return pins;
    }
    static std::size_t allowance(const NativeFactoredAction &a, const NativeFieldValue &x) {
        using namespace krylov_detail;
        auto n = add(a.state_->action.retained_entries(), x.sparse_values().nonzeros());
        for (const auto &p : dependencies(a, x)) {
            n = add(n, source_allowance(p));
            if (p.kind() == storage::NativeAccessionKind::Field ||
                p.kind() == storage::NativeAccessionKind::Tensor ||
                p.kind() == storage::NativeAccessionKind::Operator)
                n = add(n, mul(16, p.tensor_declaration()->tensor.values().nonzeros()));
        }
        return n;
    }
    static void validate_operands(const NativeFactoredAction &a, const NativeFieldValue &x,
                                  const NativeActionAvailability &available,
                                  const runtime::ExecutionLease &lease, G &g) {
        if (!a.input().same(a.output()) || !a.input().same(x.carrier()))
            throw std::invalid_argument("Krylov requires one actual endomorphism carrier and field owner");
        const auto pins = dependencies(a, x);
        for (const auto &p : pins)
            native_action_detail::available(p, available);
        const auto admitted = native_action_detail::limits(lease, pins, a.state_->dimension_limit);
        if (g.limits().dimension > admitted.dimension || g.limits().steps > admitted.steps ||
            g.limits().entries > admitted.entries || g.limits().integer_bits > admitted.integer_bits)
            throw std::invalid_argument("Krylov shared guard exceeds current native limits");
        lease.check_cancelled();
        g.sparse_vector(x.sparse_values());
        // Revalidate original coefficients even for a zero field or annihilating
        // action: a receiving budget cannot borrow a former permissive proof.
        (void)a.execute_with_guard(x.scaled(0, g), available, lease, g);
        for (const auto &p : pins) {
            field::hodge_frame_detail::source_values(p.endpoint()->source, g);
            if (p.kind() == storage::NativeAccessionKind::Field ||
                p.kind() == storage::NativeAccessionKind::Tensor ||
                p.kind() == storage::NativeAccessionKind::Operator)
                g.sparse_vector(p.tensor_declaration()->tensor.values());
        }
    }
    static PreparedNativeKrylov build(NativeFactoredAction action, NativeFieldValue start,
                                      const NativeActionAvailability &available,
                                      const runtime::ExecutionLease &lease, NativeKrylovLimits limits, G &g) {
        using namespace krylov_detail;
        const auto begin = g.steps(), n = start.carrier().size(), base = allowance(action, start);
        admit(g, base, n, 0);
        validate_operands(action, start, available, lease, g);
        std::vector<NativeFieldValue> powers{start};
        M table(n, 1);
        for (const auto &e : start.sparse_values().entries())
            table.set(e.index, 0, e.value);
        for (std::size_t d = 0;; ++d) {
            admit(g, base, n, d);
            numeric::PreparedExactFrames reduction(table, g);
            if (reduction.rank() == d) {
                if (reduction.kernel().cols() != 1 || reduction.kernel().get(d, 0) == 0)
                    throw std::logic_error("first Krylov dependence is not complete");
                V polynomial(add(d, 1));
                const auto lead = reduction.kernel().get(d, 0);
                for (std::size_t i = 0; i <= d; ++i)
                    polynomial[i] = g.div(reduction.kernel().get(i, 0), lead);
                // A separate LDL^T rank certificate of the preceding Euclidean
                // Gram table proves independence. This is not a grade metric.
                M prefix(n, d);
                for (const auto &[ij, q] : table.entries())
                    if (ij.second < d)
                        prefix.set(ij.first, ij.second, q);
                numeric::PreparedPositiveSemidefinite independence(
                    numeric::checked_multiply(prefix.transpose(), prefix, g), g);
                if (independence.rank() != d)
                    throw std::logic_error("Krylov independence failed");
                V residual(n);
                for (std::size_t j = 0; j <= d; ++j)
                    for (const auto &e : powers[j].sparse_values().entries())
                        residual[e.index] = g.add(residual[e.index], g.mul(polynomial[j], e.value));
                if (!numeric::is_zero(residual))
                    throw std::logic_error("full Krylov recurrence residual");
                const auto retained =
                    add(base, add(mul(8, add(table.nonzeros(), powers.size())),
                                  add(reduction.stored_entries(), add(reduction.stored_index_entries(),
                                                                      mul(8, mul(add(d, 1), add(d, 1)))))));
                g.entries(retained);
                auto pins = dependencies(action, start);
                g.step();
                return PreparedNativeKrylov(std::make_shared<const State>(
                    State{std::move(action), std::move(start), std::move(powers), std::move(polynomial),
                          std::move(reduction), std::move(independence), std::move(pins), retained,
                          g.steps() - begin}));
            }
            if (reduction.rank() != add(d, 1))
                throw std::logic_error("Krylov independence prefix");
            if (d >= limits.max_degree)
                throw std::length_error("Krylov degree cap before first dependence");
            admit(g, base, n, add(d, 1));
            powers.push_back(action.execute_with_guard(powers.back(), available, lease, g));
            M next(n, add(d, 2), table.entries());
            for (const auto &e : powers.back().sparse_values().entries()) {
                g.step();
                next.set(e.index, d + 1, e.value);
            }
            table = std::move(next);
        }
    }

  public:
    static PreparedNativeKrylov prepare_with_guard(NativeFactoredAction action, NativeFieldValue start,
                                                   const NativeActionAvailability &available,
                                                   const runtime::ExecutionLease &lease, G &g,
                                                   NativeKrylovLimits limits = {}) {
        return build(std::move(action), std::move(start), available, lease, limits, g);
    }
    static PreparedNativeKrylov prepare(NativeFactoredAction action, NativeFieldValue start,
                                        const NativeActionAvailability &available,
                                        const runtime::ExecutionLease &lease = runtime::ExecutionLease{},
                                        NativeKrylovLimits limits = {}) {
        G g(native_action_detail::limits(lease, dependencies(action, start), action.state_->dimension_limit),
            [&lease] { lease.check_cancelled(); });
        return build(std::move(action), std::move(start), available, lease, limits, g);
    }
    static PreparedNativeKrylov verify(NativeFactoredAction action, NativeFieldValue start, V claimed,
                                       const NativeActionAvailability &available,
                                       const runtime::ExecutionLease &lease = runtime::ExecutionLease{},
                                       NativeKrylovLimits limits = {}) {
        G g(native_action_detail::limits(lease, dependencies(action, start), action.state_->dimension_limit),
            [&lease] { lease.check_cancelled(); });
        g.vector(claimed);
        auto result = build(std::move(action), std::move(start), available, lease, limits, g);
        if (claimed != result.polynomial())
            throw std::invalid_argument("annihilator is not the verified monic minimal polynomial");
        return result;
    }
    const V &polynomial() const & noexcept {
        return state_->polynomial;
    }
    const V &polynomial() const && = delete;
    std::size_t degree() const noexcept {
        return state_->polynomial.size() - 1;
    }
    const NativeFactoredAction &action() const & noexcept {
        return state_->action;
    }
    const NativeFactoredAction &action() const && = delete;
    const NativeFieldValue &start() const & noexcept {
        return state_->start;
    }
    const NativeFieldValue &start() const && = delete;
    const std::vector<NativeFieldValue> &powers() const & noexcept {
        return state_->powers;
    }
    const std::vector<NativeFieldValue> &powers() const && = delete;
    const numeric::PreparedExactFrames &reduction() const & noexcept {
        return state_->reduction;
    }
    const numeric::PreparedExactFrames &reduction() const && = delete;
    const numeric::PreparedPositiveSemidefinite &independence() const & noexcept {
        return state_->independence;
    }
    const numeric::PreparedPositiveSemidefinite &independence() const && = delete;
    std::size_t retained_allowance() const noexcept {
        return state_->retained;
    }
    std::size_t preparation_steps() const noexcept {
        return state_->preparation;
    }
    void validate_receiving(const NativeActionAvailability &available, const runtime::ExecutionLease &lease,
                            G &g) const {
        krylov_detail::admit(g, state_->retained, start().carrier().size(), degree());
        validate_operands(action(), start(), available, lease, g);
        g.vector(polynomial());
        state_->reduction.validate(g);
        g.matrix(state_->independence.original());
        g.matrix(state_->independence.unit_lower_entries());
        for (const auto &[i, q] : state_->independence.diagonal_entries()) {
            (void)i;
            g.step();
            g.value(q);
        }
        for (const auto &x : powers())
            g.sparse_vector(x.sparse_values());
        g.step();
    }
};
} // namespace rfc::inference
