#pragma once

#include "rfc/storage/native_accession.hpp"
#include "rfc/geometry/semantic_realization.hpp"
#include "rfc/algebra/tensor_product.hpp"
#include <numeric>

namespace rfc::storage {

// Execution policy, never a claim about the mathematics or a process-wide quota.
struct NativeAtlasLimits {
    std::size_t max_sources{64}, max_definitions{4096}, max_records{2000000};
    std::size_t max_work{4000000}, max_dimension{1000000}, max_archive_bytes{256u << 20};
    std::size_t max_integer_bytes{1u << 20}, max_path_length{64};
    SourceDeclarationLimits source_declarations{};
    NativeAccessionLimits accession{};
    RelationalCodeObservationLimits observations{};
    TemporalReadLimits temporal{};
};
namespace native_atlas_detail {
using temporal_source_detail::require;
using Q=numeric::Rational;
using Bytes=std::vector<std::byte>;
struct Budget {
    std::size_t remaining;
    void take(std::size_t n=1) {
        if(n>remaining)throw std::length_error("native atlas work/record budget");
        remaining-=n;
    }
};
inline void bound(std::size_t n,std::size_t cap,const char* message) {
    if(n>cap)throw std::length_error(message);
}
inline void rational(const Q& q,const NativeAtlasLimits& limits) {
    require(mpz_sgn(mpq_denref(q.get_mpq_t()))>0,"atlas rational denominator");
    const auto size=[](mpz_srcptr p){const auto b=mpz_sizeinbase(p,2);return b/8+(b%8!=0);};
    bound(size(mpq_numref(q.get_mpq_t())),limits.max_integer_bytes,"atlas numerator budget");
    bound(size(mpq_denref(q.get_mpq_t())),limits.max_integer_bytes,"atlas denominator budget");
    auto copy=q;copy.canonicalize();require(copy==q && copy.get_num()==q.get_num() && copy.get_den()==q.get_den(),"atlas noncanonical rational");
}
inline void matrix(const numeric::ExactMatrix& m,const NativeAtlasLimits& limits,Budget& records) {
    bound(m.rows(),limits.max_dimension,"atlas matrix rows");bound(m.cols(),limits.max_dimension,"atlas matrix columns");
    records.take(m.nonzeros());for(const auto& [ij,q]:m.entries()){(void)ij;rational(q,limits);}
}
inline numeric::ExactMatrix multiply(const numeric::ExactMatrix& a,const numeric::ExactMatrix& b,Budget& work) {
    require(a.cols()==b.rows(),"atlas matrix composition dimensions");
    // Sparse multiplication without allocating b.rows() empty vectors. The work
    // budget covers both scanned entries and actual multiply/add pairs.
    std::map<std::size_t,std::vector<std::pair<std::size_t,const Q*>>> rows;
    work.take(b.nonzeros());for(const auto& [ij,q]:b.entries())rows[ij.first].push_back({ij.second,&q});
    numeric::ExactMatrix out(a.rows(),b.cols());
    for(const auto& [ij,q]:a.entries()) {
        work.take();const auto it=rows.find(ij.second);if(it==rows.end())continue;
        for(const auto& [j,v]:it->second){work.take();out.set(ij.first,j,out.get(ij.first,j)+q*(*v));}
    }
    return out;
}
inline NativeAccessionRequest tensor_request(std::uint64_t key) {
    NativeAccessionRequest r;r.kind=NativeAccessionKind::Tensor;r.subject=key;return r;
}
inline NativeAccessionRequest value_request(std::uint64_t key) {
    NativeAccessionRequest r;r.kind=NativeAccessionKind::DeclaredValue;r.subject=key;return r;
}
} // namespace native_atlas_detail

// Sources enter via immutable F3 pins. The endpoint constructor first gives F3
// a defensive copy, so callers cannot retain mutable aliases into publication.
class NativeAtlasSource {
public:
    NativeAtlasSource(std::uint64_t key,const NativeAccessionDirectory& directory)
        :key_(key),queries_(std::make_shared<const std::vector<RelationalCodeObservationQuery>>(directory.queries())),anchor_(directory.pin(NativeAccessionRequest::sparse_chain(core::Grade{0},{},{}))) {
        native_atlas_detail::require(key!=0,"atlas source key required");
    }
    NativeAtlasSource(std::uint64_t key,std::uint64_t name_space,const TemporalEndpoint& endpoint,
                      NativeAccessionLimits limits={},std::vector<RelationalCodeObservationQuery> queries={},
                      RelationalCodeObservationLimits observation_limits={})
        :NativeAtlasSource(key,NativeAccessionDirectory(endpoint,name_space,std::move(queries),
                          [](auto b){return rfcx_detail::fnv1a64(b);},limits,observation_limits)) {}
    std::vector<RelationalCodeObservationQuery> queries() const{return *queries_;}
    std::uint64_t key() const noexcept{return key_;}
    std::uint64_t name_space() const{return anchor_.descriptor().name_space();}
    std::uint64_t sequence() const{return anchor_.sequence();}
    std::shared_ptr<const TemporalEndpoint> endpoint() const{return anchor_.endpoint();}
    PinnedNativeAccession access(const NativeAccessionRequest& request) const{return anchor_.access(request);}
    PinnedNativeCell cell(StableCellKey key) const {
        return access(NativeAccessionRequest::sparse_chain(key.grade,{key.key},{})).cell(key.key);
    }
private:
    std::uint64_t key_;
    std::shared_ptr<const std::vector<RelationalCodeObservationQuery>> queries_;
    PinnedNativeAccession anchor_;
};
struct NativeAtlasCell {
    std::uint64_t source{}; StableCellKey cell;
    auto operator<=>(const NativeAtlasCell&) const = default;
};
struct NativeAtlasOccurrence {
    std::uint64_t source{}; StableOccurrenceKey occurrence;
    auto operator<=>(const NativeAtlasOccurrence&) const = default;
};
struct NativeCellImage {StableCellKey from,to; bool operator==(const NativeCellImage&)const=default;};
struct NativeOccurrenceImage {StableOccurrenceKey from,to;bool operator==(const NativeOccurrenceImage&)const=default;};

// A total injective source map, not a guessed equality match. It retains every
// occurrence and must biject each image fiber, including inactive/zero legs.
struct NativeSourceEmbedding {
    std::uint64_t key{},from{},to{};
    std::optional<SourceDeclarationMigration> declarations;
    std::vector<NativeCellImage> cells;
    std::vector<NativeOccurrenceImage> occurrences;
};
struct NativePushoutDeclaration {
    std::uint64_t key{},left_embedding{},right_embedding{};
    // Optional recodings put the entire participants into one admitted signature.
    // Their interface restrictions are checked after recoding, never assumed.
    std::optional<SourceDeclarationMigration> left_common,right_common;
};
struct NativeChainTerm {StableCellKey target;numeric::Rational coefficient{0};};
struct NativeChainColumn {StableCellKey source;std::vector<NativeChainTerm> image;};
struct NativeScaleMap {
    std::uint64_t key{},from{},to{};
    std::vector<NativeChainColumn> realization;
    std::optional<std::vector<NativeChainColumn>> retraction;
};
struct NativeScalePathEquality {
    std::uint64_t key{};
    std::vector<std::uint64_t> left,right; // execution order, not commutative sets
};
struct NativeProductManifest {
    std::uint64_t key{},left_source{},right_source{},left_factor{},right_factor{};
    // This is the native lazy CHAIN product. It does not invent a product-closed
    // source type constructor for arbitrary signatures.
};
struct NativeProductCell {
    StableCellKey left,right;
    auto operator<=>(const NativeProductCell&) const = default;
    core::Grade grade() const {
        const auto n=static_cast<std::uint32_t>(left.grade.value)+right.grade.value;
        if(n>65535u)throw std::overflow_error("atlas product grade overflow");
        return core::Grade{static_cast<std::uint16_t>(n)};
    }
};
struct NativeProductOccurrence {
    NativeProductCell upper,lower;
    std::uint64_t factor{},source{};
    StableOccurrenceKey occurrence;
    bool right{},koszul_flip{},exists{};
    numeric::Rational coefficient{0};
};
// Explicit finite support; zeros survive cancellation. A sum of entries is not
// recast as one pure product. No flat Cartesian dimension exists in this type.
struct NativeProductChain {
    core::Grade grade;
    std::map<NativeProductCell,numeric::Rational> values;
};
struct NativeLinearInterfaceLeg {
    std::uint64_t source{},field{};
    numeric::ExactMatrix restriction;
    std::optional<std::uint64_t> operation; // complete endpoint endomorphism
};
struct NativeLinearInterface {
    std::uint64_t key{},field_identity{},common_source{};
    composite::DeclarationId common_type; // an owned F1 rational tensor type
    std::vector<NativeLinearInterfaceLeg> legs;
    std::optional<numeric::ExactMatrix> common_operation;
};
struct NativeFiniteInterfaceLeg {
    std::uint64_t source{},value{};
    std::vector<std::pair<std::uint64_t,std::uint64_t>> restriction;
};
struct NativeFiniteInterface {
    std::uint64_t key{},field_identity{},common_source{};
    composite::DeclarationId common_type;
    std::vector<NativeFiniteInterfaceLeg> legs;
};
struct NativeAtlasManifest {
    std::uint64_t key{},revision{};
    std::vector<NativeSourceEmbedding> embeddings;
    std::vector<NativePushoutDeclaration> pushouts;
    std::vector<NativeProductManifest> products;
    std::vector<NativeScaleMap> scale_maps;
    std::vector<NativeScalePathEquality> scale_paths;
    std::vector<NativeLinearInterface> linear_interfaces;
    std::vector<NativeFiniteInterface> finite_interfaces;
};
struct NativeLinearInterfaceReport {
    std::vector<numeric::RationalVector> restrictions;
    std::vector<numeric::RationalVector> differences; // leg_i minus first, i>=1
    std::vector<numeric::ExactMatrix> operation_defects;
    bool compatible{true};
    std::optional<bool> operations_descend;
};
struct NativeFiniteInterfaceReport {
    std::vector<std::uint64_t> restrictions;
    bool compatible{true};
};
struct NativeGluedSource {
    core::RelationalComplex source;
    // Every member retained, including equal payloads with different identities.
    std::map<NativeAtlasCell,StableCellKey> cells;
    std::map<NativeAtlasOccurrence,StableOccurrenceKey> occurrences;
};
struct NativeContextReport {
    std::vector<std::size_t> missing_input;
    bool sufficient() const noexcept{return missing_input.empty();}
};
} // namespace rfc::storage
