#pragma once

#include "rfc/storage/relational_codes.hpp"
#include "rfc/storage/temporal_segments.hpp"
#include "rfc/storage/native_series.hpp"
#include <memory>

namespace rfc::storage {

// These are observations of the retained code layer, not source/operation
// identities. A query identity supplies the index namespace; the subject's
// retained identity supplies RelationalCodeIdentity::section.
enum class RelationalCodeObservationKind : std::uint8_t { Section, Operation, Application, Interface };
struct RelationalCodeObservationQuery {
    std::uint64_t key{};
    RelationalCodeObservationKind kind{RelationalCodeObservationKind::Section};
    // Used only for Application. The complete operation closure is observed,
    // including directions not exercised by the present section value.
    std::uint64_t operation{};
    // Section/Application only. Empty subjects selects every matching present
    // and future subject. Explicit keys may be dormant before birth/after death.
    std::optional<std::uint64_t> field;
    std::vector<std::uint64_t> subjects;
    std::size_t work_budget{1000000};
    bool operator==(const RelationalCodeObservationQuery &) const = default;
};
struct RelationalCodeObservationLimits {
    std::size_t max_queries{10000}, max_observations{1000000};
    std::size_t max_dependency_edges{4000000}, max_dependencies{4000000};
    std::size_t max_canonical_bytes{256u * 1024u * 1024u};
    std::size_t max_work_per_observation{1000000};
};
struct RelationalCodeObservation {
    std::vector<std::byte> bytes;
    // Present only for a declared Interface's matching-section test. False is
    // retained obstruction, not a failed replay, repair or curvature verdict.
    std::optional<bool> compatible;
};
struct RelationalCodeObservationStats {
    std::size_t added{}, removed{}, invalidated{}, evaluated{}, reused{}, bytes_changed{};
    std::size_t dependencies{}, dependency_edges{}, changed_dependencies{}, compared_dependencies{};
    std::size_t canonical_bytes{}, operation_applications{};
};

namespace relational_observation_detail {
using temporal_source_detail::require;
using Bytes = std::vector<std::byte>;
using Kind = RelationalCodeObservationKind;
using Id = RelationalCodeIdentity;
using Writer = temporal_codec_detail::Writer;
enum class FactKind : std::uint8_t { Namespace, Type, Operation, Section, Interface, SourceAspect };
struct Fact {
    FactKind kind{};
    std::uint64_t key{};
    std::uint16_t grade{};
    std::string aspect;
    auto operator<=>(const Fact &) const = default;
};
using Dependencies = std::set<Fact>;
struct Descriptor {
    Kind kind{};
    std::uint64_t subject{}, operation{};
    std::size_t budget{};
    bool operator==(const Descriptor &) const = default;
};
struct Entry {
    Descriptor descriptor;
    Dependencies dependencies;
    RelationalCodeObservation observation;
};
inline void blob(Writer &w, std::span<const std::byte> bytes) {
    w.u64(bytes.size());
    w.raw(bytes);
}
inline void account(std::size_t &total, std::size_t n, std::size_t limit, const char *message) {
    require(total <= limit && n <= limit - total, message);
    total += n;
}

// One immutable endpoint context per batch. Indexes, source resolution and
// declaration closures are shared across subjects, not rebuilt per observation.
// Construction follows endpoint validation; this class stays internal.
class Context {
  public:
    explicit Context(const TemporalEndpoint &endpoint) : endpoint_(endpoint), source_(endpoint.source) {
        if (endpoint.codes) {
            algebra_ = std::make_unique<RelationalCodeAlgebra>(*endpoint.codes);
            sections_ = relational_code_detail::index(endpoint.codes->sections);
            interfaces_ = relational_code_detail::index(endpoint.codes->interfaces);
        }
    }
    const RelationalCodeAlgebra &algebra() const {
        require(algebra_ != nullptr, "observation needs a code layer");
        return *algebra_;
    }
    const RelationalCodeSection &section(std::uint64_t key) const { return *sections_.at(key); }
    const RelationalCodeInterface &interface(std::uint64_t key) const { return *interfaces_.at(key); }
    const Dependencies &type_dependencies(std::uint64_t key) {
        if (const auto it = type_deps_.find(key); it != type_deps_.end()) return it->second;
        Dependencies out{{FactKind::Type, key, 0, {}}};
        for (const auto &factor : algebra().type(key).factors) {
            const auto &deps = type_dependencies(factor.type);
            out.insert(deps.begin(), deps.end());
        }
        return type_deps_.emplace(key, std::move(out)).first->second;
    }
    const Dependencies &operation_dependencies(std::uint64_t key) {
        if (const auto it = op_deps_.find(key); it != op_deps_.end()) return it->second;
        const auto &op = algebra().operation(key);
        Dependencies out{{FactKind::Operation, key, 0, {}}};
        const auto merge = [&](const Dependencies &deps) { out.insert(deps.begin(), deps.end()); };
        merge(type_dependencies(op.input));
        merge(type_dependencies(op.output));
        // FiniteMap::word holds symbols, not operation references.
        if (op.opcode == RelationalCodeOpcode::Word)
            for (auto id : op.word) merge(operation_dependencies(id));
        for (const auto &leg : op.legs) merge(operation_dependencies(leg.operation));
        return op_deps_.emplace(key, std::move(out)).first->second;
    }
    const Dependencies &section_dependencies(std::uint64_t key) {
        if (const auto it = section_deps_.find(key); it != section_deps_.end()) return it->second;
        const auto &s = section(key);
        Dependencies out = type_dependencies(s.type);
        out.insert({FactKind::Section, key, 0, {}});
        if (s.source_aspects)
            for (const auto &factor : algebra().type(s.type).factors)
                out.insert({FactKind::SourceAspect, s.occurrence->key.value,
                            s.occurrence->upper_grade.value, factor.source_aspect});
        return section_deps_.emplace(key, std::move(out)).first->second;
    }
    Dependencies dependencies(const Descriptor &d) {
        Dependencies out{{FactKind::Namespace, 0, 0, {}}};
        const auto merge = [&](const Dependencies &deps) { out.insert(deps.begin(), deps.end()); };
        if (d.kind == Kind::Operation) merge(operation_dependencies(d.subject));
        else if (d.kind == Kind::Interface) {
            const auto &i = interface(d.subject);
            out.insert({FactKind::Interface, i.key, 0, {}});
            merge(section_dependencies(i.left));
            merge(section_dependencies(i.right));
            merge(operation_dependencies(i.left_restriction));
            merge(operation_dependencies(i.right_restriction));
        } else {
            merge(section_dependencies(d.subject));
            if (d.kind == Kind::Application) {
                require(algebra().operation(d.operation).input == section(d.subject).type,
                        "observation application has incompatible input type");
                merge(operation_dependencies(d.operation));
            }
        }
        return out;
    }
    Bytes fact_bytes(const Fact &f) const {
        Writer w;
        switch (f.kind) {
        case FactKind::Namespace: w.u64(endpoint_.codes->namespace_key); break;
        case FactKind::Type: temporal_codec_detail::code_type(w, algebra().type(f.key)); append_declared_bindings(w, Dependencies{f}); break;
        case FactKind::Operation: temporal_codec_detail::code_operation(w, algebra().operation(f.key)); break;
        case FactKind::Section: temporal_codec_detail::code_section(w, section(f.key)); break;
        case FactKind::Interface: temporal_codec_detail::code_interface(w, interface(f.key)); break;
        case FactKind::SourceAspect: w.u8(source_bit(f.grade, f.key, f.aspect)); break;
        }
        return w.bytes();
    }
    const RelationalCodeValue &resolved(std::uint64_t key) {
        if (const auto it = values_.find(key); it != values_.end()) return it->second;
        const auto &s = section(key);
        if (!s.source_aspects) return s.value;
        RelationalCodeValue::Product value;
        for (const auto &factor : algebra().type(s.type).factors)
            value.push_back(RelationalCodeValue{static_cast<std::uint64_t>(source_bit(
                s.occurrence->upper_grade.value, s.occurrence->key.value, factor.source_aspect))});
        return values_.emplace(key, RelationalCodeValue{std::move(value)}).first->second;
    }
    const Bytes &operation_bytes(std::uint64_t key) {
        if (const auto it = operations_.find(key); it != operations_.end()) return it->second;
        RelationalCodeState declaration;
        declaration.namespace_key = endpoint_.codes->namespace_key;
        for (const auto &fact : operation_dependencies(key)) {
            if (fact.kind == FactKind::Type) declaration.types.push_back(algebra().type(fact.key));
            if (fact.kind == FactKind::Operation) declaration.operations.push_back(algebra().operation(fact.key));
        }
        Writer w;
        w.u64(0x31504f444f435252ULL); // Existing v1 operation observation.
        w.u64(key);
        temporal_codec_detail::code_state(w, declaration);
        append_declared_bindings(w, operation_dependencies(key));
        return operations_.emplace(key, w.bytes()).first->second;
    }
    RelationalCodeObservation evaluate(const Descriptor &d) {
        if (d.kind == Kind::Operation) return {operation_bytes(d.subject), std::nullopt};
        if (d.kind == Kind::Section)
            return {section_bytes(section(d.subject), resolved(d.subject), section(d.subject).type), std::nullopt};
        Writer w;
        if (d.kind == Kind::Application) {
            const auto &s = section(d.subject);
            const auto &op = algebra().operation(d.operation);
            require(op.input == s.type, "observation application has incompatible input type");
            auto value = apply(d.operation, resolved(d.subject), d.budget);
            w.u64(0x314c505041435252ULL); // RR CAPPL1, domain-separated application envelope.
            blob(w, operation_bytes(d.operation));
            blob(w, section_bytes(s, value, op.output));
            return {w.bytes(), std::nullopt};
        }
        const auto &i = interface(d.subject);
        auto left = apply(i.left_restriction, resolved(i.left), d.budget);
        auto right = apply(i.right_restriction, resolved(i.right), d.budget);
        const bool matching = relational_code_detail::value_equal(left, right);
        w.u64(0x3145434146435252ULL); // RR CFACE1, ordered declared-interface observation.
        w.u64(endpoint_.codes->namespace_key);
        temporal_codec_detail::code_interface(w, i);
        blob(w, operation_bytes(i.left_restriction));
        blob(w, operation_bytes(i.right_restriction));
        blob(w, section_bytes(section(i.left), left, algebra().operation(i.left_restriction).output));
        blob(w, section_bytes(section(i.right), right, algebra().operation(i.right_restriction).output));
        w.u8(matching);
        return {w.bytes(), matching};
    }
    std::size_t operation_applications() const noexcept { return applications_; }

