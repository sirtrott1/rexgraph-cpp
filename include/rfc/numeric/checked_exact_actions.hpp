#pragma once

#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/machine/checked_size.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>

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
    void sparse_vector(const SparseRationalVector& v) {
        dimension(v.size());entries(v.nonzeros());
        for(const auto& entry:v.entries()){step();value(entry.value);}
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
// Algebraic transpose action; this is not a metric adjoint.
inline RationalVector checked_transpose_apply(const ExactMatrix& a,const RationalVector& x,ExactActionGuard& guard) {
    if(x.size()!=a.rows())throw std::invalid_argument("checked transpose input dimension");
    guard.matrix(a);const auto input=guard.canonical_input(x);RationalVector out(a.cols());
    for(const auto& [key,q]:a.entries())out[key.second]=guard.add(out[key.second],guard.mul(q,input[key.first]));
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
// Full coordinate bijections: ordered row/column position -> original position.
// They do not reorder the retained source or declare a native carrier transport.
struct ExactSolveOrdering {std::vector<std::size_t> rows,columns;};
namespace exact_order_detail {
struct Permutation {std::vector<std::size_t> inverse;int sign{1};};
inline Permutation validate(const std::vector<std::size_t>& order,std::size_t n,ExactActionGuard& guard) {
    if(order.size()!=n)throw std::invalid_argument("exact ordering must cover every coordinate");
    guard.dimension(n);guard.entries(machine::checked_size_mul(n,3));Permutation result{std::vector<std::size_t>(n,n),1};
    for(std::size_t i=0;i<n;++i){
        guard.step();if(order[i]>=n || result.inverse[order[i]]!=n)throw std::invalid_argument("exact ordering is not a bijection");
        result.inverse[order[i]]=i;
    }
    std::vector<bool> seen(n);std::size_t cycles{};
    for(std::size_t i=0;i<n;++i){
        guard.step();if(!seen[i]){++cycles;auto j=i;do{guard.step();seen[j]=true;j=order[j];}while(!seen[j]);}
    }
    if((n-cycles)%2)result.sign=-1;
    return result;
}
} // namespace exact_order_detail
// Deterministic greedy minimum degree on the symmetrized numerical support.
// Symbolic neighbor completion is an execution heuristic, not source incidence
// or a guarantee about actual numerical fill with subsequent row pivoting.
inline ExactSolveOrdering minimum_degree_ordering(ExactMatrix a,ExactActionGuard& guard) {
    guard.step();if(a.rows()!=a.cols())throw std::invalid_argument("minimum degree requires a square declared map");
    guard.matrix(a);const auto n=a.rows(),fixed=machine::checked_size_mul(n,4);guard.entries(fixed);
    std::vector<std::set<std::size_t>> graph(n);std::vector<bool> active(n,true);std::size_t entries{};
    ExactSolveOrdering result;result.rows.reserve(n);result.columns.reserve(n);
    const auto insert=[&](std::size_t i,std::size_t j){
        guard.step();if(!graph[i].contains(j)){
            const auto next=machine::checked_size_add(entries,1);guard.entries(machine::checked_size_add(fixed,next));
            graph[i].insert(j);entries=next;
        }
    };
    for(const auto& [key,q]:a.entries()){
        (void)q;guard.step();if(key.first!=key.second){insert(key.first,key.second);insert(key.second,key.first);}
    }
    for(std::size_t step=0;step<n;++step){
        std::size_t chosen=n,degree=std::numeric_limits<std::size_t>::max();
        for(std::size_t i=0;i<n;++i){guard.step();if(active[i] && graph[i].size()<degree){chosen=i;degree=graph[i].size();}}
        if(chosen==n)throw std::logic_error("minimum degree active support");
        const std::vector<std::size_t> neighbors(graph[chosen].begin(),graph[chosen].end());
        for(std::size_t i=0;i<neighbors.size();++i)for(std::size_t j=i+1;j<neighbors.size();++j){
            insert(neighbors[i],neighbors[j]);insert(neighbors[j],neighbors[i]);
        }
        for(const auto i:neighbors){guard.step();if(graph[i].erase(chosen)!=1)throw std::logic_error("minimum degree symmetry");--entries;}
        entries-=graph[chosen].size();graph[chosen].clear();active[chosen]=false;result.rows.push_back(chosen);
    }
    if(entries)throw std::logic_error("minimum degree unconsumed support");
    for(const auto i:result.rows){guard.step();result.columns.push_back(i);}
    return result;
}
inline ExactSolveOrdering minimum_degree_ordering(ExactMatrix a,ExactActionLimits limits={},std::function<void()> check={}) {
    ExactActionGuard guard(limits,std::move(check));return minimum_degree_ordering(std::move(a),guard);
}
// Reusable row-pivoted sparse elimination. No inverse is formed. Every saved
// elimination operation and upper factor belongs to one immutable matrix owner.
struct ExactSolvePreparationStats {
    std::size_t pivot_candidates{},eliminated_rows{},index_updates{};
    std::size_t peak_rational_entries{},peak_index_entries{},peak_integer_bits{};
    std::size_t fill_insertions{},exact_cancellations{},upper_factor_entries{},elimination_factor_entries{};
};
class PreparedSelectedExactSolve;
class ExactSolveFactorReceipt;
class PreparedExactSolve {
    struct State {
        ExactMatrix original;
        std::optional<ExactSolveOrdering> ordering;
        int row_sign{1},column_sign{1},pivot_sign{1};
        std::vector<std::map<std::size_t,Rational>> upper;
        struct Pivot {std::size_t row;std::vector<std::pair<std::size_t,Rational>> eliminate;};
        std::vector<Pivot> pivots;
        std::vector<std::vector<std::size_t>> upper_columns;
        std::size_t stored_entries{},stored_index_entries{},preparation_steps{};
        ExactSolvePreparationStats stats;
        State(ExactMatrix a,ExactActionGuard& guard,std::optional<ExactSolveOrdering> requested={})
            :original(std::move(a)),ordering(std::move(requested)) {
            const auto start=guard.steps();
            if(ordering)guard.step();
            if(original.rows()!=original.cols())throw std::invalid_argument("prepared solve requires a square declared operator");
            guard.matrix(original);
            upper.resize(original.rows());
            // Derived execution support only. The original declared map is kept
            // separately, and the same minimum-row-size/tie pivot rule is used.
            std::vector<std::vector<std::size_t>> columns(original.cols());
            const auto ordering_entries=ordering?machine::checked_size_mul(original.rows(),2):0;
            std::size_t upper_entries{},elimination_entries{},index_entries{},ordering_scratch{};
            const auto observe=[&](const Rational& q){
                const auto bits=[](mpz_srcptr z){return mpz_sgn(z)?mpz_sizeinbase(z,2):std::size_t{0};};
                stats.peak_integer_bits=std::max(stats.peak_integer_bits,
                    std::max(bits(mpq_numref(q.get_mpq_t())),bits(mpq_denref(q.get_mpq_t()))));
            };
            const auto account=[&](){
                const auto rational=machine::checked_size_add(original.nonzeros(),
                    machine::checked_size_add(upper_entries,elimination_entries));
                const auto indices=machine::checked_size_add(ordering_entries,machine::checked_size_add(ordering_scratch,index_entries));
                guard.entries(rational);guard.entries(indices);
                stats.peak_rational_entries=std::max(stats.peak_rational_entries,rational);
                stats.peak_index_entries=std::max(stats.peak_index_entries,indices);
            };
            const auto index=[&](std::size_t col,std::size_t row,bool add){
                guard.step();++stats.index_updates;
                auto& members=columns[col];const auto it=std::lower_bound(members.begin(),members.end(),row);
                if(add){
                    if(it!=members.end() && *it==row)throw std::logic_error("duplicate solve support index");
                    index_entries=machine::checked_size_add(index_entries,1);
                    guard.entries(machine::checked_size_add(ordering_entries,machine::checked_size_add(ordering_scratch,index_entries)));
                    members.insert(it,row);
                }else{
                    if(it==members.end() || *it!=row)throw std::logic_error("missing solve support index");
                    members.erase(it);--index_entries;
                }
            };
            {
                std::vector<std::size_t> row_index,column_index;
                if(ordering){
                    const auto peak=machine::checked_size_mul(original.rows(),5);guard.entries(peak);
                    auto rows=exact_order_detail::validate(ordering->rows,original.rows(),guard);
                    auto cols=exact_order_detail::validate(ordering->columns,original.cols(),guard);
                    row_index=std::move(rows.inverse);column_index=std::move(cols.inverse);row_sign=rows.sign;column_sign=cols.sign;
                    ordering_scratch=ordering_entries;stats.peak_index_entries=peak;
                }
                for(const auto& [key,q]:original.entries()){
                    const auto i=ordering?row_index[key.first]:key.first,j=ordering?column_index[key.second]:key.second;
                    guard.step();observe(q);upper[i].emplace(j,q);
                    ++upper_entries;index(j,i,true);account();
                }
            }
            ordering_scratch=0;
            for(std::size_t k=0;k<upper.size();++k) {
                std::size_t pivot=upper.size(),best=std::numeric_limits<std::size_t>::max();
                for(const auto i:columns[k]){
                    guard.step();++stats.pivot_candidates;
                    if(i<k)throw std::logic_error("inactive row in solve support index");
                    if(upper[i].size()<best){pivot=i;best=upper[i].size();}
                }
                if(pivot==upper.size())throw std::invalid_argument("singular operator in admitted exact solve profile");
                pivots.push_back({pivot,{}});
                if(pivot!=k){
                    pivot_sign=-pivot_sign;
                    for(const auto row:{k,pivot})for(const auto& [j,q]:upper[row]){(void)q;index(j,row,false);}
                    std::swap(upper[k],upper[pivot]);
                    for(const auto row:{k,pivot})for(const auto& [j,q]:upper[row]){(void)q;index(j,row,true);}
                }
                for(const auto& [j,q]:upper[k]){(void)q;index(j,k,false);}
                // Snapshot because removing k and later fill/cancellation update
                // column membership while these actual rows are eliminated.
                const auto targets=columns[k];guard.dimension(targets.size());
                for(const auto i:targets) {
                    guard.step();++stats.eliminated_rows;
                    const auto ratio=guard.div(upper[i].at(k),upper[k].at(k));observe(ratio);
                    pivots.back().eliminate.emplace_back(i,ratio);++elimination_entries;
                    upper[i].erase(k);--upper_entries;index(k,i,false);
                    for(const auto& [j,q]:upper[k])if(j>k) {
                        const auto found=upper[i].find(j);const Rational old=found==upper[i].end()?Rational{0}:found->second;
                        const auto product=guard.mul(ratio,q);observe(product);
                        auto updated=guard.sub(old,product);observe(updated);
                        if(updated==0){
                            if(found!=upper[i].end()){++stats.exact_cancellations;upper[i].erase(found);--upper_entries;index(j,i,false);}
                        }else{
                            if(found==upper[i].end()){++stats.fill_insertions;++upper_entries;index(j,i,true);}
                            upper[i][j]=std::move(updated);
                        }
                        account();
                    }
                    account();
                }
            }
            if(index_entries)throw std::logic_error("unconsumed solve support index");
            upper_columns.resize(upper.size());
            for(std::size_t i=0;i<upper.size();++i)for(const auto& [j,q]:upper[i])if(j>i){
                (void)q;guard.step();upper_columns[j].push_back(i);++index_entries;account();
            }
            stored_entries=machine::checked_size_add(original.nonzeros(),
                machine::checked_size_add(upper_entries,elimination_entries));
            stored_index_entries=machine::checked_size_add(ordering_entries,index_entries);
            stats.upper_factor_entries=upper_entries;stats.elimination_factor_entries=elimination_entries;
            guard.entries(stored_entries);preparation_steps=guard.steps()-start;
        }
    };
public:
    PreparedExactSolve(const PreparedExactSolve&)=default;
    PreparedExactSolve& operator=(const PreparedExactSolve&)=default;
    PreparedExactSolve(ExactMatrix a,ExactActionGuard& guard):state_(std::make_shared<const State>(std::move(a),guard)){}
    explicit PreparedExactSolve(ExactMatrix a,ExactActionLimits limits={},std::function<void()> check={}) {
        ExactActionGuard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(a),guard);
    }
    static PreparedExactSolve prepare_ordered(ExactMatrix a,ExactSolveOrdering order,ExactActionGuard& guard) {
        return PreparedExactSolve(std::make_shared<const State>(std::move(a),guard,std::move(order)));
    }
    static PreparedExactSolve prepare_ordered(ExactMatrix a,ExactSolveOrdering order,ExactActionLimits limits={},
                                              std::function<void()> check={}) {
        ExactActionGuard guard(limits,std::move(check));return prepare_ordered(std::move(a),std::move(order),guard);
    }
    RationalVector apply(const RationalVector& rhs,ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();
        if(rhs.size()!=pin->upper.size())throw std::invalid_argument("prepared solve right-hand side dimension");
        validate(*pin,guard);const auto input=guard.canonical_input(rhs);
        auto b=input;
        if(pin->ordering)for(std::size_t i=0;i<b.size();++i){guard.step();b[i]=input[pin->ordering->rows[i]];}
        for(std::size_t k=0;k<b.size();++k) {
            const auto& p=pin->pivots[k];if(p.row!=k)std::swap(b[k],b[p.row]);
            for(const auto& [i,ratio]:p.eliminate){guard.value(ratio);b[i]=guard.sub(b[i],guard.mul(ratio,b[k]));}
        }
        RationalVector x(b.size());
        for(std::size_t i=b.size();i-- >0;) {
            Rational sum;
            for(const auto& [j,q]:pin->upper[i])if(j>i)sum=guard.add(sum,guard.mul(q,x[j]));
            x[i]=guard.div(guard.sub(b[i],sum),pin->upper[i].at(i));
        }
        if(pin->ordering){auto ordered=x;for(std::size_t i=0;i<x.size();++i){guard.step();x[pin->ordering->columns[i]]=std::move(ordered[i]);}}
        if(checked_apply(pin->original,x,guard)!=input)throw std::logic_error("prepared solve has nonzero exact residual");
        return x;
    }
    RationalVector apply_transpose(const RationalVector& rhs,ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();
        if(rhs.size()!=pin->upper.size())throw std::invalid_argument("prepared transpose right-hand side dimension");
        validate(*pin,guard);const auto input=guard.canonical_input(rhs);RationalVector b,z(input.size());
        if(pin->ordering){b.resize(input.size());for(std::size_t i=0;i<b.size();++i){guard.step();b[i]=input[pin->ordering->columns[i]];}}
        const auto& ordered_input=pin->ordering?b:input;
        // U=T A. First U^T w=rhs, then z=T^T w. Reverse every
        // elimination before reversing its row swap; their order matters.
        for(std::size_t i=0;i<z.size();++i){
            Rational sum;
            for(const auto j:pin->upper_columns[i])sum=guard.add(sum,guard.mul(pin->upper[j].at(i),z[j]));
            z[i]=guard.div(guard.sub(ordered_input[i],sum),pin->upper[i].at(i));
        }
        for(std::size_t k=z.size();k-- >0;){
            const auto& p=pin->pivots[k];
            for(const auto& [i,ratio]:p.eliminate)z[k]=guard.sub(z[k],guard.mul(ratio,z[i]));
            if(p.row!=k)std::swap(z[k],z[p.row]);
        }
        if(pin->ordering){auto ordered=z;for(std::size_t i=0;i<z.size();++i){guard.step();z[pin->ordering->rows[i]]=std::move(ordered[i]);}}
        if(checked_transpose_apply(pin->original,z,guard)!=input)throw std::logic_error("prepared transpose has nonzero exact residual");
        return z;
    }
    RationalVector apply_transpose(const RationalVector& rhs,ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return apply_transpose(rhs,guard);
    }
    RationalVector apply(const RationalVector& rhs,ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return apply(rhs,guard);
    }
    const ExactMatrix& matrix() const & {return state_->original;}
    const ExactMatrix& matrix() const &&=delete;
    std::size_t stored_entries() const {return state_->stored_entries;}
    std::size_t stored_index_entries() const noexcept {return state_->stored_index_entries;}
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    ExactSolvePreparationStats preparation_stats() const noexcept {return state_->stats;}
    ExactSolveFactorReceipt factor_receipt(ExactActionGuard& guard) const;
    ExactSolveFactorReceipt factor_receipt(ExactActionLimits limits={},std::function<void()> check={}) const;
    PreparedSelectedExactSolve prepare_selected(std::vector<std::size_t> outputs,ExactActionGuard& guard) const;
    PreparedSelectedExactSolve prepare_selected(std::vector<std::size_t> outputs,ExactActionLimits limits={},
                                                std::function<void()> check={}) const;
private:
    friend class PreparedSelectedExactSolve;
    friend class ExactSolveFactorReceipt;
    explicit PreparedExactSolve(std::shared_ptr<const State> state):state_(std::move(state)){}
    static void validate(const State& state,ExactActionGuard& guard) {
        guard.entries(state.stored_entries);guard.entries(state.stored_index_entries);guard.matrix(state.original);
        if(state.ordering){
            for(const auto* order:{&state.ordering->rows,&state.ordering->columns}){
                guard.dimension(order->size());for(const auto i:*order){guard.step();if(i>=state.original.rows())throw std::logic_error("retained ordering coordinate");}
            }
        }
        for(const auto& row:state.upper)for(const auto& [j,q]:row){(void)j;guard.step();guard.value(q);}
        for(const auto& p:state.pivots)for(const auto& [i,q]:p.eliminate){(void)i;guard.step();guard.value(q);}
    }
    void validate(ExactActionGuard& guard) const {
        const auto pin=state_;validate(*pin,guard);
    }
    std::shared_ptr<const State> state_;
};
// Inspection/observations stay bound to the actual original operator and factor
// owner. The determinant is computed on request, under current bit/work limits.
class ExactSolveFactorReceipt {
public:
    const ExactMatrix& matrix() const & {return state_->original;}
    const ExactMatrix& matrix() const &&=delete;
    std::size_t size() const noexcept {return state_->original.rows();}
    std::size_t row_coordinate(std::size_t i) const {check(i);return state_->ordering?state_->ordering->rows[i]:i;}
    std::size_t column_coordinate(std::size_t i) const {check(i);return state_->ordering?state_->ordering->columns[i]:i;}
    std::size_t pivot_row_slot(std::size_t i) const {check(i);return state_->pivots[i].row;}
    Rational pivot_value(std::size_t i) const {check(i);return state_->upper[i].at(i);}
    int row_order_sign() const noexcept {return state_->row_sign;}
    int column_order_sign() const noexcept {return state_->column_sign;}
    int pivot_swap_sign() const noexcept {return state_->pivot_sign;}
    bool has_declared_ordering() const noexcept {return state_->ordering.has_value();}
    Rational determinant(ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();PreparedExactSolve::validate(*pin,guard);
        Rational value{pin->row_sign*pin->column_sign*pin->pivot_sign};guard.value(value);
        for(std::size_t i=0;i<pin->upper.size();++i)value=guard.mul(value,pin->upper[i].at(i));
        return value;
    }
    Rational determinant(ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return determinant(guard);
    }
private:
    friend class PreparedExactSolve;
    explicit ExactSolveFactorReceipt(std::shared_ptr<const PreparedExactSolve::State> state):state_(std::move(state)){}
    void check(std::size_t i) const {if(i>=size())throw std::out_of_range("factor receipt coordinate");}
    std::shared_ptr<const PreparedExactSolve::State> state_;
};
inline ExactSolveFactorReceipt PreparedExactSolve::factor_receipt(ExactActionGuard& guard) const {
    const auto pin=state_;guard.step();validate(*pin,guard);return ExactSolveFactorReceipt(pin);
}
inline ExactSolveFactorReceipt PreparedExactSolve::factor_receipt(ExactActionLimits limits,std::function<void()> check) const {
    ExactActionGuard guard(limits,std::move(check));return factor_receipt(guard);
}
// Explicit coordinate readout on this owned square operator, not a new native
// carrier or a source restriction. Repeated output coordinates keep their order.
class PreparedSelectedExactSolve {
    struct State {
        PreparedExactSolve solve;
        std::vector<std::size_t> outputs,unique;
        std::vector<std::vector<std::pair<std::size_t,Rational>>> duals;
        std::size_t retained_entries{},preparation_steps{};
        bool full{};
        State(PreparedExactSolve owner,std::vector<std::size_t> requested,ExactActionGuard& guard)
            :solve(std::move(owner)),outputs(std::move(requested)){
            const auto start=guard.steps();solve.validate(guard);guard.dimension(outputs.size());
            for(const auto i:outputs){guard.step();if(i>=solve.matrix().cols())throw std::out_of_range("selected solve output coordinate");}
            unique=outputs;std::sort(unique.begin(),unique.end());unique.erase(std::unique(unique.begin(),unique.end()),unique.end());
            full=unique.size()==solve.matrix().cols();retained_entries=solve.stored_entries();
            guard.entries(outputs.size());guard.entries(unique.size());
            // A complete output request uses the existing primal solve, so the
            // selection path cannot silently prepare a complete inverse matrix.
            if(!full)for(const auto i:unique){
                RationalVector basis(solve.matrix().rows());basis[i]=1;
                const auto z=solve.apply_transpose(basis,guard);
                duals.emplace_back();
                for(std::size_t j=0;j<z.size();++j)if(z[j]!=0){
                    guard.step();retained_entries=machine::checked_size_add(retained_entries,1);guard.entries(retained_entries);
                    duals.back().emplace_back(j,z[j]);
                }
            }
            preparation_steps=guard.steps()-start;
        }
    };
public:
    PreparedSelectedExactSolve(const PreparedSelectedExactSolve&)=default;
    PreparedSelectedExactSolve& operator=(const PreparedSelectedExactSolve&)=default;
    const std::vector<std::size_t>& output_coordinates() const & {return state_->outputs;}
    const std::vector<std::size_t>& output_coordinates() const &&=delete;
    std::size_t input_size() const noexcept {return state_->solve.matrix().rows();}
    std::size_t stored_entries() const noexcept {return state_->retained_entries;}
    const PreparedExactSolve& solve_owner() const & {return state_->solve;}
    const PreparedExactSolve& solve_owner() const &&=delete;
    // Tracked factor indexes, both selection words and sparse dual coordinates.
    // This does not measure map nodes, container headers or process memory.
    std::size_t stored_index_entries() const {
        auto count=machine::checked_size_add(state_->solve.stored_index_entries(),
            machine::checked_size_add(state_->outputs.size(),state_->unique.size()));
        for(const auto& dual:state_->duals)count=machine::checked_size_add(count,dual.size());
        return count;
    }
    std::size_t preparation_steps() const noexcept {return state_->preparation_steps;}
    bool uses_full_solve() const noexcept {return state_->full;}
    RationalVector apply(const RationalVector& rhs,ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();
        if(rhs.size()!=pin->solve.matrix().rows())throw std::invalid_argument("selected solve right-hand side dimension");
        pin->solve.validate(guard);guard.dimension(pin->outputs.size());guard.entries(pin->retained_entries);
        guard.entries(pin->outputs.size());guard.entries(pin->unique.size());
        const auto input=guard.canonical_input(rhs);RationalVector values;
        if(pin->full)values=pin->solve.apply(input,guard);
        else for(std::size_t col=0;col<pin->unique.size();++col){
            RationalVector z(input.size());
            for(const auto& [j,q]:pin->duals[col]){guard.step();guard.value(q);z[j]=q;}
            const auto witness=checked_transpose_apply(pin->solve.matrix(),z,guard);
            for(std::size_t j=0;j<witness.size();++j){
                guard.step();if(witness[j]!=Rational{j==pin->unique[col]?1:0})throw std::logic_error("selected dual certificate residual");
            }
            Rational result;
            for(const auto& [j,q]:pin->duals[col])result=guard.add(result,guard.mul(q,input[j]));
            values.push_back(std::move(result));
        }
        RationalVector out;out.reserve(pin->outputs.size());
        for(const auto i:pin->outputs){
            guard.step();const auto slot=pin->full?i:static_cast<std::size_t>(std::lower_bound(pin->unique.begin(),pin->unique.end(),i)-pin->unique.begin());
            out.push_back(values[slot]);
        }
        return out;
    }
    RationalVector apply(const RationalVector& rhs,ExactActionLimits limits={},std::function<void()> check={}) const {
        ExactActionGuard guard(limits,std::move(check));return apply(rhs,guard);
    }
private:
    friend class PreparedExactSolve;
    PreparedSelectedExactSolve(PreparedExactSolve owner,std::vector<std::size_t> outputs,ExactActionGuard& guard)
        :state_(std::make_shared<const State>(std::move(owner),std::move(outputs),guard)){}
    std::shared_ptr<const State> state_;
};
inline PreparedSelectedExactSolve PreparedExactSolve::prepare_selected(std::vector<std::size_t> outputs,ExactActionGuard& guard) const {
    return PreparedSelectedExactSolve(*this,std::move(outputs),guard);
}
inline PreparedSelectedExactSolve PreparedExactSolve::prepare_selected(std::vector<std::size_t> outputs,ExactActionLimits limits,
                                                                      std::function<void()> check) const {
    ExactActionGuard guard(limits,std::move(check));return prepare_selected(std::move(outputs),guard);
}
} // namespace rfc::numeric
