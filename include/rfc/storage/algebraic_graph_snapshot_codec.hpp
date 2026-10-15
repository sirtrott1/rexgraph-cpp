#pragma once
#include "rfc/storage/algebraic_graph_snapshot.hpp"
#include "rfc/storage/algebraic_archive_graph_codec.hpp"

namespace rfc::storage {
namespace algebraic_graph_snapshot_codec_detail {
inline constexpr std::array<std::byte,8> magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'H'},std::byte{0},std::byte{0},std::byte{0}};
inline AlgebraicArchiveLimits policy(const AlgebraicGraphSnapshotLimits& limits) {
    auto result=algebraic_graph_codec_detail::aggregate_policy(limits.graph);result.max_file_bytes=limits.max_file_bytes;return result;
}
template<class T> void write_map(algebraic_archive_detail::Writer& writer,const std::vector<std::vector<T>>& rows) {
    writer.u64(rows.size());for (const auto& row:rows) {writer.u64(row.size());for (const auto value:row) writer.u64(value);}
}
template<class T> std::vector<std::vector<T>> read_map(algebraic_archive_detail::Reader& reader,
    const core::RelationalComplex& source,bool occurrences,std::size_t& remaining) {
    using algebraic_archive_detail::require;
    const auto grades=reader.count(source.grade_count(),8);require(grades==source.grade_count(),"graph source map grade count differs");
    std::vector<std::vector<T>> result;result.reserve(grades);
    for (std::size_t k=0;k<grades;++k) {
        const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        const auto expected=occurrences?(k?arena.incidence_count():0):arena.cell_count();
        const auto n=reader.count(std::min(expected,remaining),8);require(n==expected,"graph source map extent differs");remaining-=n;
        auto& row=result.emplace_back();row.reserve(n);
        for (std::size_t i=0;i<n;++i) {
            const auto value=reader.u64();require(value<=std::numeric_limits<T>::max(),"graph source map index overflows its native type");
            row.push_back(static_cast<T>(value));
        }
    }
    return result;
}
} // namespace algebraic_graph_snapshot_codec_detail

