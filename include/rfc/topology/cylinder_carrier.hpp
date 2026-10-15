#pragma once
#include "rfc/topology/owned_cylinder.hpp"

namespace rfc::topology {
// A product flag is identified by its actual upper pair, factor side and
// original occurrence. Attaching position is checked against the whole fiber.
struct CylinderFlag {
    algebra::ProductCell upper;
    algebra::ProductBoundarySide side;
    core::OccurrenceKey occurrence;
    auto operator<=>(const CylinderFlag&) const = default;
};
struct CylinderCellImage { algebra::ProductCell from; std::uint32_t to{}; };
struct CylinderFlagImage { CylinderFlag from; std::size_t to{}; };
struct CylinderTypeInterpretation { std::uint64_t product_type{}; std::optional<core::RelationTypeId> carrier_type; };
struct CylinderCarrierDeclaration {
    std::uint64_t key{}, revision{};
    // Complete constructor interpretation, including unused signature pairs.
    // Endpoint types obey the unit law. No interpretation is inferred by value.
    std::vector<CylinderTypeInterpretation> types;
    std::vector<CylinderCellImage> cells;
    std::vector<CylinderFlagImage> occurrences;
};
namespace cylinder_carrier_detail {
using owned_cylinder_detail::Guard;
using owned_cylinder_detail::add;
using owned_cylinder_detail::mul;
using owned_cylinder_detail::grade;
using owned_cylinder_detail::require;
using CellMap = std::map<algebra::ProductCell, std::uint32_t>;
using FlagMap = std::map<CylinderFlag, std::size_t>;
inline std::size_t words(const CylinderCarrierDeclaration& d) {
    return add(2, add(mul(d.types.size(), 2), add(mul(d.cells.size(), 6), mul(d.occurrences.size(), 8))));
}
inline std::size_t population(const OwnedRelationalCylinder& cylinder) {
    auto n = std::size_t{}; const auto p = cylinder.product();
    for (std::size_t k = 0; k <= p.max_grade().value; ++k) n = add(n, p.cell_count(grade(k)));
    return n;
}
template<class F> inline void each_cell(const OwnedRelationalCylinder& cylinder, Guard& guard, F callback) {
    const auto p = cylinder.product();
    for (std::size_t k = 0; k <= p.max_grade().value; ++k) {
        guard.dimension(p.cell_count(grade(k)));
        for (std::size_t l = 0; l < cylinder.source().grade_count(); ++l) {
            if (l > k || k - l >= cylinder.interval().grade_count()) continue;
            const auto a = grade(l), b = grade(k - l);
            for (std::size_t i = 0; i < cylinder.source().grade(a).cell_count(); ++i)
                for (std::size_t j = 0; j < cylinder.interval().grade(b).cell_count(); ++j) {
                    guard.step(); callback(algebra::ProductCell{{a, static_cast<std::uint32_t>(i)}, {b, static_cast<std::uint32_t>(j)}});
                }
        }
    }
}
inline void aspects(const core::RelationSignature& a, const core::RelationSignature& b, Guard& guard) {
    require(a.aspects().size() == b.aspects().size(), "cylinder carrier requires a common complete aspect schema");
    for (std::size_t j = 0; j < a.aspects().size(); ++j) {
        guard.step(); const auto id = static_cast<composite::AspectId>(j);
        const auto& x = a.aspects().at(id); const auto& y = b.aspects().at(id);
        require(x.name == y.name && x.role == y.role && x.default_value == y.default_value,
            "cylinder carrier changes declared aspect semantics");
    }
}
struct State {
    OwnedRelationalCylinder cylinder;
    core::RelationalComplex carrier;
    CylinderCarrierDeclaration declaration;
    CellMap cells;
    FlagMap occurrences;
    std::array<algebra::RetainedSourceMap, 2> endpoints;
    std::size_t retained{}, envelope{};
};
inline void validate(const State& s, Guard& guard) {
    guard.entries(s.envelope); s.cylinder.validate(guard);
    algebra::owned_gluing_detail::source(s.carrier, guard);
    for (std::size_t n = 0; n < words(s.declaration); ++n) guard.step();
}
} // namespace cylinder_carrier_detail

// Certifies a supplied scalar-key carrier of this particular declared cylinder.
// The original ordered pair carrier and all constructor data remain owned.
// This bounded interpretation is not an automatic signature recoding service.
class OwnedCylinderCarrier {
public:
    static OwnedCylinderCarrier prepare(OwnedRelationalCylinder cylinder, core::RelationalComplex carrier,
        CylinderCarrierDeclaration input, numeric::ExactActionGuard& guard) {
        using namespace cylinder_carrier_detail; auto declaration = input;
        require(declaration.key && declaration.revision, "cylinder carrier needs its constructor identity and revision");
        const auto retained = add(cylinder.retained_words(), add(algebra::owned_gluing_detail::words(carrier), words(declaration)));
        const auto envelope = add(1, mul(add(retained, population(cylinder)), 32));
        State s{std::move(cylinder), std::move(carrier), std::move(declaration), {}, {}, {}, retained,
            envelope};
        cylinder_carrier_detail::validate(s, guard);
        const auto& x = s.cylinder.source(); const auto& c = s.carrier; const auto p = s.cylinder.product();
        require(c.grade_count() == add(p.max_grade().value, 1), "cylinder carrier changes its complete grade inventory");
        require(x.semantic_scale() == c.semantic_scale() && s.cylinder.interval().semantic_scale() == c.semantic_scale(),
            "cylinder carrier changes semantic scale");
        aspects(x.signature(), c.signature(), guard); aspects(s.cylinder.interval().signature(), c.signature(), guard);
        algebra::owned_gluing_detail::chain(c, guard, s.envelope);
        std::map<std::uint64_t, std::optional<core::RelationTypeId>> types;
        for (const auto& t : s.declaration.types) {
            guard.step(); require(t.product_type && (!t.carrier_type || *t.carrier_type < c.signature().type_count()), "invalid cylinder carrier type");
            require(types.emplace(t.product_type, t.carrier_type).second, "duplicate cylinder carrier type interpretation");
        }
        require(types.size() == p.declaration().types.size(), "cylinder carrier omits complete product types");
        for (const auto& t : p.declaration().types) {
            guard.step(); require(types.contains(t.key), "cylinder carrier omits an unused product type");
            const auto out = types.at(t.key);
            if (!t.right) require(out == t.left, "cylinder endpoint type interpretation violates the unit law");
            else {
                require(out.has_value(), "positive cylinder product type cannot become an entity");
                require(c.signature().type(*out).boundary_policy == core::BoundaryPolicy::DeclaredExact,
                    "swept cylinder type needs a declared exact boundary constructor");
            }
        }
        for (const auto& image : s.declaration.cells) {
            guard.step(); (void)p.cell(image.from);
            require(image.to < c.grade(image.from.grade()).cell_count(), "cylinder carrier cell image out of range");
            require(s.cells.emplace(image.from, image.to).second, "duplicate cylinder product cell image");
        }
        for (const auto& image : s.declaration.occurrences) {
            guard.step(); (void)p.cell(image.from.upper);
            require(image.from.upper.grade().value > 0, "grade-zero cylinder cell has no flag");
            require(image.to < c.grade(image.from.upper.grade()).incidence_count(), "cylinder carrier flag image out of range");
            require(s.occurrences.emplace(image.from, image.to).second, "duplicate cylinder product flag image");
        }
        std::vector<std::set<std::uint32_t>> used_cells(c.grade_count());
        std::vector<std::set<std::size_t>> used_flags(c.grade_count());
        std::size_t flags{};
        each_cell(s.cylinder, guard, [&](auto id) {
            require(s.cells.contains(id), "cylinder carrier omits a retained product cell");
            const auto g = id.grade(); const auto i = s.cells.at(id); const auto& arena = c.grade(g);
            require(used_cells[g.value].insert(i).second, "cylinder carrier identifies distinct ordered product cells");
            if (!g.value) return;
            require(types.at(p.cell(id).type) == std::optional{arena.relation_types[i]}, "cylinder carrier cell changes its declared product type");
            const auto routes = p.retained_boundary(id, guard); const auto [b, e] = arena.incidence_range(i);
            require(routes.size() == e - b, "cylinder carrier changes a complete occurrence fiber");
            for (std::size_t pos = 0; pos < routes.size(); ++pos) {
                guard.step(); const auto& r = routes[pos]; const CylinderFlag key{id, r.side, r.factor_occurrence};
                require(s.occurrences.contains(key), "cylinder carrier omits a retained product flag");
                const auto q = s.occurrences.at(key); ++flags;
                require(q == b + pos && used_flags[g.value].insert(q).second, "cylinder carrier changes complete attaching order or multiplicity");
                require(s.cells.at(r.lower) == arena.lower_index[q], "cylinder carrier changes a shared boundary identity");
                require(r.coefficient == algebra::owned_gluing_detail::coefficient(c, g, q, guard), "cylinder carrier changes exact product coefficient");
                for (std::size_t a = 0; a < r.factor_aspects.size(); ++a) {
                    guard.step(); const auto aspect = static_cast<composite::AspectId>(a);
                    const bool expected = r.factor_aspects[a] != (r.koszul_flip && aspect == c.signature().orientation_aspect());
                    require(expected == arena.aspects.get(aspect, q), "cylinder carrier changes retained aspect or Koszul orientation");
                }
            }
        });
        require(s.cells.size() == population(s.cylinder) && s.occurrences.size() == flags, "cylinder carrier contains foreign product data");
        for (std::size_t k = 0; k < c.grade_count(); ++k) {
            guard.step(); require(used_cells[k].size() == c.grade(grade(k)).cell_count() && used_flags[k].size() == c.grade(grade(k)).incidence_count(),
                "supplied cylinder carrier contains undeclared cells or flags");
        }
        for (std::size_t end = 0; end < 2; ++end) {
            auto& map = s.endpoints[end]; map.cells.resize(x.grade_count()); map.occurrences.resize(x.grade_count());
            for (std::size_t k = 0; k < x.grade_count(); ++k) {
                const auto g = grade(k); const auto& a = x.grade(g);
                for (std::size_t i = 0; i < a.cell_count(); ++i) {
                    guard.step(); const auto id = s.cylinder.endpoint_cell({g, static_cast<std::uint32_t>(i)}, end ? CylinderEndpoint::Finish : CylinderEndpoint::Start).cell;
                    map.cells[k].push_back(s.cells.at(id));
                }
                for (std::size_t q = 0; q < a.incidence_count(); ++q) {
                    guard.step(); const auto id = s.cylinder.endpoint_cell({g, a.upper_index[q]}, end ? CylinderEndpoint::Finish : CylinderEndpoint::Start).cell;
                    map.occurrences[k].push_back(s.occurrences.at({id, algebra::ProductBoundarySide::Left, a.occurrence_identities[q]}));
                }
            }
            algebra::owned_gluing_detail::map(x, c, map, true, guard);
        }
        return OwnedCylinderCarrier(std::make_shared<const State>(std::move(s)));
    }
    OwnedRelationalCylinder cylinder() const { return state_->cylinder; }
    const core::RelationalComplex& carrier() const & { return state_->carrier; }
    const core::RelationalComplex& carrier() const && = delete;
    const CylinderCarrierDeclaration& declaration() const & { return state_->declaration; }
    const CylinderCarrierDeclaration& declaration() const && = delete;
    const algebra::RetainedSourceMap& endpoint(CylinderEndpoint end) const & {
        owned_cylinder_detail::require(end == CylinderEndpoint::Start || end == CylinderEndpoint::Finish, "unknown carrier endpoint");
        return state_->endpoints[end == CylinderEndpoint::Finish ? 1 : 0];
    }
    const algebra::RetainedSourceMap& endpoint(CylinderEndpoint) const && = delete;
    std::uint32_t image(algebra::ProductCell id) const { return state_->cells.at(id); }
    std::size_t image(CylinderFlag id) const { return state_->occurrences.at(id); }
    std::size_t retained_words() const { return state_->retained; }
    std::size_t entry_envelope() const { return state_->envelope; }
    void validate(numeric::ExactActionGuard& guard) const { const auto pin = state_; cylinder_carrier_detail::validate(*pin, guard); }
private:
    explicit OwnedCylinderCarrier(std::shared_ptr<const cylinder_carrier_detail::State> s) : state_(std::move(s)) {}
    std::shared_ptr<const cylinder_carrier_detail::State> state_;
};
} // namespace rfc::topology
