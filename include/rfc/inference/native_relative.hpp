#pragma once
#include "rfc/algebra/owned_relative.hpp"
#include "rfc/inference/native_gluing.hpp"

namespace rfc::inference {
struct NativeRelativePairRequest {
    storage::PinnedNativeAccession source;
    std::uint64_t key{},revision{};
    std::vector<storage::StableCellKey> whole,sub;
    std::size_t dimension_limit{64};
};
struct NativeRelativeMapAvailability { runtime::WorkAvailability source,target; };
struct NativeRelativeMapRequest { NativeGluingSourceMap map; };
enum class NativeRelativeAction { DeclaredChain, NativeProjection, Projection, Injection, Representative, Boundary, ConnectingWitness, PairMap };
struct NativeRelativeProvenance {
    NativeRelativeAction action;
    std::uint64_t key{},revision{},source_namespace{},sequence{};
    runtime::WorkAvailability preparation_availability,execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string preparation_policy,execution_policy;
    bool fallback{};
};
namespace native_relative_detail {
using native_gluing_detail::Pin;
using native_gluing_detail::Guard;
using native_gluing_detail::requirements;
using algebra::owned_relative_detail::require;
using algebra::owned_relative_detail::add;
using algebra::owned_relative_detail::mul;
using Values=algebra::OwnedRelativePair::Values;
struct State {
    NativeRelativePairRequest request;
    algebra::OwnedRelativePair pair;
    runtime::WorkAvailability preparation;
    std::size_t steps;
    std::string policy;
    bool fallback;
};
inline void available(const State& s,runtime::WorkAvailability a) {
    for(const auto w:{s.preparation,a}) runtime::admit_work_availability(s.request.source.source_namespace(),s.request.source.sequence(),w);
}
inline numeric::ExactActionLimits limits(const State& s,const runtime::ExecutionLease& lease) {
    const std::array<Pin,1> pins{s.request.source}; return native_gluing_detail::limits(lease,pins,s.request.dimension_limit);
}
inline NativeRelativeProvenance report(const State& s,NativeRelativeAction action,runtime::WorkAvailability a,
    const runtime::ExecutionLease& lease,const Guard& guard,runtime::ExecutionRequirements required) {
    return {action,s.request.key,s.request.revision,s.request.source.source_namespace(),s.request.source.sequence(),s.preparation,a,
        required,guard.limits(),s.steps,guard.steps(),s.policy,lease.policy().name,s.fallback||lease.policy().backend!=runtime::ExecutionBackend::ExactCpu};
}
inline std::size_t chain_envelope(const State& s,std::size_t input) { return add(s.pair.entry_envelope(),mul(input,4)); }
}
class DerivedNativeRelativeChain;
class NativeRelativeConnectingWitness;
class PreparedNativeRelativePairMap;
class PreparedNativeRelativePair {
public:
    using Values=algebra::OwnedRelativePair::Values;
    static PreparedNativeRelativePair prepare(NativeRelativePairRequest input,runtime::WorkAvailability a,
        runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_relative_detail; auto request=input; lease.check_cancelled();
        require(request.key && request.revision && request.dimension_limit,"native relative pair needs a declaration and positive dimension limit");
        require(request.source.kind()==storage::NativeAccessionKind::Chain && request.source.support_count()==0,"native relative source must be an empty owning Chain anchor");
        runtime::admit_work_availability(request.source.source_namespace(),request.source.sequence(),a);
        const auto& source=request.source.endpoint()->source;
        const auto words=add(algebra::owned_gluing_detail::words(source),add(request.whole.size(),request.sub.size()));
        const auto envelope=add(1,mul(words,32)); runtime::admit_execution(requirements(words,envelope),lease.policy());
        const std::array<Pin,1> pins{request.source}; Guard guard(native_gluing_detail::limits(lease,pins,request.dimension_limit),[&lease]{lease.check_cancelled();}); guard.entries(envelope);
        algebra::owned_gluing_detail::source(source,guard); storage::StableSourceIndex index(source);
        const auto select=[&](const std::vector<storage::StableCellKey>& cells) {
            algebra::SubcomplexMask mask(source);
            for(const auto key:cells) {
                guard.step(); const auto cell=index.cell(key); require(cell.has_value(),"relative selection names an absent ambient identity");
                require(!mask.contains(*cell),"relative selection repeats an identity"); mask.select(*cell);
            }
            return mask;
        };
        auto whole=select(request.whole),sub=select(request.sub);
        auto pair=algebra::OwnedRelativePair::prepare(source,std::move(whole),std::move(sub),guard);
        runtime::admit_execution(requirements(words,add(envelope,pair.entry_envelope())),lease.policy()); lease.check_cancelled();
        return PreparedNativeRelativePair(std::make_shared<const native_relative_detail::State>(native_relative_detail::State{
            std::move(request),std::move(pair),a,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedRelativePair numeric_plan() const { return state_->pair; }
    storage::PinnedNativeAccession source() const { return state_->request.source; }
    const NativeRelativePairRequest& declaration() const & { return state_->request; }
    const NativeRelativePairRequest& declaration() const &&=delete;
    runtime::WorkAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeRelativePair& other) const { return state_==other.state_; }
    DerivedNativeRelativeChain chain(algebra::RelativeSpace,core::Grade,Values,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeRelativeChain project(storage::PinnedNativeAccession,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeRelativeChain project(DerivedNativeRelativeChain,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeRelativeChain inject(DerivedNativeRelativeChain,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeRelativeChain representative(DerivedNativeRelativeChain,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeRelativeChain boundary(DerivedNativeRelativeChain,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeRelativeConnectingWitness connecting_witness(DerivedNativeRelativeChain,runtime::WorkAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeRelativeChain; friend class PreparedNativeRelativePairMap;
    explicit PreparedNativeRelativePair(std::shared_ptr<const native_relative_detail::State> s):state_(std::move(s)){}
    DerivedNativeRelativeChain transform(DerivedNativeRelativeChain,NativeRelativeAction,runtime::WorkAvailability,runtime::ExecutionLease) const;
    std::shared_ptr<const native_relative_detail::State> state_;
};
class DerivedNativeRelativeChain {
public:
    PreparedNativeRelativePair plan() const { return owner_; }
    const algebra::OwnedRelativeChain& chain() const & { return chain_; }
    const algebra::OwnedRelativeChain& chain() const &&=delete;
    const NativeRelativeProvenance& provenance() const & { return report_; }
    const NativeRelativeProvenance& provenance() const &&=delete;
    std::shared_ptr<const DerivedNativeRelativeChain> input() const { return input_; }
    std::optional<storage::PinnedNativeAccession> native_input() const { return native_input_; }
private:
    friend class PreparedNativeRelativePair; friend class PreparedNativeRelativePairMap;
    DerivedNativeRelativeChain(PreparedNativeRelativePair owner,algebra::OwnedRelativeChain chain,NativeRelativeProvenance report,
        std::shared_ptr<const DerivedNativeRelativeChain> input={},std::optional<storage::PinnedNativeAccession> native={})
        :owner_(std::move(owner)),chain_(std::move(chain)),report_(std::move(report)),input_(std::move(input)),native_input_(std::move(native)){}
    PreparedNativeRelativePair owner_; algebra::OwnedRelativeChain chain_; NativeRelativeProvenance report_;
    std::shared_ptr<const DerivedNativeRelativeChain> input_; std::optional<storage::PinnedNativeAccession> native_input_;
};
inline DerivedNativeRelativeChain PreparedNativeRelativePair::chain(algebra::RelativeSpace which,core::Grade g,Values values,runtime::WorkAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_relative_detail; const auto pin=state_; auto captured=values; lease.check_cancelled(); available(*pin,a);
    const auto required=requirements(pin->pair.retained_words(),chain_envelope(*pin,values.size())); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();}); auto output=pin->pair.chain(which,g,std::move(captured),guard); lease.check_cancelled();
    return {PreparedNativeRelativePair(pin),std::move(output),report(*pin,NativeRelativeAction::DeclaredChain,a,lease,guard,required)};
}
inline DerivedNativeRelativeChain PreparedNativeRelativePair::project(storage::PinnedNativeAccession input,runtime::WorkAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_relative_detail; const auto pin=state_; lease.check_cancelled(); available(*pin,a);
    require(input.kind()==storage::NativeAccessionKind::Chain && input.endpoint()==pin->request.source.endpoint() && input.same_execution_authority(pin->request.source),
            "relative projection needs a Chain from its actual ambient publication and authority");
    const auto required=requirements(pin->pair.retained_words(),chain_envelope(*pin,input.support_count())); runtime::admit_execution(required,lease.policy());
    const std::array<Pin,2> pins{pin->request.source,input}; Guard guard(native_gluing_detail::limits(lease,pins,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});
    pin->pair.validate(guard);
    const auto descriptor=input.descriptor(); const auto g=descriptor.request().grade; Values values; storage::StableSourceIndex index(pin->pair.ambient());
    input.for_each_cell([&](auto key){
        guard.step(); const auto cell=index.cell({g,key}); const auto q=input.state(key); require(cell && q,"relative native Chain support changed");
        const auto local=pin->pair.local_index(algebra::RelativeSpace::Whole,*cell); require(local.has_value(),"relative native Chain support is outside the whole subcomplex");
        guard.value(*q); values.emplace(*local,*q);
    });
    auto whole=pin->pair.chain(algebra::RelativeSpace::Whole,g,std::move(values),guard); auto output=pin->pair.project(whole,guard); lease.check_cancelled();
    auto parent=DerivedNativeRelativeChain(PreparedNativeRelativePair(pin),std::move(whole),report(*pin,NativeRelativeAction::NativeProjection,a,lease,guard,required),{},input);
    return {PreparedNativeRelativePair(pin),std::move(output),report(*pin,NativeRelativeAction::NativeProjection,a,lease,guard,required),std::make_shared<const DerivedNativeRelativeChain>(std::move(parent)),std::move(input)};
}
inline DerivedNativeRelativeChain PreparedNativeRelativePair::transform(DerivedNativeRelativeChain input,NativeRelativeAction action,runtime::WorkAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_relative_detail; const auto pin=state_; lease.check_cancelled(); available(*pin,a); require(input.owner_.state_==pin,"relative action needs its actual pair owner");
    const auto required=requirements(pin->pair.retained_words(),chain_envelope(*pin,input.chain_.values().size())); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();});
    auto output=[&] {
        if(action==NativeRelativeAction::Projection) return pin->pair.project(input.chain_,guard);
        if(action==NativeRelativeAction::Injection) return pin->pair.inject(input.chain_,guard);
        if(action==NativeRelativeAction::Representative) return pin->pair.representative(input.chain_,guard);
        require(action==NativeRelativeAction::Boundary,"unknown native relative action"); return pin->pair.boundary(input.chain_,guard);
    }(); lease.check_cancelled();
    return {PreparedNativeRelativePair(pin),std::move(output),report(*pin,action,a,lease,guard,required),std::make_shared<const DerivedNativeRelativeChain>(std::move(input))};
}
inline DerivedNativeRelativeChain PreparedNativeRelativePair::project(DerivedNativeRelativeChain input,runtime::WorkAvailability a,runtime::ExecutionLease l) const { return transform(std::move(input),NativeRelativeAction::Projection,a,std::move(l)); }
inline DerivedNativeRelativeChain PreparedNativeRelativePair::inject(DerivedNativeRelativeChain input,runtime::WorkAvailability a,runtime::ExecutionLease l) const { return transform(std::move(input),NativeRelativeAction::Injection,a,std::move(l)); }
inline DerivedNativeRelativeChain PreparedNativeRelativePair::representative(DerivedNativeRelativeChain input,runtime::WorkAvailability a,runtime::ExecutionLease l) const { return transform(std::move(input),NativeRelativeAction::Representative,a,std::move(l)); }
inline DerivedNativeRelativeChain PreparedNativeRelativePair::boundary(DerivedNativeRelativeChain input,runtime::WorkAvailability a,runtime::ExecutionLease l) const { return transform(std::move(input),NativeRelativeAction::Boundary,a,std::move(l)); }
class NativeRelativeConnectingWitness {
public:
    std::shared_ptr<const DerivedNativeRelativeChain> input() const { return input_; }
    const algebra::RelativeConnectingWitness& witness() const & { return witness_; }
    const algebra::RelativeConnectingWitness& witness() const &&=delete;
    const NativeRelativeProvenance& provenance() const & { return report_; }
    const NativeRelativeProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeRelativePair;
    NativeRelativeConnectingWitness(std::shared_ptr<const DerivedNativeRelativeChain> input,algebra::RelativeConnectingWitness witness,NativeRelativeProvenance report)
        :input_(std::move(input)),witness_(std::move(witness)),report_(std::move(report)){}
    std::shared_ptr<const DerivedNativeRelativeChain> input_; algebra::RelativeConnectingWitness witness_; NativeRelativeProvenance report_;
};
inline NativeRelativeConnectingWitness PreparedNativeRelativePair::connecting_witness(DerivedNativeRelativeChain input,runtime::WorkAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_relative_detail; const auto pin=state_; lease.check_cancelled(); available(*pin,a); require(input.owner_.state_==pin,"connecting witness needs its actual native pair");
    std::size_t count{}; const auto numeric=pin->pair;
    for(std::size_t k=0;k<numeric.ambient().grade_count();++k) count=add(count,numeric.coordinates(algebra::RelativeSpace::Whole,core::Grade{static_cast<std::uint16_t>(k)}).size());
    const auto required=requirements(numeric.retained_words(),add(numeric.entry_envelope(),mul(count,12))); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(*pin,lease),[&lease]{lease.check_cancelled();}); auto witness=numeric.connecting_witness(input.chain_,guard); lease.check_cancelled();
    return {std::make_shared<const DerivedNativeRelativeChain>(std::move(input)),std::move(witness),report(*pin,NativeRelativeAction::ConnectingWitness,a,lease,guard,required)};
}
namespace native_relative_detail {
struct MapState {
    PreparedNativeRelativePair source,target;
    NativeRelativeMapRequest request;
    algebra::OwnedRelativePairMap map;
    NativeRelativeMapAvailability preparation;
    std::size_t steps;
    std::string policy;
    bool fallback;
};
}
class MappedNativeRelativeChain {
public:
    PreparedNativeRelativePairMap plan() const;
    const algebra::OwnedRelativeChain& chain() const & { return chain_; }
    const algebra::OwnedRelativeChain& chain() const &&=delete;
    std::shared_ptr<const DerivedNativeRelativeChain> input() const { return input_; }
    NativeRelativeMapAvailability execution_availability() const { return availability_; }
    const NativeRelativeProvenance& provenance() const & { return report_; }
    const NativeRelativeProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeRelativePairMap;
    MappedNativeRelativeChain(std::shared_ptr<const native_relative_detail::MapState> owner,algebra::OwnedRelativeChain chain,
        std::shared_ptr<const DerivedNativeRelativeChain> input,NativeRelativeMapAvailability a,NativeRelativeProvenance report)
        :owner_(std::move(owner)),chain_(std::move(chain)),input_(std::move(input)),availability_(a),report_(std::move(report)){}
    std::shared_ptr<const native_relative_detail::MapState> owner_; algebra::OwnedRelativeChain chain_;
    std::shared_ptr<const DerivedNativeRelativeChain> input_; NativeRelativeMapAvailability availability_; NativeRelativeProvenance report_;
};
class PreparedNativeRelativePairMap {
public:
    static PreparedNativeRelativePairMap prepare(PreparedNativeRelativePair source,PreparedNativeRelativePair target,
        NativeRelativeMapRequest input,NativeRelativeMapAvailability a,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_relative_detail; auto request=input; lease.check_cancelled(); available(*source.state_,a.source); available(*target.state_,a.target);
        require(request.map.key && request.map.from==source.declaration().key && request.map.to==target.declaration().key,"relative map needs explicit pair declaration identities");
        const std::array<Pin,2> pins{source.source(),target.source()};
        const auto words=add(source.numeric_plan().retained_words(),add(target.numeric_plan().retained_words(),add(request.map.cells.size(),request.map.occurrences.size())));
        const auto envelope=add(1,mul(words,32)); runtime::admit_execution(requirements(words,envelope),lease.policy());
        Guard guard(native_gluing_detail::limits(lease,pins,std::min(source.state_->request.dimension_limit,target.state_->request.dimension_limit)),[&lease]{lease.check_cancelled();}); guard.entries(envelope);
        source.numeric_plan().validate(guard); target.numeric_plan().validate(guard);
        auto map=native_gluing_detail::indexed(source.source().endpoint()->source,target.source().endpoint()->source,request.map.cells,request.map.occurrences,guard);
        auto prepared=algebra::OwnedRelativePairMap::prepare(source.numeric_plan(),target.numeric_plan(),std::move(map),guard);
        runtime::admit_execution(requirements(words,add(envelope,prepared.entry_envelope())),lease.policy()); lease.check_cancelled();
        return PreparedNativeRelativePairMap(std::make_shared<const native_relative_detail::MapState>(native_relative_detail::MapState{
            std::move(source),std::move(target),std::move(request),std::move(prepared),a,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeRelativePair source() const { return state_->source; }
    PreparedNativeRelativePair target() const { return state_->target; }
    const NativeRelativeMapRequest& declaration() const & { return state_->request; }
    const NativeRelativeMapRequest& declaration() const &&=delete;
    algebra::OwnedRelativePairMap numeric_plan() const { return state_->map; }
    NativeRelativeMapAvailability preparation_availability() const { return state_->preparation; }
    MappedNativeRelativeChain execute(DerivedNativeRelativeChain input,NativeRelativeMapAvailability a,runtime::ExecutionLease lease=runtime::ExecutionLease{}) const {
        using namespace native_relative_detail; const auto pin=state_; lease.check_cancelled(); require(pin->source.same_owner(input.plan()),"native pair map needs its actual input owner");
        available(*pin->source.state_,pin->preparation.source); available(*pin->target.state_,pin->preparation.target);
        available(*pin->source.state_,a.source); available(*pin->target.state_,a.target);
        const auto required=requirements(pin->map.retained_words(),add(pin->map.entry_envelope(),mul(input.chain_.values().size(),4))); runtime::admit_execution(required,lease.policy());
        const std::array<Pin,2> pins{pin->source.source(),pin->target.source()};
        Guard guard(native_gluing_detail::limits(lease,pins,std::min(pin->source.state_->request.dimension_limit,pin->target.state_->request.dimension_limit)),[&lease]{lease.check_cancelled();});
        auto output=pin->map.apply(input.chain_,guard); lease.check_cancelled();
        auto receipt=report(*pin->source.state_,NativeRelativeAction::PairMap,a.source,lease,guard,required);
        receipt.preparation_availability=pin->preparation.source; receipt.preparation_steps=pin->steps; receipt.preparation_policy=pin->policy; receipt.fallback=receipt.fallback||pin->fallback;
        return {pin,std::move(output),std::make_shared<const DerivedNativeRelativeChain>(std::move(input)),a,std::move(receipt)};
    }
private:
    friend class MappedNativeRelativeChain;
    explicit PreparedNativeRelativePairMap(std::shared_ptr<const native_relative_detail::MapState> s):state_(std::move(s)){}
    std::shared_ptr<const native_relative_detail::MapState> state_;
};
inline PreparedNativeRelativePairMap MappedNativeRelativeChain::plan() const { return PreparedNativeRelativePairMap(owner_); }
} // namespace rfc::inference
