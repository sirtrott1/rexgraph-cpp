#pragma once

#include "rfc/inference/relational_literal_input.hpp"
#include "rfc/learning/relational_finite_field.hpp"
#include "rfc/storage/prepared_byte_packet.hpp"

namespace rfc::learning {
namespace literal_finite_field_detail {
inline constexpr std::array<std::byte, 8> value_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'L'}, std::byte{'V'},
    std::byte{1}, std::byte{0}, std::byte{0}};

inline storage::PreparedBytePacket value(inference::OwnedRelationalInputUseAsset asset,
    std::uint64_t use, std::size_t max_bytes, numeric::ExactActionGuard& guard) {
    using namespace inference::field_program_detail;
    const auto& spans = asset.use(use).spans; guard.dimension(spans.size());
    std::size_t size = 40;
    for (const auto span : spans) {
        guard.step(); size = add(size, add(8, span.end - span.begin));
    }
    require(size <= max_bytes, "literal use value exceeds byte admission"); guard.entries(size);
    storage::BytePacketBuilder writer(value_magic, max_bytes); writer.u64(spans.size());
    for (const auto span : spans) {
        guard.step();
        for (auto i = span.begin; i < span.end; ++i) guard.step();
        writer.blob(storage::PreparedBytePacket::pin(asset, [span](const auto& held) {
            return std::span<const std::byte>(held.declaration().bytes).subspan(span.begin, span.end - span.begin);
        }));
    }
    return std::move(writer).finish([&] { guard.step(); });
}
} // namespace literal_finite_field_detail

// A value encodes only the ordered span bytes. The asset retains the distinct
// original uses, cuts, source identities and bytes outside those cuts.
inline storage::PreparedBytePacket prepare_relational_literal_use_value(
    inference::OwnedRelationalInputUseAsset asset, std::uint64_t use, std::size_t max_bytes,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    numeric::ExactActionGuard guard(limits, std::move(check));
    return literal_finite_field_detail::value(std::move(asset), use, max_bytes, guard);
}
inline std::vector<std::byte> encode_relational_literal_use_value(
    inference::OwnedRelationalInputUseAsset asset, std::uint64_t use, std::size_t max_bytes,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    return prepare_relational_literal_use_value(std::move(asset), use, max_bytes, limits, check).bytes(check);
}

struct RelationalLiteralFiniteFieldRule {
    std::uint64_t key{};
    std::vector<std::uint64_t> bindings, symbols;
    std::optional<std::size_t> target;
    std::vector<inference::OwnedRelationalLiteralInput> inputs;
    std::string unsupported;
};
struct RelationalLiteralFiniteFieldDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalLiteralFiniteFieldRule> rules;
};
class RelationalLiteralFiniteFieldResult;

