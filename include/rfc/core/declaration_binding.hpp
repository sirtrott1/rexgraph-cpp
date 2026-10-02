#pragma once

#include "rfc/composite/layout.hpp"
#include "rfc/composite/execution.hpp"
#include "rfc/core/relational_complex.hpp"
#include <string>

namespace rfc::core {
namespace declaration_binding_detail {
inline bool same_signature(const RelationSignature& a, const RelationSignature& b) {
    if (a.aspects().size() != b.aspects().size() || a.type_count() != b.type_count()) return false;
    for (std::size_t i = 0; i < a.aspects().size(); ++i) {
        const auto& x = a.aspects().at(static_cast<composite::AspectId>(i));
        const auto& y = b.aspects().at(static_cast<composite::AspectId>(i));
        if (x.id != y.id || x.name != y.name || x.role != y.role || x.default_value != y.default_value) return false;
    }
    for (std::size_t i = 0; i < a.type_count(); ++i) {
        const auto& x = a.type(static_cast<RelationTypeId>(i)); const auto& y = b.type(static_cast<RelationTypeId>(i));
        if (x.id != y.id || x.name != y.name || x.boundary_policy != y.boundary_policy ||
            x.min_arity != y.min_arity || x.max_arity != y.max_arity || x.constructor != y.constructor ||
            x.constraints.shape != y.constraints.shape || x.constraints.distinguished_role != y.constraints.distinguished_role ||
            x.constraints.rules.size() != y.constraints.rules.size()) return false;
        for (std::size_t j = 0; j < x.constraints.rules.size(); ++j) {
            const auto& p = x.constraints.rules[j]; const auto& q = y.constraints.rules[j];
            if (p.kind != q.kind || p.aspect != q.aspect || p.count != q.count || p.value != q.value) return false;
        }
    }
    return true;
}
} // namespace declaration_binding_detail

enum class BranchPopulation : std::uint8_t { Retained = 0, Active = 1 };
enum class BranchSemantics : std::uint8_t { Generic = 0, NormalizedOneHead = 1 };
struct BranchRoleDeclaration {
    std::uint64_t key{};
    std::string source_aspect;
    std::size_t min_count{}, max_count{};
    bool operator==(const BranchRoleDeclaration&) const = default;
};
struct BranchDeclaration {
    composite::DeclarationId id;
    composite::DeclarationId payload;
    BranchPopulation population{BranchPopulation::Retained};
    BranchSemantics semantics{BranchSemantics::Generic};
    bool partition{true};
    std::vector<BranchRoleDeclaration> roles;
    bool operator==(const BranchDeclaration&) const = default;
};

// F1 adds a reusable validator over a fixed signature; it does not introduce new
// source rule opcodes or claim the contract is already in RFCX temporal history.
// Cardinalities count occurrences, not distinct participants or Cartesian pairs.
class BranchDeclarationBinding {
public:
    BranchDeclarationBinding(RelationSignature signature, RelationTypeId type,
                             BranchDeclaration declaration, composite::CompiledLayout payload)
        : signature_(std::move(signature)), type_(type), declaration_(std::move(declaration)), payload_(std::move(payload)) {
        using composite::declaration_detail::require;
        require(declaration_.id.valid() && declaration_.payload == payload_.root(), "branch declaration/payload identity");
        require(declaration_.population == BranchPopulation::Retained || declaration_.population == BranchPopulation::Active,
                "unknown branch population");
        const auto& t = signature_.type(type_);
        switch (declaration_.semantics) {
            case BranchSemantics::Generic:
                require(t.constructor == CompositeConstructorKind::None &&
                        (t.constraints.shape == RelationShape::Generic || t.constraints.shape == RelationShape::Branching),
                        "generic branch cannot inherit a normalized constructor"); break;
            case BranchSemantics::NormalizedOneHead:
                require(t.constructor == CompositeConstructorKind::NormalizedOneHead &&
                        t.constraints.shape == RelationShape::Branching && t.constraints.distinguished_role.has_value(),
                        "normalized branch needs its declared source constructor");
                for (const auto& needed : RelationConstraints::branching_normalized(*t.constraints.distinguished_role).rules) {
                    const auto it = std::find_if(t.constraints.rules.begin(), t.constraints.rules.end(), [&](const auto& actual) {
                        return actual.kind == needed.kind && actual.aspect == needed.aspect &&
                               actual.count == needed.count && actual.value == needed.value;
                    });
                    require(it != t.constraints.rules.end(), "normalized source is missing an enforced rule");
                }
                break;
            default: throw std::invalid_argument("unknown branch semantics");
        }
        composite::declaration_detail::budget(declaration_.roles.size(), payload_.limits().max_components, "branch role budget");
        std::set<std::uint64_t> keys; std::set<composite::AspectId> aspects;
        require(!declaration_.roles.empty(), "empty branch roles");
        for (const auto& r : declaration_.roles) {
            require(r.key && keys.insert(r.key).second && r.min_count <= r.max_count, "invalid branch role identity/bounds");
            const auto id = signature_.aspects().id(r.source_aspect);
            require(signature_.aspects().at(id).role == composite::AspectRole::RelationRole && aspects.insert(id).second,
                    "branch role must bind a distinct declared source-role aspect");
            aspects_.push_back(id);
        }
    }
    [[nodiscard]] const BranchDeclaration& declaration() const noexcept { return declaration_; }
    [[nodiscard]] const composite::CompiledLayout& payload_layout() const noexcept { return payload_; }
    [[nodiscard]] std::vector<std::size_t> validate(const RelationalComplex& source, RelationId relation,
                                                   const composite::DeclaredValue& payload,
                                                   std::size_t occurrence_budget = 1000000) const {
        using composite::declaration_detail::require;
        require(declaration_binding_detail::same_signature(source.signature(), signature_), "branch binding signature changed");
        require(relation.grade.value != 0 && relation.grade.value < source.grade_count(), "branch relation grade");
        const auto& arena = source.grade(relation.grade);
        require(relation.index < arena.cell_count() && arena.relation_types.at(relation.index) == type_, "branch relation type");
        payload_.validate(payload);
        const auto [begin, end] = arena.incidence_range(relation.index);
        composite::declaration_detail::budget(end - begin, occurrence_budget, "branch validation work budget");
        std::vector<std::size_t> counts(aspects_.size(), 0);
        for (auto p = begin; p < end; ++p) {
            if (declaration_.population == BranchPopulation::Active && !arena.aspects.get(signature_.existence_aspect(), p)) continue;
            std::size_t memberships = 0;
            for (std::size_t r = 0; r < aspects_.size(); ++r) if (arena.aspects.get(aspects_[r], p)) {
                ++counts[r]; ++memberships;
            }
            require(!declaration_.partition || memberships == 1, "occurrence does not have exactly one partition role");
        }
        for (std::size_t r = 0; r < counts.size(); ++r)
            require(counts[r] >= declaration_.roles[r].min_count && counts[r] <= declaration_.roles[r].max_count,
                    "branch role cardinality violation");
        return counts;
    }
private:
    RelationSignature signature_;
    RelationTypeId type_;
    BranchDeclaration declaration_;
    composite::CompiledLayout payload_;
    std::vector<composite::AspectId> aspects_;
};

struct SourceAspectFactorBinding {
    std::uint64_t factor{};
    std::string source_aspect;
};
// Definition-level source references only. The existing source owns its planes;
// explicit value materialization copies only one requested occurrence. Packed
// execution reads source words directly and uses BinaryProgram's domain masking.
class SourceAspectLayoutBinding {
public:
    SourceAspectLayoutBinding(RelationSignature signature, composite::CompiledLayout layout,
                              std::vector<SourceAspectFactorBinding> bindings)
        : signature_(std::move(signature)), layout_(std::move(layout)) {
        using composite::declaration_detail::require;
        const auto& root = layout_.declarations().at(layout_.root());
        require(root.kind == composite::DeclarationKind::Product && root.factors.size() == bindings.size(),
                "source aspect layout requires a complete product binding");
        require(bindings.size() < std::numeric_limits<composite::AspectId>::max(), "source aspect factor machine limit");
        std::map<std::uint64_t, std::string> by_key;
        for (auto& b : bindings) require(by_key.emplace(b.factor, std::move(b.source_aspect)).second, "duplicate source factor binding");
        for (const auto& f : root.factors) {
            require(layout_.declarations().at(f.type).kind == composite::DeclarationKind::Boolean,
                    "source aspect factor requires Boolean declaration");
            const auto it = by_key.find(f.key); require(it != by_key.end(), "missing source factor binding");
            aspects_.push_back(signature_.aspects().id(it->second));
        }
    }
    [[nodiscard]] composite::DeclaredValue materialize_occurrence(const RelationalComplex& source, Grade grade,
                                                                  std::size_t slot) const {
        check(source, grade);
        const auto& planes = source.grade(grade).aspects;
        if (slot >= planes.slots()) throw std::out_of_range("source occurrence slot");
        std::vector<composite::DeclaredValue> values; values.reserve(aspects_.size());
        for (const auto aspect : aspects_) values.push_back(composite::DeclaredValue::boolean(planes.get(aspect, slot)));
        auto value = composite::DeclaredValue::product(std::move(values));
        layout_.validate(value); return value;
    }
    [[nodiscard]] machine::PackedBits execute(const RelationalComplex& source, Grade grade,
                                              const composite::BinaryProgram& program, std::uint16_t result) const {
        check(source, grade);
        // Validate every reference even on a zero-occurrence domain.
        if (result >= program.register_count()) throw std::out_of_range("source program result register");
        for (const auto& i : program.instructions())
            if (i.opcode == composite::BinaryOpcode::LoadAspect && i.aspect >= aspects_.size())
                throw std::invalid_argument("source program factor index");
        const auto& planes = source.grade(grade).aspects;
        composite::BinaryExecutionWorkspace workspace;
        const composite::BinaryExecutionPlan plan(program, {result});
        auto output = plan.materialize(planes.slots(),
            [&](auto factor, auto word) { return planes.plane(aspects_.at(factor)).word(word); }, workspace);
        return std::move(output.front());
    }
    [[nodiscard]] std::vector<machine::PackedBits> execute(
        const RelationalComplex& source, Grade grade,
        const composite::BinaryExecutionPlan& plan, composite::BinaryExecutionWorkspace& workspace) const {
        check(source, grade);
        plan.validate_aspects(aspects_.size());
        const auto& planes = source.grade(grade).aspects;
        return plan.materialize(planes.slots(),
            [&](auto factor, auto word) { return planes.plane(aspects_.at(factor)).word(word); }, workspace);
    }
    [[nodiscard]] const composite::CompiledLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] const std::vector<composite::AspectId>& source_aspects() const & noexcept { return aspects_; }
    const std::vector<composite::AspectId>& source_aspects() const && = delete;
private:
    void check(const RelationalComplex& source, Grade grade) const {
        if (!declaration_binding_detail::same_signature(source.signature(), signature_))
            throw std::invalid_argument("source aspect binding signature changed");
        if (!grade.value || grade.value >= source.grade_count()) throw std::out_of_range("source aspect grade");
    }
    RelationSignature signature_;
    composite::CompiledLayout layout_;
    std::vector<composite::AspectId> aspects_;
};
} // namespace rfc::core
