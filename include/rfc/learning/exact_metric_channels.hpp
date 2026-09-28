#pragma once
#include "rfc/field/augmented_green.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/learning/operator_basis.hpp"
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

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] core::Grade grade() const noexcept { return grade_; }
    [[nodiscard]] const std::vector<field::ExactMetric>& metrics() const noexcept { return *metrics_; }
    [[nodiscard]] const field::ExactHodgeOperator& hodge() const noexcept { return hodge_; }
    [[nodiscard]] const field::HodgeProjectors& projectors() const noexcept { return projectors_; }

    [[nodiscard]] numeric::RationalVector apply(const ExactMetricChannel& channel,
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
                                                              std::vector<ExactMetricChannel> candidates) {
    if (candidates.empty()) throw std::invalid_argument("exact metric channel reduction requires candidates");
    const std::size_t n=context.hodge().size();
    std::vector<std::vector<numeric::Rational>> columns(candidates.size(),std::vector<numeric::Rational>(n*n,numeric::Rational{0}));
    for (std::size_t c=0;c<candidates.size();++c) {
        for (std::size_t j=0;j<n;++j) {
            numeric::RationalVector e(n,numeric::Rational{0}); e[j]=1;
            const auto y=context.apply(candidates[c],e);
            for (std::size_t i=0;i<n;++i) columns[c][j*n+i]=y[i];
        }
    }
    std::vector<std::vector<numeric::Rational>> rows(n*n,std::vector<numeric::Rational>(candidates.size(),numeric::Rational{0}));
    for (std::size_t r=0;r<rows.size();++r) for (std::size_t c=0;c<candidates.size();++c) rows[r][c]=columns[c][r];
    const auto pivots=detail::rref_pivot_columns(rows);
    std::vector<std::vector<numeric::Rational>> basis_columns;
    for (const auto p:pivots) basis_columns.push_back(columns[p]);
    ExactMetricOperatorBasis basis;
    basis.context=&context;
    basis.candidates=std::move(candidates);
    basis.independent_indices=pivots;
    for (const auto& col:columns) basis.coordinates.push_back(detail::solve_full_column_rank(basis_columns,col));
    return basis;
}

inline std::vector<numeric::Rational> reduce_exact_metric_gains(const ExactMetricOperatorBasis& basis,
                                                                const std::vector<numeric::Rational>& gains) {
    if (gains.size()!=basis.candidates.size()) throw std::invalid_argument("exact metric gain count mismatch");
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
        if (channels_.size()!=gains_.size()) throw std::invalid_argument("exact metric layer channel/gain mismatch");
    }

    [[nodiscard]] numeric::RationalVector forward(const numeric::RationalVector& x) const {
        numeric::RationalVector out(x.size(),bias_);
        for (std::size_t c=0;c<channels_.size();++c) {
            if (gains_[c]==0) continue;
            const auto response=context_->apply(channels_[c],x);
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
                                                                numeric::Rational bias=0) {
    auto basis=reduce_exact_metric_channels(context,candidates);
    auto reduced=reduce_exact_metric_gains(basis,gains);
    CompiledExactMetricLayer layer(context,basis.basis_channels(),std::move(reduced),std::move(bias));
    return {std::move(basis),std::move(layer)};
}

} // namespace rfc::learning
