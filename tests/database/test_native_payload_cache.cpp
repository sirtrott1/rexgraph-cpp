#include "rfc/database/native_payload_cache.hpp"
#include <condition_variable>
#include <iostream>
#include <thread>
#include <chrono>
#include <source_location>

namespace s=rfc::storage;namespace d=rfc::database;
using Bytes=std::vector<std::byte>;
namespace {
std::size_t checks{};
void ck(bool value,std::source_location where=std::source_location::current()){
    ++checks;if(!value)throw std::runtime_error("native payload assertion failed at line "+std::to_string(where.line()));
}
template<class F>void refuses(F&& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}ck(failed);}
struct Fixture {
    std::vector<s::DeclaredRfcxPayload> payloads;
    s::PayloadManifest manifest;
    static rfc::core::RelationalComplex source(std::uint64_t key) {
        using namespace rfc;core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,4);
        core::RelationalComplexBuilder builder(sig);numeric::Rational q=1;
        if(key==2){numeric::Integer n=1;n<<=130;q=numeric::Rational(n+1,7);q.canonicalize();}
        const auto left=key==1?1:(key==2?3:2),right=key==1?2:(key==2?4:3);
        builder.add_grade_one(core::RelationKey{100+key},edge,{
            {static_cast<std::uint64_t>(left),true,true,q,{},core::OccurrenceKey{1000+key*10}},
            {static_cast<std::uint64_t>(left),false,false,q,{},core::OccurrenceKey{1001+key*10}},
            {static_cast<std::uint64_t>(right),true,false,q,{},core::OccurrenceKey{1002+key*10}}});
        return builder.freeze();
    }
    static std::vector<s::DeclaredRfcxPayload> records(bool cycle) {
        std::vector<s::DeclaredRfcxPayload> out;
        for(std::uint64_t k=1;k<=3;++k)out.push_back(s::declare_rfcx_payload(900,k,7,k,source(k)));
        if(cycle)for(std::size_t i=0;i<3;++i)out[i].descriptor.dependencies={out[(i+1)%3].descriptor.identity};
        return out;
    }
    static s::PayloadManifest declaration(const std::vector<s::DeclaredRfcxPayload>& records) {
        std::vector<s::PayloadSourceRevision> sources;std::vector<s::PayloadDescriptor> descriptors;
        for(const auto& p:records){sources.push_back(p.descriptor.identity.source);descriptors.push_back(p.descriptor);}
        return s::PayloadManifest::create(sources,descriptors);
    }
    explicit Fixture(bool cycle=false):payloads(records(cycle)),manifest(declaration(payloads)){}
    void save(const std::filesystem::path& path) const {
        std::vector<std::span<const std::byte>> values;for(const auto& p:payloads)values.push_back(p.bytes);
        s::write_payload_archive(path,manifest,values);
    }
    s::PayloadKey key(std::size_t i)const{return payloads[i].descriptor.identity;}
};
void clean(const d::NativePayloadCache& cache) {
    const auto u=cache.usage();const auto raw=cache.raw_usage();
    ck(u.loading==0 && u.pending_decoded_bytes==0 && u.pending_coefficient_bits==0 && u.scratch_bytes==0 && u.selection_bytes==0);
    ck(raw.loading==0 && raw.loading_payload_bytes==0);
}
void raw_leases(const Fixture& f) {
    d::PayloadResidency raw(f.manifest);auto ticket=raw.begin(f.key(0),f.key(0).source);raw.commit(ticket,f.payloads[0].bytes);
    {
        auto lease=raw.pin(f.key(0));auto copy=lease;
        ck(std::ranges::equal(copy.bytes(),f.payloads[0].bytes));ck(!raw.evict(f.key(0)));
        ck(raw.usage().resident_payload_bytes==f.payloads[0].bytes.size());
    }
    ck(raw.evict(f.key(0)));ck(raw.usage().resident_payload_bytes==0);
    ck(raw.status(f.key(0)).state==d::PayloadResidencyState::Unloaded);
}
void cold_and_pinned(const Fixture& f,const std::filesystem::path& path) {
    auto provider=s::PayloadArchiveProvider::open(path,f.manifest);
    ck(provider.usage().payload_io_bytes==0);
    ck(provider.usage().metadata_io_bytes==40+f.manifest.bytes().size()+16*3);
    d::NativePayloadCache cache(provider);
    ck(cache.status(f.key(0)).state==d::PayloadResidencyState::Unloaded);
    auto stale=f.key(0).source;++stale.revision;refuses([&]{(void)cache.acquire(f.key(0),stale);});
    ck(cache.archive_usage().payload_io_bytes==0);
    std::optional<d::NativePayloadCache::Lease> escaped;
    std::uint64_t first_generation{};
    {
        auto selected=cache.acquire(f.key(0),f.key(0).source);
        ck(selected.members().size()==1);ck(std::ranges::equal(selected.root().bytes(),f.payloads[0].bytes));
        ck(selected.root().source().boundary_apply({1},{1})==Fixture::source(1).boundary_apply({1},{1}));
        auto hit=cache.acquire(f.key(0),f.key(0).source);ck(selected.root().same_owner(hit.root()));
        d::NativePayloadCache independent(provider);auto other=independent.acquire(f.key(0),f.key(0).source);
        ck(!selected.root().same_owner(other.root()));ck(selected.root().key()==other.root().key());
        first_generation=selected.root().generation();escaped.emplace(selected.root());
        ck(!cache.evict(f.key(0)));ck(cache.usage().resident==1);
        ck(cache.status(f.key(1)).state==d::PayloadResidencyState::Unloaded);
        ck(cache.archive_usage().payload_io_bytes==2*f.payloads[0].bytes.size()); // shared provider observes both caches
        ck(selected.root().footprint().decoded_bytes<=f.payloads[0].descriptor.decoded_bytes);
        ck(selected.root().footprint().coefficient_bits<=f.payloads[0].descriptor.coefficient_bits);
    }
    ck(!cache.evict(f.key(0)));ck(cache.usage().selection_bytes==0);escaped.reset();
    ck(cache.evict(f.key(0)));ck(cache.usage().resident_decoded_bytes==0 && cache.raw_usage().resident_payload_bytes==0);
    {
        auto reloaded=cache.acquire(f.key(0),f.key(0).source);ck(reloaded.root().generation()>first_generation);
        ck(std::ranges::equal(reloaded.root().bytes(),f.payloads[0].bytes));
        const auto& source=reloaded.root().source();ck(source.occurrence_identity({1},1).value==1011);
        ck(!source.grade({1}).aspects.get(source.signature().existence_aspect(),1));
    }
    clean(cache);
    // A lease owns the cache context even after the creating facade is gone.
    std::optional<d::NativePayloadCache::Selection> survivor;
    {d::NativePayloadCache temporary(provider);survivor.emplace(temporary.acquire(f.key(1),f.key(1).source));}
    ck(survivor->root().source().boundary_apply({1},{1})==Fixture::source(2).boundary_apply({1},{1}));survivor.reset();
}
void limits_and_orders(const Fixture& f,const std::filesystem::path& path) {
    auto provider=s::PayloadArchiveProvider::open(path,f.manifest);
    const auto budget=[&](d::NativePayloadLimits limits,std::size_t index=0) {
        d::NativePayloadCache cache(provider,limits);refuses([&]{(void)cache.acquire(f.key(index),f.key(index).source);});clean(cache);
        ck(cache.usage().resident==0);
    };
    d::NativePayloadLimits l;l.raw.max_payload_bytes=f.payloads[0].bytes.size()-1;budget(l);
    l={};l.max_decoded_bytes=1;budget(l);l={};l.max_coefficient_bits=1;budget(l);
    l={};l.max_scratch_bytes=1;budget(l);l={};l.max_io_bytes=0;budget(l);l={};l.max_entries=0;budget(l);
    l={};l.max_selection_records=0;budget(l);l={};l.preparation.max_integer_bits=64;budget(l,1);
    {d::NativePayloadCache original(provider);l={};l.max_metadata_bytes=original.usage().metadata_bytes;budget(l);}
    l={};l.max_entries=1;d::NativePayloadCache bounded(provider,l);
    {
        auto pinned=bounded.acquire(f.key(0),f.key(0).source);refuses([&]{(void)bounded.acquire(f.key(1),f.key(1).source);});
        ck(!bounded.evict(f.key(0)));ck(pinned.root().source().grade({1}).cell_count()==1);
    }
    {auto next=bounded.acquire(f.key(1),f.key(1).source);ck(bounded.usage().evictions==1);ck(next.root().key()==f.key(1));}
    ck(bounded.status(f.key(0)).state==d::PayloadResidencyState::Unloaded);clean(bounded);
    std::array<std::size_t,3> order{0,1,2};unsigned orders{};
    do {
        d::NativePayloadCache cache(provider);std::vector<d::NativePayloadCache::Selection> retained;
        for(auto i:order) {
            retained.push_back(cache.acquire(f.key(i),f.key(i).source));
            ck(retained.back().root().source().boundary_apply({1},{1})==Fixture::source(i+1).boundary_apply({1},{1}));
        }
        ++orders;
    }while(std::next_permutation(order.begin(),order.end()));ck(orders==6);
}
void cycles(const std::filesystem::path& root) {
    Fixture f(true);const auto path=root/"cycle.archive";f.save(path);
    auto provider=s::PayloadArchiveProvider::open(path,f.manifest);d::NativePayloadCache cache(provider);
    {auto all=cache.acquire(f.key(0),f.key(0).source);ck(all.members().size()==3);ck(cache.usage().loads==3);
        for(std::size_t i=0;i<3;++i)ck(all.at(f.key(i)).source().boundary_apply({1},{1})==Fixture::source(i+1).boundary_apply({1},{1}));}
    std::size_t expected{};for(const auto& p:f.payloads)expected+=p.bytes.size();ck(cache.archive_usage().payload_io_bytes==expected);clean(cache);
    d::NativePayloadLimits limits;limits.max_entries=2;d::NativePayloadCache small(provider,limits);
    refuses([&]{(void)small.acquire(f.key(0),f.key(0).source);});clean(small);ck(small.usage().resident==2);
}
void unrelated_payloads(const Fixture& f,const std::filesystem::path& root) {
    auto records=f.payloads;
    for(std::uint64_t i=4;i<=36;++i)records.push_back(s::declare_rfcx_payload(900,i,7,i,Fixture::source(i)));
    const auto expanded=Fixture::declaration(records);std::vector<std::span<const std::byte>> values;
    for(const auto& p:records)values.push_back(p.bytes);
    const auto large=root/"unrelated.archive";s::write_payload_archive(large,expanded,values);
    auto original_provider=s::PayloadArchiveProvider::open(root/"source.archive",f.manifest);
    auto expanded_provider=s::PayloadArchiveProvider::open(large,expanded);
    d::NativePayloadLimits limits;limits.raw.max_payload_bytes=f.payloads[0].bytes.size();limits.max_entries=1;
    d::NativePayloadCache original(original_provider,limits),extra(expanded_provider,limits);
    auto a=original.acquire(f.key(0),f.key(0).source),b=extra.acquire(f.key(0),f.key(0).source);
    ck(extra.usage().metadata_bytes>original.usage().metadata_bytes);
    ck(extra.archive_usage().payload_io_bytes==original.archive_usage().payload_io_bytes);
    ck(extra.raw_usage().resident_payload_bytes==original.raw_usage().resident_payload_bytes);
    ck(extra.usage().resident_decoded_bytes==original.usage().resident_decoded_bytes);
    ck(extra.usage().peak_scratch_bytes==original.usage().peak_scratch_bytes);
    ck(b.root().source().boundary_apply({1},{1})==a.root().source().boundary_apply({1},{1}));
    ck(extra.status(records.back().descriptor.identity).state==d::PayloadResidencyState::Unloaded);
}
void higher_source_and_canonical_profile(const Fixture& f,const std::filesystem::path& root) {
    using namespace rfc;core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_type("face",core::BoundaryPolicy::DeclaredExact,3,3);core::RelationalComplexBuilder builder(sig);
    for(std::uint64_t i=0;i<3;++i)builder.add_grade_one(core::RelationKey{10+i},edge,{
        {i+1,true,true,numeric::Rational{1},{},core::OccurrenceKey{100+i*2}},
        {(i+1)%3+1,true,false,numeric::Rational{1},{},core::OccurrenceKey{101+i*2}}});
    builder.add_higher({2},core::RelationKey{20},face,{
        {0,true,false,numeric::Rational{1},{},core::OccurrenceKey{200}},
        {1,true,false,numeric::Rational{1},{},core::OccurrenceKey{201}},
        {2,true,false,numeric::Rational{1},{},core::OccurrenceKey{202}}});
    const auto source=builder.freeze();auto p=s::declare_rfcx_payload(901,1,1,1,source);
    auto manifest=s::PayloadManifest::create({p.descriptor.identity.source},{p.descriptor});
    const std::array<std::span<const std::byte>,1> parts{p.bytes};const auto path=root/"higher.archive";
    s::write_payload_archive(path,manifest,parts);auto provider=s::PayloadArchiveProvider::open(path,manifest);d::NativePayloadCache cache(provider);
    auto selected=cache.acquire(p.descriptor.identity,p.descriptor.identity.source);
    ck(selected.root().source().boundary_apply({1},selected.root().source().boundary_apply({2},{1}))==numeric::RationalVector({0,0,0}));
    ck(std::ranges::equal(selected.root().bytes(),p.bytes));
    // The established legacy reader keeps its old normalization path. Demand
    // preparation admits canonical retained lanes and never enters that fallback.
    auto raw=f.payloads[0].bytes;s::rfcx_detail::Reader header(raw,56);const auto directory=header.size_value();
    const auto g=s::rfcx_detail::read_grade_descriptor(raw,directory+s::rfcx_detail::grade_descriptor_size);
    const auto write=[&](std::size_t at,std::uint64_t value){s::rfcx_detail::Writer w;w.u64(value);std::copy(w.bytes().begin(),w.bytes().end(),raw.begin()+static_cast<std::ptrdiff_t>(at));};
    write(static_cast<std::size_t>(g.cell_scales),2);
    s::rfcx_detail::Reader coefficients(raw,static_cast<std::size_t>(g.lattice));
    for(std::size_t i=0;i<3;++i)write(static_cast<std::size_t>(g.lattice)+i*8,static_cast<std::uint64_t>(coefficients.i64()*2));
    write(72,s::rfcx_detail::fnv1a64(std::span<const std::byte>(raw).subspan(s::rfcx_detail::header_size)));
    s::MappedRfcx legacy(raw);ck(legacy.materialize().boundary_apply({1},{1})==Fixture::source(1).boundary_apply({1},{1}));
    refuses([&]{(void)legacy.materialize_canonical();});
}
void cancellation(const Fixture& f,const std::filesystem::path& path) {
    auto provider=s::PayloadArchiveProvider::open(path,f.manifest);d::NativePayloadCache cache(provider);
    bool invalidated=false;
    refuses([&]{(void)cache.acquire(f.key(0),f.key(0).source,[&]{
        if(!invalidated && cache.status(f.key(0)).state==d::PayloadResidencyState::Loading)invalidated=cache.cancel(f.key(0));
    });});
    ck(invalidated && cache.status(f.key(0)).failure==d::NativePayloadFailure::Stale);clean(cache);
    auto selected=cache.acquire(f.key(0),f.key(0).source);ck(selected.root().generation()==3);
    d::NativePayloadCache during_prepare(provider);bool cancelled=false;
    refuses([&]{(void)during_prepare.acquire(f.key(1),f.key(1).source,[&]{
        if(during_prepare.raw_usage().resident) {cancelled=true;ck(during_prepare.usage().scratch_bytes>0);throw rfc::runtime::ExecutionCancelled();}
    });});
    ck(cancelled && during_prepare.status(f.key(1)).failure==d::NativePayloadFailure::Cancelled);clean(during_prepare);
    ck(during_prepare.raw_usage().resident_payload_bytes==0);
    // A parallel borrower gets an explicit busy refusal, not a partial image.
    d::NativePayloadCache parallel(provider);std::mutex mutex;std::condition_variable changed;bool entered=false,release=false;std::exception_ptr error;
    std::thread worker([&]{try{(void)parallel.acquire(f.key(2),f.key(2).source,[&]{
        if(parallel.status(f.key(2)).state==d::PayloadResidencyState::Loading) {
            std::unique_lock lock(mutex);if(!entered){entered=true;changed.notify_all();changed.wait(lock,[&]{return release;});}
        }
    });}catch(...){error=std::current_exception();}});
    {std::unique_lock lock(mutex);changed.wait(lock,[&]{return entered;});}
    refuses([&]{(void)parallel.acquire(f.key(2),f.key(2).source);});ck(!parallel.evict(f.key(2)));ck(parallel.cancel(f.key(2)));
    {std::lock_guard lock(mutex);release=true;}changed.notify_all();worker.join();ck(error!=nullptr);clean(parallel);
    {auto retry=parallel.acquire(f.key(2),f.key(2).source);ck(retry.root().generation()==3);}
}
void bad_inputs(const Fixture& f,const std::filesystem::path& root) {
    const auto altered=[&](std::string name,auto change) {
        auto records=f.payloads;change(records[0].descriptor);auto manifest=Fixture::declaration(records);
        const auto path=root/(name+".archive");std::vector<std::span<const std::byte>> images;for(const auto& p:records)images.push_back(p.bytes);
        s::write_payload_archive(path,manifest,images);auto provider=s::PayloadArchiveProvider::open(path,manifest);d::NativePayloadCache cache(provider);
        refuses([&]{(void)cache.acquire(records[0].descriptor.identity,records[0].descriptor.identity.source);});clean(cache);ck(cache.usage().resident==0);
    };
    altered("under-decoded",[](auto& p){p.decoded_bytes=1;});altered("under-bits",[](auto& p){p.coefficient_bits=1;});
    altered("wrong-identity",[](auto& p){p.occurrences[1].exists=true;});
    altered("unsupported",[](auto& p){p.format="unknown-native-profile";});
    const auto path=root/"bad-index.archive";f.save(path);auto bytes=s::read_file_exact(path,1u<<20);
    const auto index=40+f.manifest.bytes().size();bytes[index]^=std::byte{1};s::write_file_exact(path,bytes);
    refuses([&]{(void)s::PayloadArchiveProvider::open(path,f.manifest);});
    const auto corrupt=root/"corrupt.archive";f.save(corrupt);auto raw=s::read_file_exact(corrupt,1u<<20);raw[index+3*16]^=std::byte{1};s::write_file_exact(corrupt,raw);
    auto corrupt_provider=s::PayloadArchiveProvider::open(corrupt,f.manifest);d::NativePayloadCache corrupt_cache(corrupt_provider);
    refuses([&]{(void)corrupt_cache.acquire(f.key(0),f.key(0).source);});ck(corrupt_cache.status(f.key(0)).failure==d::NativePayloadFailure::Integrity);clean(corrupt_cache);
    const auto truncated=root/"truncated.archive";f.save(truncated);auto opened=s::PayloadArchiveProvider::open(truncated,f.manifest);
    std::filesystem::resize_file(truncated,index+3*16+3);d::NativePayloadCache io_failure(opened);
    refuses([&]{(void)io_failure.acquire(f.key(0),f.key(0).source);});ck(io_failure.status(f.key(0)).failure==d::NativePayloadFailure::Provider);clean(io_failure);
    // Opened providers keep the selected inode through atomic path replacement.
    const auto replaced=root/"replaced.archive";f.save(replaced);auto pinned=s::PayloadArchiveProvider::open(replaced,f.manifest);
    s::write_file_exact(replaced,Bytes{std::byte{7}});ck(pinned.read(f.key(0),1u<<20)==f.payloads[0].bytes);
    refuses([&]{(void)s::PayloadArchiveProvider::open(replaced,f.manifest);});
    auto source=f.manifest.sources();source[0].revision++;
    refuses([&]{(void)s::PayloadManifest::decode(f.manifest.bytes(),source);});
}
}
int main(int argc,char** argv) {try {
    const auto root=argc==3 && std::string_view(argv[1])=="--emit"?std::filesystem::path(argv[2]):
        std::filesystem::temp_directory_path()/("rfc_native_payload_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);Fixture f;const auto archive=root/"source.archive";f.save(archive);
    if(argc==3) {
        s::write_file_exact(root/"manifest.bin",f.manifest.bytes());std::ofstream expected(root/"observations.tsv");
        auto provider=s::PayloadArchiveProvider::open(archive,f.manifest);d::NativePayloadLimits limits;limits.max_entries=1;d::NativePayloadCache cache(provider,limits);
        for(const auto i:std::array<std::size_t,6>{0,1,0,2,1,2}) {
            auto selected=cache.acquire(f.key(i),f.key(i).source);expected<<i+1<<'\t';
            for(const auto& q:selected.root().source().boundary_apply({1},{1}))expected<<q<<',';
            expected<<'\t'<<selected.root().generation()<<'\n';
        }
        return 0;
    }
    raw_leases(f);cold_and_pinned(f,archive);limits_and_orders(f,archive);cycles(root);unrelated_payloads(f,root);higher_source_and_canonical_profile(f,root);cancellation(f,archive);bad_inputs(f,root);
    std::cout<<checks<<" native payload cache/archive checks passed\n";std::filesystem::remove_all(root);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
