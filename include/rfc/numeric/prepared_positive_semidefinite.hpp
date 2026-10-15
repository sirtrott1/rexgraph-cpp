#pragma once

#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <algorithm>
#include <map>
#include <memory>
#include <vector>

namespace rfc::numeric {

struct PositiveSemidefiniteStats {
    std::size_t rank{}, factor_nonzeros{}, peak_rational_entries{};
    std::size_t fill_insertions{}, exact_cancellations{}, preparation_steps{};
};

// Exact square-root-free LDL certificate. The unit diagonal of L is implicit;
// absent rows and zero pivots consume no coordinate-sized scratch. Positivity
// is a checked hypothesis, independent of a caller's name for the form.
class PreparedPositiveSemidefinite {
    struct State {
        ExactMatrix original, lower;
        std::map<std::size_t,Rational> diagonal;
        PositiveSemidefiniteStats stats;
        State(ExactMatrix form, ExactActionGuard& guard)
            : original(std::move(form)), lower(original.rows(),original.cols()) {
            const auto start=guard.steps();
            if(original.rows()!=original.cols())
                throw std::invalid_argument("positive semidefinite form must be square");
            guard.matrix(original);
            std::map<std::size_t,std::map<std::size_t,Rational>> rows;
            for(const auto& [key,value]:original.entries()) {
                guard.step();
                if(value!=original.get(key.second,key.first))
                    throw std::invalid_argument("positive semidefinite form must be symmetric");
                rows[key.first].emplace(key.second,value);
            }
            std::size_t working=original.nonzeros(),neighbor_entries{};
            const auto account=[&](std::size_t additional=0) {
                const auto count=machine::checked_size_add(
                    machine::checked_size_add(original.nonzeros(),working),
                    machine::checked_size_add(machine::checked_size_add(lower.nonzeros(),diagonal.size()),
                        machine::checked_size_add(neighbor_entries,additional)));
                guard.entries(count);
                stats.peak_rational_entries=std::max(stats.peak_rational_entries,count);
            };
            const auto get=[&](std::size_t i,std::size_t j)->Rational {
                const auto row=rows.find(i);
                if(row==rows.end())return Rational{0};
                const auto item=row->second.find(j);
                return item==row->second.end()?Rational{0}:item->second;
            };
            const auto set=[&](std::size_t i,std::size_t j,const Rational& value) {
                auto row=rows.find(i);
                if(value==0) {
                    if(row!=rows.end()) {
                        if(row->second.erase(j)){--working;++stats.exact_cancellations;}
                        if(row->second.empty())rows.erase(row);
                    }
                } else {
                    auto& destination=rows[i];
                    auto [at,inserted]=destination.emplace(j,value);
                    if(inserted){++working;++stats.fill_insertions;}else at->second=value;
                }
                account();
            };
            account();
            while(!rows.empty()) {
                guard.step();
                const auto k=rows.begin()->first;
                const auto pivot=get(k,k);
                // In a PSD form, a zero diagonal forces its entire row zero.
                // Stored rows are nonempty, so a nonpositive pivot is invalid.
                if(pivot<=0)
                    throw std::invalid_argument("form is not positive semidefinite");
                account(1);diagonal.emplace(k,pivot);
                std::vector<std::pair<std::size_t,Rational>> neighbors;
                for(const auto& [j,value]:rows.begin()->second) {
                    guard.step();
                    if(j<k)throw std::logic_error("PSD elimination left a lower pivot entry");
                    if(j>k){++neighbor_entries;account();neighbors.emplace_back(j,value);}
                }
                for(std::size_t p=0;p<neighbors.size();++p) {
                    const auto& [i,ik]=neighbors[p];
                    const auto ratio=guard.div(ik,pivot);
                    lower.set(i,k,ratio);account();
                    for(std::size_t q=p;q<neighbors.size();++q) {
                        const auto& [j,jk]=neighbors[q];
                        const auto next=guard.sub(get(i,j),guard.mul(ratio,jk));
                        set(i,j,next);if(i!=j)set(j,i,next);
                    }
                    set(i,k,Rational{0});
                }
                const auto remaining=rows.find(k);
                if(remaining==rows.end())throw std::logic_error("PSD pivot disappeared");
                working-=remaining->second.size();rows.erase(remaining);
                account();
                neighbors.clear();neighbor_entries=0;account();
            }
            stats.rank=diagonal.size();
            stats.factor_nonzeros=machine::checked_size_add(lower.nonzeros(),diagonal.size());
            stats.preparation_steps=guard.steps()-start;
        }
    };
public:
    PreparedPositiveSemidefinite(ExactMatrix form,ExactActionGuard& guard)
        :state_(std::make_shared<const State>(std::move(form),guard)){}
    explicit PreparedPositiveSemidefinite(ExactMatrix form,ExactActionLimits limits={}) {
        ExactActionGuard guard(limits);state_=std::make_shared<const State>(std::move(form),guard);
    }
    const ExactMatrix& original() const & {return state().original;}
    const ExactMatrix& original() const &&=delete;
    // Strict lower entries only: L = identity + unit_lower_entries().
    const ExactMatrix& unit_lower_entries() const & {return state().lower;}
    const ExactMatrix& unit_lower_entries() const &&=delete;
    const std::map<std::size_t,Rational>& diagonal_entries() const & {return state().diagonal;}
    const std::map<std::size_t,Rational>& diagonal_entries() const &&=delete;
    const PositiveSemidefiniteStats& stats() const & {return state().stats;}
    const PositiveSemidefiniteStats& stats() const &&=delete;
    std::size_t size() const {return state().original.rows();}
    std::size_t rank() const {return state().stats.rank;}
    Rational quadratic_form(const RationalVector& x,ExactActionGuard& guard) const {
        if(x.size()!=size())throw std::invalid_argument("PSD quadratic form carrier mismatch");
        const auto canonical=guard.canonical_input(x);
        std::vector<SparseRationalEntry> entries;
        for(std::size_t i=0;i<canonical.size();++i) {
            guard.step();if(canonical[i]!=0)entries.push_back({i,canonical[i]});
        }
        return quadratic_form(SparseRationalVector(x.size(),std::move(entries)),guard);
    }
    Rational quadratic_form(const SparseRationalVector& x,ExactActionGuard& guard) const {
        guard.step();
        if(x.size()!=size())throw std::invalid_argument("PSD sparse quadratic form carrier mismatch");
        const auto& pin=state();guard.sparse_vector(x);
        guard.matrix(pin.original);guard.matrix(pin.lower);
        const auto retained=machine::checked_size_add(pin.original.nonzeros(),pin.stats.factor_nonzeros);
        guard.entries(retained);
        for(const auto& [k,pivot]:pin.diagonal){(void)k;guard.step();guard.value(pivot);}
        // x^T L D L^T x, using only nonzero projected coordinates. The unit
        // diagonal is implicit and the lower map is indexed by actual rows.
        std::map<std::size_t,Rational> coordinates;
        const auto accumulate=[&](std::size_t k,const Rational& contribution) {
            if(contribution==0)return;
            const auto at=coordinates.find(k);
            const auto value=at==coordinates.end()?contribution:guard.add(at->second,contribution);
            if(value==0){if(at!=coordinates.end())coordinates.erase(at);return;}
            if(at==coordinates.end()) {
                guard.entries(machine::checked_size_add(retained,machine::checked_size_add(x.nonzeros(),
                    machine::checked_size_add(coordinates.size(),1))));
                coordinates.emplace(k,value);
            } else at->second=value;
        };
        for(const auto& entry:x.entries()) {
            guard.step();
            if(pin.diagonal.contains(entry.index))accumulate(entry.index,entry.value);
            auto at=pin.lower.entries().lower_bound({entry.index,0});
            for(;at!=pin.lower.entries().end() && at->first.first==entry.index;++at)
                accumulate(at->first.second,guard.mul(at->second,entry.value));
        }
        Rational result{0};
        for(const auto& [k,value]:coordinates)
            result=guard.add(result,guard.mul(pin.diagonal.at(k),guard.mul(value,value)));
        return result;
    }

private:
    const State& state() const {
        if(!state_)throw std::logic_error("moved-from PSD certificate");
        return *state_;
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
