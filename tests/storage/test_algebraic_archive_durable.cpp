#include "../../examples/algebraic_storage/model.hpp"
#include "rfc/storage/algebraic_archive_durable.hpp"
#include <iostream>

using namespace archive_example;using namespace rfc::storage;
static std::size_t checks{};
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {
    bool failed=false;try {action();} catch (const std::exception&) {failed=true;}check(failed,message);
}
struct Temporary {
    std::filesystem::path path;
    Temporary() {auto pattern=(std::filesystem::temp_directory_path()/"rfc-algebraic-store-XXXXXX").string();
        const auto* result=::mkdtemp(pattern.data());if (!result) throw std::runtime_error("test temporary directory");path=result;}
    ~Temporary() {std::error_code error;std::filesystem::remove_all(path,error);}
};
static auto objects(const std::filesystem::path& path) {
    std::map<std::string,std::vector<std::byte>> out;
    for (const auto& file:std::filesystem::directory_iterator(path/"objects"))
        out.emplace(file.path().filename().string(),read_file_exact(file.path(),32u<<20));
    return out;
}
static void durable(const std::filesystem::path& path) {
    const auto sample=cases().front();const auto base=make(sample),next=make(sample,false,Q{7}),last=make(sample,false,Q{99});
    const AlgebraicArchiveChannel channel{701,1,100};const std::vector<Archive> first{next},second{last};
    DurableNativeRoot origin,committed;
    {
        auto store=AlgebraicDurableHistory::create(path,base,channel,9001,40);origin=store.root();const auto before=objects(path);
        const auto pinned=store.snapshot();const auto receipt=store.append(1,origin,first);committed=receipt.root;
        check(committed.sequence==41 && receipt.frame_count==1,"archive publication sequence differs");
        check(encode_algebraic_archive(store.at(40))==encode_algebraic_archive(base),"later value leaked into selected prefix");
        check(encode_algebraic_archive(store.at(41))==encode_algebraic_archive(next),"latest archive differs");
        rejects([&]{(void)pinned.at(41);},"old immutable snapshot sees future sequence");
        for (const auto& [name,bytes]:before) check(objects(path).at(name)==bytes,"previous durable object rewritten");
        const auto after=objects(path);check(store.append(1,origin,first).root==committed && objects(path)==after,"identical retry changed objects");
        rejects([&]{(void)store.append(1,origin,second);},"archive transaction reused with different bytes");
        rejects([&]{(void)store.append(2,origin,second);},"stale archive predecessor accepted");
        check(store.root()==committed && objects(path)==after,"failed archive publication changed durable root");
        bool unknown=false;
        try {(void)store.append(2,committed,second,[](DurableEvent event) {
            if (event.stage==DurableStage::HeadReplaced) throw std::runtime_error("publication cut");
        });} catch (const DurableUnknownOutcome&) {unknown=true;}
        check(unknown,"publication cut did not report unknown outcome");
        rejects([&]{(void)store.root();},"uncertain coordinator remained usable");
    }
    auto recovered=AlgebraicDurableHistory::open(path,channel);
    check(recovered.root().sequence==42 && recovered.status(2).has_value(),"archive publication not recovered");
    check(encode_algebraic_archive(recovered.at(40))==encode_algebraic_archive(base),"recovered prefix uses future data");
    check(encode_algebraic_archive(recovered.at(41))==encode_algebraic_archive(next),"middle archive lost");
    check(encode_algebraic_archive(recovered.at(42))==encode_algebraic_archive(last),"recovered tail differs");
    const auto before=objects(path);check(recovered.append(2,committed,second).root==recovered.root() && objects(path)==before,"recovered retry applied twice");
    rejects([&]{(void)recovered.at(43);},"future archive accepted");
    const auto endpoint=recovered.snapshot().at(40);
    rejects([&]{(void)read_algebraic_archive(endpoint.endpoint(),{702,1,100});},"foreign channel namespace accepted");
    rejects([&]{(void)read_algebraic_archive(endpoint.endpoint(),{701,2,100});},"foreign channel type accepted");
    AlgebraicArchiveLimits limit;limit.max_file_bytes=64;
    rejects([&]{(void)read_algebraic_archive(endpoint.endpoint(),channel,limit);},"opaque channel expands before byte admission");
}
static void grouped(const std::filesystem::path& path) {
    const auto sample=cases()[1];const auto base=make(sample);std::vector<Archive> revisions{make(sample,false,Q{1}),make(sample,false,Q{2})};
    const AlgebraicArchiveChannel channel{701,1,100};DurableNativeRoot final;
    {auto store=AlgebraicDurableHistory::create(path,base,channel,9002);const auto receipt=store.append(5,store.root(),revisions,[&](DurableEvent){revisions.clear();});
        final=receipt.root;check(receipt.frame_count==2 && final.sequence==2,"caller mutation split archive batch");}
    auto store=AlgebraicDurableHistory::open(path,channel);check(store.root()==final && store.snapshot().segment_count()==2,"grouped archive recovery lost frames");
}
static void publication_and_invalid_payload(const std::filesystem::path& directory) {
    const auto sample=cases().front();const auto archive=make(sample);const auto path=directory/"test.rfcxa";
    write_algebraic_archive(archive,path,FilePublication::CreateNew);
    check(encode_algebraic_archive(read_algebraic_archive(path))==encode_algebraic_archive(archive),"single file publication differs");
    rejects([&]{write_algebraic_archive(archive,path,FilePublication::CreateNew);},"create-new replaced archive");
    const auto before=read_file_exact(path,16u<<20);AlgebraicArchiveLimits tiny;tiny.max_file_bytes=32;
    rejects([&]{write_algebraic_archive(archive,path,FilePublication::ReplaceRegular,tiny);},"invalid replacement published");
    check(read_file_exact(path,16u<<20)==before,"failed replacement changed archive bytes");
    const AlgebraicArchiveChannel channel{701,1,100};auto endpoint=with_algebraic_archive(TemporalEndpoint(archive.bundle().source()),channel,archive);
    auto bytes=encode_algebraic_archive(archive);bytes.back()^=std::byte{1};
    endpoint.declarations->values.front().value=algebraic_archive_history_detail::chunks(bytes);
    // The generic store deliberately accepts declared opaque values. The typed
    // adapter must reject their false archive meaning during recovery.
    {auto raw=DurableNativeStore::create(directory/"invalid-opaque",endpoint,912);(void)raw.root();}
    rejects([&]{(void)AlgebraicDurableHistory::open(directory/"invalid-opaque",channel);},"opaque bytes accepted as valid algebraic archive");
    rejects([&]{(void)AlgebraicDurableHistory::create(directory/"not-created",archive,channel,913,0,tiny);},"invalid genesis admitted");
    check(!std::filesystem::exists(directory/"not-created"),"genesis validation created root before admission");
}
static void large_chunks(const std::filesystem::path& path) {
    const auto original=make(cases().front());auto contents=original.contents();const auto vector=std::get<rfc::local::AlgebraicModuleVector>(original.at(10));
    for (std::uint64_t i=0;i<2000;++i) contents.records.push_back({1000+i,vector});
    const auto large=Archive::declare(original.bundle(),std::move(contents));const auto bytes=encode_algebraic_archive(large);
    check(bytes.size()>128u*1024u,"large archive did not exceed one native Integer budget");
    const AlgebraicArchiveChannel channel{701,1,100};const auto endpoint=with_algebraic_archive(TemporalEndpoint(large.bundle().source()),channel,large);
    check(encode_algebraic_archive(read_algebraic_archive(endpoint,channel))==bytes,"chunked archive lost exact bytes");
    auto bad=endpoint;auto& chunks=std::get<rfc::composite::DeclaredValue::Repeated>(bad.declarations->values.front().value.data).elements;
    std::swap(chunks.front(),chunks.back());rejects([&]{(void)read_algebraic_archive(bad,channel);},"unordered archive chunks accepted");
    bad=endpoint;auto& incomplete=std::get<rfc::composite::DeclaredValue::Repeated>(bad.declarations->values.front().value.data).elements;
    incomplete.front()=rfc::composite::DeclaredValue::integer(algebraic_archive_history_detail::pack(std::span<const std::byte>(bytes).first(8)));
    rejects([&]{(void)read_algebraic_archive(bad,channel);},"short interior archive chunk accepted");
    {auto store=AlgebraicDurableHistory::create(path,large,channel,992);(void)store.root();}
    auto restored=AlgebraicDurableHistory::open(path,channel);
    check(encode_algebraic_archive(restored.at(0))==bytes,"large chunked archive durable reopen differs");
}
int main() {try {Temporary directory;durable(directory.path/"history");grouped(directory.path/"grouped");publication_and_invalid_payload(directory.path);
    large_chunks(directory.path/"large");std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";}catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
