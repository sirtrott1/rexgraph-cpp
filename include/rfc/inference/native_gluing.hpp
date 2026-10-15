#pragma once
#include "rfc/algebra/owned_gluing.hpp"
#include "rfc/storage/native_atlas.hpp"
#include "rfc/runtime/work_graph.hpp"

namespace rfc::inference {
struct NativeGluingAvailability { runtime::WorkAvailability common,left,right; };
struct NativeGluingMapAvailability { NativeGluingAvailability source; runtime::WorkAvailability target; };
struct NativeGluingRequest { storage::NativeAtlas atlas; std::uint64_t pushout{}; std::size_t dimension_limit{64}; };
// A strict source map may identify whole primary cells. Unlike an embedding,
// injectivity is not required, but every complete occurrence fiber is retained.
struct NativeGluingSourceMap {
    std::uint64_t key{},from{},to{};
    std::vector<storage::NativeCellImage> cells;
    std::vector<storage::NativeOccurrenceImage> occurrences;
};
struct NativeGluingMorphismRequest {
    storage::PinnedNativeAccession target;
    std::uint64_t target_source{};
    NativeGluingSourceMap left,right;
};
enum class NativeGluingAction { DeclaredChain, Inclusion, SameFieldSections, Boundary, Mediator };
struct NativeGluingProvenance {
    NativeGluingAction action;
    std::uint64_t declaration{},revision{},pushout{},field_identity{};
    std::array<std::uint64_t,3> sources,namespaces,sequences;
    NativeGluingAvailability preparation_availability,execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits limits;
    std::size_t preparation_steps{},execution_steps{};
    std::string preparation_policy,execution_policy;
    bool fallback{};
};
namespace native_gluing_detail {
using Pin=storage::PinnedNativeAccession;
using Guard=numeric::ExactActionGuard;
using Q=numeric::Rational;
using algebra::owned_gluing_detail::require;
using algebra::owned_gluing_detail::add;
using algebra::owned_gluing_detail::mul;
using algebra::owned_gluing_detail::grade;
using Values=algebra::OwnedRelationalGluing::Values;
inline numeric::ExactActionLimits limits(const runtime::ExecutionLease& lease,std::span<const Pin> pins,std::size_t dimension) {
    const auto& cap=lease.policy().limits; auto work=cap.scalar_work,bits=cap.integer_bits;
    for (const auto& p:pins) { work=std::min(work,p.limits().max_action_work); bits=std::min(bits,mul(p.limits().max_integer_bytes,8)); }
    return {dimension,std::min(cap.rational_slots,cap.sparse_entries),work,bits};
}
inline runtime::ExecutionRequirements requirements(std::size_t words,std::size_t entries) { return {words,entries,entries,0,0}; }
inline void available(const std::array<Pin,3>& pins,NativeGluingAvailability a) {
    const std::array<runtime::WorkAvailability,3> witnesses{a.common,a.left,a.right};
    for (std::size_t i=0;i<3;++i) runtime::admit_work_availability(pins[i].source_namespace(),pins[i].sequence(),witnesses[i]);
}
inline algebra::RetainedSourceMap indexed(const core::RelationalComplex& a,const core::RelationalComplex& b,
    const std::vector<storage::NativeCellImage>& cells,const std::vector<storage::NativeOccurrenceImage>& occurrences,Guard& guard) {
    storage::StableSourceIndex ia(a),ib(b);
    algebra::RetainedSourceMap out;
    const auto absent=std::numeric_limits<std::uint32_t>::max(); const auto no_occ=std::numeric_limits<std::size_t>::max();
    for (std::size_t k=0;k<a.grade_count();++k) {
        out.cells.emplace_back(a.grade(grade(k)).cell_count(),absent);
        out.occurrences.emplace_back(a.grade(grade(k)).incidence_count(),no_occ);
    }
    for (const auto& p:cells) {
        guard.step(); const auto from=ia.cell(p.from),to=ib.cell(p.to);
        require(from && to && p.from.grade==p.to.grade,"gluing cell map names absent cells or changes grade");
        auto& value=out.cells.at(from->grade.value).at(from->index); require(value==absent,"duplicate gluing cell map source"); value=to->index;
    }
    for (const auto& p:occurrences) {
        guard.step(); const auto from=ia.occurrence(p.from),to=ib.occurrence(p.to);
        require(from && to && p.from.upper_grade==p.to.upper_grade,"gluing occurrence map names absent occurrences or changes grade");
        auto& value=out.occurrences.at(p.from.upper_grade.value).at(*from); require(value==no_occ,"duplicate gluing occurrence map source"); value=*to;
    }
    for (const auto& row:out.cells) for (const auto x:row) { guard.step(); require(x!=absent,"gluing source map omits a cell"); }
    for (const auto& row:out.occurrences) for (const auto x:row) { guard.step(); require(x!=no_occ,"gluing source map omits an occurrence"); }
    return out;
}
struct State {
    NativeGluingRequest request;
    std::array<std::uint64_t,3> keys;
    std::array<Pin,3> pins;
    storage::NativeGluedSource constructed;
    algebra::OwnedRelationalGluing gluing;
    NativeGluingAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
inline void revalidate(const State& s,NativeGluingAvailability current) { available(s.pins,s.preparation); available(s.pins,current); }
inline Values chain_input(const State& s,bool right,const Pin& input,core::Grade g,Guard& guard) {
    const auto& anchor=s.pins[right?2:1];
    require(input.kind()==storage::NativeAccessionKind::Chain && input.endpoint()==anchor.endpoint() && input.same_execution_authority(anchor),
            "gluing needs a Chain from the actual participant publication and execution authority");
    const auto descriptor=input.descriptor();
    require(descriptor.request().grade==g,"gluing local field grade mismatch");
    Values out; storage::StableSourceIndex index(anchor.endpoint()->source);
    input.for_each_cell([&](auto key) {
        guard.step(); const auto cell=index.cell({g,key}); const auto value=input.state(key);
        require(cell && value,"gluing Chain support changed"); guard.value(*value); out.emplace(cell->index,*value);
    });
    return out;
}
inline NativeGluingProvenance report(const State& s,NativeGluingAction action,std::uint64_t field,
    NativeGluingAvailability a,const runtime::ExecutionLease& lease,const Guard& guard,runtime::ExecutionRequirements required) {
    return {action,s.request.atlas.manifest().key,s.request.atlas.manifest().revision,s.request.pushout,field,s.keys,
        {s.pins[0].source_namespace(),s.pins[1].source_namespace(),s.pins[2].source_namespace()},
        {s.pins[0].sequence(),s.pins[1].sequence(),s.pins[2].sequence()},s.preparation,a,required,guard.limits(),s.steps,guard.steps(),
        s.policy,lease.policy().name,s.fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu};
}
} // namespace native_gluing_detail

class DerivedNativeGluedChain;
class PreparedNativeGluingMorphism;
class PreparedNativeGluing {
public:
    using Values=algebra::OwnedRelationalGluing::Values;
    static PreparedNativeGluing prepare(NativeGluingRequest request,NativeGluingAvailability available,
        runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_gluing_detail;
        lease.check_cancelled(); require(request.dimension_limit>0,"native gluing dimension limit must be positive");
        const auto& m=request.atlas.manifest();
        require(m.embeddings.size()==2 && m.pushouts.size()==1 && m.products.empty() && m.scale_maps.empty() && m.scale_paths.empty() &&
                m.linear_interfaces.empty() && m.finite_interfaces.empty() && request.atlas.sources().size()==3,
                "prepared native gluing requires exactly the declared three-source square");
        const auto& p=storage::native_atlas_detail::named(m.pushouts,request.pushout);
        const auto& l=storage::native_atlas_detail::named(m.embeddings,p.left_embedding);
        const auto& r=storage::native_atlas_detail::named(m.embeddings,p.right_embedding);
        require(l.from==r.from && l.to!=r.to,"native gluing requires a common interface and distinct participants");
        require(!l.declarations && !r.declarations && !p.left_common && !p.right_common,"prepared gluing uses one fixed complete signature; recodings require a separate profile");
        const std::array<std::uint64_t,3> keys{l.from,l.to,r.to};
        const auto anchor=[&](auto key){ return request.atlas.access(key,storage::NativeAccessionRequest::sparse_chain(core::Grade{0},{},{})); };
        const std::array<Pin,3> pins{anchor(keys[0]),anchor(keys[1]),anchor(keys[2])}; native_gluing_detail::available(pins,available);
        auto words=add(l.cells.size(),add(r.cells.size(),add(l.occurrences.size(),r.occurrences.size())));
        for (const auto& pin:pins) words=add(words,algebra::owned_gluing_detail::words(pin.endpoint()->source));
        const auto initial=add(1,mul(words,32)); runtime::admit_execution(requirements(words,initial),lease.policy());
        Guard guard(limits(lease,pins,request.dimension_limit),[&lease]{lease.check_cancelled();}); guard.entries(initial);
        for (const auto& pin:pins) algebra::owned_gluing_detail::source(pin.endpoint()->source,guard);
        // Precharge the existing atlas materialization's complete declared record
        // visit count before its one synchronous materialization; native
        // cancellation is checked at its call boundaries and throughout all
        // subsequent exact certification/actions. No source is published.
        std::size_t construction_steps{};
        for (const auto side:{1u,2u}) {
            const auto& c=pins[side].endpoint()->source;
            for (std::size_t k=0;k<c.grade_count();++k) {
                const auto& arena=c.grade(grade(k));
                construction_steps=add(construction_steps,add(arena.cell_count(),mul(arena.incidence_count(),add(c.signature().aspects().size(),2))));
            }
        }
        require(construction_steps<=request.atlas.limits().max_work,"native gluing materialization exceeds its atlas work budget");
        for (std::size_t k=0;k<std::max(pins[1].endpoint()->source.grade_count(),pins[2].endpoint()->source.grade_count());++k) {
            const auto count=[&](std::size_t i){const auto& c=pins[i].endpoint()->source; return k<c.grade_count()?c.grade(grade(k)).cell_count():0;};
            guard.dimension(add(count(1),count(2))-count(0));
        }
        for (std::size_t n=0;n<construction_steps;++n) guard.step();
        lease.check_cancelled(); auto glued=request.atlas.glue(request.pushout); lease.check_cancelled();
        algebra::RelationalGluingDeclaration d{m.key,m.revision,
            indexed(pins[0].endpoint()->source,pins[1].endpoint()->source,l.cells,l.occurrences,guard),
            indexed(pins[0].endpoint()->source,pins[2].endpoint()->source,r.cells,r.occurrences,guard),{}, {}};
        const auto inclusion=[&](std::size_t side) {
            std::vector<storage::NativeCellImage> cells; std::vector<storage::NativeOccurrenceImage> occurrences;
            for (const auto& [from,to]:glued.cells) if (from.source==keys[side]) cells.push_back({from.cell,to});
            for (const auto& [from,to]:glued.occurrences) if (from.source==keys[side]) occurrences.push_back({from.occurrence,to});
            return indexed(pins[side].endpoint()->source,glued.source,cells,occurrences,guard);
        };
        d.left_glued=inclusion(1); d.right_glued=inclusion(2);
        auto gluing=algebra::OwnedRelationalGluing::prepare(pins[0].endpoint()->source,pins[1].endpoint()->source,pins[2].endpoint()->source,glued.source,std::move(d),guard);
        runtime::admit_execution(requirements(gluing.retained_words(),add(gluing.entry_envelope(),initial)),lease.policy());
        lease.check_cancelled();
        return PreparedNativeGluing(std::make_shared<const State>(State{std::move(request),keys,pins,std::move(glued),std::move(gluing),
            available,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedRelationalGluing numeric_plan() const { return state_->gluing; }
    storage::NativeAtlas atlas() const { return state_->request.atlas; }
    std::array<std::uint64_t,3> source_keys() const { return state_->keys; }
    storage::PinnedNativeAccession source_anchor(algebra::GluingSource which) const {
        native_gluing_detail::require(which!=algebra::GluingSource::Glued,"materialized gluing is prospective, not a native publication");
        return state_->pins.at(static_cast<std::size_t>(which));
    }
    std::shared_ptr<const storage::NativeGluedSource> constructed_source() const { return {state_,&state_->constructed}; }
    NativeGluingAvailability preparation_availability() const { return state_->preparation; }
    bool same_owner(const PreparedNativeGluing& other) const noexcept { return state_==other.state_; }
    DerivedNativeGluedChain chain(core::Grade,Values,NativeGluingAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeGluedChain include(bool right,core::Grade,storage::PinnedNativeAccession,NativeGluingAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeGluedChain glue_sections(std::uint64_t field_identity,core::Grade,storage::PinnedNativeAccession left,
        storage::PinnedNativeAccession right,NativeGluingAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    DerivedNativeGluedChain boundary(DerivedNativeGluedChain,NativeGluingAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
private:
    friend class DerivedNativeGluedChain; friend class PreparedNativeGluingMorphism;
    explicit PreparedNativeGluing(std::shared_ptr<const native_gluing_detail::State> state):state_(std::move(state)) {}
    std::shared_ptr<const native_gluing_detail::State> state_;
};
class DerivedNativeGluedChain {
public:
    PreparedNativeGluing plan() const { return owner_; }
    const algebra::OwnedGluedChain& chain() const & { return chain_; }
    const algebra::OwnedGluedChain& chain() const &&=delete;
    const NativeGluingProvenance& provenance() const & { return report_; }
    const NativeGluingProvenance& provenance() const &&=delete;
    const std::vector<storage::PinnedNativeAccession>& local_inputs() const & { return inputs_; }
    const std::vector<storage::PinnedNativeAccession>& local_inputs() const &&=delete;
    std::shared_ptr<const DerivedNativeGluedChain> input() const { return input_; }
private:
    friend class PreparedNativeGluing; friend class PreparedNativeGluingMorphism;
    DerivedNativeGluedChain(PreparedNativeGluing owner,algebra::OwnedGluedChain chain,NativeGluingProvenance report,
        std::vector<storage::PinnedNativeAccession> inputs={},std::shared_ptr<const DerivedNativeGluedChain> input={})
        :owner_(std::move(owner)),chain_(std::move(chain)),report_(std::move(report)),inputs_(std::move(inputs)),input_(std::move(input)) {}
    PreparedNativeGluing owner_; algebra::OwnedGluedChain chain_; NativeGluingProvenance report_;
    std::vector<storage::PinnedNativeAccession> inputs_; std::shared_ptr<const DerivedNativeGluedChain> input_;
};
inline DerivedNativeGluedChain PreparedNativeGluing::chain(core::Grade g,Values values,NativeGluingAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_gluing_detail; const auto pin=state_; auto owned_values=values; lease.check_cancelled(); revalidate(*pin,a);
    const auto required=requirements(pin->gluing.retained_words(),add(pin->gluing.entry_envelope(),mul(values.size(),3))); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(lease,pin->pins,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});
    auto output=pin->gluing.chain(g,std::move(owned_values),guard); lease.check_cancelled();
    return {PreparedNativeGluing(pin),std::move(output),report(*pin,NativeGluingAction::DeclaredChain,0,a,lease,guard,required)};
}
inline DerivedNativeGluedChain PreparedNativeGluing::include(bool right,core::Grade g,storage::PinnedNativeAccession input,NativeGluingAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_gluing_detail; const auto pin=state_; lease.check_cancelled(); revalidate(*pin,a);
    const std::array<Pin,4> pins{pin->pins[0],pin->pins[1],pin->pins[2],input};
    const auto required=requirements(pin->gluing.retained_words(),add(pin->gluing.entry_envelope(),mul(input.support_count(),4))); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(lease,pins,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});
    auto output=pin->gluing.include(right,g,chain_input(*pin,right,input,g,guard),guard); lease.check_cancelled();
    return {PreparedNativeGluing(pin),std::move(output),report(*pin,NativeGluingAction::Inclusion,0,a,lease,guard,required),{std::move(input)}};
}
inline DerivedNativeGluedChain PreparedNativeGluing::glue_sections(std::uint64_t field,core::Grade g,storage::PinnedNativeAccession left,
    storage::PinnedNativeAccession right,NativeGluingAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_gluing_detail; const auto pin=state_; lease.check_cancelled(); revalidate(*pin,a); require(field!=0,"same-field gluing needs an explicit field identity");
    const std::array<Pin,5> pins{pin->pins[0],pin->pins[1],pin->pins[2],left,right};
    const auto required=requirements(pin->gluing.retained_words(),add(pin->gluing.entry_envelope(),mul(add(left.support_count(),right.support_count()),5))); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(lease,pins,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});
    auto lv=chain_input(*pin,false,left,g,guard),rv=chain_input(*pin,true,right,g,guard);
    auto output=pin->gluing.glue_sections(g,std::move(lv),std::move(rv),guard); lease.check_cancelled();
    return {PreparedNativeGluing(pin),std::move(output),report(*pin,NativeGluingAction::SameFieldSections,field,a,lease,guard,required),{std::move(left),std::move(right)}};
}
inline DerivedNativeGluedChain PreparedNativeGluing::boundary(DerivedNativeGluedChain input,NativeGluingAvailability a,runtime::ExecutionLease lease) const {
    using namespace native_gluing_detail; const auto pin=state_; lease.check_cancelled(); revalidate(*pin,a);
    require(input.owner_.state_==pin,"native gluing boundary needs its actual chain owner");
    const auto required=requirements(pin->gluing.retained_words(),add(pin->gluing.entry_envelope(),mul(input.chain_.values().size(),2))); runtime::admit_execution(required,lease.policy());
    Guard guard(limits(lease,pin->pins,pin->request.dimension_limit),[&lease]{lease.check_cancelled();});
    auto output=pin->gluing.boundary(input.chain_,guard); lease.check_cancelled();
    return {PreparedNativeGluing(pin),std::move(output),report(*pin,NativeGluingAction::Boundary,0,a,lease,guard,required),{},std::make_shared<const DerivedNativeGluedChain>(std::move(input))};
}

namespace native_gluing_detail {
struct MapState {
    PreparedNativeGluing source;
    NativeGluingMorphismRequest request;
    algebra::OwnedGluingMorphism map;
    NativeGluingMapAvailability preparation;
    std::size_t steps{};
    std::string policy;
    bool fallback{};
};
}
class MappedNativeGluingChain {
public:
    PreparedNativeGluingMorphism plan() const;
    const native_gluing_detail::Values& values() const & { return values_; }
    const native_gluing_detail::Values& values() const &&=delete;
    core::Grade grade() const { return input_->chain().grade(); }
    std::shared_ptr<const DerivedNativeGluedChain> input() const { return input_; }
    NativeGluingMapAvailability execution_availability() const { return availability_; }
    const NativeGluingProvenance& provenance() const & { return report_; }
    const NativeGluingProvenance& provenance() const &&=delete;
private:
    friend class PreparedNativeGluingMorphism;
    MappedNativeGluingChain(std::shared_ptr<const native_gluing_detail::MapState> owner,native_gluing_detail::Values values,
        std::shared_ptr<const DerivedNativeGluedChain> input,NativeGluingMapAvailability available,NativeGluingProvenance report)
        :owner_(std::move(owner)),values_(std::move(values)),input_(std::move(input)),availability_(available),report_(std::move(report)) {}
    std::shared_ptr<const native_gluing_detail::MapState> owner_; native_gluing_detail::Values values_;
    std::shared_ptr<const DerivedNativeGluedChain> input_; NativeGluingMapAvailability availability_; NativeGluingProvenance report_;
};
class PreparedNativeGluingMorphism {
public:
    static PreparedNativeGluingMorphism prepare(PreparedNativeGluing source,NativeGluingMorphismRequest input,
        NativeGluingMapAvailability available,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_gluing_detail; auto request=input; lease.check_cancelled(); revalidate(*source.state_,available.source);
        runtime::admit_work_availability(request.target.source_namespace(),request.target.sequence(),available.target);
        require(request.target.kind()==storage::NativeAccessionKind::Chain && request.target.support_count()==0,"gluing morphism target must be an empty owning Chain anchor");
        const auto keys=source.source_keys();
        require(request.target_source && request.left.key && request.right.key && request.left.key!=request.right.key &&
            request.left.from==keys[1] && request.right.from==keys[2] && request.left.to==request.target_source && request.right.to==request.target_source,
            "gluing morphism needs complete declared participant and target source identities");
        const std::array<Pin,4> pins{source.state_->pins[0],source.state_->pins[1],source.state_->pins[2],request.target};
        const auto words=add(source.numeric_plan().retained_words(),add(algebra::owned_gluing_detail::words(request.target.endpoint()->source),
            add(add(request.left.cells.size(),request.right.cells.size()),add(request.left.occurrences.size(),request.right.occurrences.size()))));
        const auto envelope=add(source.numeric_plan().entry_envelope(),mul(words,32)); runtime::admit_execution(requirements(words,envelope),lease.policy());
        Guard guard(limits(lease,pins,source.state_->request.dimension_limit),[&lease]{lease.check_cancelled();}); guard.entries(envelope);
        auto left=indexed(source.state_->pins[1].endpoint()->source,request.target.endpoint()->source,request.left.cells,request.left.occurrences,guard);
        auto right=indexed(source.state_->pins[2].endpoint()->source,request.target.endpoint()->source,request.right.cells,request.right.occurrences,guard);
        auto map=algebra::OwnedGluingMorphism::prepare(source.numeric_plan(),request.target.endpoint()->source,std::move(left),std::move(right),guard);
        runtime::admit_execution(requirements(map.retained_words(),add(map.entry_envelope(),envelope)),lease.policy()); lease.check_cancelled();
        return PreparedNativeGluingMorphism(std::make_shared<const native_gluing_detail::MapState>(native_gluing_detail::MapState{
            std::move(source),std::move(request),std::move(map),available,guard.steps(),lease.policy().name,lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeGluing source() const { return state_->source; }
    storage::PinnedNativeAccession target() const { return state_->request.target; }
    const NativeGluingMorphismRequest& declaration() const & { return state_->request; }
    const NativeGluingMorphismRequest& declaration() const &&=delete;
    const algebra::OwnedGluingMorphism& numeric_plan() const & { return state_->map; }
    const algebra::OwnedGluingMorphism& numeric_plan() const &&=delete;
    NativeGluingMapAvailability preparation_availability() const { return state_->preparation; }
    MappedNativeGluingChain execute(DerivedNativeGluedChain input,NativeGluingMapAvailability available,runtime::ExecutionLease lease=runtime::ExecutionLease{}) const {
        using namespace native_gluing_detail; const auto pin=state_; lease.check_cancelled();
        require(pin->source.same_owner(input.plan()),"native gluing morphism needs its actual input owner");
        revalidate(*pin->source.state_,pin->preparation.source); revalidate(*pin->source.state_,available.source);
        runtime::admit_work_availability(pin->request.target.source_namespace(),pin->request.target.sequence(),pin->preparation.target);
        runtime::admit_work_availability(pin->request.target.source_namespace(),pin->request.target.sequence(),available.target);
        const std::array<Pin,4> pins{pin->source.state_->pins[0],pin->source.state_->pins[1],pin->source.state_->pins[2],pin->request.target};
        const auto required=requirements(pin->map.retained_words(),add(pin->map.entry_envelope(),mul(input.chain_.values().size(),2))); runtime::admit_execution(required,lease.policy());
        Guard guard(limits(lease,pins,pin->source.state_->request.dimension_limit),[&lease]{lease.check_cancelled();});
        auto output=pin->map.apply(input.chain_,guard); lease.check_cancelled();
        auto receipt=report(*pin->source.state_,NativeGluingAction::Mediator,0,available.source,lease,guard,required);
        receipt.preparation_steps=pin->steps; receipt.preparation_policy=pin->policy; receipt.fallback=receipt.fallback || pin->fallback;
        return {pin,std::move(output),std::make_shared<const DerivedNativeGluedChain>(std::move(input)),available,std::move(receipt)};
    }
private:
    friend class MappedNativeGluingChain;
    explicit PreparedNativeGluingMorphism(std::shared_ptr<const native_gluing_detail::MapState> state):state_(std::move(state)) {}
    std::shared_ptr<const native_gluing_detail::MapState> state_;
};
inline PreparedNativeGluingMorphism MappedNativeGluingChain::plan() const { return PreparedNativeGluingMorphism(owner_); }
} // namespace rfc::inference
