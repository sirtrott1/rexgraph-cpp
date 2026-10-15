#pragma once
#include "rfc/core/batch_attachment_update.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"

namespace rfc::field {
struct BatchHodgeDecomposition {
    numeric::SparseRationalVector gradient,curl,harmonic;
    numeric::SparseRationalVector transferred,transfer_coordinates,reconstructed;
};
namespace batch_hodge_detail {
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using S=numeric::SparseRationalVector;
using E=numeric::SparseRationalEntry;
using Guard=numeric::ExactActionGuard;
using Columns=std::map<std::size_t,std::vector<E>>;
inline std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
inline std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
inline Columns columns(const M& matrix,Guard& guard) {
    guard.matrix(matrix);Columns out;
    for(const auto& [key,q]:matrix.entries()){guard.step();out[key.second].push_back({key.first,q});}
    return out;
}
inline S column(const Columns& columns,std::size_t n,std::size_t j,Guard& guard) {
    guard.step();const auto found=columns.find(j);return found==columns.end()?S(n):S(n,found->second);
}
inline S sum(const S& a,const S& b,int sign,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("batch Hodge sparse carrier mismatch");
    auto values=numeric::checked_sparse_detail::coordinates(a,guard);guard.sparse_vector(b);
    for(const auto& entry:b.entries()) {
        guard.step();numeric::checked_sparse_detail::set(values,entry.index,
            guard.add(numeric::checked_sparse_detail::get(values,entry.index),guard.mul(Q{sign},entry.value)),guard,a.nonzeros()+b.nonzeros());
    }
    return numeric::checked_sparse_detail::vector(a.size(),std::move(values),guard);
}
} // namespace batch_hodge_detail

// Companion Theorem 15.1: with B_k and M_k fixed, a batch Z moves precisely
// im(Pi_h Z) from harmonic to curl. Only the independent harmonic residual
// frame gets a new Gram factor; the original complete Hodge frames are reused.
class PreparedBatchHarmonicAttachment {
    using Guard=numeric::ExactActionGuard;
    using M=numeric::ExactMatrix;
    using S=numeric::SparseRationalVector;
    struct State {
        PreparedHodgeFrames original;
        core::PreparedBatchAttachment attachment;
        M residuals,candidate_dual,upper_cycles;
        std::optional<numeric::PreparedExactFrames> reduction;
        std::optional<hodge_frame_detail::Projection> transfer;
        std::size_t retained{},indices{},steps{};
        State(PreparedHodgeFrames frames,core::PreparedBatchAttachment staged,Guard& guard)
            :original(std::move(frames)),attachment(std::move(staged)) {
            using namespace batch_hodge_detail;const auto start=guard.steps();guard.step();
            if(original.source().get()!=attachment.before().get() || attachment.upper_grade().value!=original.grade().value+1u)
                throw std::invalid_argument("batch Hodge attachment requires its actual original source owner and adjacent grade");
            original.validate(guard);const auto& z=attachment.columns();const auto n=z.rows(),m=z.cols();
            const auto& metric=original.metrics()[original.grade().value];
            if(n!=metric.rows())throw std::invalid_argument("batch candidate carrier differs from the selected Hodge carrier");
            retained=add(original.stored_entries(),attachment.retained_entries());indices=original.stored_index_entries();
            guard.entries(retained);guard.entries(indices);guard.matrix(z);
            if(original.grade().value) {
                const auto lower=hodge_frame_detail::boundary(*original.source(),original.grade(),guard);
                if(numeric::checked_multiply(lower,z,guard).nonzeros())throw std::invalid_argument("batch candidate boundary is not closed");
            }
            const auto declared_columns=columns(z,guard);residuals=M(n,m);
            for(std::size_t j=0;j<m;++j) {
                const auto residual=original.project_sector(2,column(declared_columns,n,j,guard),guard);
                for(const auto& entry:residual.entries()) {
                    guard.step();residuals.set(entry.index,j,entry.value);guard.entries(add(retained,residuals.nonzeros()));
                }
            }
            reduction.emplace(residuals,guard);
            transfer.emplace(reduction->image(),metric,guard);
            candidate_dual=numeric::checked_multiply(z.transpose(),metric,guard);
            // Every residual relation y has Zy in the old curl image. Lift it
            // through the cached old curl Gram solve and actual old pivot
            // cells, producing [-x;y] in ker[C Z]. These m-r witnesses are
            // independent modulo old upper cycles and old upper boundaries.
            const auto& y=reduction->kernel();const auto dependencies=columns(y,guard);
            const auto old_count=original.curl_reduction().original().cols();
            guard.dimension(add(old_count,m));upper_cycles=M(add(old_count,m),y.cols());
            const auto& pivots=original.curl_reduction().pivots();
            for(std::size_t j=0;j<y.cols();++j) {
                const auto relation=column(dependencies,m,j,guard),boundary=numeric::checked_apply(z,relation,guard);
                const auto lifted=original.project_sector_with_coordinates(1,boundary,guard);
                if(lifted.first.entries()!=boundary.entries())throw std::logic_error("residual relation is not an old boundary");
                for(const auto& entry:lifted.second.entries()) {
                    guard.step();upper_cycles.set(pivots.at(entry.index),j,guard.sub(0,entry.value));
                }
                for(const auto& entry:relation.entries()){guard.step();upper_cycles.set(add(old_count,entry.index),j,entry.value);}
            }
            const auto updated=hodge_frame_detail::boundary(*attachment.after(),attachment.upper_grade(),guard);
            if(numeric::checked_multiply(updated,upper_cycles,guard).nonzeros())throw std::logic_error("batch upper-cycle witnesses have a nonzero exact boundary");
            retained=add(retained,add(residuals.nonzeros(),add(candidate_dual.nonzeros(),add(upper_cycles.nonzeros(),
                add(reduction->stored_entries(),transfer->entries())))));
            indices=add(indices,add(reduction->stored_index_entries(),transfer->indices()));
            guard.entries(retained);guard.entries(indices);steps=guard.steps()-start;
        }
    };
    std::shared_ptr<const State> state_;
    static void validate_state(const State& state,Guard& guard) {
        guard.step();guard.entries(state.retained);guard.entries(state.indices);state.original.validate(guard);
        guard.matrix(state.attachment.columns());guard.matrix(state.residuals);guard.matrix(state.candidate_dual);
        guard.matrix(state.upper_cycles);state.reduction->validate(guard);state.transfer->validate(guard);
    }
public:
    PreparedBatchHarmonicAttachment(PreparedHodgeFrames original,core::PreparedBatchAttachment attachment,Guard& guard)
        :state_(std::make_shared<const State>(std::move(original),std::move(attachment),guard)){}
    static PreparedBatchHarmonicAttachment prepare(PreparedHodgeFrames original,std::vector<core::BatchAttachmentCell> cells,Guard& guard) {
        const auto upper=core::Grade{static_cast<std::uint16_t>(original.grade().value+1u)};
        auto attachment=core::PreparedBatchAttachment::prepare(original.source(),upper,std::move(cells),guard);
        return PreparedBatchHarmonicAttachment(std::move(original),std::move(attachment),guard);
    }
    const PreparedHodgeFrames& original_frames() const & {return state_->original;}
    const core::PreparedBatchAttachment& attachment() const & {return state_->attachment;}
    const M& residuals() const & {return state_->residuals;}
    const numeric::PreparedExactFrames& residual_reduction() const & {return *state_->reduction;}
    const M& upper_cycle_witnesses() const & {return state_->upper_cycles;}
    const PreparedHodgeFrames& original_frames() const &&=delete;
    const core::PreparedBatchAttachment& attachment() const &&=delete;
    const M& residuals() const &&=delete;
    const numeric::PreparedExactFrames& residual_reduction() const &&=delete;
    const M& upper_cycle_witnesses() const &&=delete;
    std::size_t harmonic_removed() const noexcept {return state_->reduction->rank();}
    std::size_t upper_homology_added() const noexcept {return state_->attachment.columns().cols()-harmonic_removed();}
    std::size_t harmonic_remaining() const noexcept {return state_->original.harmonic_frame().cols()-harmonic_removed();}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t stored_index_entries() const noexcept {return state_->indices;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}
    void validate(Guard& guard) const {
        const auto pin=state_;validate_state(*pin,guard);
    }
    BatchHodgeDecomposition decompose(S input,Guard& guard) const {
        using namespace batch_hodge_detail;const auto pin=state_;validate_state(*pin,guard);guard.sparse_vector(input);
        if(input.size()!=pin->attachment.columns().rows())throw std::invalid_argument("batch Hodge input requires the retained selected carrier");
        auto gradient=pin->original.project_sector(0,input,guard),curl=pin->original.project_sector(1,input,guard),harmonic=pin->original.project_sector(2,input,guard);
        auto moved=pin->transfer->apply(input,guard);
        curl=sum(curl,moved.first,1,guard);harmonic=sum(harmonic,moved.first,-1,guard);
        if(numeric::checked_apply(pin->candidate_dual,harmonic,guard).nonzeros())throw std::logic_error("updated harmonic field has a nonzero attachment residual");
        auto reconstructed=sum(sum(gradient,curl,1,guard),harmonic,1,guard);
        if(reconstructed.entries()!=input.entries())throw std::logic_error("batch Hodge field reconstruction failed");
        guard.entries(add(pin->retained,add(gradient.nonzeros(),add(curl.nonzeros(),add(harmonic.nonzeros(),add(moved.first.nonzeros(),
            add(moved.second.nonzeros(),reconstructed.nonzeros())))))));
        return {std::move(gradient),std::move(curl),std::move(harmonic),std::move(moved.first),std::move(moved.second),std::move(reconstructed)};
    }
};
} // namespace rfc::field
