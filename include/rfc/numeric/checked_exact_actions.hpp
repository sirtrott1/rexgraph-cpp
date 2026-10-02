#pragma once

#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/machine/checked_size.hpp"
#include <functional>
#include <limits>
#include <memory>

namespace rfc::numeric {
struct ExactActionLimits {
    std::size_t dimension{4096},entries{16000000},steps{64000000},integer_bits{1u<<24};
};
// These limits cover this action's scalar operations/containers, not GMP/RSS.
class ExactActionGuard {
public:
    explicit ExactActionGuard(ExactActionLimits limits={},std::function<void()> checkpoint={})
        :limits_(limits),checkpoint_(std::move(checkpoint)){}
    void step() {
        if(checkpoint_)checkpoint_();
        if(steps_==limits_.steps)throw std::length_error("exact action step limit");
        ++steps_;
    }
    void value(const Rational& q) const {
        if(mpz_sgn(mpq_denref(q.get_mpq_t()))==0)throw std::invalid_argument("zero exact rational denominator");
        const auto bits=[](mpz_srcptr z){return mpz_sgn(z)?mpz_sizeinbase(z,2):std::size_t{0};};
        if(std::max(bits(mpq_numref(q.get_mpq_t())),bits(mpq_denref(q.get_mpq_t())))>limits_.integer_bits)
            throw std::length_error("exact action integer bit limit");
    }
    void vector(const RationalVector& v) {
        dimension(v.size());for(const auto& q:v){step();value(q);}
    }
    RationalVector canonical_input(const RationalVector& v) {
        vector(v);RationalVector out;out.reserve(v.size());
        for(const auto& q:v){step();out.push_back(canonical_rational(q));value(out.back());}
        return out;
    }
    void matrix(const ExactMatrix& a) {
        dimension(a.rows());dimension(a.cols());entries(a.nonzeros());for(const auto& [key,q]:a.entries()){(void)key;step();value(q);}
    }
    void dimension(std::size_t n) const {if(n>limits_.dimension)throw std::length_error("exact action dimension limit");}
    void entries(std::size_t n) const {if(n>limits_.entries)throw std::length_error("exact action stored entry limit");}
    const ExactActionLimits& limits() const & {return limits_;}
    const ExactActionLimits& limits() const &&=delete;
    std::size_t steps() const noexcept {return steps_;}
    Rational mul(const Rational& a,const Rational& b) {step();Rational q=a*b;value(q);return q;}
    Rational sub(const Rational& a,const Rational& b) {step();Rational q=a-b;value(q);return q;}
    Rational add(const Rational& a,const Rational& b) {step();Rational q=a+b;value(q);return q;}
    Rational div(const Rational& a,const Rational& b) {step();if(b==0)throw std::invalid_argument("zero exact divisor");Rational q=a/b;value(q);return q;}
private:
    ExactActionLimits limits_;std::function<void()> checkpoint_;std::size_t steps_{};
};
inline RationalVector checked_apply(const ExactMatrix& a,const RationalVector& x,ExactActionGuard& guard) {
    if(x.size()!=a.cols())throw std::invalid_argument("checked exact action input dimension");
    guard.matrix(a);const auto input=guard.canonical_input(x);RationalVector out(a.rows());
    for(const auto& [key,q]:a.entries())out[key.first]=guard.add(out[key.first],guard.mul(q,input[key.second]));
    return out;
}
inline ExactMatrix checked_multiply(const ExactMatrix& a,const ExactMatrix& b,ExactActionGuard& guard) {
    if(a.cols()!=b.rows())throw std::invalid_argument("checked matrix product dimensions");
    guard.matrix(a);guard.matrix(b);ExactMatrix out(a.rows(),b.cols());
    std::vector<std::vector<std::pair<std::size_t,Rational>>> rows(b.rows());
    for(const auto& [key,q]:b.entries())rows[key.first].emplace_back(key.second,q);
    for(const auto& [key,q]:a.entries())for(const auto& [j,v]:rows[key.second]) {
        out.set(key.first,j,guard.add(out.get(key.first,j),guard.mul(q,v)));guard.entries(out.nonzeros());
    }
    return out;
}
// Reusable row-pivoted sparse elimination. No inverse is formed. Every saved
// elimination operation and upper factor belongs to one immutable matrix owner.
class PreparedExactSolve {
    struct State {
        ExactMatrix original;
        std::vector<std::map<std::size_t,Rational>> upper;
        struct Pivot {std::size_t row;std::vector<std::pair<std::size_t,Rational>> eliminate;};
        std::vector<Pivot> pivots;
        std::size_t stored_entries{},preparation_steps{};
        State(ExactMatrix a,ExactActionGuard& guard):original(std::move(a)) {
            if(original.rows()!=original.cols())throw std::invalid_argument("prepared solve requires a square declared operator");
            guard.matrix(original);
            upper.resize(original.rows());
            for(const auto& [key,q]:original.entries())upper[key.first].emplace(key.second,q);
            for(std::size_t k=0;k<upper.size();++k) {
                std::size_t pivot=upper.size(),best=std::numeric_limits<std::size_t>::max();
                for(std::size_t i=k;i<upper.size();++i){guard.step();if(upper[i].contains(k) && upper[i].size()<best){pivot=i;best=upper[i].size();}}
                if(pivot==upper.size())throw std::invalid_argument("singular operator in admitted exact solve profile");
                pivots.push_back({pivot,{}});if(pivot!=k)std::swap(upper[k],upper[pivot]);
                for(std::size_t i=k+1;i<upper.size();++i)if(const auto it=upper[i].find(k);it!=upper[i].end()) {
                    const auto ratio=guard.div(it->second,upper[k].at(k));pivots.back().eliminate.emplace_back(i,ratio);upper[i].erase(it);
                    for(const auto& [j,q]:upper[k])if(j>k) {
                        const auto found=upper[i].find(j);const Rational old=found==upper[i].end()?Rational{0}:found->second;
                        auto updated=guard.sub(old,guard.mul(ratio,q));
                        if(updated==0)upper[i].erase(j);else upper[i][j]=std::move(updated);
                    }
                    guard.entries(count_entries());
                }
            }
            stored_entries=count_entries();guard.entries(stored_entries);preparation_steps=guard.steps();
        }
        std::size_t count_entries() const {
            auto n=original.nonzeros();for(const auto& row:upper)n=machine::checked_size_add(n,row.size());
            for(const auto& p:pivots)n=machine::checked_size_add(n,p.eliminate.size());
            return n;
        }
    };
public:
    PreparedExactSolve(const PreparedExactSolve&)=default;
    PreparedExactSolve& operator=(const PreparedExactSolve&)=default;
    PreparedExactSolve(ExactMatrix a,ExactActionGuard& guard):state_(std::make_shared<const State>(std::move(a),guard)){}
    explicit PreparedExactSolve(ExactMatrix a,ExactActionLimits limits={},std::function<void()> check={}) {
        ExactActionGuard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(a),guard);
    }
    RationalVector apply(const RationalVector& rhs,ExactActionGuard& guard) const {
        if(rhs.size()!=state_->upper.size())throw std::invalid_argument("prepared solve right-hand side dimension");
        guard.entries(state_->stored_entries);guard.matrix(state_->original);const auto input=guard.canonical_input(rhs);
        for(const auto& row:state_->upper)for(const auto& [j,q]:row){(void)j;guard.step();guard.value(q);}
        auto b=input;
        for(std::size_t k=0;k<b.size();++k) {
            const auto& p=state_->pivots[k];if(p.row!=k)std::swap(b[k],b[p.row]);
            for(const auto& [i,ratio]:p.eliminate){guard.value(ratio);b[i]=guard.sub(b[i],guard.mul(ratio,b[k]));}
        }
        RationalVector x(b.size());
        for(std::size_t i=b.size();i-- >0;) {
            Rational sum;
            for(const auto& [j,q]:state_->upper[i])if(j>i)sum=guard.add(sum,guard.mul(q,x[j]));
            x[i]=guard.div(guard.sub(b[i],sum),state_->upper[i].at(i));
        }
        if(checked_apply(state_->original,x,guard)!=input)throw std::logic_error("prepared solve has nonzero exact residual");
        return x;
    }
    RationalVector apply(const RationalVector& rhs,ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return apply(rhs,guard);
    }
    const ExactMatrix& matrix() const & {return state_->original;}
    const ExactMatrix& matrix() const &&=delete;
    std::size_t stored_entries() const {return state_->stored_entries;}
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
