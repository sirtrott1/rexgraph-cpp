#pragma once

#include "rfc/storage/temporal_codec.hpp"

namespace rfc::storage {

template <class T> struct TemporalReplacement {
    bool birth{};
    T target;
};
struct TemporalTensorUpdate {
    bool birth{};
    bool field_innovation{};
    // Field records contain only target metadata/shape/support here (zero
    // values). Other tensor roles carry a complete reconstructive replacement.
    TemporalTensor target;
    numeric::SparseRationalVector innovation{0};
};
struct TemporalSegment {
    std::uint64_t base_sequence{}, target_sequence{};
    std::uint64_t base_endpoint_hash{}, target_endpoint_hash{};
    std::uint64_t base_source_hash{}, target_source_hash{};
    TemporalSourcePatch source;
    std::vector<std::uint64_t> removed_blocks, removed_carriers, removed_tensors;
    std::vector<TemporalReplacement<LocalBlockDefinition>> blocks;
    std::vector<TemporalReplacement<TemporalCarrier>> carriers;
    std::vector<TemporalTensorUpdate> tensors;
    std::vector<TemporalCorrespondence> correspondences;
    std::vector<std::uint64_t> removed_finite_systems;
    std::vector<TemporalReplacement<TemporalFiniteSystem>> finite_systems;
    std::optional<RelationalCodeUpdate> codes;
    std::optional<TemporalDeclarationUpdate> declarations;
    std::optional<SourceDeclarationMigration> source_migration;
};

namespace temporal_segment_detail {
using temporal_source_detail::require;
using temporal_source_detail::unique;
using namespace temporal_endpoint_detail;

template <class T, class Key>
inline void apply_replacements(std::vector<T> &values, const std::vector<std::uint64_t> &removed,
                               const std::vector<TemporalReplacement<T>> &replacements, Key key) {
    std::map<std::uint64_t, T> result;
    for (const auto &v : values)
        require(result.emplace(key(v), v).second, "duplicate endpoint key");
    std::set<std::uint64_t> seen;
    for (const auto id : removed) {
        unique(seen, id);
        require(result.erase(id) == 1, "removal of missing temporal object");
    }
    for (const auto &r : replacements) {
        const auto id = key(r.target);
        unique(seen, id);
        require(result.contains(id) != r.birth, "temporal replacement birth/update precondition failed");
        result.insert_or_assign(id, r.target);
    }
    values.clear();
    for (auto &[id, value] : result) {
        (void)id;
        values.push_back(std::move(value));
    }
}
inline CarrierCorrespondenceResolver
validate_correspondences(const TemporalEndpoint &old, const TemporalEndpoint &next,
                         const std::vector<TemporalCorrespondence> &maps) {
    CarrierCorrespondenceResolver resolver(old, next, maps);
    resolver.validate_all();
    return resolver;
}

} // namespace temporal_segment_detail

inline TemporalEndpoint replay_temporal_segment(const TemporalEndpoint &base,
                                                const TemporalSegment &segment,
                                                const SourceDeclarationLimits& source_limits = {}) {
    using namespace temporal_segment_detail;
    require(base.sequence == segment.base_sequence &&
                base.sequence != std::numeric_limits<std::uint64_t>::max() &&
                segment.target_sequence == base.sequence + 1u,
            "temporal sequence/predecessor mismatch");
    {
        const auto source = encode_rfcx(base.source);
        require(source.stats.payload_hash == segment.base_source_hash,
                "temporal base RFCX hash mismatch");
        const StableSourceIndex source_index(base.source);
        temporal_endpoint_detail::validate(base, source_index);
        require(temporal_codec_detail::endpoint_hash(base, source_index, rfcx_detail::fnv1a64(source.bytes)) ==
                    segment.base_endpoint_hash, "temporal base endpoint/channel hash mismatch");
    }
    // All mutations below are on an owning candidate. A failure cannot publish
    // a partly changed complex or modify the caller's endpoint.
    std::optional<TemporalEndpoint> aligned_base;
    if (segment.source_migration) {
        aligned_base.emplace(recode_temporal_source(base.source, *segment.source_migration, source_limits), base.sequence);
        aligned_base->carriers = base.carriers;
    }
    const auto& carrier_base = aligned_base ? *aligned_base : base;
    TemporalEndpoint next(replay_temporal_source(carrier_base.source, segment.source), segment.target_sequence);
    const auto target_source_hash = [&] {
        const auto source = encode_rfcx(next.source);
        require(source.stats.payload_hash == segment.target_source_hash,
                "replayed target RFCX hash mismatch");
        return rfcx_detail::fnv1a64(source.bytes);
    }();
    next.local_blocks = base.local_blocks;
    next.carriers = base.carriers;
    apply_replacements(next.local_blocks, segment.removed_blocks, segment.blocks,
                       [](const auto &b) { return b.block_key; });
    const StableSourceIndex next_local_index(next.source);
    for (auto &b : next.local_blocks)
        b = normalize_local(next.source, next_local_index, std::move(b));
    apply_replacements(next.carriers, segment.removed_carriers, segment.carriers,
                       [](const auto &c) { return c.key; });
    auto comparison = validate_correspondences(carrier_base, next, segment.correspondences);
    const auto old_tensors = temporal_endpoint_detail::tensors(base);
    std::map<std::uint64_t, TemporalTensor> target_tensors;
    for (const auto &t : base.tensors)
        target_tensors.emplace(t.key, t);
    std::set<std::uint64_t> seen;
    for (const auto key : segment.removed_tensors) {
        unique(seen, key);
        require(target_tensors.erase(key) == 1, "removal of missing temporal tensor");
    }
    for (const auto &update : segment.tensors) {
        auto target = update.target;
        unique(seen, target.key);
        require(target_tensors.contains(target.key) != update.birth,
                "tensor birth/update precondition failed");
        require(update.field_innovation == (target.role == TemporalTensorRole::Field),
                "field must use innovation, map/form must use replacement");
        if (!update.birth) {
            const auto &old = *old_tensors.at(target.key);
            require((old.role == TemporalTensorRole::Field) == (target.role == TemporalTensorRole::Field),
                    "field/nonfield role migration requires a new payload identity");
            if (update.field_innovation)
                require(old.output == target.output,
                        "field carrier identity migration requires a new payload identity");
        }
        if (update.field_innovation) {
            require(target.output.has_value() && target.tensor.values().nonzeros() == 0,
                    "field record must contain zero target metadata and explicit innovation");
            const auto &J = comparison.get(*target.output);
            require(update.innovation.size() == J.rows(), "field innovation target dimension mismatch");
            numeric::SparseRationalVector propagated(J.rows());
            if (!update.birth)
                propagated = sparse_apply(J, old_tensors.at(target.key)->tensor.values());
            const auto values = sparse_sum(propagated, update.innovation);
            target.tensor =
                numeric::SparseRationalTensor(target.tensor.domain(), target.tensor.support(), values);
        } else
            require(update.innovation.size() == 0 && update.innovation.nonzeros() == 0,
                    "nonfield tensor has spurious field innovation");
        target_tensors.insert_or_assign(target.key, std::move(target));
    }
    // An unchanged persistent field still evolves under J. Only absence of a
    // field record means eta=0; it never means ignore a changed correspondence.
    for (auto &[key, target] : target_tensors) {
        if (target.role != TemporalTensorRole::Field || seen.contains(key))
            continue;
        require(target.output.has_value(), "field has no target carrier");
        if (comparison.is_identity(*target.output)) continue;
        const auto &J = comparison.get(*target.output);
        target.tensor = numeric::SparseRationalTensor(target.tensor.domain(), target.tensor.support(),
                                                      sparse_apply(J, target.tensor.values()));
    }
    for (auto &[key, t] : target_tensors) {
        (void)key;
        next.tensors.push_back(std::move(t));
    }
    next.finite_systems = base.finite_systems;
    const auto old_finite = index(base.finite_systems, [](const auto &f) { return f.key; });
    for (const auto &replacement : segment.finite_systems)
        if (!replacement.birth && old_finite.contains(replacement.target.key))
            require(old_finite.at(replacement.target.key)->kind == replacement.target.kind,
                    "finite value-kind migration requires a new system identity");
    apply_replacements(next.finite_systems, segment.removed_finite_systems, segment.finite_systems,
                       [](const auto &system) { return system.key; });
    for (auto &system : next.finite_systems)
        system = normalize_temporal_finite_system(std::move(system));
    next.codes = segment.codes ? replay_relational_code_update(base.codes, *segment.codes) : base.codes;
    validate_relational_code_transition(base.codes, next.codes,
                                        segment.codes ? segment.codes->transports
                                                      : std::vector<RelationalCodeTransport>{});
    if (segment.declarations) validate_temporal_declaration_transition(base.declarations, *segment.declarations);
    next.declarations = segment.declarations ? segment.declarations->target : base.declarations;
    temporal_endpoint_detail::validate(next, next_local_index);
    require(temporal_codec_detail::endpoint_hash(next, next_local_index, target_source_hash) == segment.target_endpoint_hash,
            "replayed target endpoint/channel hash mismatch");
    return next;
}

namespace temporal_segment_detail {
inline TemporalSegment build_temporal_segment(const TemporalEndpoint &base, const TemporalEndpoint &target,
                                             std::vector<TemporalCorrespondence> correspondences = {},
                                             std::vector<RelationalCodeTransport> code_transports = {},
                                             RelationalCodeEncoding code_encoding = RelationalCodeEncoding::Automatic,
                                             std::optional<SourceDeclarationMigration> source_migration = {},
                                             std::optional<TemporalDeclarationUpdate> declaration_update = {},
                                             const SourceDeclarationLimits& source_limits = {}) {
    using namespace temporal_segment_detail;
    require(code_encoding <= RelationalCodeEncoding::Sparse, "unknown code update encoding");
    validate_temporal_endpoint(base);
    validate_temporal_endpoint(target);
    require(base.sequence != std::numeric_limits<std::uint64_t>::max() &&
                target.sequence == base.sequence + 1u,
            "temporal segment requires consecutive endpoint sequences");
    TemporalSegment segment;
    segment.base_sequence = base.sequence;
    segment.target_sequence = target.sequence;
    segment.base_source_hash = encode_rfcx(base.source).stats.payload_hash;
    segment.target_source_hash = encode_rfcx(target.source).stats.payload_hash;
    segment.base_endpoint_hash = temporal_endpoint_hash(base);
    segment.target_endpoint_hash = temporal_endpoint_hash(target);
    segment.source_migration = std::move(source_migration);
    std::optional<TemporalEndpoint> aligned_base;
    if (segment.source_migration) {
        aligned_base.emplace(recode_temporal_source(base.source, *segment.source_migration, source_limits), base.sequence);
        aligned_base->carriers = base.carriers;
    }
    const auto& carrier_base = aligned_base ? *aligned_base : base;
    segment.source = make_temporal_source_patch(carrier_base.source, target.source);
    segment.declarations = std::move(declaration_update);
    if (!segment.declarations && !temporal_codec_detail::declaration_state_equal(base.declarations, target.declarations))
        segment.declarations = make_temporal_declaration_update(base.declarations, target.declarations);
    if (segment.declarations)
        require(temporal_codec_detail::declaration_state_equal(segment.declarations->target, target.declarations),
                "declaration update target differs from requested endpoint");
    std::sort(correspondences.begin(), correspondences.end(),
              [](const auto &a, const auto &b) { return a.carrier < b.carrier; });
    segment.correspondences = std::move(correspondences);
    if (segment.source_migration) {
        // Persist the derived stable-source J explicitly so independent existing
        // temporal diagnostics need not reinterpret a different source signature.
        auto resolved = validate_correspondences(carrier_base, target, segment.correspondences);
        auto complete = segment.correspondences;
        for (const auto& carrier : target.carriers) {
            if (carrier.kind != TemporalCarrierKind::SourceGrade) continue;
            const auto it = std::find_if(complete.begin(), complete.end(), [&](const auto& c){return c.carrier == carrier.key;});
            if (it == complete.end()) complete.push_back({carrier.key,resolved.get(carrier.key)});
        }
        segment.correspondences = std::move(complete);
        std::sort(segment.correspondences.begin(), segment.correspondences.end(), [](const auto& a,const auto& b){return a.carrier<b.carrier;});
    }
    auto comparison = validate_correspondences(carrier_base, target, segment.correspondences);
    const StableSourceIndex base_local_index(base.source), target_local_index(target.source);
    auto old_blocks = index(base.local_blocks, [](const auto &b) { return b.block_key; });
    auto new_blocks = index(target.local_blocks, [](const auto &b) { return b.block_key; });
    for (const auto &[key, b] : old_blocks) {
        (void)b;
        if (!new_blocks.contains(key))
            segment.removed_blocks.push_back(key);
    }
    for (const auto &[key, b] : new_blocks) {
        auto normalized = normalize_local(target.source, target_local_index, *b);
        if (!old_blocks.contains(key) ||
            !local_equal(normalize_local(base.source, base_local_index, *old_blocks.at(key)), normalized))
            segment.blocks.push_back({!old_blocks.contains(key), std::move(normalized)});
    }
    const auto old_cs = temporal_endpoint_detail::carriers(base),
               next_cs = temporal_endpoint_detail::carriers(target);
    for (const auto &[key, c] : old_cs) {
        (void)c;
        if (!next_cs.contains(key))
            segment.removed_carriers.push_back(key);
    }
    for (const auto &[key, c] : next_cs)
        if (!old_cs.contains(key) || *old_cs.at(key) != *c)
            segment.carriers.push_back({!old_cs.contains(key), *c});
    const auto old_ts = temporal_endpoint_detail::tensors(base),
               next_ts = temporal_endpoint_detail::tensors(target);
    for (const auto &[key, t] : old_ts) {
        (void)t;
        if (!next_ts.contains(key))
            segment.removed_tensors.push_back(key);
    }
    for (const auto &[key, t] : next_ts) {
        const bool birth = !old_ts.contains(key);
        if (!birth)
            require((old_ts.at(key)->role == TemporalTensorRole::Field) ==
                        (t->role == TemporalTensorRole::Field),
                    "field/nonfield role migration requires a new payload identity");
        if (t->role == TemporalTensorRole::Field) {
            if (!birth)
                require(old_ts.at(key)->output == t->output,
                        "field carrier migration requires a new payload identity");
            const auto &J = comparison.get(*t->output);
            numeric::SparseRationalVector propagated(J.rows());
            if (!birth)
                propagated = sparse_apply(J, old_ts.at(key)->tensor.values());
            auto eta = sparse_sum(t->tensor.values(), propagated, -1);
            if (birth || !metadata_equal(*old_ts.at(key), *t) || eta.nonzeros() != 0) {
                TemporalTensorUpdate update;
                update.birth = birth;
                update.field_innovation = true;
                update.target = *t;
                update.target.tensor =
                    numeric::SparseRationalTensor::zero(t->tensor.domain(), t->tensor.support());
                update.innovation = std::move(eta);
                segment.tensors.push_back(std::move(update));
            }
        } else if (birth || !tensor_equal(*old_ts.at(key), *t))
            segment.tensors.push_back({birth, false, *t, numeric::SparseRationalVector(0)});
    }
    const auto old_fs = index(base.finite_systems, [](const auto &f) { return f.key; });
    const auto next_fs = index(target.finite_systems, [](const auto &f) { return f.key; });
    for (const auto &[key, f] : old_fs) {
        (void)f;
        if (!next_fs.contains(key))
            segment.removed_finite_systems.push_back(key);
    }
    for (const auto &[key, f] : next_fs) {
        auto normalized = normalize_temporal_finite_system(*f);
        const bool birth = !old_fs.contains(key);
        if (!birth)
            require(old_fs.at(key)->kind == f->kind,
                    "finite value-kind migration requires a new system identity");
        if (birth || normalize_temporal_finite_system(*old_fs.at(key)) != normalized)
            segment.finite_systems.push_back({birth, std::move(normalized)});
    }
    if (!temporal_codec_detail::code_state_equal(base.codes, target.codes) || !code_transports.empty())
        segment.codes = make_relational_code_update(base.codes, target.codes, std::move(code_transports), code_encoding);
    return segment;
}
} // namespace temporal_segment_detail

inline TemporalSegment make_temporal_segment(const TemporalEndpoint &base, const TemporalEndpoint &target,
                                             std::vector<TemporalCorrespondence> correspondences = {},
                                             std::vector<RelationalCodeTransport> code_transports = {},
                                             RelationalCodeEncoding code_encoding = RelationalCodeEncoding::Automatic,
                                             std::optional<SourceDeclarationMigration> source_migration = {},
                                             std::optional<TemporalDeclarationUpdate> declaration_update = {},
                                             const SourceDeclarationLimits& source_limits = {}) {
    auto segment = temporal_segment_detail::build_temporal_segment(base, target, std::move(correspondences),
        std::move(code_transports), code_encoding, std::move(source_migration), std::move(declaration_update), source_limits);
    (void)replay_temporal_segment(base, segment, source_limits);
    return segment;
}

namespace temporal_segment_detail {
inline constexpr std::array<std::byte, 8> magic{std::byte{'R'}, std::byte{'F'}, std::byte{'C'},
                                                std::byte{'X'}, std::byte{'T'}, std::byte{1},
                                                std::byte{0},   std::byte{0}};
inline constexpr std::size_t header_size = 32;

// Set/map-like edit collections have canonical key order. Fiber words,
// source cell order, tensor axes and operation words remain ordered data.
inline TemporalSegment canonical_segment(TemporalSegment s) {
    const auto sort_by = [](auto &xs, auto key) {
        std::sort(xs.begin(), xs.end(), [&](const auto &a, const auto &b) { return key(a) < key(b); });
    };
    const auto sort_keys = [](auto &xs) { std::sort(xs.begin(), xs.end()); };
    sort_by(s.source.grades, [](const auto &g) { return g.grade; });
    for (auto &g : s.source.grades) {
        sort_keys(g.removed_cells);
        sort_keys(g.removed_occurrences);
        sort_by(g.cells, [](const auto &x) { return x.key; });
        sort_by(g.born_occurrences, [](const auto &x) { return x.key; });
        sort_by(g.occurrences, [](const auto &x) { return x.key; });
        sort_by(g.fibers, [](const auto &x) { return x.parent; });
        sort_by(g.aspect_xor, [](const auto &x) { return std::pair{x.aspect, x.word_index}; });
    }
    sort_keys(s.removed_blocks);
    sort_keys(s.removed_carriers);
    sort_keys(s.removed_tensors);
    sort_keys(s.removed_finite_systems);
    for (auto &b : s.blocks)
        canonicalize_local_block(b.target);
    sort_by(s.blocks, [](const auto &x) { return x.target.block_key; });
    sort_by(s.carriers, [](const auto &x) { return x.target.key; });
    sort_by(s.tensors, [](const auto &x) { return x.target.key; });
    sort_by(s.correspondences, [](const auto &x) { return x.carrier; });
    sort_by(s.finite_systems, [](const auto &x) { return x.target.key; });
    return s;
}
inline bool has_finite_extension(const TemporalSegment &s) {
    return !s.removed_finite_systems.empty() || !s.finite_systems.empty();
}
inline void write_body(temporal_codec_detail::Writer &w, const TemporalSegment &s) {
    using namespace temporal_codec_detail;
    w.u64(s.base_sequence);
    w.u64(s.target_sequence);
    w.u64(s.base_endpoint_hash);
    w.u64(s.target_endpoint_hash);
    w.u64(s.base_source_hash);
    w.u64(s.target_source_hash);
    source_patch(w, s.source);
    keys(w, s.removed_blocks);
    w.u64(s.blocks.size());
    for (const auto &b : s.blocks) {
        w.u8(b.birth);
        local(w, b.target);
    }
    keys(w, s.removed_carriers);
    w.u64(s.carriers.size());
    for (const auto &c : s.carriers) {
        w.u8(c.birth);
        carrier(w, c.target);
    }
    keys(w, s.removed_tensors);
    w.u64(s.tensors.size());
    for (const auto &t : s.tensors) {
        w.u8(t.birth);
        w.u8(t.field_innovation);
        tensor_record(w, t.target);
        w.u64(t.innovation.size());
        values(w, t.innovation);
    }
    w.u64(s.correspondences.size());
    for (const auto &c : s.correspondences) {
        w.u64(c.carrier);
        matrix(w, c.map);
    }
    if (has_finite_extension(s)) {
        keys(w, s.removed_finite_systems);
        w.u64(s.finite_systems.size());
        for (const auto &f : s.finite_systems) {
            w.u8(f.birth);
            finite_system(w, f.target);
        }
    }
    if (s.codes)
        code_update(w, *s.codes);
    if (s.declarations) declaration_update(w, *s.declarations);
    if (s.source_migration) source_migration(w, *s.source_migration);
}
inline TemporalSegment read_body(temporal_codec_detail::Reader &r, bool finite_extension = false,
                                 bool code_extension = false, bool sparse_code_extension = false,
                                 bool declaration_extension = false, bool migration_extension = false) {
    using namespace temporal_codec_detail;
    TemporalSegment s;
    s.base_sequence = r.u64();
    s.target_sequence = r.u64();
    s.base_endpoint_hash = r.u64();
    s.target_endpoint_hash = r.u64();
    s.base_source_hash = r.u64();
    s.target_source_hash = r.u64();
    s.source = source_patch(r);
    s.removed_blocks = keys(r);
    auto n = r.count();
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag();
        s.blocks.push_back({birth, local(r)});
    }
    s.removed_carriers = keys(r);
    n = r.count();
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag();
        s.carriers.push_back({birth, carrier(r)});
    }
    s.removed_tensors = keys(r);
    n = r.count();
    for (std::size_t i = 0; i < n; ++i) {
        TemporalTensorUpdate t;
        t.birth = r.flag();
        t.field_innovation = r.flag();
        t.target = tensor_record(r);
        const auto size = r.dimension();
        t.innovation = values(r, size);
        s.tensors.push_back(std::move(t));
    }
    n = r.count();
    for (std::size_t i = 0; i < n; ++i) {
        const auto key = r.u64();
        s.correspondences.push_back({key, matrix(r)});
    }
    if (finite_extension) {
        s.removed_finite_systems = keys(r);
        n = r.count(34);
        for (std::size_t i = 0; i < n; ++i) {
            const bool birth = r.flag();
            s.finite_systems.push_back({birth, finite_system(r)});
        }
        require(has_finite_extension(s), "empty finite segment extension");
    }
    require(!(code_extension && sparse_code_extension), "conflicting code extension flags");
    if (code_extension || sparse_code_extension)
        s.codes = code_update(r, sparse_code_extension);
    if (declaration_extension) s.declarations = declaration_update(r);
    if (migration_extension) s.source_migration = source_migration(r);
    require(r.remaining() == 0, "trailing bytes in temporal frame");
    require(s.base_sequence != std::numeric_limits<std::uint64_t>::max() &&
                s.target_sequence == s.base_sequence + 1u,
            "invalid temporal sequence");
    return s;
}
inline void check_chain(const TemporalSegment &previous, const TemporalSegment &next) {
    require(previous.target_sequence == next.base_sequence &&
                previous.target_endpoint_hash == next.base_endpoint_hash &&
                previous.target_source_hash == next.base_source_hash,
            "temporal append chain/predecessor mismatch");
}
} // namespace temporal_segment_detail

