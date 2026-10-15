#pragma once
#include "rfc/storage/algebraic_archive_graph.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"

namespace rfc::storage {
namespace algebraic_graph_codec_detail {
using namespace algebraic_archive_detail;
inline constexpr std::array<std::byte,8> graph_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'G'},std::byte{0},std::byte{0},std::byte{0}};
inline AlgebraicArchiveLimits aggregate_policy(const AlgebraicArchiveGraphLimits& limits) {
    auto policy=limits.archive;policy.max_file_bytes=limits.max_file_bytes;policy.max_rational_coordinates=limits.max_rational_coordinates;
    return policy;
}
template<class T,class F> void write_table(Writer& writer,const std::vector<AlgebraicGraphEntry<T>>& rows,F action) {
    writer.u64(rows.size());for (const auto& row:rows) {writer.u64(row.key);action(row.value);}
}
template<class T,class F> void read_table(Reader& reader,std::vector<AlgebraicGraphEntry<T>>& rows,
    std::size_t& remaining,std::size_t minimum,F action) {
    const auto n=reader.count(remaining,minimum);remaining-=n;rows.reserve(n);std::uint64_t last{};
    for (std::size_t i=0;i<n;++i) {
        const auto key=reader.u64();require(key>last,"graph owner names must be nonzero and strictly ordered");last=key;
        rows.push_back({key,action()});
    }
}
inline field::AlgebraicMetricDomain read_domain(const AlgebraicSelectedEmbedding& root,
    const numeric::AlgebraicInvolution& involution,const AlgebraicArchiveGraphLimits& limits) {
    return std::visit([&](const auto& selected) {
        if constexpr (std::same_as<std::decay_t<decltype(selected)>,geometry::SelectedRealAlgebraicEmbedding>)
            return field::AlgebraicMetricDomain::real(selected,involution,limits.archive.metric);
        else return field::AlgebraicMetricDomain::hermitian(selected,involution,limits.archive.metric);
    },root);
}
inline void compare_budget(Admission& admission,const local::AlgebraicModule& source,const local::AlgebraicModule& target) {
    admission.matrix(source,target);
}
} // namespace algebraic_graph_codec_detail