  private:
    void append_declared_bindings(Writer& w, const Dependencies& dependencies) const {
        if (!endpoint_.declarations) return;
        std::vector<std::pair<std::uint64_t,Bytes>> bindings;
        for (const auto& fact : dependencies) if (fact.kind == FactKind::Type) {
            auto bytes = bound_code_declaration_bytes(*endpoint_.declarations,fact.key);
            if (!bytes.empty()) bindings.emplace_back(fact.key,std::move(bytes));
        }
        if (bindings.empty()) return; // Exact legacy observation bytes.
        w.u64(0x3144434e554f4252ULL); // domain-separated owning declaration bindings
        w.u64(bindings.size());
        for (const auto& [key,bytes] : bindings) { w.u64(key); blob(w,bytes); }
    }
    bool source_bit(std::uint16_t grade, std::uint64_t key, const std::string &name) const {
        const StableOccurrenceKey occurrence{core::Grade{grade}, core::OccurrenceKey{key}};
        const auto slot = source_.occurrence(occurrence);
        require(slot.has_value(), "observation source occurrence absent");
        const auto aspect = endpoint_.source.signature().aspects().id(name);
        return endpoint_.source.grade(core::Grade{grade}).aspects.get(aspect, *slot);
    }
    RelationalCodeValue apply(std::uint64_t op, const RelationalCodeValue &value, std::size_t budget) {
        ++applications_;
        return algebra().apply(op, value, budget);
    }
    Bytes section_bytes(const RelationalCodeSection &s, const RelationalCodeValue &value, std::uint64_t type) {
        const auto key = std::make_pair(s.field, type);
        auto it = type_prefixes_.find(key);
        if (it == type_prefixes_.end()) {
            RelationalCodeState declaration;
            declaration.namespace_key = endpoint_.codes->namespace_key;
            declaration.fields = {s.field};
            for (const auto &fact : type_dependencies(type)) declaration.types.push_back(algebra().type(fact.key));
            Writer prefix;
            prefix.u64(0x31444c4549464352ULL); // Existing v1 section observation.
            temporal_codec_detail::code_state(prefix, declaration);
            append_declared_bindings(prefix, type_dependencies(type));
            it = type_prefixes_.emplace(key, prefix.bytes()).first;
        }
        Writer w;
        w.raw(it->second);
        w.u64(type);
        w.u64(s.block);
        w.u8(s.cell.has_value());
        if (s.cell) temporal_codec_detail::cell(w, *s.cell);
        w.u8(s.occurrence.has_value());
        if (s.occurrence) temporal_codec_detail::occurrence(w, *s.occurrence);
        temporal_codec_detail::code_value(w, value);
        return w.bytes();
    }
    const TemporalEndpoint &endpoint_;
    StableSourceIndex source_;
    std::unique_ptr<RelationalCodeAlgebra> algebra_;
    std::map<std::uint64_t, const RelationalCodeSection *> sections_;
    std::map<std::uint64_t, const RelationalCodeInterface *> interfaces_;
    std::map<std::uint64_t, Dependencies> type_deps_, op_deps_, section_deps_;
    std::map<std::uint64_t, RelationalCodeValue> values_;
    std::map<std::uint64_t, Bytes> operations_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, Bytes> type_prefixes_;
    std::size_t applications_{};
};

inline void validate_queries(std::vector<RelationalCodeObservationQuery> &queries,
                             const RelationalCodeObservationLimits &limits) {
    require(queries.size() <= limits.max_queries, "observation query limit");
    std::set<std::uint64_t> seen;
    for (auto &q : queries) {
        require(q.key != 0 && seen.insert(q.key).second, "zero/duplicate observation query key");
        require(q.kind <= Kind::Interface, "unknown observation kind");
        require((q.kind == Kind::Application) == (q.operation != 0), "observation operation lane mismatch");
        require(!q.field || (*q.field != 0 && (q.kind == Kind::Section || q.kind == Kind::Application)),
                "invalid observation field filter");
        require(q.work_budget != 0 && q.work_budget <= limits.max_work_per_observation,
                "observation work budget exceeds limit");
        std::sort(q.subjects.begin(), q.subjects.end());
        require(std::adjacent_find(q.subjects.begin(), q.subjects.end()) == q.subjects.end() &&
                    (q.subjects.empty() || q.subjects.front() != 0), "zero/duplicate observation subject");
    }
    std::sort(queries.begin(), queries.end(), [](const auto &a, const auto &b) { return a.key < b.key; });
}
inline std::map<Id, Descriptor> select(const TemporalEndpoint &endpoint,
                                       const std::vector<RelationalCodeObservationQuery> &queries,
                                       const RelationalCodeObservationLimits &limits) {
    std::map<Id, Descriptor> out;
    if (!endpoint.codes) return out;
    for (const auto &q : queries) {
        const auto add = [&](std::uint64_t key, std::optional<std::uint64_t> field = {}) {
            if (q.field && q.field != field) return;
            if (!q.subjects.empty() && !std::binary_search(q.subjects.begin(), q.subjects.end(), key)) return;
            require(out.size() < limits.max_observations, "observation count limit");
            require(out.emplace(Id{q.key, key}, Descriptor{q.kind, key, q.operation, q.work_budget}).second,
                    "duplicate selected observation");
        };
        if (q.kind == Kind::Operation)
            for (const auto &op : endpoint.codes->operations) add(op.key);
        else if (q.kind == Kind::Interface)
            for (const auto &i : endpoint.codes->interfaces) add(i.key);
        else
            for (const auto &s : endpoint.codes->sections) add(s.key, s.field);
    }
    return out;
}
} // namespace relational_observation_detail

// An owning, rebuildable execution accelerator over ONE endpoint. It is not a
// persisted history, mathematical quotient, multi-source atlas or transaction
// manager. The retained source and operation definitions remain authoritative.
class RelationalCodeObservationStore {
  public:
    using Selector = RelationalCodeObservationIndex::Selector;
    explicit RelationalCodeObservationStore(const TemporalEndpoint &endpoint,
                                            std::vector<RelationalCodeObservationQuery> queries,
                                            Selector selector = [](auto bytes) { return rfcx_detail::fnv1a64(bytes); },
                                            RelationalCodeObservationLimits limits = {})
        : limits_(limits) {
        Guard guard(busy_);
        validate_temporal_endpoint(endpoint);
        auto index = std::make_unique<RelationalCodeObservationIndex>(std::move(selector));
        published_ = build(std::make_shared<const TemporalEndpoint>(endpoint), queries, std::move(index), nullptr);
    }
    // A series pin is minted only by canonical native ownership. Arbitrary
    // shared_ptr<const TemporalEndpoint> is deliberately not a public intake.
    explicit RelationalCodeObservationStore(const NativeSeriesEndpoint& endpoint,
            std::vector<RelationalCodeObservationQuery> queries,
            Selector selector = [](auto bytes) { return rfcx_detail::fnv1a64(bytes); },
            RelationalCodeObservationLimits limits = {}) : limits_(limits) {
        Guard guard(busy_);validate_temporal_endpoint(endpoint.endpoint());
        auto index=std::make_unique<RelationalCodeObservationIndex>(std::move(selector));
        published_=build(endpoint.pin(),std::move(queries),std::move(index),nullptr);
    }
    RelationalCodeObservationStore(const RelationalCodeObservationStore &) = delete;
    RelationalCodeObservationStore &operator=(const RelationalCodeObservationStore &) = delete;
    RelationalCodeObservationStore(RelationalCodeObservationStore &&) = delete;
    RelationalCodeObservationStore &operator=(RelationalCodeObservationStore &&) = delete;

