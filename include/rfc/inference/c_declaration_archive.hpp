#pragma once
#include "rfc/field/elliptic_factor_construction.hpp"
#include "rfc/field/square_tiled_surface.hpp"
#include "rfc/inference/native_cayley_orbits.hpp"
#include "rfc/local/branch_chain.hpp"
#include "rfc/storage/c_declaration_record.hpp"
#include "rfc/temporal/refinement_diagram.hpp"
#include "rfc/temporal/rotating_coefficient.hpp"
#include "rfc/temporal/surface_inheritance.hpp"
#include "rfc/temporal/word_derivative.hpp"
#include "rfc/topology/cyclic_characters.hpp"

namespace rfc::inference {
namespace c_archive_detail {
using namespace storage::c_declaration_detail;
using Kind = storage::CDeclarationKind;
using Record = storage::CDeclarationRecord;
struct Publication {
    std::shared_ptr<const storage::NativeAccessionDirectory> directory;
};
struct NativeFamily {
    PreparedNativeOperatorFamily family;
    NativeActionAvailability available;
};
inline G native_guard(const NativeFamily &family, const NativeFieldValue &field, G &g) {
    auto pins = family.family.publications();
    for (const auto &p : field.publications())
        native_action_detail::append_pin(pins, p);
    runtime::ExecutionHostPolicy p;
    p.limits = {g.limits().entries, g.limits().entries, g.limits().entries, g.limits().steps,
                g.limits().integer_bits};
    auto limits = native_action_detail::limits(runtime::ExecutionLease(p), pins, g.limits().dimension);
    return G(limits, [&g] { g.step(); });
}
using Value = std::variant<
    core::OwnedRelationalSource, storage::AlgebraicArchive, topology::PreparedCyclicEquivariantChain,
    topology::PreparedCyclicCharacterDecomposition, local::PreparedBranchChain,
    local::PreparedBranchChain::ModeFamily, field::PreparedIntegralPeriodConstruction,
    field::PreparedIntegralPeriodLattice, field::PreparedIntegralSymplecticBasis,
    field::PreparedSquareTiledSurface, field::PreparedSquareTiledPeriods,
    field::PreparedFiniteJacobianNormalization, field::PreparedFiniteEllipticFactors,
    geometry::UnivariateCoordinate, geometry::UnivariateDomain, geometry::OwnedRationalExpression,
    geometry::UnivariateCarrier, geometry::PreparedUnivariateFamily, temporal::OwnedJetPoint,
    temporal::JetDirection, temporal::OwnedJetBase, temporal::PreparedCovariantJet,
    temporal::RectangularCovariantJet, temporal::PreparedResponseJet, temporal::OwnedDerivativeField,
    temporal::PreparedResponseJet::Result, temporal::DerivativeAction, temporal::PreparedDerivativeWord,
    temporal::PreparedDerivativeWord::Result, temporal::CrossPairingJet, temporal::OwnedSurfaceCocycle,
    temporal::PreparedSurfaceHolonomy, temporal::PreparedSurfaceInheritance,
    temporal::OwnedConsistentLinearSystem, temporal::PreparedRotatingCoefficient,
    temporal::OwnedRefinementLevel, temporal::PreparedRefinementMap, temporal::PreparedRefinementRetraction,
    temporal::PreparedRefinementDiagram, temporal::RefinementField,
    temporal::PreparedRefinementMap::EnergyReceipt, temporal::PreparedRefinementRetraction::Split,
    temporal::PreparedRefinementResponse, temporal::PreparedRefinementResponse::Result,
    temporal::PreparedRefinementDiagram::Accumulation, temporal::PreparedRefinementDiagram::LimitContract,
    Publication, NativeFamily, NativeFieldValue, PreparedNativeKrylov, PreparedNativeCayleyOrbit,
    temporal::PreparedCovariantFamily, temporal::CovariantMapFamily, temporal::PreparedLayerFamily,
    temporal::PreparedLayerJet>;
inline numeric::IntegerMatrix integer_matrix(const M &m, G &g) {
    numeric::IntegerMatrix z(m.rows(), m.cols());
    for (const auto &[ij, q] : m.entries()) {
        g.value(q);
        require(q.get_den() == 1, "C declaration integral matrix");
        z.set(ij.first, ij.second, q.get_num());
    }
    return z;
}
inline std::uint32_t u32(std::uint64_t n) {
    if (!std::in_range<std::uint32_t>(n))
        throw std::out_of_range("C declaration uint32");
    return static_cast<std::uint32_t>(n);
}
inline core::Grade grade(Reader &r) {
    auto n = r.u64();
    if (!std::in_range<std::uint16_t>(n))
        throw std::out_of_range("C declaration grade");
    return {static_cast<std::uint16_t>(n)};
}
inline storage::TemporalReadLimits temporal_limits(const Admission &a) {
    storage::TemporalReadLimits l;
    l.max_file_bytes = a.limits.max_file_bytes;
    l.max_records = a.limits.max_words;
    l.max_dimension = a.limits.max_dimension;
    l.max_integer_bytes = a.limits.max_integer_bytes;
    return l;
}
inline runtime::ExecutionHostPolicy policy(const G &g) {
    runtime::ExecutionHostPolicy p;
    p.limits = {g.limits().entries, g.limits().entries, g.limits().entries, g.limits().steps,
                g.limits().integer_bits};
    return p;
}
inline storage::NativeAccessionRequest request(storage::NativeAccessionKind kind, std::uint64_t subject) {
    storage::NativeAccessionRequest r;
    r.kind = kind;
    r.subject = subject;
    return r;
}
inline temporal::RefinementLimits refinement_limits(Reader &r) {
    temporal::RefinementLimits l;
    l.max_grades = r.size();
    l.max_cells = r.size();
    l.max_occurrences = r.size();
    l.max_terms = r.size();
    l.max_levels = r.size();
    l.max_maps = r.size();
    l.max_paths = r.size();
    l.max_depth = r.size();
    temporal::refinement_detail::limits(l);
    return l;
}
inline std::vector<std::string> strings(Reader &r) {
    auto n = r.count(8);
    std::vector<std::string> out;
    for (std::size_t i = 0; i < n; ++i)
        out.push_back(r.string());
    return out;
}
inline numeric::IntegerMatrix::SparseVector fundamental(Reader &r) {
    auto n = r.count(18);
    numeric::IntegerMatrix::SparseVector out;
    std::optional<std::size_t> previous;
    for (std::size_t i = 0; i < n; ++i) {
        auto at = r.size();
        auto q = r.rational();
        require(q != 0 && q.get_den() == 1 && (!previous || *previous < at),
                "C declaration fundamental chain");
        out.emplace(at, q.get_num());
        previous = at;
    }
    return out;
}
inline std::vector<field::PeriodCupTerm> cup(Reader &r) {
    auto n = r.count(34);
    std::vector<field::PeriodCupTerm> out;
    for (std::size_t i = 0; i < n; ++i) {
        auto face = u32(r.u64());
        auto first = r.size();
        auto second = r.size();
        auto q = r.rational();
        out.push_back({face, first, second, q});
    }
    return out;
}
class Replay {
    const Record &record_;
    const std::vector<Value> &values_;
    Reader &r_;
    Admission &a_;
    template <class T> const T &dep(std::size_t i) const {
        require(i < record_.dependencies.size(), "C declaration missing dependency");
        const auto *p = std::get_if<T>(&values_.at(record_.dependencies[i]));
        require(p != nullptr, "C declaration dependency type");
        return *p;
    }
    void arity(std::size_t n) const {
        require(record_.dependencies.size() == n, "C declaration dependency arity");
    }
    template <class T> std::vector<T> deps(std::size_t start = 0) const {
        std::vector<T> out;
        for (auto i = start; i < record_.dependencies.size(); ++i)
            out.push_back(dep<T>(i));
        return out;
    }

