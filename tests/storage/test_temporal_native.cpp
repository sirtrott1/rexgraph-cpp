#include "rfc/storage/temporal_finite.hpp"
#include "rfc/storage/temporal_diagnostics.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <chrono>
#include <iostream>
#include <random>

#ifndef RFC_TEST_SOURCE_DIR
#define RFC_TEST_SOURCE_DIR "."
#endif

using namespace rfc;
using namespace rfc::storage;
using R = numeric::Rational;
using M = numeric::ExactMatrix;
namespace {
std::size_t checks = 0;
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template <class F> void rejects(F&& action, const char* message) {
    bool threw = false; try { action(); } catch (const std::exception&) { threw = true; }
    check(threw, message);
}
StableCellKey vertex(std::uint64_t key) { return {core::Grade{0},key}; }
StableCellKey edge(std::uint64_t key) { return {core::Grade{1},key}; }
TemporalFiniteValue integer(std::int64_t value) { return value; }
core::CellId resolve(const TemporalEndpoint& e, StableCellKey key) { return resolve_stable_cell(e.source,key).value(); }
bool equal(const M& a, const M& b) { return a.rows()==b.rows() && a.cols()==b.cols() && a.add(b.scale(R{-1})).nonzeros()==0; }
std::vector<std::byte> bytes_at(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary); if(!in)throw std::runtime_error("test input unavailable");
    const auto size=std::filesystem::file_size(path);std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!in)throw std::runtime_error("test input truncated");
    return bytes;
}
void rehash(std::vector<std::byte>& bytes) {
    const auto h=rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for(unsigned i=0;i<8;++i)bytes[24+i]=static_cast<std::byte>((h>>(8*i))&0xffu);
}
core::RelationalComplex triangle(bool reorder=false, bool extra=false) {
    core::RelationSignature sig;
    const auto type=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto empty=sig.add_type("empty",core::BoundaryPolicy::DeclaredExact,0,0);
    core::RelationalComplexBuilder b(sig);
    for(const auto v:reorder?std::vector<std::uint64_t>{12,11,10}:std::vector<std::uint64_t>{10,11,12})b.declare_witness_identity(v);
    if(extra)b.declare_witness_identity(99);
    for(const auto i:reorder?std::vector<std::uint64_t>{2,0,1}:std::vector<std::uint64_t>{0,1,2})
        b.add_grade_one(core::RelationKey{100+i},type,{
            {10+i,true,true,R{1},{},core::OccurrenceKey{1000+2*i}},
            {10+(i+1)%3,true,false,R{1},{},core::OccurrenceKey{1001+2*i}}});
    if(extra)b.add_grade_one(core::RelationKey{150},empty,{});
    return b.freeze();
}
TemporalFiniteSystem integer_system(const TemporalEndpoint& e,bool impossible) {
    local::FiniteIntegerConstraintSystem native(e.source);
    for(auto v:{10u,11u,12u})native.set_boolean_domain(resolve(e,vertex(v)));
    for(std::uint64_t i=0;i<3;++i) {
        auto patch=native.carrier().cell_patch(resolve(e,edge(100+i)));
        native.add_chart("pair-"+std::to_string(i),std::move(patch),
            {resolve(e,vertex(10+i)),resolve(e,vertex(10+(i+1)%3))},
            i==2 && !impossible ? std::vector<std::vector<std::int64_t>>{{0,0},{1,1}}
                                : std::vector<std::vector<std::int64_t>>{{0,1},{1,0}});
    }
    auto record=capture_temporal_finite_system(700,native,{300,301,302});
    local::BasicSection<std::int64_t> section;
    section[resolve(e,vertex(10))]=0;section[resolve(e,vertex(11))]=0;section[resolve(e,vertex(12))]=1;
    record.sections.push_back(capture_temporal_finite_section(e.source,900,section));
    return record;
}
TemporalFiniteSystem relation_system(const TemporalEndpoint& e) {
    local::FiniteRelationConstraintSystem native(e.source);
    const core::RelationIdentity a{core::Grade{1},core::RelationKey{100}},b{core::Grade{1},core::RelationKey{101}};
    native.set_domain(resolve(e,vertex(10)),{b,a}); // non-sorted enumeration is meaningful
    auto patch=native.carrier().cell_patch(resolve(e,vertex(10)));
    native.add_chart("choose",std::move(patch),{resolve(e,vertex(10))},{{a}});
    auto record=capture_temporal_finite_system(701,native,{400});
    local::BasicSection<core::RelationIdentity> section;section[resolve(e,vertex(10))]=a;
    record.sections.push_back(capture_temporal_finite_section(e.source,901,section));
    return record;
}
void finite_roundtrip(const std::filesystem::path& dir) {
    TemporalEndpoint old(triangle()),next(triangle(true,true),1);
    old.finite_systems={integer_system(old,false),relation_system(old)};
    next.finite_systems={integer_system(next,true),relation_system(next)};
    next.finite_systems[0].domains.push_back({vertex(99),{integer(std::numeric_limits<std::int64_t>::min()),integer(0),integer(std::numeric_limits<std::int64_t>::max())}});
    next.finite_systems[0].sections.push_back({902,{{vertex(99),integer(std::numeric_limits<std::int64_t>::min())}}});
    auto native_old=temporal_finite_constraint_system<std::int64_t>(old,700);
    check(native_old.global_section().has_value(),"old finite fixture not satisfiable");
    check(!native_old.globally_valid(temporal_finite_section<std::int64_t>(old,700,900)),"incompatible retained section silently repaired");
    const auto segment=make_temporal_segment(old,next);
    check(segment.finite_systems.size()==1 && !segment.finite_systems[0].birth,"finite changed-system replacement not sparse by system");
    check(segment.tensors.empty() && segment.correspondences.empty(),"finite change fabricated linear innovation");
    const auto bytes=encode_temporal_segment(segment);
    check(bytes[12]==std::byte{1},"finite segment feature flag missing");
    check(encode_temporal_segment(decode_temporal_segment(bytes))==bytes,"finite segment not byte stable");
    const auto restored=replay_temporal_segment(old,decode_temporal_segment(bytes));
    check(temporal_endpoint_hash(restored)==temporal_endpoint_hash(next),"finite endpoint target mismatch");
    auto native=temporal_finite_constraint_system<std::int64_t>(restored,700);
    check(!native.global_section().has_value(),"global finite obstruction lost");
    const auto certificate=native.exact_global_unsat_core();
    check(certificate.has_value() && certificate->subset_minimal && certificate->chart_indices==std::vector<std::size_t>{0,1,2},"chart-index unsat certificate changed");
    const auto& domain=native.domain(resolve(restored,vertex(99)));
    check(domain==std::vector<std::int64_t>{std::numeric_limits<std::int64_t>::min(),0,std::numeric_limits<std::int64_t>::max()},"signed extrema/domain order lost");
    check(temporal_finite_section<std::int64_t>(restored,700,902).at(resolve(restored,vertex(99)))==std::numeric_limits<std::int64_t>::min(),"finite partial section lost");
    auto relations=temporal_finite_constraint_system<core::RelationIdentity>(restored,701);
    check(relations.global_section().has_value(),"relation-valued system did not reconstruct");
    check(relations.domain(resolve(restored,vertex(10))).front().key.value==101,"relation domain order lost");
    check(relations.globally_valid(temporal_finite_section<core::RelationIdentity>(restored,701,901)),"relation-valued section lost");
    check(capture_temporal_finite_system(701,relations,{400}).charts==relation_system(restored).charts,"native relation capture is not inverse");
    rejects([&]{(void)temporal_finite_constraint_system<std::int64_t>(restored,701);},"wrong finite value specialization accepted");
    rejects([&]{(void)temporal_finite_section<std::int64_t>(restored,700,1234);},"missing finite section accepted");
    const auto base_path=dir/"finite.rfcx",seed_path=dir/"finite.rfcxe",history_path=dir/"finite.rfcxt";
    write_rfcx(old.source,base_path);write_temporal_endpoint(old,seed_path);append_temporal_segment(history_path,old,segment);
    MappedRfcx mapped(base_path);const auto disk_base=read_temporal_endpoint(mapped,seed_path);
    check(temporal_endpoint_hash(replay_temporal_segment(disk_base,read_temporal_segment(history_path)))==temporal_endpoint_hash(next),"disk-only finite replay failed");
    const auto seed=encode_temporal_endpoint(old);
    check(encode_temporal_endpoint(decode_temporal_endpoint(old.source,seed))==seed,"finite seed is not byte stable");
    auto wrong=old;wrong.finite_systems[0].sections[0].assignments[0].value=integer(1);
    rejects([&]{(void)replay_temporal_segment(wrong,segment);},"finite predecessor content not hash-bound");
    auto changed=old;changed.sequence=1;std::swap(changed.finite_systems[0].charts[0].support[0],changed.finite_systems[0].charts[0].support[1]);
    const auto support_delta=make_temporal_segment(old,changed);
    check(support_delta.finite_systems.size()==1,"tuple coordinate order change omitted");
    check(replay_temporal_segment(old,support_delta).finite_systems[0].charts[0].support==changed.finite_systems[0].charts[0].support,"tuple coordinate order not retained");
    TemporalEndpoint reordered(triangle(true),1);reordered.finite_systems={integer_system(reordered,false),relation_system(reordered)};
    const auto order_delta=make_temporal_segment(old,reordered);
    check(order_delta.finite_systems.empty(),"arena reorder spuriously changed finite records");
    const auto order_target=replay_temporal_segment(old,order_delta);
    check(temporal_finite_constraint_system<std::int64_t>(order_target,700).global_section().has_value(),"stable finite identities failed arena reorder");
    auto normalized=old;std::reverse(normalized.finite_systems.begin(),normalized.finite_systems.end());
    for(auto& system:normalized.finite_systems){std::reverse(system.domains.begin(),system.domains.end());for(auto& chart:system.charts)std::reverse(chart.patch.begin(),chart.patch.end());}
    check(encode_temporal_endpoint(normalized)==seed,"set-like record order changed canonical finite image");
    auto later=restored;later.sequence=2;later.finite_systems.clear();
    const auto removal=make_temporal_segment(restored,later);
    check(removal.removed_finite_systems==std::vector<std::uint64_t>{700,701},"finite removals not retained");
    check(replay_temporal_segment(restored,decode_temporal_segment(encode_temporal_segment(removal))).finite_systems.empty(),"finite removal replay failed");
    auto newborn=later;newborn.sequence=3;newborn.finite_systems.push_back({702,TemporalFiniteValueKind::SignedInteger,{},{},{}});
    const auto birth=make_temporal_segment(later,newborn);
    check(birth.finite_systems.size()==1 && birth.finite_systems.front().birth,"empty finite-system birth lost");
    check(replay_temporal_segment(later,decode_temporal_segment(encode_temporal_segment(birth))).finite_systems.size()==1,"empty finite system mistaken for absence");
    std::cout<<"finite_segment_bytes="<<bytes.size()<<" finite_seed_bytes="<<seed.size()<<'\n';
}
void finite_rejections() {
    TemporalEndpoint e(triangle());e.finite_systems={integer_system(e,false),relation_system(e)};
    const auto bad=[&](auto edit,const char* message){auto f=e;edit(f);rejects([&]{validate_temporal_endpoint(f);},message);};
    bad([](auto& f){f.finite_systems.push_back(f.finite_systems[0]);},"duplicate finite system accepted");
    bad([](auto& f){f.finite_systems[0].domains.push_back(f.finite_systems[0].domains[0]);},"duplicate domain accepted");
    bad([](auto& f){f.finite_systems[0].domains[0].values.clear();},"empty native domain accepted");
    bad([](auto& f){f.finite_systems[0].domains[0].values.push_back(integer(0));},"duplicate domain value accepted");
    bad([](auto& f){f.finite_systems[0].domains[0].cell=vertex(999);},"missing domain owner accepted");
    bad([](auto& f){f.finite_systems[0].kind=static_cast<TemporalFiniteValueKind>(9);},"unknown finite value kind accepted");
    bad([](auto& f){f.finite_systems[0].domains[0].values[0]=core::RelationIdentity{{1},{100}};},"mixed finite value domains accepted");
    bad([](auto& f){f.finite_systems[1].domains[0].values[0]=core::RelationIdentity{{0},{10}};},"vertex masquerading as relation value accepted");
    bad([](auto& f){f.finite_systems[1].domains[0].values[0]=core::RelationIdentity{{1},{999}};},"dangling relation-valued choice accepted");
    bad([](auto& f){f.finite_systems[0].charts.push_back(f.finite_systems[0].charts[0]);},"duplicate finite chart accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].patch={edge(100)};},"nonclosed chart patch accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].patch.push_back(vertex(10));},"duplicate patch cell accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].support[0]=vertex(12);},"support outside patch accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].support[0]=f.finite_systems[0].charts[0].support[1];},"duplicate support coordinate accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].support.clear();},"empty support accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].allowed.clear();},"native-invalid empty allowed relation accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].allowed[0].pop_back();},"wrong tuple arity accepted");
    bad([](auto& f){f.finite_systems[0].charts[0].allowed[0][0]=integer(8);},"out-of-domain tuple accepted");
    bad([](auto& f){f.finite_systems[0].sections.push_back(f.finite_systems[0].sections[0]);},"duplicate section key accepted");
    bad([](auto& f){f.finite_systems[0].sections[0].assignments.push_back(f.finite_systems[0].sections[0].assignments[0]);},"duplicate section assignment accepted");
    bad([](auto& f){f.finite_systems[0].sections[0].assignments[0].value=integer(8);},"out-of-domain section value accepted");
    auto next=e;next.sequence=1;next.finite_systems[0].charts[0].name="changed";
    auto s=make_temporal_segment(e,next);
    auto tampered=s;tampered.finite_systems.push_back(tampered.finite_systems[0]);
    rejects([&]{(void)replay_temporal_segment(e,tampered);},"duplicate finite replacement accepted");
    tampered=s;tampered.finite_systems[0].birth=true;
    rejects([&]{(void)replay_temporal_segment(e,tampered);},"existing finite identity reborn without removal");
    tampered=s;tampered.removed_finite_systems={700};
    rejects([&]{(void)replay_temporal_segment(e,tampered);},"conflicting finite remove and update accepted");
    auto migrated=e;migrated.sequence=1;migrated.finite_systems[0]=migrated.finite_systems[1];migrated.finite_systems[0].key=700;
    rejects([&]{(void)make_temporal_segment(e,migrated);},"same-key finite value-kind migration accepted");
    tampered=s;tampered.finite_systems[0].target=migrated.finite_systems[0];tampered.target_endpoint_hash=temporal_endpoint_hash(migrated);
    rejects([&]{(void)replay_temporal_segment(e,tampered);},"direct replay bypassed finite value-kind contract");
    const auto original_hash=temporal_endpoint_hash(e);
    const auto encoded=encode_temporal_segment(s),seed=encode_temporal_endpoint(e);
    for(const auto cut:std::vector<std::size_t>{0,31,32,seed.size()/2,seed.size()-1})
        rejects([&]{(void)decode_temporal_endpoint(e.source,std::span<const std::byte>(seed).first(cut));},"truncated finite seed accepted");
    auto broken=encoded;broken[12]=std::byte{2};
    rejects([&]{(void)decode_temporal_segment(broken);},"unknown segment feature accepted");
    broken=encoded;broken[12]=std::byte{0};
    rejects([&]{(void)decode_temporal_segment(broken);},"finite suffix accepted without flag");
    broken=seed;broken[12]=std::byte{2};
    rejects([&]{(void)decode_temporal_endpoint(e.source,broken);},"unknown seed feature accepted");
    auto limits=TemporalReadLimits{};limits.max_records=4;
    rejects([&]{(void)decode_temporal_endpoint(e.source,seed,limits);},"finite nested record budget ignored");
    std::mt19937 random(20260927);limits.max_records=10000;limits.max_dimension=1000;limits.max_integer_bytes=1024;
    std::size_t accepted=0;
    for(std::size_t trial=0;trial<500;++trial){
        auto mutation=encoded;const auto pos=32+random()%(mutation.size()-32);mutation[pos]^=static_cast<std::byte>(1u<<(random()%8));rehash(mutation);
        std::optional<std::uint64_t> hash;
        try {hash=temporal_endpoint_hash(replay_temporal_segment(e,decode_temporal_segment(mutation,limits)));}catch(const std::exception&){}
        if(hash){++accepted;check(*hash==temporal_endpoint_hash(next),"rehash mutation published wrong finite endpoint");}
    }
    check(temporal_endpoint_hash(e)==original_hash,"failed finite replay modified base");
    std::cout<<"finite_rehashed_mutations=500 accepted_same_endpoint="<<accepted<<'\n';
}
TemporalEndpoint sheaf_endpoint(bool changed) {
    core::RelationSignature sig;const auto rt=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(sig);
    builder.add_grade_one(core::RelationKey{100},rt,{
        {10,true,!changed,changed?R{3}:R{2},{},core::OccurrenceKey{1000}},
        {11,true,changed,changed?R{3}:R{2},{},core::OccurrenceKey{1001}}});
    TemporalEndpoint e(builder.freeze(),changed?1:0);
    e.carriers={{1,TemporalCarrierKind::CellStalk,{0},vertex(10),11,numeric::IndexDomain::linear(changed?2:1)},
                {2,TemporalCarrierKind::CellStalk,{0},vertex(11),12,numeric::IndexDomain::linear(1)},
                {3,TemporalCarrierKind::CellStalk,{1},edge(100),13,numeric::IndexDomain::linear(changed?3:2)}};
    const auto add=[&](std::uint64_t key,TemporalTensorRole role,std::uint64_t occurrence,std::uint64_t in,std::uint64_t out,M map){
        TemporalTensor t;t.key=key;t.role=role;t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={{1},{occurrence}};
        t.input=in;t.output=out;t.tensor=temporal_matrix_tensor(map);e.tensors.push_back(std::move(t));
    };
    add(10,TemporalTensorRole::SheafRestriction,1000,1,3,changed?M::from_rows({{1,7},{2,8},{5,9}}):M::from_rows({{1},{2}}));
    add(11,TemporalTensorRole::SheafRestriction,1001,2,3,changed?M::from_rows({{3},{4},{6}}):M::from_rows({{3},{4}}));
    add(20,TemporalTensorRole::RealizedIncidenceMap,1000,3,1,changed?M::from_rows({{9,10,11},{12,13,14}}):M::from_rows({{9,10}}));
    add(21,TemporalTensorRole::CoefficientIndependentMap,1001,3,2,changed?M::from_rows({{2,3,4}}):M::from_rows({{2,3}}));
    return e;
}
void sheaf_roundtrip() {
    const auto old=sheaf_endpoint(false),next=sheaf_endpoint(true);
    const std::vector<TemporalCorrespondence> maps{{3,M::from_rows({{1,0},{0,1},{0,0}})},{1,M::from_rows({{1},{0}})}};
    const auto restored=replay_temporal_segment(old,decode_temporal_segment(encode_temporal_segment(make_temporal_segment(old,next,maps))));
    const auto old_view=temporal_linear_cellular_sheaf(old,{1,2,3});
    const auto view=temporal_linear_cellular_sheaf(restored,{1,2,3});
    check(equal(old_view.sheaf().coboundary({0}),M::from_rows({{-2,6},{-4,8}})),"old upward incidence weighted incorrectly");
    const auto expected=M::from_rows({{3,21,-9},{6,24,-12},{15,27,-18}});
    check(equal(view.sheaf().coboundary({0}),expected),"replayed upward map weighted twice or wrong direction");
    check(equal(view.sheaf().restriction({1},0),M::from_rows({{1,7},{2,8},{5,9}})),"new-direction sheaf restriction lost");
    check(equal(temporal_tensor_map_defect(old,restored,10,maps),M::from_rows({{0},{0},{5}})),"rectangular restriction defect wrong");
    const auto fibers=temporal_linear_fiber_system(restored,{1,2,3});
    check(equal(fibers.flag_map({1},0).transport,M::from_rows({{9,10,11},{12,13,14}})),"realized downward map reweighted");
    check(equal(fibers.flag_map({1},1).transport,M::from_rows({{-6,-9,-12}})),"coefficient-independent downward map not realized");
    check(!view.sheaf().is_local_system(),"rectangular sheaf incorrectly claimed invertible");
    std::optional<TemporalLinearCellularSheaf> retained;
    {auto temporary=temporal_linear_cellular_sheaf(restored,{1,2,3});retained=temporary;}
    check(equal(retained->sheaf().coboundary({0}),expected),"copied sheaf borrowed destroyed stalk bundle");
    auto moved=std::move(*retained);retained.reset();
    check(equal(moved.sheaf().coboundary({0}),expected),"moved sheaf borrowed moved-from bundle");
    auto assigned=temporal_linear_cellular_sheaf(old,{1,2,3});assigned=std::move(moved);
    check(equal(assigned.sheaf().coboundary({0}),expected),"move-assigned sheaf retained stale bundle");
    auto bad=old;auto duplicate=bad.tensors[0];duplicate.key=100;bad.tensors.push_back(duplicate);
    rejects([&]{validate_temporal_endpoint(bad);},"duplicate upward restriction accepted");
    bad=old;std::swap(bad.tensors[0].input,bad.tensors[0].output);
    rejects([&]{validate_temporal_endpoint(bad);},"upward restriction with downward carriers accepted");
    bad=old;bad.tensors.erase(bad.tensors.begin());
    rejects([&]{(void)temporal_linear_cellular_sheaf(bad,{1,2,3});},"undeclared rectangular restriction invented");
    rejects([&]{(void)temporal_linear_cellular_sheaf(old,{1,2});},"missing selected stalk accepted");
    rejects([&]{(void)temporal_linear_cellular_sheaf(old,{1,2,3,3});},"duplicate selected stalk accepted");
}
void legacy_and_append(const std::filesystem::path& dir) {
    const auto fixture=std::filesystem::path(RFC_TEST_SOURCE_DIR)/"tests/storage/fixtures/phase03_v1";
    MappedRfcx source(fixture/"base.rfcx");const auto seed=bytes_at(fixture/"base.rfcxe"),segment_bytes=bytes_at(fixture/"history.rfcxt");
    const auto base=read_temporal_endpoint(source,fixture/"base.rfcxe");
    const auto segment=decode_temporal_segment(segment_bytes);
    check(base.finite_systems.empty() && segment.finite_systems.empty(),"legacy fixture gained finite channels");
    check(encode_temporal_endpoint(base)==seed,"legacy seed bytes/binding changed");
    check(encode_temporal_segment(segment)==segment_bytes,"legacy segment bytes changed");
    const auto target=replay_temporal_segment(base,segment);
    check(target.sequence==1 && temporal_endpoint_hash(target)==segment.target_endpoint_hash,"legacy source/seed/segment replay failed");
    TemporalEndpoint zero(triangle()),one(triangle(),1),two(triangle(),2);
    const auto first=make_temporal_segment(zero,one),second=make_temporal_segment(one,two);
    TemporalReadLimits limits;limits.max_records=1;
    const auto path=dir/"limited.rfcxt";
    append_temporal_segment(path,first,limits);const auto before=bytes_at(path);
    rejects([&]{append_temporal_segment(path,second,limits);},"append exceeded stream frame count");
    check(bytes_at(path)==before && read_temporal_segments(path,limits).size()==1,"rejected append modified readable history");
    limits.max_records=0;const auto no_path=dir/"zero-limit.rfcxt";
    rejects([&]{append_temporal_segment(no_path,first,limits);},"zero-frame limit ignored");
    check(!std::filesystem::exists(no_path),"rejected initial append created file");
}
void randomized_finite_and_correspondence() {
    std::mt19937 random(70302026);
    for (std::size_t trial=0; trial<64; ++trial) {
        TemporalEndpoint old(triangle(false,trial%3==0)),next(triangle(true,trial%3!=0),1);
        const auto build=[&](const TemporalEndpoint& e) {
            local::FiniteIntegerConstraintSystem system(e.source);
            for(auto v:{10u,11u,12u}) system.set_boolean_domain(resolve(e,vertex(v)));
            for(std::uint64_t i=0;i<3;++i) {
                const auto bits=1u+random()%15u;
                system.add_boolean_truth_chart("truth-"+std::to_string(i),system.carrier().cell_patch(resolve(e,edge(100+i))),
                    {resolve(e,vertex(10+i)),resolve(e,vertex(10+(i+1)%3))},bits);
            }
            return capture_temporal_finite_system(1,system,{4,5,6});
        };
        old.finite_systems.push_back(build(old));next.finite_systems.push_back(build(next));
        // Independent enumeration of the eight Boolean assignments.
        bool expected=false;
        for(unsigned bits=0;bits<8;++bits) {
            bool valid=true;
            for(const auto& chart:next.finite_systems[0].charts) {
                std::vector<TemporalFiniteValue> tuple;
                for(const auto cell:chart.support)tuple.push_back(integer((bits>>(cell.key-10))&1u));
                if(std::find(chart.allowed.begin(),chart.allowed.end(),tuple)==chart.allowed.end())valid=false;
            }
            expected=expected||valid;
        }
        for(std::uint16_t k=0;k<2;++k) {
            const auto key=static_cast<std::uint64_t>(k+1);
            old.carriers.push_back({key,TemporalCarrierKind::SourceGrade,{k},std::nullopt,0,numeric::IndexDomain::linear(old.source.grade({k}).cell_count())});
            next.carriers.push_back({key,TemporalCarrierKind::SourceGrade,{k},std::nullopt,0,numeric::IndexDomain::linear(next.source.grade({k}).cell_count())});
            const auto reference=temporal::RelationalSourceDelta(old.source,next.source).correspondence({k});
            check(equal(temporal_carrier_correspondence(old,next,key),reference),"indexed source J differs from native retained-source alignment");
            TemporalTensor a;a.key=key;a.role=TemporalTensorRole::Field;a.output=key;
            a.tensor={old.carriers.back().domain,numeric::StructuralSupport::full(reference.cols()),numeric::SparseRationalVector(reference.cols(),{{0,R{2}}})};
            auto b=a;b.tensor={next.carriers.back().domain,numeric::StructuralSupport::full(reference.rows()),temporal_endpoint_detail::sparse_apply(reference,a.tensor.values())};
            old.tensors.push_back(std::move(a));next.tensors.push_back(std::move(b));
        }
        const auto delta=make_temporal_segment(old,next);
        for(const auto& update:delta.tensors)check(update.innovation.nonzeros()==0,"finite/source change fabricated amplitude innovation");
        const auto encoded=encode_temporal_segment(delta);
        check(encode_temporal_segment(decode_temporal_segment(encoded))==encoded,"random finite encoding not canonical");
        const auto restored=replay_temporal_segment(old,decode_temporal_segment(encoded));
        check(temporal_endpoint_hash(restored)==temporal_endpoint_hash(next),"random finite endpoint replay failed");
        const auto native=temporal_finite_constraint_system<std::int64_t>(restored,1);
        check(native.global_section().has_value()==expected,"native finite reconstruction disagrees with independent exhaustive search");
        for(std::size_t i=0;i<restored.tensors.size();++i)
            check(temporal_endpoint_detail::tensor_equal(restored.tensors[i],next.tensors[i]),"cached correspondence changed exact field transport");
    }
}
void mixed_feature_history(const std::filesystem::path& dir) {
    TemporalEndpoint plain(triangle());auto finite=plain;finite.sequence=1;finite.finite_systems.push_back(integer_system(finite,false));
    auto idle=finite;idle.sequence=2;auto changed=idle;changed.sequence=3;changed.finite_systems[0].charts[0].name="retained-name-change";
    auto removed=changed;removed.sequence=4;removed.finite_systems.clear();
    const std::vector<TemporalEndpoint> endpoints{plain,finite,idle,changed,removed};
    const auto path=dir/"mixed.rfcxt";
    for(std::size_t i=1;i<endpoints.size();++i) {
        const auto segment=make_temporal_segment(endpoints[i-1],endpoints[i]);
        const auto bytes=encode_temporal_segment(segment);
        check(bytes[12]==(i==2?std::byte{0}:std::byte{1}),"feature bit does not match actual finite delta records");
        append_temporal_segment(path,endpoints[i-1],segment);
    }
    auto reconstructed=plain;const auto frames=read_temporal_segments(path);
    check(frames.size()==4,"mixed legacy/finite stream lost frames");
    for(std::size_t i=0;i<frames.size();++i) {
        reconstructed=replay_temporal_segment(reconstructed,frames[i]);
        check(temporal_endpoint_hash(reconstructed)==temporal_endpoint_hash(endpoints[i+1]),"mixed-feature lineage or replay failed");
    }
    check(reconstructed.finite_systems.empty(),"finite channel removal not reflected in legacy-compatible endpoint");
}

} // namespace
int main() {
    const auto dir=std::filesystem::temp_directory_path()/("rfc-temporal-native-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {std::filesystem::create_directory(dir);finite_roundtrip(dir);finite_rejections();sheaf_roundtrip();legacy_and_append(dir);randomized_finite_and_correspondence();mixed_feature_history(dir);
        std::filesystem::remove_all(dir);std::cout<<"temporal_native_checks="<<checks<<" PASS\n";return 0;
    }catch(const std::exception& e){std::filesystem::remove_all(dir);std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
