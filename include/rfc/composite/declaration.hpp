#pragma once

#include "rfc/machine/checked_size.hpp"
#include "rfc/numeric/index_domain.hpp"
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::composite {

// Stable semantic names, not local slots, display strings or content hashes.
struct DeclarationId {
    std::uint64_t name_space{}, key{}, version{};
    auto operator<=>(const DeclarationId&) const = default;
    [[nodiscard]] bool valid() const noexcept { return name_space && key && version; }
};

enum class DeclarationKind : std::uint8_t {
    Boolean = 0, Finite = 1, Integer = 2, Rational = 3,
    Product = 4, Alternative = 5, Optional = 6, Repeated = 7, RationalTensor = 8
};
struct DeclarationFactor {
    std::uint64_t key{};
    DeclarationId type;
    std::uint16_t grade{}; // Retained provenance; product encoding does not apply a boundary.
    bool operator==(const DeclarationFactor&) const = default;
};
struct DeclarationAxis {
    std::uint64_t key{};
    std::size_t extent{};
    // Empty = declared ordinal coordinates; otherwise stable coordinate names,
    // in meaningful order. This bridges existing relational-code coordinate IDs.
    std::vector<std::uint64_t> coordinates;
    bool operator==(const DeclarationAxis&) const = default;
};

// Builder-time definition. A DeclarationRegistry owns validated immutable copies.
// Display labels intentionally live outside this executable semantic definition.
struct Declaration {
    DeclarationId id;
    DeclarationKind kind{DeclarationKind::Boolean};
    std::vector<std::uint64_t> symbols;
    std::vector<DeclarationFactor> factors;
    DeclarationId element;
    std::size_t min_count{}, max_count{}; // Repeated: finite inclusive bounds, including zero.
    std::vector<DeclarationAxis> axes;
    bool operator==(const Declaration&) const = default;

    [[nodiscard]] static Declaration boolean(DeclarationId id) { return leaf(id, DeclarationKind::Boolean); }
    [[nodiscard]] static Declaration integer(DeclarationId id) { return leaf(id, DeclarationKind::Integer); }
    [[nodiscard]] static Declaration rational(DeclarationId id) { return leaf(id, DeclarationKind::Rational); }
    [[nodiscard]] static Declaration finite(DeclarationId id, std::vector<std::uint64_t> symbols) {
        auto d = leaf(id, DeclarationKind::Finite); d.symbols = std::move(symbols); return d;
    }
    [[nodiscard]] static Declaration product(DeclarationId id, std::vector<DeclarationFactor> factors) {
        auto d = leaf(id, DeclarationKind::Product); d.factors = std::move(factors); return d;
    }
    [[nodiscard]] static Declaration alternative(DeclarationId id, std::vector<DeclarationFactor> alternatives) {
        auto d = leaf(id, DeclarationKind::Alternative); d.factors = std::move(alternatives); return d;
    }
    [[nodiscard]] static Declaration optional(DeclarationId id, DeclarationId element) {
        auto d = leaf(id, DeclarationKind::Optional); d.element = element; return d;
    }
    [[nodiscard]] static Declaration repeated(DeclarationId id, DeclarationId element,
                                              std::size_t min_count, std::size_t max_count) {
        auto d = leaf(id, DeclarationKind::Repeated); d.element = element;
        d.min_count = min_count; d.max_count = max_count; return d;
    }
    [[nodiscard]] static Declaration rational_tensor(DeclarationId id, std::vector<DeclarationAxis> axes) {
        auto d = leaf(id, DeclarationKind::RationalTensor); d.axes = std::move(axes); return d;
    }
private:
    static Declaration leaf(DeclarationId id, DeclarationKind kind) {
        Declaration d; d.id = id; d.kind = kind; return d;
    }
};

// Resource policy is not a mathematical value restriction and is not persisted as
// a type. A caller can tighten it. Hard depth ceilings prevent a policy from
// disabling stack safety. These are per operation, not process-wide quotas.
struct DeclarationLimits {
    std::size_t max_definitions{4096};
    std::size_t max_components{65536};
    std::size_t max_depth{64};
    std::size_t max_value_nodes{1u << 20};
    std::size_t max_encoded_bytes{64u << 20};
    std::size_t max_integer_bits{1u << 20};
    std::size_t max_materialized_bytes{64u << 20};
    std::size_t max_tensor_rank{64};
    void validate() const {
        if (!max_definitions || !max_depth || max_depth > 128 || !max_value_nodes ||
            !max_encoded_bytes || !max_integer_bits || !max_materialized_bytes || max_tensor_rank > 128)
            throw std::invalid_argument("invalid declaration resource limits");
    }
};

namespace declaration_detail {
inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
inline void budget(std::size_t amount, std::size_t maximum, const char* message) {
    if (amount > maximum) throw std::length_error(message);
}
inline std::size_t tag_bits(std::size_t count) {
    require(count != 0, "empty finite code domain");
    std::size_t bits = 0;
    for (--count; count; count >>= 1u) ++bits;
    return bits;
}
inline std::size_t varuint_bytes(std::size_t n) {
    std::size_t bytes = 1;
    while (n >= 128) { n >>= 7u; ++bytes; }
    return bytes;
}
} // namespace declaration_detail

class DeclarationRegistry {
public:
    explicit DeclarationRegistry(std::vector<Declaration> definitions,
                                 DeclarationLimits limits = {}) : definitions_(std::move(definitions)) {
        using namespace declaration_detail;
        limits.validate(); budget(definitions_.size(), limits.max_definitions, "too many declarations");
        std::sort(definitions_.begin(), definitions_.end(), [](const auto& a, const auto& b){ return a.id < b.id; });
        // Exact repeated imports are harmless; conflicting definitions of the
        // same immutable name are not. Wire readers additionally demand no duplicates.
        std::vector<Declaration> unique;
        unique.reserve(definitions_.size());
        for (auto& d : definitions_) {
            if (!unique.empty() && unique.back().id == d.id) {
                require(unique.back() == d, "conflicting declaration import");
            } else unique.push_back(std::move(d));
        }
        definitions_ = std::move(unique);
        std::size_t components = 0;
        for (std::size_t i = 0; i < definitions_.size(); ++i) {
            const auto& d = definitions_[i];
            require(d.id.valid(), "zero declaration namespace/key/version");
            index_.emplace(d.id, i);
            components = machine::checked_size_add(components, d.symbols.size());
            components = machine::checked_size_add(components, d.factors.size());
            components = machine::checked_size_add(components, d.axes.size());
            for (const auto& a : d.axes) components = machine::checked_size_add(components, a.coordinates.size());
            budget(components, limits.max_components, "declaration component budget");
            validate_definition(d, limits);
            if (d.kind == DeclarationKind::Finite) {
                auto& positions = symbol_positions_[d.id];
                for (std::size_t j = 0; j < d.symbols.size(); ++j) positions.emplace(d.symbols[j], j);
            }
            if (d.kind == DeclarationKind::Product || d.kind == DeclarationKind::Alternative) {
                auto& positions = alternative_positions_[d.id];
                for (std::size_t j = 0; j < d.factors.size(); ++j) positions.emplace(d.factors[j].key, j);
            }
        }
        // Memoized longest path, not merely visited DFS: shared DAG subgraphs
        // must not hide excessive depth on a later incoming path.
        std::map<DeclarationId, unsigned> state;
        std::function<std::size_t(DeclarationId, std::size_t)> visit = [&](DeclarationId id, std::size_t stack) {
            budget(stack, limits.max_depth, "declaration nesting budget");
            const auto& d = at(id);
            if (state[id] == 1) throw std::invalid_argument("cyclic declaration graph");
            if (state[id] == 2) return depths_.at(id);
            state[id] = 1; std::size_t depth = 1;
            auto child = [&](DeclarationId ref) {
                const auto next = machine::checked_size_add(visit(ref, stack + 1), 1);
                depth = std::max(depth, next);
            };
            for (const auto& f : d.factors) child(f.type);
            if (d.element.valid()) child(d.element);
            budget(depth, limits.max_depth, "declaration nesting budget");
            state[id] = 2; depths_[id] = depth; return depth;
        };
        for (const auto& d : definitions_) (void)visit(d.id, 1);
    }
    [[nodiscard]] const Declaration& at(DeclarationId id) const {
        const auto it = index_.find(id);
        if (it == index_.end()) throw std::invalid_argument("unresolved declaration reference");
        return definitions_.at(it->second);
    }
    [[nodiscard]] const std::vector<Declaration>& definitions() const noexcept { return definitions_; }
    [[nodiscard]] std::size_t depth(DeclarationId id) const { return depths_.at(id); }
    [[nodiscard]] std::size_t symbol_ordinal(DeclarationId id, std::uint64_t symbol) const {
        return symbol_positions_.at(id).at(symbol);
    }
    [[nodiscard]] std::size_t alternative_ordinal(DeclarationId id, std::uint64_t key) const {
        if(at(id).kind!=DeclarationKind::Alternative)
            throw std::out_of_range("not an alternative declaration");
        return factor_ordinal(id,key);
    }
    [[nodiscard]] std::size_t factor_ordinal(DeclarationId id, std::uint64_t key) const {
        return alternative_positions_.at(id).at(key);
    }
    [[nodiscard]] DeclarationRegistry imported(const DeclarationRegistry& other,
                                                DeclarationLimits limits = {}) const {
        auto result = definitions_;
        declaration_detail::budget(machine::checked_size_add(result.size(), other.definitions_.size()),
                                   limits.max_definitions, "declaration import budget");
        result.insert(result.end(), other.definitions_.begin(), other.definitions_.end());
        return DeclarationRegistry(std::move(result), limits);
    }
    [[nodiscard]] static numeric::IndexDomain tensor_domain(const Declaration& d) {
        declaration_detail::require(d.kind == DeclarationKind::RationalTensor, "not a tensor declaration");
        std::vector<std::size_t> extents; extents.reserve(d.axes.size());
        for (const auto& a : d.axes) extents.push_back(a.extent);
        return numeric::IndexDomain(std::move(extents));
    }
private:
    static void validate_definition(const Declaration& d, const DeclarationLimits& limits) {
        using namespace declaration_detail;
        const bool finite = d.kind == DeclarationKind::Finite;
        const bool factors = d.kind == DeclarationKind::Product || d.kind == DeclarationKind::Alternative;
        const bool element = d.kind == DeclarationKind::Optional || d.kind == DeclarationKind::Repeated;
        const bool repeated = d.kind == DeclarationKind::Repeated;
        const bool tensor = d.kind == DeclarationKind::RationalTensor;
        switch (d.kind) {
            case DeclarationKind::Boolean: case DeclarationKind::Finite: case DeclarationKind::Integer:
            case DeclarationKind::Rational: case DeclarationKind::Product: case DeclarationKind::Alternative:
            case DeclarationKind::Optional: case DeclarationKind::Repeated: case DeclarationKind::RationalTensor: break;
            default: throw std::invalid_argument("unknown executable declaration constructor");
        }
        require(finite || d.symbols.empty(), "unexpected symbol metadata");
        require(factors || d.factors.empty(), "unexpected factor metadata");
        require(element ? d.element.valid() : d.element == DeclarationId{}, "invalid element reference");
        require(repeated ? d.min_count <= d.max_count : d.min_count == 0 && d.max_count == 0,
                "invalid repetition bounds");
        require(tensor || d.axes.empty(), "unexpected tensor metadata");
        if (finite) {
            require(!d.symbols.empty(), "empty finite declaration");
            const std::set<std::uint64_t> names(d.symbols.begin(), d.symbols.end());
            require(names.size() == d.symbols.size(), "duplicate finite symbol");
        }
        if (factors) {
            require(d.kind != DeclarationKind::Alternative || !d.factors.empty(), "empty alternative declaration");
            std::set<std::uint64_t> names;
            for (const auto& f : d.factors) {
                require(f.key && names.insert(f.key).second, "zero/duplicate factor identity");
                require(f.type.valid(), "invalid factor type reference");
                require(d.kind != DeclarationKind::Alternative || f.grade == 0,
                        "alternative cases are not graded product factors");
            }
        }
        if (tensor) {
            budget(d.axes.size(), limits.max_tensor_rank, "tensor rank budget");
            std::set<std::uint64_t> names;
            for (const auto& a : d.axes) {
                require(a.key && names.insert(a.key).second, "zero/duplicate tensor axis identity");
                require(a.coordinates.empty() || a.coordinates.size() == a.extent, "coordinate identity count");
                const std::set<std::uint64_t> coordinates(a.coordinates.begin(), a.coordinates.end());
                require(coordinates.size() == a.coordinates.size(), "duplicate tensor coordinate identity");
            }
            (void)tensor_domain(d); // Checked shape product, without Cartesian allocation.
        }
    }
    std::vector<Declaration> definitions_;
    std::map<DeclarationId, std::size_t> index_, depths_;
    std::map<DeclarationId, std::map<std::uint64_t, std::size_t>> symbol_positions_, alternative_positions_;
};

// Partial modules are allowed while assembling declarations, but never while
// executing them. freeze resolves imports and validates the entire graph before
// publishing an independent immutable registry.
class DeclarationBuilder {
public:
    explicit DeclarationBuilder(DeclarationLimits limits = {}) : limits_(limits) { limits_.validate(); }
    void define(Declaration definition) {
        declaration_detail::budget(machine::checked_size_add(definitions_.size(), 1), limits_.max_definitions,
                                   "declaration builder budget");
        definitions_.push_back(std::move(definition));
    }
    void include(const DeclarationRegistry& imported) {
        declaration_detail::budget(machine::checked_size_add(definitions_.size(), imported.definitions().size()),
                                   limits_.max_definitions, "declaration builder import budget");
        definitions_.insert(definitions_.end(), imported.definitions().begin(), imported.definitions().end());
    }
    [[nodiscard]] DeclarationRegistry freeze() const { return DeclarationRegistry(definitions_, limits_); }
private:
    DeclarationLimits limits_;
    std::vector<Declaration> definitions_;
};

} // namespace rfc::composite
