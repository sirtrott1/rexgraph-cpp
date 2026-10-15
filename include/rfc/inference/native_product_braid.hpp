#pragma once
#include "rfc/algebra/owned_product_braid.hpp"
#include "rfc/inference/native_product_hexagon.hpp"

namespace rfc::inference {
enum class NativeProductBraidAction { AdjacentWitness, YangBaxterWitness, CellWitness };
struct NativeProductBraidProvenance {
    NativeProductBraidAction action;
    std::uint64_t declaration{}, revision{}, hexagon_declaration{}, hexagon_revision{};
    std::array<std::uint64_t,3> factors, namespaces, sequences;
    NativeProductHexagonAvailability preparation_availability, adoption_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, adoption_steps{}, numeric_steps{}, native_steps{}, execution_steps{};
    std::string preparation_policy, adoption_policy, execution_policy;
    bool fallback{};
    algebra::ProductHexagonCarrier source_carrier, target_carrier;
    std::optional<algebra::ProductBraidGenerator> generator;
};
namespace native_braid_detail {
using algebra::product_braid_detail::add;
using algebra::product_braid_detail::mul;
using algebra::product_braid_detail::require;
using native_product_detail::Guard;
struct State {
    PreparedNativeProductHexagon hexagon;
    algebra::OwnedProductBraid numeric;
    NativeProductHexagonAvailability adoption;
    std::size_t preparation_steps{}, steps{};
    std::string preparation_policy, policy;
    bool fallback{};
};
inline runtime::ExecutionRequirements requirements(const algebra::OwnedProductHexagon& h,std::size_t support=0) {
    return native_product_detail::requirements(algebra::product_braid_detail::words(h),algebra::product_braid_detail::entries(h,support));
}
inline void admit(const State& s,NativeProductHexagonAvailability a,const runtime::ExecutionLease& lease,std::size_t support=0) {
    lease.check_cancelled(); const auto& r=s.hexagon.declaration();
    native_hexagon_detail::available(r,s.hexagon.preparation_availability()); native_hexagon_detail::available(r,s.adoption); native_hexagon_detail::available(r,a);
    runtime::admit_execution(requirements(s.numeric.hexagon(),support),lease.policy());
}
inline void account(Guard& guard,std::size_t native,const runtime::ExecutionLease& lease) {
    lease.check_cancelled(); if (add(guard.steps(),native)>guard.limits().steps) throw std::length_error("product braid total work limit");
}
inline std::vector<DerivedNativeHexagonField> trace(const State& s,DerivedNativeHexagonField input,
    const std::vector<algebra::ProductHexagonAction>& word,NativeProductHexagonAvailability a,const runtime::ExecutionLease& lease,Guard& guard,std::size_t& native) {
    std::vector<DerivedNativeHexagonField> out{input};
    for (const auto action : word) {
        guard.step(); const auto start=guard.steps();
        const auto expected=s.numeric.hexagon().step(out.back().field(),action,guard);
        const auto cost=guard.steps()-start;
        // Reserve the exact canonical phase cost before dispatching it. The
        // rehearsal uses the same immutable plan and current arithmetic limits.
        account(guard,add(native,cost),lease);
        auto next=s.hexagon.step(out.back(),action,a,lease);
        require(next.provenance().execution_steps==cost && next.field().values()==expected.values() &&
            s.numeric.hexagon().carrier(next.field())==s.numeric.hexagon().carrier(expected),"native braid phase must match its exact admitted canonical rehearsal");
        native=add(native,next.provenance().execution_steps); account(guard,native,lease); out.push_back(std::move(next));
    }
    return out;
}
inline NativeProductBraidProvenance report(const State& s,NativeProductBraidAction action,NativeProductHexagonAvailability a,
    const runtime::ExecutionLease& lease,Guard& guard,std::size_t native,algebra::ProductHexagonCarrier from,algebra::ProductHexagonCarrier to,
    std::size_t support=0,std::optional<algebra::ProductBraidGenerator> generator={},bool input_fallback=false) {
    account(guard,native,lease); const auto& r=s.hexagon.declaration(); std::array<std::uint64_t,3> ns{},seq{};
    for (std::size_t i=0;i<3;++i) { ns[i]=r.factors[i].source_namespace(); seq[i]=r.factors[i].sequence(); }
    return {action,s.numeric.declaration().key,s.numeric.declaration().revision,r.declaration.key,r.declaration.revision,r.factor_identities,ns,seq,
        s.hexagon.preparation_availability(),s.adoption,a,requirements(s.numeric.hexagon(),support),guard.limits(),s.preparation_steps,s.steps,guard.steps(),native,add(guard.steps(),native),
        s.preparation_policy,s.policy,lease.policy().name,s.fallback || input_fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu,from,to,generator};
}
} // namespace native_braid_detail
class NativeProductBraidStepWitness;
class NativeProductBraidWitness;
class NativeProductBraidCellWitness;
class PreparedNativeProductBraid {
public:
    static PreparedNativeProductBraid adopt(PreparedNativeProductHexagon input,algebra::ProductBraidDeclaration declaration,
        NativeProductHexagonAvailability a,runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        namespace detail=native_braid_detail; const auto d=declaration;
        lease.check_cancelled(); const auto& r=input.declaration();
        native_hexagon_detail::available(r,input.preparation_availability()); native_hexagon_detail::available(r,a);
        runtime::admit_execution(detail::requirements(input.numeric_plan()),lease.policy());
        detail::Guard guard(native_product_detail::limits(lease,r.factors,r.dimension_limit),[&lease]{lease.check_cancelled();});
        auto numeric=algebra::OwnedProductBraid::adopt(input.numeric_plan(),d,guard);
        const auto c=algebra::product_hexagon_detail::carrier(0); const auto start=guard.steps();
        (void)numeric.hexagon().field(c,{0},{},guard); const auto cost=guard.steps()-start;
        detail::account(guard,cost,lease);
        // An actual empty canonical observation exposes the original native
        // preparation metadata without changing source or publishing an endpoint.
        const auto anchor=input.field(c,{0},{},a,lease); const auto& report=anchor.provenance();
        detail::require(report.execution_steps==cost,"native braid adoption probe must match its exact reserved work"); lease.check_cancelled();
        return PreparedNativeProductBraid(std::make_shared<const detail::State>(detail::State{
            std::move(input),std::move(numeric),a,report.preparation_steps,detail::add(guard.steps(),cost),report.preparation_policy,lease.policy().name,
            report.fallback || lease.policy().backend!=runtime::ExecutionBackend::ExactCpu}));
    }
    bool same_owner(const PreparedNativeProductBraid& other) const noexcept { return state_==other.state_; }
    PreparedNativeProductHexagon hexagon() const { return state_->hexagon; }
    algebra::OwnedProductBraid numeric_plan() const { return state_->numeric; }
    NativeProductHexagonAvailability adoption_availability() const { return state_->adoption; }
    DerivedNativeHexagonField adjacent(DerivedNativeHexagonField,algebra::ProductBraidGenerator,NativeProductHexagonAvailability,
        runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeProductBraidStepWitness step_witness(DerivedNativeHexagonField,algebra::ProductBraidGenerator,NativeProductHexagonAvailability,
        runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeProductBraidWitness witness(DerivedNativeHexagonField,NativeProductHexagonAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
    NativeProductBraidCellWitness cell_witness(std::array<core::CellId,3>,NativeProductHexagonAvailability,runtime::ExecutionLease=runtime::ExecutionLease{}) const;
private:
    explicit PreparedNativeProductBraid(std::shared_ptr<const native_braid_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const native_braid_detail::State> state_;
};
class NativeProductBraidStepWitness {
public:
    PreparedNativeProductBraid plan() const { return plan_; }
    const algebra::ProductBraidStepWitness& witness() const & { return witness_; }
    const algebra::ProductBraidStepWitness& witness() const && = delete;
    const std::vector<DerivedNativeHexagonField>& native_path() const & { return path_; }
    const std::vector<DerivedNativeHexagonField>& native_path() const && = delete;
    const DerivedNativeHexagonField& image() const & { return path_.back(); }
    const DerivedNativeHexagonField& image() const && = delete;
    const NativeProductBraidProvenance& provenance() const & { return report_; }
    const NativeProductBraidProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductBraid;
    NativeProductBraidStepWitness(PreparedNativeProductBraid p,algebra::ProductBraidStepWitness w,std::vector<DerivedNativeHexagonField> path,NativeProductBraidProvenance r)
        : plan_(std::move(p)),witness_(std::move(w)),path_(std::move(path)),report_(std::move(r)) {}
    PreparedNativeProductBraid plan_;
    algebra::ProductBraidStepWitness witness_;
    std::vector<DerivedNativeHexagonField> path_;
    NativeProductBraidProvenance report_;
};
class NativeProductBraidWitness {
public:
    PreparedNativeProductBraid plan() const { return plan_; }
    const algebra::ProductBraidWitness& witness() const & { return witness_; }
    const algebra::ProductBraidWitness& witness() const && = delete;
    const std::array<std::vector<DerivedNativeHexagonField>,2>& native_paths() const & { return paths_; }
    const std::array<std::vector<DerivedNativeHexagonField>,2>& native_paths() const && = delete;
    const NativeProductBraidProvenance& provenance() const & { return report_; }
    const NativeProductBraidProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductBraid;
    NativeProductBraidWitness(PreparedNativeProductBraid p,algebra::ProductBraidWitness w,std::array<std::vector<DerivedNativeHexagonField>,2> paths,NativeProductBraidProvenance r)
        : plan_(std::move(p)),witness_(std::move(w)),paths_(std::move(paths)),report_(std::move(r)) {}
    PreparedNativeProductBraid plan_;
    algebra::ProductBraidWitness witness_;
    std::array<std::vector<DerivedNativeHexagonField>,2> paths_;
    NativeProductBraidProvenance report_;
};
class NativeProductBraidCellWitness {
public:
    PreparedNativeProductBraid plan() const { return plan_; }
    const algebra::ProductBraidCellWitness& witness() const & { return witness_; }
    const algebra::ProductBraidCellWitness& witness() const && = delete;
    const NativeProductBraidProvenance& provenance() const & { return report_; }
    const NativeProductBraidProvenance& provenance() const && = delete;
private:
    friend class PreparedNativeProductBraid;
    NativeProductBraidCellWitness(PreparedNativeProductBraid p,algebra::ProductBraidCellWitness w,NativeProductBraidProvenance r)
        : plan_(std::move(p)),witness_(std::move(w)),report_(std::move(r)) {}
    PreparedNativeProductBraid plan_;
    algebra::ProductBraidCellWitness witness_;
    NativeProductBraidProvenance report_;
};
inline NativeProductBraidStepWitness PreparedNativeProductBraid::step_witness(DerivedNativeHexagonField input,algebra::ProductBraidGenerator generator,
    NativeProductHexagonAvailability a,runtime::ExecutionLease lease) const {
    namespace detail=native_braid_detail; const auto pin=state_; const auto support=input.field().values().size(); detail::admit(*pin,a,lease,support);
    detail::require(pin->hexagon.same_owner(input.plan()),"native adjacent witness needs its actual hexagon field");
    const auto h=pin->numeric.hexagon(); const auto from=h.carrier(input.field());
    std::size_t native{}; detail::Guard* active{};
    detail::Guard guard(native_product_detail::limits(lease,pin->hexagon.declaration().factors,pin->hexagon.declaration().dimension_limit),[&]{
        lease.check_cancelled(); if (active && detail::add(active->steps(),native)>=active->limits().steps) throw std::length_error("product braid total work limit"); }); active=&guard;
    auto w=pin->numeric.step_witness(input.field(),generator,guard);
    auto path=detail::trace(*pin,input,algebra::product_braid_detail::actions(from,generator),a,lease,guard,native);
    detail::require(h.carrier(path.back().field())==h.carrier(w.data().image) && path.back().field().values()==w.data().image.values(),"canonical native adjacent path agrees with its numeric witness");
    auto report=detail::report(*pin,NativeProductBraidAction::AdjacentWitness,a,lease,guard,native,from,h.carrier(path.back().field()),support,generator,input.provenance().fallback);
    return NativeProductBraidStepWitness(PreparedNativeProductBraid(pin),std::move(w),std::move(path),std::move(report));
}
inline DerivedNativeHexagonField PreparedNativeProductBraid::adjacent(DerivedNativeHexagonField input,algebra::ProductBraidGenerator generator,
    NativeProductHexagonAvailability a,runtime::ExecutionLease lease) const {
    const auto w=step_witness(std::move(input),generator,a,std::move(lease)); return w.image();
}
inline NativeProductBraidWitness PreparedNativeProductBraid::witness(DerivedNativeHexagonField input,NativeProductHexagonAvailability a,runtime::ExecutionLease lease) const {
    namespace detail=native_braid_detail; const auto pin=state_; const auto support=input.field().values().size(); detail::admit(*pin,a,lease,support);
    detail::require(pin->hexagon.same_owner(input.plan()),"native Yang-Baxter witness needs its actual hexagon field");
    const auto h=pin->numeric.hexagon(); const auto from=h.carrier(input.field());
    std::size_t native{}; detail::Guard* active{};
    detail::Guard guard(native_product_detail::limits(lease,pin->hexagon.declaration().factors,pin->hexagon.declaration().dimension_limit),[&]{
        lease.check_cancelled(); if (active && detail::add(active->steps(),native)>=active->limits().steps) throw std::length_error("product braid total work limit"); }); active=&guard;
    auto w=pin->numeric.witness(input.field(),guard);
    auto first=detail::trace(*pin,input,algebra::product_braid_detail::word(from,0),a,lease,guard,native);
    auto second=detail::trace(*pin,input,algebra::product_braid_detail::word(from,1),a,lease,guard,native);
    std::array<std::vector<DerivedNativeHexagonField>,2> paths{std::move(first),std::move(second)};
    for (std::size_t r=0;r<2;++r) {
        std::size_t position{};
        for (const auto& step : w.routes()[r].steps) for (const auto& edge : step.data().edges) {
            guard.step(); const auto& actual=paths[r].at(++position);
            detail::require(h.carrier(actual.field())==h.carrier(edge.image) && actual.field().values()==edge.image.values(),"every canonical native braid intermediate matches its owning numeric observation");
        }
        detail::require(position+1==paths[r].size(),"complete canonical native braid history");
    }
    auto report=detail::report(*pin,NativeProductBraidAction::YangBaxterWitness,a,lease,guard,native,from,h.carrier(paths[0].back().field()),support,{},input.provenance().fallback);
    return NativeProductBraidWitness(PreparedNativeProductBraid(pin),std::move(w),std::move(paths),std::move(report));
}
inline NativeProductBraidCellWitness PreparedNativeProductBraid::cell_witness(std::array<core::CellId,3> ids,NativeProductHexagonAvailability a,runtime::ExecutionLease lease) const {
    namespace detail=native_braid_detail; const auto pin=state_; detail::admit(*pin,a,lease);
    detail::Guard guard(native_product_detail::limits(lease,pin->hexagon.declaration().factors,pin->hexagon.declaration().dimension_limit),[&lease]{lease.check_cancelled();});
    auto w=pin->numeric.cell_witness(ids,guard); const auto c=algebra::product_hexagon_detail::carrier(0);
    auto report=detail::report(*pin,NativeProductBraidAction::CellWitness,a,lease,guard,0,c,c);
    return NativeProductBraidCellWitness(PreparedNativeProductBraid(pin),std::move(w),std::move(report));
}
} // namespace rfc::inference
