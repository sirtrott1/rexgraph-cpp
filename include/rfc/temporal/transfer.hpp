#pragma once
#include "rfc/field/exact_metric.hpp"
#include "rfc/field/low_rank_update.hpp"
#include "rfc/temporal/change.hpp"
#include "rfc/temporal/operation_word.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::temporal {

struct FieldTransferDelta {
    numeric::RationalVector transported_old;
    numeric::RationalVector innovation;
    numeric::RationalVector delta;
};

inline FieldTransferDelta field_delta(const numeric::ExactMatrix& correspondence,
                                      const numeric::RationalVector& old_field,
                                      const numeric::RationalVector& new_field) {
    const auto transported = correspondence.apply(old_field);
    if (transported.size() != new_field.size()) throw std::invalid_argument("field transfer size mismatch");
    return {transported, numeric::sub(new_field, transported), numeric::sub(new_field, transported)};
}

struct SourceInjectionDelta {
    numeric::ExactMatrix realization_defect;
    numeric::RationalVector coordinate_innovation;
    numeric::RationalVector representation_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector aggregate;
};

inline SourceInjectionDelta source_injection_delta(const numeric::ExactMatrix& old_injection,
                                                   const numeric::ExactMatrix& new_injection,
                                                   const numeric::ExactMatrix& ambient_correspondence,
                                                   const numeric::ExactMatrix& source_correspondence,
                                                   const numeric::RationalVector& old_coordinates,
                                                   const numeric::RationalVector& new_coordinates) {
    if (old_injection.cols() != old_coordinates.size() || new_injection.cols() != new_coordinates.size()) throw std::invalid_argument("injection coordinate size mismatch");
    const auto EI = new_injection.multiply(source_correspondence)
        .add(ambient_correspondence.multiply(old_injection).scale(numeric::Rational{-1}));
    const auto zeta = numeric::sub(new_coordinates, source_correspondence.apply(old_coordinates));
    const auto representation = EI.apply(old_coordinates);
    const auto innovation = new_injection.apply(zeta);
    return {EI, zeta, representation, innovation, numeric::add(representation, innovation)};
}

struct ObservationDelta {
    numeric::ExactMatrix accession_defect;
    numeric::RationalVector accession_term;
    numeric::RationalVector field_term;
    numeric::RationalVector aggregate;
};

inline ObservationDelta observation_delta(const numeric::ExactMatrix& old_accession,
                                          const numeric::ExactMatrix& new_accession,
                                          const numeric::ExactMatrix& ambient_correspondence,
                                          const numeric::ExactMatrix& output_correspondence,
                                          const numeric::RationalVector& old_field,
                                          const numeric::RationalVector& new_field) {
    const auto EA = new_accession.multiply(ambient_correspondence)
        .add(output_correspondence.multiply(old_accession).scale(numeric::Rational{-1}));
    const auto eta = numeric::sub(new_field, ambient_correspondence.apply(old_field));
    const auto accession_term = EA.apply(old_field);
    const auto field_term = new_accession.apply(eta);
    return {EA, accession_term, field_term, numeric::add(accession_term, field_term)};
}

inline numeric::ExactMatrix metric_pullback_defect(const field::ExactMetric& old_metric,
                                                   const field::ExactMetric& new_metric,
                                                   const numeric::ExactMatrix& correspondence) {
    if (correspondence.cols() != old_metric.size() || correspondence.rows() != new_metric.size()) throw std::invalid_argument("metric correspondence shape mismatch");
    return correspondence.transpose().multiply(new_metric.materialize()).multiply(correspondence)
        .add(old_metric.materialize().scale(numeric::Rational{-1}));
}

inline numeric::RationalVector shifted_solve(const numeric::ExactMatrix& L,
                                             const numeric::Rational& lambda,
                                             const numeric::RationalVector& rhs) {
    if (L.rows() != L.cols() || rhs.size() != L.rows()) throw std::invalid_argument("shifted solve dimensions");
    numeric::ExactActionGuard guard;
    return numeric::PreparedExactSolve(field::update_detail::shifted(L,lambda,guard),guard).apply(rhs,guard);
}