    // Publish only after replay, validation, evaluation, allocation and bucket
    // selection all succeed. A failure leaves endpoint, observations and stats
    // unchanged. External side effects of a caller's selector cannot roll back.
    RelationalCodeObservationStats advance(const TemporalSegment &segment,
        std::optional<std::vector<RelationalCodeObservationQuery>> queries = std::nullopt) {
        Guard guard(busy_);
        auto endpoint = replay_temporal_segment(*published_->endpoint, segment);
        return publish(std::move(endpoint), queries ? *queries : published_->queries);
    }
    RelationalCodeObservationStats reconfigure(std::vector<RelationalCodeObservationQuery> queries) {
        Guard guard(busy_);
        auto index=std::make_unique<RelationalCodeObservationIndex>(*published_->index);
        auto next=build(published_->endpoint,std::move(queries),std::move(index),published_.get());
        published_.swap(next);return published_->stats;
    }
    std::uint64_t sequence() const { readable(); return published_->endpoint->sequence; }
    TemporalEndpoint snapshot() const { readable(); return *published_->endpoint; }
    // Immutable aliases pin ONE publication, not the mutable store. The source
    // and observation payloads are not copied and survive later publication.
    std::shared_ptr<const TemporalEndpoint> pin_snapshot() const {
        readable(); return published_->endpoint;
    }
    std::shared_ptr<const RelationalCodeObservation> pin_observation(RelationalCodeIdentity id) const {
        readable(); return {published_, &published_->entries.at(id).observation};
    }
    // Stage a larger transaction in an independent store. Both stores initially
    // share an immutable publication; advances publish only in the fork. This is
    // not synchronization for simultaneous mutation of one store.
    std::unique_ptr<RelationalCodeObservationStore> fork() const {
        readable();
        return std::unique_ptr<RelationalCodeObservationStore>(
            new RelationalCodeObservationStore(limits_, published_));
    }
    std::vector<RelationalCodeObservationQuery> queries() const { readable(); return published_->queries; }
    RelationalCodeObservationStats stats() const { readable(); return published_->stats; }
    std::size_t size() const { readable(); return published_->entries.size(); }
    bool contains(RelationalCodeIdentity id) const { readable(); return published_->entries.contains(id); }
    RelationalCodeObservation observation(RelationalCodeIdentity id) const {
        readable();
        return published_->entries.at(id).observation; // owning result survives later publication
    }
    std::vector<RelationalCodeIdentity> identities() const {
        readable();
        std::vector<RelationalCodeIdentity> out;
        for (const auto &[id, entry] : published_->entries) { (void)entry; out.push_back(id); }
        return out;
    }
    std::vector<RelationalCodeIdentity> find(std::span<const std::byte> exact) const {
        Guard guard(busy_);
        return published_->index->find(exact);
    }

