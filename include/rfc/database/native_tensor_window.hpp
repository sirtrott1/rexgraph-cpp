#pragma once

#include "rfc/storage/native_accession.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <bit>
#include <span>

namespace rfc::database {
namespace tensor_window_detail {
inline std::size_t bits(const numeric::Rational& q) {
    return std::max(composite::declaration_detail::integer_bits(q.get_num()),composite::declaration_detail::integer_bits(q.get_den()));
}
struct Plan {
    storage::PinnedNativeAccession publication;
    std::shared_ptr<const numeric::SparseRationalTensor> tensor;
    std::vector<std::size_t> lower,upper;
    numeric::IndexDomain domain;
    std::size_t first{},last{},entry_begin{},entry_end{},supported{},nonzeros{},support_work{};
    runtime::ExecutionRequirements requirements;
    Plan(storage::PinnedNativeAccession p,std::shared_ptr<const numeric::SparseRationalTensor> t,
        std::vector<std::size_t> lo,std::vector<std::size_t> hi,numeric::IndexDomain d)
        :publication(std::move(p)),tensor(std::move(t)),lower(std::move(lo)),upper(std::move(hi)),domain(std::move(d)){}
    bool contains(std::size_t index) const {
        if (!domain.logical_size() || index<first || index>=last) return false;
        const auto& source=tensor->domain();
        for (std::size_t axis=0;axis<source.rank();++axis) {
            const auto coordinate=(index/source.strides()[axis])%source.extents()[axis];
            if (coordinate<lower[axis] || coordinate>=upper[axis]) return false;
        }
        return true;
    }
    std::size_t source_index(std::size_t index) const {
        if (index>=domain.logical_size()) throw std::out_of_range("tensor window coordinate");
        std::size_t out=0;
        for (std::size_t axis=0;axis<domain.rank();++axis)
            out+=((index/domain.strides()[axis])%domain.extents()[axis]+lower[axis])*tensor->domain().strides()[axis];
        return out;
    }
    std::optional<std::size_t> window_index(std::size_t index) const {
        if (index>=tensor->domain().logical_size()) throw std::out_of_range("tensor source coordinate");
        if (!contains(index)) return std::nullopt;
        std::size_t out=0;
        for (std::size_t axis=0;axis<domain.rank();++axis)
            out+=((index/tensor->domain().strides()[axis])%tensor->domain().extents()[axis]-lower[axis])*domain.strides()[axis];
        return out;
    }
};
} // namespace tensor_window_detail

// The coordinate pin owns the sparse value backing its pointer. A supported
// coordinate without a nonzero entry is zero; it remains distinct from absence.
class NativeTensorCoordinate {
public:
    std::size_t source_index() const noexcept {return source_index_;}
    bool supported() const noexcept {return supported_;}
    bool stored_nonzero() const noexcept {return nonzero_!=nullptr;}
    const numeric::Rational* nonzero_value() const & noexcept {return nonzero_;}
    const numeric::Rational* nonzero_value() const &&=delete;
    std::optional<numeric::Rational> state() const {
        if (!supported_) return std::nullopt;
        return nonzero_?*nonzero_:numeric::Rational{0};
    }
    std::shared_ptr<const numeric::SparseRationalTensor> tensor_pin() const {return tensor_;}
private:
    NativeTensorCoordinate(std::shared_ptr<const numeric::SparseRationalTensor> t,std::size_t i)
        :tensor_(std::move(t)),source_index_(i),supported_(tensor_->support().contains(i)) {
        const auto& entries=tensor_->values().entries();
        const auto found=std::lower_bound(entries.begin(),entries.end(),i,[](const auto& e,auto n){return e.index<n;});
        if (found!=entries.end() && found->index==i) nonzero_=&found->value;
    }
    std::shared_ptr<const numeric::SparseRationalTensor> tensor_;
    std::size_t source_index_;bool supported_;const numeric::Rational* nonzero_{};
    friend class NativeTensorWindow;
};
class NativeTensorWindow;
class NativeTensorWindowSummary {
public:
    std::size_t supported_count() const {return plan_->supported;}
    std::size_t nonzero_count() const {return plan_->nonzeros;}
    const std::optional<numeric::Rational>& sum() const & {return sum_;}
    const std::optional<numeric::Rational>& sum() const &&=delete;
    std::size_t visited_entries() const {return plan_->entry_end-plan_->entry_begin;}
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const {return plan_->publication.endpoint();}
    runtime::WorkAvailability availability() const {return availability_;}
    const numeric::IndexDomain& domain() const & {return plan_->domain;}
    const numeric::IndexDomain& domain() const &&=delete;
    const std::vector<std::size_t>& lower() const & {return plan_->lower;}
    const std::vector<std::size_t>& lower() const &&=delete;
    const std::vector<std::size_t>& upper() const & {return plan_->upper;}
    const std::vector<std::size_t>& upper() const &&=delete;
    storage::NativeAccessionDescriptor source_descriptor() const {return plan_->publication.descriptor();}
    const runtime::ExecutionHostPolicy& execution_policy() const & {return policy_;}
    const runtime::ExecutionHostPolicy& execution_policy() const &&=delete;
    bool cpu_fallback() const {return policy_.backend!=runtime::ExecutionBackend::ExactCpu;}
private:
    NativeTensorWindowSummary(std::shared_ptr<const tensor_window_detail::Plan> p,
        std::optional<numeric::Rational> sum,runtime::WorkAvailability a,runtime::ExecutionHostPolicy policy)
        :plan_(std::move(p)),sum_(std::move(sum)),availability_(a),policy_(std::move(policy)){}
    std::shared_ptr<const tensor_window_detail::Plan> plan_;
    std::optional<numeric::Rational> sum_;runtime::WorkAvailability availability_;
    runtime::ExecutionHostPolicy policy_;
    friend class NativeTensorWindow;
};

// Half-open boxes preserve axis order and a reversible coordinate realization.
// No dense tensor, remapped carrier or source replacement is constructed.
class NativeTensorWindow {
public:
    static NativeTensorWindow prepare(const storage::PinnedNativeAccession& publication,
        const std::vector<std::size_t>& lower,const std::vector<std::size_t>& upper,
        runtime::WorkAvailability availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(publication.source_namespace(),publication.sequence(),availability);
        const auto tensor=publication.tensor();const auto& source=tensor->domain();
        if (lower.size()!=source.rank() || upper.size()!=source.rank()) throw std::invalid_argument("tensor window rank mismatch");
        runtime::ExecutionRequirements required;
        required.records=machine::checked_size_add(1,machine::checked_size_mul(source.rank(),3));
        required.rational_slots=2;runtime::admit_execution(required,lease.policy());
        std::vector<std::size_t> extents;extents.reserve(source.rank());
        for (std::size_t axis=0;axis<source.rank();++axis) {
            if (lower[axis]>upper[axis] || upper[axis]>source.extents()[axis]) throw std::out_of_range("tensor window axis bounds");
            extents.push_back(upper[axis]-lower[axis]);
        }
        auto p=std::make_shared<tensor_window_detail::Plan>(publication,tensor,lower,upper,numeric::IndexDomain(std::move(extents)));
        if (p->domain.logical_size()) {
            p->last=1;
            for (std::size_t axis=0;axis<source.rank();++axis) {
                p->first+=lower[axis]*source.strides()[axis];p->last+=(upper[axis]-1)*source.strides()[axis];
            }
        }
        const auto& entries=tensor->values().entries();
        const auto by_index=[](const auto& entry,auto index){return entry.index<index;};
        p->entry_begin=static_cast<std::size_t>(std::lower_bound(entries.begin(),entries.end(),p->first,by_index)-entries.begin());
        p->entry_end=static_cast<std::size_t>(std::lower_bound(entries.begin(),entries.end(),p->last,by_index)-entries.begin());
        const auto& support=tensor->support();
        std::size_t support_begin=0,support_end=0;
        if (const auto* indices=support.sparse_indices()) {
            support_begin=static_cast<std::size_t>(std::lower_bound(indices->begin(),indices->end(),p->first)-indices->begin());
            support_end=static_cast<std::size_t>(std::lower_bound(indices->begin(),indices->end(),p->last)-indices->begin());
            p->support_work=support_end-support_begin;
        } else if (support.packed_bits() && p->last>p->first) {
            support_begin=p->first/64;support_end=machine::ceil_div(p->last,64);
            p->support_work=machine::checked_size_mul(support_end-support_begin,64);
        }
        required.records=machine::checked_size_add(required.records,p->entry_end-p->entry_begin);
        required.scalar_work=machine::checked_size_mul(machine::checked_size_add(
            machine::checked_size_mul(p->entry_end-p->entry_begin,2),p->support_work),machine::checked_size_add(source.rank(),1));
        interpretation_bound(required.scalar_work,publication.limits().max_action_work);
        runtime::admit_execution(required,lease.policy());
        if (support.storage()==numeric::StructuralSupport::Storage::Full) p->supported=p->domain.logical_size();
        else if (const auto* indices=support.sparse_indices()) {
            for (auto i=support_begin;i<support_end;++i) {lease.check_cancelled();if (p->contains((*indices)[i])) ++p->supported;}
        } else if (const auto* packed=support.packed_bits()) {
            for (auto word=support_begin;word<support_end;++word) {
                lease.check_cancelled();auto bits=packed->word(word);
                if (word==support_begin) bits&=~std::uint64_t{0}<<(p->first%64);
                if (word+1==support_end && p->last%64) bits&=~std::uint64_t{0}>>(64-p->last%64);
                if (p->last-p->first==p->domain.logical_size()) p->supported+=static_cast<std::size_t>(std::popcount(bits));
                else while (bits) {
                    const auto index=word*64+static_cast<std::size_t>(std::countr_zero(bits));
                    if (p->contains(index)) ++p->supported;
                    bits&=bits-1;
                }
            }
        }
        // Supported zero still returns a Rational with denominator one.
        if (p->supported) required.integer_bits=1;
        for (auto i=p->entry_begin;i<p->entry_end;++i) {
            lease.check_cancelled();if (!p->contains(entries[i].index)) continue;
            ++p->nonzeros;required.integer_bits=std::max(required.integer_bits,tensor_window_detail::bits(entries[i].value));
        }
        if (p->nonzeros) required.integer_bits=machine::checked_size_mul(2,machine::checked_size_add(1,
            machine::checked_size_mul(p->nonzeros,machine::checked_size_add(required.integer_bits,1))));
        p->requirements=required;runtime::admit_execution(required,lease.policy());lease.check_cancelled();
        return NativeTensorWindow(std::move(p));
    }
    const numeric::IndexDomain& domain() const & {return plan().domain;}
    const numeric::IndexDomain& domain() const &&=delete;
    const numeric::IndexDomain& source_domain() const & {return plan().tensor->domain();}
    const numeric::IndexDomain& source_domain() const &&=delete;
    const std::vector<std::size_t>& lower() const & {return plan().lower;}
    const std::vector<std::size_t>& lower() const &&=delete;
    const std::vector<std::size_t>& upper() const & {return plan().upper;}
    const std::vector<std::size_t>& upper() const &&=delete;
    std::size_t source_index(std::size_t index) const {return plan().source_index(index);}
    std::optional<std::size_t> window_index(std::size_t index) const {return plan().window_index(index);}
    NativeTensorCoordinate coordinate(std::size_t index) const {return NativeTensorCoordinate(plan().tensor,source_index(index));}
    std::size_t supported_count() const {return plan().supported;}
    std::size_t nonzero_count() const {return plan().nonzeros;}
    runtime::ExecutionRequirements requirements() const {return plan().requirements;}
    std::shared_ptr<const numeric::SparseRationalTensor> tensor_pin() const {return plan().tensor;}
    storage::NativeAccessionDescriptor source_descriptor() const {return plan().publication.descriptor();}
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const {return plan().publication.endpoint();}
    std::uint64_t source_namespace() const {return plan().publication.source_namespace();}
    NativeTensorWindowSummary summarize(runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto& p=plan();lease.check_cancelled();
        runtime::admit_work_availability(source_namespace(),p.publication.sequence(),availability);
        runtime::admit_execution(p.requirements,lease.policy());
        std::optional<numeric::Rational> sum;if (p.supported) sum=numeric::Rational{0};
        const auto& entries=p.tensor->values().entries();
        for (auto i=p.entry_begin;i<p.entry_end;++i) {
            lease.check_cancelled();if (!p.contains(entries[i].index)) continue;
            *sum+=entries[i].value;
            const auto n=tensor_window_detail::bits(*sum);
            if (n>lease.policy().limits.integer_bits) throw runtime::ExecutionLimitExceeded("tensor sum integer bits",n,lease.policy().limits.integer_bits);
        }
        lease.check_cancelled();return NativeTensorWindowSummary(plan_,std::move(sum),availability,lease.policy());
    }
private:
    static void interpretation_bound(std::size_t work,std::size_t limit) {
        if (work>limit) throw runtime::ExecutionLimitExceeded("native tensor work envelope",work,limit);
    }
    explicit NativeTensorWindow(std::shared_ptr<const tensor_window_detail::Plan> p):plan_(std::move(p)){}
    const tensor_window_detail::Plan& plan() const {if (!plan_) throw std::invalid_argument("moved tensor window");return *plan_;}
    std::shared_ptr<const tensor_window_detail::Plan> plan_;
};
} // namespace rfc::database
