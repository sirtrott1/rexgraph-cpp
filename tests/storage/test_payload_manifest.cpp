#include "rfc/database/payload_residency.hpp"
#include "rfc/core/owned_source.hpp"
#include <atomic>
#include <iostream>
#include <thread>

namespace s=rfc::storage;
namespace d=rfc::database;
namespace {
std::size_t checks{};
void ck(bool value) { ++checks; if(!value) throw std::runtime_error("payload manifest assertion failed"); }
template<class F> void refuses(F&& f) {
    bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } ck(rejected);
}
using Bytes=std::vector<std::byte>;
const s::PayloadSourceRevision source{71,8,3,123456};
Bytes data{std::byte{4},std::byte{0},std::byte{9}};
s::PayloadDescriptor descriptor(std::uint64_t key, const Bytes& raw) {
    s::PayloadDescriptor p; p.identity={source,key}; p.format="synthetic-exact-v1";
    p.stored_bytes=raw.size(); p.checksum=s::rfcx_detail::fnv1a64(raw); p.decoded_bytes=4096; p.coefficient_bits=513;
    p.cells={{{0},0},{{0},1},{{1},10}};
    p.occurrences={{{71},{{1},10},{{0},0},0,true},{{72},{{1},10},{{0},0},1,false},{{73},{{1},10},{{0},1},2,true}};
    return p;
}
s::PayloadManifest fixture() {
    auto a=descriptor(1,data), b=descriptor(2,{}), c=descriptor(3,data);
    a.dependencies={b.identity}; b.dependencies={c.identity}; c.dependencies={a.identity};
    return s::PayloadManifest::create({source},{a,b,c});
}
void manifest_tests() {
    auto m=fixture(); const auto original=Bytes(m.bytes().begin(),m.bytes().end());
    auto cold=s::PayloadManifest::decode_exact(original,m);
    ck(!cold.same_owner(m)); ck(cold.payloads()==m.payloads()); ck(cold.sources()==m.sources());
    ck(cold.usage()==m.usage()); ck(cold.usage().payloads==3 && cold.usage().occurrences==9);
    ck(cold.usage().index_bytes>0 && cold.usage().metadata_bytes>cold.usage().encoded_bytes);
    auto shared=m; ck(shared.same_owner(m));
    ck(m.payloads()[0].occurrences[0].lower==m.payloads()[0].occurrences[1].lower);
    ck(!m.payloads()[0].occurrences[1].exists && m.payloads()[0].occurrences[1].position==1);
    ck(m.payloads()[0].dependencies[0]==m.payloads()[1].identity); // cycle retained, no DAG assumption
    auto sources=m.sources(); ++sources[0].revision;
    refuses([&]{(void)s::PayloadManifest::decode(original,sources);});
    sources=m.sources(); ++sources[0].checksum;
    refuses([&]{(void)s::PayloadManifest::decode(original,sources);});
    for(std::size_t n=0;n<original.size();++n) refuses([&]{(void)s::PayloadManifest::decode(std::span(original).first(n),m.sources());});
    for(std::size_t i=0;i<original.size();++i) {
        auto changed=original; changed[i]^=std::byte{1};
        refuses([&]{(void)s::PayloadManifest::decode_exact(changed,m);});
    }
    auto p=m.payloads(); p[0].format="different-valid-codec"; auto changed=s::PayloadManifest::create(m.sources(),p);
    refuses([&]{(void)s::PayloadManifest::decode_exact(changed.bytes(),m);});
    p=m.payloads(); ++p[0].identity.source.revision;
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].dependencies[0].source.revision++;
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[1].identity=p[0].identity;
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].dependencies.push_back(p[0].dependencies[0]);
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].dependencies={p[0].identity};
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].occurrences[1].key=p[0].occurrences[0].key;
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].occurrences[1].position=0;
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p=m.payloads(); p[0].cells.erase(p[0].cells.begin());
    refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    p[0].scope=s::PayloadScope::Contribution; // actual direct dependency supplies the missing cell
    auto fragment=s::PayloadManifest::create(m.sources(),p); ck(fragment.payloads()[0].cells.size()==2);
    p[0].dependencies.clear(); refuses([&]{(void)s::PayloadManifest::create(m.sources(),p);});
    const auto small=[&](auto member,std::size_t maximum) {
        s::PayloadManifestLimits limits; limits.*member=maximum;
        refuses([&]{(void)s::PayloadManifest::create(m.sources(),m.payloads(),limits);});
        refuses([&]{(void)s::PayloadManifest::decode(original,m.sources(),limits);});
    };
    small(&s::PayloadManifestLimits::max_encoded_bytes,original.size()-1);
    small(&s::PayloadManifestLimits::max_metadata_bytes,m.usage().metadata_bytes-1);
    small(&s::PayloadManifestLimits::max_validation_bytes,m.usage().validation_bytes-1);
    small(&s::PayloadManifestLimits::max_sources,0); small(&s::PayloadManifestLimits::max_payloads,2);
    small(&s::PayloadManifestLimits::max_cells,8); small(&s::PayloadManifestLimits::max_occurrences,8);
    small(&s::PayloadManifestLimits::max_dependencies,2); small(&s::PayloadManifestLimits::max_format_bytes,1);
    small(&s::PayloadManifestLimits::max_validation_steps,0);
    auto mutable_wire=original;
    auto frozen=s::PayloadManifest::decode(mutable_wire,m.sources(),{},[&]{mutable_wire[0]=std::byte{0};});
    ck(std::equal(frozen.bytes().begin(),frozen.bytes().end(),original.begin()));
    auto mutable_sources=m.sources();
    auto snapshot=s::PayloadManifest::decode(original,mutable_sources,{},[&]{mutable_sources.clear();});
    ck(snapshot.sources()==m.sources());
    auto mutable_expected=m; auto empty_expected=s::PayloadManifest::create({},{});
    auto pinned_expected=s::PayloadManifest::decode_exact(original,mutable_expected,{},[&]{mutable_expected=empty_expected;});
    ck(pinned_expected.payloads()==m.payloads());
    auto mutable_records=m.payloads(); bool first=true;
    auto frozen_records=s::PayloadManifest::create(m.sources(),mutable_records,{},[&]{if(first){mutable_records.clear();first=false;}});
    ck(frozen_records.payloads()==m.payloads());
    refuses([&]{(void)s::PayloadManifest::decode(original,m.sources(),{},[]{throw rfc::runtime::ExecutionCancelled();});});
    auto empty=s::PayloadManifest::create({},{}); ck(empty.payloads().empty());
    auto empty_cold=s::PayloadManifest::decode_exact(empty.bytes(),empty); ck(empty_cold.payloads().empty());
    s::PayloadManifestLimits tiny; tiny.max_validation_bytes=64; bool called=false;
    refuses([&]{(void)s::PayloadManifest::decode(empty.bytes(),m.sources(),tiny,[&]{called=true;});});
    ck(!called);
}
void residency_tests() {
    auto m=fixture(); auto a=m.payloads()[0].identity,b=m.payloads()[1].identity,c=m.payloads()[2].identity;
    d::PayloadResidency r(m); auto before=r.usage();
    ck(before.metadata_bytes>0 && before.resident_payload_bytes==0 && before.loading_payload_bytes==0);
    ck(r.status(a).state==d::PayloadResidencyState::Unloaded);
    refuses([&]{(void)r.copy_payload(a,100);});
    auto missing=a; missing.key=99; ck(!m.find(missing)); refuses([&]{(void)r.status(missing);});
    auto stale=source; ++stale.revision; refuses([&]{(void)r.begin(a,stale);}); ck(r.usage()==before);
    refuses([&]{(void)r.begin(a,source,[]{throw rfc::runtime::ExecutionCancelled();});}); ck(r.usage()==before);
    {
        auto t=r.begin(a,source); ck(r.status(a).state==d::PayloadResidencyState::Loading);
        ck(r.usage().loading_payload_bytes==data.size() && r.usage().resident_payload_bytes==0);
        refuses([&]{(void)r.copy_payload(a,100);}); refuses([&]{(void)r.begin(a,source);});
    }
    ck(r.status(a).state==d::PayloadResidencyState::Refused && r.status(a).refusal==d::PayloadRefusal::Abandoned);
    ck(r.usage().loading==0 && r.usage().loading_payload_bytes==0);
    auto t=r.begin(a,source); ck(t.generation()==2);
    auto bad=data; bad[0]=std::byte{99}; refuses([&]{r.commit(t,bad);});
    ck(r.status(a).refusal==d::PayloadRefusal::Integrity); ck(r.usage().loading_payload_bytes==0);
    auto cancelled=r.begin(a,source);
    refuses([&]{r.commit(cancelled,data,[]{throw rfc::runtime::ExecutionCancelled();});});
    ck(r.status(a).refusal==d::PayloadRefusal::Cancelled && r.usage().resident==0);
    auto wrong_size=r.begin(a,source); refuses([&]{r.commit(wrong_size,{});}); ck(r.usage().loading==0);
    auto final=r.begin(a,source); auto mutable_data=data;
    r.commit(final,mutable_data,[&]{mutable_data[0]=std::byte{88};});
    ck(r.copy_payload(a,3)==data); ck(r.status(a).state==d::PayloadResidencyState::Resident);
    ck(r.usage().resident_payload_bytes==3 && r.usage().loading_payload_bytes==0);
    refuses([&]{(void)r.copy_payload(a,2);}); refuses([&]{r.commit(final,data);});
    auto empty=r.begin(b,source); r.commit(empty,{});
    ck(r.status(b).state==d::PayloadResidencyState::Resident && r.copy_payload(b,0).empty());
    ck(r.usage().resident==2 && r.usage().resident_payload_bytes==3);
    ck(r.status(c).state==d::PayloadResidencyState::Unloaded); // no automatic dependency reads
    d::PayloadReceivingLimits limits; limits.max_metadata_bytes=before.metadata_bytes-1;
    refuses([&]{d::PayloadResidency small(m,limits);});
    limits={}; limits.max_payload_bytes=0; d::PayloadResidency small(m,limits);
    refuses([&]{(void)small.begin(a,source);}); ck(small.status(a).refusal==d::PayloadRefusal::Budget);
    auto zero=small.begin(b,source); small.commit(zero,{}); ck(small.usage().resident==1);
    limits={}; limits.max_loading=1; limits.max_payload_bytes=3; d::PayloadResidency capped(m,limits);
    auto one=capped.begin(a,source); refuses([&]{(void)capped.begin(c,source);});
    capped.refuse(one,d::PayloadRefusal::Provider); ck(capped.usage().loading==0);
    auto retry=capped.begin(c,source); capped.commit(retry,data); ck(capped.usage().resident_payload_bytes==3);
    refuses([&]{(void)capped.begin(a,source);}); ck(capped.usage().resident==1);
    d::PayloadResidency other(s::PayloadManifest::decode_exact(m.bytes(),m));
    auto foreign=other.begin(c,source); refuses([&]{r.commit(foreign,data);});
    other.refuse(foreign,d::PayloadRefusal::Provider); ck(other.usage().loading==0);
    // Reentrant cancellation keeps the still-live frozen buffer charged and
    // prevents a replacement generation until the original completion unwinds.
    d::PayloadResidency reentrant(m,limits); auto active=reentrant.begin(a,source);
    refuses([&]{reentrant.commit(active,data,[&]{
        reentrant.refuse(active,d::PayloadRefusal::Cancelled);
        ck(reentrant.usage().loading_payload_bytes==3);
        refuses([&]{(void)reentrant.begin(a,source);});
        refuses([&]{(void)reentrant.begin(c,source);});
    });});
    ck(reentrant.status(a).refusal==d::PayloadRefusal::Cancelled && reentrant.usage().loading==0);
    auto next=reentrant.begin(a,source); ck(next.generation()==2); reentrant.commit(next,data);
}
void native_source_test() {
    using namespace rfc;
    core::RelationSignature sig; const auto type=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,4);
    core::RelationalComplexBuilder builder(sig);
    numeric::Integer n=1; n<<=90; numeric::Rational q(n+1,7); q.canonicalize();
    builder.add_grade_one(core::RelationKey{77},type,{
        {1,true,true,q,{},core::OccurrenceKey{101}},
        {1,false,true,q,{},core::OccurrenceKey{102}},
        {2,true,false,q,{},core::OccurrenceKey{103}}});
    const auto original=builder.freeze(); const auto raw=s::encode_rfcx(original).bytes;
    const s::PayloadSourceRevision src{98,2,7,s::rfcx_detail::fnv1a64(raw)};
    s::PayloadDescriptor p; p.identity={src,5}; p.format="RFCX-1"; p.stored_bytes=raw.size(); p.checksum=src.checksum;
    p.decoded_bytes=16384; p.coefficient_bits=1024;
    for(auto key:original.grade({0}).identities) p.cells.push_back({{0},key});
    p.cells.push_back({{1},77}); const auto& grade=original.grade({1});
    for(std::size_t i=0;i<grade.incidence_count();++i)
        p.occurrences.push_back({original.occurrence_identity({1},i),{{1},77},
            {{0},original.grade({0}).identities[grade.lower_index[i]]},i,grade.aspects.get(sig.existence_aspect(),i)});
    auto manifest=s::PayloadManifest::create({src},{p}); auto cold_manifest=s::PayloadManifest::decode_exact(manifest.bytes(),manifest);
    d::PayloadResidency residency(cold_manifest); auto ticket=residency.begin(p.identity,src); residency.commit(ticket,raw);
    const auto retained=residency.copy_payload(p.identity,raw.size()); s::MappedRfcx mapped(retained);
    const auto restored=mapped.materialize(); ck(s::encode_rfcx(restored).bytes==raw);
    core::OwnedRelationalSource old_owner(original),fresh_owner(restored); ck(!old_owner.same_owner(fresh_owner));
    ck(restored.boundary_apply({1},{1})==original.boundary_apply({1},{1}));
    ck(restored.occurrence_identity({1},1)==core::OccurrenceKey{102});
    ck(!restored.grade({1}).aspects.get(restored.signature().existence_aspect(),1));
    ck(restored.incidence_coefficient({1},0)==original.incidence_coefficient({1},0));
    ck(cold_manifest.payloads()[0].occurrences==p.occurrences);
}
void concurrency_test() {
    auto m=fixture(); auto key=m.payloads()[0].identity; d::PayloadResidency r(m);
    std::atomic<unsigned> entered{},ready{}; std::atomic<bool> go{}; std::vector<std::thread> threads;
    for(unsigned i=0;i<8;++i) threads.emplace_back([&]{
        ++ready; while(!go.load()) std::this_thread::yield();
        try {auto ticket=r.begin(key,source); ++entered; while(ready.load()!=16) std::this_thread::yield(); r.commit(ticket,data);}
        catch(const std::invalid_argument&) {}
        ++ready;
    });
    while(ready.load()!=8) std::this_thread::yield();
    go=true;
    // Release the winning thread after all seven refusals. The winner is the
    // remaining participant, so 15 means every competing admission returned.
    while(ready.load()!=15) std::this_thread::yield();
    ++ready;
    for(auto& thread:threads) thread.join();
    ck(entered==1 && r.usage().resident==1 && r.copy_payload(key,3)==data);
}
}
int main() {try {manifest_tests(); residency_tests(); native_source_test(); concurrency_test(); std::cout<<checks<<" payload manifest/residency checks passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
