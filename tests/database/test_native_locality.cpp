#include "rfc/database/native_field_view.hpp"
#include "rfc/database/field_work.hpp"
#include "rfc/database/locality_native_field.hpp"
#include "rfc/database/staged_field_work.hpp"
#include "rfc/database/native_semantic_field.hpp"
#include "rfc/local/owned_semantic_boundary.hpp"
#include "rfc/local/interior_interface_solve.hpp"
#include "native_execution_fixture.hpp"
#include <iostream>
#include <future>
#include <latch>

namespace {
using namespace native_execution_fixture;
using namespace rfc::database;
std::size_t checks{};
void check(bool value,const char* why) { ++checks; if(!value)throw std::runtime_error(why); }
template<class F> void rejects(F&& f,const char* why) {
    bool failed{};try{f();}catch(const std::exception&){failed=true;}check(failed,why);
}
NativeFieldAnalysis analysis(const NativeAccessionDirectory& d,bool scalar=false) {
    return NativeFieldAnalysis::inspect(anchor(d),request(d),scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{});
}
void prepared_adoption() {
    for(const bool scalar:{false,true}) {
        auto e=endpoint(scalar);NativeAccessionDirectory directory(e,77);
        const auto a=analysis(directory,scalar);
        const auto prepared=prepare_parallel_field(a,1);
        NativeFieldCache cache({},[](auto){return std::uint64_t{0};});
        runtime::CpuTileExecutor executor({4,4,1000000});
        std::size_t compiles{},adopts{};
        const auto first=cache.evaluate_parallel(prepared,executor,runtime::ExecutionLease{},[&](auto stage){
            if(stage==NativeFieldCacheStage::BeforeCompile)++compiles;
            if(stage==NativeFieldCacheStage::BeforeAdopt)++adopts;
        });
        const auto reference=execute(prepare_field_action(anchor(directory),request(directory),
            scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{}));
        check(first.values()==reference.values(),"adopted action differs from independent reference");
        check(compiles==0 && adopts==1 && first.reuse().prepared,"cold request did not adopt sealed owner");
        check(cache.stats().compiled_plans==0 && cache.stats().adopted_plans==1 && cache.stats().prepared_input_uses==1,
            "adoption counters confused compilation or input materialization");
        const auto view=NativeFieldVectorView::input(prepared);
        check(view.values_pin()==prepared.input_pin(),"native input view copied numerical payload");
        for(const auto& b:a.binding().inputs()) {
            const auto& stalk=view.at_cell(b.cell);
            check(stalk.binding().carrier==b.carrier && stalk.binding().coordinates==b.coordinates,"view lost carrier/coordinates");
            check(stalk.numerical_values().data()==prepared.input_pin()->data()+b.offset,"resolved stalk span points elsewhere");
            for(std::size_t j=0;j<b.dimension;++j)
                check(stalk.state(j)==std::optional<Q>{prepared.input_pin()->at(b.offset+j)},"native view lost exact input");
            rejects([&]{(void)stalk.state(b.dimension);},"view accepted a coordinate outside carrier");
        }
        const auto output=NativeFieldVectorView::output(first);
        check(!output.is_input() && output.values_pin()==first.values_pin(),"result view failed to retain exact owner");
        const auto warm=cache.evaluate_parallel(prepared,executor);
        check(warm.reuse().result && warm.values_pin()==first.values_pin(),"sealed warm request missed result cache");
        auto host=runtime::ExecutionHostPolicy{};host.limits.integer_bits=1;
        rejects([&]{(void)cache.evaluate_parallel(prepared,executor,runtime::ExecutionLease(host));},"warm view bypassed current bit limit");
        auto cancel=std::make_shared<std::atomic_bool>(true);
        rejects([&]{(void)cache.evaluate_parallel(prepared,executor,runtime::ExecutionLease({},cancel));},"warm view bypassed cancellation");
        auto next=e;next.sequence=1;
        const auto field=next.tensors.front().key;
        auto& tensor=next.tensors.front().tensor;
        tensor={tensor.domain(),tensor.support(),numeric::SparseRationalVector(tensor.domain().logical_size(),{{0,Q{19,23}}})};
        directory.advance(make_temporal_segment(e,next));
        const auto current=prepare_parallel_field(analysis(directory,scalar),1);
        const auto changed=cache.evaluate_parallel(current,executor);
        check(changed.reuse().plan && !changed.reuse().result,"changed input conflated plan and result");
        check(changed.source_publication()!=first.source_publication() && first.provenance().sequence==0,"old source pin changed");
        for(const auto& b:current.analysis().binding().inputs())if(b.field==field)
            check(NativeFieldVectorView::input(current).values_pin()->at(b.offset)==Q{19,23},"prepared input came from old publication");
        check(view.source_publication()->sequence==0 && output.source_publication()->sequence==0,"native views followed mutable directory");
        rejects([&]{(void)make_cached_field_work({1,1},current,std::make_shared<NativeFieldCache>(),1,{},
            runtime::WorkAvailability{current.report().source_namespace,0});},"prepared work admitted future publication");
        NativeFieldCache failed;
        rejects([&]{(void)failed.evaluate_parallel(prepared,executor,runtime::ExecutionLease{},[](auto stage){
            if(stage==NativeFieldCacheStage::BeforeAdopt)throw std::runtime_error("injected adoption failure");
        });},"adoption hook failure was hidden");
        check(failed.stats().results==0 && failed.stats().plans==0 && failed.stats().in_flight==0,"failed adoption leaked publication/job");
        check(failed.evaluate_parallel(prepared,executor).values()==reference.values(),"adoption failure poisoned retry");
    }
}
void factored_fields() {
    for(const bool scalar:{false,true}) {
        auto e=endpoint(scalar);NativeAccessionDirectory directory(e,77);
        const auto a=analysis(directory,scalar);
        const auto ordinary=prepare_parallel_field(a,1);
        const auto fibers=prepare_fiber_field(a,2);
        const auto reference=execute(prepare_field_action(anchor(directory),request(directory),
            scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{}));
        for(const std::size_t workers:{1u,2u,4u}) {
            runtime::CpuTileExecutor executor({workers,4,1000000});auto workspace=make_parallel_workspace(fibers);
            const auto result=execute_parallel(fibers,workspace,executor);
            check(result.values()==reference.values(),"fiber action differs from reference");
            check(result.provenance().implementation=="PreparedGradeFieldAction/GMP/fiber-tiles-v1","fiber execution hid its implementation");
        }
        const auto& action=fibers.ranges().action();const auto& old=ordinary.ranges().action();
        check(action.statistics().factored && action.blocks().size()==old.blocks().size(),"fiber layout changed retained occurrences");
        check(action.statistics().entries==old.statistics().entries,"fiber layout changed logical map entries");
        check(action.target_offsets()==old.target_offsets(),"fiber layout changed complete row contribution counts");
        check(action.target_order()==old.target_order(),"fiber diagnostic traversal lost reversible mapping");
        for(std::size_t i=0;i<action.entries().size();++i) {
            const auto& x=action.entries()[i];const auto& y=old.entries()[i];
            check(x.source==y.source && x.target==y.target && x.block==y.block && x.coefficient==y.coefficient,
                "fiber diagnostic entries changed declared maps/coefficient order");
            check(action.physical_entry(action.logical_entry(i))==i,"fiber logical map failed round trip");
        }
        NativeFieldCache cache;runtime::CpuTileExecutor executor;
        check(cache.evaluate_parallel(fibers,executor).values()==reference.values(),"cache did not execute sealed fibers");
        check(cache.evaluate(a).provenance().implementation=="PreparedGradeFieldAction/GMP/fiber-tiles-v1",
            "cross-layout result reuse changed producer facts");
    }
    NativeAccessionDirectory directory(endpoint(true),77);
    const auto empty=prepare_fiber_field(NativeFieldAnalysis::inspect(anchor(directory,core::Grade{2}),request(directory,core::Grade{2}),
        NativeFieldRealization::constant_fiber(900)),1);
    runtime::CpuTileExecutor executor;auto workspace=make_parallel_workspace(empty);const auto result=execute_parallel(empty,workspace,executor);
    check(result.values()==numeric::RationalVector(empty.analysis().binding().target_dimension()),"empty higher grade lost supported lower zeros");
}
void locality_partitions() {
    runtime::HardwareInventory hardware;hardware.allowed_cpus={{4,0,0,{}}};
    hardware.caches={{1,32768,64,"Data",{4}},{2,1024,64,"Unified",{99}},{2,4096,64,"Unified",{4}}};
    runtime::LocalityTilePolicy policy{4,6,4096,2,4};
    const auto tile=runtime::plan_locality_tiles({{2,512},{3,512},{2,512},{9,2048},{1,512}},hardware,policy);
    check(tile.cache && tile.cache->bytes==4096 && tile.working_set_target==1024,"tile policy used inaccessible cache");
    check(tile.rows==std::vector<std::pair<std::size_t,std::size_t>>{{0,2},{2,3},{3,4},{4,5}} && tile.oversized_rows==1,
        "incidence tile ownership/work budget changed");
    hardware.caches.clear();check(!runtime::plan_locality_tiles({{1,1}},hardware,policy).cache,"unknown cache became observed");
    auto bad=policy;bad.max_rows=0;rejects([&]{(void)runtime::plan_locality_tiles({},hardware,bad);},"zero tile target accepted");
    NativeAccessionDirectory directory(endpoint(true),77);const auto sealed=prepare_fiber_field(analysis(directory,true),1);
    auto custom=policy;custom.max_rows=3;custom.scalar_work=2;custom.object_bytes=256;
    const auto tiled=retile_field(sealed,hardware,custom);
    runtime::CpuTileExecutor executor({4,4,1000000});NativeFieldCache cache;
    const auto expected=execute(prepare_field_action(anchor(directory),request(directory),NativeFieldRealization::constant_fiber(900))).values();
    check(cache.evaluate_parallel(tiled,executor).values()==expected,"uneven incidence tiles changed field");
    cache.clear();check(cache.evaluate_parallel(sealed,executor).values()==expected,"scratch reuse ignored tile partition");
    auto workspace=make_parallel_workspace(tiled);check(execute_parallel(tiled,workspace,executor).values()==expected,"retiled action failed");
    const auto& action=sealed.ranges().action();
    rejects([&]{(void)local::PreparedGradeFieldRanges::partition(action,{{1,action.statistics().target_dimension}});},"partition omitted first row");
    rejects([&]{(void)local::PreparedGradeFieldRanges::partition(action,{{0,1},{0,action.statistics().target_dimension}});},"partition overlap accepted");
    rejects([&]{(void)local::PreparedGradeFieldRanges::partition(action,{});},"empty incomplete partition accepted");
}
void regions_and_sections() {
    for(const bool scalar:{false,true}) {
        NativeAccessionDirectory directory(endpoint(scalar),77);const auto sealed=prepare_fiber_field(analysis(directory,scalar),1);
        const auto reference=execute(prepare_field_action(anchor(directory),request(directory),
            scalar?NativeFieldRealization::constant_fiber(900):NativeFieldRealization{}));
        for(const auto& b:sealed.analysis().binding().outputs()) {
            const auto region=PreparedNativeFieldRegion::select(sealed,{b.cell});const auto output=region.execute();
            check(output.values().size()==b.dimension,"region allocated an unnamed full output");
            for(std::size_t j=0;j<b.dimension;++j)check(output.values()[j]==reference.values()[b.offset+j],"region missed outside coface");
            std::size_t incoming{};for(const auto& o:sealed.analysis().binding().occurrences())if(o.lower==b.cell)++incoming;
            check(region.incoming().size()==incoming,"region omitted retained inactive/repeated incoming incidence");
            check(output.plan().operation().analysis().source_publication()==sealed.analysis().source_publication(),"region lost publication owner");
            rejects([&]{(void)region.execute(runtime::ExecutionLease{},runtime::WorkAvailability{999,0});},"region accepted unrelated availability namespace");
        }
        rejects([&]{(void)PreparedNativeFieldRegion::select(sealed,{{core::Grade{0},999999}});},"region accepted undeclared output");
        const auto empty=PreparedNativeFieldRegion::select(sealed,{});const auto empty_result=empty.execute();check(empty_result.values().empty(),"empty region changed output contract");
    }
    const auto e=endpoint(true);local::LinearRelationalFiberSystem original(local::LinearStalkBundle(e.source,1));
    const local::OwnedLinearFiberSystem owner(original);
    const core::CellId cell{core::Grade{0},0};const local::RelationalLocalCarrier carrier(owner.fibers().complex());
    const auto patch=carrier.cell_patch(cell);
    const local::OwnedLinearSection one(owner,patch,{{cell,{Q{1}}}}),two(owner,patch,{{cell,{Q{2}}}});
    const auto glued=one.glue_compatible(one),summed=one.sum_contributions(one),different=one.sum_contributions(two);
    check(glued.values().at(cell)==numeric::RationalVector{Q{1}},"section glue summed shared intrinsic value");
    check(summed.values().at(cell)==numeric::RationalVector{Q{2}},"boundary sum glued contributions");
    check(different.values().at(cell)==numeric::RationalVector{Q{3}},"contribution assembly averaged values");
    rejects([&]{(void)one.glue_compatible(two);},"incompatible interface glued");
    const local::OwnedLinearSection unrelated(local::OwnedLinearFiberSystem(original),patch,{{cell,{Q{1}}}});
    rejects([&]{(void)one.glue_compatible(unrelated);},"equal values inferred carrier identity");
    rejects([&]{(void)local::OwnedLinearSection(owner,patch,{});},"section omitted patch stalk");
    const auto restricted=one.restrict_to(patch);check(restricted.values()==one.values(),"owned restriction changed source section");
    const local::LinearSection raw{{cell,{Q{7,7}}}};
    const local::OwnedLinearSection normalized(owner,patch,raw);
    check(normalized.values()==one.values(),"owned section retained noncanonical arithmetic input");
    const auto equal=normalized.glue_compatible(one),double_value=normalized.sum_contributions(one);
    check(equal.values()==one.values() && double_value.values()==summed.values(),"section interface used raw fraction storage for equality");
    check(raw.at(cell).front().get_str()=="7/7","section normalization mutated supplied source values");
    Q invalid{1};mpz_set_ui(mpq_denref(invalid.get_mpq_t()),0);
    rejects([&]{(void)local::OwnedLinearSection(owner,patch,{{cell,{invalid}}});},"owned section admitted zero denominator");
}
local::OwnedSemanticBoundary semantic(bool retract) {
    core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder mb(sig,core::SemanticScale{1});mb.add_grade_one(edge,{{10,true,true,false,1},{20,true,false,false,1}});
    core::RelationalComplexBuilder ub(sig,core::SemanticScale{0});ub.add_grade_one(edge,{{10,true,true,false,1},{15,true,false,false,1}});
    ub.add_grade_one(edge,{{15,true,true,false,1},{20,true,false,false,1}});
    const auto macro=mb.freeze(),micro=ub.freeze();local::LinearRelationalFiberSystem mf{local::LinearStalkBundle(macro)},uf{local::LinearStalkBundle(micro)};
    std::vector<geometry::LinearGradeMap> cells(2);cells[0]={{{0,Q{1}}},{{2,Q{1}}}};cells[1]={{{0,Q{1}},{1,Q{1}}}};
    geometry::SemanticScaleRealization realization(core::SemanticScale{1},core::SemanticScale{0},geometry::RelationalChainMap(macro,micro,cells));
    std::vector<numeric::ExactMatrix> maps{numeric::ExactMatrix::from_rows({{1,0},{0,0},{0,1}}),numeric::ExactMatrix::from_rows({{1},{1}})};
    std::optional<std::vector<numeric::ExactMatrix>> reverse;
    if(retract)reverse=std::vector<numeric::ExactMatrix>{numeric::ExactMatrix::from_rows({{1,0,0},{0,1,1}}),numeric::ExactMatrix::from_rows({{1,0}})};
    const local::SemanticStalkRealization stalks(mf,uf,realization,maps,reverse);
    return local::OwnedSemanticBoundary(stalks);
}
void semantic_and_solves() {
    const auto owned=semantic(true);
    const auto result=owned.apply_fine(core::Grade{1},{Q{3},Q{5}});
    check(result.macro_input==numeric::RationalVector{Q{3}} && result.fine_residual==numeric::RationalVector({Q{0},Q{2}}),"semantic residual truncated fine data");
    check(result.full_boundary==numeric::RationalVector({Q{-3},Q{-2},Q{5}}),"macro plus residual differs from exact micro action");
    check(owned.apply_realized(core::Grade{1},{Q{3}})==numeric::RationalVector({Q{-3},Q{0},Q{3}}),"declared macro action lost naturality");
    rejects([&]{(void)semantic(false).apply_fine(core::Grade{1},{Q{3},Q{5}});},"rectangular realization inferred a retraction");
    rejects([&]{(void)owned.apply_realized(core::Grade{0},{});},"semantic action accepted grade zero");
    numeric::ExactActionLimits low;low.steps=1;rejects([&]{(void)owned.apply_fine(core::Grade{1},{Q{3},Q{5}},low);},"semantic action bypassed current step limit");
    const auto matrix=numeric::ExactMatrix::from_rows({{2,-1,0},{-1,2,-1},{0,-1,2}});
    const local::LinearSolveDeclaration declaration{10,20,30,40,numeric::IndexDomain::linear(3),numeric::IndexDomain::linear(3)};
    for(const auto& interior:std::vector<std::vector<std::size_t>>{{},{0},{0,2},{2,0},{0,1,2}}) {
        const local::PreparedInteriorInterfaceSolve solve(declaration,matrix,interior);
        for(const auto& rhs:std::vector<numeric::RationalVector>{{1,0,0},{Q{2,7},Q{-3,5},Q{4,11}},{0,0,0}})
            check(solve.apply(rhs)==matrix.solve(rhs),"interior/interface reconstruction differs from independent full solve");
        check(solve.declaration().input_carrier==10 && solve.declaration().output_carrier==20,"solve identified separate declared carriers");
        auto limits=numeric::ExactActionLimits{};limits.integer_bits=1;
        rejects([&]{(void)solve.apply({1,0,0},limits);},"prepared solve bypassed current arithmetic limit");
        rejects([&]{(void)solve.apply({1,0,0},{},[]{throw std::runtime_error("cancel solve");});},"solve ignored checkpoint failure");
    }
    rejects([&]{(void)local::PreparedInteriorInterfaceSolve(declaration,matrix,{0,0});},"solve accepted duplicate interior coordinates");
    rejects([&]{(void)local::PreparedInteriorInterfaceSolve(declaration,numeric::ExactMatrix::from_rows({{0,1,0},{1,0,0},{0,0,1}}),{0});},"singular interior implicitly inferred inverse");
    rejects([&]{(void)numeric::PreparedExactSolve(numeric::ExactMatrix::from_rows({{1,1},{1,1}}));},"singular prepared factor accepted");
    core::RelationSignature signature;const auto type=signature.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto complex=[&](core::SemanticScale scale) {
        core::RelationalComplexBuilder builder(signature,scale);
        for(unsigned i=0;i<2;++i)builder.add_grade_one(type,{{10,true,true,false,1},{20,true,false,false,1}});
        builder.add_higher(core::Grade{2},type,{{0,true,false,Q{1}},{1,true,true,Q{1}}});return builder.freeze();
    };
    const auto a=complex(core::SemanticScale{1}),b=complex(core::SemanticScale{0});
    local::LinearRelationalFiberSystem af{local::LinearStalkBundle(a)},bf{local::LinearStalkBundle(b)};
    const auto deformed=numeric::ExactMatrix::from_rows({{Q{-2}}});af.set_transport(core::Grade{2},1,deformed);bf.set_transport(core::Grade{2},1,deformed);
    std::vector<geometry::LinearGradeMap> cells;
    std::vector<numeric::ExactMatrix> identity;
    for(std::size_t k=0;k<a.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};geometry::LinearGradeMap row(a.grade(grade).cell_count());
        for(std::size_t i=0;i<row.size();++i)row[i].push_back({static_cast<std::uint32_t>(i),Q{1}});
        cells.push_back(std::move(row));identity.push_back(numeric::ExactMatrix::identity(af.stalks().total_dimension(grade)));
    }
    const geometry::SemanticScaleRealization natural(core::SemanticScale{1},core::SemanticScale{0},geometry::RelationalChainMap(a,b,cells));
    const local::SemanticStalkRealization declared(af,bf,natural,identity);
    rejects([&]{(void)local::OwnedSemanticBoundary(declared);},"naturality alone admitted a nonzero complete realized chain family");
}
void native_semantic_publications() {
    core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder a(sig,core::SemanticScale{1}),b(sig,core::SemanticScale{0});
    a.add_grade_one(edge,{{10,true,true,false,1},{20,true,false,false,1}});
    b.add_grade_one(edge,{{10,true,true,false,1},{15,true,false,false,1}});b.add_grade_one(edge,{{15,true,true,false,1},{20,true,false,false,1}});
    const auto endpoint_for=[](core::RelationalComplex source) {
        storage::TemporalEndpoint e(std::move(source));
        for(std::size_t k=0;k<e.source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            for(std::size_t i=0;i<e.source.grade(grade).cell_count();++i) {
                const auto cell=storage::stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)});const auto carrier=10000+100*k+i;
                e.carriers.push_back({carrier,storage::TemporalCarrierKind::CellStalk,grade,cell,900,numeric::IndexDomain::linear(1)});
                if(k) {
                    storage::TemporalTensor tensor;tensor.key=20000+i;tensor.role=storage::TemporalTensorRole::Field;
                    tensor.owner.kind=storage::TemporalOwnerKind::Cell;tensor.owner.cell=cell;tensor.output=carrier;
                    tensor.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{static_cast<long>(3+2*i)}}})};
                    e.tensors.push_back(std::move(tensor));
                }
            }
        }
        return e;
    };
    auto macro_endpoint=endpoint_for(a.freeze()),micro_endpoint=endpoint_for(b.freeze());
    NativeAccessionDirectory macro_directory(macro_endpoint,101),micro_directory(micro_endpoint,202);
    const auto prepare=[](const NativeAccessionDirectory& directory) {
        storage::NativeFieldRequest q;
        for(const auto& carrier:directory.pin_snapshot()->carriers)q.stalk_carriers.push_back(carrier.key);
        for(const auto& tensor:directory.pin_snapshot()->tensors) {
            storage::NativeAccessionRequest field;field.kind=storage::NativeAccessionKind::Field;field.subject=tensor.key;q.fields.push_back(directory.pin(field));
        }
        return prepare_fiber_field(NativeFieldAnalysis::inspect(anchor(directory),q,NativeFieldRealization::constant_fiber(900)),1);
    };
    const auto macro=prepare(macro_directory),micro=prepare(micro_directory);
    const std::vector<geometry::LinearGradeMap> cells{{{{0,Q{1}}},{{2,Q{1}}}},{{{0,Q{1}},{1,Q{1}}}}};
    const std::vector<numeric::ExactMatrix> maps{numeric::ExactMatrix::from_rows({{1,0},{0,0},{0,1}}),numeric::ExactMatrix::from_rows({{1},{1}})};
    const std::vector<numeric::ExactMatrix> retract{numeric::ExactMatrix::from_rows({{1,0,0},{0,1,1}}),numeric::ExactMatrix::from_rows({{1,0}})};
    const auto plan=PreparedNativeSemanticField::prepare_static(macro,micro,303,cells,maps,retract);
    check(plan.static_declaration()==303,"semantic static declaration identity was lost");
    rejects([&]{(void)PreparedNativeSemanticField::prepare_static(macro,micro,0,cells,maps,retract);},"semantic constants were implicitly declared without identity");
    const auto result=plan.execute_fine();check(result.decomposition().full_boundary==numeric::RationalVector({-3,-2,5}),"native semantic result changed sealed micro input");
    check(result.plan().macro().analysis().source_publication()==macro.analysis().source_publication(),"native semantic lost macro owner");
    auto next=micro_endpoint;next.sequence=1;next.tensors[1].tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{7}}})};
    micro_directory.advance(make_temporal_segment(micro_endpoint,next));
    const auto future=PreparedNativeSemanticField::prepare_static(macro,prepare(micro_directory),303,cells,maps,retract);
    rejects([&]{(void)future.execute_fine(runtime::ExecutionLease{},runtime::WorkAvailability{101,0},runtime::WorkAvailability{202,0});},"semantic plan admitted future micro publication");
    const auto current=future.execute_fine(runtime::ExecutionLease{},runtime::WorkAvailability{101,0},runtime::WorkAvailability{202,1});
    check(current.decomposition().full_boundary==numeric::RationalVector({-3,-4,7}) && result.plan().micro().analysis().source_publication()->sequence==0,
        "semantic publication advance changed old output/source pins");
    rejects([&]{(void)plan.execute_fine(runtime::ExecutionLease{},runtime::WorkAvailability{202,0});},"semantic macro namespace replaced by micro namespace");
}
runtime::HardwareInventory hardware() {
    runtime::HardwareInventory h;h.affinity_observed=true;h.allowed_cpus={{0,0,0,{}},{1,0,1,{}},{2,0,2,{}},{3,0,3,{}}};return h;
}
void staged_resources() {
    const auto h=hardware();runtime::CpuResourceCoordinator resources(h,{4,4096,1000000});
    {
        runtime::StagedResourceLease stage(resources,{"interior",4,0,0,1024,0});
        check(resources.available_workers()==0 && resources.available_declared_buffer_bytes()==3072,"stage initial charge omitted resources");
        stage.enter({"interface",1,0,256,0,128});
        check(resources.available_workers()==3 && resources.available_declared_buffer_bytes()==3712,"stage did not release interior resources");
        auto other=resources.reserve(3,2048);
        rejects([&]{stage.enter({"busy",4,0,0,0,0});},"busy stage upgrade ignored concurrent reservation");
        check(resources.available_workers()==0 && resources.available_declared_buffer_bytes()==1664,"failed upgrade changed previous charge");
        auto value=stage.finish(Q{7,11},128);check(*value==Q{7,11} && resources.available_workers()==1,"finished output retained CPU workers");
        check(stage.receipts().size()==2,"stage receipt lost completed phase");
    }
    check(resources.available_workers()==4 && resources.available_declared_buffer_bytes()==4096,"stage/output exception paths leaked cooperative resources");
    NativeAccessionDirectory directory(endpoint(true),77);const auto sealed=prepare_fiber_field(analysis(directory,true),1);
    const auto region=PreparedNativeFieldRegion::select(sealed,{sealed.analysis().binding().outputs().front().cell});
    std::shared_ptr<const numeric::RationalVector> escaped_values;
    std::optional<NativeFieldRegionResult> escaped_result;
    {
        auto run=runtime::WorkGraph({make_staged_region_work({1,1},region)}).submit(resources);
        const auto output=run.completion({1,1}).result<StagedNativeFieldResult>();run.wait();
        const auto expected=region.execute();check(output->output->values()==expected.values() && output->stages.size()==3,"phased native job lost exact output/receipts");
        check(resources.available_workers()==4,"phased job retained caller after completion");
        check(resources.available_declared_buffer_bytes()==4096-region.output_dimension()*sizeof(Q),"output pin lost its declared resource charge");
        escaped_values=output->output->values_pin();escaped_result=*output->output;
    }
    check(resources.available_declared_buffer_bytes()==4096-region.output_dimension()*sizeof(Q),"escaped payload/result copy lost declared charge");
    escaped_result.reset();check(resources.available_declared_buffer_bytes()==4096-region.output_dimension()*sizeof(Q),"numerical pin released charge before payload");
    escaped_values.reset();
    check(resources.available_declared_buffer_bytes()==4096,"completed phased graph leaked retained output budget");
}
void concurrent_phases_and_solves() {
    runtime::CpuResourceCoordinator resources(hardware(),{4,4096,1000000});std::latch second_entered(1);
    auto phased=runtime::WorkJob::make_phased<std::shared_ptr<const int>>({10,1},{4,1024,4},{4,1024,4},{},
        [&](const auto&,auto& assigned,auto reservation,auto) {
            runtime::StagedResourceLease stage(assigned,std::move(reservation),{"interior",4,0,0,1024,0});
            stage.enter({"interface",1,0,0,128,0});second_entered.wait();return stage.finish(7,4);
        });
    auto other=runtime::WorkJob::make<int>({11,1},{3,256,4},{},[&](const auto&,auto& assigned,auto) {
        auto charge=assigned.reserve(3,256);second_entered.count_down();return 9;
    });
    std::shared_ptr<const int> retained;
    {
        auto run=runtime::WorkGraph({phased,other}).submit(resources);run.wait();
        retained=*run.completion({10,1}).result<std::shared_ptr<const int>>();
        check(*retained==7 && *run.completion({11,1}).result<int>()==9,"phase release did not admit independent job");
        check(run.peak_jobs_workers().second<=4,"phase admission bound exceeded coordinator capacity");
    }
    check(resources.available_workers()==4 && resources.available_declared_buffer_bytes()==4092,"output pin did not preserve only retained bytes");
    retained.reset();check(resources.available_declared_buffer_bytes()==4096,"escaped output owner leaked resource charge");
    const auto failed=runtime::WorkJob::make_phased<int>({12,1},{1,0,4},{2,128,4},{},[](const auto&,auto& assigned,auto reservation,auto)->int {
        runtime::StagedResourceLease stage(assigned,std::move(reservation),{"resolve",1,0,0,0,0});stage.enter({"execute",2,0,0,128,0});throw std::bad_alloc{};
    });
    auto failed_run=runtime::WorkGraph({failed}).submit(resources);failed_run.wait();
    rejects([&]{(void)failed_run.completion({12,1}).result<int>();},"phased failure published result");
    check(resources.available_workers()==4 && resources.available_declared_buffer_bytes()==4096,"phased failure leaked reservation");
    std::latch produced(1),release(1);
    auto cancelled=runtime::WorkJob::make_phased<std::shared_ptr<const int>>({13,1},{1,0,4},{1,128,4},{},[&](const auto&,auto& assigned,auto reservation,auto) {
        runtime::StagedResourceLease stage(assigned,std::move(reservation),{"resolve",1,0,0,0,0});stage.enter({"execute",1,0,0,128,0});
        auto value=stage.finish(17,4);produced.count_down();release.wait();return value;
    });
    auto cancelled_run=runtime::WorkGraph({cancelled}).submit(resources);produced.wait();cancelled_run.completion({13,1}).cancel();release.count_down();cancelled_run.wait();
    rejects([&]{(void)cancelled_run.completion({13,1}).result<std::shared_ptr<const int>>();},"cancelled phase published output");
    check(resources.available_workers()==4 && resources.available_declared_buffer_bytes()==4096,"cancelled phase leaked output charge");
    runtime::CpuResourceCoordinator small(hardware(),{4,1024,1000000});
    auto large=runtime::WorkJob::make_phased<int>({14,1},{1,0,800},{1,1024,800},{},[](const auto&,auto&,auto,auto){return 0;});
    auto flat=runtime::WorkJob::make<int>({15,1},{1,512,4},{},[](const auto&,auto&,auto){return 0;});
    rejects([&]{(void)runtime::WorkGraph({large,flat}).submit(small);},"own-graph retained outputs could deadlock remaining flat job");
    const auto matrix=numeric::ExactMatrix::from_rows({{0,1,0},{1,0,1},{0,1,1}});
    const numeric::PreparedExactSolve plan(matrix);
    std::vector<std::future<bool>> results;
    for(unsigned t=0;t<4;++t)results.push_back(std::async(std::launch::async,[&,t] {
        for(unsigned i=0;i<32;++i) {
            const numeric::RationalVector rhs{Q{static_cast<long>(t+i+1),7},Q{static_cast<long>(i+2),11},Q{-3,13}};
            if(plan.apply(rhs)!=matrix.solve(rhs))return false;
        }
        return true;
    }));
    for(auto& result:results)check(result.get(),"shared exact factor mutated concurrent right-hand sides");
}
void raw_rational_inputs() {
    const auto identity=numeric::ExactMatrix::identity(2);
    const numeric::RationalVector raw{Q{7,7},Q{6,9}},canonical{Q{1},numeric::rational(2,3)};
    check(identity.apply(raw)==canonical && identity.solve(raw)==canonical,"reference matrix did not canonicalize raw input");
    const numeric::PreparedExactSolve factor(identity);
    check(factor.apply(raw)==canonical,"prepared solve compared unreduced residual storage");
    const local::LinearSolveDeclaration declaration{1,2,3,4,numeric::IndexDomain::linear(2),numeric::IndexDomain::linear(2)};
    const local::PreparedInteriorInterfaceSolve split(declaration,identity,{0});
    check(split.apply(raw)==canonical,"interface solve lost raw rational equivalence");
    const auto boundary=semantic(true);
    check(boundary.apply_fine(core::Grade{1},raw).full_boundary==boundary.apply_fine(core::Grade{1},canonical).full_boundary,
        "fine decomposition performed arithmetic on unreduced input");
    check(boundary.apply_realized(core::Grade{1},{Q{7,7}})==boundary.apply_realized(core::Grade{1},{Q{1}}),
        "semantic boundary performed arithmetic on unreduced input");
    check(raw[0].get_str()=="7/7" && raw[1].get_str()=="6/9","normalization mutated caller/source storage");
    Q invalid{1};mpz_set_ui(mpq_denref(invalid.get_mpq_t()),0);
    rejects([&]{(void)identity.apply({invalid,Q{0}});},"reference action admitted zero denominator");
    rejects([&]{(void)factor.apply({invalid,Q{0}});},"prepared action admitted zero denominator");
    numeric::ExactActionLimits limits;limits.integer_bits=2;
    rejects([&]{(void)factor.apply(raw,limits);},"canonicalization bypassed raw integer bit admission");
}
void emit_oracle() {
    for(unsigned case_number=0;case_number<40;++case_number) {
        const bool scalar=case_number%3==0,realized=!scalar && case_number%3==1,zero=case_number%10==0;
        auto e=endpoint(scalar,realized,zero);
        for(auto& tensor:e.tensors) {
            if(tensor.role==TemporalTensorRole::Field && !zero) {
                std::vector<numeric::SparseRationalEntry> entries;
                for(std::size_t j=0;j<tensor.tensor.domain().logical_size();++j) {
                    Q q{static_cast<long>(case_number+3*j+1),static_cast<long>(11+j)};
                    if(case_number==38 && j==0){numeric::Integer big=1;big<<=1100;q=Q{big,numeric::Integer{37}};}
                    entries.push_back({j,q});
                }
                tensor.tensor={tensor.tensor.domain(),tensor.tensor.support(),numeric::SparseRationalVector(tensor.tensor.domain().logical_size(),entries)};
            } else if(tensor.role==TemporalTensorRole::CoefficientIndependentMap || tensor.role==TemporalTensorRole::RealizedIncidenceMap) {
                auto matrix=temporal_tensor_matrix(tensor);
                if(case_number%13==0)matrix=numeric::ExactMatrix(matrix.rows(),matrix.cols());
                else matrix=matrix.scale(Q{static_cast<long>(case_number+1),17});
                tensor.tensor=temporal_matrix_tensor(matrix);
            }
        }
        NativeAccessionDirectory directory(e,77);const auto prepared=prepare_fiber_field(analysis(directory,scalar),1);
        NativeFieldCache cache;runtime::CpuTileExecutor executor({4,4,1000000});const auto result=cache.evaluate_parallel(prepared,executor);
        const auto& plan=prepared.analysis().binding();const auto tensors=temporal_endpoint_detail::tensors(*result.source_publication());
        std::cout<<"case "<<case_number<<'\n';
        for(const auto& b:plan.inputs()) {
            std::cout<<"upper "<<b.cell.key<<' '<<b.dimension<<'\n';
            for(std::size_t j=0;j<b.dimension;++j)std::cout<<"x "<<b.cell.key<<' '<<j<<' '<<*tensors.at(b.field)->tensor.state(j)<<'\n';
        }
        for(const auto& b:plan.outputs()) {
            std::cout<<"lower "<<b.cell.key<<' '<<b.dimension<<'\n';
            const auto selected=PreparedNativeFieldRegion::select(prepared,{b.cell}).execute();
            for(std::size_t j=0;j<b.dimension;++j) {
                std::cout<<"y "<<b.cell.key<<' '<<j<<' '<<result.values()[b.offset+j]<<'\n';
                std::cout<<"z "<<b.cell.key<<' '<<j<<' '<<selected.values()[j]<<'\n';
            }
        }
        for(const auto& b:plan.occurrences()) {
            std::cout<<"occ "<<b.occurrence.key.value<<' '<<b.upper.key<<' '<<b.lower.key<<' '<<b.active<<' '<<b.coefficient<<' '
                <<(b.map?(b.role==TemporalTensorRole::RealizedIncidenceMap?"realized":"independent"):"constant")<<'\n';
            if(b.map) {
                const auto matrix=temporal_tensor_matrix(*tensors.at(*b.map));
                for(const auto& [key,q]:matrix.entries())
                    std::cout<<"m "<<b.occurrence.key.value<<' '<<key.first<<' '<<key.second<<' '<<q<<'\n';
            }
        }
        std::cout<<"end\n";
    }
}
void solve_probe() {
    std::size_t n{},count{};
    while(std::cin>>n>>count) {
        if(n>16 || count>n)throw std::invalid_argument("solve probe size");
        std::vector<std::size_t> interior(count);for(auto& i:interior)std::cin>>i;
        numeric::ExactMatrix matrix(n,n);Q q;
        for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j){std::cin>>q;matrix.set(i,j,q);}
        numeric::RationalVector rhs(n);for(auto& v:rhs)std::cin>>v;
        if(!std::cin)throw std::invalid_argument("truncated solve probe");
        const local::LinearSolveDeclaration declaration{10,20,30,40,numeric::IndexDomain::linear(n),numeric::IndexDomain::linear(n)};
        const local::PreparedInteriorInterfaceSolve plan(declaration,matrix,interior);
        const auto out=plan.apply(rhs);std::cout<<out.size();for(const auto& v:out)std::cout<<' '<<v;std::cout<<'\n';
    }
}
void semantic_probe() {
    const auto plan=semantic(true);Q a,b;
    while(std::cin>>a>>b) {
        const auto out=plan.apply_fine(core::Grade{1},{a,b});
        for(const auto* values:{&out.macro_input,&out.fine_residual,&out.full_boundary})for(const auto& q:*values)std::cout<<q<<' ';
        std::cout<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2) {
            const std::string mode=argv[1];
            if(mode=="--oracle")emit_oracle();else if(mode=="--solve-probe")solve_probe();else if(mode=="--semantic-probe")semantic_probe();else throw std::invalid_argument("unknown probe");
            return 0;
        }
        prepared_adoption();factored_fields();locality_partitions();regions_and_sections();semantic_and_solves();native_semantic_publications();staged_resources();concurrent_phases_and_solves();raw_rational_inputs();std::cout<<checks<<" native locality checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
