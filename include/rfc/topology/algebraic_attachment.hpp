#pragma once
#include "rfc/numeric/algebraic_field.hpp"
#include "rfc/topology/cyclic_attachment.hpp"
#include <array>

namespace rfc::topology {
struct AlgebraicAttachingOccurrence {
    std::size_t source_slot;
    std::uint32_t generator;
    bool active;
    numeric::Rational coefficient;
    numeric::AlgebraicElement prefix_before,contribution;
};
struct AlgebraicAttachingFace {
    core::RelationId face;
    std::vector<AlgebraicAttachingOccurrence> occurrences;
    std::vector<numeric::AlgebraicElement> derivatives;
    numeric::AlgebraicElement holonomy,curvature;
};
struct AlgebraicAttachmentLimits {
    AttachingTransportLimits native;
    std::size_t max_algebraic_coordinates{1000000};
};

// The formal Laurent source stays pinned independently of its specialization.
// Scalar restriction to the declared power basis uses the existing native
// transport/stalk/fiber implementation, not a second field execution engine.
class PreparedAlgebraicAttachingRealization {
    struct State {
        PreparedCyclicAttachingRealization formal;
        numeric::AlgebraicElement parameter;
        PreparedAttachingTransport native;
        std::vector<AlgebraicAttachingFace> faces;
    };
public:
    PreparedAlgebraicAttachingRealization(const PreparedAlgebraicAttachingRealization&)=default;
    PreparedAlgebraicAttachingRealization& operator=(const PreparedAlgebraicAttachingRealization&)=default;
    static PreparedAlgebraicAttachingRealization common_generator(
        const PreparedCyclicAttachingRealization& formal,const numeric::AlgebraicElement& parameter,
        AttachingTransportConvention convention,AlgebraicAttachmentLimits limits={}) {
        if (parameter.is_zero()) throw std::invalid_argument("Laurent specialization requires a nonzero algebraic parameter");
        const auto field=parameter.field(); const auto d=field.degree();
        const auto generators=formal.source().grade(core::Grade{1}).cell_count();
        std::size_t coordinates=d;
        for (const auto& face:formal.faces()) {
            const auto terms=machine::checked_size_add(machine::checked_size_mul(face.occurrences.size(),2),
                machine::checked_size_add(generators,2));
            coordinates=machine::checked_size_add(coordinates,machine::checked_size_mul(terms,d));
        }
        if (coordinates>limits.max_algebraic_coordinates)
            throw std::length_error("algebraic attachment coefficient coordinate budget");
        std::vector<GeneratorTransport> maps; maps.reserve(generators);
        const auto action=parameter.regular_action();
        for (std::size_t i=0;i<generators;++i)
            maps.push_back({{core::Grade{1},static_cast<std::uint32_t>(i)},action});
        auto native=PreparedAttachingTransport::common_rational_coordinates(formal.source(),d,maps,convention,limits.native);
        std::vector<AlgebraicAttachingFace> faces; faces.reserve(formal.faces().size());
        for (std::size_t f=0;f<formal.faces().size();++f) {
            const auto& source=formal.faces()[f];
            AlgebraicAttachingFace face{source.face,{},{},field.specialize(source.holonomy,parameter),
                                                       field.specialize(source.curvature,parameter)};
            face.occurrences.reserve(source.occurrences.size()); face.derivatives.reserve(generators);
            for (const auto& term:source.occurrences)
                face.occurrences.push_back({term.source_slot,term.generator,term.active,term.coefficient,
                    field.specialize(term.prefix_before,parameter),field.specialize(term.contribution,parameter)});
            for (const auto& derivative:source.derivatives) face.derivatives.push_back(field.specialize(derivative,parameter));
            // Check the scalar restriction against the native occurrence assembly.
            const auto& native_face=native.faces()[f];
            if (face.holonomy.regular_action()!=native_face.holonomy ||
                face.curvature.regular_action()!=native_face.curvature)
                throw std::logic_error("algebraic/native holonomy mismatch");
            for (std::size_t g=0;g<generators;++g)
                if (face.derivatives[g].regular_action()!=native_face.derivatives[g])
                    throw std::logic_error("algebraic/native derivative mismatch");
            faces.push_back(std::move(face));
        }
        return PreparedAlgebraicAttachingRealization(std::make_shared<const State>(
            State{formal,parameter,std::move(native),std::move(faces)}));
    }
    const PreparedCyclicAttachingRealization& formal() const & noexcept { return state_->formal; }
    const numeric::AlgebraicElement& parameter() const & noexcept { return state_->parameter; }
    const PreparedAttachingTransport& native() const & noexcept { return state_->native; }
    const core::RelationalComplex& source() const & noexcept { return state_->native.source(); }
    const std::vector<AlgebraicAttachingFace>& faces() const & noexcept { return state_->faces; }
    const PreparedCyclicAttachingRealization& formal() const && = delete;
    const numeric::AlgebraicElement& parameter() const && = delete;
    const PreparedAttachingTransport& native() const && = delete;
    const core::RelationalComplex& source() const && = delete;
    const std::vector<AlgebraicAttachingFace>& faces() const && = delete;
    bool same_owner(const PreparedAlgebraicAttachingRealization& other) const noexcept { return state_==other.state_; }
    bool flat() const noexcept { return state_->native.flat(); }
    std::array<std::size_t,3> homology_dimensions_over_field() const {
        auto dimensions=state_->native.homology_dimensions(); const auto d=state_->parameter.field().degree();
        for (auto& value:dimensions) {
            if (value%d!=0) throw std::logic_error("algebraic scalar restriction dimension invariant");
            value/=d;
        }
        return dimensions;
    }
private:
    explicit PreparedAlgebraicAttachingRealization(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
