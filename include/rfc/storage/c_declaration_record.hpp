#pragma once
#include "rfc/core/owned_source.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"

namespace rfc::storage {
// Closed, versioned constructors. Dependencies are earlier record positions;
// neither position nor a declaration key is an identity of a live native owner.
enum class CDeclarationKind : std::uint16_t {
    Source = 1,
    AlgebraicArchive,
    CyclicChain,
    Characters,
    Branch,
    BranchModes,
    PeriodConstruction,
    PeriodLattice,
    Symplectic,
    Surface,
    SurfacePeriods,
    SurfaceSymplectic,
    Normalization,
    EllipticFactors,
    Coordinate,
    Domain,
    Expression,
    Carrier,
    UnivariateFamily,
    JetPoint,
    JetDirection,
    JetBase,
    JetReframe,
    Jet,
    RectangularJet,
    Response,
    GreenResponse,
    DerivativeField,
    ResponseApply,
    HodgeAction,
    MapAction,
    InverseAction,
    DerivativeWord,
    WordApply,
    CrossPairing,
    SurfaceCocycle,
    SurfaceHolonomy,
    SurfaceInheritance,
    RotatingSystem,
    RotatingCoefficient,
    RefinementLevel,
    RefinementReframe,
    RefinementMap,
    RefinementCompose,
    RefinementRetraction,
    RefinementDiagram,
    RefinementField,
    RefinementEnergy,
    RefinementSplit,
    RefinementResponse,
    RefinementResponseApply,
    RefinementAccumulate,
    LimitingContract,
    NativeEndpoint,
    NativeOperatorFamily,
    NativeField,
    Krylov,
    CayleyOrbit,
    CovariantFamily,
    FamilyJet,
    FamilyMap,
    FamilyMapJet,
    FamilyAction,
    FamilyResponse,
    LayerFamily,
    LayerFamilyJet
};
struct CDeclarationLimits {
    std::size_t max_file_bytes{32u << 20}, max_records{128}, max_depth{32};
    std::size_t max_words{2000000}, max_dimension{256}, max_integer_bytes{2048};
};
struct CDeclarationMetadata {
    std::uint64_t producer_namespace{}, producer{}, revision{};
    std::uint64_t evidence_namespace{}, evidence_cutoff{};
    std::string purpose;
};
class CDeclarationPayload {
    algebraic_archive_detail::Writer writer_;
    AlgebraicArchiveLimits limits_;
    algebraic_archive_detail::Admission admission_;

