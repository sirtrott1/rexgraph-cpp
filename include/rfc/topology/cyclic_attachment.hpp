#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/rational_function.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/attaching_transport.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::topology {
struct CyclicAttachmentLimits {
    std::size_t max_cells{4096};
    std::size_t max_occurrences{65536};
    std::size_t max_coefficient_slots{1000000};
    std::size_t max_transport_dimension{64};
    std::size_t max_block_coordinate_slots{1000000};
};

struct CyclicAttachmentOccurrence {
    std::size_t source_slot{};
    std::uint32_t generator{};
    bool active{};
    numeric::Rational coefficient;
    numeric::RationalFunction prefix_before;
    numeric::RationalFunction contribution;
};

struct CyclicAttachmentFace {
    core::RelationId face{};
    std::vector<CyclicAttachmentOccurrence> occurrences;
    std::vector<numeric::RationalFunction> derivatives;
    numeric::RationalFunction holonomy;
    numeric::RationalFunction curvature;
};

class CyclicAttachmentSpecialization {
public:
    const core::RelationalComplex& source() const & noexcept { return *source_; }
    const core::RelationalComplex& source() const && = delete;
    const numeric::ExactMatrix& common_transport() const & noexcept { return transport_; }
    const numeric::ExactMatrix& boundary_one() const & noexcept { return boundary_one_; }
    const numeric::ExactMatrix& boundary_two() const & noexcept { return boundary_two_; }
    const numeric::ExactMatrix& curvature() const & noexcept { return curvature_; }
    const numeric::ExactMatrix& common_transport() const && = delete;
    const numeric::ExactMatrix& boundary_one() const && = delete;
    const numeric::ExactMatrix& boundary_two() const && = delete;
    const numeric::ExactMatrix& curvature() const && = delete;
    bool flat() const noexcept { return curvature_.nonzeros() == 0; }
    std::size_t first_homology_dimension() const {
        if (!flat()) throw std::domain_error("homology requires a flat attaching realization");
        return boundary_one_.cols()-boundary_one_.rank()-boundary_two_.rank();
    }
private:
    friend class PreparedCyclicAttachingRealization;
    CyclicAttachmentSpecialization(std::shared_ptr<const core::RelationalComplex> source,
        numeric::ExactMatrix transport,numeric::ExactMatrix one,numeric::ExactMatrix two,numeric::ExactMatrix curvature)
        : source_(std::move(source)),transport_(std::move(transport)),boundary_one_(std::move(one)),
          boundary_two_(std::move(two)),curvature_(std::move(curvature)) {}
    std::shared_ptr<const core::RelationalComplex> source_;
    numeric::ExactMatrix transport_, boundary_one_, boundary_two_, curvature_;
};

// Owns a complete grade-0..2 one-vertex pairwise presentation source. Every
// meridian is explicitly assigned the same formal transport z in Q[z,z^-1].
// Ordered Fox contributions are retained before any fraction-field solve.
// Nonzero holonomy-I is a declared realization defect, not a source mutation.
class PreparedCyclicAttachingRealization {
    using F = numeric::RationalFunction;
    using Q = numeric::Rational;
    using M = numeric::ExactMatrix;
    struct State {
        core::RelationalComplex source;
        CyclicAttachmentLimits limits;
        std::vector<CyclicAttachmentFace> faces;
        std::size_t coefficient_slots{};

