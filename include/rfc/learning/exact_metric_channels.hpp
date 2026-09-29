#pragma once
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/learning/operator_basis.hpp"
#include "rfc/machine/checked_size.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace rfc::learning {

enum class ExactMetricChannelKind : std::uint8_t {
    Identity,
    HodgeDown,
    HodgeUp,
    Hodge,
    GradientProjector,
    CurlProjector,
    HarmonicProjector,
    HarmonicCompletion,
    GreenResolvent
};

struct ExactMetricChannel {
    ExactMetricChannelKind kind{ExactMetricChannelKind::Identity};
    numeric::Rational parameter{0}; // lambda for GreenResolvent
};

inline void validate_learning_rational(const numeric::Rational& value) {
    const auto n = value.get_num(), d = value.get_den();
    if (d <= 0) throw std::invalid_argument("invalid model rational denominator");
    numeric::Integer gcd;
    mpz_gcd(gcd.get_mpz_t(), n.get_mpz_t(), d.get_mpz_t());
    if (gcd != 1) throw std::invalid_argument("noncanonical model rational");
}

inline void validate_exact_metric_channel(const ExactMetricChannel& channel) {
    validate_learning_rational(channel.parameter);
    switch (channel.kind) {
        case ExactMetricChannelKind::Identity: case ExactMetricChannelKind::HodgeDown:
        case ExactMetricChannelKind::HodgeUp: case ExactMetricChannelKind::Hodge:
        case ExactMetricChannelKind::GradientProjector: case ExactMetricChannelKind::CurlProjector:
        case ExactMetricChannelKind::HarmonicProjector: case ExactMetricChannelKind::HarmonicCompletion:
            if (channel.parameter != 0) throw std::invalid_argument("unused exact metric parameter must be zero");
            return;
        case ExactMetricChannelKind::GreenResolvent:
            if (channel.parameter.get_den() <= 0 || channel.parameter <= 0)
                throw std::invalid_argument("Green channel requires positive rational lambda");
            return;
    }
    throw std::invalid_argument("unknown exact metric channel");
}

// Bounds of this dense diagnostic/certificate path, not a smaller field space.
// Counts Rational objects; does not bound GMP limbs, solver fill, or process RSS.
struct ExactMetricAnalysisLimits {
    std::size_t max_dimension{4096};
    std::size_t max_channels{64};
    std::size_t max_rational_slots{4u * 1024u * 1024u};
    std::size_t max_action_evaluations{65536};
};
inline void check_exact_metric_analysis_size(std::size_t n, std::size_t channels,
                                             const ExactMetricAnalysisLimits& limits) {
    if (n > limits.max_dimension || channels > limits.max_channels)
        throw std::length_error("exact metric analysis dimension/channel budget");
    const auto square = machine::checked_size_mul(n, n);
    const auto slots = machine::checked_size_add(
        machine::checked_size_mul(machine::checked_size_mul(square, channels), 6),
        machine::checked_size_add(machine::checked_size_mul(machine::checked_size_mul(channels, channels),3),
            machine::checked_size_add(square,machine::checked_size_add(machine::checked_size_mul(n,3),16))));
    if (slots > limits.max_rational_slots ||
        machine::checked_size_mul(n, channels) > limits.max_action_evaluations)
        throw std::length_error("exact metric analysis allocation/action budget");
}

inline std::string_view exact_metric_channel_name(ExactMetricChannelKind kind) noexcept {
    switch (kind) {
        case ExactMetricChannelKind::Identity: return "identity";
        case ExactMetricChannelKind::HodgeDown: return "hodge_down";
        case ExactMetricChannelKind::HodgeUp: return "hodge_up";
        case ExactMetricChannelKind::Hodge: return "hodge";
        case ExactMetricChannelKind::GradientProjector: return "project_gradient";
        case ExactMetricChannelKind::CurlProjector: return "project_curl";
        case ExactMetricChannelKind::HarmonicProjector: return "project_harmonic";
        case ExactMetricChannelKind::HarmonicCompletion: return "harmonic_completion";
        case ExactMetricChannelKind::GreenResolvent: return "green_resolvent";
    }
    return "unknown";
}

class ExactMetricChannelContext {
public:
    ExactMetricChannelContext(const core::RelationalComplex& complex,
                              core::Grade grade,
                              const std::vector<field::ExactMetric>& metrics)
        : complex_(&complex), grade_(grade), metrics_(&metrics), hodge_(complex,grade,metrics), projectors_(field::exact_hodge_projectors(hodge_)) {}

