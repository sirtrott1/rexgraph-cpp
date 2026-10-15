#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include <array>

namespace rfc::field {
struct HodgeFrameDecomposition {
    numeric::RationalVector gradient,curl,harmonic;
    numeric::RationalVector gradient_coordinates,curl_coordinates,harmonic_coordinates;
    numeric::RationalVector reconstructed;
};
namespace hodge_frame_detail {
using Matrix=numeric::ExactMatrix;
using Vector=numeric::RationalVector;
using Guard=numeric::ExactActionGuard;
inline std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
inline void source_values(const core::RelationalComplex& source,Guard& guard) {
    for(std::size_t k=0;k<source.grade_count();++k){
        guard.step();const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        guard.dimension(arena.cell_count());
        for(const auto& scale:arena.cell_scales){guard.step();guard.value(scale);}
        for(const auto& coefficient:arena.lattice_coefficient){guard.step();guard.value(numeric::Rational{coefficient});}
    }
}
inline Matrix boundary(const core::RelationalComplex& source,core::Grade grade,Guard& guard) {
    const auto& upper=source.grade(grade);const auto& lower=source.grade(core::Grade{static_cast<std::uint16_t>(grade.value-1u)});
    Matrix result(lower.cell_count(),upper.cell_count());
    const auto exists=source.signature().existence_aspect();
    for(std::size_t cell=0;cell<upper.cell_count();++cell){
        guard.step();const auto [begin,end]=upper.incidence_range(cell);
        for(std::size_t p=begin;p<end;++p){
            guard.step();if(!upper.aspects.get(exists,p))continue;
            const auto row=upper.lower_index[p];
            const auto value=guard.div(guard.mul(lower.cell_scales[row],numeric::Rational{upper.lattice_coefficient[p]}),upper.cell_scales[cell]);
            result.set(row,cell,guard.add(result.get(row,cell),value));guard.entries(result.nonzeros());
        }
    }
    return result;
}
// Guarded exact LDL^T certificate. This certifies a declared rational form;
// it neither normalizes eigenvectors nor infers a metric from source incidence.
inline void positive(const Matrix& form,Guard& guard) {
    const numeric::PreparedPositiveSemidefinite certificate(form,guard);
    if(certificate.rank()!=form.rows())throw std::invalid_argument("frame metric must be positive definite");
}
inline Matrix solve_columns(const numeric::PreparedExactSolve& solve,const Matrix& rhs,Guard& guard) {
    guard.matrix(rhs);Matrix result(rhs.rows(),rhs.cols());
    std::map<std::size_t,std::vector<numeric::SparseRationalEntry>> columns;
    for(const auto& [ij,q]:rhs.entries()){guard.step();columns[ij.second].push_back({ij.first,q});}
    for(auto& [j,entries]:columns){
        const auto value=solve.apply(numeric::SparseRationalVector(rhs.rows(),std::move(entries)),guard);
        for(const auto& entry:value.entries()){guard.step();result.set(entry.index,j,entry.value);guard.entries(result.nonzeros());}
    }
    return result;
}
struct Projection {
    Matrix frame,dual;
    std::optional<numeric::PreparedExactSolve> gram;
    Projection(Matrix f,const Matrix& metric,Guard& guard):frame(std::move(f)) {
        guard.matrix(frame);dual=numeric::checked_multiply(frame.transpose(),metric,guard);
        if(frame.cols())gram.emplace(numeric::checked_multiply(dual,frame,guard),guard);
    }
    std::size_t entries() const {return add(add(frame.nonzeros(),dual.nonzeros()),gram?gram->stored_entries():0);}
    std::size_t indices() const {return gram?gram->stored_index_entries():0;}
    void validate(Guard& guard) const {
        guard.matrix(frame);guard.matrix(dual);if(gram)(void)gram->factor_receipt(guard);
    }
    std::pair<Vector,Vector> apply(const Vector& input,Guard& guard) const {
        validate(guard);const auto rhs=numeric::checked_apply(dual,input,guard);
        auto coordinates=gram?gram->apply(rhs,guard):Vector{};
        auto value=numeric::checked_apply(frame,coordinates,guard);
        return {std::move(value),std::move(coordinates)};
    }
    std::pair<numeric::SparseRationalVector,numeric::SparseRationalVector> apply(const numeric::SparseRationalVector& input,Guard& guard) const {
        validate(guard);const auto rhs=numeric::checked_apply(dual,input,guard);
        auto coordinates=gram?gram->apply(rhs,guard):numeric::SparseRationalVector(0);
        auto value=numeric::checked_apply(frame,coordinates,guard);return {std::move(value),std::move(coordinates)};
    }
};
} // namespace hodge_frame_detail

// Complete strict-chain sectors at one selected grade. The complete source
// and all declared metrics are owned; no full Hodge or projector is assembled.
class PreparedHodgeFrames {
    using Guard=numeric::ExactActionGuard;
    using Matrix=numeric::ExactMatrix;
    struct State {
        std::shared_ptr<const core::RelationalComplex> source;
        std::vector<Matrix> metrics,boundaries;
        core::Grade grade;
        std::optional<numeric::PreparedExactSolve> metric_solve;
        std::optional<numeric::PreparedExactFrames> gradient_reduction,curl_reduction,harmonic_reduction;
        std::array<std::optional<hodge_frame_detail::Projection>,3> sectors;
        std::size_t retained{},indices{},preparation{},interfaces{};
        State(std::shared_ptr<const core::RelationalComplex> s,std::vector<Matrix> m,core::Grade k,Guard& guard)
            :source(std::move(s)),metrics(std::move(m)),grade(k) {
            using namespace hodge_frame_detail;
            const auto start=guard.steps();guard.step();
            if(!source || grade.value>=source->grade_count() || metrics.size()!=source->grade_count())
                throw std::invalid_argument("complete frame source, grade or metric count");
            source_values(*source,guard);
            for(std::size_t i=0;i<metrics.size();++i){
                const auto n=source->grade(core::Grade{static_cast<std::uint16_t>(i)}).cell_count();
                if(metrics[i].rows()!=n || metrics[i].cols()!=n)throw std::invalid_argument("complete frame metric carrier dimensions");
                positive(metrics[i],guard);retained=add(retained,metrics[i].nonzeros());guard.entries(retained);
            }
            boundaries.emplace_back(0,source->grade(core::Grade{0}).cell_count());
            for(std::size_t i=1;i<source->grade_count();++i){
                boundaries.push_back(boundary(*source,core::Grade{static_cast<std::uint16_t>(i)},guard));
                retained=add(retained,boundaries.back().nonzeros());guard.entries(retained);
                if(i>1){++interfaces;
                    if(numeric::checked_multiply(boundaries[i-1],boundaries[i],guard).nonzeros())
                        throw std::invalid_argument("complete Hodge frames require the full strict chain family");
                }
            }
            const auto n=source->grade(grade).cell_count();const auto& B=boundaries[grade.value];
            const auto C=grade.value<source->max_grade().value?boundaries[grade.value+1u]:Matrix(n,0);
            metric_solve.emplace(metrics[grade.value],guard);
            // M_below is invertible, so im(B^dagger)=M_here^-1 im(B^T).
            // Solve only the independent image directions, not every lower axis.
            gradient_reduction.emplace(B.transpose(),guard);
            auto gradient=solve_columns(*metric_solve,gradient_reduction->image(),guard);
            curl_reduction.emplace(C,guard);
            const auto upper=numeric::checked_multiply(C.transpose(),metrics[grade.value],guard);
            guard.dimension(add(B.rows(),upper.rows()));
            auto constraints=Matrix::vertical_concat(B,upper);guard.matrix(constraints);
            harmonic_reduction.emplace(std::move(constraints),guard);
            const auto total=add(add(gradient.cols(),curl_reduction->rank()),harmonic_reduction->kernel().cols());
            if(total!=n)throw std::logic_error("complete Hodge frame dimensions do not span the carrier");
            sectors[0].emplace(std::move(gradient),metrics[grade.value],guard);
            sectors[1].emplace(curl_reduction->image(),metrics[grade.value],guard);
            sectors[2].emplace(harmonic_reduction->kernel(),metrics[grade.value],guard);
            for(std::size_t i=0;i<3;++i)for(std::size_t j=i+1;j<3;++j)
                if(numeric::checked_multiply(sectors[i]->dual,sectors[j]->frame,guard).nonzeros())
                    throw std::logic_error("complete Hodge frames are not metric orthogonal");
            retained=add(retained,metric_solve->stored_entries());indices=metric_solve->stored_index_entries();
            for(const auto* reduction:{&*gradient_reduction,&*curl_reduction,&*harmonic_reduction}){
                retained=add(retained,reduction->stored_entries());indices=add(indices,reduction->stored_index_entries());
            }
            for(const auto& sector:sectors){retained=add(retained,sector->entries());indices=add(indices,sector->indices());}
            guard.entries(retained);guard.entries(indices);preparation=guard.steps()-start;
        }
    };
public:
    PreparedHodgeFrames(const PreparedHodgeFrames&)=default;
    PreparedHodgeFrames& operator=(const PreparedHodgeFrames&)=default;
    PreparedHodgeFrames(std::shared_ptr<const core::RelationalComplex> source,std::vector<Matrix> metrics,
                        core::Grade grade,Guard& guard)
        :state_(std::make_shared<const State>(std::move(source),std::move(metrics),grade,guard)){}
    PreparedHodgeFrames(std::shared_ptr<const core::RelationalComplex> source,std::vector<Matrix> metrics,
                        core::Grade grade,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(source),std::move(metrics),grade,guard);
    }
    const Matrix& gradient_frame() const & {return state_->sectors[0]->frame;}
    const Matrix& curl_frame() const & {return state_->sectors[1]->frame;}
    const Matrix& harmonic_frame() const & {return state_->sectors[2]->frame;}
    const Matrix& gradient_frame() const &&=delete;
    const Matrix& curl_frame() const &&=delete;
    const Matrix& harmonic_frame() const &&=delete;
    const numeric::PreparedExactFrames& gradient_reduction() const & {return *state_->gradient_reduction;}
    const numeric::PreparedExactFrames& curl_reduction() const & {return *state_->curl_reduction;}
    const numeric::PreparedExactFrames& harmonic_reduction() const & {return *state_->harmonic_reduction;}
    const numeric::PreparedExactFrames& gradient_reduction() const &&=delete;
    const numeric::PreparedExactFrames& curl_reduction() const &&=delete;
    const numeric::PreparedExactFrames& harmonic_reduction() const &&=delete;
    std::shared_ptr<const core::RelationalComplex> source() const {return state_->source;}
    const std::vector<Matrix>& metrics() const & {return state_->metrics;}
    const std::vector<Matrix>& metrics() const &&=delete;
    core::Grade grade() const noexcept {return state_->grade;}
    std::size_t chain_interfaces_checked() const noexcept {return state_->interfaces;}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t stored_index_entries() const noexcept {return state_->indices;}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    void validate(Guard& guard) const {
        using namespace hodge_frame_detail;const auto pin=state_;
        guard.step();source_values(*pin->source,guard);guard.entries(pin->retained);guard.entries(pin->indices);
        for(const auto& metric:pin->metrics)guard.matrix(metric);
        for(const auto& map:pin->boundaries)guard.matrix(map);
        for(const auto* reduction:{&*pin->gradient_reduction,&*pin->curl_reduction,&*pin->harmonic_reduction})reduction->validate(guard);
        (void)pin->metric_solve->factor_receipt(guard);
        for(const auto& sector:pin->sectors)sector->validate(guard);
    }
    numeric::Rational sector_volume(std::size_t sector,Guard& guard) const {
        const auto pin=state_;if(sector>=3)throw std::out_of_range("Hodge sector");
        validate(guard);const auto& projection=*pin->sectors[sector];
        return projection.gram?projection.gram->factor_receipt(guard).determinant(guard):numeric::Rational{1};
    }
    numeric::RationalVector project_sector(std::size_t sector,numeric::RationalVector input,Guard& guard) const {
        const auto pin=state_;if(sector>=3)throw std::out_of_range("Hodge sector");validate(guard);
        return pin->sectors[sector]->apply(input,guard).first;
    }
    numeric::SparseRationalVector project_sector(std::size_t sector,const numeric::SparseRationalVector& input,Guard& guard) const {
        const auto pin=state_;if(sector>=3)throw std::out_of_range("Hodge sector");validate(guard);
        return pin->sectors[sector]->apply(input,guard).first;
    }
    std::pair<numeric::SparseRationalVector,numeric::SparseRationalVector> project_sector_with_coordinates(
        std::size_t sector,const numeric::SparseRationalVector& input,Guard& guard) const {
        const auto pin=state_;if(sector>=3)throw std::out_of_range("Hodge sector");validate(guard);
        return pin->sectors[sector]->apply(input,guard);
    }
    HodgeFrameDecomposition decompose(numeric::RationalVector input,Guard& guard) const {
        using namespace hodge_frame_detail;
        const auto pin=state_;guard.step();source_values(*pin->source,guard);
        guard.entries(pin->retained);guard.entries(pin->indices);
        for(const auto& metric:pin->metrics)guard.matrix(metric);
        for(const auto& map:pin->boundaries)guard.matrix(map);
        for(const auto* reduction:{&*pin->gradient_reduction,&*pin->curl_reduction,&*pin->harmonic_reduction})reduction->validate(guard);
        (void)pin->metric_solve->factor_receipt(guard);
        if(input.size()!=pin->metrics[pin->grade.value].rows())throw std::invalid_argument("complete frame field dimension");
        const auto x=guard.canonical_input(input);
        auto g=pin->sectors[0]->apply(x,guard),c=pin->sectors[1]->apply(x,guard),h=pin->sectors[2]->apply(x,guard);
        Vector reconstructed(x.size());
        for(std::size_t i=0;i<x.size();++i)reconstructed[i]=guard.add(guard.add(g.first[i],c.first[i]),h.first[i]);
        if(reconstructed!=x)throw std::logic_error("complete Hodge field reconstruction failed");
        return {std::move(g.first),std::move(c.first),std::move(h.first),std::move(g.second),std::move(c.second),std::move(h.second),std::move(reconstructed)};
    }
    HodgeFrameDecomposition decompose(numeric::RationalVector input,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return decompose(std::move(input),guard);
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
