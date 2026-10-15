#pragma once
#include "rfc/numeric/checked_exact_actions.hpp"

namespace rfc::numeric {
// Complete output S solve(A,b), retaining the actual original factor owner.
// Equal complete coefficient rows may share work without merging output axes.
class PreparedExactLinearObservation {
    using Row=std::vector<std::pair<std::size_t,Rational>>;
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t workspace(std::size_t n,std::size_t q,std::size_t distinct){
        return add(machine::checked_size_mul(n,8),add(q,distinct));
    }
    static bool equal(const Row& a,const Row& b,ExactActionGuard& guard){
        guard.step();if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i){guard.step();if(a[i]!=b[i])return false;}
        return true;
    }
    struct State {
        PreparedExactSolve solve;
        ExactMatrix accession;
        std::vector<Row> rows,duals;
        std::vector<std::size_t> word;
        std::size_t retained{},indices{},preparation_steps{},nonzero_rows{};
        bool full{};
        State(PreparedExactSolve owner,ExactMatrix map,ExactActionGuard& guard)
            :solve(std::move(owner)),accession(std::move(map)){
            const auto start=guard.steps();guard.step();
            if(accession.cols()!=solve.matrix().rows())throw std::invalid_argument("linear observation input dimension");
            (void)solve.factor_receipt(guard);guard.matrix(accession);
            retained=add(solve.stored_entries(),accession.nonzeros());indices=solve.stored_index_entries();
            guard.entries(retained);guard.entries(indices);
            auto next=accession.entries().begin();
            for(std::size_t i=0;i<accession.rows();++i){
                guard.step();Row row;
                while(next!=accession.entries().end() && next->first.first==i){
                    guard.step();row.emplace_back(next->first.second,next->second);++next;
                    guard.entries(add(retained,row.size()));guard.entries(add(indices,row.size()));
                }
                auto slot=rows.size();
                for(std::size_t j=0;j<rows.size();++j)if(equal(row,rows[j],guard)){slot=j;break;}
                if(slot==rows.size()){
                    if(!row.empty())++nonzero_rows;
                    retained=add(retained,row.size());indices=add(indices,row.size());rows.push_back(std::move(row));
                }
                word.push_back(slot);indices=add(indices,1);guard.entries(retained);guard.entries(indices);
            }
            // This row-count policy avoids preparing a complete inverse when
            // the request has at least n distinct nonzero coefficient rows.
            full=nonzero_rows>=solve.matrix().rows();
            const auto scratch=workspace(solve.matrix().rows(),accession.rows(),rows.size());
            guard.entries(add(retained,scratch));
            if(!full)for(const auto& row:rows){
                guard.step();duals.emplace_back();if(row.empty())continue;
                RationalVector rhs(solve.matrix().rows());for(const auto& [j,q]:row){guard.step();rhs[j]=q;}
                const auto z=solve.apply_transpose(rhs,guard);
                for(std::size_t j=0;j<z.size();++j)if(z[j]!=0){
                    guard.step();duals.back().emplace_back(j,z[j]);retained=add(retained,1);indices=add(indices,1);
                    guard.entries(add(retained,scratch));guard.entries(indices);
                }
            }
            preparation_steps=guard.steps()-start;
        }
    };
public:
    PreparedExactLinearObservation(const PreparedExactLinearObservation&)=default;
    PreparedExactLinearObservation& operator=(const PreparedExactLinearObservation&)=default;
    static PreparedExactLinearObservation prepare(PreparedExactSolve solve,ExactMatrix accession,ExactActionGuard& guard){
        return PreparedExactLinearObservation(std::make_shared<const State>(std::move(solve),std::move(accession),guard));
    }
    static PreparedExactLinearObservation prepare(PreparedExactSolve solve,ExactMatrix accession,ExactActionLimits limits={},
                                                 std::function<void()> check={}){
        ExactActionGuard guard(limits,std::move(check));return prepare(std::move(solve),std::move(accession),guard);
    }
    const PreparedExactSolve& solve_owner() const & {return state_->solve;}
    const PreparedExactSolve& solve_owner() const &&=delete;
    const ExactMatrix& accession() const & {return state_->accession;}
    const ExactMatrix& accession() const &&=delete;
    const std::vector<std::size_t>& row_reuse_word() const & {return state_->word;}
    const std::vector<std::size_t>& row_reuse_word() const &&=delete;
    std::size_t distinct_rows() const noexcept {return state_->rows.size();}
    std::size_t certified_nonzero_rows() const noexcept {return state_->full?0:state_->nonzero_rows;}
    bool uses_full_solve() const noexcept {return state_->full;}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t stored_index_entries() const noexcept {return state_->indices;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    // Named vector-slot envelope; excludes container headers and GMP storage.
    std::size_t workspace_slots() const {
        const auto pin=state_;return workspace(pin->solve.matrix().rows(),pin->accession.rows(),pin->rows.size());
    }
    RationalVector apply(RationalVector rhs,ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();
        if(rhs.size()!=pin->solve.matrix().rows())throw std::invalid_argument("linear observation right-hand side dimension");
        (void)pin->solve.factor_receipt(guard);guard.matrix(pin->accession);
        guard.entries(add(pin->retained,workspace(rhs.size(),pin->accession.rows(),pin->rows.size())));
        guard.entries(pin->indices);
        for(const auto& row:pin->rows)for(const auto& [j,q]:row){(void)j;guard.step();guard.value(q);}
        for(const auto& dual:pin->duals)for(const auto& [j,q]:dual){(void)j;guard.step();guard.value(q);}
        const auto input=guard.canonical_input(rhs);
        if(pin->full){
            auto out=checked_apply(pin->accession,pin->solve.apply(input,guard),guard);guard.vector(out);return out;
        }
        RationalVector readings;readings.reserve(pin->rows.size());
        for(std::size_t i=0;i<pin->rows.size();++i){
            RationalVector z(input.size());for(const auto& [j,q]:pin->duals[i]){guard.step();z[j]=q;}
            const auto witness=checked_transpose_apply(pin->solve.matrix(),z,guard);
            auto next=pin->rows[i].begin();
            for(std::size_t j=0;j<witness.size();++j){
                guard.step();Rational expected;
                if(next!=pin->rows[i].end() && next->first==j){expected=next->second;++next;}
                if(witness[j]!=expected)throw std::logic_error("linear observation transpose certificate residual");
            }
            Rational value;for(const auto& [j,q]:pin->duals[i])value=guard.add(value,guard.mul(q,input[j]));
            readings.push_back(std::move(value));
        }
        RationalVector out;out.reserve(pin->word.size());
        for(const auto row:pin->word){guard.step();guard.value(readings[row]);out.push_back(readings[row]);}
        return out;
    }
    RationalVector apply(RationalVector rhs,ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return apply(std::move(rhs),guard);
    }
private:
    explicit PreparedExactLinearObservation(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
