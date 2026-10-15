#pragma once
#include "rfc/storage/algebraic_archive.hpp"
#include "rfc/storage/file_io.hpp"

namespace rfc::storage {
namespace algebraic_archive_detail {
using Q=numeric::Rational;
using E=numeric::AlgebraicElement;
inline void require(bool condition,const char* message) { if (!condition) throw std::invalid_argument(message); }
inline void bound(std::size_t n,std::size_t maximum,const char* message) { if (n>maximum) throw std::length_error(message); }
inline constexpr std::array<std::byte,8> magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'A'},std::byte{0},std::byte{0},std::byte{0}};
struct Admission {
    const AlgebraicArchiveLimits& limits;
    std::size_t remaining;
    explicit Admission(const AlgebraicArchiveLimits& policy) : limits(policy),remaining(policy.max_rational_coordinates) {
        require(limits.max_file_bytes>=32 && limits.max_integer_bytes && limits.field.max_degree &&
            limits.field.max_input_coefficients && limits.field.max_scalar_steps,
            "invalid algebraic archive admission policy");
    }
    void charge(std::size_t n) { bound(n,remaining,"aggregate algebraic archive coordinate budget"); remaining-=n; }
    void matrix(const local::AlgebraicModule& source,const local::AlgebraicModule& target) {
        charge(machine::checked_size_mul(machine::checked_size_mul(source.rank(),target.rank()),
            machine::checked_size_mul(source.field().degree(),target.field().degree())));
    }
};
class Writer {
public:
    explicit Writer(std::size_t maximum) : maximum_(maximum) {}
    void u8(std::uint8_t value) { admit(1); writer_.u8(value); }
    void u16(std::uint16_t value) { admit(2); writer_.u16(value); }
    void u32(std::uint32_t value) { admit(4); writer_.u32(value); }
    void u64(std::uint64_t value) { admit(8); writer_.u64(value); }
    void raw(std::span<const std::byte> value) { admit(value.size()); writer_.raw(value); }
    void integer(const numeric::Integer& value,const AlgebraicArchiveLimits& limits) {
        const auto n=value==0 ? 0 : machine::checked_size_add(mpz_sizeinbase(value.get_mpz_t(),2),7)/8;
        bound(n,limits.max_integer_bytes,"archive integer byte budget");
        admit(machine::checked_size_add(5,n)); rfcx_detail::write_bigint(writer_,value);
    }
    void rational(const Q& value,Admission& admission) {
        admission.charge(1); const auto canonical=numeric::canonical_rational(value);
        integer(canonical.get_num(),admission.limits); integer(canonical.get_den(),admission.limits);
    }
    void blob(std::span<const std::byte> value) { u64(value.size()); raw(value); }
    std::vector<std::byte> take() { return writer_.bytes(); }
private:
    void admit(std::size_t n) const { bound(n,maximum_-writer_.size(),"algebraic archive byte budget"); }
    rfcx_detail::Writer writer_; std::size_t maximum_;
};
class Reader {
public:
    Reader(std::span<const std::byte> bytes,Admission& admission) : reader_(bytes),size_(bytes.size()),admission_(admission) {}
    std::size_t remaining() const { return size_-reader_.position(); }
    std::uint8_t u8() { return reader_.u8(); }
    bool flag() { const auto value=u8(); require(value<=1,"invalid archive boolean"); return value!=0; }
    std::uint16_t u16() { return reader_.u16(); }
    std::uint32_t u32() { return reader_.u32(); }
    std::uint64_t u64() { return reader_.u64(); }
    void coefficients(std::size_t n) const { bound(n,admission_.remaining,"archive coefficient allocation budget"); }
    std::size_t count(std::size_t maximum,std::size_t minimum=1) {
        const auto n=u64(); require(n<=maximum && n<=remaining()/minimum,"archive count exceeds bounds");
        return static_cast<std::size_t>(n);
    }
    std::span<const std::byte> blob() { const auto n=count(admission_.limits.max_file_bytes); return reader_.slice(n); }
    numeric::Integer integer() {
        const bool negative=flag(); const auto n=u32();
        bound(n,admission_.limits.max_integer_bytes,"archive integer byte budget");
        const auto bytes=reader_.slice(n);
        require(bytes.empty() || bytes.front()!=std::byte{0},"noncanonical archive integer magnitude");
        require(!negative || !bytes.empty(),"negative zero archive integer");
        numeric::Integer out{0}; if (!bytes.empty()) mpz_import(out.get_mpz_t(),bytes.size(),1,1,1,0,bytes.data());
        if (negative) out=-out;
        return out;
    }
    Q rational() {
        admission_.charge(1); const auto n=integer(),d=integer(); require(d>0,"archive denominator must be positive");
        Q out(n,d); out.canonicalize(); require(out.get_num()==n && out.get_den()==d,"noncanonical archive rational"); return out;
    }
private:
    rfcx_detail::Reader reader_; std::size_t size_; Admission& admission_;
};
inline void element(Writer& writer,const E& value,Admission& admission) {
    for (const auto& q:value.coordinates()) writer.rational(q,admission);
}
inline E element(Reader& reader,const numeric::CertifiedAlgebraicField& field) {
    numeric::RationalVector coordinates; coordinates.reserve(field.degree());
    for (std::size_t i=0;i<field.degree();++i) coordinates.push_back(reader.rational());
    return field.element(coordinates);
}
inline void cell(Writer& writer,core::CellId id) { writer.u16(id.grade.value); writer.u32(id.index); }
inline local::AlgebraicModule cell(Reader& reader,const local::AlgebraicStalkBundle& bundle) {
    const auto grade=reader.u16(); const auto index=reader.u32(); return bundle.stalk({core::Grade{grade},index});
}
inline void values(Writer& writer,const std::vector<E>& values,Admission& admission) {
    for (const auto& value:values) element(writer,value,admission);
}
inline std::vector<E> values(Reader& reader,const numeric::CertifiedAlgebraicField& field,std::size_t count) {
    reader.coefficients(machine::checked_size_mul(count,field.degree()));
    require(count<=reader.remaining()/machine::checked_size_mul(field.degree(),std::size_t{11}),"truncated archive coefficient block");
    std::vector<E> out; out.reserve(count);
    for (std::size_t i=0;i<count;++i) out.push_back(element(reader,field));
    return out;
}
inline local::AlgebraicModuleMap map(Reader& reader,const local::AlgebraicModule& source,
                                   const local::AlgebraicModule& target,Admission& admission) {
    admission.matrix(source,target);
    return local::AlgebraicModuleMap::declare(source,target,
        values(reader,source.field(),machine::checked_size_mul(source.rank(),target.rank())));
}
// Admission precedes the RFCX parser's integer materialization and the native
// source chain calculation. RFCX itself still validates every source section.
inline void source_preflight(std::span<const std::byte> bytes,Admission& admission) {
    require(bytes.size()>=rfcx_detail::header_size,"truncated archive RFCX source");
    rfcx_detail::Reader header(bytes,28); const auto grades=header.u32();
    bound(grades,admission.limits.max_records,"archive source grade budget");
    (void)header.u32();(void)header.u32();const auto signature_offset=machine::checked_size(header.u64());
    const auto signature_size=machine::checked_size(header.u64());const auto directory=machine::checked_size(header.u64());
    const auto directory_size=machine::checked_size(header.u64());
    require(directory<=bytes.size() && directory_size<=bytes.size()-directory &&
        directory_size==machine::checked_size_mul(grades,rfcx_detail::grade_descriptor_size),"archive source grade directory bounds");
    require(signature_offset<=bytes.size() && signature_size<=bytes.size()-signature_offset,"archive source signature bounds");
    rfcx_detail::Reader signature(bytes.subspan(signature_offset,signature_size));
    const auto aspects=signature.u32();bound(aspects,admission.limits.max_records,"archive source aspect budget");
    for (std::size_t i=0;i<aspects;++i) {(void)signature.slice(signature.u32());(void)signature.u8();(void)signature.u8();}
    const auto types=signature.u32();bound(types,admission.limits.max_records,"archive source type budget");
    std::size_t rules{};
    for (std::size_t i=0;i<types;++i) {
        (void)signature.slice(signature.u32());(void)signature.u8();(void)signature.u64();(void)signature.u64();
        (void)signature.u8();(void)signature.u8();(void)signature.u16();(void)signature.u8();
        const auto count=signature.u32();rules=machine::checked_size_add(rules,count);
        bound(rules,admission.limits.max_records,"archive source rule budget");
        for (std::size_t j=0;j<count;++j) {
            (void)signature.u8();(void)signature.u16();(void)signature.u64();
            Reader scalar(bytes.subspan(signature_offset+signature.position(),signature.remaining()),admission);
            (void)scalar.rational();const auto used=signature.remaining()-scalar.remaining();(void)signature.slice(used);
        }
    }
    std::size_t cells{},occurrences{},previous{},earlier{};
    auto overflow=[&](std::uint64_t offset,std::size_t count) {
        if (!offset) return;
        rfcx_detail::Reader lane(bytes,machine::checked_size(offset));const auto records=lane.u64();
        require(records<=count && records<=lane.remaining()/32,"archive source overflow count bounds");
        for (std::size_t i=0;i<records;++i) {
            (void)lane.u64();(void)lane.u64();const auto size=lane.u32();
            bound(size,admission.limits.max_integer_bytes,"archive source integer byte budget");
            (void)lane.u32();(void)lane.u64();
        }
    };
    for (std::size_t k=0;k<grades;++k) {
        const auto descriptor=rfcx_detail::read_grade_descriptor(bytes,directory+k*rfcx_detail::grade_descriptor_size);
        const auto count=machine::checked_size(descriptor.cell_count),incidences=machine::checked_size(descriptor.incidence_count);
        cells=machine::checked_size_add(cells,count);occurrences=machine::checked_size_add(occurrences,incidences);
        bound(cells,admission.limits.modules.max_cells,"archive source cell budget");
        bound(occurrences,admission.limits.fibers.max_occurrences,"archive source occurrence budget");
        admission.charge(count);admission.charge(incidences);
        if (k) admission.charge(machine::checked_size_mul(previous,count));
        if (k>=2) admission.charge(machine::checked_size_mul(earlier,count));
        overflow(descriptor.cell_scale_overflow,count);overflow(descriptor.lattice_overflow,incidences);
        earlier=previous;previous=count;
    }
}
inline void write_field(Writer& writer,const numeric::CertifiedAlgebraicField& field,Admission& admission) {
    const auto& limits=admission.limits;
    bound(field.degree(),limits.field.max_degree,"archive field degree budget");
    bound(field.minimal_polynomial().size(),limits.field.max_input_coefficients,"archive polynomial coefficient budget");
    numeric::algebraic_detail::Work work{limits.field};
    for (const auto& q:field.minimal_polynomial()) work.admit(q);
    writer.u64(field.minimal_polynomial().size()); for (const auto& q:field.minimal_polynomial()) writer.rational(q,admission);
    writer.u8(static_cast<std::uint8_t>(field.certificate().index()));
    std::visit([&](const auto& certificate) {
        writer.u64(certificate.prime);
        if constexpr (std::same_as<std::decay_t<decltype(certificate)>,numeric::EisensteinShift>) writer.integer(certificate.shift,limits);
    },field.certificate());
}
inline numeric::CertifiedAlgebraicField read_field(Reader& reader,Admission& admission) {
    const auto& limits=admission.limits;
    const auto n=reader.count(std::min(limits.field.max_input_coefficients,machine::checked_size_add(limits.field.max_degree,1)),11);
    require(n>=2,"archive coefficient field polynomial must be nonconstant");
    reader.coefficients(n);
    numeric::RationalVector polynomial; polynomial.reserve(n);
    for (std::size_t i=0;i<n;++i) polynomial.push_back(reader.rational());
    require(polynomial.back()==1,"archive field polynomial must be monic without trailing zeros");
    const auto certificate_kind=reader.u8(); require(certificate_kind<=1,"unsupported archive field certificate");
    const auto prime=reader.u64();
    const auto certificate=certificate_kind==0 ? numeric::AlgebraicIrreducibility(numeric::IrreducibleReduction{prime}) :
        numeric::AlgebraicIrreducibility(numeric::EisensteinShift{prime,reader.integer()});
    return numeric::CertifiedAlgebraicField::declare(polynomial,certificate,limits.field);
}
inline void write_root(Writer& writer,const AlgebraicSelectedEmbedding& selected,Admission& admission) {
    writer.u8(static_cast<std::uint8_t>(selected.index()+1));
    std::visit([&](const auto& root) {
        if constexpr (std::same_as<std::decay_t<decltype(root)>,geometry::SelectedRealAlgebraicEmbedding>) {
            writer.rational(root.certificate().interval.lower,admission); writer.rational(root.certificate().interval.upper,admission);
        } else {
            const auto& region=root.certificate().region;
            writer.rational(region.center.real,admission); writer.rational(region.center.imaginary,admission); writer.rational(region.radius,admission);
        }
    },selected);
}
inline AlgebraicSelectedEmbedding read_root(Reader& reader,const numeric::CertifiedAlgebraicField& field,
    std::uint8_t selected,Admission& admission) {
    const auto& limits=admission.limits;
    require(selected==1 || selected==2,"unsupported archive selected embedding");
    if (selected==1) {
        const auto lower=reader.rational(),upper=reader.rational();
        return geometry::SelectedRealAlgebraicEmbedding::isolate(field,lower,upper,limits.real_root);
    } else if (selected==2) {
        const auto real=reader.rational(),imaginary=reader.rational(),radius=reader.rational();
        return geometry::SelectedComplexAlgebraicEmbedding::isolate(field,{real,imaginary},radius,limits.complex_root);
    }
    throw std::invalid_argument("missing archive selected embedding");
}
inline void write_source(Writer& writer,const local::AlgebraicStalkBundle& bundle,Admission& admission) {
    const auto& limits=admission.limits;
    std::size_t cells{},occurrences{};
    for (std::size_t k=0;k<bundle.source().grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        cells=machine::checked_size_add(cells,bundle.source().grade(grade).cell_count());
        occurrences=machine::checked_size_add(occurrences,bundle.source().grade(grade).incidence_count());
    }
    bound(cells,limits.modules.max_cells,"archive cell count budget");
    bound(occurrences,limits.fibers.max_occurrences,"archive occurrence count budget");
    const auto source_bytes=encode_rfcx(bundle.source()).bytes;
    source_preflight(source_bytes,admission);writer.blob(source_bytes);
}
inline core::RelationalComplex read_source(Reader& reader,Admission& admission) {
    const auto& limits=admission.limits;
    const auto source_bytes=reader.blob();
    source_preflight(source_bytes,admission);MappedRfcx mapped(source_bytes);
    std::size_t cells{},occurrences{};
    for (std::size_t k=0;k<mapped.grade_count();++k) {
        const auto grade=mapped.grade(core::Grade{static_cast<std::uint16_t>(k)});
        cells=machine::checked_size_add(cells,grade.cell_count()); occurrences=machine::checked_size_add(occurrences,grade.incidence_count());
    }
    bound(cells,limits.modules.max_cells,"archive source cell budget");
    bound(occurrences,limits.fibers.max_occurrences,"archive source occurrence budget");
    auto source=mapped.materialize();
    require(std::ranges::equal(encode_rfcx(source).bytes,source_bytes),"noncanonical archive source bytes");
    return source;
}
inline void write_ranks(Writer& writer,const local::AlgebraicStalkBundle& bundle,Admission& admission) {
    const auto& limits=admission.limits;const auto& field=bundle.field();
    writer.u64(bundle.source().grade_count());
    for (std::size_t k=0;k<bundle.source().grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        writer.u64(bundle.source().grade(grade).cell_count());
        for (std::size_t i=0;i<bundle.source().grade(grade).cell_count();++i) {
            const auto rank=bundle.stalk({grade,static_cast<std::uint32_t>(i)}).rank();
            bound(rank,limits.modules.max_rank,"archive stalk rank budget");
            admission.charge(machine::checked_size_mul(rank,field.degree())); writer.u64(rank);
        }
    }
}
inline local::AlgebraicStalkBundle read_ranks(Reader& reader,const core::RelationalComplex& source,
    const numeric::CertifiedAlgebraicField& field,Admission& admission) {
    const auto& limits=admission.limits;
    const auto grades=reader.count(source.grade_count(),8); require(grades==source.grade_count(),"archive stalk grades incomplete");
    std::vector<std::vector<std::size_t>> ranks; ranks.reserve(grades);
    for (std::size_t k=0;k<grades;++k) {
        const auto count=reader.count(limits.modules.max_cells,8);
        require(count==source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(),"archive stalk cells incomplete");
        std::vector<std::size_t> row; row.reserve(count);
        for (std::size_t i=0;i<count;++i) {
            const auto rank=reader.u64(); require(rank && rank<=limits.modules.max_rank,"archive stalk rank outside bounds");
            admission.charge(machine::checked_size_mul(static_cast<std::size_t>(rank),field.degree())); row.push_back(static_cast<std::size_t>(rank));
        }
        ranks.push_back(std::move(row));
    }
    return local::AlgebraicStalkBundle::declare(source,field,std::move(ranks),limits.modules);
}
inline void write_fibers(Writer& writer,const local::AlgebraicRelationalFiberSystem& fibers,Admission& admission) {
    const auto& bundle=fibers.bundle();
    writer.u8(static_cast<std::uint8_t>(fibers.chain_policy()));
    for (std::size_t k=1;k<bundle.source().grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t i=0;i<bundle.source().grade(grade).incidence_count();++i) {
            const auto& map=fibers.occurrence_map(grade,i);
            admission.matrix(map.source(),map.target()); values(writer,map.coefficients(),admission);
        }
    }
}
inline local::AlgebraicRelationalFiberSystem read_fibers(Reader& reader,const local::AlgebraicStalkBundle& bundle,Admission& admission) {
    const auto& source=bundle.source();
    const auto policy=reader.u8(); require(policy<=1,"unknown archive chain policy");
    std::size_t occurrences{};
    for (std::size_t k=1;k<source.grade_count();++k)
        occurrences=machine::checked_size_add(occurrences,source.grade(core::Grade{static_cast<std::uint16_t>(k)}).incidence_count());
    bound(occurrences,admission.limits.fibers.max_occurrences,"archive fiber occurrence budget");
    std::vector<local::AlgebraicOccurrenceMap> maps; maps.reserve(occurrences);
    for (std::size_t k=1;k<source.grade_count();++k) {
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)}; const auto& arena=source.grade(grade);
        for (std::size_t i=0;i<arena.incidence_count();++i) {
            const auto upper=bundle.stalk({grade,arena.upper_index[i]});
            const auto lower=bundle.stalk({core::Grade{static_cast<std::uint16_t>(k-1)},arena.lower_index[i]});
            maps.push_back({grade,i,map(reader,upper,lower,admission)});
        }
    }
    return local::AlgebraicRelationalFiberSystem::declare(bundle,maps,static_cast<local::AlgebraicChainPolicy>(policy),admission.limits.fibers);
}
inline void write_records(Writer& writer,const AlgebraicArchiveContents& contents,Admission& admission) {
    bound(machine::checked_size_add(contents.records.size(),contents.reconstructions.size()),admission.limits.max_records,"archive aggregate record budget");
    writer.u64(contents.records.size());
    for (const auto& record:contents.records) {
        writer.u64(record.key); writer.u8(static_cast<std::uint8_t>(record.value.index()));
        std::visit([&](const auto& value) {
            using T=std::decay_t<decltype(value)>;
            if constexpr (std::same_as<T,local::AlgebraicModuleMap>) {
                cell(writer,value.source().cell()); cell(writer,value.target().cell());
                admission.matrix(value.source(),value.target()); values(writer,value.coefficients(),admission);
            } else {
                cell(writer,value.carrier().cell());
                if constexpr (std::same_as<T,local::AlgebraicModuleVector>) values(writer,value.coordinates(),admission);
                else if constexpr (std::same_as<T,local::AlgebraicCovector>) values(writer,value.coefficients(),admission);
                else {
                    const auto& map=[&]() -> const local::AlgebraicModuleMap& {
                        if constexpr (std::same_as<T,field::AlgebraicMeasurementForm>) return value.coefficient_form();
                        else return value.measurement().coefficient_form();
                    }();
                    admission.matrix(map.source(),map.target()); values(writer,map.coefficients(),admission);
                }
            }
        },record.value);
    }
    writer.u64(contents.reconstructions.size());
    for (const auto& pair:contents.reconstructions) {writer.u64(pair.key);writer.u64(pair.accession);writer.u64(pair.reconstruction);}
}
inline void read_records(Reader& reader,const local::AlgebraicStalkBundle& bundle,
    AlgebraicArchiveContents& contents,Admission& admission) {
    const auto& limits=admission.limits;const auto& field=bundle.field();
    const auto records=reader.count(limits.max_records,15); contents.records.reserve(records); std::uint64_t last{};
    for (std::size_t i=0;i<records;++i) {
        const auto key=reader.u64(); require(key>last,"unordered or duplicate archive record key"); last=key;
        const auto kind=reader.u8(); require(kind<=4,"unknown algebraic record kind"); const auto carrier=cell(reader,bundle);
        if (kind==0) contents.records.push_back({key,carrier.vector(values(reader,field,carrier.rank()))});
        else if (kind==1) contents.records.push_back({key,local::AlgebraicCovector::declare(carrier,values(reader,field,carrier.rank()))});
        else if (kind==2) { const auto target=cell(reader,bundle); contents.records.push_back({key,map(reader,carrier,target,admission)}); }
        else {
            require(contents.metric_domain.has_value(),"archive form requires declared metric domain");
            const auto form=field::AlgebraicMeasurementForm::declare(*contents.metric_domain,map(reader,carrier,carrier,admission));
            if (kind==3) contents.records.push_back({key,form});
            else contents.records.push_back({key,field::AlgebraicModuleMetric::certify(form)});
        }
    }
    const auto pairs=reader.count(limits.max_records-records,24);contents.reconstructions.reserve(pairs);last=0;
    for (std::size_t i=0;i<pairs;++i) {
        const auto key=reader.u64(),a=reader.u64(),r=reader.u64();require(key>last,"unordered archive reconstruction name");last=key;
        contents.reconstructions.push_back({key,a,r});
    }
}
} // namespace algebraic_archive_detail

