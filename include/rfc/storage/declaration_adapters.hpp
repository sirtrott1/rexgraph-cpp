#pragma once
#include "rfc/storage/declaration_codec.hpp"
#include "rfc/storage/relational_code_types.hpp"

namespace rfc::storage {

// Explicit bridge for existing independently stored Rational/Finite/Product code
// values. Source-backed factor bindings are not discarded or turned into stored
// values: those use core::SourceAspectLayoutBinding or the existing code views.
inline composite::CompiledLayout compile_relational_code_layout(const RelationalCodeState& state,
                                                                 std::uint64_t type, std::uint64_t version,
                                                                 composite::DeclarationLimits limits = {}) {
    using composite::Declaration;
    using composite::DeclarationId;
    temporal_source_detail::require(state.namespace_key && version, "code declaration namespace/version required");
    composite::declaration_detail::budget(state.types.size(), limits.max_definitions, "code type import budget");
    // Validate type-only semantics without assigning meanings to operation/field state.
    RelationalCodeState types; types.types = state.types;
    RelationalCodeAlgebra algebra(types); (void)algebra.type(type);
    std::vector<Declaration> definitions; definitions.reserve(state.types.size());
    for (const auto& t : state.types) {
        const DeclarationId id{state.namespace_key, t.key, version};
        switch (t.kind) {
            case RelationalCodeTypeKind::Finite:
                definitions.push_back(Declaration::finite(id, t.atoms)); break;
            case RelationalCodeTypeKind::Rational:
                definitions.push_back(Declaration::rational_tensor(id, {{1, t.atoms.size(), t.atoms}})); break;
            case RelationalCodeTypeKind::Product: {
                std::vector<composite::DeclarationFactor> factors;
                for (const auto& f : t.factors) {
                    temporal_source_detail::require(f.source_aspect.empty(), "source factor provenance requires a source binding");
                    factors.push_back({f.key, {state.namespace_key, f.type, version}, f.grade});
                }
                definitions.push_back(Declaration::product(id, std::move(factors))); break;
            }
            default: throw std::invalid_argument("unsupported code layout constructor");
        }
    }
    return composite::CompiledLayout(composite::DeclarationRegistry(std::move(definitions), limits),
                                     {state.namespace_key, type, version}, composite::BooleanLayoutProfile::Lsb0OctetsV1, limits);
}
namespace declaration_adapter_detail {
inline composite::DeclaredValue import_value(const composite::CompiledLayout& layout,
                                             composite::DeclarationId id, const RelationalCodeValue& value,
                                             std::size_t& remaining) {
    if (!remaining) throw std::length_error("code value import work budget");
    --remaining;
    const auto& d = layout.declarations().at(id);
    switch (d.kind) {
        case composite::DeclarationKind::Finite: {
            const auto* p = std::get_if<std::uint64_t>(&value.data);
            temporal_source_detail::require(p, "code symbol type mismatch"); return composite::DeclaredValue::symbol(*p);
        }
        case composite::DeclarationKind::RationalTensor: {
            const auto* p = std::get_if<numeric::SparseRationalTensor>(&value.data);
            temporal_source_detail::require(p, "code tensor type mismatch"); return composite::DeclaredValue::tensor(*p);
        }
        case composite::DeclarationKind::Product: {
            const auto* p = std::get_if<RelationalCodeValue::Product>(&value.data);
            temporal_source_detail::require(p && p->size() == d.factors.size(), "code product type/count mismatch");
            composite::declaration_detail::budget(p->size(), remaining, "code product work budget");
            std::vector<composite::DeclaredValue> values; values.reserve(p->size());
            for (std::size_t i = 0; i < p->size(); ++i) values.push_back(import_value(layout, d.factors[i].type, (*p)[i], remaining));
            return composite::DeclaredValue::product(std::move(values));
        }
        default: throw std::invalid_argument("declaration has no legacy code representation");
    }
}
inline RelationalCodeValue export_value(const composite::CompiledLayout& layout,
                                        composite::DeclarationId id, const composite::DeclaredValue& value) {
    const auto& d = layout.declarations().at(id);
    switch (d.kind) {
        case composite::DeclarationKind::Finite: return {std::get<composite::DeclaredValue::Symbol>(value.data).key};
        case composite::DeclarationKind::RationalTensor: return {std::get<numeric::SparseRationalTensor>(value.data)};
        case composite::DeclarationKind::Product: {
            RelationalCodeValue::Product values; const auto& xs = std::get<composite::DeclaredValue::Product>(value.data).elements;
            for (std::size_t i = 0; i < xs.size(); ++i) values.push_back(export_value(layout, d.factors[i].type, xs[i]));
            return {std::move(values)};
        }
        default: throw std::invalid_argument("declaration has no legacy code representation");
    }
}
} // namespace declaration_adapter_detail
inline composite::DeclaredValue import_relational_code_value(const composite::CompiledLayout& layout,
                                                               const RelationalCodeValue& value) {
    auto remaining = layout.limits().max_value_nodes;
    auto out = declaration_adapter_detail::import_value(layout, layout.root(), value, remaining);
    layout.validate(out); return out;
}
inline RelationalCodeValue export_relational_code_value(const composite::CompiledLayout& layout,
                                                         const composite::DeclaredValue& value) {
    layout.validate(value); return declaration_adapter_detail::export_value(layout, layout.root(), value);
}
} // namespace rfc::storage