// RFCXH wraps unchanged RFCXG v1 bytes with a complete explicit native binding
// declaration. It is not a new version or reinterpretation of an RFCXG file.
inline std::vector<std::byte> encode_algebraic_graph_snapshot(const AlgebraicGraphSnapshot& snapshot,
    const AlgebraicGraphSnapshotLimits& limits={}) {
    using namespace algebraic_archive_detail;
    const auto reader_policy=algebraic_graph_snapshot_codec_detail::policy(limits);Admission admission(reader_policy);
    (void)AlgebraicGraphSnapshot::declare(snapshot.graph(),snapshot.anchor_bundle(),snapshot.source_bindings(),snapshot.archive_bindings(),limits);
    Writer writer(limits.max_file_bytes-32);writer.u64(snapshot.anchor_bundle());
    writer.blob(encode_algebraic_archive_graph(snapshot.graph(),limits.graph));
    writer.u64(snapshot.source_bindings().size());
    for (const auto& row:snapshot.source_bindings()) {
        writer.u64(row.bundle);writer.u8(static_cast<std::uint8_t>(row.kind));
        if (row.kind==AlgebraicGraphSourceKind::AnchorMorphism) {
            algebraic_graph_snapshot_codec_detail::write_map(writer,row.cells);
            algebraic_graph_snapshot_codec_detail::write_map(writer,row.occurrences);
        }
    }
    writer.u64(snapshot.archive_bindings().size());
    for (const auto& row:snapshot.archive_bindings()) {writer.u64(row.archive);writer.u64(row.bundle);}
    auto body=writer.take();Writer envelope(limits.max_file_bytes);envelope.raw(algebraic_graph_snapshot_codec_detail::magic);
    envelope.u16(1);envelope.u16(32);envelope.u32(0);envelope.u64(machine::checked_size_add(body.size(),32));
    envelope.u64(rfcx_detail::fnv1a64(body));envelope.raw(body);return envelope.take();
}
inline AlgebraicGraphSnapshot decode_algebraic_graph_snapshot(std::span<const std::byte> bytes,
    const AlgebraicGraphSnapshotLimits& limits={}) {
    using namespace algebraic_archive_detail;
    const auto reader_policy=algebraic_graph_snapshot_codec_detail::policy(limits);Admission admission(reader_policy);
    bound(bytes.size(),limits.max_file_bytes,"native graph snapshot byte budget");
    require(bytes.size()>=32 && std::ranges::equal(bytes.first(8),algebraic_graph_snapshot_codec_detail::magic),"invalid native graph snapshot header");
    Reader header(bytes.subspan(8,24),admission);
    require(header.u16()==1 && header.u16()==32 && header.u32()==0,"unsupported native graph snapshot version/flags");
    require(header.u64()==bytes.size() && header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"native graph snapshot integrity differs");
    Reader reader(bytes.subspan(32),admission);const auto anchor=reader.u64();const auto graph_bytes=reader.blob();
    bound(graph_bytes.size(),limits.graph.max_file_bytes,"embedded graph byte budget");
    auto graph=decode_algebraic_archive_graph(graph_bytes,limits.graph);(void)graph.bundle(anchor);
    std::vector<AlgebraicGraphSourceBinding> sources;std::vector<AlgebraicGraphArchiveBinding> archives;
    const auto count=reader.count(limits.max_source_bindings,9);sources.reserve(count);std::uint64_t last{};
    auto remaining=limits.max_source_coordinates;
    for (std::size_t i=0;i<count;++i) {
        const auto key=reader.u64();require(key>last,"native graph source names must be strictly ordered");last=key;
        const auto kind=reader.u8();require(kind<=1,"unknown native graph source binding");
        AlgebraicGraphSourceBinding binding{key,static_cast<AlgebraicGraphSourceKind>(kind),{},{}};
        const auto& source=graph.bundle(key).source();
        if (binding.kind==AlgebraicGraphSourceKind::AnchorMorphism) {
            binding.cells=algebraic_graph_snapshot_codec_detail::read_map<std::uint32_t>(reader,source,false,remaining);
            binding.occurrences=algebraic_graph_snapshot_codec_detail::read_map<std::size_t>(reader,source,true,remaining);
        }
        sources.push_back(std::move(binding));
    }
    const auto archive_count=reader.count(limits.max_archive_bindings,16);archives.reserve(archive_count);last=0;
    for (std::size_t i=0;i<archive_count;++i) {
        const auto key=reader.u64(),bundle=reader.u64();require(key>last,"native graph archive names must be strictly ordered");last=key;
        archives.push_back({key,bundle});
    }
    require(reader.remaining()==0,"trailing native graph snapshot bytes");
    auto snapshot=AlgebraicGraphSnapshot::declare(std::move(graph),anchor,std::move(sources),std::move(archives),limits);
    require(std::ranges::equal(encode_algebraic_graph_snapshot(snapshot,limits),bytes),"noncanonical native graph snapshot image");return snapshot;
}
inline void write_algebraic_graph_snapshot(const AlgebraicGraphSnapshot& snapshot,const std::filesystem::path& path,
    FilePublication publication=FilePublication::ReplaceRegular,const AlgebraicGraphSnapshotLimits& limits={}) {
    const auto bytes=encode_algebraic_graph_snapshot(snapshot,limits);(void)decode_algebraic_graph_snapshot(bytes,limits);
    write_file_exact(path,bytes,publication);
}
inline AlgebraicGraphSnapshot read_algebraic_graph_snapshot(const std::filesystem::path& path,const AlgebraicGraphSnapshotLimits& limits={}) {
    return decode_algebraic_graph_snapshot(read_file_exact(path,limits.max_file_bytes),limits);
}
} // namespace rfc::storage