  public:
    explicit CDeclarationPayload(std::size_t maximum = 32u << 20)
        : writer_(maximum), limits_(), admission_(limits_) {}
    CDeclarationPayload(const CDeclarationPayload &) = delete;
    CDeclarationPayload &operator=(const CDeclarationPayload &) = delete;
    CDeclarationPayload(CDeclarationPayload &&) = delete;
    CDeclarationPayload &operator=(CDeclarationPayload &&) = delete;
    CDeclarationPayload &u64(std::uint64_t x) {
        writer_.u64(x);
        return *this;
    }
    template <std::floating_point T> CDeclarationPayload &u64(T) = delete;
    CDeclarationPayload &flag(bool x) {
        writer_.u8(x);
        return *this;
    }
    CDeclarationPayload &rational(const numeric::Rational &x) {
        writer_.rational(x, admission_);
        return *this;
    }
    template <std::floating_point T> CDeclarationPayload &rational(T) = delete;
    CDeclarationPayload &vector(std::span<const numeric::Rational> values) {
        u64(values.size());
        for (const auto &q : values)
            rational(q);
        return *this;
    }
    CDeclarationPayload &matrix(const numeric::ExactMatrix &m) {
        u64(m.rows()).u64(m.cols()).u64(m.nonzeros());
        for (const auto &[ij, q] : m.entries())
            u64(ij.first).u64(ij.second).rational(q);
        return *this;
    }
    CDeclarationPayload &matrices(const std::vector<numeric::ExactMatrix> &ms) {
        u64(ms.size());
        for (const auto &m : ms)
            matrix(m);
        return *this;
    }
    CDeclarationPayload &vectors(const std::vector<numeric::RationalVector> &vs) {
        u64(vs.size());
        for (const auto &v : vs)
            vector(v);
        return *this;
    }
    CDeclarationPayload &indices(const std::vector<std::uint64_t> &values) {
        u64(values.size());
        for (auto i : values)
            u64(i);
        return *this;
    }
    CDeclarationPayload &blob(std::span<const std::byte> value) {
        writer_.blob(value);
        return *this;
    }
    CDeclarationPayload &string(const std::string &value) {
        return blob(std::as_bytes(std::span(value)));
    }
    std::vector<std::byte> take() {
        return writer_.take();
    }
};
struct CDeclarationRecord {
    CDeclarationKind kind;
    std::uint64_t key, revision;
    std::vector<std::size_t> dependencies;
    std::vector<std::byte> payload;
};
namespace c_declaration_detail {
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using G = numeric::ExactActionGuard;
using algebraic_archive_detail::require;
inline std::size_t add(std::size_t a, std::size_t b) {
    return machine::checked_size_add(a, b);
}
inline std::size_t mul(std::size_t a, std::size_t b) {
    return machine::checked_size_mul(a, b);
}
struct Admission {
    CDeclarationLimits limits;
    G &guard;
    std::size_t words{};
    void charge(std::size_t n) {
        words = add(words, n);
        if (words > limits.max_words)
            throw std::length_error("C declaration aggregate words");
        guard.entries(words);
        guard.step();
    }
    void dimension(std::size_t n) {
        if (n > limits.max_dimension)
            throw std::length_error("C declaration dimension");
        guard.dimension(n);
    }
};
class Reader {
    rfcx_detail::Reader reader_;
    Admission &a_;

