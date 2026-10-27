#pragma once

#include "rfc/storage/rfcx.hpp"
#include <compare>
#include <set>

namespace rfc::storage {

// Persistent selectors, not in-process source owners or mathematical authority.
// The checksum is a corruption witness, not authentication or an identity proof.
struct PayloadSourceRevision {
    std::uint64_t name_space{}, key{}, revision{}, checksum{};
    auto operator<=>(const PayloadSourceRevision&) const = default;
};
struct PayloadKey {
    PayloadSourceRevision source;
    std::uint64_t key{};
    auto operator<=>(const PayloadKey&) const = default;
};
struct PayloadCell {
    core::Grade grade;
    core::IdentityKey key{};
    auto operator<=>(const PayloadCell&) const = default;
};
struct PayloadOccurrence {
    core::OccurrenceKey key;
    PayloadCell upper, lower;
    std::uint64_t position{};
    bool exists{true};
    bool operator==(const PayloadOccurrence&) const = default;
};
enum class PayloadScope : std::uint8_t { BoundaryClosed = 1, Contribution = 2 };
struct PayloadDescriptor {
    PayloadKey identity;
    std::string format;
    PayloadScope scope{PayloadScope::BoundaryClosed};
    std::uint64_t stored_bytes{}, checksum{}, decoded_bytes{}, coefficient_bits{};
    std::vector<PayloadCell> cells;
    std::vector<PayloadOccurrence> occurrences;
    std::vector<PayloadKey> dependencies;
    bool operator==(const PayloadDescriptor&) const = default;
};
struct PayloadManifestLimits {
    std::size_t max_encoded_bytes{8u << 20}, max_metadata_bytes{32u << 20}, max_validation_bytes{32u << 20};
    std::size_t max_sources{4096}, max_payloads{65536}, max_cells{1000000}, max_occurrences{1000000};
    std::size_t max_dependencies{1000000}, max_format_bytes{128};
    std::size_t max_validation_steps{16000000};
};
struct PayloadManifestUsage {
    std::size_t encoded_bytes{}, metadata_bytes{}, index_bytes{}, validation_bytes{};
    std::size_t sources{}, payloads{}, cells{}, occurrences{}, dependencies{};
    bool operator==(const PayloadManifestUsage&) const = default;
};
namespace payload_manifest_detail {
using machine::checked_size_add;
using machine::checked_size_mul;
using machine::checked_size;
inline void require(bool value, const char* message) { if (!value) throw std::invalid_argument(message); }
inline void bound(std::size_t value, std::size_t maximum, const char* message) {
    if (value > maximum) throw std::length_error(message);
}
inline void source(rfcx_detail::Writer& w, const PayloadSourceRevision& s) {
    w.u64(s.name_space); w.u64(s.key); w.u64(s.revision); w.u64(s.checksum);
}
inline PayloadSourceRevision source(rfcx_detail::Reader& r) { return {r.u64(), r.u64(), r.u64(), r.u64()}; }
inline void key(rfcx_detail::Writer& w, const PayloadKey& k) { source(w, k.source); w.u64(k.key); }
inline PayloadKey key(rfcx_detail::Reader& r) { const auto s = source(r); return {s, r.u64()}; }
inline void cell(rfcx_detail::Writer& w, const PayloadCell& c) { w.u16(c.grade.value); w.u64(c.key); }
inline PayloadCell cell(rfcx_detail::Reader& r) { const auto g = r.u16(); return {{g}, r.u64()}; }
inline constexpr std::uint64_t magic = 0x31464d5058434652ULL;
inline void write(rfcx_detail::Writer& w, const std::vector<PayloadSourceRevision>& sources,
                  const std::vector<PayloadDescriptor>& payloads) {
    w.u64(magic); w.u64(1); w.u64(sources.size());
    for (const auto& s : sources) source(w, s);
    w.u64(payloads.size());
    for (const auto& p : payloads) {
        key(w, p.identity); w.u64(p.format.size()); w.raw(std::as_bytes(std::span(p.format)));
        w.u8(static_cast<std::uint8_t>(p.scope));
        w.u64(p.stored_bytes); w.u64(p.checksum); w.u64(p.decoded_bytes); w.u64(p.coefficient_bits);
        w.u64(p.cells.size()); for (const auto& c : p.cells) cell(w, c);
        w.u64(p.occurrences.size());
        for (const auto& o : p.occurrences) {
            w.u64(o.key.value); cell(w, o.upper); cell(w, o.lower); w.u64(o.position); w.u8(o.exists ? 1 : 0);
        }
        w.u64(p.dependencies.size()); for (const auto& d : p.dependencies) key(w, d);
    }
}
} // namespace payload_manifest_detail

// Immutable, bounded declarations only. No payload is fetched or interpreted.
// Dependencies are complete as DECLARED; only a later typed native decoder can
// establish that they actually cover the mathematical operation/source.
class PayloadManifest {
    struct IndexEntry { PayloadKey key; std::size_t index; };
    struct State {
        std::vector<PayloadSourceRevision> sources;
        std::vector<PayloadDescriptor> payloads;
        std::vector<IndexEntry> index;
        std::vector<std::byte> bytes;
        PayloadManifestUsage usage;
    };
    std::shared_ptr<const State> state_;
    explicit PayloadManifest(std::shared_ptr<const State> s) : state_(std::move(s)) {}
    static PayloadManifestUsage measure(const std::vector<PayloadSourceRevision>& sources,
        const std::vector<PayloadDescriptor>& payloads, const PayloadManifestLimits& limits) {
        using namespace payload_manifest_detail;
        PayloadManifestUsage u; u.sources = sources.size(); u.payloads = payloads.size();
        bound(u.sources, limits.max_sources, "payload manifest source admission");
        bound(u.payloads, limits.max_payloads, "payload manifest record admission");
        u.index_bytes = checked_size_mul(payloads.size(), sizeof(IndexEntry));
        u.metadata_bytes = checked_size_add(sizeof(State), checked_size_add(u.index_bytes,
            checked_size_add(checked_size_mul(sources.size(), sizeof(PayloadSourceRevision)),
                             checked_size_mul(payloads.size(), sizeof(PayloadDescriptor)))));
        for (const auto& p : payloads) {
            bound(p.format.size(), limits.max_format_bytes, "payload manifest format admission");
            u.cells = checked_size_add(u.cells, p.cells.size());
            u.occurrences = checked_size_add(u.occurrences, p.occurrences.size());
            u.dependencies = checked_size_add(u.dependencies, p.dependencies.size());
            bound(u.cells, limits.max_cells, "payload manifest cell admission");
            bound(u.occurrences, limits.max_occurrences, "payload manifest occurrence admission");
            bound(u.dependencies, limits.max_dependencies, "payload manifest dependency admission");
            u.metadata_bytes = checked_size_add(u.metadata_bytes, checked_size_add(p.format.size(),
                checked_size_add(checked_size_mul(p.cells.size(), sizeof(PayloadCell)),
                checked_size_add(checked_size_mul(p.occurrences.size(), sizeof(PayloadOccurrence)),
                                 checked_size_mul(p.dependencies.size(), sizeof(PayloadKey))))));
            bound(u.metadata_bytes, limits.max_metadata_bytes, "payload manifest metadata admission");
        }
        // Validation sets use a conservative named-node allowance; this and
        // metadata element accounting do not claim to cap allocator/process RSS.
        const auto records = checked_size_add(u.sources, checked_size_add(u.payloads,
            checked_size_add(u.cells, checked_size_add(checked_size_mul(u.occurrences, 2), u.dependencies))));
        u.validation_bytes = checked_size_mul(records, 128);
        auto w = rfcx_detail::Writer::measure_only(); write(w, sources, payloads);
        u.encoded_bytes = checked_size_add(w.size(), 8);
        bound(u.encoded_bytes, limits.max_encoded_bytes, "payload manifest encoded admission");
        u.metadata_bytes = checked_size_add(u.metadata_bytes, u.encoded_bytes);
        bound(u.metadata_bytes, limits.max_metadata_bytes, "payload manifest metadata admission");
        u.validation_bytes = checked_size_add(u.validation_bytes, checked_size_add(u.encoded_bytes,u.metadata_bytes));
        bound(u.validation_bytes, limits.max_validation_bytes, "payload manifest validation admission");
        return u;
    }
public:
    static PayloadManifest create(const std::vector<PayloadSourceRevision>& sources,
        const std::vector<PayloadDescriptor>& payloads, PayloadManifestLimits limits = {},
        std::function<void()> check = {}) {
        using namespace payload_manifest_detail;
        const auto usage = measure(sources, payloads, limits); // admission before defensive copies/callbacks
        auto s = std::make_shared<State>(); s->sources = sources; s->payloads = payloads; s->usage = usage;
        std::size_t steps{};
        const auto tick = [&] {
            bound(checked_size_add(steps,1),limits.max_validation_steps,"payload manifest validation work admission");
            ++steps; if (check) check();
        }; tick();
        std::set<PayloadSourceRevision> source_set;
        for (const auto& src : s->sources) {
            tick(); require(src.name_space && src.key && src.revision, "payload source requires an exact revision");
            require(source_set.insert(src).second, "duplicate payload source revision");
        }
        s->index.reserve(s->payloads.size());
        for (std::size_t i = 0; i < s->payloads.size(); ++i) {
            tick(); const auto& p = s->payloads[i];
            require(source_set.contains(p.identity.source) && p.identity.key, "undeclared payload source/key");
            require(!p.format.empty(), "payload requires a declared codec profile");
            require(p.scope == PayloadScope::BoundaryClosed || p.scope == PayloadScope::Contribution, "unknown payload scope");
            (void)checked_size(p.stored_bytes); (void)checked_size(p.decoded_bytes); (void)checked_size(p.coefficient_bits);
            s->index.push_back({p.identity, i});
            std::set<PayloadCell> cells;
            for (auto c : p.cells) { tick(); require(cells.insert(c).second, "duplicate payload cell"); }
            std::set<std::pair<core::Grade, core::OccurrenceKey>> occurrence_keys;
            std::set<std::pair<PayloadCell, std::uint64_t>> positions;
            for (const auto& o : p.occurrences) {
                tick(); require(cells.contains(o.upper), "payload occurrence upper cell is undeclared");
                require(o.upper.grade.value && o.lower.grade.value == o.upper.grade.value - 1, "payload occurrence grades differ");
                require(occurrence_keys.insert({o.upper.grade, o.key}).second, "duplicate payload occurrence identity");
                require(positions.insert({o.upper, o.position}).second, "duplicate payload attaching position");
                if (p.scope == PayloadScope::BoundaryClosed)
                    require(cells.contains(o.lower), "boundary-closed payload omits an incidence participant");
            }
            std::set<PayloadKey> dependencies;
            for (const auto& d : p.dependencies) {
                tick(); require(d != p.identity && dependencies.insert(d).second, "self/duplicate payload dependency");
            }
        }
        std::sort(s->index.begin(), s->index.end(), [&](const auto& a, const auto& b) { tick(); return a.key < b.key; });
        for (std::size_t i = 1; i < s->index.size(); ++i)
            require(s->index[i-1].key != s->index[i].key, "duplicate payload identity");
        PayloadManifest result(s);
        for (const auto& p : s->payloads) for (const auto& d : p.dependencies) {
            tick(); require(result.find(d).has_value(), "payload dependency is not declared at its exact revision");
        }
        for (const auto& p : s->payloads) if (p.scope==PayloadScope::Contribution) {
            std::set<PayloadCell> available(p.cells.begin(),p.cells.end());
            for(const auto& dependency:p.dependencies) {
                tick();
                if(dependency.source!=p.identity.source) continue;
                const auto& other=s->payloads[*result.find(dependency)];
                // No implicit traversal of the whole graph: a fragment must name
                // the direct provider of each missing boundary participant.
                for(auto c:other.cells) { tick(); available.insert(c); }
            }
            for(const auto& o:p.occurrences) { tick(); require(available.contains(o.lower),"contribution omits a declared boundary dependency"); }
        }
        rfcx_detail::Writer w(usage.encoded_bytes); write(w, s->sources, s->payloads);
        w.u64(rfcx_detail::fnv1a64(w.bytes())); s->bytes = std::move(w).take_bytes(); tick();
        return result;
    }
    static PayloadManifest decode(std::span<const std::byte> bytes,
        const std::vector<PayloadSourceRevision>& expected_sources, PayloadManifestLimits limits = {},
        std::function<void()> check = {}) {
        using namespace payload_manifest_detail;
        bound(bytes.size(), limits.max_encoded_bytes, "payload manifest encoded admission");
        bound(bytes.size(), limits.max_metadata_bytes, "payload manifest receiving metadata admission");
        bound(expected_sources.size(),limits.max_sources,"payload manifest expected source admission");
        const auto input_storage=checked_size_add(bytes.size(),checked_size_mul(expected_sources.size(),sizeof(PayloadSourceRevision)));
        bound(input_storage,limits.max_validation_bytes,"payload manifest frozen input admission");
        require(bytes.size() >= 40, "truncated payload manifest");
        // Freeze before callbacks; an external mutable span never becomes retained authority.
        const std::vector<std::byte> frozen(bytes.begin(), bytes.end());
        const auto expected_snapshot=expected_sources;
        if (check) check();
        rfcx_detail::Reader checksum(std::span<const std::byte>(frozen).last(8));
        require(checksum.u64() == rfcx_detail::fnv1a64(std::span<const std::byte>(frozen).first(frozen.size()-8)), "payload manifest checksum mismatch");
        rfcx_detail::Reader r(std::span<const std::byte>(frozen).first(frozen.size()-8));
        require(r.u64() == magic && r.u64() == 1, "unsupported payload manifest version");
        std::size_t cells{}, occurrences{}, dependencies{}, allocated = input_storage, read_steps{};
        const auto count = [&](std::size_t minimum, std::size_t maximum, std::size_t object_size) {
            const auto n = r.size_value(); r.require_items(n, minimum); bound(n, maximum, "payload manifest receiving record admission");
            allocated = checked_size_add(allocated, checked_size_mul(n, object_size));
            bound(allocated, limits.max_metadata_bytes, "payload manifest receiving metadata admission");
            bound(allocated, limits.max_validation_bytes, "payload manifest receiving validation admission");
            read_steps=checked_size_add(read_steps,checked_size_add(n,1));
            bound(read_steps,limits.max_validation_steps,"payload manifest receiving work admission");
            if (check) check();
            return n;
        };
        std::vector<PayloadSourceRevision> sources;
        const auto ns = count(32, limits.max_sources, sizeof(PayloadSourceRevision)); sources.reserve(ns);
        for (std::size_t i=0; i<ns; ++i) sources.push_back(source(r));
        require(sources == expected_snapshot, "payload manifest source revisions differ from receiving snapshot");
        std::vector<PayloadDescriptor> payloads;
        const auto np = count(105, limits.max_payloads, sizeof(PayloadDescriptor)); payloads.reserve(np);
        for (std::size_t i=0; i<np; ++i) {
            PayloadDescriptor p; p.identity = key(r);
            const auto nf = count(1, limits.max_format_bytes, 1); const auto text = r.slice(nf);
            p.format.assign(reinterpret_cast<const char*>(text.data()), text.size());
            p.scope = static_cast<PayloadScope>(r.u8());
            p.stored_bytes=r.u64(); p.checksum=r.u64(); p.decoded_bytes=r.u64(); p.coefficient_bits=r.u64();
            const auto nc=count(10, limits.max_cells-cells, sizeof(PayloadCell)); cells=checked_size_add(cells,nc); p.cells.reserve(nc);
            for (std::size_t j=0; j<nc; ++j) p.cells.push_back(cell(r));
            const auto no=count(37, limits.max_occurrences-occurrences, sizeof(PayloadOccurrence)); occurrences=checked_size_add(occurrences,no); p.occurrences.reserve(no);
            for (std::size_t j=0; j<no; ++j) {
                const auto k=r.u64(); const auto upper=cell(r), lower=cell(r); const auto pos=r.u64(); const bool exists=r.boolean();
                p.occurrences.push_back({{k},upper,lower,pos,exists});
            }
            const auto nd=count(40, limits.max_dependencies-dependencies, sizeof(PayloadKey)); dependencies=checked_size_add(dependencies,nd); p.dependencies.reserve(nd);
            for (std::size_t j=0; j<nd; ++j) p.dependencies.push_back(key(r));
            payloads.push_back(std::move(p));
        }
        require(r.remaining()==0, "trailing payload manifest records");
        limits.max_validation_steps-=read_steps;
        auto result=create(sources,payloads,limits,std::move(check));
        require(result.bytes().size()==frozen.size() && std::equal(result.bytes().begin(),result.bytes().end(),frozen.begin()), "noncanonical payload manifest");
        return result;
    }
    // Reopen a previously selected declaration, including every dependency and
    // byte receipt. Recomputing a checksum cannot substitute different metadata.
    static PayloadManifest decode_exact(std::span<const std::byte> bytes, const PayloadManifest& expected,
        PayloadManifestLimits limits={}, std::function<void()> check={}) {
        const auto original=expected;
        auto result=decode(bytes,original.sources(),limits,std::move(check));
        payload_manifest_detail::require(result.bytes().size()==original.bytes().size() &&
            std::equal(result.bytes().begin(),result.bytes().end(),original.bytes().begin()),"payload manifest differs from selected original declaration");
        return result;
    }
    const std::vector<PayloadSourceRevision>& sources() const & noexcept { return state_->sources; }
    const std::vector<PayloadDescriptor>& payloads() const & noexcept { return state_->payloads; }
    std::span<const std::byte> bytes() const & noexcept { return state_->bytes; }
    const std::vector<PayloadSourceRevision>& sources() const && = delete;
    const std::vector<PayloadDescriptor>& payloads() const && = delete;
    std::span<const std::byte> bytes() const && = delete;
    PayloadManifestUsage usage() const noexcept { return state_->usage; }
    bool same_owner(const PayloadManifest& other) const noexcept { return state_ == other.state_; }
    std::optional<std::size_t> find(const PayloadKey& key) const {
        const auto i=std::lower_bound(state_->index.begin(),state_->index.end(),key,[](const auto& a,const auto& b){return a.key<b;});
        if(i==state_->index.end() || i->key!=key) return std::nullopt;
        return i->index;
    }
};
} // namespace rfc::storage
