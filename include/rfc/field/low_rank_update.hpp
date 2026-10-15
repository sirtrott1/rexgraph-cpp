#pragma once
#include "rfc/numeric/checked_exact_actions.hpp"
#include <optional>

namespace rfc::field {

namespace update_detail {
using Matrix = numeric::ExactMatrix;
using Vector = numeric::RationalVector;
using Guard = numeric::ExactActionGuard;
inline std::size_t count(std::size_t a, std::size_t b) {return machine::checked_size_add(a,b);}
inline Matrix transpose(const Matrix& a, Guard& guard) {
    guard.matrix(a);Matrix out(a.cols(),a.rows());
    for(const auto& [key,q]:a.entries()){guard.step();out.set(key.second,key.first,q);}
    return out;
}
inline Matrix add(const Matrix& a, const Matrix& b, Guard& guard) {
    if(a.rows()!=b.rows() || a.cols()!=b.cols())throw std::invalid_argument("update matrix sum dimensions");
    guard.matrix(a);guard.matrix(b);Matrix out=a;
    for(const auto& [key,q]:b.entries()){
        auto value=guard.add(out.get(key.first,key.second),q);
        if(value!=0 && out.get(key.first,key.second)==0)guard.entries(count(out.nonzeros(),1));
        out.set(key.first,key.second,std::move(value));
    }
    return out;
}
inline Matrix scale(const Matrix& a, const numeric::Rational& scalar, Guard& guard) {
    guard.matrix(a);guard.step();guard.value(scalar);const auto q=numeric::canonical_rational(scalar);guard.value(q);
    Matrix out(a.rows(),a.cols());
    for(const auto& [key,value]:a.entries())out.set(key.first,key.second,guard.mul(q,value));
    return out;
}
inline Matrix identity(std::size_t n, Guard& guard) {
    guard.dimension(n);guard.entries(n);guard.value(numeric::Rational{1});Matrix out(n,n);
    for(std::size_t i=0;i<n;++i){guard.step();out.set(i,i,1);}
    return out;
}
inline Vector add(const Vector& a, const Vector& b, Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("update vector sum dimensions");
    const auto x=guard.canonical_input(a),y=guard.canonical_input(b);Vector out(x.size());
    for(std::size_t i=0;i<x.size();++i)out[i]=guard.add(x[i],y[i]);
    return out;
}
inline Vector scale(const Vector& a, const numeric::Rational& scalar, Guard& guard) {
    guard.step();guard.value(scalar);const auto q=numeric::canonical_rational(scalar);guard.value(q);
    auto out=guard.canonical_input(a);for(auto& value:out)value=guard.mul(q,value);return out;
}
inline Vector sub(const Vector& a, const Vector& b, Guard& guard) {return add(a,scale(b,-1,guard),guard);}
inline Matrix shifted(const Matrix& a, const numeric::Rational& lambda, Guard& guard) {
    if(a.rows()!=a.cols())throw std::invalid_argument("shifted operator dimensions");
    return add(identity(a.rows(),guard),scale(a,lambda,guard),guard);
}
inline Matrix columns(const numeric::PreparedExactSolve& solve, const Matrix& b, Guard& guard,
                      std::size_t resident=0) {
    if(solve.matrix().rows()!=b.rows())throw std::invalid_argument("column solve dimension mismatch");
    guard.matrix(b);guard.entries(count(resident,solve.stored_entries()));
    // Validate the complete immutable factor even for an empty column request.
    (void)solve.apply(Vector(b.rows()),guard);Matrix out(b.rows(),b.cols());
    for(std::size_t j=0;j<b.cols();++j){
        Vector rhs(b.rows());for(std::size_t i=0;i<b.rows();++i){guard.step();rhs[i]=b.get(i,j);}
        const auto x=solve.apply(rhs,guard);
        for(std::size_t i=0;i<x.size();++i){
            guard.step();if(x[i]!=0){
                guard.entries(count(count(resident,solve.stored_entries()),count(out.nonzeros(),1)));
                out.set(i,j,x[i]);
            }
        }
    }
    return out;
}
inline void shapes(const Matrix& a, const Matrix& u, const Matrix& c, const Matrix& v, Guard& guard) {
    if(a.rows()!=a.cols() || u.rows()!=a.rows() || v.rows()!=a.rows() ||
       u.cols()!=c.rows() || c.rows()!=c.cols() || c.cols()!=v.cols())
        throw std::invalid_argument("Woodbury dimension mismatch");
    guard.matrix(a);guard.matrix(u);guard.matrix(c);guard.matrix(v);
}
} // namespace update_detail
inline numeric::ExactMatrix solve_columns_exact(const numeric::PreparedExactSolve& solve,
                                                const numeric::ExactMatrix& b, numeric::ExactActionGuard& guard) {
    return update_detail::columns(solve,b,guard);
}
inline numeric::ExactMatrix solve_columns_exact(const numeric::ExactMatrix& a, const numeric::ExactMatrix& b) {
    if(a.rows()!=a.cols() || a.rows()!=b.rows())throw std::invalid_argument("column solve dimension mismatch");
    numeric::ExactActionGuard guard;guard.matrix(b);
    return solve_columns_exact(numeric::PreparedExactSolve(a,guard),b,guard);
}
// Owns the complete declared maps. V^T is the algebraic transpose over Q,
// not a metric adjoint. A, C and I+C V^T A^-1 U must be invertible.
class PreparedWoodburySolve {
    using Matrix=numeric::ExactMatrix;
    using Vector=numeric::RationalVector;
    using Guard=numeric::ExactActionGuard;
    struct State {
        numeric::PreparedExactSolve base,coupling;
        Matrix u,v,base_u;
        std::optional<numeric::PreparedExactSolve> correction;
        std::size_t retained{};
        State(numeric::PreparedExactSolve owner,Matrix U,Matrix C,Matrix V,Guard& guard)
            :base(std::move(owner)),coupling(std::move(C),guard),u(std::move(U)),v(std::move(V)) {
            retained=update_detail::count(update_detail::count(base.stored_entries(),coupling.stored_entries()),
                                          update_detail::count(u.nonzeros(),v.nonzeros()));guard.entries(retained);
            base_u=update_detail::columns(base,u,guard,
                update_detail::count(coupling.stored_entries(),update_detail::count(u.nonzeros(),v.nonzeros())));
            retained=update_detail::count(retained,base_u.nonzeros());guard.entries(retained);
            auto middle=update_detail::add(update_detail::identity(coupling.matrix().rows(),guard),
                numeric::checked_multiply(coupling.matrix(),numeric::checked_multiply(
                    update_detail::transpose(v,guard),base_u,guard),guard),guard);
            guard.entries(update_detail::count(retained,middle.nonzeros()));
            correction.emplace(std::move(middle),guard);
            retained=update_detail::count(retained,correction->stored_entries());guard.entries(retained);
        }
    };
    static std::shared_ptr<const State> prepare(numeric::PreparedExactSolve base,Matrix u,Matrix c,Matrix v,Guard& guard) {
        update_detail::shapes(base.matrix(),u,c,v,guard);
        return std::make_shared<const State>(std::move(base),std::move(u),std::move(c),std::move(v),guard);
    }
public:
    PreparedWoodburySolve(Matrix a,Matrix u,Matrix c,Matrix v,Guard& guard) {
        const auto start=guard.steps();update_detail::shapes(a,u,c,v,guard);
        state_=prepare(numeric::PreparedExactSolve(std::move(a),guard),std::move(u),std::move(c),std::move(v),guard);
        preparation_=guard.steps()-start;
    }
    PreparedWoodburySolve(numeric::PreparedExactSolve base,Matrix u,Matrix c,Matrix v,Guard& guard) {
        const auto start=guard.steps();state_=prepare(std::move(base),std::move(u),std::move(c),std::move(v),guard);
        preparation_=guard.steps()-start;
    }
    PreparedWoodburySolve(Matrix a,Matrix u,Matrix c,Matrix v,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));const auto start=guard.steps();update_detail::shapes(a,u,c,v,guard);
        state_=prepare(numeric::PreparedExactSolve(std::move(a),guard),std::move(u),std::move(c),std::move(v),guard);
        preparation_=guard.steps()-start;
    }
    PreparedWoodburySolve(numeric::PreparedExactSolve base,Matrix u,Matrix c,Matrix v,
                          numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        Guard guard(limits,std::move(check));const auto start=guard.steps();
        state_=prepare(std::move(base),std::move(u),std::move(c),std::move(v),guard);preparation_=guard.steps()-start;
    }
    Vector apply(const Vector& rhs,Guard& guard) const {
        const auto pin=state_;guard.step();
        if(rhs.size()!=pin->base.matrix().rows())throw std::invalid_argument("Woodbury right-hand side dimension");
        guard.entries(pin->retained);guard.matrix(pin->u);guard.matrix(pin->v);guard.matrix(pin->base_u);
        const auto b=guard.canonical_input(rhs);
        // C remains an invertibility witness, never a stored inverse. Recheck
        // its retained factors under current limits, including a zero update.
        (void)pin->coupling.apply(Vector(pin->coupling.matrix().rows()),guard);
        const auto x=pin->base.apply(b,guard);
        const auto coefficients=pin->correction->apply(numeric::checked_apply(pin->coupling.matrix(),
            numeric::checked_transpose_apply(pin->v,x,guard),guard),guard);
        const auto answer=update_detail::sub(x,numeric::checked_apply(pin->base_u,coefficients,guard),guard);
        const auto residual=update_detail::add(numeric::checked_apply(pin->base.matrix(),answer,guard),
            numeric::checked_apply(pin->u,numeric::checked_apply(pin->coupling.matrix(),
                numeric::checked_transpose_apply(pin->v,answer,guard),guard),guard),guard);
        if(residual!=b)throw std::logic_error("Woodbury original updated-system residual");
        return answer;
    }
    Vector apply(const Vector& rhs,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(rhs,guard);
    }
    const numeric::PreparedExactSolve& base_solve() const & {return state_->base;}
    const numeric::PreparedExactSolve& base_solve() const &&=delete;
    std::size_t update_size() const noexcept {return state_->coupling.matrix().rows();}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return preparation_;}
private:
    std::shared_ptr<const State> state_;
    std::size_t preparation_{};
};
inline numeric::RationalVector woodbury_solve(const numeric::ExactMatrix& a,const numeric::ExactMatrix& u,
                                              const numeric::ExactMatrix& c,const numeric::ExactMatrix& v,
                                              const numeric::RationalVector& rhs) {
    numeric::ExactActionGuard guard;
    if(rhs.size()!=a.rows())throw std::invalid_argument("Woodbury right-hand side dimension");
    guard.vector(rhs);return PreparedWoodburySolve(a,u,c,v,guard).apply(rhs,guard);
}

inline numeric::ExactMatrix low_rank_updated_matrix(const numeric::ExactMatrix& A,
                                                    const numeric::ExactMatrix& U,
                                                    const numeric::ExactMatrix& C,
                                                    const numeric::ExactMatrix& V) {
    return A.add(U.multiply(C).multiply(V.transpose()));
}

} // namespace rfc::field
