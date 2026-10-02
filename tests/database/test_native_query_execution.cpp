#include "../../examples/query_tensor/sample.hpp"
#include "rfc/database/native_role_join.hpp"
#include "interpretation_fixture.hpp"
#include <iostream>
#include <thread>

namespace {
using namespace query_tensor_sample;
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F&& f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
void queries() {
    auto source=InterpretedSnapshot::interpret(schema(),input(131));const auto bytes=encode_interpreted_snapshot(source);
    const auto cutoff=available(source);auto req=request();
    // Keep a repeated operation; only the identical path binding is shared.
    req.all_of.push_back(req.all_of.front());
    const auto op=PreparedNativeQuery::prepare(source,10,req,cutoff,runtime::ExecutionLease{},17);
    check(op.projection_count()==3 && op.payload_resolutions()==131*3,"path binding not shared or incompletely resolved");
    check(op.request().all_of.size()==4,"duplicate operation removed");
    std::vector<std::uint64_t> expected;std::size_t unknown=0,numeric_count=0;std::optional<Q> sum;
    for (const auto key:source.assertions(10)) {
        auto truth=NativeTruth::True;
        for (const auto& p:req.all_of) {
            const auto scalar=native_query_detail::scalar(source.schema(),10,p.factors);
            const auto t=native_query_detail::evaluate(scalar,p,*source.payload(key,p.factors).declared_value());
            if (t==NativeTruth::False){truth=t;break;}
            if (t==NativeTruth::Unknown)truth=t;
        }
        if (truth==NativeTruth::Unknown)++unknown;
        if (truth!=NativeTruth::True)continue;
        expected.push_back(key);
        const auto value=source.payload(key,*req.sum).declared_value();
        const auto& option=std::get<V::Optional>(value->data);
        if (!option.element.empty()) {if (!sum)sum=Q{0};*sum+=std::get<Q>(option.element.front().data);++numeric_count;}
    }
    for (const std::size_t workers:{1u,2u,4u}) {
        runtime::CpuTileExecutor executor({workers,4,1000});
        for (unsigned repeat=0;repeat<4;++repeat) {
            const auto result=op.execute(cutoff,executor);
            check(result.keys()==expected && result.selected_count()==expected.size(),"fused keys/order mismatch");
            check(result.unknown_count()==unknown && result.sum()==sum && result.numeric_contributions()==numeric_count,"three-valued aggregate mismatch");
            check(result.source_publication()==source.endpoint() && result.cpu_report().completed_tiles==8,"query lost source or work");
        }
    }
    runtime::CpuTileExecutor executor;
    const auto indexed=PreparedNativeQuery::prepare(source,10,req,cutoff,runtime::ExecutionLease{},17,NativeQueryIndex::NumericRanges);
    const auto indexed_result=indexed.execute(cutoff,executor);
    check(indexed.indexed_projection_count()==1 && indexed.candidate_row_count()<op.row_count(),"numeric range index did not narrow rows");
    check(indexed_result.keys()==expected && indexed_result.unknown_count()==unknown && indexed_result.sum()==sum,"index dropped unknowns, duplicates or source order");
    check(indexed_result.predicate_evaluations()<op.execute(cutoff,executor).predicate_evaluations(),"selective index did not reduce predicate work");
    NativeQueryCache cache(1<<20);
    const auto fresh=cache.execute(op,cutoff,executor),hit=cache.execute(indexed,cutoff,executor);
    check(!fresh.cache_hit() && hit.cache_hit() && hit.keys()==expected,"exact dependency cache failed");
    auto unchanged=input(131);unchanged.revision="measurements/unused-factor";
    for(auto& a:unchanged.assertions) {
        auto& outer=std::get<V::Product>(a.payload.data);
        auto& inner=std::get<V::Product>(outer.elements.front().data);
        inner.elements.back()=V::integer(777);
    }
    const auto later=source.replace(unchanged);
    const auto later_op=PreparedNativeQuery::prepare(later,10,req,available(later));
    const auto reused=cache.execute(later_op,available(later),executor);
    check(reused.cache_hit() && reused.source_publication()==later.endpoint() && reused.availability().max_sequence==later.sequence(),"dependency cache lost new publication provenance");
    rejects([&]{(void)cache.execute(later_op,cutoff,executor);},"cache admitted future input");
    auto reordered=req;std::swap(reordered.all_of[0],reordered.all_of[1]);
    const auto new_order=PreparedNativeQuery::prepare(source,10,reordered,cutoff);
    check(!cache.execute(new_order,cutoff,executor).cache_hit(),"cache identified distinct ordered operations");
    auto bounded=runtime::ExecutionHostPolicy{};bounded.limits.records=0;
    rejects([&]{(void)cache.execute(new_order,cutoff,executor,runtime::ExecutionLease(bounded));},"cache bypassed current host limits");
    for (const auto kind:{ExactPredicateKind::Equal,ExactPredicateKind::NotEqual,ExactPredicateKind::Less,ExactPredicateKind::LessEqual,
                         ExactPredicateKind::Greater,ExactPredicateKind::GreaterEqual,ExactPredicateKind::IsNull,ExactPredicateKind::IsNotNull}) {
        ExactScalarPredicate p{{21,11},kind,V::rational(Q{0})};
        const auto one=PreparedNativeQuery::prepare(source,10,{{p},{},NativeQueryOutput::Keys},cutoff);
        const auto result=one.execute(cutoff,executor);
        check(result.keys()==select_exact(source,10,p),"single predicate differs from original query");
        const auto range=PreparedNativeQuery::prepare(source,10,{{p},{},NativeQueryOutput::Keys},cutoff,runtime::ExecutionLease{},13,NativeQueryIndex::NumericRanges);
        const auto ranged=range.execute(cutoff,executor);
        check(ranged.keys()==result.keys() && ranged.unknown_count()==result.unknown_count(),"indexed range/null comparison differs");
    }
    const auto all=PreparedNativeQuery::prepare(source,10,{{},std::vector<std::uint64_t>{21,11},NativeQueryOutput::Aggregate},cutoff);
    const auto aggregate=all.execute(cutoff,executor);
    check(aggregate.keys().empty() && aggregate.selected_count()==131 && aggregate.sum()==sum_exact(source,10,{21,11}),"aggregate-only request materialized keys or changed sum");
    const auto ints=PreparedNativeQuery::prepare(source,10,{{},std::vector<std::uint64_t>{21,14},NativeQueryOutput::Keys},cutoff);
    const auto integer_result=ints.execute(cutoff,executor);
    check(integer_result.sum()==sum_exact(source,10,{21,14}),"unbounded integer sum changed");
    auto empty=InterpretedSnapshot::interpret(schema(),input(0));
    const auto none=PreparedNativeQuery::prepare(empty,10,request(),available(empty));
    const auto empty_result=none.execute(available(empty),executor);
    check(empty_result.keys().empty() && !empty_result.sum(),"empty source acquired a numeric value");
    rejects([&]{(void)PreparedNativeQuery::prepare(empty,10,{{{{21,13},ExactPredicateKind::Less,V::symbol(7)}},{},NativeQueryOutput::Keys},available(empty));},"empty source bypassed declaration validation");
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,{{{{21,11},ExactPredicateKind::Equal,V::integer(0)}},{},NativeQueryOutput::Keys},cutoff);},"comparison coerced integer to rational");
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,{{},std::vector<std::uint64_t>{21,12},NativeQueryOutput::Aggregate},cutoff);},"Boolean sum accepted");
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,request(),{999,0});},"query accepted foreign namespace");
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,request(),cutoff,runtime::ExecutionLease{},0);},"query accepted zero tile width");
    auto host=runtime::ExecutionHostPolicy{};host.limits.records=1;
    rejects([&]{(void)op.execute(cutoff,executor,runtime::ExecutionLease(host));},"prepared query bypassed current record cap");
    host={};host.limits.integer_bits=1;
    rejects([&]{(void)op.execute(cutoff,executor,runtime::ExecutionLease(host));},"prepared query bypassed current bit cap");
    host={};host.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{(void)op.execute(cutoff,executor,runtime::ExecutionLease(host));},"rational query fabricated a GPU backend");
    host.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=op.execute(cutoff,executor,runtime::ExecutionLease(host));
    check(fallback.cpu_fallback() && fallback.execution_policy().backend==runtime::ExecutionBackend::ExactGpu,"explicit CPU fallback provenance lost");
    check(fallback.assertion_kind()==10 && fallback.schema().source_namespace==cutoff.source_namespace,"result lost assertion/declaration identity");
    host={};host.allow_allocation=false;
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,req,cutoff,runtime::ExecutionLease(host));},"query ignored allocation refusal");
    host={};host.limits.integer_bits=64;
    const auto unused=V::integer(numeric::Integer{1}<<1001);
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,{{{{21,11},ExactPredicateKind::IsNull,unused}},{},NativeQueryOutput::Keys},cutoff,runtime::ExecutionLease(host));},"unused literal bypassed copy bit admission");
    V unused_container;unused_container.data=V::Optional{{V::integer(0),unused}};
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,{{{{21,11},ExactPredicateKind::IsNull,unused_container}},{},NativeQueryOutput::Keys},cutoff,runtime::ExecutionLease(host));},"unused metadata child bypassed copy bit admission");
    auto cancel=std::make_shared<std::atomic_bool>(true);
    rejects([&]{(void)op.execute(cutoff,executor,runtime::ExecutionLease({},cancel));},"query ignored cancellation");
    auto changed=input(131);changed.revision="measurements/future";
    for (auto& a:changed.assertions)a.payload=payload(Q{777},true,7,numeric::Integer{2});
    source=source.replace(changed);
    rejects([&]{(void)PreparedNativeQuery::prepare(source,10,req,cutoff);},"query admitted a future publication");
    const auto retained_result=op.execute(cutoff,executor);
    check(retained_result.keys()==expected && op.source_publication()->sequence==0,"prepared query followed replacement");
    std::vector<std::jthread> threads;
    // Separate byte slots avoid vector<bool>'s packed-word race.
    std::vector<unsigned char> ok(8);
    for (std::size_t i=0;i<ok.size();++i)threads.emplace_back([&,i]{runtime::CpuTileExecutor cpu({2,2,1000});const auto result=op.execute(cutoff,cpu);ok[i]=result.keys()==expected;});
    for(auto& thread:threads)thread.join();
    for (const auto good:ok)check(good!=0,"concurrent immutable query changed results");
    check(encode_temporal_endpoint(*op.source_publication())==encode_temporal_endpoint(*decode_interpreted_snapshot(bytes).endpoint()),"query mutated retained source bytes");
}
void windows() {
    for (unsigned encoding=0;encoding<3;++encoding) {
        auto domain=numeric::IndexDomain({3,5,7});std::vector<std::size_t> support;
        for (std::size_t i=0;i<domain.logical_size();++i)if (i%3) support.push_back(i);
        const auto s=encoding==0?numeric::StructuralSupport::full(domain.logical_size()):encoding==1?
            numeric::StructuralSupport::sparse(domain.logical_size(),support):numeric::StructuralSupport::packed(domain.logical_size(),support);
        numeric::SparseRationalVector values(domain.logical_size(),{{1,Q{3}},{8,Q{-3}},{68,Q{7,11}},{100,Q{2}}});
        auto source=tensor_source({domain,s,values});const auto cutoff=available(source);const auto pin=source.payload(1);
        const auto bytes=encode_interpreted_snapshot(source);
        for (const auto& bounds:std::vector<std::pair<std::vector<std::size_t>,std::vector<std::size_t>>>{{{0,0,0},{3,5,7}},{{1,1,2},{3,4,7}},{{3,0,0},{3,5,7}},{{0,0,0},{1,2,7}}}) {
            const auto window=NativeTensorWindow::prepare(pin,bounds.first,bounds.second,cutoff);
            check(window.tensor_pin().get()==pin.tensor().get(),"window copied stored tensor");
            std::size_t count=0,nonzero=0;std::optional<Q> sum;
            for (std::size_t i=0;i<window.domain().logical_size();++i) {
                const auto coords=window.domain().unflatten(i);auto original=coords;
                for (std::size_t axis=0;axis<coords.size();++axis)original[axis]+=bounds.first[axis];
                const auto source_index=domain.flatten(original);const auto c=window.coordinate(i);
                check(c.source_index()==source_index && window.window_index(source_index)==i,"window coordinate realization not reversible");
                check(c.state()==pin.tensor()->state(source_index),"window changed absence/zero/value");
                if (c.supported()) {++count;if (!sum)sum=Q{0};*sum+=*c.state();}
                if (c.stored_nonzero())++nonzero;
            }
            const auto summary=window.summarize(cutoff);
            check(summary.supported_count()==count && summary.nonzero_count()==nonzero && summary.sum()==sum,"sparse window summary differs from coordinate scan");
            check(summary.domain()==window.domain() && summary.lower()==bounds.first && summary.upper()==bounds.second && summary.source_descriptor()==pin.descriptor(),"summary lost window or accession provenance");
            rejects([&]{(void)window.coordinate(window.domain().logical_size());},"window accepted outside coordinate");
            auto host=runtime::ExecutionHostPolicy{};host.limits.scalar_work=0;
            if (window.requirements().scalar_work)rejects([&]{(void)window.summarize(cutoff,runtime::ExecutionLease(host));},"window bypassed current work cap");
            rejects([&]{(void)window.summarize({999,0});},"window accepted foreign availability");
        }
        const auto w=NativeTensorWindow::prepare(pin,{0,0,0},{3,5,7},cutoff);auto escaped=w.coordinate(68);
        check(escaped.nonzero_value() && *escaped.nonzero_value()==Q{7,11},"coordinate lost nonzero pointer");
        rejects([&]{(void)NativeTensorWindow::prepare(pin,{0},{1},cutoff);},"window accepted wrong rank");
        rejects([&]{(void)NativeTensorWindow::prepare(pin,{0,0,0},{4,5,7},cutoff);},"window accepted outside shape");
        auto future=source.export_snapshot();future.revision="tensor/future";source=source.replace(future);
        rejects([&]{(void)NativeTensorWindow::prepare(source.payload(1),{0,0,0},{3,5,7},cutoff);},"window admitted a future publication");
        check(w.summarize(cutoff).source_publication()->sequence==0 && encode_interpreted_snapshot(decode_interpreted_snapshot(bytes))==bytes,"window or codec mutated source");
    }
    for (const auto& shape:std::vector<std::vector<std::size_t>>{{},{0,3}}) {
        numeric::IndexDomain domain(shape);auto source=tensor_source(numeric::SparseRationalTensor::zero(domain,numeric::StructuralSupport::full(domain.logical_size())));
        std::vector<std::size_t> lower(shape.size());const auto window=NativeTensorWindow::prepare(source.payload(1),lower,shape,available(source));
        const auto result=window.summarize(available(source));
        check(result.supported_count()==domain.logical_size() && (result.sum().has_value()==(domain.logical_size()!=0)),"scalar/zero-extent window changed support");
        auto host=runtime::ExecutionHostPolicy{};host.limits.integer_bits=0;
        if(domain.logical_size())rejects([&]{(void)window.summarize(available(source),runtime::ExecutionLease(host));},"supported zero bypassed denominator bit admission");
        auto cancel=std::make_shared<std::atomic_bool>(true);
        rejects([&]{(void)window.summarize(available(source),runtime::ExecutionLease({},cancel));},"tensor window ignored cancellation");
    }
    // A huge logical domain with two sparse stored coordinates never densifies.
    numeric::IndexDomain huge({1000000000,2});auto source=tensor_source({huge,numeric::StructuralSupport::sparse(huge.logical_size(),{0,1999999999}),numeric::SparseRationalVector(huge.logical_size(),{{0,Q{1}},{1999999999,Q{-1}}})});
    const auto window=NativeTensorWindow::prepare(source.payload(1),{0,0},{1000000000,2},available(source));
    const auto huge_summary=window.summarize(available(source));
    check(huge_summary.sum()==Q{0} && window.requirements().scalar_work<100,"sparse logical shape was densified");
    const auto escaped=[] {
        auto src=tensor_source({numeric::IndexDomain({1}),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{17,19}}})});
        return NativeTensorWindow::prepare(src.payload(1),{0},{1},available(src)).coordinate(0);
    }();
    check(escaped.nonzero_value() && *escaped.nonzero_value()==Q{17,19},"escaped coordinate outlived its sparse value owner");
    numeric::IndexDomain wide({4,1000});std::vector<numeric::SparseRationalEntry> dense_entries;
    for(std::size_t i=0;i<wide.logical_size();++i)dense_entries.push_back({i,Q{1}});
    auto wide_source=tensor_source({wide,numeric::StructuralSupport::full(wide.logical_size()),numeric::SparseRationalVector(wide.logical_size(),std::move(dense_entries))});
    const auto narrow=NativeTensorWindow::prepare(wide_source.payload(1),{0,999},{4,1000},available(wide_source));
    const auto scanned=NativeTensorWindow::prepare(wide_source.payload(1),{0,999},{4,1000},available(wide_source),runtime::ExecutionLease{},0);
    const auto narrow_sum=narrow.summarize(available(wide_source)),scanned_sum=scanned.summarize(available(wide_source));
    check(narrow.uses_contiguous_intervals() && narrow.interval_count()==4 && narrow_sum.visited_entries()==4,"axis intervals traversed excluded tensor entries");
    check(narrow_sum.sum()==Q{4} && scanned_sum.sum()==Q{4},"bounded interval fallback changed tensor contraction");
    check(scanned_sum.visited_entries()>narrow_sum.visited_entries(),"interval traversal did not remove bounding-box gaps");
}
void role_joins() {
    auto source=InterpretedSnapshot::interpret(f5_fixture::schema(),f5_fixture::snapshot());
    const auto cutoff=available(source);const auto bytes=encode_interpreted_snapshot(source);
    const auto join=PreparedNativeRoleJoin::prepare(source,{10,11,10,12,NativeRoleActivity::Retained},cutoff);
    const auto result=join.execute(cutoff);
    check(result.matches().size()==2,"role join merged repeated/inactive participants");
    check(result.matches()[0].left_position==0 && result.matches()[1].left_position==1 &&
        result.matches()[0].left_occurrence!=result.matches()[1].left_occurrence,"role join lost ordered occurrence identities");
    check(join.missing_fibers()==2 && join.null_fibers()==2,"role join conflated null and missing fibers");
    const auto active=PreparedNativeRoleJoin::prepare(source,{10,11,10,12,NativeRoleActivity::ActiveOnly},cutoff);
    const auto active_result=active.execute(cutoff);
    check(active_result.matches().size()==1,"active-only role join retained inactive witness");
    const auto self=PreparedNativeRoleJoin::prepare(source,{10,11,10,11,NativeRoleActivity::Retained},cutoff);
    const auto self_result=self.execute(cutoff);
    check(self_result.matches().size()==5,"self role join lost occurrence cross terms");
    rejects([&]{(void)PreparedNativeRoleJoin::prepare(source,{10,99,10,12},cutoff);},"join inferred undeclared role");
    auto host=runtime::ExecutionHostPolicy{};host.limits.records=1;
    rejects([&]{(void)self.execute(cutoff,runtime::ExecutionLease(host));},"role join bypassed output admission");
    auto changed=f5_fixture::snapshot();changed.revision="roles/later";changed.assertions[0].roles[0].occurrences.clear();
    source=source.replace(changed);
    const auto old=join.execute(cutoff);
    check(old.matches()==result.matches(),"role join followed later source");
    rejects([&]{(void)PreparedNativeRoleJoin::prepare(source,{10,11,10,12},cutoff);},"role join accepted a future source");
    check(encode_interpreted_snapshot(decode_interpreted_snapshot(bytes))==bytes,"role join mutated canonical source");
}
void work() {
    auto source=InterpretedSnapshot::interpret(schema(),input(129));const auto cutoff=available(source);
    const auto op=PreparedNativeQuery::prepare(source,10,request(),cutoff,runtime::ExecutionLease{},16);
    auto ts=tensor_source({numeric::IndexDomain({2,3}),numeric::StructuralSupport::sparse(6,{0,1,5}),numeric::SparseRationalVector(6,{{0,Q{3}},{5,Q{-3}}})});
    const auto window=NativeTensorWindow::prepare(ts.payload(1),{0,0},{2,3},available(ts));
    runtime::HardwareInventory hardware;hardware.affinity_observed=true;
    for(std::size_t i=0;i<4;++i)hardware.allowed_cpus.push_back({i,0,i,{}});
    runtime::CpuResourceCoordinator resources(hardware,{4,1<<24,1000});
    auto run=runtime::WorkGraph({make_query_work({1,1},op,2,cutoff),make_tensor_summary_work({2,1},window,available(ts)),make_query_work({3,1},op,2,cutoff,{{1,1}})}).submit(resources);
    run.wait();auto q=run.completion({1,1}).result<DerivedNativeQuery>();auto t=run.completion({2,1}).result<NativeTensorWindowSummary>();
    runtime::CpuTileExecutor executor;
    const auto reference=op.execute(cutoff,executor);
    check(q->keys()==reference.keys() && t->sum()==Q{0},"owned query/tensor graph changed results");
    check(run.completion({3,1}).result<DerivedNativeQuery>()->keys()==q->keys(),"readiness edge rebound query input");
    check(resources.available_workers()==4,"read graph leaked worker admission");
    // Detached key views must keep the numerical result's budget alive after
    // the graph, completion and result wrapper have all gone away.
    runtime::CpuResourceCoordinator owned(hardware,{4,1<<24,1000});
    std::shared_ptr<const std::vector<std::uint64_t>> escaped;
    std::size_t charged{};
    {
        auto graph=runtime::WorkGraph({make_query_work({8,1},op,2,cutoff)}).submit(owned);
        graph.wait();const auto value=graph.completion({8,1}).result<DerivedNativeQuery>();
        escaped=value->keys_pin();charged=value->retained_object_bytes();
        check(charged>=sizeof(DerivedNativeQuery)+sizeof(prepared_query_detail::Data),"result omitted its owning containers");
    }
    check(owned.available_workers()==4 && owned.available_declared_buffer_bytes()==(1<<24)-charged,"escaped keys dropped their resource charge");
    check(*escaped==reference.keys(),"escaped charged keys changed");
    escaped.reset();check(owned.available_declared_buffer_bytes()==1<<24,"escaped query owner leaked admission");
    std::optional<NativeTensorWindowSummary> retained;
    {
        auto graph=runtime::WorkGraph({make_tensor_summary_work({9,1},window,available(ts))}).submit(owned);
        graph.wait();retained=*graph.completion({9,1}).result<NativeTensorWindowSummary>();charged=retained->retained_object_bytes();
    }
    check(retained->sum()==Q{0} && owned.available_declared_buffer_bytes()==(1<<24)-charged,"copied tensor summary dropped its charge");
    retained.reset();check(owned.available_declared_buffer_bytes()==1<<24,"tensor summary owner leaked admission");
    runtime::CpuResourceCoordinator tiny(hardware,{4,op.execution_object_bytes()-1,1000});
    rejects([&]{(void)runtime::WorkGraph({make_query_work({10,1},op,2,cutoff)}).submit(tiny);},"query ignored limb/container admission");
    check(tiny.available_workers()==4 && tiny.available_declared_buffer_bytes()==op.execution_object_bytes()-1,"failed query admission leaked resources");
}
void oracle() {
    std::string tag;
    while (std::cin>>tag) {
        if (tag=="q") {
            std::size_t rows,workers,tile;unsigned output;bool sum_integer;std::cin>>rows>>workers>>tile>>output>>sum_integer;
            LogicalSnapshot in;in.revision="oracle";
            for (std::size_t i=0;i<rows;++i) {std::string value,integer;bool flag;std::uint64_t category;std::cin>>value>>flag>>category>>integer;
                in.assertions.push_back({{10,token(i)},payload(value=="null"?std::nullopt:std::optional<Q>{Q(value)},flag,category,numeric::Integer(integer)),{}});}
            auto source=InterpretedSnapshot::interpret(schema(),in);auto req=request(output?NativeQueryOutput::Aggregate:NativeQueryOutput::Keys);
            if (sum_integer) req.sum=std::vector<std::uint64_t>{21,14};
            auto op=PreparedNativeQuery::prepare(source,10,req,available(source),runtime::ExecutionLease{},tile,
                workers>1?NativeQueryIndex::NumericRanges:NativeQueryIndex::None);runtime::CpuTileExecutor executor({workers,4,10000});auto r=op.execute(available(source),executor);
            std::cout<<"q "<<r.selected_count()<<' '<<r.unknown_count()<<' '<<r.numeric_contributions()<<' '<<(r.sum()?r.sum()->get_str():"null");
            for(const auto key:r.keys())std::cout<<' '<<key;
            std::cout<<'\n';
        } else if (tag=="w") {
            unsigned encoding;std::size_t rank,n;std::cin>>encoding>>rank;std::vector<std::size_t> shape(rank),lo(rank),hi(rank);
            for(auto& x:shape)std::cin>>x;
            for(auto& x:lo)std::cin>>x;
            for(auto& x:hi)std::cin>>x;
            numeric::IndexDomain domain(shape);std::cin>>n;std::vector<std::size_t> active(n);for(auto& x:active)std::cin>>x;
            std::cin>>n;std::vector<numeric::SparseRationalEntry> entries(n);for(auto& x:entries){std::string q;std::cin>>x.index>>q;x.value=Q(q);}
            const auto support=encoding==0?numeric::StructuralSupport::full(domain.logical_size()):encoding==1?numeric::StructuralSupport::sparse(domain.logical_size(),active):numeric::StructuralSupport::packed(domain.logical_size(),active);
            auto source=tensor_source({domain,support,numeric::SparseRationalVector(domain.logical_size(),std::move(entries))});
            auto window=NativeTensorWindow::prepare(source.payload(1),lo,hi,available(source));auto s=window.summarize(available(source));
            std::cout<<"w "<<s.supported_count()<<' '<<s.nonzero_count()<<' '<<(s.sum()?s.sum()->get_str():"null");
            for(std::size_t i=0;i<window.domain().logical_size();++i){const auto c=window.coordinate(i);auto q=c.state();std::cout<<' '<<c.source_index()<<':'<<(q?q->get_str():"null");}
            std::cout<<'\n';
        } else throw std::invalid_argument("unknown oracle case");
    }
}
}
int main(int argc,char** argv) {
    if (argc==2 && std::string(argv[1])=="--oracle") {oracle();return 0;}
    queries();windows();role_joins();work();std::cout<<checks<<" native query/tensor checks passed\n";
}