  private:
    using Fact = relational_observation_detail::Fact;
    using Bytes = relational_observation_detail::Bytes;
    using Entry = relational_observation_detail::Entry;
    using Id = RelationalCodeIdentity;
    struct Published {
        std::shared_ptr<const TemporalEndpoint> endpoint;
        std::vector<RelationalCodeObservationQuery> queries;
        std::unique_ptr<RelationalCodeObservationIndex> index;
        std::map<Id, Entry> entries;
        std::map<Fact, Bytes> facts;
        std::map<Fact, std::set<Id>> reverse;
        RelationalCodeObservationStats stats;
        Published(std::shared_ptr<const TemporalEndpoint> e, std::vector<RelationalCodeObservationQuery> q,
                  std::unique_ptr<RelationalCodeObservationIndex> i)
            : endpoint(std::move(e)), queries(std::move(q)), index(std::move(i)) {}
    };
    struct Guard {
        bool &busy;
        explicit Guard(bool &b) : busy(b) {
            temporal_source_detail::require(!busy, "observation store callback reentry");
            busy = true;
        }
        ~Guard() { busy = false; }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
    };
    void readable() const { temporal_source_detail::require(!busy_, "observation store callback reentry"); }
    RelationalCodeObservationStats publish(TemporalEndpoint endpoint,
                                            std::vector<RelationalCodeObservationQuery> queries) {
        auto index = std::make_unique<RelationalCodeObservationIndex>(*published_->index);
        auto next = build(std::make_shared<const TemporalEndpoint>(std::move(endpoint)), std::move(queries), std::move(index), published_.get());
        published_.swap(next); // noexcept publication of complete candidate
        return published_->stats;
    }
    std::shared_ptr<const Published> build(std::shared_ptr<const TemporalEndpoint> endpoint,
                                    std::vector<RelationalCodeObservationQuery> queries,
                                    std::unique_ptr<RelationalCodeObservationIndex> index,
                                    const Published *old) const {
        using namespace relational_observation_detail;
        validate_queries(queries, limits_);
        auto next = std::make_shared<Published>(std::move(endpoint), std::move(queries), std::move(index));
        Context context(*next->endpoint);
        const auto selected = select(*next->endpoint, next->queries, limits_);
        std::set<Id> dirty;
        for (const auto &[id, descriptor] : selected) {
            auto deps = context.dependencies(descriptor);
            account(next->stats.dependency_edges, deps.size(), limits_.max_dependency_edges,
                    "observation dependency edge limit");
            for (const auto &fact : deps) {
                next->reverse[fact].insert(id);
                if (!next->facts.contains(fact)) {
                    require(next->facts.size() < limits_.max_dependencies, "observation dependency limit");
                    auto bytes = context.fact_bytes(fact);
                    account(next->stats.canonical_bytes, bytes.size(), limits_.max_canonical_bytes,
                            "observation canonical byte limit");
                    next->facts.emplace(fact, std::move(bytes));
                }
            }
            if (!old || !old->entries.contains(id)) { dirty.insert(id); ++next->stats.added; }
            else if (old->entries.at(id).descriptor != descriptor || old->entries.at(id).dependencies != deps)
                dirty.insert(id);
            next->entries.emplace(id, Entry{descriptor, std::move(deps), {}});
        }
        for (const auto &[fact, bytes] : next->facts) {
            const bool existed = old && old->facts.contains(fact);
            if (existed) ++next->stats.compared_dependencies;
            if (!existed || old->facts.at(fact) != bytes) {
                ++next->stats.changed_dependencies;
                const auto &affected = next->reverse.at(fact);
                dirty.insert(affected.begin(), affected.end());
            }
        }
        if (old) {
            for (const auto &[fact, bytes] : old->facts) {
                (void)bytes;
                if (!next->facts.contains(fact)) ++next->stats.changed_dependencies;
            }
            for (const auto &[id, entry] : old->entries) {
                (void)entry;
                if (!next->entries.contains(id)) { next->index->erase(id); ++next->stats.removed; }
            }
        }
        for (auto &[id, entry] : next->entries) {
            const bool existed = old && old->entries.contains(id);
            if (dirty.contains(id)) {
                entry.observation = context.evaluate(entry.descriptor);
                ++next->stats.evaluated;
                if (existed) ++next->stats.invalidated;
            } else { entry.observation = old->entries.at(id).observation; ++next->stats.reused; }
            account(next->stats.canonical_bytes, entry.observation.bytes.size(), limits_.max_canonical_bytes,
                    "observation canonical byte limit");
            if (!existed) next->index->insert(id, entry.observation.bytes);
            else if (old->entries.at(id).observation.bytes != entry.observation.bytes) {
                next->index->replace(id, entry.observation.bytes);
                ++next->stats.bytes_changed;
            }
        }
        next->stats.dependencies = next->facts.size();
        next->stats.operation_applications = context.operation_applications();
        return next;
    }
    RelationalCodeObservationStore(RelationalCodeObservationLimits limits,
                                   std::shared_ptr<const Published> published)
        : limits_(limits), published_(std::move(published)) {}
    RelationalCodeObservationLimits limits_;
    std::shared_ptr<const Published> published_;
    mutable bool busy_{};
};

} // namespace rfc::storage
