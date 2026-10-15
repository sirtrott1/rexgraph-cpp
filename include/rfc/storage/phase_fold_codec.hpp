#pragma once
#include "rfc/storage/phase_fold_archive.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"

namespace rfc::storage {
struct PhaseFoldLimits {
    AlgebraicArchiveLimits wire{};
    temporal::ExactHistoryLimits history{};
    topology::CyclicCoverLimits cover{};
    geometry::PanelAssemblyLimits panels{};
    std::size_t max_proposals{128};
};
namespace phase_fold_codec_detail {
using namespace algebraic_archive_detail;
inline constexpr std::array<std::byte,8> word_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'W'},std::byte{0},std::byte{0},std::byte{0}};
inline AlgebraicArchiveLimits source_policy(const PhaseFoldLimits& limits,std::uint8_t kind) {
    auto wire=limits.wire;
    if (kind<=3) {
        wire.modules.max_cells=std::min(wire.modules.max_cells,limits.history.max_source_cells);
        wire.fibers.max_occurrences=std::min(wire.fibers.max_occurrences,limits.history.max_source_occurrences);
        if (kind==3) {
            wire.modules.max_cells=std::min(wire.modules.max_cells,limits.panels.max_cells);
            wire.fibers.max_occurrences=std::min(wire.fibers.max_occurrences,limits.panels.max_occurrences);
        }
    } else {
        const auto cells=machine::checked_size_mul(limits.cover.max_vertices,2);
        wire.modules.max_cells=std::min(wire.modules.max_cells,cells);wire.fibers.max_occurrences=std::min(wire.fibers.max_occurrences,cells);
    }
    return wire;
}
inline void phase(Writer& w,const numeric::RationalPhase& p,Admission& a) {w.rational(p.real(),a);w.rational(p.imaginary(),a);}
inline numeric::RationalPhase phase(Reader& r) {const auto real=r.rational(),imaginary=r.rational();return {real,imaginary};}
inline void phase(Writer& w,const numeric::AlgebraicUnitPhase& p,Admission& a) {element(w,p.value(),a);}
inline numeric::AlgebraicUnitPhase phase(Reader& r,const numeric::AlgebraicInvolution& involution) {return {involution,element(r,involution.field())};}
inline void domain(Writer& w,const numeric::AlgebraicUnitPhase& p,Admission& a) {
    write_field(w,p.involution().field(),a);element(w,p.involution().generator_image(),a);
}
inline numeric::AlgebraicInvolution domain(Reader& r,Admission& a) {
    const auto field=read_field(r,a);return numeric::AlgebraicInvolution::declare(field,element(r,field));
}
inline void frame(Writer& w,const geometry::RationalRigidFrame& f,Admission& a) {
    for (std::size_t i=0;i<3;++i) for (std::size_t j=0;j<4;++j) w.rational(f.affine().get(i,j),a);
}
inline geometry::RationalRigidFrame frame(Reader& r) {
    auto affine=numeric::ExactMatrix::identity(4);
    for (std::size_t i=0;i<3;++i) for (std::size_t j=0;j<4;++j) affine.set(i,j,r.rational());
    return geometry::RationalRigidFrame::from_affine(affine);
}
template<temporal::ExactPhase P> inline void history(Writer& w,const temporal::PhaseOperationHistory<P>& h,Admission& a,const PhaseFoldLimits& limits) {
    bound(h.operations().size(),limits.history.max_steps,"phase word step budget");
    const auto& initial=h.phase_at(0);
    if constexpr (std::same_as<P,numeric::AlgebraicUnitPhase>) domain(w,initial,a);
    cell(w,h.cell());phase(w,initial,a);w.u64(h.operations().size());
    temporal::history_detail::admit_source(h.source(),limits.history);
    const auto slots=machine::checked_size_mul(machine::checked_size_add(h.operations().size(),1),temporal::history_detail::action(initial).rows());
    bound(slots,limits.history.max_coordinate_slots,"phase word endpoint coordinate budget");
    for (std::size_t i=0;i<=h.operations().size();++i) temporal::history_detail::admit(temporal::history_detail::action(h.phase_at(i)),limits.history);
    for (const auto& op:h.operations()) {
        temporal::history_detail::admit(temporal::history_detail::action(op.phase),limits.history);
        w.u64(op.identity);w.u8(op.active);phase(w,op.phase,a);
    }
}
template<temporal::ExactPhase P,class ReadPhase> inline PhaseFoldArchive history(Reader& r,const core::RelationalComplex& source,ReadPhase read_phase,const PhaseFoldLimits& limits) {
    const auto grade=r.u16();const auto index=r.u32();const auto initial=read_phase();
    auto h=temporal::PhaseOperationHistory<P>::begin(source,{core::Grade{grade},index},initial,limits.history);
    const auto n=r.count(limits.history.max_steps,9);
    for (std::size_t i=0;i<n;++i) {
        const auto identity=r.u64();const auto active=r.flag();const auto value=read_phase();h=h.append({identity,value,active});
    }
    return PhaseFoldArchive::declare(h);
}
inline void fold(Writer& w,const temporal::RationalFoldHistory& h,Admission& a,const PhaseFoldLimits& limits) {
    const auto& assembly=h.assembly();
    temporal::history_detail::admit_source(assembly.source(),limits.history);
    (void)geometry::RationalPanelAssembly::declare(assembly.source(),assembly.vertices(),assembly.frames(),assembly.creases(),limits.panels);
    bound(h.operations().size(),limits.history.max_steps,"fold word step budget");
    const auto slots=machine::checked_size_mul(machine::checked_size_mul(machine::checked_size_add(h.operations().size(),1),h.poses().size()),16);
    bound(slots,limits.history.max_coordinate_slots,"fold word endpoint coordinate budget");
    w.u64(assembly.vertices().size());
    for (const auto& vertex:assembly.vertices()) for (const auto& q:vertex) w.rational(q,a);
    w.u64(assembly.frames().size());for (const auto& f:assembly.frames()) frame(w,f,a);
    w.u64(assembly.creases().size());for (const auto& crease:assembly.creases()) {
        w.u32(crease.edge);w.u32(crease.fixed_panel);w.u32(crease.moving_panel);frame(w,crease.reference_frame,a);
    }
    w.u64(h.operations().size());
    for (std::size_t i=0;i<=h.operations().size();++i) for (const auto& pose:h.poses_at(i)) temporal::history_detail::admit(pose.affine(),limits.history);
    for (const auto& op:h.operations()) {
        temporal::history_detail::admit(op.phase.real_action(),limits.history);
        bound(op.moving_panels.size(),limits.history.max_source_cells,"fold word moving group budget");
        w.u64(op.identity);w.u64(op.crease);w.u8(op.active);phase(w,op.phase,a);
        w.u64(op.moving_panels.size());for (const auto panel:op.moving_panels) w.u32(panel);
    }
}
inline PhaseFoldArchive fold(Reader& r,const core::RelationalComplex& source,const PhaseFoldLimits& limits) {
    std::vector<numeric::RationalVector> vertices;const auto nv=r.count(limits.panels.max_cells,33);vertices.reserve(nv);
    for (std::size_t i=0;i<nv;++i) {numeric::RationalVector v;for (std::size_t j=0;j<3;++j) v.push_back(r.rational());vertices.push_back(std::move(v));}
    std::vector<geometry::RationalRigidFrame> frames;const auto nf=r.count(limits.panels.max_cells,132);frames.reserve(nf);
    for (std::size_t i=0;i<nf;++i) frames.push_back(frame(r));
    std::vector<geometry::PanelCrease> creases;const auto nc=r.count(limits.panels.max_creases,144);creases.reserve(nc);
    for (std::size_t i=0;i<nc;++i) {
        const auto edge=r.u32(),fixed=r.u32(),moving=r.u32();auto f=frame(r);creases.push_back({edge,fixed,moving,std::move(f)});
    }
    auto h=temporal::RationalFoldHistory::begin(geometry::RationalPanelAssembly::declare(source,std::move(vertices),std::move(frames),std::move(creases),limits.panels),limits.history);
    const auto n=r.count(limits.history.max_steps,47);
    for (std::size_t i=0;i<n;++i) {
        const auto identity=r.u64(),crease=r.u64();const auto active=r.flag();const auto value=phase(r);
        std::vector<std::uint32_t> moving;const auto nm=r.count(limits.history.max_source_cells,4);moving.reserve(nm);
        for (std::size_t j=0;j<nm;++j) moving.push_back(r.u32());
        h=h.append({identity,machine::checked_size(crease),std::move(moving),value,active});
    }
    return PhaseFoldArchive::declare(h);
}
template<temporal::ExactPhase P> inline void walk(Writer& w,const temporal::CyclicPhaseWalk<P>& h,Admission& a,const PhaseFoldLimits& limits) {
    const auto& lift=h.lift();const auto& cover=lift.cover();
    bound(cover.period(),limits.cover.max_vertices,"phase walk vertex budget");
    bound(lift.path().steps().size(),limits.cover.max_path_steps,"phase walk step budget");
    for (const auto& position:lift.positions())
        bound(mpz_sizeinbase(position.get_mpz_t(),2),limits.cover.max_integer_bits,"phase walk lift bit budget");
    if constexpr (std::same_as<P,numeric::AlgebraicUnitPhase>) domain(w,h.generator(),a);
    phase(w,h.generator(),a);w.u64(cover.period());w.u64(cover.branch_vertex());
    const auto& arena=cover.source().grade(core::Grade{0});
    for (std::size_t i=0;i<cover.period();++i) w.u64(arena.identities[topology::pairwise_endpoints(cover.source(),i).tail]);
    w.integer(lift.positions().front(),a.limits);w.u32(lift.path().source());w.u64(lift.path().steps().size());
    for (const auto& step:lift.path().steps()) {w.u32(step.relation);w.u8(step.forward);}
}
template<temporal::ExactPhase P> inline PhaseFoldArchive walk(Reader& r,const core::RelationalComplex& source,P generator,const PhaseFoldLimits& limits) {
    const auto n=r.count(limits.cover.max_vertices,8);const auto branch=machine::checked_size(r.u64());
    std::vector<core::IdentityKey> vertices;vertices.reserve(n);for (std::size_t i=0;i<n;++i) vertices.push_back(r.u64());
    const auto cover=topology::InfiniteCyclicCover::declare(vertices,branch,limits.cover);
    require(encode_rfcx(cover.source()).bytes==encode_rfcx(source).bytes,"phase walk source must be its complete declared polygon");
    const auto initial=r.integer();const auto start=r.u32();const auto ns=r.count(limits.cover.max_path_steps,5);
    std::vector<topology::EdgeStep> steps;steps.reserve(ns);
    for (std::size_t i=0;i<ns;++i) {const auto edge=r.u32();const auto forward=r.flag();steps.push_back({edge,forward});}
    return PhaseFoldArchive::declare(temporal::CyclicPhaseWalk<P>(cover.lift(topology::PairwisePath(cover.source(),start,std::move(steps)),initial),std::move(generator)));
}
} // namespace phase_fold_codec_detail