inline std::vector<std::byte> encode_temporal_segment(const TemporalSegment &segment) {
    using namespace temporal_segment_detail;
    temporal_codec_detail::Writer w;
    w.raw(magic);
    w.u32(1u);
    w.u32((has_finite_extension(segment) ? temporal_codec_detail::finite_systems_flag : 0u) |
          (segment.codes ? (segment.codes->patch ? temporal_codec_detail::relational_code_patch_flag :
                            temporal_codec_detail::relational_codes_flag) : 0u) |
          (segment.declarations ? temporal_codec_detail::declarations_flag : 0u) |
          (segment.source_migration ? temporal_codec_detail::source_migration_flag : 0u));
    w.u64(0);
    w.u64(0);
    write_body(w, canonical_segment(segment));
    w.patch_u64(16, w.size());
    w.patch_u64(24, rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(header_size)));
    return w.bytes();
}
inline TemporalSegment decode_temporal_segment(std::span<const std::byte> bytes,
                                               const TemporalReadLimits &limits = {}) {
    using namespace temporal_segment_detail;
    require(bytes.size() >= header_size && bytes.size() <= limits.max_file_bytes,
            "invalid temporal frame size");
    rfcx_detail::Reader header(bytes);
    require(std::equal(magic.begin(), magic.end(), header.slice(8).begin()), "invalid temporal magic");
    require(header.u32() == 1u, "unsupported temporal version");
    const auto flags = header.u32();
    require((flags & ~(temporal_codec_detail::finite_systems_flag |
                       temporal_codec_detail::relational_codes_flag |
                       temporal_codec_detail::relational_code_patch_flag |
                       temporal_codec_detail::declarations_flag | temporal_codec_detail::source_migration_flag)) == 0,
            "unsupported temporal flags");
    require((flags & (temporal_codec_detail::relational_codes_flag |
                      temporal_codec_detail::relational_code_patch_flag)) !=
                (temporal_codec_detail::relational_codes_flag |
                 temporal_codec_detail::relational_code_patch_flag),
            "conflicting code extension flags");
    require(header.u64() == bytes.size(), "temporal frame length mismatch");
    const auto hash = header.u64();
    const auto body = bytes.subspan(header_size);
    require(hash == rfcx_detail::fnv1a64(body), "temporal frame integrity mismatch");
    temporal_codec_detail::Reader r(body, limits);
    auto segment = read_body(r, (flags & temporal_codec_detail::finite_systems_flag) != 0,
                             (flags & temporal_codec_detail::relational_codes_flag) != 0,
                             (flags & temporal_codec_detail::relational_code_patch_flag) != 0,
                             (flags & temporal_codec_detail::declarations_flag) != 0,
                             (flags & temporal_codec_detail::source_migration_flag) != 0);
    if (limits.require_canonical) {
        const auto canonical = encode_temporal_segment(segment);
        require(canonical.size() == bytes.size() &&
                    std::equal(canonical.begin(), canonical.end(), bytes.begin()),
                "noncanonical temporal segment encoding");
    }
    return segment;
}
// Incremental framing retains at most one raw frame plus decoder/canonical
// scratch, never the whole stream. Callback bytes are valid only during the
// callback. Failures (including callback failure) poison the decoder; no retry
// can deliver a partially accepted frame twice. Call finish() to detect tails.
class TemporalSegmentStreamDecoder {
  public:
    explicit TemporalSegmentStreamDecoder(TemporalReadLimits limits = {}) : limits_(limits) {}
    TemporalSegmentStreamDecoder(const TemporalSegmentStreamDecoder &) = delete;
    TemporalSegmentStreamDecoder &operator=(const TemporalSegmentStreamDecoder &) = delete;
    template <class Consumer> void feed(std::span<const std::byte> chunk, Consumer &&consume) {
        using temporal_source_detail::require;
        require(!failed_ && !finished_ && !feeding_,
                "temporal stream decoder is closed, failed or reentered");
        feeding_ = true;
        try {
            require(total_bytes_ <= limits_.max_file_bytes &&
                        chunk.size() <= limits_.max_file_bytes - total_bytes_,
                    "temporal stream exceeds byte limit");
            total_bytes_ += chunk.size();
            while (!chunk.empty()) {
                const auto needed =
                    (frame_size_ == 0 ? temporal_segment_detail::header_size : frame_size_) - buffer_.size();
                const auto n = std::min(needed, chunk.size());
                buffer_.insert(buffer_.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(n));
                chunk = chunk.subspan(n);
                peak_buffered_ = std::max(peak_buffered_, buffer_.size());
                if (frame_size_ == 0 && buffer_.size() == temporal_segment_detail::header_size) {
                    require(std::equal(temporal_segment_detail::magic.begin(),
                                       temporal_segment_detail::magic.end(), buffer_.begin()),
                            "invalid streamed temporal magic");
                    rfcx_detail::Reader header(buffer_);
                    (void)header.slice(8);
                    require(header.u32() == 1u, "unsupported streamed temporal version");
                    const auto flags = header.u32();
                    require((flags & ~(temporal_codec_detail::finite_systems_flag |
                                       temporal_codec_detail::relational_codes_flag |
                                       temporal_codec_detail::relational_code_patch_flag |
                       temporal_codec_detail::declarations_flag | temporal_codec_detail::source_migration_flag)) == 0 &&
                                (flags & (temporal_codec_detail::relational_codes_flag |
                                          temporal_codec_detail::relational_code_patch_flag)) !=
                                    (temporal_codec_detail::relational_codes_flag |
                                     temporal_codec_detail::relational_code_patch_flag),
                            "unsupported/conflicting streamed temporal flags");
                    const auto length = rfcx_detail::load_u64_at(buffer_, 16);
                    require(length >= temporal_segment_detail::header_size &&
                                length <= limits_.max_file_bytes,
                            "invalid streamed temporal frame length");
                    require(frame_count_ < limits_.max_records, "too many temporal stream frames");
                    frame_size_ = static_cast<std::size_t>(length);
                }
                if (frame_size_ != 0 && buffer_.size() == frame_size_) {
                    auto segment = decode_temporal_segment(buffer_, limits_);
                    if (previous_)
                        require((*previous_)[0] == segment.base_sequence &&
                                    (*previous_)[1] == segment.base_endpoint_hash &&
                                    (*previous_)[2] == segment.base_source_hash,
                                "streamed temporal chain mismatch");
                    const std::array<std::uint64_t, 3> tail{
                        segment.target_sequence, segment.target_endpoint_hash, segment.target_source_hash};
                    consume(segment, std::span<const std::byte>(buffer_));
                    previous_ = tail;
                    ++frame_count_;
                    buffer_.clear();
                    frame_size_ = 0;
                }
            }
            feeding_ = false;
        } catch (...) {
            feeding_ = false;
            failed_ = true;
            throw;
        }
    }
    void finish() {
        temporal_source_detail::require(!failed_ && !feeding_, "temporal stream decoder is failed or active");
        if (finished_)
            return;
        if (!buffer_.empty()) {
            failed_ = true;
            throw std::invalid_argument("truncated temporal stream tail");
        }
        finished_ = true;
    }
    std::size_t frame_count() const noexcept {
        return frame_count_;
    }
    std::size_t peak_buffered_bytes() const noexcept {
        return peak_buffered_;
    }