struct GreenResponseDelta {
    numeric::RationalVector old_response;
    numeric::RationalVector new_response;
    numeric::ExactMatrix injection_defect;
    numeric::ExactMatrix operator_defect;
    numeric::RationalVector coordinate_innovation;
    numeric::RationalVector realization_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector operator_term;
    numeric::RationalVector aggregate;
};

// Finite declared numeric maps, including every new ambient/source direction.
// These maps do not infer native carrier identities or temporal cutoffs.
class PreparedGreenResponseDelta {
    using Matrix=numeric::ExactMatrix;
    using Vector=numeric::RationalVector;
    using Guard=numeric::ExactActionGuard;
    struct State {
        Matrix old_L,new_L,J,old_I,new_I,K,EI,EL;
        numeric::Rational lambda;
        std::optional<numeric::PreparedExactSolve> old_solve,new_solve;
        std::size_t retained{},preparation{};
        State(Matrix old_operator,Matrix new_operator,Matrix ambient,Matrix old_injection,
              Matrix new_injection,Matrix source,numeric::Rational shift,Guard& guard)
            :old_L(std::move(old_operator)),new_L(std::move(new_operator)),J(std::move(ambient)),
             old_I(std::move(old_injection)),new_I(std::move(new_injection)),K(std::move(source)),lambda(std::move(shift)) {
            const auto start=guard.steps();guard.step();
            if(old_L.rows()!=old_L.cols() || new_L.rows()!=new_L.cols() ||
               J.rows()!=new_L.rows() || J.cols()!=old_L.rows() || old_I.rows()!=old_L.rows() ||
               new_I.rows()!=new_L.rows() || K.rows()!=new_I.cols() || K.cols()!=old_I.cols())
                throw std::invalid_argument("prepared Green response map dimensions");
            guard.value(lambda);lambda=numeric::canonical_rational(lambda);guard.value(lambda);retained=1;
            for(const auto* map:{&old_L,&new_L,&J,&old_I,&new_I,&K}){
                guard.matrix(*map);retained=field::update_detail::count(retained,map->nonzeros());guard.entries(retained);
            }
            old_solve.emplace(field::update_detail::shifted(old_L,lambda,guard),guard);
            retained=field::update_detail::count(retained,old_solve->stored_entries());guard.entries(retained);
            new_solve.emplace(field::update_detail::shifted(new_L,lambda,guard),guard);
            retained=field::update_detail::count(retained,new_solve->stored_entries());guard.entries(retained);
            EI=field::update_detail::add(numeric::checked_multiply(new_I,K,guard),
                field::update_detail::scale(numeric::checked_multiply(J,old_I,guard),-1,guard),guard);
            retained=field::update_detail::count(retained,EI.nonzeros());guard.entries(retained);
            EL=field::update_detail::add(numeric::checked_multiply(new_L,J,guard),
                field::update_detail::scale(numeric::checked_multiply(J,old_L,guard),-1,guard),guard);
            retained=field::update_detail::count(retained,EL.nonzeros());guard.entries(retained);
            preparation=guard.steps()-start;
        }
    };
public:
    PreparedGreenResponseDelta(Matrix old_L,Matrix new_L,Matrix J,Matrix old_I,Matrix new_I,Matrix K,
                               numeric::Rational lambda,Guard& guard)
        :state_(std::make_shared<const State>(std::move(old_L),std::move(new_L),std::move(J),std::move(old_I),
                                              std::move(new_I),std::move(K),std::move(lambda),guard)){}
    PreparedGreenResponseDelta(Matrix old_L,Matrix new_L,Matrix J,Matrix old_I,Matrix new_I,Matrix K,
                               numeric::Rational lambda,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(old_L),std::move(new_L),
            std::move(J),std::move(old_I),std::move(new_I),std::move(K),std::move(lambda),guard);
    }
    GreenResponseDelta apply(const Vector& old_coordinates,const Vector& new_coordinates,Guard& guard) const {
        const auto pin=state_;guard.step();
        if(old_coordinates.size()!=pin->old_I.cols() || new_coordinates.size()!=pin->new_I.cols())
            throw std::invalid_argument("prepared Green response coordinate dimensions");
        guard.entries(pin->retained);guard.value(pin->lambda);
        for(const auto* map:{&pin->old_L,&pin->new_L,&pin->J,&pin->old_I,&pin->new_I,&pin->K,&pin->EI,&pin->EL})guard.matrix(*map);
        const auto u=guard.canonical_input(old_coordinates),up=guard.canonical_input(new_coordinates);
        const auto g=pin->old_solve->apply(numeric::checked_apply(pin->old_I,u,guard),guard);
        const auto gp=pin->new_solve->apply(numeric::checked_apply(pin->new_I,up,guard),guard);
        const auto zeta=field::update_detail::sub(up,numeric::checked_apply(pin->K,u,guard),guard);
        const auto realization=pin->new_solve->apply(numeric::checked_apply(pin->EI,u,guard),guard);
        const auto innovation=pin->new_solve->apply(numeric::checked_apply(pin->new_I,zeta,guard),guard);
        const auto operator_term=field::update_detail::scale(pin->new_solve->apply(
            numeric::checked_apply(pin->EL,g,guard),guard),-pin->lambda,guard);
        const auto aggregate=field::update_detail::add(field::update_detail::add(realization,innovation,guard),operator_term,guard);
        if(aggregate!=field::update_detail::sub(gp,numeric::checked_apply(pin->J,g,guard),guard))
            throw std::logic_error("Green response delta identity failed");
        return {g,gp,pin->EI,pin->EL,zeta,realization,innovation,operator_term,aggregate};
    }
    GreenResponseDelta apply(const Vector& old_coordinates,const Vector& new_coordinates,
                             numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(old_coordinates,new_coordinates,guard);
    }
    const Matrix& ambient_correspondence() const & {return state_->J;}
    const Matrix& ambient_correspondence() const &&=delete;
    const Matrix& injection_defect() const & {return state_->EI;}
    const Matrix& injection_defect() const &&=delete;
    const Matrix& operator_defect() const & {return state_->EL;}
    const Matrix& operator_defect() const &&=delete;
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
private:
    friend class PreparedObservedGreenResponseDelta;
    void validate(numeric::ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();guard.entries(pin->retained);guard.value(pin->lambda);
        for(const auto* map:{&pin->old_L,&pin->new_L,&pin->J,&pin->old_I,&pin->new_I,&pin->K,&pin->EI,&pin->EL})guard.matrix(*map);
        (void)pin->old_solve->apply(numeric::RationalVector(pin->old_L.rows()),guard);
        (void)pin->new_solve->apply(numeric::RationalVector(pin->new_L.rows()),guard);
    }
    std::shared_ptr<const State> state_;
};
inline GreenResponseDelta green_response_delta(const numeric::ExactMatrix& old_L,
                                               const numeric::ExactMatrix& new_L,
                                               const numeric::ExactMatrix& ambient_correspondence,
                                               const numeric::ExactMatrix& old_injection,
                                               const numeric::ExactMatrix& new_injection,
                                               const numeric::ExactMatrix& source_correspondence,
                                               const numeric::RationalVector& old_coordinates,
                                               const numeric::RationalVector& new_coordinates,
                                               const numeric::Rational& lambda) {
    numeric::ExactActionGuard guard;
    guard.vector(old_coordinates);guard.vector(new_coordinates);
    return PreparedGreenResponseDelta(old_L,new_L,ambient_correspondence,old_injection,new_injection,
                                      source_correspondence,lambda,guard).apply(old_coordinates,new_coordinates,guard);
}