inline std::vector<std::byte> encode_phase_fold_archive(const PhaseFoldArchive& archive,const PhaseFoldLimits& limits={}) {
    using namespace phase_fold_codec_detail;
    const auto wire=source_policy(limits,static_cast<std::uint8_t>(archive.word().index()+1));
    Admission admission(wire);Writer writer(wire.max_file_bytes-32);
    const auto source=encode_rfcx(archive.source()).bytes;source_preflight(source,admission);writer.blob(source);
    writer.u8(static_cast<std::uint8_t>(archive.word().index()+1));
    std::visit([&](const auto& h) {
        using T=std::decay_t<decltype(h)>;
        if constexpr (std::same_as<T,temporal::RationalFoldHistory>) fold(writer,h,admission,limits);
        else if constexpr (std::same_as<T,RationalPhaseWalk> || std::same_as<T,AlgebraicPhaseWalk>) walk(writer,h,admission,limits);
        else history(writer,h,admission,limits);
    },archive.word());
    bound(archive.proposals().size(),limits.max_proposals,"phase/fold prospective table budget");writer.u64(archive.proposals().size());
    for (const auto& p:archive.proposals()) {
        writer.u64(p.key);writer.u64(p.position);writer.u8(p.active);
        std::visit([&](const auto& value){temporal::history_detail::admit(temporal::history_detail::action(value),limits.history);phase(writer,value,admission);},p.replacement);
    }
    const auto payload=writer.take();Writer envelope(limits.wire.max_file_bytes);envelope.raw(word_magic);envelope.u16(1);envelope.u16(32);envelope.u32(0);
    envelope.u64(machine::checked_size_add(payload.size(),32));envelope.u64(rfcx_detail::fnv1a64(payload));envelope.raw(payload);return envelope.take();
}
inline PhaseFoldArchive decode_phase_fold_archive(std::span<const std::byte> bytes,const PhaseFoldLimits& limits={}) {
    using namespace phase_fold_codec_detail;
    auto wire=limits.wire;Admission admission(wire);bound(bytes.size(),wire.max_file_bytes,"phase/fold file byte budget");
    require(bytes.size()>=32 && std::ranges::equal(bytes.first(8),word_magic),"invalid phase/fold archive header");
    Reader header(bytes.subspan(8,24),admission);
    require(header.u16()==1 && header.u16()==32 && header.u32()==0,"unsupported phase/fold version or flags");
    require(header.u64()==bytes.size(),"phase/fold length differs");require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"phase/fold checksum differs");
    // Peek only bounded spans, then apply the selected source budget before
    // materializing any RFCX integers or validating its chain family.
    Reader peek(bytes.subspan(32),admission);(void)peek.blob();const auto kind=peek.u8();
    require(kind>=1 && kind<=5,"unsupported phase/fold word kind");wire=source_policy(limits,kind);
    Reader reader(bytes.subspan(32),admission);const auto source=read_source(reader,admission);(void)reader.u8();
    auto result=[&]() -> PhaseFoldArchive {
        if (kind==1) return history<numeric::RationalPhase>(reader,source,[&]{return phase(reader);},limits);
        if (kind==2) {const auto involution=domain(reader,admission);return history<numeric::AlgebraicUnitPhase>(reader,source,[&]{return phase(reader,involution);},limits);}
        if (kind==3) return fold(reader,source,limits);
        if (kind==4) {const auto generator=phase(reader);return walk(reader,source,generator,limits);}
        if (kind==5) {const auto involution=domain(reader,admission);const auto generator=phase(reader,involution);return walk(reader,source,generator,limits);}
        throw std::invalid_argument("unsupported phase/fold word kind");
    }();
    const auto n=reader.count(limits.max_proposals,17);
    require(n==0 || kind<=3,"cyclic path proposal profile is not declared");
    for (std::size_t i=0;i<n;++i) {
        const auto key=reader.u64();const auto position=machine::checked_size(reader.u64());const auto active=reader.flag();
        if (kind==2) {
            const auto& h=std::get<temporal::AlgebraicPhaseHistory>(result.word());const auto p=phase(reader,h.phase_at(0).involution());result=result.propose(key,position,p,active);
        } else {const auto p=phase(reader);result=result.propose(key,position,p,active);}
    }
    require(reader.remaining()==0,"trailing phase/fold archive bytes");
    require(std::ranges::equal(encode_phase_fold_archive(result,limits),bytes),"noncanonical phase/fold archive image");return result;
}
inline void write_phase_fold_archive(const PhaseFoldArchive& archive,const std::filesystem::path& path,
    FilePublication publication=FilePublication::ReplaceRegular,const PhaseFoldLimits& limits={}) {
    const auto bytes=encode_phase_fold_archive(archive,limits);(void)decode_phase_fold_archive(bytes,limits);write_file_exact(path,bytes,publication);
}
inline PhaseFoldArchive read_phase_fold_archive(const std::filesystem::path& path,const PhaseFoldLimits& limits={}) {
    return decode_phase_fold_archive(read_file_exact(path,limits.wire.max_file_bytes),limits);
}
} // namespace rfc::storage