        State(const core::RelationalComplex& original, CyclicAttachmentLimits selected)
            : source(original), limits(selected) {
            const auto generators = source.grade(core::Grade{1}).cell_count();
            for (std::size_t edge = 0; edge < generators; ++edge) {
                const auto ends = pairwise_endpoints(source,edge);
                if (ends.tail != 0 || ends.head != 0)
                    throw std::invalid_argument("cyclic attaching profile requires one-vertex pairwise loops");
            }
            // Complete projection validates every face's declared attaching type,
            // active unit signs, path connectivity, and closedness.
            (void)attaching_words(source);
            const auto& arena = source.grade(core::Grade{2});
            const auto exists = source.signature().existence_aspect();
            const auto z = F::parameter();
            const F one(Q{1}), zero;
            faces.reserve(arena.cell_count());
            for (std::size_t j = 0; j < arena.cell_count(); ++j) {
                CyclicAttachmentFace face;
                face.face = {core::Grade{2},static_cast<std::uint32_t>(j)};
                face.derivatives.resize(generators);
                const auto [begin,end] = arena.incidence_range(j);
                face.occurrences.reserve(end-begin);
                auto prefix = one;
                for (std::size_t slot = begin; slot < end; ++slot) {
                    const bool active = arena.aspects.get(exists,slot);
                    const auto coefficient = source.incidence_coefficient(core::Grade{2},slot);
                    CyclicAttachmentOccurrence term{slot,arena.lower_index[slot],active,coefficient,prefix,zero};
                    if (active) {
                        if (coefficient == 1) {
                            term.contribution = prefix;
                            prefix = prefix*z;
                        } else if (coefficient == -1) {
                            prefix = prefix/z;
                            term.contribution = zero-prefix;
                        } else {
                            throw std::invalid_argument("cyclic attaching profile requires active unit incidences");
                        }
                        face.derivatives.at(term.generator) = face.derivatives.at(term.generator)+term.contribution;
                    }
                    account(term.prefix_before); account(term.contribution);
                    face.occurrences.push_back(std::move(term));
                }
                face.holonomy = prefix;
                face.curvature = prefix-one;
                F assembled;
                for (const auto& derivative : face.derivatives) {
                    account(derivative);
                    assembled = assembled+(z-one)*derivative;
                }
                if (assembled != face.curvature)
                    throw std::logic_error("ordered Fox boundary identity failed");
                account(face.holonomy); account(face.curvature);
                faces.push_back(std::move(face));
            }
        }
        void account(const F& value) {
            coefficient_slots = machine::checked_size_add(coefficient_slots,value.coefficient_count());
            if (coefficient_slots > limits.max_coefficient_slots)
                throw std::length_error("cyclic attaching coefficient slot budget");
        }
    };
public:
    explicit PreparedCyclicAttachingRealization(const core::RelationalComplex& source,
                                               CyclicAttachmentLimits limits = {}) {
        if (source.grade_count() != 3 || source.grade(core::Grade{0}).cell_count() != 1 ||
            source.grade(core::Grade{1}).cell_count() == 0)
            throw std::invalid_argument("cyclic attaching profile requires a complete one-vertex grade-0..2 source");
        std::size_t cells{}, occurrences{};
        for (std::uint16_t k = 0; k < 3; ++k) {
            cells = machine::checked_size_add(cells,source.grade(core::Grade{k}).cell_count());
            occurrences = machine::checked_size_add(occurrences,source.grade(core::Grade{k}).incidence_count());
        }
        if (cells > limits.max_cells || occurrences > limits.max_occurrences)
            throw std::length_error("cyclic attaching source budget");
        state_ = std::make_shared<const State>(source,limits);
    }
    const core::RelationalComplex& source() const & noexcept { return state_->source; }
    const core::RelationalComplex& source() const && = delete;
    const std::vector<CyclicAttachmentFace>& faces() const & noexcept { return state_->faces; }
    const std::vector<CyclicAttachmentFace>& faces() const && = delete;
    std::size_t coefficient_slots() const noexcept { return state_->coefficient_slots; }
    bool same_owner(const PreparedCyclicAttachingRealization& other) const noexcept { return state_ == other.state_; }
    bool formal_flat() const {
        for (const auto& face : state_->faces) if (!face.curvature.is_zero()) return false;
        return true;
    }

    // All source loops and face fibers use the caller-declared common rational
    // coordinate space and one invertible endomorphism T. No metric is inferred.
    CyclicAttachmentSpecialization specialize_common_transport(const M& transport) const {
        const auto d = transport.rows();
        const auto generators = state_->source.grade(core::Grade{1}).cell_count();
        const auto face_count = state_->faces.size();
        if (d == 0 || transport.cols() != d)
            throw std::invalid_argument("cyclic attaching transport requires a nonempty square endomorphism");
        if (d > state_->limits.max_transport_dimension)
            throw std::length_error("cyclic attaching transport dimension budget");
        const auto edge_slots = machine::checked_size_mul(generators,d);
        const auto face_slots = machine::checked_size_mul(face_count,d);
        auto slots = machine::checked_size_mul(d,d);
        slots = machine::checked_size_add(slots,machine::checked_size_mul(d,edge_slots));
        slots = machine::checked_size_add(slots,machine::checked_size_mul(edge_slots,face_slots));
        slots = machine::checked_size_add(slots,machine::checked_size_mul(d,face_slots));
        if (slots > state_->limits.max_block_coordinate_slots)
            throw std::length_error("cyclic attaching block coordinate budget");
        const auto backward = transport.inverse();
        const auto identity = M::identity(d);
        const auto difference = transport.add(identity.scale(Q{-1}));
        M boundary_one(d,edge_slots), boundary_two(edge_slots,face_slots), curvature(d,face_slots);
        auto add_block = [](M& target,std::size_t row,std::size_t col,const M& block) {
            for (std::size_t i = 0; i < block.rows(); ++i)
                for (std::size_t j = 0; j < block.cols(); ++j)
                    target.set(row+i,col+j,Q(target.get(row+i,col+j)+block.get(i,j)));
        };
        for (std::size_t g = 0; g < generators; ++g) add_block(boundary_one,0,g*d,difference);
        for (std::size_t f = 0; f < face_count; ++f) {
            const auto prefix = detail::chronological_attaching_walk(state_->source,f,d,
                [&](std::uint32_t,bool forward) -> const M& { return forward ? transport : backward; },
                [&](std::size_t,std::uint32_t generator,bool active,const Q&,
                    const M&,const M& contribution,const M&) {
                    if (!active) return;
                    add_block(boundary_two,static_cast<std::size_t>(generator)*d,f*d,contribution);
                });
            add_block(curvature,0,f*d,prefix.add(identity.scale(Q{-1})));
        }
        if (boundary_one.multiply(boundary_two) != curvature)
            throw std::logic_error("specialized Fox boundary identity failed");
        auto pin = std::shared_ptr<const core::RelationalComplex>(state_,&state_->source);
        return CyclicAttachmentSpecialization(std::move(pin),transport,std::move(boundary_one),
                                             std::move(boundary_two),std::move(curvature));
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
