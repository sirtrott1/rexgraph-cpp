#pragma once

#include "rfc/inference/relational_field_interface.hpp"

namespace rfc::inference {
enum class RelationalComparisonChannel : std::uint8_t { T, RawG, C, F };
enum class RelationalComparisonParticipation : std::uint8_t { Share, LegacyCount };
struct RelationalComparisonWeight { core::CellId cell; numeric::Rational value; };
struct RelationalComparisonDeclaration {
    std::uint64_t key{}, revision{};
    core::Grade grade;
    std::vector<RelationalComparisonWeight> weights;
    RelationalComparisonParticipation participation{RelationalComparisonParticipation::Share};
};
namespace relational_comparison_detail {
using namespace field_interface_detail;
using Q = numeric::Rational;
using V = numeric::RationalVector;
inline Q absolute(const Q& q) { return q < 0 ? Q{-q} : q; }
inline void accumulate(V& target, const V& value, const Q& weight, Guard& guard) {
    require(target.size() == value.size(), "comparison common stalk carrier mismatch"); guard.value(weight);
    for (std::size_t i = 0; i < target.size(); ++i) target[i] = guard.add(target[i], guard.mul(weight, value[i]));
}
inline V difference(V a, const V& b, const Q& scale, Guard& guard) { accumulate(a, b, scale, guard); return a; }
inline Q pairing(const V& x, const V& y, Guard& guard) {
    require(x.size() == y.size(), "comparison quadrance carrier mismatch"); Q q{};
    for (std::size_t i = 0; i < x.size(); ++i) q = guard.add(q, guard.mul(x[i], y[i]));
    return q;
}
struct Flag {
    std::size_t upper{}, lower{}, slot{};
    Q coefficient, beta, weight, signed_weight, unsigned_weight, mass;
    Matrix phi;
};
struct LowerMass { Q beta{}, positive{}, negative{}; };
struct State {
    OwnedRelationalFieldProgram program;
    RelationalComparisonDeclaration declaration;
    std::vector<std::size_t> upper, lower;
    std::vector<Matrix> upper_metrics, lower_metrics;
    std::vector<numeric::PreparedExactSolve> upper_solves;
    std::vector<Flag> flags;
    std::vector<LowerMass> masses;
    std::size_t retained{};
    State(OwnedRelationalFieldProgram p, RelationalComparisonDeclaration d, Guard& guard)
        : program(std::move(p)), declaration(std::move(d)) {
        require(declaration.key && declaration.revision && declaration.grade.value,
            "comparison declaration needs identity/version/positive grade");
        require(declaration.participation == RelationalComparisonParticipation::Share ||
            declaration.participation == RelationalComparisonParticipation::LegacyCount, "unsupported comparison participation policy");
        const auto publication = program.publication(); const auto& source = publication.endpoint().source;
        require(declaration.grade.value < source.grade_count(), "comparison original grade absent"); source_values(source, guard);
        const auto g = declaration.grade, lo = grade(g.value-1u);
        upper = program.fibers().fibers().stalks().offsets(g); lower = program.fibers().fibers().stalks().offsets(lo);
        guard.dimension(upper.back()); guard.dimension(lower.back());
        require(declaration.weights.size()+1 == upper.size(), "comparison weights omit actual original upper relations");
        for (std::size_t i = 0; i < declaration.weights.size(); ++i) {
            guard.step(); auto& w = declaration.weights[i];
            require(w.cell == core::CellId{g, static_cast<std::uint32_t>(i)}, "comparison weight identity/order");
            guard.value(w.value); w.value = numeric::canonical_rational(w.value);
            upper_metrics.push_back(stalk(program, w.cell).metric); upper_solves.emplace_back(upper_metrics.back(), guard);
            retained = add(retained, add(upper_metrics.back().nonzeros(), upper_solves.back().stored_entries()));
        }
        for (std::size_t i = 0; i+1 < lower.size(); ++i) {
            lower_metrics.push_back(stalk(program, {lo, static_cast<std::uint32_t>(i)}).metric);
            retained = add(retained, lower_metrics.back().nonzeros()); masses.emplace_back();
        }
        const auto& arena = source.grade(g);
        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
            guard.step(); const auto declared = std::find_if(program.declaration().occurrences.begin(), program.declaration().occurrences.end(),
                [&](const auto& o) { return o.grade == g && o.occurrence == slot; });
            require(declared != program.declaration().occurrences.end() && declared->role == storage::TemporalTensorRole::CoefficientIndependentMap,
                "comparison requires actual explicit Phi at every original flag, including inactive/zero");
            const auto c = arena.aspects.get(source.signature().existence_aspect(), slot) ? source.incidence_coefficient(g, slot) : Q{0};
            const auto beta = declaration.participation == RelationalComparisonParticipation::Share ? absolute(c) : Q{c != 0 ? 1 : 0};
            const auto e = arena.upper_index[slot], v = arena.lower_index[slot]; const auto w = declaration.weights[e].value;
            const auto signed_weight = guard.mul(w, c), unsigned_weight = guard.mul(w, absolute(c));
            flags.push_back({e, v, slot, c, beta, w, signed_weight, unsigned_weight, absolute(signed_weight), declared->map});
            auto& mass = masses[v]; mass.beta = guard.add(mass.beta, beta);
            if (c > 0) mass.positive = guard.add(mass.positive, absolute(signed_weight));
            if (c < 0) mass.negative = guard.add(mass.negative, absolute(signed_weight));
            retained = add(retained, add(9, declared->map.nonzeros())); guard.entries(retained);
        }
        retained = add(retained, add(add(upper.size(), lower.size()), machine::checked_size_mul(masses.size(), 3)));
        guard.entries(retained);
    }
    void validate(Guard& guard) const {
        const auto p = program.publication(); source_values(p.endpoint().source, guard); guard.entries(retained);
        for (const auto& m : lower_metrics) guard.matrix(m);
        for (const auto& solve : upper_solves) (void)solve.factor_receipt(guard);
        for (const auto& f : flags) {
            guard.matrix(f.phi); for (const auto* q : {&f.coefficient, &f.beta, &f.weight, &f.signed_weight, &f.unsigned_weight, &f.mass}) guard.value(*q);
        }
    }
};
struct Sums {
    V signed_field, unsigned_field, beta_field, positive_field, negative_field;
};
inline Tensor flatten(const std::vector<V>& values, Guard& guard) {
    V flat; for (const auto& value : values) {
        guard.dimension(add(flat.size(), value.size())); flat.insert(flat.end(), value.begin(), value.end());
    }
    return tensor(flat, guard);
}
} // namespace relational_comparison_detail