    ExactMetricChannelContext(const core::RelationalComplex&&, core::Grade, const std::vector<field::ExactMetric>&) = delete;
    ExactMetricChannelContext(const core::RelationalComplex&, core::Grade, const std::vector<field::ExactMetric>&&) = delete;
    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const std::vector<field::ExactMetric>& metrics() const noexcept { return *metrics_; }
    [[nodiscard]] const field::ExactHodgeOperator& hodge() const noexcept { return hodge_; }
    [[nodiscard]] const field::HodgeProjectors& projectors() const noexcept { return projectors_; }

    [[nodiscard]] numeric::RationalVector apply(const ExactMetricChannel& channel,
                                                 const numeric::RationalVector& x) const {
        validate_exact_metric_channel(channel);
        if (x.size() != hodge_.size()) throw std::invalid_argument("exact metric input carrier size mismatch");
        for (const auto& value : x) validate_learning_rational(value);
        return apply_validated(channel,x);
    }

private:
    friend class CompiledExactMetricLayer;
    // Prepared layers validate their immutable channels at construction and the
    // complete input once per call, not once again for every active channel.
    [[nodiscard]] numeric::RationalVector apply_validated(const ExactMetricChannel& channel,
                                                         const numeric::RationalVector& x) const {
        switch (channel.kind) {
            case ExactMetricChannelKind::Identity: return x;
            case ExactMetricChannelKind::HodgeDown: return hodge_.apply_down(x);
            case ExactMetricChannelKind::HodgeUp: return hodge_.apply_up(x);
            case ExactMetricChannelKind::Hodge: return hodge_.apply(x);
            case ExactMetricChannelKind::GradientProjector: return projectors_.project_gradient(x);
            case ExactMetricChannelKind::CurlProjector: return projectors_.project_curl(x);
            case ExactMetricChannelKind::HarmonicProjector: return projectors_.project_harmonic(x);
            case ExactMetricChannelKind::HarmonicCompletion:
                return numeric::add(hodge_.apply(x),projectors_.project_harmonic(x));
            case ExactMetricChannelKind::GreenResolvent: {
                const auto solved=field::exact_green_apply(hodge_,channel.parameter,x);
                if (!solved.exact) throw std::runtime_error("exact Green channel residual failed");
                return solved.value;
            }
        }
        throw std::logic_error("unknown exact metric channel");
    }

private:
    const core::RelationalComplex* complex_{};
    core::Grade grade_{};
    const std::vector<field::ExactMetric>* metrics_{};
    field::ExactHodgeOperator hodge_;
    field::HodgeProjectors projectors_;
};

struct ExactMetricOperatorBasis {
    const ExactMetricChannelContext* context{};
    std::vector<ExactMetricChannel> candidates;
    std::vector<std::size_t> independent_indices;
    std::vector<std::vector<numeric::Rational>> coordinates;

    [[nodiscard]] std::size_t rank() const noexcept { return independent_indices.size(); }
    [[nodiscard]] std::vector<ExactMetricChannel> basis_channels() const {
        std::vector<ExactMetricChannel> out;
        out.reserve(independent_indices.size());
        for (const auto i:independent_indices) out.push_back(candidates.at(i));
        return out;
    }
};

inline ExactMetricOperatorBasis reduce_exact_metric_channels(const ExactMetricChannelContext& context,
                                                              std::vector<ExactMetricChannel> candidates,
                                                              const ExactMetricAnalysisLimits& limits = {}) {
    if (candidates.empty()) throw std::invalid_argument("exact metric channel reduction requires candidates");
    for (const auto& channel : candidates) validate_exact_metric_channel(channel);
    const std::size_t n=context.hodge().size();
    check_exact_metric_analysis_size(n, candidates.size(), limits);
    const auto square = machine::checked_size_mul(n, n);
    std::vector<std::vector<numeric::Rational>> columns(candidates.size(),std::vector<numeric::Rational>(square,numeric::Rational{0}));
    for (std::size_t c=0;c<candidates.size();++c) {
        for (std::size_t j=0;j<n;++j) {
            numeric::RationalVector e(n,numeric::Rational{0}); e[j]=1;
            const auto y=context.apply(candidates[c],e);
            for (std::size_t i=0;i<n;++i) columns[c][j*n+i]=y[i];
        }
    }
    std::vector<std::vector<numeric::Rational>> rows(square,std::vector<numeric::Rational>(candidates.size(),numeric::Rational{0}));
    for (std::size_t r=0;r<rows.size();++r) for (std::size_t c=0;c<candidates.size();++c) rows[r][c]=columns[c][r];
    const auto pivots=detail::rref_pivot_columns(rows);
    std::vector<std::vector<numeric::Rational>> basis_columns;
    for (const auto p:pivots) basis_columns.push_back(columns[p]);
    ExactMetricOperatorBasis basis;
    basis.context=&context;
    basis.candidates=std::move(candidates);
    basis.independent_indices=pivots;
    basis.coordinates.reserve(basis.candidates.size());
    for (const auto& col:columns) basis.coordinates.push_back(detail::solve_full_column_rank(basis_columns,col));
    return basis;
}