struct ObservedGreenResponseDelta {
    numeric::ExactMatrix accession_defect;
    numeric::RationalVector accession_term;
    numeric::RationalVector realization_term;
    numeric::RationalVector innovation_term;
    numeric::RationalVector operator_term;
    numeric::RationalVector aggregate;
};

namespace transfer_detail {
inline ObservedGreenResponseDelta observe(const GreenResponseDelta& green,
                                          const numeric::ExactMatrix& old_accession,
                                          const numeric::ExactMatrix& new_accession,
                                          const numeric::ExactMatrix& output_correspondence,
                                          const numeric::ExactMatrix& EA,numeric::ExactActionGuard& guard) {
    const auto accession_term=numeric::checked_apply(EA,green.old_response,guard);
    const auto realization_term=numeric::checked_apply(new_accession,green.realization_term,guard);
    const auto innovation_term=numeric::checked_apply(new_accession,green.innovation_term,guard);
    const auto operator_term=numeric::checked_apply(new_accession,green.operator_term,guard);
    auto aggregate=field::update_detail::add(accession_term,realization_term,guard);
    aggregate=field::update_detail::add(aggregate,innovation_term,guard);
    aggregate=field::update_detail::add(aggregate,operator_term,guard);
    const auto direct=field::update_detail::sub(numeric::checked_apply(new_accession,green.new_response,guard),
        numeric::checked_apply(output_correspondence,numeric::checked_apply(old_accession,green.old_response,guard),guard),guard);
    if(aggregate!=direct)throw std::logic_error("observed Green response delta identity failed");
    return {EA,accession_term,realization_term,innovation_term,operator_term,aggregate};
}
} // namespace transfer_detail
inline ObservedGreenResponseDelta observed_green_response_delta(const GreenResponseDelta& green,
                                                                const numeric::ExactMatrix& old_accession,
                                                                const numeric::ExactMatrix& new_accession,
                                                                const numeric::ExactMatrix& ambient_correspondence,
                                                                const numeric::ExactMatrix& output_correspondence) {
    numeric::ExactActionGuard guard;
    const auto EA=field::update_detail::add(numeric::checked_multiply(new_accession,ambient_correspondence,guard),
        field::update_detail::scale(numeric::checked_multiply(output_correspondence,old_accession,guard),-1,guard),guard);
    return transfer_detail::observe(green,old_accession,new_accession,output_correspondence,EA,guard);
}

