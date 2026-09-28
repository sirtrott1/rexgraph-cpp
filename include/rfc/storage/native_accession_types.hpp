#pragma once

#include "rfc/storage/temporal_seed.hpp"
#include <functional>
#include <memory>
#include <unordered_map>

namespace rfc::storage {

// Rebuildable access paths over one immutable endpoint. These are not new
// primary identities, schema definitions or a serialization of the vector space.
enum class NativeAccessionKind : std::uint8_t {
    Chain, Cochain, Field, Tensor, Operator, LocalComplex, DeclaredValue,
    SourceAspects, CodeSection, CodeOperation, Observation
};
enum class NativePairing : std::uint8_t { None, AlgebraicDual };
enum class NativeMissingPolicy : std::uint8_t { Reject, Dormant };
struct NativeCoefficient {
    std::uint64_t cell{};
    numeric::Rational value{0};
};
struct NativeAccessionRequest {
    NativeAccessionKind kind{NativeAccessionKind::Tensor};
    // Tensor/value/block/layout/code key. For Observation this is the query key;
    // secondary is its subject. Inline chain/cochain values use neither key.
    std::uint64_t subject{}, secondary{};
    core::Grade grade{};
    NativePairing pairing{NativePairing::None};
    NativeMissingPolicy missing{NativeMissingPolicy::Reject};
    // Stable cell keys in the declared grade, not arena offsets. Full support
    // is lazy. An explicit empty selection is different from full support.
    bool all_cells{true}, inline_value{};
    std::vector<std::uint64_t> cells;
    std::vector<NativeCoefficient> coefficients;
    // Stable product factor path on a declared value; never array offsets.
    std::vector<std::uint64_t> factors;
    std::optional<StableOccurrenceKey> occurrence;

