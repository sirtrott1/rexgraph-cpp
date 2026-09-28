#pragma once

#include "rfc/storage/relational_code_codec.hpp"

namespace rfc::storage {
namespace relational_code_delta_detail {
using relational_code_detail::require;
using relational_code_detail::index;

inline bool metadata_equal(const RelationalCodeSection &a, const RelationalCodeSection &b) {
    return a.key == b.key && a.field == b.field && a.type == b.type && a.block == b.block &&
           a.cell == b.cell && a.occurrence == b.occurrence && a.source_aspects == b.source_aspects;
}
inline bool section_equal(const RelationalCodeSection &a, const RelationalCodeSection &b) {
    return metadata_equal(a, b) && relational_code_detail::value_equal(a.value, b.value);
}
inline bool operation_equal(const RelationalCodeOperation &a, const RelationalCodeOperation &b) {
    temporal_codec_detail::Writer x, y;
    temporal_codec_detail::code_operation(x, a); temporal_codec_detail::code_operation(y, b);
    return x.bytes() == y.bytes();
}
inline std::uint64_t state_hash(const RelationalCodeState &s) {
    return rfcx_detail::fnv1a64(temporal_codec_detail::code_state_bytes(s));
}
inline void identity(std::set<std::uint64_t> &seen, std::uint64_t id) {
    require(id != 0 && seen.insert(id).second, "zero/duplicate/conflicting code patch identity");
}
// Every changed object is owning. Apply all directories before validating the
// complete graph: record order is not a sequence of independently valid states.
template <class T, class Equal>
inline void apply_records(std::vector<T> &items, const std::vector<std::uint64_t> &removed,
                          const std::vector<RelationalCodeReplacement<T>> &updates, Equal equal) {
    std::map<std::uint64_t, T> next;
    for (const auto &v : items) require(next.emplace(v.key, v).second, "duplicate base code identity");
    std::set<std::uint64_t> seen;
    for (auto id : removed) {
        identity(seen, id);
        require(next.erase(id) == 1, "removal of missing code declaration");
    }
    for (const auto &u : updates) {
        identity(seen, u.target.key);
        const auto it = next.find(u.target.key);
        require((it == next.end()) == u.birth, "code replacement birth/update precondition");
        if (!u.birth) require(!equal(it->second, u.target), "redundant code replacement");
        next.insert_or_assign(u.target.key, u.target);
    }
    items.clear(); items.reserve(next.size());
    for (auto &[id, value] : next) { (void)id; items.push_back(std::move(value)); }
}
template <class T, class Equal>
inline void diff_records(const std::vector<T> &a, const std::vector<T> &b,
                         std::vector<std::uint64_t> &removed,
                         std::vector<RelationalCodeReplacement<T>> &updates, Equal equal) {
    const auto old = index(a), next = index(b);
    for (const auto &[id, p] : old) { (void)p; if (!next.contains(id)) removed.push_back(id); }
    for (const auto &[id, p] : next) {
        const auto it = old.find(id);
        if (it == old.end() || !equal(*it->second, *p)) updates.push_back({it == old.end(), *p});
    }
}
inline void diff_value(const RelationalCodeAlgebra &algebra, std::uint64_t type_key,
                       const RelationalCodeValue &a, const RelationalCodeValue &b,
                       std::vector<std::uint64_t> &path, std::vector<RelationalCodeLeafEdit> &edits) {
    const auto &type = algebra.type(type_key);
    if (type.kind == RelationalCodeTypeKind::Product) {
        const auto &av = std::get<2>(a.data), &bv = std::get<2>(b.data);
        for (std::size_t i = 0; i < type.factors.size(); ++i) {
            path.push_back(type.factors[i].key);
            diff_value(algebra, type.factors[i].type, av[i], bv[i], path, edits);
            path.pop_back();
        }
        return;
    }
    RelationalCodeLeafEdit edit;
    edit.factors = path; edit.kind = type.kind;
    if (type.kind == RelationalCodeTypeKind::Finite) {
        if (std::get<1>(a.data) == std::get<1>(b.data)) return;
        edit.symbol = std::get<1>(b.data);
    } else {
        const auto &av = std::get<0>(a.data), &bv = std::get<0>(b.data);
        require(av.domain() == bv.domain(), "unchanged code type changed value domain");
        const auto n = type.atoms.size();
        if (av.support().count() != n || bv.support().count() != n) {
            av.support().for_each_active([&](auto i) {
                if (!bv.support().contains(i)) edit.removed_support.push_back(type.atoms[i]);
            });
            bv.support().for_each_active([&](auto i) {
                if (!av.support().contains(i)) edit.added_support.push_back(type.atoms[i]);
            });
        }
        const auto &ae = av.values().entries(), &be = bv.values().entries();
        std::size_t i = 0, j = 0;
        while (i != ae.size() || j != be.size()) {
            if (j == be.size() || (i < ae.size() && ae[i].index < be[j].index)) {
                edit.values.push_back({type.atoms[ae[i++].index], numeric::Rational{0}});
            } else if (i == ae.size() || be[j].index < ae[i].index) {
                edit.values.push_back({type.atoms[be[j].index], be[j].value}); ++j;
            } else {
                if (ae[i].value != be[j].value)
                    edit.values.push_back({type.atoms[be[j].index], be[j].value});
                ++i; ++j;
            }
        }
        if (edit.values.empty() && edit.added_support.empty() && edit.removed_support.empty()) return;
    }
    edits.push_back(std::move(edit));
}
inline void apply_leaf(const RelationalCodeType &type, RelationalCodeValue &value,
                       const RelationalCodeLeafEdit &edit) {
    require(type.kind == edit.kind && type.kind != RelationalCodeTypeKind::Product,
            "code patch path must end at declared leaf kind");
    if (type.kind == RelationalCodeTypeKind::Finite) {
        require(edit.symbol && edit.values.empty() && edit.added_support.empty() &&
                    edit.removed_support.empty(), "invalid finite code edit lanes");
        require(std::find(type.atoms.begin(), type.atoms.end(), *edit.symbol) != type.atoms.end(),
                "code edit uses undeclared symbol");
        require(std::get<1>(value.data) != *edit.symbol, "redundant finite code edit");
        value.data = *edit.symbol;
        return;
    }
    require(!edit.symbol && (!edit.values.empty() || !edit.added_support.empty() ||
                            !edit.removed_support.empty()), "empty/invalid rational code edit");
    const auto &tensor = std::get<0>(value.data);
    std::map<std::uint64_t, std::size_t> coordinates;
    for (std::size_t i = 0; i < type.atoms.size(); ++i) coordinates.emplace(type.atoms[i], i);
    const auto coordinate = [&](auto id) {
        const auto it = coordinates.find(id);
        require(it != coordinates.end(), "code edit uses undeclared coordinate identity");
        return it->second;
    };
    auto support = tensor.support();
    std::set<std::size_t> added, removed;
    for (auto id : edit.added_support) {
        const auto i = coordinate(id);
        require(!support.contains(i) && added.insert(i).second, "duplicate/existing support birth");
    }
    for (auto id : edit.removed_support) {
        const auto i = coordinate(id);
        require(support.contains(i) && !added.contains(i) && removed.insert(i).second,
                "duplicate/missing/conflicting support removal");
    }
    if (!added.empty() || !removed.empty()) {
        const auto count = support.count() - removed.size() + added.size();
        if (count == type.atoms.size()) support = numeric::StructuralSupport::full(count);
        else {
            std::vector<std::size_t> active;
            active.reserve(count);
            support.for_each_active([&](auto i) { if (!removed.contains(i)) active.push_back(i); });
            active.insert(active.end(), added.begin(), added.end());
            support = numeric::StructuralSupport::sparse(type.atoms.size(), std::move(active));
        }
    }
    std::map<std::size_t, numeric::Rational> values;
    for (const auto &v : tensor.values().entries()) values.emplace(v.index, v.value);
    std::set<std::size_t> touched;
    for (const auto &v : edit.values) {
        const auto i = coordinate(v.coordinate);
        require(touched.insert(i).second, "duplicate numeric code edit");
        const auto it = values.find(i);
        const numeric::Rational old = it == values.end() ? numeric::Rational{0} : it->second;
        numeric::Rational target = v.target;
        target.canonicalize();
        require(old != target, "redundant numerical code edit");
        if (target == 0) values.erase(i);
        else values.insert_or_assign(i, std::move(target));
    }
    std::vector<numeric::SparseRationalEntry> entries;
    entries.reserve(values.size());
    for (auto &[i, v] : values) {
        require(support.contains(i), "support deletion needs explicit numerical clear");
        entries.push_back({i, std::move(v)});
    }
    // Read the domain before overwriting the variant that owns tensor.
    auto domain = tensor.domain();
    value.data = numeric::SparseRationalTensor(std::move(domain), std::move(support),
        numeric::SparseRationalVector(type.atoms.size(), std::move(entries)));
}
inline void apply_section_edit(const RelationalCodeAlgebra &algebra, RelationalCodeSection &section,
                               const RelationalCodeSectionEdit &edit) {
    require(section.key == edit.section && section.type == edit.type && !section.source_aspects &&
                !edit.leaves.empty(), "invalid sparse code section edit");
    std::set<std::vector<std::uint64_t>> paths;
    for (const auto &leaf : edit.leaves) {
        require(leaf.factors.size() < 64 && paths.insert(leaf.factors).second,
                "duplicate/too-deep code edit path");
        auto type_key = section.type;
        auto *value = &section.value;
        for (auto factor : leaf.factors) {
            const auto &t = algebra.type(type_key);
            require(t.kind == RelationalCodeTypeKind::Product, "code edit path traverses a leaf");
            const auto i = relational_code_detail::factor_index(t, factor);
            value = &std::get<2>(value->data).at(i);
            type_key = t.factors[i].type;
        }
        apply_leaf(algebra.type(type_key), *value, leaf);
    }
    algebra.validate_value(section.type, section.value);
}
} // namespace relational_code_delta_detail

inline RelationalCodeState replay_relational_code_patch(
    const RelationalCodeState &base, const RelationalCodePatch &patch,
    const std::vector<RelationalCodeTransport> &transports = {}) {
    using namespace relational_code_delta_detail;
    validate_relational_code_state(base);
    require(base.namespace_key == patch.namespace_key && state_hash(base) == patch.base_state_hash,
            "code patch namespace/base binding mismatch");
    RelationalCodeState next = base;
    std::vector<RelationalCodeReplacement<RelationalCodeType>> types;
    for (const auto &t : patch.types) types.push_back({true, t});
    const auto equal = [](const auto &a, const auto &b) { return a == b; };
    apply_records(next.types, patch.removed_types, types, equal);
    apply_records(next.operations, patch.removed_operations, patch.operations, operation_equal);
    std::set<std::uint64_t> fields(next.fields.begin(), next.fields.end()), seen;
    for (auto id : patch.removed_fields) {
        identity(seen, id); require(fields.erase(id) == 1, "removal of missing code field");
    }
    for (auto id : patch.fields) {
        identity(seen, id); require(fields.insert(id).second, "birth of existing code field");
    }
    next.fields.assign(fields.begin(), fields.end());
    apply_records(next.sections, patch.removed_sections, patch.sections, section_equal);
    apply_records(next.interfaces, patch.removed_interfaces, patch.interfaces, equal);
    apply_records(next.descents, patch.removed_descents, patch.descents, equal);
    RelationalCodeAlgebra algebra(next);
    seen.clear();
    for (auto id : patch.removed_sections) identity(seen, id);
    for (const auto &s : patch.sections) identity(seen, s.target.key);
    std::map<std::uint64_t, RelationalCodeSection *> sections;
    for (auto &s : next.sections) sections.emplace(s.key, &s);
    for (const auto &edit : patch.section_edits) {
        identity(seen, edit.section);
        require(sections.contains(edit.section), "edit of missing code section");
        apply_section_edit(algebra, *sections.at(edit.section), edit);
    }
    validate_relational_code_state(next);
    validate_relational_code_transition(base, next, transports);
    require(state_hash(next) == patch.target_state_hash, "code patch target binding mismatch");
    return next;
}

inline RelationalCodePatch make_relational_code_patch(
    const RelationalCodeState &base, const RelationalCodeState &target,
    const std::vector<RelationalCodeTransport> &transports = {}) {
    using namespace relational_code_delta_detail;
    validate_relational_code_state(base); validate_relational_code_state(target);
    validate_relational_code_transition(base, target, transports);
    RelationalCodePatch p;
    p.namespace_key = base.namespace_key; p.base_state_hash = state_hash(base); p.target_state_hash = state_hash(target);
    const auto bt = index(base.types), tt = index(target.types);
    for (const auto &[id, t] : bt) { (void)t; if (!tt.contains(id)) p.removed_types.push_back(id); }
    for (const auto &[id, t] : tt) if (!bt.contains(id)) p.types.push_back(*t);
    const std::set<std::uint64_t> bf(base.fields.begin(), base.fields.end()), tf(target.fields.begin(), target.fields.end());
    for (auto id : bf) if (!tf.contains(id)) p.removed_fields.push_back(id);
    for (auto id : tf) if (!bf.contains(id)) p.fields.push_back(id);
    diff_records(base.operations, target.operations, p.removed_operations, p.operations, operation_equal);
    diff_records(base.sections, target.sections, p.removed_sections, p.sections, section_equal);
    const auto eq = [](const auto &a, const auto &b) { return a == b; };
    diff_records(base.interfaces, target.interfaces, p.removed_interfaces, p.interfaces, eq);
    diff_records(base.descents, target.descents, p.removed_descents, p.descents, eq);
    const auto old_sections = index(base.sections);
    RelationalCodeAlgebra algebra(target);
    std::vector<RelationalCodeReplacement<RelationalCodeSection>> replacements;
    for (auto &r : p.sections) {
        const auto old = old_sections.find(r.target.key);
        if (!r.birth && !r.target.source_aspects && metadata_equal(*old->second, r.target)) {
            RelationalCodeSectionEdit edit;
            edit.section = r.target.key; edit.type = r.target.type;
            std::vector<std::uint64_t> path;
            diff_value(algebra, r.target.type, old->second->value, r.target.value, path, edit.leaves);
            temporal_codec_detail::Writer sparse, full;
            temporal_codec_detail::code_section_edit(sparse, edit);
            full.u8(false); temporal_codec_detail::code_section(full, r.target);
            if (sparse.size() < full.size()) { p.section_edits.push_back(std::move(edit)); continue; }
        }
        replacements.push_back(std::move(r));
    }
    p.sections = std::move(replacements);
    // The reference builder verifies all preconditions and the exact target,
    // not merely the fixed-size binding values.
    const auto rebuilt = replay_relational_code_patch(base, p, transports);
    require(temporal_codec_detail::code_state_bytes(rebuilt) == temporal_codec_detail::code_state_bytes(target),
            "code patch builder failed exact reconstruction");
    return p;
}

inline std::optional<RelationalCodeState> replay_relational_code_update(
    const std::optional<RelationalCodeState> &base, const RelationalCodeUpdate &update) {
    temporal_source_detail::require(!(update.patch && update.target), "ambiguous code update encoding");
    if (update.patch) {
        temporal_source_detail::require(base.has_value(), "sparse code update has no predecessor layer");
        return replay_relational_code_patch(*base, *update.patch, update.transports);
    }
    validate_relational_code_transition(base, update.target, update.transports);
    return update.target;
}

inline RelationalCodeUpdate make_relational_code_update(
    const std::optional<RelationalCodeState> &base, const std::optional<RelationalCodeState> &target,
    std::vector<RelationalCodeTransport> transports = {},
    RelationalCodeEncoding encoding = RelationalCodeEncoding::Automatic) {
    temporal_source_detail::require(encoding <= RelationalCodeEncoding::Sparse, "unknown code update encoding");
    if (base) validate_relational_code_state(*base);
    if (target) validate_relational_code_state(*target);
    validate_relational_code_transition(base, target, transports);
    RelationalCodeUpdate full{target, transports, std::nullopt};
    if (encoding == RelationalCodeEncoding::Full || !base || !target) return full;
    RelationalCodeUpdate sparse{std::nullopt, std::move(transports), std::nullopt};
    sparse.patch = make_relational_code_patch(*base, *target, sparse.transports);
    if (encoding == RelationalCodeEncoding::Sparse) return sparse;
    temporal_codec_detail::Writer a, b;
    temporal_codec_detail::code_update(a, full); temporal_codec_detail::code_update(b, sparse);
    return b.size() < a.size() ? std::move(sparse) : std::move(full);
}

} // namespace rfc::storage
