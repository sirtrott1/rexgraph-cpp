#pragma once

#include "rfc/storage/temporal_codec.hpp"
#include "rfc/composite/execution.hpp"

namespace rfc::storage {

struct RelationalCodeGlueResult {
    std::uint64_t interface{};
    RelationalCodeValue left, right;
    bool compatible{};
};
inline std::vector<RelationalCodeGlueResult> relational_code_glue(const RelationalCodeState &state) {
    validate_relational_code_state(state);
    for (const auto &section : state.sections)
        temporal_source_detail::require(!section.source_aspects,
                                        "source views require the endpoint gluing overload");
    RelationalCodeAlgebra algebra(state);
    const auto sections = relational_code_detail::index(state.sections);
    std::vector<RelationalCodeGlueResult> out;
    for (const auto &i : state.interfaces) {
        auto left = algebra.apply(i.left_restriction, sections.at(i.left)->value);
        auto right = algebra.apply(i.right_restriction, sections.at(i.right)->value);
        const bool compatible = relational_code_detail::value_equal(left, right);
        out.push_back({i.key, std::move(left), std::move(right), compatible});
    }
    return out;
}
// This is the matching-section construction on the declared interfaces. It is
// not a quotient/pushout of vector spaces or a claim of undeclared coverage.
inline std::vector<RelationalCodeSection> matched_relational_code_sections(const RelationalCodeState &state) {
    for (const auto &result : relational_code_glue(state))
        temporal_source_detail::require(result.compatible, "incompatible relational code sections");
    return state.sections;
}

enum class RelationalCodeCertification : std::uint8_t { Certified, Obstructed, NotCertified };
struct RelationalCodeDescentResult {
    std::uint64_t descent{};
    RelationalCodeCertification left{}, right{};
    bool certified() const noexcept {
        return left == RelationalCodeCertification::Certified && right == left;
    }
};
namespace relational_code_detail {
struct LinearAction {
    numeric::ExactMatrix values;
    std::set<numeric::ExactMatrix::Key> support;
};
inline bool spend(std::size_t &budget, std::size_t n) {
    if (n > budget)
        return false;
    budget -= n;
    return true;
}
inline std::optional<LinearAction> linear_action(const RelationalCodeAlgebra &algebra, std::uint64_t key,
                                                 std::size_t &budget) {
    if (!spend(budget, 1))
        return std::nullopt;
    const auto &op = algebra.operation(key);
    const auto &in = algebra.type(op.input);
    const auto &out = algebra.type(op.output);
    if (in.kind != RelationalCodeTypeKind::Rational || out.kind != in.kind)
        return std::nullopt;
    if (op.opcode == RelationalCodeOpcode::Identity ||
        (op.opcode == RelationalCodeOpcode::Word && op.word.empty())) {
        if (!spend(budget, in.atoms.size()))
            return std::nullopt;
        LinearAction a{numeric::ExactMatrix::identity(in.atoms.size()), {}};
        for (std::size_t i = 0; i < in.atoms.size(); ++i)
            a.support.emplace(i, i);
        return a;
    }
    if (op.opcode == RelationalCodeOpcode::Linear) {
        if (!spend(budget, op.kernel->support().count()))
            return std::nullopt;
        LinearAction a{numeric::ExactMatrix(out.atoms.size(), in.atoms.size()), {}};
        const auto cols = in.atoms.size();
        op.kernel->support().for_each_active([&](auto ij) { a.support.emplace(ij / cols, ij % cols); });
        for (const auto &e : op.kernel->values().entries())
            a.values.set(e.index / cols, e.index % cols, e.value);
        return a;
    }
    if (op.opcode != RelationalCodeOpcode::Word)
        return std::nullopt;
    std::optional<LinearAction> result;
    for (auto id : op.word) {
        auto next = linear_action(algebra, id, budget);
        if (!next)
            return std::nullopt;
        if (!result) {
            result = std::move(next);
            continue;
        }
        // Count every structural candidate, including supported numerical zeros.
        std::set<numeric::ExactMatrix::Key> support;
        for (const auto &a : next->support)
            for (const auto &b : result->support) {
                if (!spend(budget, 1))
                    return std::nullopt;
                if (a.second == b.first)
                    support.emplace(a.first, b.second);
            }
        result = LinearAction{next->values.multiply(result->values), std::move(support)};
    }
    return result;
}
inline std::optional<LinearAction> compose_linear(const LinearAction &after, const LinearAction &before,
                                                  std::size_t &budget) {
    std::set<numeric::ExactMatrix::Key> support;
    for (const auto &a : after.support)
        for (const auto &b : before.support) {
            if (!spend(budget, 1))
                return std::nullopt;
            if (a.second == b.first)
                support.emplace(a.first, b.second);
        }
    return LinearAction{after.values.multiply(before.values), std::move(support)};
}
inline std::optional<std::size_t> finite_cardinality(const RelationalCodeAlgebra &algebra, std::uint64_t key,
                                                     std::size_t limit) {
    std::map<std::uint64_t, std::optional<std::size_t>> cache;
    std::function<std::optional<std::size_t>(std::uint64_t)> visit =
        [&](auto id) -> std::optional<std::size_t> {
        if (cache.contains(id))
            return cache.at(id);
        const auto &t = algebra.type(id);
        std::optional<std::size_t> result;
        if (t.kind == RelationalCodeTypeKind::Finite) {
            if (t.atoms.size() <= limit)
                result = t.atoms.size();
        } else if (t.kind == RelationalCodeTypeKind::Product) {
            std::size_t count = 1;
            bool valid = true;
            for (const auto &f : t.factors) {
                const auto n = visit(f.type);
                if (!n || (count != 0 && *n > limit / count)) {
                    valid = false;
                    break;
                }
                count *= *n;
            }
            if (valid && count <= limit)
                result = count;
        }
        cache.emplace(id, result);
        return result;
    };
    return visit(key);
}
inline std::optional<std::vector<RelationalCodeValue>> finite_values(const RelationalCodeAlgebra &algebra,
                                                                     std::uint64_t key, std::size_t &budget) {
    const auto &type = algebra.type(key);
    if (!spend(budget, 1) || type.kind == RelationalCodeTypeKind::Rational)
        return std::nullopt;
    std::vector<RelationalCodeValue> out;
    if (type.kind == RelationalCodeTypeKind::Finite) {
        if (!spend(budget, type.atoms.size()))
            return std::nullopt;
        for (auto atom : type.atoms)
            out.push_back(RelationalCodeValue{atom});
        return out;
    }
    out.push_back(RelationalCodeValue{RelationalCodeValue::Product{}});
    for (const auto &factor : type.factors) {
        const auto values = finite_values(algebra, factor.type, budget);
        if (!values)
            return std::nullopt;
        if (!out.empty() && values->size() > budget / out.size())
            return std::nullopt;
        const auto count = out.size() * values->size();
        if (!spend(budget, count))
            return std::nullopt;
        std::vector<RelationalCodeValue> next;
        for (const auto &prefix : out)
            for (const auto &suffix : *values) {
                auto tuple = std::get<2>(prefix.data);
                tuple.push_back(suffix);
                next.push_back(RelationalCodeValue{std::move(tuple)});
            }
        out = std::move(next);
    }
    return out;
}
inline RelationalCodeCertification square(const RelationalCodeAlgebra &algebra, std::uint64_t restriction,
                                          std::uint64_t local, std::uint64_t common, std::size_t budget) {
    const auto &r = algebra.operation(restriction);
    if (algebra.type(r.input).kind == RelationalCodeTypeKind::Rational) {
        const auto R = linear_action(algebra, restriction, budget), L = linear_action(algebra, local, budget),
                   K = linear_action(algebra, common, budget);
        if (!R || !L || !K)
            return RelationalCodeCertification::NotCertified;
        const auto a = compose_linear(*R, *L, budget), b = compose_linear(*K, *R, budget);
        if (!a || !b)
            return RelationalCodeCertification::NotCertified;
        return a->values == b->values && a->support == b->support ? RelationalCodeCertification::Certified
                                                                  : RelationalCodeCertification::Obstructed;
    }
    if (!finite_cardinality(algebra, r.input, budget))
        return RelationalCodeCertification::NotCertified;
    const auto inputs = finite_values(algebra, r.input, budget);
    if (!inputs)
        return RelationalCodeCertification::NotCertified;
    for (const auto &value : *inputs) {
        // Evaluation has an independent per-call budget; state enumeration is
        // bounded above. Exceeding either bound is not a false certificate.
        try {
            const auto a = algebra.apply(restriction, algebra.apply(local, value, budget), budget);
            const auto b = algebra.apply(common, algebra.apply(restriction, value, budget), budget);
            if (!value_equal(a, b))
                return RelationalCodeCertification::Obstructed;
        } catch (const std::invalid_argument &) {
            return RelationalCodeCertification::NotCertified;
        }
    }
    return RelationalCodeCertification::Certified;
}
} // namespace relational_code_detail
inline std::vector<RelationalCodeDescentResult>
certify_relational_code_descents(const RelationalCodeState &state, std::size_t budget = 1000000) {
    validate_relational_code_state(state);
    RelationalCodeAlgebra algebra(state);
    const auto interfaces = relational_code_detail::index(state.interfaces);
    std::vector<RelationalCodeDescentResult> out;
    for (const auto &d : state.descents) {
        const auto &i = *interfaces.at(d.interface);
        out.push_back({d.key,
                       relational_code_detail::square(algebra, i.left_restriction, d.left_operation,
                                                      d.interface_operation, budget),
                       relational_code_detail::square(algebra, i.right_restriction, d.right_operation,
                                                      d.interface_operation, budget)});
    }
    return out;
}

struct RelationalCodeInnovation {
    RelationalCodeValue propagated, target;
    // Present only on rational carriers; finite/product changes are retained
    // nonadditive target changes, never fabricated rational innovations.
    std::optional<numeric::SparseRationalVector> rational;
};
inline RelationalCodeInnovation
relational_code_innovation(const RelationalCodeState &old, const RelationalCodeState &next,
                           std::uint64_t section, const std::vector<RelationalCodeTransport> &maps = {}) {
    validate_relational_code_state(old);
    validate_relational_code_state(next);
    validate_relational_code_transition(old, next, maps);
    const auto os = relational_code_detail::index(old.sections),
               ns = relational_code_detail::index(next.sections);
    temporal_source_detail::require(!os.at(section)->source_aspects && !ns.at(section)->source_aspects,
                                    "resolve source views before innovation comparison");
    auto propagated = os.at(section)->value;
    const auto &target = ns.at(section)->value;
    RelationalCodeAlgebra algebra(next);
    for (const auto &m : maps)
        if (m.section == section)
            propagated = algebra.apply(m.operation, propagated);
    RelationalCodeInnovation out{std::move(propagated), target, std::nullopt};
    if (auto p = std::get_if<numeric::SparseRationalTensor>(&out.propagated.data)) {
        const auto &t = std::get<0>(target.data);
        out.rational = temporal_endpoint_detail::sparse_sum(t.values(), p->values(), -1);
    }
    return out;
}

// Sign of a declared factor permutation. Values stay factorized; callers of a
// graded tensor realization apply this sign rather than silently dropping it.
inline int relational_code_permutation_sign(const RelationalCodeType &from, const RelationalCodeType &to) {
    using namespace relational_code_detail;
    require(from.kind == RelationalCodeTypeKind::Product && to.kind == from.kind &&
                from.factors.size() == to.factors.size(),
            "not a factor permutation");
    std::set<std::uint64_t> seen;
    std::vector<std::size_t> order;
    for (const auto &f : to.factors) {
        require(seen.insert(f.key).second, "duplicate permutation factor");
        const auto i = factor_index(from, f.key);
        require(from.factors[i] == f, "factor permutation changes type/grade");
        order.push_back(i);
    }
    int sign = 1;
    for (std::size_t i = 0; i < order.size(); ++i)
        for (std::size_t j = i + 1; j < order.size(); ++j)
            if (order[i] > order[j] && (to.factors[i].grade & 1u) && (to.factors[j].grade & 1u))
                sign = -sign;
    return sign;
}

struct RelationalCodeIdentity {
    std::uint64_t namespace_key{}, section{};
    auto operator<=>(const RelationalCodeIdentity &) const = default;
};
// A physical observation accelerator. A bucket never identifies an assertion;
// all hits undergo exact byte comparison, retaining every distinct identity.
class RelationalCodeObservationIndex {
  public:
    // The selector must be deterministic for this index's lifetime. It only
    // selects candidates; exact bytes decide matches, even under collisions.
    using Selector = std::function<std::uint64_t(std::span<const std::byte>)>;
    explicit RelationalCodeObservationIndex(Selector selector =
                                                [](auto bytes) { return rfcx_detail::fnv1a64(bytes); })
        : selector_(std::move(selector)) {
        if (!selector_) throw std::invalid_argument("missing observation selector");
    }
    void insert(RelationalCodeIdentity id, std::vector<std::byte> exact) {
        temporal_source_detail::require(!selecting_, "observation selector cannot reenter mutation");
        temporal_source_detail::require(id.namespace_key != 0 && id.section != 0 && !entries_.contains(id),
                                        "invalid/duplicate observation identity");
        const auto bucket = select(exact);
        auto [it, inserted] = entries_.emplace(id, Entry{bucket, std::move(exact)});
        temporal_source_detail::require(inserted, "duplicate observation identity after selection");
        try { buckets_[bucket].insert(id); }
        catch (...) {
            entries_.erase(it);
            const auto b = buckets_.find(bucket);
            if (b != buckets_.end() && b->second.empty()) buckets_.erase(b);
            throw;
        }
    }
    // Strict persistent-identity update. Select and allocate before publishing;
    // a throwing selector or allocation leaves the previous observation intact.
    void replace(RelationalCodeIdentity id, std::vector<std::byte> exact) {
        temporal_source_detail::require(!selecting_, "observation selector cannot reenter mutation");
        const auto it = entries_.find(id);
        temporal_source_detail::require(it != entries_.end(), "replacement of missing observation identity");
        const auto bucket = select(exact), old_bucket = it->second.bucket;
        if (bucket != old_bucket) {
            try { buckets_[bucket].insert(id); }
            catch (...) {
                const auto b = buckets_.find(bucket);
                if (b != buckets_.end() && b->second.empty()) buckets_.erase(b);
                throw;
            }
        }
        it->second.exact.swap(exact);
        it->second.bucket = bucket;
        if (bucket != old_bucket) erase_bucket_identity(old_bucket, id);
    }
    bool erase(RelationalCodeIdentity id) {
        temporal_source_detail::require(!selecting_, "observation selector cannot reenter mutation");
        const auto it = entries_.find(id);
        if (it == entries_.end()) return false;
        erase_bucket_identity(it->second.bucket, id);
        entries_.erase(it);
        return true;
    }
    bool contains(RelationalCodeIdentity id) const { return entries_.contains(id); }
    std::size_t size() const noexcept { return entries_.size(); }
    std::vector<RelationalCodeIdentity> find(std::span<const std::byte> exact) const {
        std::vector<RelationalCodeIdentity> out;
        const auto it = buckets_.find(select(exact));
        if (it == buckets_.end()) return out;
        for (const auto &id : it->second) {
            const auto &stored = entries_.at(id).exact;
            if (stored.size() == exact.size() && std::equal(stored.begin(), stored.end(), exact.begin()))
                out.push_back(id);
        }
        return out;
    }