inline std::vector<numeric::Rational> reduce_exact_metric_gains(const ExactMetricOperatorBasis& basis,
                                                                const std::vector<numeric::Rational>& gains) {
    if (!basis.context || basis.coordinates.size() != basis.candidates.size())
        throw std::invalid_argument("invalid exact metric basis coordinate count");
    for (const auto& row : basis.coordinates) if (row.size() != basis.rank())
        throw std::invalid_argument("invalid exact metric basis coordinate width");
    if (gains.size()!=basis.candidates.size()) throw std::invalid_argument("exact metric gain count mismatch");
    for (const auto& gain : gains) validate_learning_rational(gain);
    for (const auto& row : basis.coordinates)
        for (const auto& coefficient : row) validate_learning_rational(coefficient);
    for (const auto index : basis.independent_indices)
        if (index >= basis.candidates.size()) throw std::invalid_argument("invalid exact metric basis index");
    std::vector<numeric::Rational> reduced(basis.rank(),numeric::Rational{0});
    for (std::size_t j=0;j<gains.size();++j) for (std::size_t i=0;i<basis.rank();++i) reduced[i]+=gains[j]*basis.coordinates[j][i];
    for (auto& v:reduced) v.canonicalize();
    return reduced;
}

class CompiledExactMetricLayer {
public:
    CompiledExactMetricLayer(const ExactMetricChannelContext& context,
                             std::vector<ExactMetricChannel> channels,
                             std::vector<numeric::Rational> gains,
                             numeric::Rational bias = 0)
        : context_(&context), channels_(std::move(channels)), gains_(std::move(gains)), bias_(std::move(bias)) {
        for (const auto& channel : channels_) validate_exact_metric_channel(channel);
        validate_learning_rational(bias_);
        for (const auto& gain : gains_) validate_learning_rational(gain);
        if (channels_.size()!=gains_.size()) throw std::invalid_argument("exact metric layer channel/gain mismatch");
    }

    CompiledExactMetricLayer(const ExactMetricChannelContext&&,
        std::vector<ExactMetricChannel>, std::vector<numeric::Rational>, numeric::Rational = 0) = delete;
    [[nodiscard]] numeric::RationalVector forward(const numeric::RationalVector& x) const {
        if (x.size() != context_->hodge().size()) throw std::invalid_argument("exact metric layer carrier size mismatch");
        for (const auto& value : x) validate_learning_rational(value);
        numeric::RationalVector out(x.size(),bias_);
        for (std::size_t c=0;c<channels_.size();++c) {
            if (gains_[c]==0) continue;
            const auto response=context_->apply_validated(channels_[c],x);
            for (std::size_t i=0;i<out.size();++i) out[i]+=gains_[c]*response[i];
        }
        return out;
    }
private:
    const ExactMetricChannelContext* context_{};
    std::vector<ExactMetricChannel> channels_;
    std::vector<numeric::Rational> gains_;
    numeric::Rational bias_{};
};

struct CompiledExactMetricLayerPlan {
    ExactMetricOperatorBasis basis;
    CompiledExactMetricLayer layer;
};

inline CompiledExactMetricLayerPlan compile_exact_metric_layer(const ExactMetricChannelContext& context,
                                                                const std::vector<ExactMetricChannel>& candidates,
                                                                const std::vector<numeric::Rational>& gains,
                                                                numeric::Rational bias=0,
                                                                const ExactMetricAnalysisLimits& limits = {}) {
    auto basis=reduce_exact_metric_channels(context,candidates,limits);
    auto reduced=reduce_exact_metric_gains(basis,gains);
    CompiledExactMetricLayer layer(context,basis.basis_channels(),std::move(reduced),std::move(bias));
    return {std::move(basis),std::move(layer)};
}

inline ExactMetricOperatorBasis reduce_exact_metric_channels(const ExactMetricChannelContext&&,
    std::vector<ExactMetricChannel>, const ExactMetricAnalysisLimits& = {}) = delete;
inline CompiledExactMetricLayerPlan compile_exact_metric_layer(const ExactMetricChannelContext&&,
    const std::vector<ExactMetricChannel>&, const std::vector<numeric::Rational>&,
    numeric::Rational = 0, const ExactMetricAnalysisLimits& = {}) = delete;

} // namespace rfc::learning
