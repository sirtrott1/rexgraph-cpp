#pragma once
#include "rfc/storage/payload_manifest.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"

namespace rfc::storage {
struct RfcxPayloadLimits {
    std::size_t max_file_bytes{16u<<20};
    std::size_t max_source_words{2000000},max_cells{1000000},max_occurrences{1000000},max_integer_bits{1u<<20};
};
struct RfcxPayloadFootprint {std::size_t decoded_bytes{},coefficient_bits{},scratch_bytes{},source_words{},max_integer_bits{};};
namespace rfcx_payload_detail {
using namespace payload_manifest_detail;
inline RfcxPayloadFootprint inspect(std::span<const std::byte> bytes,const RfcxPayloadLimits& policy) {
    bound(bytes.size(),policy.max_file_bytes,"native payload file admission");
    AlgebraicArchiveLimits limits;limits.max_file_bytes=bytes.size();limits.max_records=policy.max_source_words;
    limits.max_rational_coordinates=policy.max_source_words;limits.modules.max_cells=policy.max_cells;
    limits.fibers.max_occurrences=policy.max_occurrences;
    limits.max_integer_bytes=machine::ceil_div(policy.max_integer_bits,8);
    algebraic_archive_detail::Admission admission(limits);algebraic_archive_detail::source_preflight(bytes,admission);
    const auto words=limits.max_rational_coordinates-admission.remaining;
    rfcx_detail::Reader header(bytes,28);const auto grades=header.u32();(void)header.u32();(void)header.u32();
    const auto signature_offset=checked_size(header.u64()),signature_size=checked_size(header.u64()),directory=checked_size(header.u64());
    const auto signature=rfcx_detail::read_signature(bytes.subspan(signature_offset,signature_size),0);
    std::size_t maximum=1;
    const auto bits=[](mpz_srcptr z){return mpz_sgn(z)?mpz_sizeinbase(z,2):std::size_t{0};};
    for(std::size_t i=0;i<signature.type_count();++i)for(const auto& rule:signature.type(static_cast<core::RelationTypeId>(i)).constraints.rules)
        maximum=std::max({maximum,bits(mpq_numref(rule.value.get_mpq_t())),bits(mpq_denref(rule.value.get_mpq_t()))});
    const auto lane=[&](std::uint64_t offset,std::uint64_t overflow,std::size_t count) {
        rfcx_detail::Reader values(bytes,checked_size(offset));
        for(std::size_t i=0;i<count;++i) {
            const auto value=values.i64();if(value==rfcx_detail::overflow_sentinel)continue;
            const auto magnitude=value<0?static_cast<std::uint64_t>(-value):static_cast<std::uint64_t>(value);
            maximum=std::max(maximum,static_cast<std::size_t>(std::bit_width(magnitude)));
        }
        if(!overflow)return;
        rfcx_detail::Reader records(bytes,checked_size(overflow));const auto n=records.size_value();
        for(std::size_t i=0;i<n;++i) {
            (void)records.u64();const auto position=records.size_value();const auto size=records.u32();(void)records.u32();(void)records.u64();
            require(position<=bytes.size() && size<=bytes.size()-position,"native source magnitude range");
            if(size)maximum=std::max(maximum,checked_size_add(checked_size_mul(size-1,8),static_cast<std::size_t>(std::bit_width(static_cast<std::uint8_t>(bytes[position])))));
        }
    };
    for(std::size_t k=0;k<grades;++k) {
        const auto d=rfcx_detail::read_grade_descriptor(bytes,directory+k*rfcx_detail::grade_descriptor_size);
        lane(d.cell_scales,d.cell_scale_overflow,checked_size(d.cell_count));
        lane(d.lattice,d.lattice_overflow,checked_size(d.incidence_count));
    }
    bound(maximum,policy.max_integer_bits,"native source integer bit admission");
    // Conservative preallocation envelopes, checked again against the prepared
    // native object's named storage and GMP limb capacities before publication.
    // Canonical lanes keep cell-scale denominators shared. Account for products,
    // sum carries and retained GMP capacity in the admitted chain-validation words.
    const auto arithmetic_bits=checked_size_add(checked_size_mul(maximum,8),64);
    const auto word_bytes=checked_size_add(256,numeric::rational_limb_envelope(arithmetic_bits));
    const auto decoded=checked_size_add(checked_size_mul(bytes.size(),128),checked_size_mul(words,word_bytes));
    return {decoded,checked_size_mul(bytes.size(),8),checked_size_add(checked_size_mul(decoded,2),checked_size_mul(bytes.size(),3)),words,maximum};
}
template<class T> inline std::size_t vector_bytes(const std::vector<T>& v){return checked_size_mul(v.capacity(),sizeof(T));}
inline RfcxPayloadFootprint footprint(const core::RelationalComplex& source,std::function<void()> check) {
    RfcxPayloadFootprint out;out.decoded_bytes=checked_size_add(sizeof(source),checked_size_mul(source.grade_count(),sizeof(core::GradeArena)));
    const auto integer=[&](mpz_srcptr z){
        const auto bits=mpz_sgn(z)?mpz_sizeinbase(z,2):0;
        out.coefficient_bits=checked_size_add(out.coefficient_bits,bits);out.max_integer_bits=std::max(out.max_integer_bits,bits);
        out.decoded_bytes=checked_size_add(out.decoded_bytes,numeric::integer_limb_bytes(z));
    };
    const auto rational=[&](const numeric::Rational& q){integer(mpq_numref(q.get_mpq_t()));integer(mpq_denref(q.get_mpq_t()));};
    const auto& sig=source.signature();
    // Private signature lookup controls get a named conservative allowance;
    // public arena capacities and GNU MP retained limbs are measured directly.
    out.decoded_bytes=checked_size_add(out.decoded_bytes,checked_size_mul(sig.type_count()+sig.aspects().size(),1024));
    for(std::size_t a=0;a<sig.aspects().size();++a)
        out.decoded_bytes=checked_size_add(out.decoded_bytes,checked_size_mul(sig.aspects().at(static_cast<composite::AspectId>(a)).name.capacity()+1,2));
    for(std::size_t t=0;t<sig.type_count();++t) {
        const auto& type=sig.type(static_cast<core::RelationTypeId>(t));
        out.decoded_bytes=checked_size_add(out.decoded_bytes,checked_size_add(checked_size_mul(type.name.capacity()+1,2),vector_bytes(type.constraints.rules)));
        for(const auto& rule:type.constraints.rules){if(check){check();}rational(rule.value);}
    }
    for(std::size_t k=0;k<source.grade_count();++k) {
        if(check){check();}const auto& a=source.grade({static_cast<std::uint16_t>(k)});
        auto& n=out.decoded_bytes;
        n=checked_size_add(n,vector_bytes(a.relation_types));n=checked_size_add(n,vector_bytes(a.identities));
        n=checked_size_add(n,vector_bytes(a.relation_identities));n=checked_size_add(n,vector_bytes(a.cell_scales));
        n=checked_size_add(n,vector_bytes(a.offsets));n=checked_size_add(n,vector_bytes(a.lower_index));
        n=checked_size_add(n,vector_bytes(a.upper_index));n=checked_size_add(n,vector_bytes(a.occurrence_identities));
        n=checked_size_add(n,vector_bytes(a.lattice_coefficient));n=checked_size_add(n,vector_bytes(a.lower_offsets));
        n=checked_size_add(n,vector_bytes(a.incidence_by_lower));
        n=checked_size_add(n,checked_size_mul(sig.aspects().size(),sizeof(machine::PackedBits)));
        for(std::size_t i=0;i<sig.aspects().size();++i)n=checked_size_add(n,vector_bytes(a.aspects.plane(static_cast<composite::AspectId>(i)).words()));
        for(const auto& q:a.cell_scales){if(check){check();}rational(q);}
        for(const auto& z:a.lattice_coefficient){if(check){check();}integer(z.get_mpz_t());}
    }
    return out;
}
inline void metadata(PayloadDescriptor& p,const core::RelationalComplex& source) {
    const auto cell=[&](std::size_t k,std::size_t i){const auto& g=source.grade({static_cast<std::uint16_t>(k)});
        return PayloadCell{{static_cast<std::uint16_t>(k)},k?g.relation_identities[i].value:g.identities[i]};};
    for(std::size_t k=0;k<source.grade_count();++k) {
        const auto& g=source.grade({static_cast<std::uint16_t>(k)});
        for(std::size_t i=0;i<g.cell_count();++i) {
            p.cells.push_back(cell(k,i));if(!k){continue;}
            const auto [begin,end]=g.incidence_range(i);
            for(auto pos=begin;pos<end;++pos)p.occurrences.push_back({g.occurrence_identities[pos],cell(k,i),cell(k-1,g.lower_index[pos]),
                pos-begin,g.aspects.get(source.signature().existence_aspect(),pos)});
        }
    }
}
} // namespace rfcx_payload_detail

struct DeclaredRfcxPayload {PayloadDescriptor descriptor;std::vector<std::byte> bytes;};
inline DeclaredRfcxPayload declare_rfcx_payload(std::uint64_t name_space,std::uint64_t source_key,std::uint64_t revision,
    std::uint64_t payload_key,const core::RelationalComplex& source,RfcxPayloadLimits limits={}) {
    auto bytes=encode_rfcx(source).bytes;const auto checksum=rfcx_detail::fnv1a64(bytes);
    const auto plan=rfcx_payload_detail::inspect(bytes,limits);PayloadDescriptor p;
    p.identity={{name_space,source_key,revision,checksum},payload_key};p.format="RFCX-1";p.stored_bytes=bytes.size();p.checksum=checksum;
    p.decoded_bytes=plan.decoded_bytes;p.coefficient_bits=plan.coefficient_bits;rfcx_payload_detail::metadata(p,source);
    return {std::move(p),std::move(bytes)};
}
struct PreparedRfcxPayload {core::RelationalComplex source;RfcxPayloadFootprint footprint;};
inline PreparedRfcxPayload prepare_rfcx_payload(std::span<const std::byte> input,const PayloadManifest& original,const PayloadKey& key,
    RfcxPayloadLimits policy={},std::function<void()> check={}) {
    using namespace payload_manifest_detail;
    const auto manifest=original;const auto index=manifest.find(key);require(index.has_value(),"undeclared native source revision");
    const auto& descriptor=manifest.payloads()[*index];
    bound(input.size(),policy.max_file_bytes,"native payload file admission");
    const std::vector<std::byte> frozen(input.begin(),input.end());const std::span<const std::byte> bytes(frozen);
    require(descriptor.format=="RFCX-1" && descriptor.scope==PayloadScope::BoundaryClosed,"unsupported native payload codec/scope");
    require(bytes.size()==descriptor.stored_bytes && rfcx_detail::fnv1a64(bytes)==descriptor.checksum &&
            descriptor.identity.source.checksum==descriptor.checksum,"native payload/source receipt mismatch");
    if(check){check();}const auto plan=rfcx_payload_detail::inspect(bytes,policy);
    bound(plan.decoded_bytes,checked_size(descriptor.decoded_bytes),"native decoded declaration understates preparation");
    bound(plan.coefficient_bits,checked_size(descriptor.coefficient_bits),"native coefficient declaration understates preparation");
    MappedRfcx image(bytes);if(check){check();}auto source=image.materialize_canonical();if(check){check();}
    auto encoded=encode_rfcx(source).bytes;
    require(encoded.size()==bytes.size() && std::equal(encoded.begin(),encoded.end(),bytes.begin()),"noncanonical native payload source");
    PayloadDescriptor actual;rfcx_payload_detail::metadata(actual,source);
    require(actual.cells==descriptor.cells && actual.occurrences==descriptor.occurrences,"native payload differs from complete original occurrence metadata");
    const auto footprint=rfcx_payload_detail::footprint(source,check);
    bound(footprint.decoded_bytes,plan.decoded_bytes,"native retained storage exceeds preparation envelope");
    bound(footprint.coefficient_bits,plan.coefficient_bits,"native coefficients exceed preparation envelope");
    bound(footprint.max_integer_bits,policy.max_integer_bits,"native integer bit admission");
    return {std::move(source),footprint};
}
} // namespace rfc::storage
