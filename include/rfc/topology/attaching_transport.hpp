#pragma once
#include "rfc/local/fiber_system.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/local_system_transport.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::topology {

// Steps are chronological. On column vectors the next step acts on the left:
// P_after = A_step * P_before. This is a right action of concatenated words.
enum class AttachingTransportConvention : std::uint8_t { ChronologicalColumns };

struct AttachingTransportLimits {
    std::size_t max_cells{4096};
    std::size_t max_occurrences{65536};
    std::size_t max_transport_dimension{64};
    std::size_t max_retained_coordinate_slots{1000000};
    std::size_t max_integer_bits{262144};
};

struct GeneratorTransport {
    core::RelationId generator{};
    numeric::ExactMatrix forward;
};

struct TransportedAttachingOccurrence {
    std::size_t source_slot{};
    core::RelationId generator{};
    bool active{};
    numeric::Rational coefficient;
    numeric::ExactMatrix prefix_before;
    numeric::ExactMatrix contribution;
    // The full two-step route contribution, before summing other occurrences.
    numeric::ExactMatrix boundary_contribution;
};

struct TransportedAttachingFace {
    core::RelationId face{};
    std::vector<TransportedAttachingOccurrence> occurrences;
    std::vector<numeric::ExactMatrix> derivatives;
    numeric::ExactMatrix holonomy;
    numeric::ExactMatrix curvature;
};

namespace detail {
// Shared ordered matrix walk for cyclic and noncommuting realizations. The
// caller validates the complete attaching source and supplies explicit maps.
template<class Map, class Visit>
inline numeric::ExactMatrix chronological_attaching_walk(const core::RelationalComplex& source,
    std::size_t face, std::size_t dimension, Map step_map, Visit visit) {
    using M = numeric::ExactMatrix;
    using Q = numeric::Rational;
    const auto& arena = source.grade(core::Grade{2});
    const auto [begin,end] = arena.incidence_range(face);
    auto prefix = M::identity(dimension);
    for (std::size_t slot = begin; slot < end; ++slot) {
        const auto generator = arena.lower_index[slot];
        const bool active = arena.aspects.get(source.signature().existence_aspect(),slot);
        const auto coefficient = source.incidence_coefficient(core::Grade{2},slot);
        if (!active) {
            const M zero(dimension,dimension);
            visit(slot,generator,false,coefficient,prefix,zero,prefix);
            continue;
        }
        if (coefficient != 1 && coefficient != -1)
            throw std::invalid_argument("attaching transport requires active unit face incidences");
        const auto before = std::move(prefix);
        prefix = step_map(generator,coefficient == 1).multiply(before);
        if (coefficient == 1) visit(slot,generator,true,coefficient,before,before,prefix);
        else {
            const auto contribution = prefix.scale(Q{-1});
            visit(slot,generator,true,coefficient,before,contribution,prefix);
        }
    }
    return prefix;
}
} // namespace detail

// A named declaration identifies every cell stalk with the same standard Q^d
// coordinates. It freezes a complete one-vertex pairwise presentation and one
// invertible step map per primary generator. Equal sizes do not create this
// identification implicitly. Field overrides are already realized maps.
class PreparedAttachingTransport {
    using M = numeric::ExactMatrix;
    using Q = numeric::Rational;
    struct State {
        core::RelationalComplex source;
        AttachingTransportLimits limits;
        local::LinearRelationalFiberSystem fibers;
        std::vector<GeneratorTransport> generators;
        std::vector<M> backwards;
        std::vector<TransportedAttachingFace> faces;
        M one, two, defect;

