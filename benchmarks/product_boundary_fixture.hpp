#pragma once
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query.hpp"
#include "rfc/database/logical_manifest.hpp"
#include "rfc/models/native_context.hpp"
#include <iostream>
#include <sstream>
namespace rfc_product_boundary {
using namespace rfc;
using namespace database;
using namespace storage;
using Q=numeric::Rational;
using V=composite::DeclaredValue;
using D=composite::Declaration;
struct Verify {
    std::size_t count{};
    void operator()(bool value,const char* message) {++count;if(!value)throw std::runtime_error(std::string("boundary product: ")+message);}
    template<class F> void rejects(F&& f,const char* message) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}(*this)(failed,message);}
};
inline Q rational(long numerator, unsigned long denominator) { Q value{numerator,denominator};value.canonicalize();return value; }
inline composite::DeclarationId id(std::uint64_t key){return {9606,key,1};}
inline InterpretationSchema schema() {
    InterpretationSchema s;s.source_namespace=9600;s.binding_namespace=9601;
    s.values=composite::DeclarationRegistry({D::rational(id(1)),D::optional(id(2),id(1)),D::finite(id(3),{0,1,127,128,255}),
        D::repeated(id(4),id(3),0,32),D::product(id(5),{{1,id(2),0},{2,id(4),0}})});
    s.subjects={{1,"participants"}};
    s.assertions={{10,"collective",id(5),{{11,1,2,2,false,false},{12,1,3,3,false,false}},{{1,"exact_quantity"},{2,"tagged_octets"}}}};
    return s;
}
inline LogicalSnapshot logical(std::size_t n,bool target) {
    LogicalSnapshot input;input.revision=target?"snapshot/1":"snapshot/0";
    input.subjects={{1,"alice"},{1,"bob"},{1,"carol"}};
    for(std::size_t k=0;k<n+(target?1u:0u);++k) {
        const auto quantity=k%7==0?V::absent():V::present(V::rational(k%5==0?Q{0}:rational(static_cast<long>(k+1),static_cast<unsigned long>(k+3))));
        ExternalAssertion assertion{{10,"event/"+std::to_string(k)},V::product({quantity,V::repeated({V::symbol(0),V::symbol(255),V::symbol(0),V::symbol(128)})}),
            {{11,ReferenceState::Present,{{"producer/a",{1,"alice"},true},{"producer/b",{1,"bob"},true}}},
             {12,ReferenceState::Present,{{"consumer/a",{1,"alice"},true},{"consumer/c",{1,"carol"},true},{"consumer/again",{1,"alice"},false}}}}};
        if(target && k==1)assertion.payload=V::product({V::present(V::rational(Q{0})),V::repeated({})});
        if(target && k==2)std::reverse(assertion.roles[1].occurrences.begin(),assertion.roles[1].occurrences.end());
        input.assertions.push_back(std::move(assertion));
    }
    return input;
}
inline void verify_export(Verify& verify,const InterpretationSchema& s,const LogicalSnapshot& expected,const InterpretedSnapshot& actual) {
    const auto exported=actual.export_snapshot();verify(exported.revision==expected.revision,"logical revision");
    verify(exported.subjects==expected.subjects,"retained subject identities");
    std::map<ExternalOrigin,const ExternalAssertion*> by_origin;
    for(const auto& assertion:expected.assertions)by_origin.emplace(assertion.origin,&assertion);
    verify(exported.assertions.size()==by_origin.size(),"assertion multiplicity");
    const composite::CompiledLayout layout(s.values,id(5));
    for(const auto& assertion:exported.assertions) {
        verify(by_origin.contains(assertion.origin),"origin retained");const auto& before=*by_origin.at(assertion.origin);
        const auto a=encode_declared_bits(layout,before.payload),b=encode_declared_bits(layout,assertion.payload);
        verify(a.bit_count==b.bit_count && a.bytes==b.bytes,"complete canonical payload and byte constructors");
        verify(before.roles==assertion.roles,"repeated participants, inactive legs, meaningful order");
    }
}
inline Q expected_sum(std::size_t n,bool changed) {
    Q sum=0;for(std::size_t k=0;k<n+(changed?1u:0u);++k)if(k%7 && k%5 && !(changed && k==1))sum+=rational(static_cast<long>(k+1),static_cast<unsigned long>(k+3));return sum;
}
template<class Measure> void run(std::size_t n,const std::optional<std::filesystem::path>& out,Verify& verify,Measure&& measure) {
    if(n<3 || n>256)throw std::invalid_argument("boundary fixture requires 3..256 assertions");
    const auto s=schema();const auto before=logical(n,false),after=logical(n,true);
    const auto base=measure("interpret",0,[&]{return InterpretedSnapshot::interpret(s,before);});
    const auto bytes=measure("interpretation_encode",0,[&]{return encode_interpreted_snapshot(base);});
    const auto disk_base=measure("interpretation_decode_validate",0,[&]{return decode_interpreted_snapshot(bytes);});
    verify(encode_interpreted_snapshot(disk_base)==bytes,"canonical archive");verify_export(verify,s,before,disk_base);
    auto reordered=before;std::reverse(reordered.assertions.begin(),reordered.assertions.end());std::reverse(reordered.subjects.begin(),reordered.subjects.end());
    const auto reordered_native=InterpretedSnapshot::interpret(s,reordered);
    verify(encode_interpreted_snapshot(reordered_native)==bytes,"scan order does not become identity");
    const auto sum=measure("native_sum",0,[&]{return sum_exact(disk_base,10,{1});});verify(sum==std::optional<Q>{expected_sum(n,false)},"native exact query after import");
    for(const auto key:disk_base.assertions(10)) {
        verify(disk_base.participants(key,11).size()==2 && disk_base.participants(key,12).size()==3,"2-to-3 collective is five retained legs, not six pairs");
        verify(disk_base.participants(key,12,true).size()==2,"inactive leg retained but filtered explicitly");
        const auto consumers=disk_base.participants(key,12);verify(consumers[0].participant()==consumers[2].participant(),"shared participant identity does not merge occurrences");
        verify(consumers[0].key()!=consumers[2].key(),"occurrence identities retained separately");
    }
    const auto target=measure("interpretation_replace",1,[&]{return base.replace(after);});
    const auto update=measure("interpretation_difference",1,[&]{return base.difference(target);});
    const auto frame=measure("interpretation_update_encode",1,[&]{return encode_interpretation_update(update);});
    const auto replay=measure("interpretation_replay",1,[&]{return disk_base.apply(decode_interpretation_update(frame));});
    verify_export(verify,s,after,replay);verify(encode_interpreted_snapshot(replay)==encode_interpreted_snapshot(target),"complete target image replay");
    verify(sum_exact(replay,10,{1})==std::optional<Q>{expected_sum(n,true)},"target exact sum");
    LogicalSnapshotManifest manifest;manifest.revision=before.revision;
    const auto middle=before.assertions.begin()+static_cast<std::ptrdiff_t>(n/2);
    manifest.fragments={{"part/a",before.subjects,{before.assertions.begin(),middle}},
        {"part/b",before.subjects,{middle,before.assertions.end()}}};
    const auto from_fragments=interpret_logical_manifest(s,manifest);
    verify(encode_interpreted_snapshot(from_fragments)==bytes,"logical fragments and direct interpretation coincide");
    manifest.revision="deleted/1";manifest.deletions.push_back(before.assertions.back().origin);
    verify(materialize_logical_snapshot(manifest).assertions.size()==n-1,"logical snapshot deletion honored");
    // Two namespaces may have identical raw keys and bytes. Permission for one
    // never authorizes the other. The source is pinned before host callbacks.
    NativeAtlasManifest atlas_plan;atlas_plan.key=96;atlas_plan.revision=1;
    const NativeAtlas atlas({replay.source(),NativeAtlasSource(2,9602,*replay.endpoint())},atlas_plan);
    const auto payload_key=replay.assertions(10).front();
    const auto descriptor=replay.payload(payload_key,{1}).descriptor();
    const auto request=descriptor.request();
    const auto policy=[](const models::NativeContextAuthorization& origin){return origin.source==1;};
    const auto context=measure("authorized_context",1,[&]{return models::NativeConsumerContext::build(atlas,{{1,request}},policy);});
    verify(context.items().size()==1,"bounded authorized context");
    verify.rejects([&]{models::NativeConsumerContext::build(atlas,{{1,request},{2,request}},policy);},"matching content/keys do not transfer permission");
    verify.rejects([&]{models::NativeConsumerContext::build(atlas,{{1,request}},{});},"absent host policy denies");
    const auto before_proposal=encode_interpreted_snapshot(replay);
    const auto proposal=context.propose(models::ProspectiveEncoding::Approximate,"fixture-only","uncommitted candidate",{std::byte{1},std::byte{2}});
    verify(proposal.encoding()==models::ProspectiveEncoding::Approximate && proposal.bytes().size()==2,"prospective encoding remains explicit");
    verify(encode_interpreted_snapshot(replay)==before_proposal,"model proposal cannot alter exact state");
    if(out) {
        interpretation_codec_detail::write(*out/"base.rfcxi",bytes);interpretation_codec_detail::write(*out/"change.rfcxiu",frame);
        write_interpreted_snapshot(replay,*out/"target.rfcxi");
        const auto reloaded=read_interpreted_snapshot(*out/"base.rfcxi");ReadOnlyMappedFile update_file(*out/"change.rfcxiu");
        verify(encode_interpreted_snapshot(reloaded.apply(decode_interpretation_update(update_file.bytes())))==before_proposal,"actual disk-only boundary replay");
        std::ofstream result(*out/"boundary.csv");
        result<<"assertions,step,retained_occurrences,exact_sum,archive_bytes\n";
        result<<n<<",0,"<<disk_base.endpoint()->source.grade(core::Grade{1}).incidence_count()<<','<<sum->get_str()<<','<<bytes.size()<<'\n';
        result<<n+1<<",1,"<<replay.endpoint()->source.grade(core::Grade{1}).incidence_count()<<','<<sum_exact(replay,10,{1})->get_str()<<','<<before_proposal.size()<<'\n';
        if(!result)throw std::runtime_error("boundary CSV write failed");
    }
}
}