    static NativeAccessionRequest chain(std::uint64_t tensor, core::Grade grade) {
        NativeAccessionRequest r; r.kind=NativeAccessionKind::Chain; r.subject=tensor; r.grade=grade; return r;
    }
    static NativeAccessionRequest cochain(std::uint64_t tensor, core::Grade grade) {
        auto r=chain(tensor,grade); r.kind=NativeAccessionKind::Cochain; r.pairing=NativePairing::AlgebraicDual; return r;
    }
    static NativeAccessionRequest sparse_chain(core::Grade grade, std::vector<std::uint64_t> support,
                                               std::vector<NativeCoefficient> values) {
        auto r=chain(0,grade); r.inline_value=true; r.all_cells=false;
        r.cells=std::move(support); r.coefficients=std::move(values); return r;
    }
};
struct NativeAccessionLimits {
    std::size_t max_handles{100000}, max_bucket_entries{4096};
    std::size_t max_index_entries{4000000}, max_selection_cells{1000000};
    std::size_t max_descriptor_bytes{16u << 20}, max_total_descriptor_bytes{128u << 20};
    std::size_t max_binding_bytes{256u << 20}, max_factor_depth{64};
    std::size_t max_integer_bytes{1u << 20}, max_action_work{4000000};
};
struct NativeAccessionStats {
    std::size_t source_cells{}, source_occurrences{}, indexed_records{}, binding_bytes{};
    std::size_t handles{}, dormant{}, descriptor_bytes{};
    std::size_t handle_resolutions{}, descriptor_hashes{}, bucket_comparisons{}, cold_bindings{};
};
class NativeSubjectMissing : public std::runtime_error {
public:
    explicit NativeSubjectMissing(const char* message) : std::runtime_error(message) {}
};

namespace native_accession_detail {
using Bytes=std::vector<std::byte>;
using temporal_source_detail::require;
inline void budget(std::size_t n, std::size_t limit, const char* message) {
    if(n>limit) throw std::length_error(message);
}
inline bool chain_kind(NativeAccessionKind k) {
    return k==NativeAccessionKind::Chain || k==NativeAccessionKind::Cochain;
}
inline void preflight(const NativeAccessionRequest& r,const NativeAccessionLimits& limits) {
    budget(r.cells.size(),limits.max_selection_cells,"accession selection budget");
    budget(r.coefficients.size(),limits.max_selection_cells,"accession coefficient budget");
    budget(r.factors.size(),limits.max_factor_depth,"accession factor depth budget");
    std::size_t size=49;
    size=machine::checked_size_add(size,machine::checked_size_mul(r.cells.size(),8));
    size=machine::checked_size_add(size,machine::checked_size_mul(r.factors.size(),8));
    if(r.occurrence)size=machine::checked_size_add(size,10);
    for(const auto& c:r.coefficients) {
        require(mpz_sgn(mpq_denref(c.value.get_mpq_t()))>0,"invalid inline rational denominator");
        const auto bytes=[](mpz_srcptr value) {
            if(mpz_sgn(value)==0)return std::size_t{0};
            const auto bits=mpz_sizeinbase(value,2);return bits/8+(bits%8!=0);
        };
        const auto n=bytes(mpq_numref(c.value.get_mpq_t())),d=bytes(mpq_denref(c.value.get_mpq_t()));
        budget(n,limits.max_integer_bytes,"inline numerator budget");budget(d,limits.max_integer_bytes,"inline denominator budget");
        size=machine::checked_size_add(size,18);size=machine::checked_size_add(size,n);size=machine::checked_size_add(size,d);
    }
    budget(size,limits.max_descriptor_bytes,"accession descriptor byte budget");
}
inline void canonicalize(NativeAccessionRequest& r, const NativeAccessionLimits& limits) {
    preflight(r,limits);
    require(static_cast<std::uint8_t>(r.kind)<=static_cast<std::uint8_t>(NativeAccessionKind::Observation),"unknown accession kind");
    require(r.missing==NativeMissingPolicy::Reject || r.missing==NativeMissingPolicy::Dormant,"unknown missing-subject policy");
    require(r.pairing==(r.kind==NativeAccessionKind::Cochain?NativePairing::AlgebraicDual:NativePairing::None),
            "accession must declare algebraic dual, not implicit metric identification");
    budget(r.cells.size(),limits.max_selection_cells,"accession selection budget");
    budget(r.coefficients.size(),limits.max_selection_cells,"accession coefficient budget");
    budget(r.factors.size(),limits.max_factor_depth,"accession factor depth budget");
    if(!chain_kind(r.kind)) {
        require(r.grade.value==0 && r.all_cells && r.cells.empty() && !r.inline_value && r.coefficients.empty(),
                "non-chain accession has chain metadata");
    } else {
        require(r.all_cells?r.cells.empty():true,"full selection has explicit cells");
        require(r.inline_value?r.subject==0:r.coefficients.empty(),"invalid inline chain metadata");
        std::sort(r.cells.begin(),r.cells.end());
        require(std::adjacent_find(r.cells.begin(),r.cells.end())==r.cells.end(),"duplicate accession support cell");
        std::sort(r.coefficients.begin(),r.coefficients.end(),[](const auto& a,const auto& b){return a.cell<b.cell;});
        for(std::size_t i=0;i<r.coefficients.size();++i) {
            auto& c=r.coefficients[i];
            require(!i || r.coefficients[i-1].cell!=c.cell,"duplicate inline coefficient");
            require(c.value.get_den()>0,"invalid inline rational denominator");
            
            
            c.value.canonicalize();
            require(r.all_cells || std::binary_search(r.cells.begin(),r.cells.end(),c.cell),"inline coefficient outside support");
        }
        // Numerical zeros are canonical implicit zeros; their support survives.
        std::erase_if(r.coefficients,[](const auto& c){return c.value==0;});
    }
    require(r.kind==NativeAccessionKind::DeclaredValue || r.factors.empty(),"factor path requires declared value");
    require((r.kind==NativeAccessionKind::SourceAspects)==r.occurrence.has_value(),"source-aspect occurrence metadata");
    require(r.kind==NativeAccessionKind::Observation || r.secondary==0,"extraneous accession secondary key");
}
inline Bytes request_bytes(const NativeAccessionRequest& r, const NativeAccessionLimits& limits) {
    std::size_t estimate=49;
    estimate=machine::checked_size_add(estimate,machine::checked_size_mul(r.cells.size(),8));
    estimate=machine::checked_size_add(estimate,machine::checked_size_mul(r.factors.size(),8));
    if(r.occurrence) estimate=machine::checked_size_add(estimate,10);
    for(const auto& c:r.coefficients) {
        const auto n=composite::declaration_detail::integer_bits(c.value.get_num());
        const auto d=composite::declaration_detail::integer_bits(c.value.get_den());
        estimate=machine::checked_size_add(estimate,18);
        estimate=machine::checked_size_add(estimate,n/8+(n%8!=0));
        estimate=machine::checked_size_add(estimate,d/8+(d%8!=0));
    }
    budget(estimate,limits.max_descriptor_bytes,"accession descriptor byte budget");
    rfcx_detail::Writer w;
    // Canonical in-process descriptor grammar, NOT an additional persisted file.
    w.u8(1); w.u8(static_cast<std::uint8_t>(r.kind)); w.u64(r.subject); w.u64(r.secondary);
    w.u16(r.grade.value); w.u8(static_cast<std::uint8_t>(r.pairing)); w.u8(static_cast<std::uint8_t>(r.missing));
    w.u8(r.all_cells?1:0); w.u8(r.inline_value?1:0); w.u64(r.cells.size());
    for(auto key:r.cells) w.u64(key);
    w.u64(r.coefficients.size());
    for(const auto& c:r.coefficients) {
        w.u64(c.cell); rfcx_detail::write_rational(w,c.value);
        budget(w.size(),limits.max_descriptor_bytes,"accession descriptor byte budget");
    }
    w.u64(r.factors.size()); for(auto key:r.factors) w.u64(key);
    w.u8(r.occurrence?1:0);
    if(r.occurrence) { w.u16(r.occurrence->upper_grade.value); w.u64(r.occurrence->key.value); }
    budget(w.size(),limits.max_descriptor_bytes,"accession descriptor byte budget");
    return w.bytes();
}
struct Binding {
    std::uint64_t name_space{}, sequence{};
    // The immutable endpoint is the exact witness. Fingerprints only reject
    // unequal candidates. We do not retain a second copy of source bitplanes
    // merely to accelerate rare cross-snapshot descriptor comparisons.
    std::shared_ptr<const TemporalEndpoint> endpoint;
    std::uint64_t source_fingerprint{}, endpoint_fingerprint{};
    std::size_t source_bytes{}, endpoint_bytes{};
    Bytes queries;
    bool operator==(const Binding& b) const {
        if(name_space!=b.name_space || sequence!=b.sequence || queries!=b.queries ||
           source_fingerprint!=b.source_fingerprint || endpoint_fingerprint!=b.endpoint_fingerprint ||
           source_bytes!=b.source_bytes || endpoint_bytes!=b.endpoint_bytes)return false;
        if(endpoint==b.endpoint)return true;
        // This is a cold exact-context comparison, never a known-handle lookup.
        // Equal hashes alone cannot authorize foreign data or interpretation.
        if(encode_rfcx(endpoint->source).bytes!=encode_rfcx(b.endpoint->source).bytes)return false;
        return encode_temporal_endpoint(*endpoint)==encode_temporal_endpoint(*b.endpoint);
    }
};
struct Snapshot;
struct Entry;
struct Generation {};
} // namespace native_accession_detail

class NativeAccessionDirectory;
class PinnedNativeAccession;
class NativeAccessionHandle {
public:
    NativeAccessionHandle(const NativeAccessionHandle&) = default;
    NativeAccessionHandle& operator=(const NativeAccessionHandle&) = default;
    NativeAccessionHandle()=default;
    bool operator==(const NativeAccessionHandle& b) const noexcept {
        return slot_==b.slot_ && !generation_.owner_before(b.generation_) && !b.generation_.owner_before(generation_);
    }
private:
    std::weak_ptr<const native_accession_detail::Generation> generation_;
    std::size_t slot_{};
    NativeAccessionHandle(const std::shared_ptr<const native_accession_detail::Generation>& g,std::size_t i)
        : generation_(g),slot_(i) {}
    friend class NativeAccessionDirectory;
};
class NativeAccessionDescriptor {
public:
    NativeAccessionDescriptor(const NativeAccessionDescriptor&) = default;
    NativeAccessionDescriptor& operator=(const NativeAccessionDescriptor&) = default;
    std::uint64_t name_space() const noexcept { return binding_->name_space; }
    std::uint64_t sequence() const noexcept { return binding_->sequence; }
    const NativeAccessionRequest& request() const & noexcept { return request_; }
    const NativeAccessionRequest& request() const && = delete;
    const std::vector<std::byte>& key_bytes() const & noexcept { return bytes_; }
    const std::vector<std::byte>& key_bytes() const && = delete;
    bool operator==(const NativeAccessionDescriptor& b) const {
        return bytes_==b.bytes_ && (binding_==b.binding_ || *binding_==*b.binding_);
    }
private:
    std::shared_ptr<const native_accession_detail::Binding> binding_;
    NativeAccessionRequest request_;
    std::vector<std::byte> bytes_;
    NativeAccessionDescriptor(std::shared_ptr<const native_accession_detail::Binding> b,
                             NativeAccessionRequest r,std::vector<std::byte> bytes)
        : binding_(std::move(b)),request_(std::move(r)),bytes_(std::move(bytes)) {}
    friend class NativeAccessionDirectory;
    friend class PinnedNativeAccession;
};
} // namespace rfc::storage
