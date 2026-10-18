#pragma once

#include "rfc/inference/relational_field_interface.hpp"

namespace rfc::inference {

enum class RelationalLiteralPredicateKind : std::uint8_t {
    Equal, Prefix, Suffix, AsciiPunctuation
};

struct RelationalLiteralCoordinate {
    std::uint64_t key{};
    core::CellId cell;
    std::uint64_t direction{}, use{};
    std::size_t span_index{};
    RelationalByteSpan span;
    RelationalLiteralPredicateKind kind{RelationalLiteralPredicateKind::Equal};
    std::vector<std::byte> operand;
};

struct RelationalLiteralInputDeclaration {
    std::uint64_t key{}, revision{}, use{};
    std::vector<RelationalLiteralCoordinate> coordinates;
};

namespace literal_input_detail {
inline bool punctuation(std::byte value) noexcept {
    const auto c = std::to_integer<unsigned>(value);
    return (c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) ||
        (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e);
}

inline bool evaluate(const OwnedRelationalInputUseAsset& asset,
    const RelationalLiteralCoordinate& coordinate, numeric::ExactActionGuard& guard) {
    using namespace field_program_detail;
    guard.step(); const auto& use = asset.use(coordinate.use);
    require(coordinate.span_index < use.spans.size(), "literal input span ordinal is absent");
    const auto span = use.spans[coordinate.span_index];
    require(span == coordinate.span, "literal input original span differs");
    const auto& bytes = asset.declaration().bytes;
    const auto size = span.end - span.begin;
    const auto& operand = coordinate.operand;
    switch (coordinate.kind) {
    case RelationalLiteralPredicateKind::Equal:
        if (size != operand.size()) return false;
        break;
    case RelationalLiteralPredicateKind::Prefix:
    case RelationalLiteralPredicateKind::Suffix:
        if (operand.size() > size) return false;
        break;
    case RelationalLiteralPredicateKind::AsciiPunctuation:
        require(operand.empty(), "ASCII punctuation predicate has no operand");
        if (!size) return false;
        for (auto i = span.begin; i < span.end; ++i) {
            guard.step(); if (!punctuation(bytes[i])) return false;
        }
        return true;
    default:
        throw std::invalid_argument("unknown literal input predicate");
    }
    const auto offset = coordinate.kind == RelationalLiteralPredicateKind::Suffix
        ? span.end - operand.size() : span.begin;
    for (std::size_t i = 0; i < operand.size(); ++i) {
        guard.step(); if (bytes[offset + i] != operand[i]) return false;
    }
    return true;
}
} // namespace literal_input_detail

// Exact observations on declared byte spans. The Boolean field reconstructs
// through its existing accession; it does not claim to reconstruct the bytes.
// This owner grants no publisher authority or annotation availability.
class OwnedRelationalLiteralInput {
    struct State {
        RelationalLiteralInputDeclaration declaration;
        RelationalAccessionInput input;
    };
    std::shared_ptr<const State> state_;
public:
    OwnedRelationalLiteralInput(OwnedRelationalInputAccession accession,
        const RelationalLiteralInputDeclaration& declaration, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) {
        using namespace field_interface_detail;
        const auto pin = accession; const auto asset = pin.asset();
        Guard guard(limits, check);
        require(declaration.key && declaration.revision && declaration.use,
            "literal input identity, revision and original use required");
        (void)asset.use(declaration.use);
        const auto& stalks = pin.declaration().source_stalks;
        std::size_t extent{};
        for (const auto& stalk : stalks) { guard.step(); extent = add(extent, stalk.directions.size()); }
        guard.dimension(extent);
        require(declaration.coordinates.size() == extent,
            "literal input coordinates must cover the complete original stalk field");
        std::size_t retained = add(machine::checked_size_mul(extent, 5), 3);
        guard.entries(retained);
        for (const auto& coordinate : declaration.coordinates) {
            guard.step(); retained = add(retained, coordinate.operand.size()); guard.entries(retained);
        }
        std::set<std::uint64_t> keys;
        std::vector<numeric::SparseRationalEntry> values;
        std::size_t index{};
        for (const auto& stalk : stalks) {
            for (const auto direction : stalk.directions) {
                guard.step(); const auto& coordinate = declaration.coordinates[index];
                require(coordinate.key && keys.insert(coordinate.key).second,
                    "literal input predicate identity is zero or repeated");
                require(coordinate.cell == stalk.cell && coordinate.direction == direction,
                    "literal input changes an original cell or ordered basis direction");
                if (literal_input_detail::evaluate(asset, coordinate, guard))
                    values.push_back({index, numeric::Rational{1}});
                ++index;
            }
        }
        numeric::SparseRationalTensor field{numeric::IndexDomain::linear(extent),
            numeric::StructuralSupport::full(extent),
            numeric::SparseRationalVector(extent, std::move(values))};
        auto remaining = limits; remaining.steps -= guard.steps();
        auto input = pin.admit(declaration.use, std::move(field), remaining, std::move(check));
        state_ = std::make_shared<const State>(State{declaration, std::move(input)});
    }
    const RelationalLiteralInputDeclaration& declaration() const & { return state_->declaration; }
    const RelationalLiteralInputDeclaration& declaration() const && = delete;
    const RelationalAccessionInput& input() const & { return state_->input; }
    const RelationalAccessionInput& input() const && = delete;
    bool same_owner(const OwnedRelationalLiteralInput& other) const noexcept { return state_ == other.state_; }
};

} // namespace rfc::inference
