#pragma once

#include "rfc/storage/temporal_declarations.hpp"
#include "rfc/storage/relational_code_types.hpp"

namespace rfc::storage {
// Bind code interpretation to the owning F1 declaration DAG without duplicating
// values or source bitplanes. Code operations still use their existing algebra.
inline void validate_declared_code_bindings(const std::optional<RelationalCodeState>& codes,
                                             const TemporalDeclarationState& state) {
    using temporal_source_detail::require;
    if (state.code_types.empty()) return;
    require(codes.has_value(), "declared code binding has no code layer");
    RelationalCodeAlgebra algebra(*codes);
    std::set<std::pair<std::uint64_t,composite::DeclarationId>> visited;
    std::function<void(std::uint64_t,composite::DeclarationId)> check = [&](auto key, auto id) {
        if (!visited.emplace(key,id).second) return;
        const auto& c = algebra.type(key); const auto& d = state.registry.at(id);
        switch(c.kind) {
        case RelationalCodeTypeKind::Finite:
            require((d.kind == composite::DeclarationKind::Finite && c.atoms == d.symbols) ||
                    (d.kind == composite::DeclarationKind::Boolean && relational_code_detail::boolean(c)),
                    "code symbol order/domain differs from declaration"); break;
        case RelationalCodeTypeKind::Rational:
            require(d.kind == composite::DeclarationKind::RationalTensor && d.axes.size() == 1 &&
                    d.axes[0].extent == c.atoms.size() && d.axes[0].coordinates == c.atoms,
                    "code coordinates differ from declaration"); break;
        case RelationalCodeTypeKind::Product:
            require(d.kind == composite::DeclarationKind::Product && d.factors.size() == c.factors.size(),
                    "code factor population differs from declaration");
            for (std::size_t i=0;i<c.factors.size();++i) {
                require(c.factors[i].key == d.factors[i].key && c.factors[i].grade == d.factors[i].grade,
                        "code factor identity/order/grade differs from declaration");
                check(c.factors[i].type,d.factors[i].type);
            }
            break;
        default: throw std::invalid_argument("unknown code declaration constructor");
        }
    };
    for (const auto& binding : state.code_types) check(binding.key,binding.type);
}

// Exact reachable definitions only: unrelated payload or schema branches must
// not invalidate a code observation. Full endpoint persistence still owns all.
inline std::vector<std::byte> bound_code_declaration_bytes(const TemporalDeclarationState& state,
                                                          std::uint64_t code_type) {
    const auto it=std::find_if(state.code_types.begin(),state.code_types.end(),[&](const auto& b){return b.key==code_type;});
    if(it==state.code_types.end())return {};
    std::set<composite::DeclarationId> visited;
    std::vector<composite::Declaration> definitions;
    std::function<void(composite::DeclarationId)> visit=[&](auto id){
        if(!visited.insert(id).second)return;
        const auto& d=state.registry.at(id);definitions.push_back(d);
        for(const auto& f:d.factors)visit(f.type);
        if(d.element.valid())visit(d.element);
    };
    visit(it->type);
    const composite::CompiledLayout layout(composite::DeclarationRegistry(std::move(definitions),state.resource_limits),it->type,state.profile,state.resource_limits);
    rfcx_detail::Writer w;w.u64(state.namespace_key);w.raw(encode_declarations(layout));return w.bytes();
}
} // namespace rfc::storage
