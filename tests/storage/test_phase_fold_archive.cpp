#include "../../examples/phase_fold_storage/model.hpp"
#include "rfc/storage/phase_fold_history.hpp"
#include <iostream>

using namespace phase_fold_example;
using namespace rfc::storage;
static std::size_t checks{};
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {
    bool failed=false;try {action();} catch (const std::exception&) {failed=true;}check(failed,message);
}
static void reseal(std::vector<std::byte>& bytes) {
    const auto size=static_cast<std::uint64_t>(bytes.size()),hash=rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for (std::size_t i=0;i<8;++i) {bytes[16+i]=std::byte((size>>(8*i))&255);bytes[24+i]=std::byte((hash>>(8*i))&255);}
}
static std::size_t kind_offset(const std::vector<std::byte>& bytes) {
    rfcx_detail::Reader r(bytes,32);const auto size=rfc::machine::checked_size(r.u64());return 40+size;
}
static void roundtrips(const std::filesystem::path& fixtures) {
    for (const auto& [name,archive]:samples()) {
        const auto bytes=encode_phase_fold_archive(archive);const auto restored=decode_phase_fold_archive(bytes);
        check(encode_phase_fold_archive(restored)==bytes,"canonical phase/fold roundtrip differs");
        check(encode_rfcx(restored.source()).bytes==encode_rfcx(archive.source()).bytes,"source occurrences or attaching order changed");
        check(restored.word().index()==archive.word().index() && restored.operation_count()==archive.operation_count(),"word kind or original length changed");
        check(restored.proposals().size()==archive.proposals().size() && !restored.same_owner(archive),"pending table or fresh archive owner differs");
        const auto again=decode_phase_fold_archive(bytes);check(!again.same_owner(restored),"decode interns archive by equal bytes");
        for (std::size_t prefix=0;prefix<=restored.operation_count();++prefix) {
            const auto cut=restored.observed_prefix(prefix);check(cut.operation_count()==prefix && cut.proposals().empty(),"observation retains prospective changes or future operations");
            check(encode_phase_fold_archive(cut)==encode_phase_fold_archive(archive.observed_prefix(prefix)),"selected prefix depends on a suffix");
            rejects([&]{(void)cut.observed_prefix(prefix+1);},"prefix can read future operation");
            check(encode_phase_fold_archive(decode_phase_fold_archive(encode_phase_fold_archive(cut)))==encode_phase_fold_archive(cut),"prefix roundtrip differs");
        }
        if (!fixtures.empty()) check(read_file_exact(fixtures/(name+".rfcxw"),16u<<20)==bytes,"frozen RFCXW v1 fixture differs");
        // Every byte truncation and every unsuppressed one-bit corruption rejects.
        for (std::size_t i=0;i<bytes.size();++i) {
            rejects([&]{(void)decode_phase_fold_archive(std::span<const std::byte>(bytes).first(i));},"truncated phase/fold image accepted");
            auto bad=bytes;bad[i]^=std::byte{1};rejects([&]{(void)decode_phase_fold_archive(bad);},"corrupted phase/fold image accepted");
        }
        auto bad=bytes;bad[kind_offset(bytes)]=std::byte{6};reseal(bad);rejects([&]{(void)decode_phase_fold_archive(bad);},"unimplemented word kind accepted");
        bad=bytes;bad.push_back(std::byte{0});reseal(bad);rejects([&]{(void)decode_phase_fold_archive(bad);},"trailing word data accepted");
        PhaseFoldLimits tiny;tiny.wire.max_file_bytes=32;
        rejects([&]{(void)decode_phase_fold_archive(bytes,tiny);},"file budget ignored");
        rejects([&]{(void)encode_phase_fold_archive(archive,tiny);},"encode file budget ignored");
        tiny={};tiny.wire.max_rational_coordinates=1;rejects([&]{(void)decode_phase_fold_archive(bytes,tiny);},"source preflight aggregate budget ignored");
    }
}
static void phases() {
    const auto original=samples()[0].second;const auto bytes=encode_phase_fold_archive(original);const auto restored=decode_phase_fold_archive(bytes);
    const auto& old=std::get<temporal::RationalPhaseHistory>(restored.word());const auto branch=restored.apply(7);
    const auto& changed=std::get<temporal::RationalPhaseHistory>(branch.word());
    check(old.same_carrier(changed) && !old.same_history(changed) && branch.proposals().empty(),"applying proposal does not create a separate replayed branch");
    check(old.operations()[0].identity==91 && old.operations()[1].identity==91 && old.operations()[2].identity==0 && !old.operations()[2].active,"repeated/zero identities or inactive operation erased");
    check(old.phase_at(0)==Phase::cayley(Q{1,2}) && old.endpoint()==old.phase_at(0),"initial phase not retained independently of composite");
    check(old.defect(changed).exact_identity() && temporal::sum_factor_interactions(old.attribution(changed),2,2)==changed.composite().add(old.composite().scale(Q{-1})),"restored complete finite interactions fail");
    const auto& independent=std::get<temporal::RationalPhaseHistory>(original.word());
    rejects([&]{(void)old.attribution(independent);},"restored equal source bytes identify an old carrier");
    rejects([&]{(void)restored.propose(7,1,Phase::identity(),true);},"duplicate prospective name accepted");
    rejects([&]{(void)restored.propose(0,1,Phase::identity(),true);},"null prospective name accepted");
    rejects([&]{(void)restored.propose(8,3,Phase::identity(),true);},"future prospective position accepted");
    rejects([&]{(void)branch.apply(7);},"prior-base proposal survives in new branch");
    rejects([&]{(void)restored.propose(8,0,root(),true);},"wrong coefficient proposal accepted");
    PhaseFoldLimits small;small.max_proposals=0;rejects([&]{(void)decode_phase_fold_archive(bytes,small);},"prospective table budget ignored");
    small={};small.history.max_steps=2;rejects([&]{(void)decode_phase_fold_archive(bytes,small);},"phase step budget ignored");
    small={};small.history.max_coordinate_slots=7;rejects([&]{(void)decode_phase_fold_archive(bytes,small);},"phase endpoint slot budget ignored");
    small={};small.history.max_integer_bits=2;rejects([&]{(void)decode_phase_fold_archive(bytes,small);},"phase integer budget ignored");
    auto bad=bytes;AlgebraicArchiveLimits policy;algebraic_archive_detail::Admission admission(policy);
    algebraic_archive_detail::Reader r(std::span<const std::byte>(bytes).subspan(kind_offset(bytes)+1),admission);
    (void)r.u16();(void)r.u32();(void)r.rational();(void)r.rational();(void)r.u64();(void)r.u64();
    const auto flag=bytes.size()-r.remaining();bad[flag]=std::byte{2};reseal(bad);rejects([&]{(void)decode_phase_fold_archive(bad);},"nonboolean activity accepted after checksum repair");
    const auto algebraic=decode_phase_fold_archive(encode_phase_fold_archive(samples()[1].second));
    const auto& a=std::get<temporal::AlgebraicPhaseHistory>(algebraic.word());const auto edited=algebraic.apply(7);
    const auto& b=std::get<temporal::AlgebraicPhaseHistory>(edited.word());
    check(a.same_carrier(b) && a.phase_at(0).involution().same_owner(b.endpoint().involution()),"algebraic edit loses actual declared involution");
    check(a.endpoint().value().coordinates()==V({1,0}) && b.endpoint().value().coordinates()==V({-1,1}),"algebraic replay changes phase algebra");
    check(a.defect(b).exact_identity() && a.attribution(b).size()==7,"algebraic full word defects unavailable after restore");
    const auto foreign=root();rejects([&]{(void)algebraic.propose(8,0,foreign,false);},"inactive prospective unit interns independent field owner");
    const auto& f=a.phase_at(0).involution().field();
    const auto equal_involution=rfc::numeric::AlgebraicInvolution::declare(f,f.scalar(1)-f.generator());
    rejects([&]{(void)algebraic.propose(8,0,rfc::numeric::AlgebraicUnitPhase(equal_involution,f.generator()),false);},"equal involution image identifies distinct owner");
    const auto cubic=rfc::numeric::CertifiedAlgebraicField::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto involution=rfc::numeric::AlgebraicInvolution::identity(cubic);
    const auto h=temporal::AlgebraicPhaseHistory::begin(a.source(),a.cell(),rfc::numeric::AlgebraicUnitPhase(involution,cubic.scalar(-1)));
    const auto c=decode_phase_fold_archive(encode_phase_fold_archive(Archive::declare(h)));
    check(std::get<temporal::AlgebraicPhaseHistory>(c.word()).endpoint().value().coordinates()==V({-1,0,0}),"Eisenstein field or declared identity involution replaced by complex convention");
}
static void folds() {
    const auto original=samples()[2].second;const auto restored=decode_phase_fold_archive(encode_phase_fold_archive(original));
    const auto& old=std::get<temporal::RationalFoldHistory>(restored.word());const auto branch=restored.apply(7);
    const auto& edited=std::get<temporal::RationalFoldHistory>(branch.word());
    check(old.operations()[0].moving_panels==std::vector<std::uint32_t>({2,1}),"moving group order was sorted in storage");
    check(!old.operations()[2].active && old.operations()[2].phase==old.operations()[0].phase,"inactive fold value lost");
    check(old.same_carrier(edited) && old.assembly().same_owner(edited.assembly()),"restored branch replaces assembly owner");
    const auto terms=old.attribution(edited,2);
    check(terms.size()==7 && temporal::sum_factor_interactions(terms,4,4)==edited.poses()[2].affine().add(old.poses()[2].affine().scale(Q{-1})),"fold replay truncates interactions");
    check(terms[2].contribution.nonzeros()>0 && old.defect(edited,2).exact_identity(),"upstream edit did not replay downstream world crease");
    const auto early=restored.observed_prefix(0);check(std::get<temporal::RationalFoldHistory>(early.word()).poses()[2].affine()==M::identity(4),"future fold pose leaked into zero prefix");
    const auto empty=Archive::declare(temporal::RationalFoldHistory::begin(panels()));
    check(decode_phase_fold_archive(encode_phase_fold_archive(empty)).operation_count()==0,"empty fold word not admitted");
    PhaseFoldLimits tiny;tiny.panels.max_creases=1;rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(original),tiny);},"crease budget ignored");
    tiny={};tiny.history.max_coordinate_slots=191;rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(original),tiny);},"fold endpoint coordinate budget ignored");
    auto translated=panels();auto vertices=translated.vertices();for (auto& v:vertices) {v[0]+=2;v[1]-=3;v[2]+=5;}
    const Pose shift(M::identity(3),{2,-3,5});auto frames=translated.frames();for (auto& f:frames) f=shift.compose(f);
    auto creases=translated.creases();for (auto& crease:creases) crease.reference_frame=shift.compose(crease.reference_frame);
    const auto assembly=rfc::geometry::RationalPanelAssembly::declare(translated.source(),vertices,frames,creases);
    const auto h=temporal::RationalFoldHistory::begin(assembly).append({0,0,{1,2},Phase::cayley(Q{1,2}),true});
    const auto stored=decode_phase_fold_archive(encode_phase_fold_archive(Archive::declare(h)));
    check(std::get<temporal::RationalFoldHistory>(stored.word()).poses()[1].affine()==h.poses()[1].affine(),"translated affine frame was treated as its transpose");
}
static void walks() {
    for (std::size_t n=3;n<=9;++n) for (std::size_t branch=0;branch<n;++branch) for (int turns=-2;turns<=2;++turns) {
        std::vector<rfc::core::IdentityKey> ids;for (std::size_t i=0;i<n;++i) ids.push_back(1000+17*(n-i));
        const auto original=Archive::declare(walk(ids,branch,Phase::identity(),turns,-1000));
        const auto restored=decode_phase_fold_archive(encode_phase_fold_archive(original));const auto& h=std::get<RationalPhaseWalk>(restored.word());
        check(h.lift().winding()==turns && h.lift().path().steps().size()==n*static_cast<std::size_t>(turns<0 ? -turns : turns)+2,"cyclic original backtracks or signed winding lost");
        check(h.endpoint()==Phase::identity() && h.lift().positions().front()==-1000,"nonfaithful phase replaced the original lift");
        check(h.lift().cover().branch_vertex()==branch,"branch chart changed on restore");
    }
    const auto original=samples()[4].second;const auto restored=decode_phase_fold_archive(encode_phase_fold_archive(original));
    const auto& h=std::get<AlgebraicPhaseWalk>(restored.word());
    const auto endpoint=h.endpoint();check(endpoint.value().coordinates()==V({1,0}) && h.lift().winding()==-2,"same phase erased two negative windings");
    check(h.lift().path().steps().size()==14,"immediate inverse traversal reduced during storage");
    const auto cut=restored.observed_prefix(1);rejects([&]{(void)std::get<AlgebraicPhaseWalk>(cut.word()).lift().winding();},"open prefix acquired closed-path winding");
    rejects([&]{(void)restored.propose(1,0,root(),true);},"undeclared cyclic path editing accepted");
    PhaseFoldLimits tiny;tiny.cover.max_path_steps=13;rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(original),tiny);},"cover path budget ignored");
    tiny={};tiny.cover.max_vertices=5;rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(original),tiny);},"cover source budget ignored");
    tiny={};tiny.cover.max_integer_bits=32;
    rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(samples()[3].second),tiny);},"arbitrary lift integer budget ignored");
}
static void channels() {
    const auto archive=samples()[2].second;const PhaseFoldChannel channel{701,1,100};
    const auto first=with_phase_fold_archive(TemporalEndpoint(archive.source(),40),channel,archive);
    check(encode_phase_fold_archive(read_phase_fold_archive(first,channel))==encode_phase_fold_archive(archive),"native word channel differs");
    auto next=with_phase_fold_archive(first,channel,archive.apply(7));next.sequence=41;
    NativeTemporalSeries series(first,991);series.append(make_temporal_segment(first,next));
    const auto old=series.at(40),latest=series.at(41);
    check(encode_phase_fold_archive(read_phase_fold_archive(old.endpoint(),channel))==encode_phase_fold_archive(archive),"later applied proposal leaked into old channel");
    const auto applied=read_phase_fold_archive(latest.endpoint(),channel);check(applied.proposals().empty(),"applied snapshot inherits stale pending changes");
    auto damaged=next;auto bytes=encode_phase_fold_archive(archive);bytes.back()^=std::byte{1};
    damaged.declarations->values.front().value=algebraic_archive_history_detail::chunks(bytes);damaged.sequence=42;
    series.append(make_temporal_segment(next,damaged));
    const auto past=series.at(40),future=series.at(42);
    check(encode_phase_fold_archive(read_phase_fold_archive(past.endpoint(),channel))==encode_phase_fold_archive(archive),"selected past read inspects a corrupt future word");
    rejects([&]{(void)read_phase_fold_archive(future.endpoint(),channel);},"opaque future bytes considered a valid word");
    rejects([&]{(void)read_phase_fold_archive(first,{702,1,100});},"foreign channel namespace accepted");
    rejects([&]{(void)read_phase_fold_archive(first,{701,2,100});},"foreign channel declaration accepted");
    rejects([&]{(void)with_phase_fold_archive(TemporalEndpoint(rfc::algebra::oriented_polygon({1,2,3},rfc::algebra::PolygonFilling::BoundaryOnly)),channel,archive);},"different endpoint source accepted");
    auto extra=first;extra.declarations->values.push_back({101,channel.chunk_type(),0,0,{},{},rfc::composite::DeclaredValue::integer(45)});
    const auto retained=with_phase_fold_archive(extra,channel,archive.apply(7));
    const auto& value=retained.declarations->values.back();
    check(value.key==101 && value.type==channel.chunk_type() && std::get<rfc::numeric::Integer>(value.value.data)==45,"publishing word removed independent endpoint value");
}
static void semantic_corruption() {
    const auto cases=samples();AlgebraicArchiveLimits policy;algebraic_archive_detail::Admission admission(policy);
    auto bytes=encode_phase_fold_archive(cases[0].second);
    algebraic_archive_detail::Reader phase_reader(std::span<const std::byte>(bytes).subspan(kind_offset(bytes)+1),admission);
    (void)phase_reader.u16();(void)phase_reader.u32();
    auto bad=bytes;const auto real=bytes.size()-phase_reader.remaining();bad[real+5]=std::byte{2};reseal(bad);
    rejects([&]{(void)decode_phase_fold_archive(bad);},"nonunit initial phase accepted with correct checksum");
    bytes=encode_phase_fold_archive(cases[2].second);algebraic_archive_detail::Reader fold_reader(std::span<const std::byte>(bytes).subspan(kind_offset(bytes)+1),admission);
    const auto vertices=fold_reader.u64();for (std::size_t i=0;i<vertices*3;++i) (void)fold_reader.rational();
    (void)fold_reader.u64();const auto orientation=bytes.size()-fold_reader.remaining();bad=bytes;bad[orientation]=std::byte{1};reseal(bad);
    rejects([&]{(void)decode_phase_fold_archive(bad);},"improper reflected panel frame accepted with correct checksum");
    for (std::size_t i=0;i<3*12;++i) (void)fold_reader.rational();
    const auto creases=fold_reader.u64();for (std::size_t i=0;i<creases;++i) {
        (void)fold_reader.u32();(void)fold_reader.u32();(void)fold_reader.u32();for (std::size_t j=0;j<12;++j) (void)fold_reader.rational();
    }
    (void)fold_reader.u64();(void)fold_reader.u64();(void)fold_reader.u64();(void)fold_reader.flag();
    (void)fold_reader.rational();(void)fold_reader.rational();(void)fold_reader.u64();
    const auto group=bytes.size()-fold_reader.remaining();bad=bytes;
    std::copy_n(bad.begin()+static_cast<std::ptrdiff_t>(group),4,bad.begin()+static_cast<std::ptrdiff_t>(group+4));reseal(bad);
    rejects([&]{(void)decode_phase_fold_archive(bad);},"duplicate moving panel accepted with correct checksum");
    bytes=encode_phase_fold_archive(cases[3].second);algebraic_archive_detail::Reader walk_reader(std::span<const std::byte>(bytes).subspan(kind_offset(bytes)+1),admission);
    (void)walk_reader.rational();(void)walk_reader.rational();const auto n=walk_reader.u64();const auto branch=bytes.size()-walk_reader.remaining();
    bad=bytes;bad[branch]=std::byte{static_cast<unsigned char>(n)};reseal(bad);
    rejects([&]{(void)decode_phase_fold_archive(bad);},"invalid branch chart accepted with correct checksum");
    (void)walk_reader.u64();for (std::size_t i=0;i<n;++i) (void)walk_reader.u64();
    (void)walk_reader.integer();(void)walk_reader.u32();(void)walk_reader.u64();const auto edge=bytes.size()-walk_reader.remaining();
    bad=bytes;bad[edge]=std::byte{99};reseal(bad);
    rejects([&]{(void)decode_phase_fold_archive(bad);},"undeclared cyclic edge accepted with correct checksum");
    PhaseFoldLimits tiny;tiny.history.max_source_cells=1;
    rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(cases[0].second),tiny);},"history source budget not applied before replay");
    tiny={};tiny.wire.field.max_degree=1;
    rejects([&]{(void)decode_phase_fold_archive(encode_phase_fold_archive(cases[1].second),tiny);},"coefficient degree budget ignored");
}
int main(int argc,char** argv) {try {
    roundtrips(argc>1 ? std::filesystem::path(argv[1]) : std::filesystem::path{});phases();folds();walks();channels();semantic_corruption();
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