        State(const core::RelationalComplex& original,std::size_t dimension,
              const std::vector<GeneratorTransport>& maps,AttachingTransportLimits selected)
            : source(original),limits(selected),fibers(local::LinearStalkBundle(source,dimension)) {
            const auto count = source.grade(core::Grade{1}).cell_count();
            generators.resize(count); backwards.resize(count);
            std::vector<bool> assigned(count,false);
            for (const auto& map : maps) {
                if (map.generator.grade != core::Grade{1} || map.generator.index >= count)
                    throw std::invalid_argument("transport does not name a retained grade-one generator");
                const auto index = map.generator.index;
                if (assigned[index]) throw std::invalid_argument("duplicate generator transport");
                if (map.forward.rows() != dimension || map.forward.cols() != dimension)
                    throw std::invalid_argument("transport does not act on the declared common carrier");
                admit(map.forward);
                generators[index] = map;
                backwards[index] = map.forward.inverse(); admit(backwards[index]);
                assigned[index] = true;
            }
            const auto identity = M::identity(dimension);
            for (std::size_t g = 0; g < count; ++g) {
                if (!assigned[g]) throw std::invalid_argument("missing generator transport");
                const auto ends = pairwise_endpoints(source,g);
                if (ends.tail != 0 || ends.head != 0)
                    throw std::invalid_argument("attaching transport requires one-vertex pairwise loops");
                const auto [tail,head] = pairwise_incidence_slots(source,g);
                const auto [begin,end] = source.grade(core::Grade{1}).incidence_range(g);
                for (std::size_t slot = begin; slot < end; ++slot) {
                    M realized(dimension,dimension);
                    if (slot == tail) realized = identity.scale(Q{-1});
                    if (slot == head) realized = generators[g].forward;
                    fibers.set_transport(core::Grade{1},slot,std::move(realized));
                }
            }
            (void)attaching_words(source); // Every face, not a selected chart.
            const auto face_count = source.grade(core::Grade{2}).cell_count();
            faces.reserve(face_count);
            for (std::size_t f = 0; f < face_count; ++f) {
                TransportedAttachingFace face;
                face.face = {core::Grade{2},static_cast<std::uint32_t>(f)};
                face.derivatives.assign(count,M(dimension,dimension));
                const auto [begin,end] = source.grade(core::Grade{2}).incidence_range(f);
                face.occurrences.reserve(end-begin);
                face.holonomy = detail::chronological_attaching_walk(source,f,dimension,
                    [&](std::uint32_t g,bool forward) -> const M& {
                        return forward ? generators.at(g).forward : backwards.at(g);
                    },
                    [&](std::size_t slot,std::uint32_t g,bool active,const Q& coefficient,
                        const M& before,const M& contribution,const M& after) {
                        admit(after); admit(contribution);
                        const auto route = active ? after.add(before.scale(Q{-1})) : M(dimension,dimension);
                        admit(route);
                        if (active) {
                            face.derivatives[g] = face.derivatives[g].add(contribution);
                            admit(face.derivatives[g]);
                        }
                        face.occurrences.push_back({slot,{core::Grade{1},g},active,coefficient,
                                                    before,contribution,route});
                        fibers.set_transport(core::Grade{2},slot,contribution);
                    });
                face.curvature = face.holonomy.add(identity.scale(Q{-1})); admit(face.curvature);
                faces.push_back(std::move(face));
            }
            one = assemble(core::Grade{1}); two = assemble(core::Grade{2});
            defect = one.multiply(two); admit(defect);
            for (std::size_t f = 0; f < faces.size(); ++f)
                for (std::size_t i = 0; i < dimension; ++i)
                    for (std::size_t j = 0; j < dimension; ++j)
                        if (defect.get(i,f*dimension+j) != faces[f].curvature.get(i,j))
                            throw std::logic_error("noncommuting attaching boundary identity failed");
        }
        void admit(const M& matrix) const {
            for (const auto& [key,value] : matrix.entries()) {
                (void)key;
                if (mpz_sgn(mpq_denref(value.get_mpq_t())) == 0)
                    throw std::invalid_argument("invalid rational transport coefficient");
                if (mpz_sizeinbase(mpq_numref(value.get_mpq_t()),2) > limits.max_integer_bits ||
                    mpz_sizeinbase(mpq_denref(value.get_mpq_t()),2) > limits.max_integer_bits)
                    throw std::length_error("attaching transport integer bit budget");
            }
        }
        M assemble(core::Grade grade) const {
            const auto lower = fibers.stalks().offsets(core::Grade{static_cast<std::uint16_t>(grade.value-1u)});
            const auto upper = fibers.stalks().offsets(grade);
            M result(lower.back(),upper.back());
            for (std::size_t cell = 0; cell < source.grade(grade).cell_count(); ++cell) {
                const auto tensor = fibers.relation_tensor(grade,cell);
                for (const auto& block : tensor.blocks())
                    for (const auto& [key,value] : block.map.entries()) {
                        const auto row = lower[block.target.index]+key.first;
                        const auto col = upper[cell]+key.second;
                        result.set(row,col,Q(result.get(row,col)+value));
                    }
            }
            admit(result); return result;
        }
    };
public:
    PreparedAttachingTransport(const PreparedAttachingTransport&) = default;
    PreparedAttachingTransport& operator=(const PreparedAttachingTransport&) = default;

