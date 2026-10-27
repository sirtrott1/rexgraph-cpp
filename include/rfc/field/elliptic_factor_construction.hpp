#pragma once
#include "rfc/field/finite_jacobian_normalization.hpp"

namespace rfc::field {
// Complete automatic factor construction on the admitted rational, rank-two
// frequency profile. A repeated larger sector or nonrational frequency refuses
// this profile without changing its accepted finite-frequency normalization. Supplied-plane
// workflows on PreparedFiniteJacobian remain available independently.
class PreparedFiniteEllipticFactors {
    using Q=numeric::Rational;using I=numeric::Integer;using Z=numeric::IntegerMatrix;
    using M=numeric::ExactMatrix;using A=numeric::SparseAlgebraicMatrix;using Guard=numeric::ExactActionGuard;
public:
    struct Kernel {
        Q squared_frequency;
        I denominator;
        Z integer_equation,lattice_basis;
        numeric::SmithNormalFormActions smith;
    };
    struct Work {std::size_t period_rank,factors,retained_entry_allowance,preparation_steps;};
    static PreparedFiniteEllipticFactors prepare(PreparedFiniteJacobianNormalization normalization,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();const auto& jacobian=normalization.jacobian();
        const auto field=jacobian.embedding().field();const auto h=jacobian.lattice().period_rank(),r=normalization.sectors().size();
        guard.dimension(h);guard.dimension(r);guard.dimension(field.degree());
        if(field.limits().max_integer_bits>guard.limits().integer_bits)throw std::length_error("factor coefficient field exceeds receiving bit allowance");
        if(mul(2,r)!=h)throw std::invalid_argument("automatic factors require complete two-dimensional frequency sectors");
        auto retained=add(normalization.work().retained_entry_allowance,mul(64,add(mul(add(r,1),mul(h,h)),mul(r,field.degree()))));
        auto validation=phase(guard,retained);validation.matrix(normalization.star_square());normalization.normalization().validate(validation);
        std::vector<Q> frequencies;frequencies.reserve(r);
        for(const auto& sector:normalization.sectors()){
            validation.step();if(sector.dimension!=2)throw std::invalid_argument("automatic factors do not split a higher-dimensional repeated sector");
            const auto& coordinates=sector.declaration.squared_frequency.coordinates();validation.vector(coordinates);
            for(std::size_t i=1;i<coordinates.size();++i)if(coordinates[i]!=0)throw std::invalid_argument("automatic factors require rational squared frequencies");
            auto mu=coordinates.front();mu.canonicalize();if(mu<=0)throw std::logic_error("accepted normalization has a nonpositive rational frequency");
            jacobian_detail::value(sector.declaration.positive_root,validation);frequencies.push_back(std::move(mu));
        }
        std::vector<Kernel> kernels;std::vector<DeclaredPeriodPlane> declarations;
        kernels.reserve(r);declarations.reserve(r);
        for(std::size_t part=0;part<r;++part){
            auto action=phase(guard,retained);auto equation=normalization.star_square();
            for(std::size_t i=0;i<h;++i)equation.set(i,i,action.add(equation.get(i,i),frequencies[part]));
            // One scalar clears the equation, preserving its kernel on the
            // original integral cohomology coordinates. Columns are not rescaled.
            I denominator{1};
            for(const auto& [ij,q]:equation.entries()){
                (void)ij;action.step();const auto d=numeric::denominator(q);
                mpz_lcm(denominator.get_mpz_t(),denominator.get_mpz_t(),d.get_mpz_t());action.value(Q{denominator});
            }
            Z integral(h,h);
            for(const auto& [ij,q]:equation.entries()){
                const auto value=action.mul(q,Q{denominator});
                if(value.get_den()!=1)throw std::logic_error("factor equation denominator certificate failed");
                integral.set(ij.first,ij.second,value.get_num());
            }
            auto smith=numeric::smith_normal_form_actions(integral,action);
            if(h-smith.rank!=2)throw std::invalid_argument("declared rational frequency kernel is not exactly two-dimensional");
            auto basis=smith.right_columns(smith.rank,2,action);
            if(integral.multiply(basis,action).nonzeros())throw std::logic_error("full integral factor kernel residual");
            const auto algebraic=A::from_integer(basis,field,action);
            if(normalization.sectors()[part].projector.multiply(algebraic,action)!=algebraic)
                throw std::logic_error("factor kernel differs from its actual normalization sector");
            declarations.push_back({frequencies[part],normalization.sectors()[part].declaration.positive_root,period_detail::rational(basis,action)});
            retained=add(retained,add(action_entries(smith),mul(8,add(integral.nonzeros(),basis.nonzeros()))));guard.entries(retained);
            kernels.push_back({frequencies[part],std::move(denominator),std::move(integral),std::move(basis),std::move(smith)});
        }
        auto acceptance=phase(guard,retained);
        auto certificate=jacobian.declared_elliptic_factors(declarations,acceptance);
        // The final consumer can orient/rebase a plane; certify that both its
        // lattice and ours are in the same full source kernel, with no pooling.
        for(std::size_t i=0;i<r;++i)if(kernels[i].integer_equation.multiply(certificate.factors()[i].saturated_lattice,acceptance).nonzeros())
            throw std::logic_error("accepted factor lattice left its original rational kernel");
        guard.step();Work work{h,r,retained,guard.steps()-start};
        return PreparedFiniteEllipticFactors(std::make_shared<const State>(State{std::move(normalization),std::move(kernels),std::move(certificate),work}));
    }
    const PreparedFiniteJacobianNormalization& normalization() const & noexcept{return state_->normalization;}
    const PreparedFiniteJacobianNormalization& normalization() const &&=delete;
    const std::vector<Kernel>& kernels() const & noexcept{return state_->kernels;}
    const std::vector<Kernel>& kernels() const &&=delete;
    const FiniteIsogenyCertificate& certificate() const & noexcept{return state_->certificate;}
    const FiniteIsogenyCertificate& certificate() const &&=delete;
    const Work& work() const & noexcept{return state_->work;}
    const Work& work() const &&=delete;
private:
    struct State {PreparedFiniteJacobianNormalization normalization;std::vector<Kernel> kernels;FiniteIsogenyCertificate certificate;Work work;};
    explicit PreparedFiniteEllipticFactors(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