  private:
    TemporalReadLimits limits_;
    std::vector<std::byte> buffer_;
    std::size_t frame_size_{}, frame_count_{}, total_bytes_{}, peak_buffered_{};
    std::optional<std::array<std::uint64_t, 3>> previous_;
    bool failed_{}, finished_{}, feeding_{};
};

template <class Consumer>
inline void visit_temporal_segments(const std::filesystem::path &path, Consumer &&consume,
                                    const TemporalReadLimits &limits = {}) {
    validate_file_path(path);
    if (!std::filesystem::is_regular_file(path)) throw std::invalid_argument("temporal stream must be a regular file");
    temporal_source_detail::require(std::filesystem::file_size(path) <= limits.max_file_bytes,
                                    "temporal stream exceeds byte limit");
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("unable to open temporal stream");
    TemporalSegmentStreamDecoder decoder(limits);
    std::array<std::byte, 65536> chunk;
    while (in) {
        in.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto n = in.gcount();
        if (n > 0)
            decoder.feed(std::span<const std::byte>(chunk).first(static_cast<std::size_t>(n)), consume);
    }
    if (in.bad() || !in.eof())
        throw std::runtime_error("unable to read temporal stream");
    decoder.finish();
}
inline std::vector<TemporalSegment> read_temporal_segments(const std::filesystem::path &path,
                                                           const TemporalReadLimits &limits = {}) {
    std::vector<TemporalSegment> out;
    visit_temporal_segments(path, [&](const auto &segment, auto) { out.push_back(segment); }, limits);
    return out;
}
inline TemporalSegment read_temporal_segment(const std::filesystem::path &path,
                                             const TemporalReadLimits &limits = {}) {
    auto segments = read_temporal_segments(path, limits);
    temporal_source_detail::require(segments.size() == 1, "expected exactly one temporal segment");
    return std::move(segments.front());
}
// Single-writer logical append. Existing frames are validated and copied byte
// for byte into a staged replacement; a failed write cannot tear the old stream.
// This adds O(existing file bytes) physical copy work, not bounded in-place append.
// It is NOT an fsync transaction/crash-recovery/multiwriter API.
inline std::uint64_t append_temporal_segment(const std::filesystem::path &path,
                                             const TemporalSegment &segment,
                                             const TemporalReadLimits &limits = {}) {
    validate_file_path(path);
    file_io_detail::check_destination(path, FilePublication::ReplaceRegular);
    std::size_t frame_count = 0;
    if (std::filesystem::exists(path) && std::filesystem::file_size(path) != 0) {
        std::optional<std::array<std::uint64_t, 3>> tail;
        visit_temporal_segments(path, [&](const auto &previous, auto) {
            tail = std::array<std::uint64_t, 3>{previous.target_sequence,
                previous.target_endpoint_hash, previous.target_source_hash};
            ++frame_count;
        }, limits);
        temporal_source_detail::require(tail && (*tail)[0] == segment.base_sequence &&
            (*tail)[1] == segment.base_endpoint_hash && (*tail)[2] == segment.base_source_hash,
            "temporal append chain/predecessor mismatch");
    }
    temporal_source_detail::require(frame_count < limits.max_records,
                                    "append would exceed temporal frame limit");
    const auto bytes = encode_temporal_segment(segment);
    (void)decode_temporal_segment(bytes, limits);
    const auto previous_size = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
    temporal_source_detail::require(previous_size <= limits.max_file_bytes &&
                                        bytes.size() <= limits.max_file_bytes - previous_size,
                                    "append would exceed temporal byte limit");
    publish_file_exact(path, FilePublication::ReplaceRegular, [&](std::ostream& out) {
        if (previous_size) {
            std::ifstream in(path, std::ios::binary);
            if (!in) throw std::runtime_error("cannot reopen temporal predecessor");
            std::array<std::byte, 65536> chunk;
            std::uintmax_t copied = 0;
            while (in) {
                in.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
                const auto n = static_cast<std::size_t>(in.gcount());
                temporal_source_detail::require(n <= previous_size - copied, "temporal predecessor grew during copy");
                write_stream_exact(out, std::span<const std::byte>(chunk).first(n));
                copied += n;
            }
            if (in.bad() || !in.eof() || copied != previous_size)
                throw std::runtime_error("temporal predecessor changed or failed during copy");
        }
        write_stream_exact(out, bytes);
    });
    return bytes.size();
}

// Semantic-validation overload for callers holding the predecessor endpoint.
inline std::uint64_t append_temporal_segment(const std::filesystem::path &path, const TemporalEndpoint &base,
                                             const TemporalSegment &segment,
                                             const TemporalReadLimits &limits = {}) {
    (void)replay_temporal_segment(base, segment, limits.source_declarations);
    return append_temporal_segment(path, segment, limits);
}

} // namespace rfc::storage