    static PreparedAttachingTransport common_rational_coordinates(const core::RelationalComplex& source,
        std::size_t dimension,const std::vector<GeneratorTransport>& maps,
        AttachingTransportConvention convention,AttachingTransportLimits limits = {}) {
        if (convention != AttachingTransportConvention::ChronologicalColumns)
            throw std::invalid_argument("unsupported attaching transport composition convention");
        if (source.grade_count() != 3 || source.grade(core::Grade{0}).cell_count() != 1 ||
            source.grade(core::Grade{1}).cell_count() == 0)
            throw std::invalid_argument("attaching transport requires a complete one-vertex grade-0..2 source");
        if (dimension == 0) throw std::invalid_argument("attaching transport carrier is empty");
        if (dimension > limits.max_transport_dimension) throw std::length_error("attaching transport dimension budget");
        if (limits.max_integer_bits == 0) throw std::invalid_argument("attaching transport integer bit budget is empty");
        std::size_t cells{},occurrences{};
        for (std::uint16_t k = 0; k < 3; ++k) {
            cells = machine::checked_size_add(cells,source.grade(core::Grade{k}).cell_count());
            occurrences = machine::checked_size_add(occurrences,source.grade(core::Grade{k}).incidence_count());
        }
        if (cells > limits.max_cells || occurrences > limits.max_occurrences)
            throw std::length_error("attaching transport source budget");
        const auto g = source.grade(core::Grade{1}).cell_count();
        const auto f = source.grade(core::Grade{2}).cell_count();
        if (maps.size() != g) throw std::invalid_argument("every retained generator requires one declared transport");
        // Logical retained coordinates include maps, inverses, occurrence traces,
        // derivative blocks, fiber overrides and complete assembled boundaries.
        const auto gf = machine::checked_size_mul(g,f);
        auto blocks = machine::checked_size_add(machine::checked_size_mul(3,g),machine::checked_size_mul(3,f));
        blocks = machine::checked_size_add(blocks,machine::checked_size_mul(2,gf));
        blocks = machine::checked_size_add(blocks,occurrences);
        blocks = machine::checked_size_add(blocks,machine::checked_size_mul(3,source.grade(core::Grade{2}).incidence_count()));
        const auto slots = machine::checked_size_mul(blocks,machine::checked_size_mul(dimension,dimension));
        if (slots > limits.max_retained_coordinate_slots)
            throw std::length_error("attaching transport retained coordinate budget");
        return PreparedAttachingTransport(std::make_shared<const State>(source,dimension,maps,limits));
    }
    template<std::floating_point Dimension>
    static PreparedAttachingTransport common_rational_coordinates(const core::RelationalComplex&,
        Dimension,const std::vector<GeneratorTransport>&,AttachingTransportConvention,
        AttachingTransportLimits = {}) = delete;
    const core::RelationalComplex& source() const & noexcept { return state_->source; }
    const local::LinearRelationalFiberSystem& fibers() const & noexcept { return state_->fibers; }
    const std::vector<GeneratorTransport>& generators() const & noexcept { return state_->generators; }
    const std::vector<TransportedAttachingFace>& faces() const & noexcept { return state_->faces; }
    const M& boundary_one() const & noexcept { return state_->one; }
    const M& boundary_two() const & noexcept { return state_->two; }
    const M& curvature() const & noexcept { return state_->defect; }
    const core::RelationalComplex& source() const && = delete;
    const local::LinearRelationalFiberSystem& fibers() const && = delete;
    const std::vector<GeneratorTransport>& generators() const && = delete;
    const std::vector<TransportedAttachingFace>& faces() const && = delete;
    const M& boundary_one() const && = delete;
    const M& boundary_two() const && = delete;
    const M& curvature() const && = delete;
    bool same_owner(const PreparedAttachingTransport& other) const noexcept { return state_ == other.state_; }
    bool flat() const noexcept { return state_->defect.nonzeros() == 0; }
    std::array<std::size_t,3> homology_dimensions() const {
        if (!flat()) throw std::domain_error("homology requires a flat attaching transport realization");
        const auto r1 = state_->one.rank(), r2 = state_->two.rank();
        return {state_->one.rows()-r1,state_->one.cols()-r1-r2,state_->two.cols()-r2};
    }
    M path_transport(const PairwisePath& path) const {
        if (&path.complex() != &state_->source)
            throw std::invalid_argument("transport path belongs to a different source owner");
        if (path.steps().size() > state_->limits.max_occurrences)
            throw std::length_error("attaching path step budget");
        auto result = M::identity(state_->one.rows());
        for (const auto& step : path.steps()) {
            const auto& map = step.forward ? state_->generators.at(step.relation).forward : state_->backwards.at(step.relation);
            result = map.multiply(result); state_->admit(result);
        }
        return result;
    }
private:
    explicit PreparedAttachingTransport(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