  public:
    Replay(const Record &d, const std::vector<Value> &v, Reader &r, Admission &a)
        : record_(d), values_(v), r_(r), a_(a) {}
    Value run() {
        using namespace temporal;
        using namespace geometry;
        auto &r = r_;
        auto &g = a_.guard;
        const auto id = RefinementRevision{record_.key, record_.revision};
        switch (record_.kind) {
        case Kind::Source: {
            arity(0);
            return source(r.blob(), a_);
        }
        case Kind::AlgebraicArchive: {
            arity(0);
            return storage::decode_algebraic_archive(r.blob(), algebraic_limits(a_));
        }
        case Kind::CyclicChain: {
            arity(1);
            auto order = r.u64();
            auto basis = r.u64();
            require(basis <= 1, "C declaration coefficient basis");
            auto n = r.count(8);
            std::vector<std::vector<std::uint64_t>> volts;
            for (std::size_t i = 0; i < n; ++i)
                volts.push_back(r.indices());
            return topology::PreparedCyclicEquivariantChain::prepare(
                dep<core::OwnedRelationalSource>(0).source(), order, std::move(volts),
                static_cast<topology::EquivariantCoefficientBasis>(basis), id.key, id.revision, g);
        }
        case Kind::Characters: {
            arity(1);
            auto l = algebraic_limits(a_).field;
            l.max_degree = r.size();
            l.max_input_coefficients = r.size();
            l.max_scalar_steps = r.size();
            l.max_matrix_coordinates = r.size();
            return topology::PreparedCyclicCharacterDecomposition::prepare(
                dep<topology::PreparedCyclicEquivariantChain>(0), l, g);
        }
        case Kind::Branch: {
            arity(0);
            auto shares = r.vector();
            auto vertices = r.indices(), keys = r.indices();
            auto scale = r.u64();
            require(std::in_range<std::uint16_t>(scale), "C declaration semantic scale");
            std::vector<core::RelationKey> relations;
            for (auto k : keys)
                relations.push_back({k});
            return local::PreparedBranchChain::prepare(shares, vertices, relations, g,
                                                       {static_cast<std::uint16_t>(scale)});
        }
        case Kind::BranchModes: {
            arity(2);
            const auto &field = dep<storage::AlgebraicArchive>(1).bundle().field();
            auto n = r.count(8);
            std::vector<numeric::AlgebraicElement> roots;
            for (std::size_t i = 0; i < n; ++i)
                roots.push_back(field.element(r.vector()));
            return dep<local::PreparedBranchChain>(0).declared_modes(field, roots, g);
        }
        case Kind::PeriodConstruction:
        case Kind::PeriodLattice: {
            arity(1);
            std::optional<numeric::IntegerMatrix> cycles;
            std::optional<M> dual;
            if (record_.kind == Kind::PeriodLattice) {
                cycles = integer_matrix(r.matrix(), g);
                dual = r.matrix();
            }
            auto metric = field::ExactMetric(r.matrix(), g);
            auto f = fundamental(r);
            auto c = cup(r);
            const auto &s = dep<core::OwnedRelationalSource>(0).source();
            if (cycles)
                return field::PreparedIntegralPeriodLattice::prepare(
                    s, std::move(*cycles), std::move(*dual), std::move(metric), std::move(f), std::move(c),
                    field::PeriodCoordinateBasis::CoordinatedIntegralLattice, id.key, id.revision, g);
            return field::PreparedIntegralPeriodConstruction::prepare(
                s, std::move(metric), std::move(f), std::move(c),
                field::PeriodCoordinateBasis::CoordinatedIntegralLattice, id.key, id.revision, g);
        }
        case Kind::Symplectic: {
            arity(1);
            auto at = record_.dependencies[0];
            if (auto *p = std::get_if<field::PreparedIntegralPeriodConstruction>(&values_[at]))
                return field::PreparedIntegralSymplecticBasis::prepare(p->lattice(), g);
            return field::PreparedIntegralSymplecticBasis::prepare(
                dep<field::PreparedIntegralPeriodLattice>(0), g);
        }
        case Kind::Surface: {
            arity(0);
            auto right = r.indices(), up = r.indices();
            std::vector<std::uint32_t> a, b;
            for (auto x : right)
                a.push_back(u32(x));
            for (auto x : up)
                b.push_back(u32(x));
            auto n = u32(r.u64());
            auto orientation = r.u64();
            require(orientation <= 1, "C declaration orientation");
            return field::PreparedSquareTiledSurface::prepare(
                std::move(a), std::move(b), n, static_cast<field::SquareTiledOrientation>(orientation),
                id.key, id.revision, g);
        }
        case Kind::SurfacePeriods: {
            arity(1);
            return dep<field::PreparedSquareTiledSurface>(0).integral_periods(
                field::ExactMetric(r.matrix(), g), g);
        }
        case Kind::SurfaceSymplectic: {
            arity(1);
            return dep<field::PreparedSquareTiledPeriods>(0).symplectic();
        }
        case Kind::Normalization: {
            arity(2);
            const auto &archive = dep<storage::AlgebraicArchive>(1);
            require(archive.contents().embedding.has_value(), "C declaration selected root missing");
            const auto *e = std::get_if<SelectedRealAlgebraicEmbedding>(&*archive.contents().embedding);
            require(e != nullptr, "C declaration real embedding required");
            const auto &field = archive.bundle().field();
            auto n = r.count(16);
            std::vector<field::DeclaredPeriodFrequency> freq;
            for (std::size_t i = 0; i < n; ++i) {
                auto mu = field.element(r.vector());
                auto root = field.element(r.vector());
                freq.push_back({std::move(mu), std::move(root)});
            }
            return field::PreparedFiniteJacobianNormalization::prepare(
                dep<field::PreparedIntegralSymplecticBasis>(0), std::move(freq), *e, g);
        }
        case Kind::EllipticFactors: {
            arity(1);
            return field::PreparedFiniteEllipticFactors::prepare(
                dep<field::PreparedFiniteJacobianNormalization>(0), g);
        }
        case Kind::Coordinate: {
            arity(0);
            auto name = r.string();
            auto revision = r.u64();
            return UnivariateCoordinate::declare(std::move(name), revision);
        }
        case Kind::Domain: {
            arity(1);
            auto lower = r.rational(), upper = r.rational();
            auto lc = r.flag(), uc = r.flag();
            return UnivariateDomain::declare(dep<UnivariateCoordinate>(0), lower, upper, lc, uc, r.vectors(),
                                             g);
        }
        case Kind::Expression: {
            arity(1);
            auto n = r.vector(), d = r.vector();
            return OwnedRationalExpression::declare(dep<UnivariateDomain>(0), std::move(n), std::move(d), g);
        }
        case Kind::Carrier: {
            arity(0);
            auto name = r.string();
            auto n = r.dimension();
            auto revision = r.u64();
            return UnivariateCarrier::declare(std::move(name), n, revision);
        }
        case Kind::UnivariateFamily: {
            require(record_.dependencies.size() >= 3, "C declaration matrix family owners");
            return PreparedUnivariateFamily::prepare(dep<UnivariateDomain>(0), dep<UnivariateCarrier>(1),
                                                     dep<UnivariateCarrier>(2),
                                                     deps<OwnedRationalExpression>(3), g);
        }
        case Kind::JetPoint: {
            arity(1);
            auto d = dep<UnivariateDomain>(0);
            auto q = r.rational();
            return OwnedJetPoint::declare(d, d.at(q, g), g);
        }
        case Kind::JetDirection: {
            if (record_.dependencies.empty())
                return JetDirection::declare(r.string());
            arity(1);
            return JetDirection::coordinate(dep<UnivariateCoordinate>(0));
        }
        case Kind::JetBase: {
            arity(2);
            auto coefficients = r.vectors();
            auto metrics = r.matrices();
            auto fields = r.vectors();
            return OwnedJetBase::declare(dep<core::OwnedRelationalSource>(0), dep<OwnedJetPoint>(1),
                                         std::move(coefficients), std::move(metrics), std::move(fields), g);
        }
        case Kind::JetReframe: {
            arity(1);
            return dep<OwnedJetBase>(0).reframe(r.matrices(), g);
        }
        case Kind::Jet: {
            arity(2);
            CovariantJetInput in;
            in.occurrence_derivatives = r.vectors();
            in.field_derivatives = r.vectors();
            in.metric_derivatives = r.matrices();
            in.connections = r.matrices();
            auto general = r.flag();
            return PreparedCovariantJet::prepare(dep<OwnedJetBase>(0), dep<JetDirection>(1), std::move(in), g,
                                                 general ? JetMetricContract::General
                                                         : JetMetricContract::Compatible);
        }
        case Kind::RectangularJet: {
            arity(2);
            auto a = r.size(), b = r.size();
            auto m = r.matrix(), d = r.matrix();
            return RectangularCovariantJet::declare(dep<PreparedCovariantJet>(0), a,
                                                    dep<PreparedCovariantJet>(1), b, std::move(m),
                                                    std::move(d), g);
        }
        case Kind::Response: {
            arity(1);
            auto k = r.matrix(), d = r.matrix();
            return PreparedResponseJet::gauged(dep<RectangularCovariantJet>(0), std::move(k), std::move(d),
                                               g);
        }
        case Kind::GreenResponse: {
            arity(1);
            auto k = r.size();
            auto lambda = r.rational(), dot = r.rational();
            return PreparedResponseJet::green(dep<PreparedCovariantJet>(0), k, lambda, dot, g);
        }
        case Kind::DerivativeField: {
            arity(1);
            auto k = r.size();
            auto value = r.vector(), dot = r.vector();
            return OwnedDerivativeField::declare(dep<PreparedCovariantJet>(0), k, std::move(value),
                                                 std::move(dot), g);
        }
        case Kind::ResponseApply: {
            arity(2);
            return dep<PreparedResponseJet>(0).apply(dep<OwnedDerivativeField>(1), g);
        }
        case Kind::HodgeAction: {
            arity(1);
            return DerivativeAction::hodge(dep<PreparedCovariantJet>(0), r.size(), g);
        }
        case Kind::MapAction: {
            arity(1);
            return DerivativeAction::map(dep<RectangularCovariantJet>(0), g);
        }
        case Kind::InverseAction: {
            arity(1);
            return dep<PreparedResponseJet>(0).inverse_action(g);
        }
        case Kind::DerivativeWord: {
            require(!record_.dependencies.empty(), "C declaration word input");
            return PreparedDerivativeWord::prepare(dep<PreparedCovariantJet>(0), r.size(),
                                                   deps<DerivativeAction>(1), g);
        }
        case Kind::WordApply: {
            arity(2);
            return dep<PreparedDerivativeWord>(0).execute(dep<OwnedDerivativeField>(1), g);
        }
        case Kind::CrossPairing: {
            arity(2);
            auto a = r.size(), b = r.size();
            auto form = r.matrix(), dot = r.matrix();
            return CrossPairingJet::declare(dep<PreparedCovariantJet>(0), a, dep<PreparedCovariantJet>(1), b,
                                            std::move(form), std::move(dot), g);
        }
        case Kind::SurfaceCocycle: {
            arity(1);
            auto values = r.vector();
            std::vector<numeric::Integer> z;
            for (const auto &q : values) {
                require(q.get_den() == 1, "C declaration integral cocycle");
                z.push_back(q.get_num());
            }
            return OwnedSurfaceCocycle::declare(dep<field::PreparedSquareTiledSurface>(0), std::move(z), g);
        }
        case Kind::SurfaceHolonomy: {
            arity(3);
            return PreparedSurfaceHolonomy::prepare(dep<field::PreparedSquareTiledSurface>(0),
                                                    dep<OwnedSurfaceCocycle>(1), dep<UnivariateDomain>(2), g);
        }
        case Kind::SurfaceInheritance: {
            arity(1);
            return PreparedSurfaceInheritance::prepare(dep<PreparedSurfaceHolonomy>(0), g);
        }
        case Kind::RotatingSystem: {
            arity(1);
            auto rhs = r.vector();
            auto metrics = r.matrices();
            return OwnedConsistentLinearSystem::declare(dep<core::OwnedRelationalSource>(0), rhs, metrics, g);
        }
        case Kind::RotatingCoefficient: {
            arity(2);
            auto row = r.size(), slot = r.size();
            const auto &s = dep<OwnedConsistentLinearSystem>(0);
            return PreparedRotatingCoefficient::prepare(s, s.coefficient(s.row(row, g), slot, g),
                                                        dep<UnivariateDomain>(1), g);
        }
        case Kind::RefinementLevel: {
            arity(1);
            auto metrics = r.matrices();
            auto l = refinement_limits(r);
            return OwnedRefinementLevel::prepare(dep<core::OwnedRelationalSource>(0), std::move(metrics), id,
                                                 g, l);
        }
        case Kind::RefinementReframe: {
            arity(1);
            return dep<OwnedRefinementLevel>(0).reframe(r.matrices(), id, g);
        }
        case Kind::RefinementMap: {
            arity(2);
            auto l = refinement_limits(r);
            auto n = r.count(8);
            RefinementMapDeclaration d{id, {}};
            for (std::size_t k = 0; k < n; ++k) {
                auto cols = r.count(8);
                geometry::LinearGradeMap m(cols);
                for (auto &col : m) {
                    auto terms = r.count(18);
                    for (std::size_t i = 0; i < terms; ++i) {
                        auto target = u32(r.u64());
                        auto q = r.rational();
                        col.push_back({target, std::move(q)});
                    }
                }
                d.grades.push_back(std::move(m));
            }
            return PreparedRefinementMap::prepare(dep<OwnedRefinementLevel>(0), dep<OwnedRefinementLevel>(1),
                                                  std::move(d), g, l);
        }
        case Kind::RefinementCompose: {
            arity(2);
            return dep<PreparedRefinementMap>(0).then(dep<PreparedRefinementMap>(1), id, g);
        }
        case Kind::RefinementRetraction: {
            arity(2);
            return PreparedRefinementRetraction::prepare(dep<PreparedRefinementMap>(0),
                                                         dep<PreparedRefinementMap>(1), g);
        }
        case Kind::RefinementDiagram: {
            auto l = refinement_limits(r);
            auto n = r.size(), m = r.size();
            require(n <= record_.dependencies.size() && m <= record_.dependencies.size() - n,
                    "C declaration diagram counts");
            std::vector<OwnedRefinementLevel> levels;
            std::vector<PreparedRefinementMap> maps;
            std::vector<PreparedRefinementRetraction> reverse;
            for (std::size_t i = 0; i < n; ++i)
                levels.push_back(dep<OwnedRefinementLevel>(i));
            for (std::size_t i = n; i < n + m; ++i)
                maps.push_back(dep<PreparedRefinementMap>(i));
            for (std::size_t i = n + m; i < record_.dependencies.size(); ++i)
                reverse.push_back(dep<PreparedRefinementRetraction>(i));
            return PreparedRefinementDiagram::prepare(std::move(levels), std::move(maps), std::move(reverse),
                                                      id, g, l);
        }
        case Kind::RefinementField: {
            arity(1);
            auto k = grade(r);
            auto v = r.vector();
            return dep<OwnedRefinementLevel>(0).field(k, v, g);
        }
        case Kind::RefinementEnergy: {
            arity(3);
            return dep<PreparedRefinementMap>(0).energy_change(dep<RefinementField>(1),
                                                               dep<RefinementField>(2), g);
        }
        case Kind::RefinementSplit: {
            arity(2);
            return dep<PreparedRefinementRetraction>(0).split(dep<RefinementField>(1), g);
        }
        case Kind::RefinementResponse: {
            arity(1);
            auto k = grade(r);
            auto lambda = r.rational();
            return PreparedRefinementResponse::prepare(dep<PreparedRefinementMap>(0), k, lambda, g);
        }
        case Kind::RefinementResponseApply: {
            arity(3);
            return dep<PreparedRefinementResponse>(0).apply(dep<RefinementField>(1), dep<RefinementField>(2),
                                                            g);
        }
        case Kind::RefinementAccumulate: {
            require(record_.dependencies.size() >= 2, "C declaration joint innovations");
            const auto &d = dep<PreparedRefinementDiagram>(0);
            auto route = d.route(r.size(), g);
            return d.accumulate(route, dep<RefinementField>(1), deps<RefinementField>(2), g);
        }
        case Kind::LimitingContract: {
            arity(1);
            auto name = r.string(), theorem = r.string();
            auto codes = r.indices();
            std::vector<RefinementProperty> checked;
            for (auto p : codes) {
                require(p <= 3, "C declaration finite premise");
                checked.push_back(static_cast<RefinementProperty>(p));
            }
            auto unresolved = strings(r), observations = strings(r);
            return dep<PreparedRefinementDiagram>(0).limit_contract(std::move(name), std::move(theorem),
                                                                    std::move(checked), std::move(unresolved),
                                                                    std::move(observations), g);
        }
        case Kind::NativeEndpoint: {
            arity(1);
            auto ns = r.u64();
            require(ns != 0, "C declaration publication namespace");
            auto bytes = r.blob();
            auto endpoint = storage::decode_temporal_endpoint(dep<core::OwnedRelationalSource>(0).source(),
                                                              bytes, temporal_limits(a_));
            return Publication{std::make_shared<const storage::NativeAccessionDirectory>(endpoint, ns)};
        }
        case Kind::NativeOperatorFamily: {
            arity(1);
            auto subject = r.u64();
            auto selection = r.u64();
            require(selection <= 2, "C declaration metric selection");
            auto carriers = r.indices(), metrics = r.indices();
            const auto &dir = *dep<Publication>(0).directory;
            auto pin = dir.pin(request(storage::NativeAccessionKind::Field, subject));
            NativeExactTowerRequest req;
            req.metrics = static_cast<NativeMetricSelection>(selection);
            req.profile = field::PreparedExactProfile::GlobalActions;
            req.grade_carriers = std::move(carriers);
            req.global_grade_dimension_limit = a_.limits.max_dimension;
            for (auto key : metrics)
                req.metric_tensors.push_back(dir.pin(request(storage::NativeAccessionKind::Tensor, key)));
            NativeActionAvailability available{{pin.source_namespace(), pin.endpoint()->sequence}};
            auto p = policy(g);
            auto tower = PreparedNativeExactTower::prepare(pin, req, p);
            return NativeFamily{PreparedNativeOperatorFamily::prepare(std::move(tower), available,
                                                                      runtime::ExecutionLease(p),
                                                                      g.limits().dimension),
                                available};
        }
        case Kind::NativeField: {
            arity(1);
            auto key = r.u64();
            return NativeFieldValue::stored(
                dep<Publication>(0).directory->pin(request(storage::NativeAccessionKind::Field, key)));
        }
        case Kind::Krylov: {
            arity(2);
            const auto &f = dep<NativeFamily>(0);
            auto kind = r.u64();
            require(kind <= static_cast<std::uint64_t>(field::OperatorKind::GradientPotential),
                    "C declaration operator kind");
            auto k = grade(r);
            auto t = r.rational();
            auto action = f.family.action({static_cast<field::OperatorKind>(kind), k, t}, f.available,
                                          runtime::ExecutionLease(policy(g)));
            auto receive = native_guard(f, dep<NativeFieldValue>(1), g);
            return PreparedNativeKrylov::prepare_with_guard(std::move(action), dep<NativeFieldValue>(1),
                                                            f.available, runtime::ExecutionLease(policy(g)),
                                                            receive);
        }
        case Kind::CayleyOrbit: {
            arity(2);
            const auto &f = dep<NativeFamily>(0);
            auto receive = native_guard(f, dep<NativeFieldValue>(1), g);
            return PreparedNativeCayleyOrbit::prepare_with_guard(f.family, dep<NativeFieldValue>(1),
                                                                 r.rational(), f.available,
                                                                 runtime::ExecutionLease(policy(g)), receive);
        }
        case Kind::CovariantFamily: {
            require(record_.dependencies.size() >= 2, "C declaration rational family");
            CovariantFamilyInput in;
            std::size_t at = 2;
            for (auto *group : {&in.occurrence_coefficients, &in.metrics, &in.connections, &in.fields}) {
                auto n = r.count(8);
                for (std::size_t k = 0; k < n; ++k) {
                    auto m = r.size();
                    require(m <= record_.dependencies.size() - at,
                            "C declaration rational family expression count");
                    std::vector<OwnedRationalExpression> row;
                    for (std::size_t j = 0; j < m; ++j)
                        row.push_back(dep<OwnedRationalExpression>(at++));
                    group->push_back(std::move(row));
                }
            }
            require(at == record_.dependencies.size(), "C declaration rational family extra inputs");
            auto general = r.flag();
            UnivariateLimits limits;
            limits.max_dimension = r.dimension();
            limits.max_degree = r.size();
            limits.max_minors = r.size();
            limits.max_bisections = r.size();
            return PreparedCovariantFamily::prepare(
                dep<core::OwnedRelationalSource>(0), dep<UnivariateDomain>(1), std::move(in), g,
                general ? JetMetricContract::General : JetMetricContract::Compatible, limits);
        }
        case Kind::FamilyJet: {
            arity(2);
            return dep<PreparedCovariantFamily>(0).jet(dep<OwnedJetPoint>(1), g);
        }
        case Kind::FamilyMap: {
            require(record_.dependencies.size() >= 2, "C family map owners");
            auto a = r.size(), b = r.size();
            return CovariantMapFamily::declare(dep<PreparedCovariantFamily>(0), a,
                                               dep<PreparedCovariantFamily>(1), b,
                                               deps<OwnedRationalExpression>(2), g);
        }
        case Kind::FamilyMapJet: {
            arity(3);
            return dep<CovariantMapFamily>(0).jet(dep<PreparedCovariantJet>(1), dep<PreparedCovariantJet>(2),
                                                  g);
        }
        case Kind::FamilyAction: {
            arity(3);
            return DerivativeAction::family(dep<CovariantMapFamily>(0), dep<PreparedCovariantJet>(1),
                                            dep<PreparedCovariantJet>(2), g);
        }
        case Kind::FamilyResponse: {
            arity(3);
            auto k = r.matrix(), d = r.matrix();
            return PreparedResponseJet::family(dep<CovariantMapFamily>(0), dep<PreparedCovariantJet>(1),
                                               dep<PreparedCovariantJet>(2), std::move(k), std::move(d), g);
        }
        case Kind::LayerFamily: {
            require(record_.dependencies.size() >= 2, "C joint layer map owners");
            auto offset_count = r.size(), free = r.size();
            require(offset_count <= record_.dependencies.size() - 2, "C joint layer offset count");
            std::vector<OwnedRationalExpression> offset, kernel;
            for (std::size_t i = 2; i < record_.dependencies.size(); ++i)
                (i - 2 < offset_count ? offset : kernel).push_back(dep<OwnedRationalExpression>(i));
            return PreparedLayerFamily::prepare(dep<CovariantMapFamily>(0), dep<CovariantMapFamily>(1),
                                                std::move(offset), free, std::move(kernel), g);
        }
        case Kind::LayerFamilyJet: {
            arity(2);
            return dep<PreparedLayerFamily>(0).jet(dep<OwnedJetPoint>(1), g);
        }
        default:
            throw std::invalid_argument("unknown C declaration constructor/version");
        }
    }
};
} // namespace c_archive_detail

class CDeclarationArchive {
    using G = numeric::ExactActionGuard;
    using Record = storage::CDeclarationRecord;
    using Value = c_archive_detail::Value;
    struct State {
        storage::CDeclarationMetadata metadata;
        std::vector<Record> records;
        std::vector<std::size_t> exports;
        std::vector<Value> values;
    };
    std::shared_ptr<const State> state_;
    explicit CDeclarationArchive(std::shared_ptr<const State> s) : state_(std::move(s)) {}