  public:
    Reader(std::span<const std::byte> b, Admission &a) : reader_(b), a_(a) {
        if (b.size() > a.limits.max_file_bytes)
            throw std::length_error("C declaration bytes");
    }
    std::size_t remaining() const {
        return reader_.remaining();
    }
    std::uint64_t u64() {
        a_.guard.step();
        return reader_.u64();
    }
    std::size_t size() {
        return machine::checked_size(u64());
    }
    bool flag() {
        a_.guard.step();
        auto v = reader_.u8();
        require(v <= 1, "C declaration boolean");
        return v != 0;
    }
    std::size_t count(std::size_t minimum = 1) {
        auto n = size();
        require(n <= remaining() / minimum, "C declaration truncated count");
        a_.charge(n);
        return n;
    }
    std::size_t dimension() {
        auto n = size();
        a_.dimension(n);
        return n;
    }
    std::span<const std::byte> blob() {
        auto n = size();
        require(n <= remaining(), "C declaration truncated blob");
        a_.charge(n);
        return reader_.slice(n);
    }
    std::string string() {
        auto b = blob();
        return {reinterpret_cast<const char *>(b.data()), b.size()};
    }
    numeric::Integer integer() {
        auto neg = flag();
        auto n = reader_.u32();
        if (n > a_.limits.max_integer_bytes)
            throw std::length_error("C declaration integer bytes");
        require(n <= remaining(), "C declaration integer truncation");
        a_.charge(n);
        auto b = reader_.slice(n);
        require(b.empty() || b.front() != std::byte{0}, "C declaration nonminimal integer");
        require(!neg || n, "C declaration negative zero");
        numeric::Integer z = 0;
        if (n)
            mpz_import(z.get_mpz_t(), n, 1, 1, 1, 0, b.data());
        if (neg)
            z = -z;
        a_.guard.value(Q{z});
        return z;
    }
    Q rational() {
        auto n = integer();
        auto d = integer();
        require(d > 0, "C declaration denominator");
        Q q(n, d);
        q.canonicalize();
        require(q.get_num() == n && q.get_den() == d, "C declaration noncanonical rational");
        a_.guard.value(q);
        return q;
    }
    V vector() {
        auto n = count(10);
        V v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(rational());
        return v;
    }
    std::vector<V> vectors() {
        auto n = count(8);
        std::vector<V> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(vector());
        return out;
    }
    std::vector<std::uint64_t> indices() {
        auto n = count(8);
        std::vector<std::uint64_t> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(u64());
        return out;
    }
    M matrix() {
        auto r = dimension(), c = dimension(), n = count(26);
        require(n <= mul(r, c), "C declaration matrix support");
        M out(r, c);
        std::optional<std::pair<std::size_t, std::size_t>> previous;
        for (std::size_t i = 0; i < n; ++i) {
            auto row = size();
            auto col = size();
            auto q = rational();
            auto at = std::pair{row, col};
            require(row < r && col < c && q != 0 && (!previous || *previous < at),
                    "C declaration matrix order/carrier");
            out.set(row, col, q);
            previous = at;
        }
        a_.guard.matrix(out);
        return out;
    }
    std::vector<M> matrices() {
        auto n = count(24);
        std::vector<M> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(matrix());
        return out;
    }
};
inline AlgebraicArchiveLimits algebraic_limits(const Admission &a) {
    AlgebraicArchiveLimits l;
    l.max_file_bytes = a.limits.max_file_bytes;
    l.max_records = a.limits.max_words;
    l.max_rational_coordinates = a.limits.max_words;
    l.max_integer_bytes =
        std::min(a.limits.max_integer_bytes, machine::ceil_div(a.guard.limits().integer_bits, 8));
    l.field.max_integer_bits = std::min(l.field.max_integer_bits, a.guard.limits().integer_bits);
    l.max_records = std::min(l.max_records, a.guard.limits().entries);
    l.max_rational_coordinates = std::min(l.max_rational_coordinates, a.guard.limits().entries);
    l.field.max_scalar_steps = std::min(l.field.max_scalar_steps, a.guard.limits().steps - a.guard.steps());
    l.modules.max_cells = std::min(l.modules.max_cells, a.limits.max_dimension);
    l.modules.max_rank = std::min(l.modules.max_rank, a.limits.max_dimension);
    l.modules.max_retained_coordinates =
        std::min(l.modules.max_retained_coordinates, a.guard.limits().entries);
    l.modules.max_matrix_coordinates = std::min(l.modules.max_matrix_coordinates, a.guard.limits().entries);
    l.fibers.max_retained_map_coordinates =
        std::min(l.fibers.max_retained_map_coordinates, a.guard.limits().entries);
    l.metric.max_factor_steps = std::min(l.metric.max_factor_steps, a.guard.limits().steps - a.guard.steps());

    return l;
}
inline core::OwnedRelationalSource source(std::span<const std::byte> bytes, Admission &a) {
    auto l = algebraic_limits(a);
    algebraic_archive_detail::Admission use(l);
    algebraic_archive_detail::source_preflight(bytes, use);
    a.charge(l.max_rational_coordinates - use.remaining);
    rfcx_detail::Reader header(bytes, 28);
    auto grades = header.u32();
    a.dimension(grades);
    (void)header.u32();
    (void)header.u32();
    (void)header.u64();
    (void)header.u64();
    auto directory = machine::checked_size(header.u64());
    for (std::size_t k = 0; k < grades; ++k) {
        rfcx_detail::Reader d(bytes, directory + k * rfcx_detail::grade_descriptor_size);
        (void)d.u64();
        a.dimension(machine::checked_size(d.u64()));
    }
    MappedRfcx mapped(bytes);
    auto s = mapped.materialize();
    for (std::size_t k = 0; k < s.grade_count(); ++k)
        a.dimension(s.grade({static_cast<std::uint16_t>(k)}).cell_count());
    return core::OwnedRelationalSource(std::move(s));
}
} // namespace c_declaration_detail
} // namespace rfc::storage
