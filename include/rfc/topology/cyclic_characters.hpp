#pragma once
#include "rfc/topology/equivariant_torsion.hpp"
#include <algorithm>

namespace rfc::topology {

// Container/coordinate counts and outer ExactActionGuard steps, not GMP/RSS or
// a count of the scalar engine's internal operations. Scalar calls separately
// obey the retained AlgebraicFieldLimits, including max_scalar_steps per call.
struct CyclicCharacterWorkReceipt {
    std::size_t source_cells{}, source_occurrences{}, formal_terms{};
    std::size_t sectors{}, degree_sum{}, dimension_entries{};
    std::size_t retained_entries_bound{}, scalar_scratch_entries_bound{}, guard_steps{};
};

// Complete rational character decomposition of the declared free Q[C_n] chain.
// This does not construct or certify a geometric covering space. Primary cells,
// every occurrence voltage and the coefficient-basis choice stay in source().
class PreparedCyclicCharacterDecomposition {
public:
    class Sector {
    public:
        std::uint64_t order() const noexcept { return order_; }
        std::size_t weight() const noexcept { return realization_.character().field().degree(); }
        const CyclicEquivariantSpecialization& realization() const & noexcept { return realization_; }
        const CyclicEquivariantSpecialization& realization() const && = delete;
        const std::vector<std::size_t>& homology_dimensions() const & noexcept { return dimensions_; }
        const std::vector<std::size_t>& homology_dimensions() const && = delete;
    private:
        friend class PreparedCyclicCharacterDecomposition;
        Sector(std::uint64_t order, CyclicEquivariantSpecialization realization, std::vector<std::size_t> dimensions)
            : order_(order), realization_(std::move(realization)), dimensions_(std::move(dimensions)) {}
        std::uint64_t order_;
        CyclicEquivariantSpecialization realization_;
        std::vector<std::size_t> dimensions_;
    };

