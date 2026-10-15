#pragma once

#include "rfc/field/prepared_hodge_frames.hpp"
#include <optional>
#include <span>

namespace rfc::learning {

struct AssertionDiagnosis {
    std::shared_ptr<const core::RelationalComplex> source;
    core::Grade grade;
    numeric::RationalVector input,gradient,curl,harmonic,residual;
    numeric::RationalVector minimum_potential,contradiction_boundary;
    numeric::Rational residual_squared_norm{},contradiction_pairing{};
    bool consistent{},exact{};
};
struct AssertionCorruptionSignature {
    std::shared_ptr<const core::RelationalComplex> source;
    core::Grade grade;
    std::size_t index{};
    core::RelationKey retained_key{};
    numeric::RationalVector residual;
    numeric::Rational squared_norm{};
    bool detectable{};
};
struct AssertionCorruptionCandidate {
    AssertionCorruptionSignature signature;
    std::optional<numeric::Rational> amplitude;
    numeric::Rational score{},unexplained_squared_norm{};
    bool explains_residual{},compatible_with_nonzero_single_corruption{};
};
struct KnownAssertionRepair {
    std::size_t index{};
    bool identifiable{},compatible{};
    std::optional<numeric::Rational> amplitude;
    std::optional<AssertionDiagnosis> corrected;
};

// Complete finite assertion diagnostics from companion §§28-29. An assertion
// field y is consistent when y = B^dagger phi. For literal equation covectors
// c use analyze_covectors(c): c = B^T M_lower phi. In the ideal unit tower this
// is exactly B^T phi = c. Metrics, source identities and the full potential
// kernel are retained. Only requested corruption signatures are evaluated.
class PreparedAssertionDiagnosis {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct State {
        field::PreparedHodgeFrames frames;
        numeric::PreparedExactSolve lower_metric,upper_metric;
        M gauge;
        std::optional<numeric::PreparedExactSolve> gauge_gram;
        State(std::shared_ptr<const core::RelationalComplex> source,std::vector<M> forms,
              core::Grade grade,Guard& guard)
            :State(field::PreparedHodgeFrames(std::move(source),std::move(forms),grade,guard),guard){}
        State(field::PreparedHodgeFrames prepared,Guard& guard)
            :frames(std::move(prepared)),lower_metric(checked_lower(frames.metrics(),frames.grade()),guard),
             upper_metric(frames.metrics().at(frames.grade().value),guard) {
            gauge=field::hodge_frame_detail::solve_columns(lower_metric,frames.gradient_reduction().kernel(),guard);
            if(gauge.cols()) {
                const auto measured=numeric::checked_multiply(frames.metrics().at(frames.grade().value-1u),gauge,guard);
                gauge_gram.emplace(numeric::checked_multiply(gauge.transpose(),measured,guard),guard);
            }
        }
        static const M& checked_lower(const std::vector<M>& forms,core::Grade grade) {
            if(grade.value==0 || grade.value>=forms.size())
                throw std::invalid_argument("assertion diagnosis needs a positive source grade");
            return forms.at(grade.value-1u);
        }
    };
    std::shared_ptr<const State> state_;
    static V sum(const V& a,const V& b,Guard& guard) {
        if(a.size()!=b.size())throw std::invalid_argument("assertion field carrier mismatch");
        V out(a.size());for(std::size_t i=0;i<a.size();++i)out[i]=guard.add(a[i],b[i]);return out;
    }
    static Q dot(const V& a,const V& b,Guard& guard) {
        if(a.size()!=b.size())throw std::invalid_argument("assertion pairing carrier mismatch");
        Q out{0};for(std::size_t i=0;i<a.size();++i)out=guard.add(out,guard.mul(a[i],b[i]));return out;
    }
    Q inner(const V& a,const V& b,Guard& guard) const {
        return dot(a,numeric::checked_apply(state().frames.metrics().at(grade().value),b,guard),guard);
    }
    V potential(const field::HodgeFrameDecomposition& parts,Guard& guard) const {
        const auto& s=state();const auto& reduction=s.frames.gradient_reduction();
        V dual(reduction.original().cols());
        for(std::size_t i=0;i<reduction.pivots().size();++i)dual[reduction.pivots()[i]]=parts.gradient_coordinates.at(i);
        auto result=s.lower_metric.apply(dual,guard);
        if(s.gauge_gram) {
            const auto measured=numeric::checked_apply(s.frames.metrics().at(grade().value-1u),result,guard);
            const auto free=s.gauge_gram->apply(numeric::checked_transpose_apply(s.gauge,measured,guard),guard);
            const auto null_part=numeric::checked_apply(s.gauge,free,guard);
            for(std::size_t i=0;i<result.size();++i)result[i]=guard.sub(result[i],null_part[i]);
        }
        const auto lower_measured=numeric::checked_apply(s.frames.metrics().at(grade().value-1u),result,guard);
        const auto covector=numeric::checked_apply(reduction.original(),lower_measured,guard);
        if(s.upper_metric.apply(covector,guard)!=parts.gradient)
            throw std::logic_error("assertion minimum potential fails original equation");
        if(!numeric::is_zero(numeric::checked_transpose_apply(s.gauge,lower_measured,guard)))
            throw std::logic_error("assertion potential fails complete kernel gauge");
        return result;
    }
    const State& state() const {
        if(!state_)throw std::logic_error("moved-from assertion diagnosis owner");
        return *state_;
    }
public:
    PreparedAssertionDiagnosis(field::PreparedHodgeFrames prepared,Guard& guard)
        :state_(std::make_shared<const State>(std::move(prepared),guard)){}
    PreparedAssertionDiagnosis(std::shared_ptr<const core::RelationalComplex> source,std::vector<M> metrics,
                               core::Grade grade,Guard& guard)
        :state_(std::make_shared<const State>(std::move(source),std::move(metrics),grade,guard)){}
    explicit PreparedAssertionDiagnosis(std::shared_ptr<const core::RelationalComplex> source,
             core::Grade grade=core::Grade{1},numeric::ExactActionLimits limits={}) {
        if(!source)throw std::invalid_argument("assertion diagnosis requires source owner");
        Guard guard(limits);std::vector<M> metrics;
        for(std::size_t k=0;k<source->grade_count();++k) {
            const auto n=source->grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
            guard.dimension(n);guard.entries(n);metrics.push_back(M::identity(n));
        }
        state_=std::make_shared<const State>(std::move(source),std::move(metrics),grade,guard);
    }
    std::shared_ptr<const core::RelationalComplex> source() const {return state().frames.source();}
    core::Grade grade() const {return state().frames.grade();}
    const M& potential_gauge_frame() const & {return state().gauge;}
    const M& potential_gauge_frame() const &&=delete;
    const field::PreparedHodgeFrames& frames() const & {return state().frames;}
    const field::PreparedHodgeFrames& frames() const &&=delete;
    AssertionDiagnosis analyze(const V& input,Guard& guard) const {
        const auto& s=state();const auto canonical=guard.canonical_input(input);
        auto parts=s.frames.decompose(canonical,guard);
        AssertionDiagnosis out;out.source=source();out.grade=grade();out.input=canonical;
        out.minimum_potential=potential(parts,guard);
        out.gradient=std::move(parts.gradient);out.curl=std::move(parts.curl);out.harmonic=std::move(parts.harmonic);
        out.residual=sum(out.curl,out.harmonic,guard);
        out.contradiction_boundary=numeric::checked_transpose_apply(s.frames.gradient_reduction().original(),out.residual,guard);
        out.residual_squared_norm=inner(out.residual,out.residual,guard);
        out.contradiction_pairing=inner(out.residual,canonical,guard);
        out.consistent=numeric::is_zero(out.residual);
        out.exact=numeric::is_zero(out.contradiction_boundary) &&
            out.contradiction_pairing==out.residual_squared_norm && sum(out.gradient,out.residual,guard)==canonical;
        if(!out.exact)throw std::logic_error("assertion contradiction certificate failed");
        return out;
    }
    AssertionDiagnosis analyze(const V& input,numeric::ExactActionLimits limits={}) const {
        Guard guard(limits);return analyze(input,guard);
    }
    AssertionDiagnosis analyze_covectors(const V& covectors,Guard& guard) const {
        return analyze(state().upper_metric.apply(covectors,guard),guard);
    }
    AssertionCorruptionSignature signature(std::size_t index,Guard& guard) const {
        const auto owner=source();const auto& arena=owner->grade(grade());
        if(index>=arena.cell_count())throw std::out_of_range("assertion signature original index");
        V basis(arena.cell_count());basis[index]=1;
        const auto parts=state().frames.decompose(std::move(basis),guard);
        auto residual=sum(parts.curl,parts.harmonic,guard);const auto norm=inner(residual,residual,guard);
        return {owner,grade(),index,arena.relation_identities.at(index),std::move(residual),norm,norm!=0};
    }
    std::vector<AssertionCorruptionCandidate> candidates(const V& input,std::span<const std::size_t> selected,
                                                       Guard& guard) const {
        const auto diagnosis=analyze(input,guard);std::vector<AssertionCorruptionCandidate> out;
        guard.entries(machine::checked_size_mul(selected.size(),source()->grade(grade()).cell_count()));
        for(const auto index:selected) {
            AssertionCorruptionCandidate item;item.signature=signature(index,guard);
            item.unexplained_squared_norm=diagnosis.residual_squared_norm;
            if(item.signature.detectable) {
                const auto pairing=inner(diagnosis.residual,item.signature.residual,guard);
                item.amplitude=guard.div(pairing,item.signature.squared_norm);
                item.score=guard.div(guard.mul(pairing,pairing),item.signature.squared_norm);
                item.unexplained_squared_norm=guard.sub(diagnosis.residual_squared_norm,item.score);
                item.explains_residual=item.unexplained_squared_norm==0;
                item.compatible_with_nonzero_single_corruption=item.explains_residual && *item.amplitude!=0;
            } else {
                item.explains_residual=diagnosis.consistent;
                item.compatible_with_nonzero_single_corruption=diagnosis.consistent;
            }
            out.push_back(std::move(item));
        }
        return out;
    }
    KnownAssertionRepair repair_known(const V& input,std::size_t index,Guard& guard) const {
        const std::size_t selected[]{index};const auto candidate=candidates(input,selected,guard).front();
        KnownAssertionRepair out;out.index=index;out.identifiable=candidate.signature.detectable;
        out.compatible=candidate.explains_residual;out.amplitude=candidate.amplitude;
        if(out.identifiable && out.compatible) {
            auto corrected=guard.canonical_input(input);corrected.at(index)=guard.sub(corrected.at(index),*out.amplitude);
            out.corrected=analyze(corrected,guard);
            if(!out.corrected->consistent)throw std::logic_error("known assertion repair remains inconsistent");
        }
        return out;
    }
};
} // namespace rfc::learning