class PreparedObservedGreenResponseDelta {
    using Matrix=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct State {
        PreparedGreenResponseDelta green;
        Matrix old_A,new_A,K,EA;
        std::size_t retained{},preparation{};
        State(PreparedGreenResponseDelta owner,Matrix old_accession,Matrix new_accession,Matrix output,Guard& guard)
            :green(std::move(owner)),old_A(std::move(old_accession)),new_A(std::move(new_accession)),K(std::move(output)) {
            const auto start=guard.steps();guard.step();
            green.validate(guard);
            const auto& J=green.ambient_correspondence();
            if(old_A.cols()!=J.cols() || new_A.cols()!=J.rows() || K.rows()!=new_A.rows() || K.cols()!=old_A.rows())
                throw std::invalid_argument("prepared Green observation map dimensions");
            retained=green.stored_entries();guard.entries(retained);
            for(const auto* map:{&old_A,&new_A,&K}){
                guard.matrix(*map);retained=field::update_detail::count(retained,map->nonzeros());guard.entries(retained);
            }
            EA=field::update_detail::add(numeric::checked_multiply(new_A,J,guard),
                field::update_detail::scale(numeric::checked_multiply(K,old_A,guard),-1,guard),guard);
            retained=field::update_detail::count(retained,EA.nonzeros());guard.entries(retained);
            preparation=guard.steps()-start;
        }
    };
public:
    PreparedObservedGreenResponseDelta(PreparedGreenResponseDelta green,Matrix old_A,Matrix new_A,Matrix K,Guard& guard)
        :state_(std::make_shared<const State>(std::move(green),std::move(old_A),std::move(new_A),std::move(K),guard)){}
    PreparedObservedGreenResponseDelta(PreparedGreenResponseDelta green,Matrix old_A,Matrix new_A,Matrix K,
                                       numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(green),std::move(old_A),
            std::move(new_A),std::move(K),guard);
    }
    ObservedGreenResponseDelta apply(const numeric::RationalVector& old_coordinates,
                                     const numeric::RationalVector& new_coordinates,Guard& guard) const {
        const auto pin=state_;guard.step();guard.entries(pin->retained);
        for(const auto* map:{&pin->old_A,&pin->new_A,&pin->K,&pin->EA})guard.matrix(*map);
        const auto green=pin->green.apply(old_coordinates,new_coordinates,guard);
        return transfer_detail::observe(green,pin->old_A,pin->new_A,pin->K,pin->EA,guard);
    }
    ObservedGreenResponseDelta apply(const numeric::RationalVector& old_coordinates,
                                     const numeric::RationalVector& new_coordinates,numeric::ExactActionLimits limits={},
                                     std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(old_coordinates,new_coordinates,guard);
    }
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
private:
    std::shared_ptr<const State> state_;
};

} // namespace rfc::temporal
