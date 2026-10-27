#pragma once
#include "rfc/field/finite_jacobian.hpp"
#include "rfc/field/integral_symplectic_basis.hpp"

namespace rfc::field {
// Both values belong to the actual selected real coefficient field. Frequencies
// may themselves be algebraic; no spectral or square-root discovery is implied.
struct DeclaredPeriodFrequency {
    numeric::AlgebraicElement squared_frequency,positive_root;
};

// Full-carrier polynomial normalization of the accepted harmonic star. Repeated
// eigenspaces are retained whole; the input lists distinct frequency VALUES.
class PreparedFiniteJacobianNormalization {
    using A=numeric::SparseAlgebraicMatrix;using E=numeric::AlgebraicElement;
    using M=numeric::ExactMatrix;using Q=numeric::Rational;using Guard=numeric::ExactActionGuard;
public:
    struct Sector {DeclaredPeriodFrequency declaration;A projector;std::size_t dimension;};
    struct Work {std::size_t period_rank,sectors,field_degree,source_cells,source_occurrences,retained_entry_allowance,preparation_steps;};
    static PreparedFiniteJacobianNormalization prepare(PreparedIntegralSymplecticBasis symplectic,
        std::vector<DeclaredPeriodFrequency> frequencies,geometry::SelectedRealAlgebraicEmbedding embedding,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();const auto field=embedding.field();const auto& lattice=symplectic.lattice();
        const auto h=lattice.period_rank(),r=frequencies.size(),degree=field.degree();
        guard.dimension(h);guard.dimension(r);guard.dimension(degree);
        if(r>h)throw std::invalid_argument("frequency list exceeds the complete period carrier");
        if(field.limits().max_integer_bits>guard.limits().integer_bits)throw std::length_error("normalization coefficient field exceeds receiving bit allowance");
        guard.vector(field.minimal_polynomial());guard.value(embedding.certificate().interval.lower);guard.value(embedding.certificate().interval.upper);
        Work work{h,r,degree,0,0,0,0};const auto& source=lattice.source();guard.dimension(source.grade_count());
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto& a=source.grade(core::Grade{static_cast<std::uint16_t>(k)});guard.dimension(a.cell_count());guard.dimension(a.incidence_count());
            work.source_cells=add(work.source_cells,a.cell_count());work.source_occurrences=add(work.source_occurrences,a.incidence_count());
            for(const auto& q:a.cell_scales){guard.step();guard.value(q);}
            for(const auto& z:a.lattice_coefficient){guard.step();guard.value(Q{z});}
        }
        // Retained source tables and requested algebraic frames/projectors are
        // precharged. Counts describe scoped containers/scalars, not GMP/RSS or
        // the internals of an already prepared metric/embedding scalar action.
        auto retained=add(symplectic.work().retained_entry_allowance,add(128,add(mul(64,work.source_cells),mul(96,work.source_occurrences))));
        retained=add(retained,mul(degree,add(mul(32,mul(add(r,4),mul(h,h))),mul(16,add(r,degree)))));
        auto action=phase(guard,retained);
        const auto admit=[&](const E& x){if(!field.owns(x))throw std::invalid_argument("normalization frequency/root coefficient owner differs");jacobian_detail::value(x,action);};
        for(std::size_t i=0;i<r;++i){
            action.step();const auto& f=frequencies[i];admit(f.squared_frequency);admit(f.positive_root);
            for(std::size_t j=0;j<i;++j){action.step();if(f.squared_frequency==frequencies[j].squared_frequency)throw std::invalid_argument("normalization requires distinct frequency values");}
            const auto square=f.positive_root*f.positive_root;admit(square);
            if(square!=f.squared_frequency||embedding.sign(f.squared_frequency)<=0||embedding.sign(f.positive_root)<=0)
                throw std::invalid_argument("normalization needs a positive squared frequency and its selected positive root");
        }
        const auto omega=jacobian_detail::integer_form(lattice.cup_form(),action);
        const auto smith=numeric::smith_normal_form_actions(omega,action);
        const auto star=smith.right_inverse.apply_inverse_rational(smith.left.apply_rational(lattice.gram(),action),action);
        const auto square=period_detail::product(star,star,action);
        const auto s=A::from_rational(star,field,action),s2=A::from_rational(square,field,action),identity=A::identity(h,field,action);
        const auto minus_one=field.scalar(Q{-1});
        A sum(h,h,field),weighted(h,h,field),normalization(h,h,field);std::vector<Sector> sectors;std::size_t total_dimension=0;
        for(std::size_t i=0;i<r;++i){
            auto projector=identity;
            for(std::size_t j=0;j<r;++j)if(j!=i){
                action.step();const auto difference=frequencies[j].squared_frequency-frequencies[i].squared_frequency;admit(difference);
                const auto inverse=difference.inverse();admit(inverse);
                const auto factor=s2.add(identity.scale(frequencies[j].squared_frequency,action),action).scale(inverse,action);
                projector=projector.multiply(factor,action);
            }
            if(projector.nonzeros()==0||projector.multiply(projector,action)!=projector||
                s2.add(identity.scale(frequencies[i].squared_frequency,action),action).multiply(projector,action).nonzeros()!=0||
                s.multiply(projector,action)!=projector.multiply(s,action))
                throw std::invalid_argument("declared frequency has no complete spectral projector on the original star");
            for(const auto& prior:sectors)if(projector.multiply(prior.projector,action).nonzeros()||prior.projector.multiply(projector,action).nonzeros())
                throw std::invalid_argument("normalization projectors are not mutually orthogonal");
            const auto rank=projector.column_basis(action).columns.size();
            if(!rank)throw std::invalid_argument("normalization frequency has an empty eigenspace");
            total_dimension=add(total_dimension,rank);
            sum=sum.add(projector,action);weighted=weighted.add(projector.scale(frequencies[i].squared_frequency,action),action);
            const auto inverse_root=frequencies[i].positive_root.inverse();admit(inverse_root);
            normalization=normalization.add(projector.scale(inverse_root,action),action);
            sectors.push_back({frequencies[i],std::move(projector),rank});
        }
        // The complete identity is required, even when a partial requested
        // family would have perfectly valid idempotents on its represented span.
        if(sum!=identity||total_dimension!=h||s2.add(weighted,action).nonzeros())throw std::invalid_argument("declared frequencies do not cover the full period carrier");
        const auto complex_structure=s.multiply(normalization,action);
        if(normalization.multiply(s,action)!=s.multiply(normalization,action)||complex_structure.multiply(complex_structure,action)!=identity.scale(minus_one,action)||
            normalization.multiply(normalization,action).multiply(s2,action)!=identity.scale(minus_one,action))throw std::logic_error("polynomial normalization residual");
        auto acceptance=phase(guard,retained);
        auto jacobian=PreparedFiniteJacobian::normalize(lattice,symplectic.basis(),normalization,embedding,acceptance);
        if(jacobian.complex_structure()!=complex_structure)throw std::logic_error("normalized Jacobian/source star differs");
        guard.step();work.retained_entry_allowance=retained;work.preparation_steps=guard.steps()-start;
        return PreparedFiniteJacobianNormalization(std::make_shared<const State>(State{std::move(symplectic),std::move(jacobian),std::move(square),std::move(normalization),std::move(sectors),work}));
    }
    const PreparedIntegralSymplecticBasis& symplectic() const & noexcept{return state_->symplectic;}
    const PreparedIntegralSymplecticBasis& symplectic() const &&=delete;
    const PreparedFiniteJacobian& jacobian() const & noexcept{return state_->jacobian;}
    const PreparedFiniteJacobian& jacobian() const &&=delete;
    const M& star_square() const & noexcept{return state_->square;}
    const M& star_square() const &&=delete;
    const A& normalization() const & noexcept{return state_->normalization;}
    const A& normalization() const &&=delete;
    const std::vector<Sector>& sectors() const & noexcept{return state_->sectors;}
    const std::vector<Sector>& sectors() const &&=delete;
    const Work& work() const & noexcept{return state_->work;}
    const Work& work() const &&=delete;
    FiniteJacobianPoint point(const IntegralPeriodPoint& source,Guard& guard) const {
        guard.step();const auto& field=jacobian().embedding().field();
        if(field.limits().max_integer_bits>guard.limits().integer_bits)throw std::length_error("normalization point coefficient field exceeds receiving bit allowance");
        const auto scratch=period_construction_detail::mul(16,period_construction_detail::mul(work().period_rank,work().field_degree));
        auto action=period_construction_detail::phase(guard,period_construction_detail::add(work().retained_entry_allowance,scratch));
        action.vector(source.coordinates());normalization().validate(action);jacobian().real_periods().validate(action);jacobian().imaginary_periods().validate(action);
        auto result=jacobian().point(source,action);guard.step();return result;
    }
private:
    struct State {PreparedIntegralSymplecticBasis symplectic;PreparedFiniteJacobian jacobian;M square;A normalization;std::vector<Sector> sectors;Work work;};
    explicit PreparedFiniteJacobianNormalization(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
