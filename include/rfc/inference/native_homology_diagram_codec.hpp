#pragma once

#include "rfc/inference/native_homology_diagram.hpp"
#include "rfc/storage/prepared_byte_packet.hpp"

namespace rfc::inference {
namespace diagram_codec_detail {
using storage::temporal_codec_detail::Writer;
using storage::temporal_codec_detail::Reader;
inline constexpr std::array<std::byte,8> magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'H'},std::byte{'D'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr std::array<std::byte,8> proof_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'H'},std::byte{'P'},std::byte{1},std::byte{0},std::byte{0}};
template<class F> inline void bounded_write(storage::BytePacketBuilder& packet,std::size_t maximum,F&& write,const std::function<void()>& check) {
    if(check)check();
    auto measure=Writer::measure_only();write(measure);
    storage::shared_packet_detail::bound(measure.size(),maximum,"homology diagram component byte policy");
    packet.write(write);
}
inline void matrix(storage::BytePacketBuilder& w,const numeric::ExactMatrix& m,std::size_t maximum,const std::function<void()>& check) {
    bounded_write(w,maximum,[&](auto& raw){storage::temporal_codec_detail::matrix(raw,m);},check);
}
inline void matrix(storage::BytePacketBuilder& w,const numeric::IntegerMatrix& m,std::size_t maximum,const std::function<void()>& check) {
    bounded_write(w,maximum,[&](auto& raw){raw.u64(m.rows());raw.u64(m.cols());raw.u64(m.stored_entries());
        for(const auto& [ij,q]:m.entries()){raw.u64(ij.first);raw.u64(ij.second);storage::temporal_codec_detail::rational(raw,numeric::Rational{q});}},check);
}
inline void smith(storage::BytePacketBuilder& w,const numeric::SmithNormalFormActions& s,std::size_t maximum,const std::function<void()>& check) {
    matrix(w,s.diagonal,maximum,check);w.u64(s.rank);
    for(const auto* a:{&s.left,&s.right_inverse}){
        w.u64(a->dimension());w.u64(a->operations().size());
        for(const auto& op:a->operations())bounded_write(w,maximum,[&](auto& raw){raw.u8(static_cast<std::uint8_t>(op.kind));raw.u64(op.target);raw.u64(op.source);
            storage::temporal_codec_detail::rational(raw,numeric::Rational{op.multiple});},check);
    }
}
inline storage::PreparedBytePacket proof(const PreparedNativeHomologyDiagram& diagram,std::size_t maximum,const std::function<void()>& check) {
    storage::BytePacketBuilder w(proof_magic,maximum);
    if(diagram.declaration().product==HomologyDiagramProduct::RationalZigzag){
        const auto& d=diagram.rational();w.u64(d.homology().size());
        for(const auto& h:d.homology())for(const auto* m:{&h.cycles,&h.boundaries,&h.representatives,&h.quotient_frame})matrix(w,*m,maximum,check);
        w.u64(d.induced().size());for(const auto& a:d.induced())for(const auto* m:{&a.cycle_coordinates,&a.boundary_coordinates,&a.homology_map})matrix(w,*m,maximum,check);
        w.u64(d.zigzag().intervals().size());
        for(const auto& q:d.zigzag().intervals()){w.u64(q.begin);w.u64(q.end);w.u64(q.generators.size());for(const auto& m:q.generators)matrix(w,m,maximum,check);}
    }else{
        const auto& d=diagram.integral();w.u64(d.modules().size());
        for(const auto& h:d.modules()){
            matrix(w,h.cycle_basis,maximum,check);matrix(w,h.boundary_coordinates,maximum,check);
            smith(w,h.chain_smith,maximum,check);smith(w,h.boundary_smith,maximum,check);w.u64(h.free_rank);
            w.u64(h.torsion.size());for(const auto& q:h.torsion)bounded_write(w,maximum,[&](auto& raw){storage::temporal_codec_detail::rational(raw,numeric::Rational{q});},check);
        }
        w.u64(d.induced().size());for(const auto& a:d.induced())for(const auto* m:{&a.cycle_map,&a.smith_map,&a.relation_witness})matrix(w,*m,maximum,check);
    }
    return std::move(w).finish(check);
}
} // namespace diagram_codec_detail