  private:
    std::uint64_t select(std::span<const std::byte> exact) const {
        temporal_source_detail::require(!selecting_, "reentered observation selector");
        selecting_ = true;
        try {
            const auto bucket = selector_(exact);
            selecting_ = false;
            return bucket;
        } catch (...) {
            selecting_ = false;
            throw;
        }
    }
    struct Entry { std::uint64_t bucket{}; std::vector<std::byte> exact; };
    void erase_bucket_identity(std::uint64_t bucket, RelationalCodeIdentity id) {
        const auto it = buckets_.find(bucket);
        it->second.erase(id);
        if (it->second.empty()) buckets_.erase(it);
    }
    Selector selector_;
    mutable bool selecting_{};
    std::map<RelationalCodeIdentity, Entry> entries_;
    std::map<std::uint64_t, std::set<RelationalCodeIdentity>> buckets_;
};

// Native declaration union in one retained namespace. Shared identities must
// have the same definition; equal content under different IDs stays distinct.
// Bridge interfaces can relate unlike carriers without equating their types.
inline RelationalCodeState
glue_relational_code_states(const RelationalCodeState &a, const RelationalCodeState &b,
                            const std::vector<RelationalCodeInterface> &bridges = {}) {
    using namespace relational_code_detail;
    validate_relational_code_state(a);
    validate_relational_code_state(b);
    require(a.namespace_key == b.namespace_key, "code gluing needs a declared common namespace");
    RelationalCodeState out = a;
    const auto merge = [](auto &target, const auto &incoming, auto equal) {
        for (const auto &value : incoming) {
            const auto it = std::find_if(target.begin(), target.end(),
                                         [&](const auto &item) { return item.key == value.key; });
            if (it == target.end())
                target.push_back(value);
            else
                require(equal(*it, value), "conflicting declaration on shared code identity");
        }
    };
    merge(out.types, b.types, [](const auto &x, const auto &y) { return x == y; });
    merge(out.operations, b.operations, [](const auto &x, const auto &y) {
        temporal_codec_detail::Writer wx, wy;
        temporal_codec_detail::code_operation(wx, x);
        temporal_codec_detail::code_operation(wy, y);
        return wx.bytes() == wy.bytes();
    });
    for (auto f : b.fields)
        if (std::find(out.fields.begin(), out.fields.end(), f) == out.fields.end())
            out.fields.push_back(f);
    merge(out.sections, b.sections, [](const auto &x, const auto &y) {
        return x.field == y.field && x.type == y.type && x.block == y.block && x.cell == y.cell &&
               x.occurrence == y.occurrence && x.source_aspects == y.source_aspects &&
               value_equal(x.value, y.value);
    });
    merge(out.interfaces, b.interfaces, [](const auto &x, const auto &y) { return x == y; });
    merge(out.interfaces, bridges, [](const auto &x, const auto &y) { return x == y; });
    merge(out.descents, b.descents, [](const auto &x, const auto &y) { return x == y; });
    validate_relational_code_state(out);
    return out;
}

namespace relational_code_detail {
inline void retain_type_closure(RelationalCodeState &out, const RelationalCodeAlgebra &algebra,
                                std::uint64_t key) {
    if (std::any_of(out.types.begin(), out.types.end(), [&](const auto &t) { return t.key == key; }))
        return;
    const auto &t = algebra.type(key);
    out.types.push_back(t);
    for (const auto &factor : t.factors)
        retain_type_closure(out, algebra, factor.type);
}
} // namespace relational_code_detail
// Canonical observation of a typed section, excluding the section's assertion
// ID. The lookup index returns ALL distinct assertions with this observation.
inline std::vector<std::byte> relational_code_section_bytes(const RelationalCodeState &state,
                                                            std::uint64_t section) {
    using namespace temporal_codec_detail;
    validate_relational_code_state(state);
    RelationalCodeAlgebra algebra(state);
    const auto ss = relational_code_detail::index(state.sections);
    const auto &s = *ss.at(section);
    temporal_source_detail::require(!s.source_aspects,
                                    "source view observation requires the endpoint overload");
    RelationalCodeState declaration;
    declaration.namespace_key = state.namespace_key;
    relational_code_detail::retain_type_closure(declaration, algebra, s.type);
    declaration.fields = {s.field};
    Writer w;
    w.u64(0x31444c4549464352ULL);
    code_state(w, declaration);
    w.u64(s.type);
    w.u64(s.block);
    w.u8(s.cell.has_value());
    if (s.cell)
        cell(w, *s.cell);
    w.u8(s.occurrence.has_value());
    if (s.occurrence)
        occurrence(w, *s.occurrence);
    code_value(w, s.value);
    return w.bytes();
}
// Includes the operation word, all referenced operation definitions and type
// declarations. Same observed endpoints do not identify different operations.
inline std::vector<std::byte> relational_code_operation_bytes(const RelationalCodeState &state,
                                                              std::uint64_t operation) {
    using namespace temporal_codec_detail;
    validate_relational_code_state(state);
    RelationalCodeAlgebra algebra(state);
    RelationalCodeState declaration;
    declaration.namespace_key = state.namespace_key;
    std::set<std::uint64_t> seen;
    std::function<void(std::uint64_t)> visit = [&](auto key) {
        if (!seen.insert(key).second)
            return;
        const auto &op = algebra.operation(key);
        declaration.operations.push_back(op);
        relational_code_detail::retain_type_closure(declaration, algebra, op.input);
        relational_code_detail::retain_type_closure(declaration, algebra, op.output);
        if (op.opcode == RelationalCodeOpcode::Word)
            for (auto id : op.word)
                visit(id);
        for (const auto &leg : op.legs)
            visit(leg.operation);
    };
    visit(operation);
    Writer w;
    w.u64(0x31504f444f435252ULL);
    w.u64(operation);
    code_state(w, declaration);
    return w.bytes();
}

// Explicit source-factor binding by aspect NAME, not incidental aspect number.
inline RelationalCodeType
relational_aspect_code_type(const core::RelationalComplex &source, std::uint64_t key,
                            std::uint64_t boolean_type,
                            const std::vector<std::pair<std::uint64_t, composite::AspectId>> &bindings) {
    RelationalCodeType type;
    type.key = key;
    type.kind = RelationalCodeTypeKind::Product;
    for (const auto &[factor, aspect] : bindings)
        type.factors.push_back({factor, boolean_type, 0, source.signature().aspects().at(aspect).name});
    return type;
}
inline RelationalCodeSection relational_aspect_code_section(std::uint64_t key, std::uint64_t field,
                                                            std::uint64_t type,
                                                            StableOccurrenceKey occurrence,
                                                            std::uint64_t block = 0) {
    RelationalCodeSection section;
    section.key = key;
    section.field = field;
    section.type = type;
    section.occurrence = occurrence;
    section.block = block;
    section.source_aspects = true;
    return section;
}
// Resolve only when explicit values are requested. Persistence keeps the source
// view and reads the canonical source at the requested temporal endpoint.
inline RelationalCodeState materialize_relational_code_views(const TemporalEndpoint &endpoint) {
    validate_temporal_endpoint(endpoint);
    temporal_source_detail::require(endpoint.codes.has_value(), "endpoint has no code layer");
    auto result = *endpoint.codes;
    RelationalCodeAlgebra algebra(result);
    const StableSourceIndex index(endpoint.source);
    for (auto &section : result.sections)
        if (section.source_aspects) {
            const auto &type = algebra.type(section.type);
            const auto slot = index.occurrence(*section.occurrence).value();
            const auto &planes = endpoint.source.grade(section.occurrence->upper_grade).aspects;
            RelationalCodeValue::Product values;
            for (const auto &factor : type.factors)
                values.push_back(RelationalCodeValue{static_cast<std::uint64_t>(
                    planes.get(endpoint.source.signature().aspects().id(factor.source_aspect), slot))});
            section.value = RelationalCodeValue{std::move(values)};
            section.source_aspects = false;
        }
    return result;
}
inline std::vector<RelationalCodeGlueResult> relational_code_glue(const TemporalEndpoint &endpoint) {
    return relational_code_glue(materialize_relational_code_views(endpoint));
}
inline std::vector<std::byte> relational_code_section_bytes(const TemporalEndpoint &endpoint,
                                                            std::uint64_t section) {
    return relational_code_section_bytes(materialize_relational_code_views(endpoint), section);
}
// Packed execution of a retained Binary operation against source aspect views.
// No factor tensor or one-object-per-occurrence expansion is needed.
inline std::vector<machine::PackedBits>
execute_relational_aspect_code(const TemporalEndpoint &endpoint, std::uint64_t operation, core::Grade grade) {
    validate_temporal_endpoint(endpoint);
    temporal_source_detail::require(endpoint.codes.has_value(), "endpoint has no code layer");
    RelationalCodeAlgebra algebra(*endpoint.codes);
    const auto &op = algebra.operation(operation);
    temporal_source_detail::require(op.opcode == RelationalCodeOpcode::Binary,
                                    "packed source execution needs Binary operation");
    const auto &type = algebra.type(op.input);
    std::vector<composite::AspectId> aspects;
    for (const auto &factor : type.factors)
        aspects.push_back(endpoint.source.signature().aspects().id(factor.source_aspect));
    const auto &planes = endpoint.source.grade(grade).aspects;
    if (op.registers.empty()) return {};
    const composite::BinaryExecutionPlan plan(*op.binary, op.registers);
    plan.validate_aspects(aspects.size());
    composite::BinaryExecutionWorkspace workspace;
    return plan.materialize(planes.slots(), [&](auto factor, auto wi) {
        return planes.plane(aspects.at(factor)).word(wi);
    }, workspace);
}

} // namespace rfc::storage