    static PreparedCyclicCharacterDecomposition prepare(PreparedCyclicEquivariantChain source,
        numeric::AlgebraicFieldLimits field_limits, numeric::ExactActionGuard& guard) {
        const auto start = guard.steps();
        guard.step();
        source.validate(guard);
        const auto n = static_cast<std::size_t>(source.order());
        // The full-order cyclotomic constructor requires x^n-1. Refuse before
        // enumeration/allocation, even when a smaller sector would fit.
        if(n >= field_limits.max_input_coefficients || n == std::numeric_limits<std::size_t>::max())
            throw std::length_error("cyclic census polynomial input budget");
        field_limits.max_integer_bits = std::min(field_limits.max_integer_bits, guard.limits().integer_bits);
        const auto grades = source.source().grade_count();
        CyclicCharacterWorkReceipt receipt;
        for(std::size_t k = 0; k < grades; ++k) {
            guard.step();
            const auto g = equivariant_detail::grade(k);
            const auto& arena = source.source().grade(g);
            receipt.source_cells = machine::checked_size_add(receipt.source_cells, arena.cell_count());
            receipt.source_occurrences = machine::checked_size_add(receipt.source_occurrences, arena.incidence_count());
            receipt.formal_terms = machine::checked_size_add(receipt.formal_terms, source.formal_boundary(g).size());
        }
        // Precharge the complete divisor/output carrier, not just one sector.
        guard.entries(machine::checked_size_add(n, grades));
        std::vector<std::uint64_t> divisors;
        for(std::uint64_t d = 1; d <= source.order(); ++d) {
            guard.step();
            if(source.order() % d == 0) divisors.push_back(d);
        }
        receipt.sectors = divisors.size();
        receipt.dimension_entries = machine::checked_size_mul(grades, machine::checked_size_add(divisors.size(), 1));
        // Sum phi(d)=n bounds the coordinates of all specialized terms, matrix
        // zero values, characters and minimal polynomials. Source metadata and
        // divisor/dimension records are charged too. Conservative duplicate charges
        // are intentional. Delegated action scratch uses the remaining budget.
        auto retained = machine::checked_size_add(receipt.source_cells, receipt.source_occurrences);
        retained = machine::checked_size_add(retained, receipt.formal_terms);
        retained = machine::checked_size_add(retained, machine::checked_size_mul(n,
            machine::checked_size_add(receipt.formal_terms, machine::checked_size_add(grades, 2))));
        retained = machine::checked_size_add(retained, receipt.dimension_entries);
        retained = machine::checked_size_add(retained, machine::checked_size_mul(divisors.size(), 3));
        receipt.retained_entries_bound = retained;
        // Polynomial construction/individual scalar operations use bounded
        // polynomial temporaries as well as the sparse action's own scratch.
        receipt.scalar_scratch_entries_bound = machine::checked_size_mul(machine::checked_size_add(n, 1), 8);
        const auto reserved = machine::checked_size_add(retained, receipt.scalar_scratch_entries_bound);
        guard.entries(reserved);
        auto action_limits = guard.limits();
        action_limits.entries -= reserved;
        action_limits.steps -= guard.steps();
        numeric::ExactActionGuard work(action_limits, [&guard] { guard.step(); });
        std::vector<Sector> sectors;
        sectors.reserve(divisors.size());
        std::vector<std::size_t> totals(grades, 0);
        for(const auto d : divisors) {
            work.step();
            const auto field = numeric::CertifiedAlgebraicField::declare_cyclotomic(d, field_limits);
            const auto* certificate = std::get_if<numeric::CyclotomicIrreducibility>(&field.certificate());
            if(!certificate || certificate->order != d)
                throw std::logic_error("cyclic census field order certificate differs");
            const auto character = field.generator(), one = field.scalar(numeric::Rational{1});
            equivariant_detail::algebraic_value(character, work);
            work.step();
            if(character.power(d) != one) throw std::logic_error("cyclic census character order differs");
            // Check exact order at the prime divisors, including composite d.
            auto rest = d;
            for(std::uint64_t p = 2; p <= rest / p; ++p) {
                work.step();
                if(rest % p) continue;
                if(character.power(d / p) == one) throw std::logic_error("cyclic census character is not primitive");
                do { work.step(); rest /= p; } while(rest % p == 0);
            }
            if(rest > 1) {
                work.step();
                if(character.power(d / rest) == one) throw std::logic_error("cyclic census character is not primitive");
            }
            auto realization = source.specialize(character, work);
            auto dimensions = realization.homology_dimensions(work);
            if(dimensions.size() != grades) throw std::logic_error("cyclic census omitted a source grade");
            receipt.degree_sum = machine::checked_size_add(receipt.degree_sum, field.degree());
            for(std::size_t k = 0; k < grades; ++k) {
                work.step();
                totals[k] = machine::checked_size_add(totals[k], machine::checked_size_mul(field.degree(), dimensions[k]));
                if(totals[k] > machine::checked_size_mul(n, source.source().grade(equivariant_detail::grade(k)).cell_count()))
                    throw std::logic_error("cyclic census homology exceeds its regular carrier");
            }
            sectors.push_back(Sector(d, std::move(realization), std::move(dimensions)));
        }
        work.step(); // Final cancellation checkpoint precedes publication.
        if(sectors.size() != divisors.size() || receipt.degree_sum != n)
            throw std::logic_error("cyclic census is incomplete");
        receipt.guard_steps = guard.steps() - start;
        return PreparedCyclicCharacterDecomposition(std::make_shared<const State>(
            State{std::move(source), std::move(sectors), std::move(totals), field_limits, receipt}));
    }

    const PreparedCyclicEquivariantChain& source() const & noexcept { return state_->source; }
    const PreparedCyclicEquivariantChain& source() const && = delete;
    const std::vector<Sector>& sectors() const & noexcept { return state_->sectors; }
    const std::vector<Sector>& sectors() const && = delete;
    const std::vector<std::size_t>& homology_dimensions() const & noexcept { return state_->totals; }
    const std::vector<std::size_t>& homology_dimensions() const && = delete;
    const numeric::AlgebraicFieldLimits& field_limits() const & noexcept { return state_->field_limits; }
    const numeric::AlgebraicFieldLimits& field_limits() const && = delete;
    const CyclicCharacterWorkReceipt& work_receipt() const & noexcept { return state_->receipt; }
    const CyclicCharacterWorkReceipt& work_receipt() const && = delete;
    bool same_owner(const PreparedCyclicCharacterDecomposition& other) const noexcept { return state_ == other.state_; }
    void require_source(const PreparedCyclicEquivariantChain& actual) const {
        if(!state_->source.same_owner(actual)) throw std::invalid_argument("cyclic census source owner differs");
    }
private:
    struct State {
        PreparedCyclicEquivariantChain source;
        std::vector<Sector> sectors;
        std::vector<std::size_t> totals;
        numeric::AlgebraicFieldLimits field_limits;
        CyclicCharacterWorkReceipt receipt;
    };
    explicit PreparedCyclicCharacterDecomposition(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
