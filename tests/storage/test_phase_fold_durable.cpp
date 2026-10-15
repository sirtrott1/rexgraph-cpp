#include "../../examples/phase_fold_storage/model.hpp"
#include "rfc/storage/phase_fold_durable.hpp"
#include <iostream>

using namespace phase_fold_example;using namespace rfc::storage;
static std::size_t checks{};
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {bool failed=false;try {action();} catch (const std::exception&) {failed=true;}check(failed,message);}
struct Temporary {
    std::filesystem::path path;
    Temporary() {auto pattern=(std::filesystem::temp_directory_path()/"rfc-phase-fold-XXXXXX").string();
        const auto* result=::mkdtemp(pattern.data());if (!result) throw std::runtime_error("test temporary directory");path=result;}
    ~Temporary() {std::error_code error;std::filesystem::remove_all(path,error);}
};
static auto objects(const std::filesystem::path& path) {
    std::map<std::string,std::vector<std::byte>> out;
    for (const auto& file:std::filesystem::directory_iterator(path/"objects")) out.emplace(file.path().filename().string(),read_file_exact(file.path(),32u<<20));
    return out;
}
static void durable(const std::filesystem::path& path,const Archive& base) {
    const PhaseFoldChannel channel{701,1,100};const auto applied=base.apply(7),observed=base.observed_prefix(0);
    const std::vector<Archive> first{applied},second{observed};DurableNativeRoot origin,committed;
    {
        auto store=PhaseFoldDurableHistory::create(path,base,channel,9001,40);origin=store.root();const auto before=objects(path);
        const auto pinned=store.snapshot();const auto receipt=store.append(1,origin,first);committed=receipt.root;
        check(committed.sequence==41 && receipt.frame_count==1 && receipt.first_sequence==40,"durable word transaction receipt differs");
        check(encode_phase_fold_archive(store.at(40))==encode_phase_fold_archive(base),"later branch or proposal leaked into retained revision");
        check(encode_phase_fold_archive(store.at(41))==encode_phase_fold_archive(applied),"replayed word publication differs");
        const auto observation=store.observed_prefix(40,0);check(observation.operation_count()==0 && observation.proposals().empty(),"durable observed prefix exposes proposals or suffix");
        rejects([&]{(void)pinned.at(41);},"old native snapshot sees future publication");
        for (const auto& [name,bytes]:before) check(objects(path).at(name)==bytes,"previous durable object changed");
        const auto after=objects(path);check(store.append(1,origin,first).root==committed && objects(path)==after,"identical retry rewrites original word");
        rejects([&]{(void)store.append(1,origin,second);},"transaction retry substitutes a different word");
        rejects([&]{(void)store.append(2,origin,second);},"stale publication predecessor accepted");
        check(store.root()==committed && objects(path)==after,"failed publication changed durable decision");
        // A cut before replacing HEAD has a definite negative outcome.
        rejects([&]{(void)store.append(2,committed,second,[](DurableEvent event) {
            if (event.stage==DurableStage::BeforeHeadReplace) throw std::runtime_error("before publication");
        });},"prepublication fault did not fail");
        check(store.root()==committed && !store.status(2),"prepublication fault acknowledged an unpublished revision");
        bool unknown=false;
        try {(void)store.append(2,committed,second,[](DurableEvent event) {
            if (event.stage==DurableStage::HeadReplaced) throw std::runtime_error("after publication");
        });} catch (const DurableUnknownOutcome& result) {unknown=result.transaction()==2;}
        check(unknown,"postpublication fault did not preserve unknown outcome");
        rejects([&]{(void)store.root();},"uncertain coordinator remained usable");
    }
    auto recovered=PhaseFoldDurableHistory::open(path,channel);
    check(recovered.root().sequence==42 && recovered.status(2).has_value(),"durable word decision not recovered");
    check(encode_phase_fold_archive(recovered.at(40))==encode_phase_fold_archive(base),"recovered earlier word changed");
    check(encode_phase_fold_archive(recovered.at(41))==encode_phase_fold_archive(applied),"recovered intermediate replay lost");
    check(encode_phase_fold_archive(recovered.at(42))==encode_phase_fold_archive(observed),"recovered zero-prefix snapshot differs");
    const auto before=objects(path);check(recovered.append(2,committed,second).root==recovered.root() && objects(path)==before,"recovered transaction applied twice");
    rejects([&]{(void)recovered.at(43);},"future revision admitted");
    const auto a=recovered.at(40),b=recovered.at(40);
    if (std::holds_alternative<temporal::RationalFoldHistory>(a.word())) {
        const auto& ah=std::get<temporal::RationalFoldHistory>(a.word());const auto& bh=std::get<temporal::RationalFoldHistory>(b.word());
        rejects([&]{(void)ah.attribution(bh,2);},"restoring same revision inferred transport between separate owners");
    }
}
static void grouped_walks(const std::filesystem::path& path) {
    const auto generator=root();const auto base=Archive::declare(walk({1,2,3,4,5,6},2,generator,0));
    std::vector<Archive> revisions{Archive::declare(walk({1,2,3,4,5,6},2,generator,1)),Archive::declare(walk({1,2,3,4,5,6},2,generator,2))};
    const PhaseFoldChannel channel{701,1,100};DurableNativeRoot final;
    {auto store=PhaseFoldDurableHistory::create(path,base,channel,9002);const auto receipt=store.append(5,store.root(),revisions,[&](DurableEvent){revisions.clear();});
        final=receipt.root;check(receipt.frame_count==2 && final.sequence==2,"caller mutation split durable word group");}
    auto store=PhaseFoldDurableHistory::open(path,channel);
    for (std::uint64_t seq=0;seq<=2;++seq) {
        const auto archived=store.at(seq);const auto& h=std::get<AlgebraicPhaseWalk>(archived.word());const auto phase=h.endpoint();
        check(h.lift().winding()==rfc::numeric::Integer(seq) && phase.value().coordinates()==V({1,0}),"equal endpoint phases collapsed durable winding histories");
    }
    check(store.root()==final && store.snapshot().segment_count()==2,"grouped native recovery differs");
}
static void publication_and_validation(const std::filesystem::path& directory) {
    const auto archive=samples()[2].second;const auto path=directory/"test.rfcxw";
    write_phase_fold_archive(archive,path,FilePublication::CreateNew);
    check(encode_phase_fold_archive(read_phase_fold_archive(path))==encode_phase_fold_archive(archive),"portable word file differs");
    rejects([&]{write_phase_fold_archive(archive,path,FilePublication::CreateNew);},"create-new replaced word");
    const auto before=read_file_exact(path,16u<<20);PhaseFoldLimits tiny;tiny.history.max_steps=1;
    rejects([&]{write_phase_fold_archive(archive,path,FilePublication::ReplaceRegular,tiny);},"invalid replacement published");
    check(read_file_exact(path,16u<<20)==before,"failed replacement changed word bytes");
    const PhaseFoldChannel channel{701,1,100};auto endpoint=with_phase_fold_archive(TemporalEndpoint(archive.source()),channel,archive);
    auto bytes=encode_phase_fold_archive(archive);bytes.back()^=std::byte{1};endpoint.declarations->values.front().value=algebraic_archive_history_detail::chunks(bytes);
    {auto raw=DurableNativeStore::create(directory/"invalid-opaque",endpoint,912);(void)raw.root();}
    rejects([&]{(void)PhaseFoldDurableHistory::open(directory/"invalid-opaque",channel);},"generic opaque value certified as an original word");
    rejects([&]{(void)PhaseFoldDurableHistory::create(directory/"not-created",archive,channel,913,0,tiny);},"invalid genesis admitted");
    check(!std::filesystem::exists(directory/"not-created"),"invalid genesis created filesystem root before admission");
    const auto& h=std::get<temporal::RationalFoldHistory>(archive.word());
    const auto unrelated=Archive::declare(temporal::RationalFoldHistory::begin(panels()));
    rejects([&]{(void)h.attribution(std::get<temporal::RationalFoldHistory>(unrelated.word()),2);},"equal independent source was used as fold comparison");
}
static void large_chunks(const std::filesystem::path& path) {
    std::vector<rfc::core::IdentityKey> ids;for (std::uint64_t i=0;i<900;++i) ids.push_back(1000+i);
    const auto source=rfc::algebra::oriented_polygon(ids,rfc::algebra::PolygonFilling::DoubledDiskSphere);
    const auto large=Archive::declare(temporal::RationalPhaseHistory::begin(source,{rfc::core::Grade{0},0},Phase::identity()));
    const auto bytes=encode_phase_fold_archive(large);check(bytes.size()>128u*1024u,"word fixture does not test multi-Integer native payload");
    const PhaseFoldChannel channel{701,1,100};const auto endpoint=with_phase_fold_archive(TemporalEndpoint(source),channel,large);
    check(encode_phase_fold_archive(read_phase_fold_archive(endpoint,channel))==bytes,"native chunks changed leading zeros or full source");
    auto bad=endpoint;auto& chunks=std::get<rfc::composite::DeclaredValue::Repeated>(bad.declarations->values.front().value.data).elements;
    chunks.front()=rfc::composite::DeclaredValue::integer(algebraic_archive_history_detail::pack(std::span<const std::byte>(bytes).first(8)));
    rejects([&]{(void)read_phase_fold_archive(bad,channel);},"short interior word chunk accepted");
    {auto store=PhaseFoldDurableHistory::create(path,large,channel,992);(void)store.root();}
    auto restored=PhaseFoldDurableHistory::open(path,channel);check(encode_phase_fold_archive(restored.at(0))==bytes,"large native word durable recovery differs");
}
int main() {try {Temporary directory;const auto cases=samples();
    for (std::size_t i=0;i<3;++i) durable(directory.path/("history-"+std::to_string(i)),cases[i].second);
    grouped_walks(directory.path/"walks");publication_and_validation(directory.path);large_chunks(directory.path/"large");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