class RelationalComparisonResult;
class RelationalComparisonPairReading;
class PreparedRelationalComparisonForms {
public:
    PreparedRelationalComparisonForms(OwnedRelationalFieldProgram program, RelationalComparisonDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        const auto frozen = declaration; const auto pin = program; numeric::ExactActionGuard guard(limits, std::move(check));
        state_ = std::make_shared<const relational_comparison_detail::State>(pin, frozen, guard);
    }
    OwnedRelationalFieldProgram program() const { return state_->program; }
    const RelationalComparisonDeclaration& declaration() const & { return state_->declaration; }
    const RelationalComparisonDeclaration& declaration() const && = delete;
    RelationalComparisonResult apply(RelationalComparisonChannel channel, numeric::SparseRationalTensor field,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    numeric::ExactMatrix operator_matrix(RelationalComparisonChannel channel, std::size_t max_entries,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class RelationalComparisonResult;
    explicit PreparedRelationalComparisonForms(std::shared_ptr<const relational_comparison_detail::State> state) : state_(std::move(state)) {}
    std::shared_ptr<const relational_comparison_detail::State> state_;
};
class RelationalComparisonResult {
    struct State {
        PreparedRelationalComparisonForms forms;
        RelationalComparisonChannel channel;
        numeric::SparseRationalTensor input, output, signed_boundary, unsigned_boundary;
        std::vector<numeric::SparseRationalTensor> occurrences;
        numeric::Rational quadrance;
    };
public:
    PreparedRelationalComparisonForms forms() const { return state_->forms; }
    RelationalComparisonChannel channel() const noexcept { return state_->channel; }
    const numeric::SparseRationalTensor& input() const & { return state_->input; }
    const numeric::SparseRationalTensor& input() const && = delete;
    const numeric::SparseRationalTensor& output() const & { return state_->output; }
    const numeric::SparseRationalTensor& output() const && = delete;
    const numeric::SparseRationalTensor& signed_boundary_field() const & { return state_->signed_boundary; }
    const numeric::SparseRationalTensor& signed_boundary_field() const && = delete;
    const numeric::SparseRationalTensor& unsigned_boundary_field() const & { return state_->unsigned_boundary; }
    const numeric::SparseRationalTensor& unsigned_boundary_field() const && = delete;
    const std::vector<numeric::SparseRationalTensor>& occurrence_fields() const & { return state_->occurrences; }
    const std::vector<numeric::SparseRationalTensor>& occurrence_fields() const && = delete;
    const numeric::Rational& quadrance() const & { return state_->quadrance; }
    const numeric::Rational& quadrance() const && = delete;
    RelationalComparisonPairReading pair_residual(std::size_t p, std::size_t q, RelationalComparisonChannel channel,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    friend class PreparedRelationalComparisonForms;
    explicit RelationalComparisonResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalComparisonResult PreparedRelationalComparisonForms::apply(RelationalComparisonChannel channel, numeric::SparseRationalTensor field,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_comparison_detail;
    const auto pin = state_; const auto frozen = field; Guard guard(limits, std::move(check)); pin->validate(guard);
    require(channel == RelationalComparisonChannel::T || channel == RelationalComparisonChannel::RawG ||
        channel == RelationalComparisonChannel::C || channel == RelationalComparisonChannel::F, "unknown relational comparison channel");
    admit_field(frozen, pin->upper.back(), guard); const auto input = frozen.values().dense();
    std::vector<Sums> sums; std::vector<V> upper_values, covectors;
    for (std::size_t e = 0; e+1 < pin->upper.size(); ++e) {
        guard.step(); upper_values.emplace_back(input.begin()+static_cast<std::ptrdiff_t>(pin->upper[e]), input.begin()+static_cast<std::ptrdiff_t>(pin->upper[e+1]));
        covectors.emplace_back(pin->upper[e+1]-pin->upper[e]);
    }
    for (std::size_t v = 0; v+1 < pin->lower.size(); ++v) {
        const auto n = pin->lower[v+1]-pin->lower[v]; sums.push_back({V(n), V(n), V(n), V(n), V(n)});
    }
    std::size_t scratch = add(machine::checked_size_mul(pin->lower.back(), 5), machine::checked_size_mul(pin->upper.back(), 3));
    guard.entries(add(pin->retained, scratch)); std::vector<Tensor> occurrences;
    for (const auto& f : pin->flags) {
        guard.step(); const auto value = numeric::checked_apply(f.phi, upper_values[f.upper], guard);
        scratch = add(scratch, value.size()); guard.entries(add(pin->retained, scratch)); occurrences.push_back(tensor(value, guard));
        auto& s = sums[f.lower]; accumulate(s.signed_field, value, f.signed_weight, guard); accumulate(s.unsigned_field, value, f.unsigned_weight, guard);
        accumulate(s.beta_field, value, f.beta, guard);
        if (f.coefficient > 0) accumulate(s.positive_field, value, f.unsigned_weight, guard);
        if (f.coefficient < 0) accumulate(s.negative_field, value, f.unsigned_weight, guard);
    }
    for (const auto& f : pin->flags) {
        guard.step(); const auto& sum = sums[f.lower]; const auto& mass = pin->masses[f.lower]; const auto value = occurrences[f.slot].values().dense();
        V common(value.size());
        if (channel == RelationalComparisonChannel::T) accumulate(common, sum.signed_field, f.signed_weight, guard);
        if (channel == RelationalComparisonChannel::RawG) accumulate(common, sum.unsigned_field, f.unsigned_weight, guard);
        if (channel == RelationalComparisonChannel::C) {
            accumulate(common, value, guard.mul(mass.beta, f.beta), guard); accumulate(common, sum.beta_field, Q{-f.beta}, guard);
        }
        if (channel == RelationalComparisonChannel::F) {
            if (f.coefficient > 0) {
                accumulate(common, value, guard.mul(guard.mul(2, mass.negative), f.mass), guard);
                accumulate(common, sum.negative_field, guard.mul(-2, f.unsigned_weight), guard);
            } else if (f.coefficient < 0) {
                accumulate(common, value, guard.mul(guard.mul(2, mass.positive), f.mass), guard);
                accumulate(common, sum.positive_field, guard.mul(-2, f.unsigned_weight), guard);
            }
        }
        const auto metric_field = numeric::checked_apply(pin->lower_metrics[f.lower], common, guard);
        const auto adjoint = numeric::checked_transpose_apply(f.phi, metric_field, guard);
        accumulate(covectors[f.upper], adjoint, 1, guard);
    }
    std::vector<V> output; Q quadrance{};
    for (std::size_t e = 0; e < covectors.size(); ++e) {
        guard.step(); quadrance = guard.add(quadrance, pairing(upper_values[e], covectors[e], guard));
        output.push_back(pin->upper_solves[e].apply(covectors[e], guard));
    }
    require(quadrance >= 0, "relational comparison squared-norm form produced negative quadrance");
    std::vector<V> signed_fields, unsigned_fields;
    for (const auto& s : sums) { signed_fields.push_back(s.signed_field); unsigned_fields.push_back(s.unsigned_field); }
    return RelationalComparisonResult({PreparedRelationalComparisonForms(pin), channel, frozen, flatten(output, guard),
        flatten(signed_fields, guard), flatten(unsigned_fields, guard), std::move(occurrences), quadrance});
}
class RelationalComparisonPairReading {
public:
    RelationalComparisonResult parent() const { return parent_; }
    std::size_t first() const noexcept { return first_; }
    std::size_t second() const noexcept { return second_; }
    core::CellId lower() const noexcept { return lower_; }
    RelationalComparisonChannel channel() const noexcept { return channel_; }
    const numeric::SparseRationalTensor& residual() const & { return residual_; }
    const numeric::SparseRationalTensor& residual() const && = delete;
    const numeric::ExactMatrix& metric() const & { return metric_; }
    const numeric::ExactMatrix& metric() const && = delete;
    const numeric::Rational& weight() const & { return weight_; }
    const numeric::Rational& weight() const && = delete;
    const numeric::Rational& quadrance() const & { return quadrance_; }
    const numeric::Rational& quadrance() const && = delete;
    bool admitted() const noexcept { return admitted_; }
private:
    friend class RelationalComparisonResult;
    RelationalComparisonPairReading(RelationalComparisonResult parent, std::size_t p, std::size_t q, core::CellId lower,
        RelationalComparisonChannel channel, numeric::SparseRationalTensor residual, numeric::ExactMatrix metric,
        numeric::Rational weight, numeric::Rational quadrance, bool admitted)
        : parent_(std::move(parent)), first_(p), second_(q), lower_(lower), channel_(channel), residual_(std::move(residual)),
          metric_(std::move(metric)), weight_(std::move(weight)), quadrance_(std::move(quadrance)), admitted_(admitted) {}
    RelationalComparisonResult parent_;
    std::size_t first_{}, second_{};
    core::CellId lower_;
    RelationalComparisonChannel channel_;
    numeric::SparseRationalTensor residual_;
    numeric::ExactMatrix metric_;
    numeric::Rational weight_, quadrance_;
    bool admitted_{};
};
inline RelationalComparisonPairReading RelationalComparisonResult::pair_residual(std::size_t p, std::size_t q,
    RelationalComparisonChannel channel, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_comparison_detail;
    const auto pin = *this; const auto state = state_; Guard guard(limits, std::move(check)); const auto forms = state->forms.state_; forms->validate(guard);
    require(p < forms->flags.size() && q < forms->flags.size() && p != q && forms->flags[p].lower == forms->flags[q].lower,
        "pair comparison needs distinct original flags at the same actual lower interface");
    require(channel == RelationalComparisonChannel::C || channel == RelationalComparisonChannel::F, "pair residual requires C or F");
    const auto& a = forms->flags[p]; const auto& b = forms->flags[q]; Q weight{}, sign{1}; bool admitted = true;
    if (channel == RelationalComparisonChannel::C) weight = guard.mul(a.beta, b.beta);
    else {
        const auto product = guard.mul(a.weight, b.weight); admitted = guard.mul(a.coefficient, b.coefficient) < 0;
        if (admitted && product != 0) { sign = product > 0 ? Q{1} : Q{-1}; weight = guard.mul(2, absolute(guard.mul(product, guard.mul(a.coefficient, b.coefficient)))); }
    }
    const auto a_field = state->occurrences[p].values().dense(), b_field = state->occurrences[q].values().dense();
    const auto residual = difference(a_field, b_field, Q{-sign}, guard); const auto& metric = forms->lower_metrics[a.lower];
    const auto energy = guard.mul(weight, pairing(residual, numeric::checked_apply(metric, residual, guard), guard));
    return {pin, p, q, {grade(forms->declaration.grade.value-1u), static_cast<std::uint32_t>(a.lower)}, channel,
        tensor(residual, guard), metric, weight, energy, admitted};
}
inline numeric::ExactMatrix PreparedRelationalComparisonForms::operator_matrix(RelationalComparisonChannel channel, std::size_t max_entries,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace relational_comparison_detail;
    const auto pin = *this; const auto state = state_; Guard guard(limits, std::move(check)); state->validate(guard);
    Matrix out(state->upper.back(), state->upper.back());
    for (std::size_t j = 0; j < out.cols(); ++j) {
        guard.step(); V basis(out.cols()); basis[j] = 1; const auto result = pin.apply(channel, tensor(basis, guard), limits, [&] { guard.step(); });
        for (const auto& e : result.output().values().entries()) {
            guard.step(); if (out.nonzeros() == max_entries) throw std::length_error("bounded comparison operator matrix export");
            out.set(e.index, j, e.value); guard.entries(out.nonzeros());
        }
    }
    return out;
}
} // namespace rfc::inference