inline std::vector<std::byte> encode_algebraic_archive(const AlgebraicArchive& archive,const AlgebraicArchiveLimits& limits={}) {
    using namespace algebraic_archive_detail;
    Admission admission(limits);Writer writer(limits.max_file_bytes-32);
    const auto& bundle=archive.bundle();const auto& contents=archive.contents();
    write_source(writer,bundle,admission);write_field(writer,bundle.field(),admission);
    writer.u8(contents.involution.has_value());if (contents.involution) element(writer,contents.involution->generator_image(),admission);
    if (contents.embedding) write_root(writer,*contents.embedding,admission);else writer.u8(0);
    writer.u8(contents.metric_domain.has_value());write_ranks(writer,bundle,admission);
    writer.u8(contents.fibers.has_value());if (contents.fibers) write_fibers(writer,*contents.fibers,admission);
    write_records(writer,contents,admission);
    auto payload=writer.take();Writer envelope(limits.max_file_bytes);envelope.raw(magic);envelope.u16(1);envelope.u16(32);envelope.u32(0);
    envelope.u64(machine::checked_size_add(payload.size(),32));envelope.u64(rfcx_detail::fnv1a64(payload));envelope.raw(payload);
    return envelope.take();
}

// Every decode constructs fresh owners and recomputes all applicable certificates.
inline AlgebraicArchive decode_algebraic_archive(std::span<const std::byte> bytes,const AlgebraicArchiveLimits& limits={}) {
    using namespace algebraic_archive_detail;
    Admission admission(limits);bound(bytes.size(),limits.max_file_bytes,"archive file byte budget");
    require(bytes.size()>=32 && std::ranges::equal(bytes.first(8),magic),"invalid algebraic archive header");
    Reader header(bytes.subspan(8,24),admission);
    require(header.u16()==1 && header.u16()==32 && header.u32()==0,"unsupported algebraic archive version/flags");
    require(header.u64()==bytes.size(),"archive length differs");
    require(header.u64()==rfcx_detail::fnv1a64(bytes.subspan(32)),"archive integrity checksum differs");
    Reader reader(bytes.subspan(32),admission);const auto source=read_source(reader,admission);const auto field=read_field(reader,admission);
    AlgebraicArchiveContents contents;
    if (reader.flag()) contents.involution=numeric::AlgebraicInvolution::declare(field,element(reader,field));
    const auto selected=reader.u8();require(selected<=2,"unsupported archive selected embedding");
    if (selected) contents.embedding=read_root(reader,field,selected,admission);
    if (reader.flag()) {
        require(contents.involution && contents.embedding,"archive metric domain declarations missing");
        if (selected==1) contents.metric_domain=field::AlgebraicMetricDomain::real(
            std::get<geometry::SelectedRealAlgebraicEmbedding>(*contents.embedding),*contents.involution,limits.metric);
        else contents.metric_domain=field::AlgebraicMetricDomain::hermitian(
            std::get<geometry::SelectedComplexAlgebraicEmbedding>(*contents.embedding),*contents.involution,limits.metric);
    }
    const auto bundle=read_ranks(reader,source,field,admission);
    if (reader.flag()) contents.fibers=read_fibers(reader,bundle,admission);
    read_records(reader,bundle,contents,admission);require(reader.remaining()==0,"trailing algebraic archive bytes");
    auto result=AlgebraicArchive::declare(bundle,std::move(contents));
    require(std::ranges::equal(encode_algebraic_archive(result,limits),bytes),"noncanonical algebraic archive image");return result;
}
inline void write_algebraic_archive(const AlgebraicArchive& archive,const std::filesystem::path& path,
    FilePublication publication=FilePublication::ReplaceRegular,const AlgebraicArchiveLimits& limits={}) {
    write_file_exact(path,encode_algebraic_archive(archive,limits),publication);
}
inline AlgebraicArchive read_algebraic_archive(const std::filesystem::path& path,const AlgebraicArchiveLimits& limits={}) {
    return decode_algebraic_archive(read_file_exact(path,limits.max_file_bytes),limits);
}
} // namespace rfc::storage