inline std::vector<std::byte> encode_algebraic_archive_graph(const AlgebraicArchiveGraph& graph,
    const AlgebraicArchiveGraphLimits& limits={}) {
    using namespace algebraic_graph_codec_detail;using namespace algebraic_graph_detail;
    const auto& c=graph.contents();admit_counts(c,limits);
    const auto policy=aggregate_policy(limits);Admission admission(policy);Writer writer(limits.max_file_bytes-32);
    write_table(writer,c.fields,[&](const auto& field){write_field(writer,field,admission);});
    write_table(writer,c.involutions,[&](const auto& value) {
        writer.u64(owner_key(c.fields,value.field()));element(writer,value.generator_image(),admission);
    });
    write_table(writer,c.embeddings,[&](const auto& value) {
        std::visit([&](const auto& root){writer.u64(owner_key(c.fields,root.field()));},value);write_root(writer,value,admission);
    });
    write_table(writer,c.domains,[&](const auto& value) {
        writer.u64(owner_key(c.embeddings,value.embedding()));writer.u64(owner_key(c.involutions,value.involution()));
    });
    write_table(writer,c.bundles,[&](const auto& value) {
        writer.u64(owner_key(c.fields,value.field()));write_source(writer,value,admission);write_ranks(writer,value,admission);
    });
    write_table(writer,c.fibers,[&](const auto& value) {
        writer.u64(owner_key(c.bundles,value.bundle()));write_fibers(writer,value,admission);
    });
    write_table(writer,c.archives,[&](const auto& value) {
        const auto& declarations=value.contents();writer.u64(owner_key(c.bundles,value.bundle()));
        writer.u64(declarations.involution ? owner_key(c.involutions,*declarations.involution) : 0);
        writer.u64(declarations.embedding ? owner_key(c.embeddings,*declarations.embedding) : 0);
        writer.u64(declarations.metric_domain ? owner_key(c.domains,*declarations.metric_domain) : 0);
        writer.u64(declarations.fibers ? owner_key(c.fibers,*declarations.fibers) : 0);
        write_records(writer,declarations,admission);
    });
    write_table(writer,c.scalars,[&](const auto& value) {
        writer.u64(owner_key(c.fields,value.source()));writer.u64(owner_key(c.fields,value.target()));
        admission.charge(machine::checked_size_mul(value.source().degree(),value.target().degree()));
        element(writer,value.generator_image(),admission);
    });
    write_table(writer,c.carriers,[&](const auto& value) {
        writer.u64(owner_key(c.scalars,value.scalar_comparison()));
        writer.u64(bundle_key(c,value.source()));cell(writer,value.source().cell());
        writer.u64(bundle_key(c,value.target()));cell(writer,value.target().cell());
        compare_budget(admission,value.source(),value.target());values(writer,value.coefficients(),admission);
    });
    write_table(writer,c.metric_comparisons,[&](const auto& value) {
        writer.u64(owner_key(c.scalars,value.scalar_comparison()));writer.u64(owner_key(c.domains,value.source()));
        writer.u64(owner_key(c.domains,value.target()));
    });
    writer.u64(c.reconstructions.size());
    for (const auto& row:c.reconstructions) {writer.u64(row.key);writer.u64(row.accession);writer.u64(row.reconstruction);}
    writer.u64(c.measurements.size());
    for (const auto& row:c.measurements) {
        writer.u64(row.key);writer.u64(row.domains);writer.u64(row.carrier);writer.u64(row.source_archive);
        writer.u64(row.source_record);writer.u64(row.target_archive);writer.u64(row.target_record);
    }
    auto body=writer.take();Writer envelope(limits.max_file_bytes);envelope.raw(graph_magic);envelope.u16(1);envelope.u16(32);envelope.u32(0);
    envelope.u64(machine::checked_size_add(body.size(),32));envelope.u64(rfcx_detail::fnv1a64(body));envelope.raw(body);return envelope.take();
}

