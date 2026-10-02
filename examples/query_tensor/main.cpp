#include "sample.hpp"
#include <chrono>
#include <iostream>

namespace {
using namespace query_tensor_sample;
using Clock=std::chrono::steady_clock;
std::uint64_t ns(Clock::time_point first,Clock::time_point last) {return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(last-first).count());}
struct Answer {std::vector<std::uint64_t> keys;std::size_t selected{},unknown{},numeric_count{},payload_resolutions{};std::optional<Q> sum;};
// Reference uses the original typed accessor/evaluator at each row, with the
// same pure ordered conjunction and requested output as the prepared path.
Answer reference(const InterpretedSnapshot& source,const NativeQueryRequest& request) {
    std::vector<native_query_detail::Scalar> scalars;
    for(const auto& p:request.all_of) {auto s=native_query_detail::scalar(source.schema(),10,p.factors);native_query_detail::validate(source.schema(),s,p);scalars.push_back(s);}
    Answer out;const auto& keys=source.assertions(10);
    for (const auto key:keys) {
        auto truth=NativeTruth::True;
        for(std::size_t i=0;i<scalars.size();++i) {
            const auto& p=request.all_of[i];const auto value=source.payload(key,p.factors).declared_value();++out.payload_resolutions;
            const auto t=native_query_detail::evaluate(scalars[i],p,*value);
            if(t==NativeTruth::False) {truth=t;break;}
            if(t==NativeTruth::Unknown) truth=t;
        }
        if(truth==NativeTruth::Unknown) ++out.unknown;
        if(truth!=NativeTruth::True) continue;
        ++out.selected;if(request.output==NativeQueryOutput::Keys) out.keys.push_back(key);
        if(request.sum) {
            const auto value=source.payload(key,*request.sum).declared_value();++out.payload_resolutions;
            const auto& option=std::get<V::Optional>(value->data);
            if(option.element.empty())continue;
            if(!out.sum)out.sum=Q{0};
            *out.sum+=std::get<Q>(option.element.front().data);++out.numeric_count;
        }
    }
    return out;
}
struct TensorAnswer {std::size_t supported{},nonzeros{};std::optional<Q> sum;};
TensorAnswer tensor_reference(const storage::PinnedNativeAccession& pin,
    const std::vector<std::size_t>& lower,const std::vector<std::size_t>& upper) {
    const auto tensor=pin.tensor();std::vector<std::size_t> extents;
    for(std::size_t i=0;i<lower.size();++i)extents.push_back(upper[i]-lower[i]);
    const numeric::IndexDomain domain(std::move(extents));TensorAnswer out;
    for(std::size_t i=0;i<domain.logical_size();++i) {
        std::size_t source_index=0;
        for(std::size_t axis=0;axis<domain.rank();++axis)
            source_index+=((i/domain.strides()[axis])%domain.extents()[axis]+lower[axis])*tensor->domain().strides()[axis];
        const auto q=tensor->state(source_index);
        if(q) {if(!out.sum)out.sum=Q{0};*out.sum+=*q;++out.supported;if(*q!=0)++out.nonzeros;}
    }
    return out;
}
void times(const std::vector<std::uint64_t>& samples) {
    std::cout<<",\"samples_ns\":[";
    for(std::size_t i=0;i<samples.size();++i){if(i)std::cout<<',';std::cout<<samples[i];}
    auto warm=std::vector<std::uint64_t>(samples.begin()+1,samples.end());std::sort(warm.begin(),warm.end());
    std::cout<<"],\"cold_ns\":"<<samples.front()<<",\"warm_median_ns\":"<<warm[warm.size()/2];
}
void queries(runtime::CpuResourceCoordinator& resources) {
    for(const std::size_t n:{64u,1024u,4096u}) {
        auto start=Clock::now();auto source=InterpretedSnapshot::interpret(schema(),input(n));auto source_ns=ns(start,Clock::now());
        const auto cutoff=available(source);
        for(const auto output:{NativeQueryOutput::Keys,NativeQueryOutput::Aggregate}) {
            const auto req=request(output);const auto expected=reference(source,req);
            start=Clock::now();const auto plan=PreparedNativeQuery::prepare(source,10,req,cutoff,runtime::ExecutionLease{},128);
            const auto prepare_ns=ns(start,Clock::now());
            for(const std::size_t workers:{0u,1u,2u,4u}) {
                if(workers>resources.capacity())continue;
                runtime::CpuTileExecutor executor({std::max(std::size_t{1},workers),resources.capacity(),10000});
                std::vector<std::uint64_t> samples;std::uint64_t total=workers?prepare_ns:0;
                runtime::CpuTileRunReport cpu{1,1,0,1,1};
                for(unsigned repeat=0;repeat<7;++repeat) {
                    start=Clock::now();
                    auto reservation=resources.reserve(std::max(std::size_t{1},workers));
                    if(workers) {
                        const auto result=plan.execute(cutoff,executor);
                        cpu=result.cpu_report();
                        if(result.keys()!=expected.keys || result.selected_count()!=expected.selected || result.unknown_count()!=expected.unknown || result.sum()!=expected.sum || result.numeric_contributions()!=expected.numeric_count)
                            throw std::runtime_error("prepared query differs from complete reference");
                    } else {
                        const auto result=reference(source,req);
                        if(result.keys!=expected.keys || result.sum!=expected.sum)throw std::runtime_error("reference query changed");
                    }
                    samples.push_back(ns(start,Clock::now()));total+=samples.back();
                }
                std::cout<<"{\"phase\":\"query\",\"rows\":"<<n<<",\"output\":\""<<(output==NativeQueryOutput::Keys?"keys-and-sum":"aggregate")
                         <<"\",\"mode\":\""<<(workers?"prepared":"reference")<<"\",\"requested_workers\":"<<std::max(std::size_t{1},workers)
                         <<",\"worker_slots\":"<<cpu.worker_slots<<",\"tiles\":"<<cpu.tiles<<",\"launched_threads\":"<<cpu.launched_threads
                         <<",\"source_prepare_ns\":"<<source_ns<<",\"plan_prepare_ns\":"<<(workers?prepare_ns:0)
                         <<",\"seven_uses_plus_plan_ns\":"<<total<<",\"selected\":"<<expected.selected<<",\"unknown\":"<<expected.unknown
                         <<",\"sum\":"<<std::quoted(expected.sum?expected.sum->get_str():"null")
                         <<",\"prepared_payload_resolutions\":"<<(workers?plan.payload_resolutions():0)
                         <<",\"execution_payload_resolutions\":"<<(workers?0:expected.payload_resolutions)<<",\"projection_count\":"<<(workers?plan.projection_count():0);
                times(samples);std::cout<<"}\n";
            }
        }
    }
}
void tensors_and_work(runtime::CpuResourceCoordinator& resources) {
    numeric::IndexDomain domain({256,257});std::vector<std::size_t> active;std::vector<numeric::SparseRationalEntry> entries;
    for(std::size_t i=0;i<domain.logical_size();i+=7) {
        active.push_back(i);
        if(i%3)entries.push_back({i,Q{static_cast<long>(i%19)-9,23}});
    }
    auto start=Clock::now();auto source=tensor_source({domain,numeric::StructuralSupport::sparse(domain.logical_size(),std::move(active)),numeric::SparseRationalVector(domain.logical_size(),std::move(entries))});
    const auto source_ns=ns(start,Clock::now());start=Clock::now();
    const auto pin=source.payload(1);const std::vector<std::size_t> lower{32,17},upper{224,241};
    const auto window=NativeTensorWindow::prepare(pin,lower,upper,available(source));const auto prepare_ns=ns(start,Clock::now());
    const auto expected=tensor_reference(pin,lower,upper);
    for(const bool prepared:{false,true}) {
        std::vector<std::uint64_t> samples;std::uint64_t total=prepared?prepare_ns:0;
        for(unsigned repeat=0;repeat<7;++repeat) {
            start=Clock::now();auto reservation=resources.reserve(1);
            if(prepared) {
                const auto result=window.summarize(available(source));
                if(result.sum()!=expected.sum || result.supported_count()!=expected.supported || result.nonzero_count()!=expected.nonzeros)throw std::runtime_error("sparse summary mismatch");
            } else {
                const auto result=tensor_reference(pin,lower,upper);
                if(result.sum!=expected.sum || result.supported!=expected.supported || result.nonzeros!=expected.nonzeros)throw std::runtime_error("tensor reference changed");
            }
            samples.push_back(ns(start,Clock::now()));total+=samples.back();
        }
        std::cout<<"{\"phase\":\"tensor\",\"mode\":\""<<(prepared?"sparse-window":"coordinate-reference")<<"\",\"logical_coordinates\":"<<window.domain().logical_size()
                 <<",\"supported\":"<<window.supported_count()<<",\"nonzeros\":"<<window.nonzero_count()<<",\"source_prepare_ns\":"<<source_ns
                 <<",\"plan_prepare_ns\":"<<(prepared?prepare_ns:0)<<",\"seven_uses_plus_plan_ns\":"<<total<<",\"sum\":"<<std::quoted(expected.sum?expected.sum->get_str():"null");
        times(samples);std::cout<<"}\n";
    }
    auto query_source=InterpretedSnapshot::interpret(schema(),input(1024));
    const auto query=PreparedNativeQuery::prepare(query_source,10,request(),available(query_source));
    const auto workers=std::min(std::size_t{2},resources.capacity());
    auto run=runtime::WorkGraph({make_query_work({1,1},query,workers,available(query_source)),make_tensor_summary_work({2,1},window,available(source)),
        make_query_work({3,1},query,workers,available(query_source),{{1,1}})}).submit(resources);
    run.wait();const auto q=run.completion({1,1}).result<DerivedNativeQuery>();const auto t=run.completion({2,1}).result<NativeTensorWindowSummary>();
    if(t->sum()!=expected.sum || run.completion({3,1}).result<DerivedNativeQuery>()->keys()!=q->keys())throw std::runtime_error("query/tensor work graph changed results");
    const auto peak=run.peak_jobs_workers();
    std::cout<<"{\"phase\":\"owned_jobs\",\"jobs\":3,\"peak_jobs\":"<<peak.first<<",\"peak_workers\":"<<peak.second<<",\"query_selected\":"<<q->selected_count()
             <<",\"source_sequence\":"<<q->source_publication()->sequence<<",\"published\":false}\n";
}
}
int main() {
    const auto hardware=runtime::detect_hardware();runtime::CpuResourceCoordinator resources(hardware,{4,1<<26,10000});
    std::cout<<"{\"phase\":\"inventory\",\"allowed_logical_threads\":"<<hardware.allowed_cpus.size()<<",\"admitted_workers\":"<<resources.capacity()<<"}\n";
    queries(resources);tensors_and_work(resources);
}