  public:
    static CDeclarationArchive prepare(storage::CDeclarationMetadata metadata, std::vector<Record> records,
                                       std::vector<std::size_t> exports, G &guard,
                                       storage::CDeclarationLimits limits = {}) {
        using namespace c_archive_detail;
        require(metadata.producer_namespace && metadata.producer && metadata.revision &&
                    !metadata.purpose.empty(),
                "C declaration original producer/purpose");
        if (records.size() > limits.max_records)
            throw std::length_error("C declaration record limit");
        guard.entries(limits.max_words);
        const auto slots = std::max<std::size_t>(records.size(), 1);
        const auto per_record = (guard.limits().entries - limits.max_words) / slots;
        Admission a{limits, guard, 0};
        a.charge(add(metadata.purpose.size(), add(records.size(), exports.size())));
        std::set<std::pair<std::uint64_t, std::uint64_t>> names;
        std::vector<std::size_t> depths;
        std::vector<Value> values;
        values.reserve(records.size());
        depths.reserve(records.size());
        for (std::size_t at = 0; at < records.size(); ++at) {
            const auto &d = records[at];
            require(d.key && d.revision && names.emplace(d.key, d.revision).second,
                    "C declaration duplicate/zero name");
            a.charge(add(d.payload.size(), d.dependencies.size()));
            std::size_t depth = 1;
            for (auto ref : d.dependencies) {
                guard.step();
                require(ref < at, "C declaration forward/cyclic/foreign dependency");
                depth = std::max(depth, add(depths[ref], 1));
            }
            if (depth > limits.max_depth)
                throw std::length_error("C declaration depth limit");
            depths.push_back(depth);
            auto child_limits = guard.limits();
            child_limits.entries = per_record;
            child_limits.dimension = std::min(child_limits.dimension, limits.max_dimension);
            child_limits.steps -= guard.steps();
            G child(child_limits, [&guard] { guard.step(); });
            Admission local{limits, child, 0};
            Reader r(d.payload, local);
            Replay replay(d, values, r, local);
            auto result = replay.run();
            require(r.remaining() == 0, "C declaration unknown/trailing fields");
            a.charge(local.words);
            values.push_back(std::move(result));
        }
        for (auto at : exports)
            require(at < records.size(), "C declaration unknown export");
        guard.step();
        return CDeclarationArchive(std::make_shared<const State>(
            State{std::move(metadata), std::move(records), std::move(exports), std::move(values)}));
    }
    const storage::CDeclarationMetadata &metadata() const & noexcept {
        return state_->metadata;
    }
    const storage::CDeclarationMetadata &metadata() const && = delete;
    const std::vector<Record> &declarations() const & noexcept {
        return state_->records;
    }
    const std::vector<Record> &declarations() const && = delete;
    const std::vector<std::size_t> &exports() const & noexcept {
        return state_->exports;
    }
    const std::vector<std::size_t> &exports() const && = delete;
    template <class T, std::integral I>
        requires(!std::same_as<I, bool>)
    const T &get(I at) const & {
        if (!std::in_range<std::size_t>(at))
            throw std::out_of_range("C declaration value index");
        const auto *p = std::get_if<T>(&state_->values.at(static_cast<std::size_t>(at)));
        if (!p)
            throw std::invalid_argument("C declaration requested value type");
        return *p;
    }
    template <class T, std::integral I> const T &get(I) const && = delete;
    template <class T, std::floating_point I> const T &get(I) const = delete;
};

inline std::vector<std::byte> encode_c_declaration_archive(const CDeclarationArchive &archive,
                                                           storage::CDeclarationLimits limits = {}) {
    using namespace storage;
    using namespace c_archive_detail;
    if (archive.declarations().size() > limits.max_records)
        throw std::length_error("C declaration encode record count");
    std::size_t words = add(archive.metadata().purpose.size(), archive.exports().size());
    std::vector<std::size_t> depths;
    for (const auto &record : archive.declarations()) {
        words = add(words, add(record.payload.size(), record.dependencies.size()));
        if (words > limits.max_words)
            throw std::length_error("C declaration encode words");
        std::size_t depth = 1;
        for (auto ref : record.dependencies)
            depth = std::max(depth, add(depths.at(ref), 1));
        if (depth > limits.max_depth)
            throw std::length_error("C declaration encode depth");
        depths.push_back(depth);
    }
    CDeclarationPayload out(limits.max_file_bytes);
    const auto &m = archive.metadata();
    out.u64(0x31444358434652ULL)
        .u64(1)
        .u64(m.producer_namespace)
        .u64(m.producer)
        .u64(m.revision)
        .u64(m.evidence_namespace)
        .u64(m.evidence_cutoff)
        .string(m.purpose)
        .u64(archive.declarations().size());
    for (const auto &d : archive.declarations()) {
        out.u64(static_cast<std::uint64_t>(d.kind)).u64(d.key).u64(d.revision).u64(d.dependencies.size());
        for (auto at : d.dependencies)
            out.u64(at);
        out.blob(d.payload);
    }
    out.u64(archive.exports().size());
    for (auto at : archive.exports())
        out.u64(at);
    auto bytes = out.take();
    CDeclarationPayload digest(8);
    digest.u64(rfcx_detail::fnv1a64(bytes));
    auto suffix = digest.take();
    if (add(bytes.size(), suffix.size()) > limits.max_file_bytes)
        throw std::length_error("C declaration encoded bytes");
    bytes.insert(bytes.end(), suffix.begin(), suffix.end());
    return bytes;
}
inline CDeclarationArchive decode_c_declaration_archive(std::span<const std::byte> bytes,
                                                        numeric::ExactActionGuard &guard,
                                                        storage::CDeclarationLimits limits = {}) {
    using namespace storage;
    using namespace c_archive_detail;
    require(bytes.size() >= 24, "C declaration truncated archive");
    if (bytes.size() > limits.max_file_bytes)
        throw std::length_error("C declaration file bytes");
    Admission a{limits, guard, 0};
    Reader digest(bytes.last(8), a);
    require(digest.u64() == rfcx_detail::fnv1a64(bytes.first(bytes.size() - 8)),
            "C declaration integrity checksum");
    Reader r(bytes.first(bytes.size() - 8), a);
    require(r.u64() == 0x31444358434652ULL && r.u64() == 1, "C declaration unknown archive version");
    CDeclarationMetadata m;
    m.producer_namespace = r.u64();
    m.producer = r.u64();
    m.revision = r.u64();
    m.evidence_namespace = r.u64();
    m.evidence_cutoff = r.u64();
    m.purpose = r.string();
    auto n = r.count(40);
    if (n > limits.max_records)
        throw std::length_error("C declaration record count");
    std::vector<CDeclarationRecord> records;
    records.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto code = r.u64();
        require(code >= 1 && code <= static_cast<std::uint64_t>(CDeclarationKind::LayerFamilyJet),
                "C declaration unknown opcode");
        auto key = r.u64(), revision = r.u64();
        auto count = r.count(8);
        std::vector<std::size_t> deps;
        deps.reserve(count);
        for (std::size_t j = 0; j < count; ++j) {
            auto at = r.size();
            require(at < i, "C declaration foreign/cyclic dependency");
            deps.push_back(at);
        }
        auto blob = r.blob();
        records.push_back({static_cast<CDeclarationKind>(code),
                           key,
                           revision,
                           std::move(deps),
                           {blob.begin(), blob.end()}});
    }
    auto count = r.count(8);
    std::vector<std::size_t> exports;
    exports.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        exports.push_back(r.size());
    require(!r.remaining(), "C declaration trailing archive fields");
    auto result =
        CDeclarationArchive::prepare(std::move(m), std::move(records), std::move(exports), guard, limits);
    require(std::ranges::equal(bytes, encode_c_declaration_archive(result, limits)),
            "C declaration noncanonical archive");
    return result;
}
} // namespace rfc::inference