// Explicit references restore sharing inside this graph. All owners are fresh
// relative to other reads; declarations and compatibility receipts are rechecked.
inline AlgebraicArchiveGraph decode_algebraic_archive_graph(std::span<const std::byte> bytes,
    const AlgebraicArchiveGraphLimits& limits={}) {
    using namespace algebraic_graph_codec_detail;using namespace algebraic_graph_detail;
    const auto policy=aggregate_policy(limits);Admission admission(policy);bound(bytes.size(),limits.max_file_bytes,"graph file byte budget");
    require(bytes.size()>=32 && std::ranges::equal(bytes.first(8),graph_magic),"invalid algebraic graph header");
    Reader header(bytes.subspan(8,24),admission);
    require(header.u16()==1 && header.u16()==32 && header.u32()==0,"unsupported algebraic graph version/flags");
    require(header.u64()==bytes.size(),"graph length differs");require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"graph integrity checksum differs");
    Reader reader(bytes.subspan(32),admission);AlgebraicArchiveGraphContents c;
    auto owners=limits.max_owner_records,edges=limits.max_edges;
    read_table(reader,c.fields,owners,47,[&]{return read_field(reader,admission);});
    read_table(reader,c.involutions,owners,27,[&] {
        const auto key=reader.u64();const auto& field=at(c.fields,key);
        return numeric::AlgebraicInvolution::declare(field,element(reader,field));
    });
    read_table(reader,c.embeddings,owners,39,[&]() -> AlgebraicSelectedEmbedding {
        const auto key=reader.u64();const auto kind=reader.u8();return read_root(reader,at(c.fields,key),kind,admission);
    });
    read_table(reader,c.domains,owners,24,[&] {
        const auto root=reader.u64(),involution=reader.u64();return read_domain(at(c.embeddings,root),at(c.involutions,involution),limits);
    });
    read_table(reader,c.bundles,owners,32,[&] {
        const auto key=reader.u64();const auto& field=at(c.fields,key);const auto source=read_source(reader,admission);
        return read_ranks(reader,source,field,admission);
    });
    read_table(reader,c.fibers,owners,17,[&] {
        const auto key=reader.u64();return read_fibers(reader,at(c.bundles,key),admission);
    });
    read_table(reader,c.archives,owners,64,[&] {
        const auto bundle=reader.u64(),involution=reader.u64(),root=reader.u64(),domain=reader.u64(),fiber=reader.u64();
        AlgebraicArchiveContents contents;
        if (involution) contents.involution=at(c.involutions,involution);
        if (root) contents.embedding=at(c.embeddings,root);
        if (domain) contents.metric_domain=at(c.domains,domain);
        if (fiber) contents.fibers=at(c.fibers,fiber);
        read_records(reader,at(c.bundles,bundle),contents,admission);
        return AlgebraicArchive::declare(at(c.bundles,bundle),std::move(contents));
    });
    read_table(reader,c.scalars,edges,35,[&] {
        const auto source=reader.u64(),target=reader.u64();const auto& a=at(c.fields,source);const auto& b=at(c.fields,target);
        admission.charge(machine::checked_size_mul(a.degree(),b.degree()));
        return numeric::AlgebraicFieldEmbedding::declare(a,b,element(reader,b));
    });
    read_table(reader,c.carriers,edges,47,[&] {
        const auto key=reader.u64(),source_bundle=reader.u64();const auto source=cell(reader,at(c.bundles,source_bundle));
        const auto target_bundle=reader.u64();const auto target=cell(reader,at(c.bundles,target_bundle));
        compare_budget(admission,source,target);
        return local::AlgebraicCarrierComparison::declare(at(c.scalars,key),source,target,
            values(reader,target.field(),machine::checked_size_mul(source.rank(),target.rank())));
    });
    read_table(reader,c.metric_comparisons,edges,32,[&] {
        const auto scalar=reader.u64(),source=reader.u64(),target=reader.u64();
        return field::AlgebraicMetricDomainComparison::declare(at(c.scalars,scalar),at(c.domains,source),at(c.domains,target));
    });
    const auto reconstructions=reader.count(edges,24);edges-=reconstructions;c.reconstructions.reserve(reconstructions);std::uint64_t last{};
    for (std::size_t i=0;i<reconstructions;++i) {
        const auto key=reader.u64(),a=reader.u64(),r=reader.u64();require(key>last,"unordered graph reconstruction name");last=key;
        c.reconstructions.push_back({key,a,r});
    }
    const auto measurements=reader.count(edges,56);c.measurements.reserve(measurements);last=0;
    for (std::size_t i=0;i<measurements;++i) {
        const auto key=reader.u64(),domain=reader.u64(),carrier=reader.u64(),source=reader.u64(),source_record=reader.u64(),target=reader.u64(),target_record=reader.u64();
        require(key>last,"unordered graph measurement name");last=key;
        c.measurements.push_back({key,domain,carrier,source,source_record,target,target_record});
    }
    require(reader.remaining()==0,"trailing algebraic graph bytes");auto graph=AlgebraicArchiveGraph::declare(std::move(c),limits);
    require(std::ranges::equal(encode_algebraic_archive_graph(graph,limits),bytes),"noncanonical algebraic graph image");return graph;
}
inline void write_algebraic_archive_graph(const AlgebraicArchiveGraph& graph,const std::filesystem::path& path,
    FilePublication publication=FilePublication::ReplaceRegular,const AlgebraicArchiveGraphLimits& limits={}) {
    write_file_exact(path,encode_algebraic_archive_graph(graph,limits),publication);
}
inline AlgebraicArchiveGraph read_algebraic_archive_graph(const std::filesystem::path& path,const AlgebraicArchiveGraphLimits& limits={}) {
    return decode_algebraic_archive_graph(read_file_exact(path,limits.max_file_bytes),limits);
}
} // namespace rfc::storage