inline storage::PreparedBytePacket prepare_native_homology_diagram(const PreparedNativeHomologyDiagram& diagram,
    std::size_t maximum,std::function<void()> check={}) {
    using namespace diagram_codec_detail;const auto& d=diagram.declaration();storage::BytePacketBuilder w(magic,maximum);
    w.u8(static_cast<std::uint8_t>(d.product));w.u64(d.grade.value);w.u64(d.vertices.size());
    for(const auto& v:d.vertices){if(check)check();const auto endpoint=v.publication.endpoint();
        w.u64(v.publication.source_namespace());const auto source=storage::encode_rfcx(endpoint->source);
        w.blob(source.bytes);const auto channels=storage::encode_temporal_endpoint(*endpoint);w.blob(channels);
        w.u64(v.grade_carriers.size());for(auto c:v.grade_carriers)w.u64(c);
    }
    w.u64(d.arrows.size());for(const auto& a:d.arrows){w.u8(a.forward?1:0);w.u64(a.maps.size());for(const auto& m:a.maps)matrix(w,m,maximum,check);}
    w.blob(proof(diagram,maximum,check));return std::move(w).finish(std::move(check));
}
inline std::vector<std::byte> encode_native_homology_diagram(const PreparedNativeHomologyDiagram& diagram,
    std::size_t maximum,std::function<void()> check={}) {
    return prepare_native_homology_diagram(diagram,maximum,check).bytes(std::move(check));
}
inline PreparedNativeHomologyDiagram decode_native_homology_diagram(std::span<const std::byte> bytes,
    const storage::TemporalReadLimits& read_limits={},runtime::ExecutionLease lease=runtime::ExecutionLease{},
    storage::NativeAccessionLimits accession_limits={}) {
    using namespace diagram_codec_detail;using native_diagram_detail::require;
    lease.check_cancelled();require(bytes.size()>=32,"truncated homology diagram");Reader r(bytes,read_limits);
    for(const auto b:magic)require(r.u8()==std::to_integer<std::uint8_t>(b),"homology diagram magic");
    require(r.u32()==1 && r.u32()==0,"homology diagram version");require(r.u64()==bytes.size(),"homology diagram length");
    require(r.u64()==storage::rfcx_detail::fnv1a64(bytes.subspan(32)),"homology diagram integrity");
    const auto product=r.u8();require(product<=static_cast<std::uint8_t>(HomologyDiagramProduct::IntegralModules),"homology diagram product");
    const auto grade=r.u64();require(grade<=std::numeric_limits<std::uint16_t>::max(),"homology diagram grade representation");
    NativeHomologyDiagramRequest request;request.product=static_cast<HomologyDiagramProduct>(product);request.grade=core::Grade{static_cast<std::uint16_t>(grade)};
    const auto n=r.count(32);
    for(std::size_t i=0;i<n;++i){lease.check_cancelled();const auto space=r.u64();const auto source_bytes=r.blob();
        storage::MappedRfcx source(source_bytes);
        r.consume_records(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){lease.check_cancelled();
            const auto g=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
            require(g.cell_count()<=read_limits.max_dimension,"homology source grade exceeds read dimension policy");
            r.consume_records(machine::checked_size_add(g.cell_count(),g.incidence_count()));
        }
        auto endpoint=storage::decode_temporal_endpoint(source.materialize(),r.blob(),read_limits);
        const auto count=r.count(8);std::vector<std::uint64_t> carriers;carriers.reserve(count);for(std::size_t j=0;j<count;++j)carriers.push_back(r.u64());
        storage::NativeAccessionDirectory directory(endpoint,space,{},[](auto data){return storage::rfcx_detail::fnv1a64(data);},accession_limits);
        request.vertices.push_back({directory.pin(storage::NativeAccessionRequest::sparse_chain(core::Grade{0},{},{})),std::move(carriers)});
    }
    const auto arrows=r.count(9);
    for(std::size_t i=0;i<arrows;++i){NativeHomologyDiagramArrow a;a.forward=r.flag();const auto count=r.count(24);
        for(std::size_t j=0;j<count;++j)a.maps.push_back(storage::temporal_codec_detail::matrix(r));
        request.arrows.push_back(std::move(a));}
    const auto certificate=r.blob();require(!r.remaining(),"trailing homology diagram bytes");
    auto result=PreparedNativeHomologyDiagram::prepare(std::move(request),lease);
    const auto check=[&lease]{lease.check_cancelled();};proof(result,read_limits.max_file_bytes,check).require_equal(certificate,check);
    if(read_limits.require_canonical)prepare_native_homology_diagram(result,read_limits.max_file_bytes,check).require_equal(bytes,check);
    return result;
}
} // namespace rfc::inference
