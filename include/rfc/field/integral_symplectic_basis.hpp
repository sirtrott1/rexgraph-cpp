#pragma once
#include "rfc/field/period_construction.hpp"

namespace rfc::field {
// A complete integral change of COHOMOLOGY frame. If C is the accepted dual
// frame, C'=C*T and the dual cycles are Z'=Z*T^{-transpose}. No source cells,
// metric or original period declaration are replaced by this derived frame.
class PreparedIntegralSymplecticBasis {
    using Z=numeric::IntegerMatrix;using M=numeric::ExactMatrix;
    using Q=numeric::Rational;using I=numeric::Integer;using Guard=numeric::ExactActionGuard;
public:
    struct Stage {
        std::size_t offset;
        numeric::SmithNormalFormActions row_smith;
        Z::SparseVector shear;
    };
    struct Work {std::size_t retained_entry_allowance,preparation_steps;};
    static PreparedIntegralSymplecticBasis prepare(PreparedIntegralPeriodLattice lattice,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();const auto h=lattice.period_rank();
        guard.dimension(h);guard.matrix(lattice.cup_form());guard.matrix(lattice.dual_cochains());
        Z omega(h,h);
        for(const auto& [ij,q]:lattice.cup_form().entries()){
            guard.step();if(q.get_den()!=1||q!=-lattice.cup_form().get(ij.second,ij.first))throw std::invalid_argument("symplectic basis requires an integral alternating form");
            omega.set(ij.first,ij.second,q.get_num());
        }
        std::size_t retained=add(128,add(mul(48,mul(h,h)),mul(8,add(lattice.dual_cochains().nonzeros(),lattice.integral_cycles().nonzeros()))));
        retained=add(retained,mul(16,mul(lattice.integral_cycles().rows(),h)));
        guard.entries(retained);auto validation=phase(guard,retained);
        const auto form_smith=numeric::smith_normal_form_actions(omega,validation);
        if(h%2||form_smith.rank!=h)throw std::invalid_argument("symplectic basis requires a nondegenerate even carrier");
        for(const auto& z:form_smith.invariant_factors)if(z!=1)throw std::invalid_argument("symplectic basis requires a unimodular integral form");
        retained=add(retained,action_entries(form_smith));
        Z transform=Z::identity(h);std::vector<Stage> stages;
        // Every primitive row of a unimodular alternating lattice has gcd1.
        // Smith actions construct a partner with pairing1 even if no entry is
        // +/-1. Its orthogonal complement is again an integral unimodular summand.
        for(std::size_t k=0;k<h;k+=2){
            auto action=phase(guard,retained);
            const auto current=transform.transpose().multiply(omega.multiply(transform,action),action);
            const auto m=h-k-1;Z row(1,m);
            for(std::size_t j=0;j<m;++j){action.step();row.set(0,j,current.get(k,k+1+j));}
            auto smith=numeric::smith_normal_form_actions(row,action);
            if(smith.rank!=1||smith.invariant_factors.front()!=1)throw std::logic_error("primitive symplectic row lost");
            const auto v=smith.right_columns(0,m,action);
            const auto sign=smith.left.apply(Z::identity(1),action).get(0,0);
            Z change=Z::identity(h);
            for(std::size_t i=k+1;i<h;++i)change.set(i,i,I{0});
            for(const auto& [ij,z]:v.entries()){
                const auto q=action.mul(Q{z},Q{ij.second==0?sign:I{1}});change.set(k+1+ij.first,k+1+ij.second,q.get_num());
            }
            transform=transform.multiply(change,action);
            const auto paired=transform.transpose().multiply(omega.multiply(transform,action),action);
            if(paired.get(k,k+1)!=1)throw std::logic_error("Smith symplectic partner residual");
            Z shear=Z::identity(h);Z::SparseVector coefficients;
            for(std::size_t j=k+2;j<h;++j){
                action.step();if(paired.get(k,j)!=0)throw std::logic_error("Smith row complement residual");
                const auto z=paired.get(k+1,j);if(z!=0){coefficients.emplace(j,z);shear.set(k,j,z);}
            }
            transform=transform.multiply(shear,action);
            retained=add(retained,add(action_entries(smith),mul(4,coefficients.size())));guard.entries(retained);
            stages.push_back({k,std::move(smith),std::move(coefficients)});
        }
        auto action=phase(guard,retained);Z basis(h,h);
        for(const auto& [ij,z]:transform.entries()){
            action.step();const auto j=ij.second%2?h/2+ij.second/2:ij.second/2;basis.set(ij.first,j,z);
        }
        Z canonical(h,h);for(std::size_t j=0;j<h/2;++j){action.step();canonical.set(j,h/2+j,I{1});canonical.set(h/2+j,j,I{-1});}
        if(basis.transpose().multiply(omega.multiply(basis,action),action)!=canonical)throw std::logic_error("complete symplectic pairing residual");
        auto smith=numeric::smith_normal_form_actions(basis,action);
        if(smith.rank!=h)throw std::logic_error("symplectic frame rank residual");
        for(const auto& z:smith.invariant_factors)if(z!=1)throw std::logic_error("symplectic frame integral index residual");
        auto inverse=smith.right_inverse.apply_inverse(smith.left.apply(Z::identity(h),action),action);
        if(basis.multiply(inverse,action)!=Z::identity(h)||inverse.multiply(basis,action)!=Z::identity(h))throw std::logic_error("symplectic inverse residual");
        auto cycles=lattice.integral_cycles().multiply(inverse.transpose(),action);
        auto cochains=period_detail::product(lattice.dual_cochains(),period_detail::rational(basis,action),action);
        if(period_detail::product(period_detail::rational(cycles,action).transpose(),cochains,action)!=M::identity(h))throw std::logic_error("transported dual cycle/cochain residual");
        retained=add(retained,add(action_entries(smith),mul(8,add(cycles.nonzeros(),cochains.nonzeros()))));guard.entries(retained);
        guard.step();Work work{retained,guard.steps()-start};
        return PreparedIntegralSymplecticBasis(std::make_shared<const State>(State{std::move(lattice),std::move(basis),std::move(inverse),std::move(cycles),std::move(cochains),std::move(smith),std::move(stages),work}));
    }
    const PreparedIntegralPeriodLattice& lattice() const & noexcept{return state_->lattice;}
    const PreparedIntegralPeriodLattice& lattice() const &&=delete;
    const Z& basis() const & noexcept{return state_->basis;}
    const Z& basis() const &&=delete;
    const Z& inverse() const & noexcept{return state_->inverse;}
    const Z& inverse() const &&=delete;
    const Z& cycles() const & noexcept{return state_->cycles;}
    const Z& cycles() const &&=delete;
    const M& cochains() const & noexcept{return state_->cochains;}
    const M& cochains() const &&=delete;
    const numeric::SmithNormalFormActions& basis_smith() const & noexcept{return state_->smith;}
    const numeric::SmithNormalFormActions& basis_smith() const &&=delete;
    const std::vector<Stage>& stages() const & noexcept{return state_->stages;}
    const std::vector<Stage>& stages() const &&=delete;
    const Work& work() const & noexcept{return state_->work;}
    const Work& work() const &&=delete;
private:
    struct State {PreparedIntegralPeriodLattice lattice;Z basis,inverse,cycles;M cochains;numeric::SmithNormalFormActions smith;std::vector<Stage> stages;Work work;};
    explicit PreparedIntegralSymplecticBasis(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