// This specialization derives singleton literal facts, not interpretation
// alternatives. Numerical observations remain declared maps of the bytes;
// their faithful accession does not assert a numerical inverse to those bytes.
class PreparedRelationalLiteralFiniteFieldMap {
    struct State {
        RelationalLiteralFiniteFieldDeclaration declaration;
        PreparedRelationalFiniteFieldMap map;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedRelationalLiteralFiniteFieldMap(inference::OwnedRelationalFiniteSource source,
        std::vector<RelationalFiniteFieldTarget> targets, RelationalLiteralFiniteFieldDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace inference::field_interface_detail;
        const auto pin = source; const auto frozen = declaration; const auto target_pins = targets;
        Guard guard(limits, check); const auto asset = pin.asset(); const auto& d = pin.declaration();
        guard.dimension(d.bindings.size()); guard.dimension(frozen.rules.size());
        std::size_t retained = add(d.bindings.size(), frozen.rules.size()); guard.entries(retained);
        std::map<std::uint64_t, const inference::RelationalFiniteSymbol*> symbols;
        for (const auto& symbol : d.symbols) { guard.step(); symbols.emplace(symbol.key, &symbol); }
        std::map<std::uint64_t, storage::PreparedBytePacket> values;
        std::set<std::uint64_t> bound_uses, bound_symbols, bindings;
        for (const auto& b : d.bindings) {
            guard.step(); const auto& domain = d.variables[pin.variable_position(b.cell)].domain;
            require(domain.size() == 1, "literal finite value must be a singleton fact, not interpretation alternatives");
            const auto* symbol = symbols.at(domain.front());
            if (!values.contains(b.use)) {
                auto v = literal_finite_field_detail::value(asset, b.use, limits.entries, guard);
                retained = add(retained, v.size()); guard.entries(retained); values.emplace(b.use, std::move(v));
            }
            values.at(b.use).require_equal(symbol->definition, [&] { guard.step(); });
            bound_uses.insert(b.use); bound_symbols.insert(symbol->key); bindings.insert(b.key);
        }
        require(bound_uses.size() == asset.declaration().uses.size(), "literal finite source omits complete original uses");
        require(bound_symbols.size() == symbols.size(), "literal finite source contains an underived symbol");
        RelationalFiniteFieldMapDeclaration map{frozen.key, frozen.revision, {}};
        bool consumed{};
        for (const auto& rule : frozen.rules) {
            guard.step(); retained = add(retained, add(rule.inputs.size(), add(rule.bindings.size(), rule.symbols.size())));
            guard.entries(retained);
            RelationalFiniteFieldRule r{rule.key, rule.bindings, rule.symbols, rule.target, {}, rule.unsupported};
            if (rule.target) {
                require(!rule.inputs.empty(), "literal finite interpretation must consume an actual literal input");
                const std::set<std::uint64_t> dependencies(rule.bindings.begin(), rule.bindings.end());
                require(dependencies == bindings, "literal finite rule omits whole-use dependencies");
            }
            for (const auto& literal : rule.inputs) {
                guard.step(); require(literal.input().accession().asset().same_owner(asset),
                    "literal finite input has a foreign actual byte owner");
                for (const auto& c : literal.declaration().coordinates) {
                    guard.step(); require(bound_uses.contains(c.use), "literal coordinate lacks a byte-derived finite use");
                }
                r.inputs.push_back(literal.input()); consumed = true;
            }
            map.rules.push_back(std::move(r));
        }
        require(consumed, "literal finite map has no consumed byte-derived input");
        auto remaining = limits; remaining.steps -= guard.steps();
        PreparedRelationalFiniteFieldMap actual(pin, target_pins, std::move(map), remaining, check);
        state_ = std::make_shared<const State>(State{frozen, std::move(actual)});
    }
    inference::OwnedRelationalFiniteSource source() const { return state_->map.source(); }
    PreparedRelationalFiniteFieldMap map() const { return state_->map; }
    const RelationalLiteralFiniteFieldDeclaration& declaration() const & { return state_->declaration; }
    const RelationalLiteralFiniteFieldDeclaration& declaration() const && = delete;
    bool same_owner(const PreparedRelationalLiteralFiniteFieldMap& other) const noexcept { return state_ == other.state_; }
    RelationalLiteralFiniteFieldResult execute(numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
};

class RelationalLiteralFiniteFieldResult {
    struct State {
        PreparedRelationalLiteralFiniteFieldMap map;
        RelationalFiniteFieldResult family;
    };
    std::shared_ptr<const State> state_;
    friend class PreparedRelationalLiteralFiniteFieldMap;
    explicit RelationalLiteralFiniteFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
public:
    PreparedRelationalLiteralFiniteFieldMap map() const { return state_->map; }
    RelationalFiniteFieldResult family() const { return state_->family; }
};
inline RelationalLiteralFiniteFieldResult PreparedRelationalLiteralFiniteFieldMap::execute(
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    const auto pin = *this;
    return RelationalLiteralFiniteFieldResult({pin, pin.map().execute(limits, std::move(check))});
}
} // namespace rfc::learning
