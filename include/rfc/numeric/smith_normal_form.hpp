#pragma once
#include "rfc/numeric/integer_matrix.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

// A finite exact unimodular action.  Recording or applying it never constructs
// an identity matrix or a complete inverse.  The operation sequence is in
// application order: its represented matrix is R_last ... R_first.
class IntegerUnimodularAction {
public:
    enum class Kind { Swap, Add, Negate };
    struct Operation { Kind kind; std::size_t target; std::size_t source; Integer multiple; };

    IntegerUnimodularAction() = default;
    explicit IntegerUnimodularAction(std::size_t dimension) : dimension_(dimension) {}
    [[nodiscard]] std::size_t dimension() const noexcept { return dimension_; }
    [[nodiscard]] const std::vector<Operation>& operations() const noexcept { return operations_; }

    void swap_rows(std::size_t a, std::size_t b) {
        check(a); check(b);
        if (a != b) operations_.push_back({Kind::Swap, a, b, 0});
    }
    void add_row_multiple(std::size_t target, std::size_t source, Integer multiple) {
        check(target); check(source);
        if (multiple == 0) return;
        if (target == source) throw std::invalid_argument("unimodular row addition requires distinct rows");
        operations_.push_back({Kind::Add, target, source, std::move(multiple)});
    }
    void negate_row(std::size_t row) { check(row); operations_.push_back({Kind::Negate, row, row, 0}); }

    [[nodiscard]] IntegerMatrix apply(const IntegerMatrix& input) const {
        if (input.rows() != dimension_) throw std::invalid_argument("unimodular matrix action shape mismatch");
        auto out = input;
        for (const auto& op : operations_) apply_operation(out, op, false);
        out.prune_zeros();
        return out;
    }
    [[nodiscard]] IntegerMatrix apply_inverse(const IntegerMatrix& input) const {
        if (input.rows() != dimension_) throw std::invalid_argument("inverse unimodular action shape mismatch");
        auto out = input;
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) apply_operation(out, *it, true);
        out.prune_zeros();
        return out;
    }
    // If this action represents W, return input*W^{-1}.  Right multiplication
    // transposes the row roles, and uses inverse factors in recording order.
    [[nodiscard]] IntegerMatrix apply_inverse_on_right(const IntegerMatrix& input) const {
        if (input.cols() != dimension_) throw std::invalid_argument("right unimodular action shape mismatch");
        auto out = input;
        for (const auto& op : operations_) {
            if (op.kind == Kind::Swap) out.swap_cols(op.target, op.source);
            else if (op.kind == Kind::Negate) out.col_scale(op.target, Integer{-1});
            else out.col_add_multiple(op.source, op.target, -op.multiple);
        }
        out.prune_zeros();
        return out;
    }
    [[nodiscard]] std::vector<Integer> apply(const std::vector<Integer>& input) const {
        if (input.size() != dimension_) throw std::invalid_argument("unimodular vector action shape mismatch");
        auto out = input;
        for (const auto& op : operations_) apply_operation(out, op, false);
        return out;
    }
    [[nodiscard]] std::vector<Integer> apply_inverse(const std::vector<Integer>& input) const {
        if (input.size() != dimension_) throw std::invalid_argument("inverse unimodular vector action shape mismatch");
        auto out = input;
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) apply_operation(out, *it, true);
        return out;
    }
    [[nodiscard]] IntegerMatrix::SparseVector apply_sparse(const IntegerMatrix::SparseVector& input) const {
        auto out = checked_sparse_copy(input);
        for (const auto& op : operations_) apply_operation(out, op, false);
        return out;
    }
    [[nodiscard]] IntegerMatrix::SparseVector apply_inverse_sparse(const IntegerMatrix::SparseVector& input) const {
        auto out = checked_sparse_copy(input);
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) apply_operation(out, *it, true);
        return out;
    }
    [[nodiscard]] IntegerMatrix apply(const IntegerMatrix& input,ExactActionGuard& guard) const {
        return checked_replay(input,false,false,guard);
    }
    [[nodiscard]] IntegerMatrix apply_inverse(const IntegerMatrix& input,ExactActionGuard& guard) const {
        return checked_replay(input,true,false,guard);
    }
    [[nodiscard]] IntegerMatrix apply_inverse_on_right(const IntegerMatrix& input,ExactActionGuard& guard) const {
        return checked_replay(input,true,true,guard);
    }
    [[nodiscard]] IntegerMatrix::SparseVector apply_sparse(const IntegerMatrix::SparseVector& input,ExactActionGuard& guard) const {
        return checked_sparse_replay(input,false,guard);
    }
    [[nodiscard]] IntegerMatrix::SparseVector apply_inverse_sparse(const IntegerMatrix::SparseVector& input,ExactActionGuard& guard) const {
        return checked_sparse_replay(input,true,guard);
    }
    // The same integral change of basis can act on selected rational columns
    // directly. No common denominator, full U/V, or inverse is materialized.
    [[nodiscard]] ExactMatrix apply_rational(const ExactMatrix& input,ExactActionGuard& guard) const {
        return checked_rational_replay(input,false,guard);
    }
    [[nodiscard]] ExactMatrix apply_inverse_rational(const ExactMatrix& input,ExactActionGuard& guard) const {
        return checked_rational_replay(input,true,guard);
    }

private:
    [[nodiscard]] ExactMatrix checked_rational_replay(const ExactMatrix& input,bool inverse,ExactActionGuard& guard) const {
        if(input.rows()!=dimension_)throw std::invalid_argument("guarded rational unimodular action shape mismatch");
        guard.step();guard.matrix(input);guard.entries(machine::checked_size_add(operations_.size(),machine::checked_size_mul(input.nonzeros(),2)));
        auto out=input;
        const auto perform=[&](const Operation& op){
            guard.step();guard.value(Rational{op.multiple});
            if(op.kind==Kind::Swap){
                guard.entries(machine::checked_size_mul(out.nonzeros(),2));ExactMatrix changed(out.rows(),out.cols());
                for(const auto& [key,value]:out.entries()){
                    guard.step();const auto row=key.first==op.target?op.source:key.first==op.source?op.target:key.first;
                    changed.set(row,key.second,value);
                }
                out=std::move(changed);
            }else if(op.kind==Kind::Negate){
                for(auto it=out.entries().lower_bound(ExactMatrix::Key{op.target,0});it!=out.entries().end()&&it->first.first==op.target;++it)
                    out.set(op.target,it->first.second,guard.sub(Rational{0},it->second));
            }else{
                const Rational multiple=inverse?Rational{-op.multiple}:Rational{op.multiple};
                // Source and target rows are distinct. Insertions/removals in
                // the target never invalidate these source-row map iterators.
                for(auto it=out.entries().lower_bound(ExactMatrix::Key{op.source,0});it!=out.entries().end()&&it->first.first==op.source;++it){
                    const auto col=it->first.second;const auto product=guard.mul(multiple,it->second);
                    const auto sum=guard.add(out.get(op.target,col),product);
                    const auto added=sum!=0&&out.get(op.target,col)==0?1u:0u;
                    guard.entries(machine::checked_size_add(input.nonzeros(),machine::checked_size_add(out.nonzeros(),added)));
                    out.set(op.target,col,sum);
                }
            }
            guard.entries(machine::checked_size_add(operations_.size(),machine::checked_size_add(input.nonzeros(),out.nonzeros())));
        };
        if(inverse)for(auto it=operations_.rbegin();it!=operations_.rend();++it)perform(*it);
        else for(const auto& op:operations_)perform(op);
        return out;
    }
    [[nodiscard]] IntegerMatrix checked_replay(const IntegerMatrix& input,bool inverse,bool right,ExactActionGuard& guard) const {
        if((right?input.cols():input.rows())!=dimension_)throw std::invalid_argument("guarded unimodular action shape mismatch");
        guard.step();input.validate(guard);guard.entries(machine::checked_size_add(operations_.size(),machine::checked_size_mul(input.stored_entries(),2)));
        auto out=input;
        const auto perform=[&](const Operation& op){
            guard.step();guard.value(Rational{op.multiple});
            if(right){
                if(op.kind==Kind::Swap)out.swap_cols(op.target,op.source,guard);
                else if(op.kind==Kind::Negate)out.col_scale(op.target,Integer{-1},guard);
                else out.col_add_multiple(op.source,op.target,-op.multiple,guard);
            }else{
                if(op.kind==Kind::Swap)out.swap_rows(op.target,op.source,guard);
                else if(op.kind==Kind::Negate)out.row_scale(op.target,Integer{-1},guard);
                else {Integer multiple=op.multiple;if(inverse)multiple=-multiple;out.row_add_multiple(op.target,op.source,multiple,guard);}
            }
            guard.entries(machine::checked_size_add(operations_.size(),machine::checked_size_add(input.stored_entries(),out.stored_entries())));
        };
        if(inverse&&!right)for(auto it=operations_.rbegin();it!=operations_.rend();++it)perform(*it);
        else for(const auto& op:operations_)perform(op);
        out.prune_zeros();return out;
    }
    [[nodiscard]] IntegerMatrix::SparseVector checked_sparse_replay(const IntegerMatrix::SparseVector& input,bool inverse,ExactActionGuard& guard) const {
        guard.dimension(dimension_);guard.entries(input.size());IntegerMatrix column(dimension_,1);
        for(const auto& [index,value]:input){guard.step();check(index);guard.value(Rational{value});column.set(index,0,value);}
        const auto result=inverse?apply_inverse(column,guard):apply(column,guard);
        IntegerMatrix::SparseVector out;
        for(const auto& [ij,value]:result.entries()){guard.step();out.emplace(ij.first,value);}
        return out;
    }
    void check(std::size_t index) const {
        if (index >= dimension_) throw std::out_of_range("unimodular action index");
    }
    [[nodiscard]] IntegerMatrix::SparseVector checked_sparse_copy(const IntegerMatrix::SparseVector& input) const {
        IntegerMatrix::SparseVector out;
        for (const auto& [index, value] : input) { check(index); if (value != 0) out.emplace(index, value); }
        return out;
    }
    static void apply_operation(IntegerMatrix& out, const Operation& op, bool inverse) {
        if (op.kind == Kind::Swap) out.swap_rows(op.target, op.source);
        else if (op.kind == Kind::Negate) out.row_scale(op.target, Integer{-1});
        else {
            Integer multiple = op.multiple;
            if (inverse) multiple = -multiple;
            out.row_add_multiple(op.target, op.source, multiple);
        }
    }
    static void apply_operation(std::vector<Integer>& out, const Operation& op, bool inverse) {
        if (op.kind == Kind::Swap) std::swap(out[op.target], out[op.source]);
        else if (op.kind == Kind::Negate) out[op.target] = -out[op.target];
        else if (inverse) out[op.target] -= op.multiple * out[op.source];
        else out[op.target] += op.multiple * out[op.source];
    }
    static Integer sparse_value(const IntegerMatrix::SparseVector& input, std::size_t index) {
        const auto it = input.find(index);
        return it == input.end() ? Integer{0} : it->second;
    }
    static void sparse_set(IntegerMatrix::SparseVector& out, std::size_t index, Integer value) {
        if (value == 0) out.erase(index);
        else out.insert_or_assign(index, std::move(value));
    }
    static void apply_operation(IntegerMatrix::SparseVector& out, const Operation& op, bool inverse) {
        const Integer target = sparse_value(out, op.target);
        if (op.kind == Kind::Negate) sparse_set(out, op.target, -target);
        else {
            const Integer source = sparse_value(out, op.source);
            if (op.kind == Kind::Swap) {
                sparse_set(out, op.target, source);
                sparse_set(out, op.source, target);
            } else {
                Integer updated = op.multiple * source;
                if (inverse) updated = -updated;
                sparse_set(out, op.target, target + updated);
            }
        }
    }

    std::size_t dimension_{};
    std::vector<Operation> operations_;
};

struct SmithNormalFormActions {
    IntegerMatrix diagonal;
    IntegerUnimodularAction left;
    IntegerUnimodularAction right_inverse;
    std::size_t rank{};
    std::vector<Integer> invariant_factors;

    // Selected columns of V, where U*input*V=diagonal.  This constructs only
    // the requested frame; it does not materialize the other basis columns.
    [[nodiscard]] IntegerMatrix right_columns(std::size_t begin, std::size_t count) const {
        const auto n = right_inverse.dimension();
        if (begin > n || count > n - begin) throw std::out_of_range("Smith right-column range");
        IntegerMatrix selector(n, count);
        for (std::size_t j = 0; j < count; ++j) selector.set(begin + j, j, Integer{1});
        return right_inverse.apply_inverse(selector);
    }
    [[nodiscard]] IntegerMatrix right_columns(std::size_t begin,std::size_t count,ExactActionGuard& guard) const {
        const auto n=right_inverse.dimension();
        if(begin>n||count>n-begin)throw std::out_of_range("guarded Smith right-column range");
        guard.dimension(n);guard.dimension(count);guard.entries(count);IntegerMatrix selector(n,count);
        for(std::size_t j=0;j<count;++j){guard.step();selector.set(begin+j,j,Integer{1});}
        return right_inverse.apply_inverse(selector,guard);
    }
};

// Compatibility result for callers explicitly requesting complete U and V.
// Each matrix is sparse; action-only consumers use smith_normal_form_actions.
struct SmithNormalForm {
    IntegerMatrix diagonal;
    IntegerMatrix left;
    IntegerMatrix right;
    std::size_t rank{};
    std::vector<Integer> invariant_factors;
};

namespace detail {

inline void action_capacity(const IntegerUnimodularAction& action,ExactActionGuard* guard){
    if(guard)guard->entries(machine::checked_size_add(action.operations().size(),1));
}
inline void row_swap(IntegerMatrix& a, IntegerUnimodularAction& u, std::size_t i, std::size_t j,ExactActionGuard* guard=nullptr) {
    action_capacity(u,guard);if(guard)a.swap_rows(i,j,*guard);else a.swap_rows(i,j);u.swap_rows(i,j);
}
inline void col_swap(IntegerMatrix& a, IntegerUnimodularAction& v_inverse, std::size_t i, std::size_t j,ExactActionGuard* guard=nullptr) {
    action_capacity(v_inverse,guard);if(guard)a.swap_cols(i,j,*guard);else a.swap_cols(i,j);v_inverse.swap_rows(i,j);
}
inline void row_add(IntegerMatrix& a, IntegerUnimodularAction& u, std::size_t target, std::size_t source, const Integer& q,ExactActionGuard* guard=nullptr) {
    action_capacity(u,guard);if(guard)a.row_add_multiple(target,source,q,*guard);else a.row_add_multiple(target,source,q);u.add_row_multiple(target,source,q);
}
inline void col_add(IntegerMatrix& a, IntegerUnimodularAction& v_inverse, std::size_t target, std::size_t source, const Integer& q,ExactActionGuard* guard=nullptr) {
    action_capacity(v_inverse,guard);if(guard)a.col_add_multiple(target,source,q,*guard);else a.col_add_multiple(target,source,q);
    // A <- A*C, V <- V*C, hence V^{-1} <- C^{-1}*V^{-1}.
    v_inverse.add_row_multiple(source, target, -q);
}
inline void row_negate(IntegerMatrix& a, IntegerUnimodularAction& u, std::size_t row,ExactActionGuard* guard=nullptr) {
    action_capacity(u,guard);if(guard)a.row_scale(row,Integer{-1},*guard);else a.row_scale(row,Integer{-1});u.negate_row(row);
}

inline void zero_out_pivot_row_col(IntegerMatrix& a, IntegerUnimodularAction& u,
                                   IntegerUnimodularAction& v_inverse, std::size_t k,ExactActionGuard* guard=nullptr) {
    // Euclidean row reductions on column k.
    std::vector<std::size_t> rows;
    for (const auto& [key, value] : a.entries()){
        if(guard)guard->step();
        if (key.first > k && key.second == k && value != 0) {if(guard)guard->entries(machine::checked_size_add(a.stored_entries(),machine::checked_size_add(rows.size(),1)));rows.push_back(key.first);}
    }
    for (const auto i : rows) {
        while (a.get(i,k) != 0) {
            if(guard)guard->step();
            if (a.get(k,k) == 0) { row_swap(a,u,k,i,guard); continue; }
            Integer q = a.get(i,k) / a.get(k,k);
            if(guard){guard->step();guard->value(Rational{q});}
            row_add(a,u,i,k,-q,guard);
            if (a.get(i,k) != 0 && abs_integer(a.get(i,k)) < abs_integer(a.get(k,k))) row_swap(a,u,i,k,guard);
            else if (a.get(i,k) == 0) break;
        }
    }
    // Euclidean column reductions on row k.
    std::vector<std::size_t> cols;
    for (auto it = a.entries().lower_bound(IntegerMatrix::Key{k, 0u});
         it != a.entries().end() && it->first.first == k; ++it){
        if(guard)guard->step();
        if (it->first.second > k && it->second != 0) {if(guard)guard->entries(machine::checked_size_add(a.stored_entries(),machine::checked_size_add(cols.size(),1)));cols.push_back(it->first.second);}
    }
    for (const auto j : cols) {
        while (a.get(k,j) != 0) {
            if(guard)guard->step();
            if (a.get(k,k) == 0) { col_swap(a,v_inverse,k,j,guard); continue; }
            Integer q = a.get(k,j) / a.get(k,k);
            if(guard){guard->step();guard->value(Rational{q});}
            col_add(a,v_inverse,j,k,-q,guard);
            if (a.get(k,j) != 0 && abs_integer(a.get(k,j)) < abs_integer(a.get(k,k))) col_swap(a,v_inverse,j,k,guard);
            else if (a.get(k,j) == 0) break;
        }
    }
}

} // namespace detail

inline SmithNormalFormActions smith_normal_form_actions_impl(const IntegerMatrix& input,ExactActionGuard* guard) {
    if(guard){guard->step();input.validate(*guard);guard->entries(machine::checked_size_mul(input.stored_entries(),2));}
    IntegerMatrix a = input;
    a.prune_zeros();
    IntegerUnimodularAction u(input.rows()), v_inverse(input.cols());
    const std::size_t limit = std::min(input.rows(), input.cols());

    std::size_t k = 0;
    std::vector<Integer> invariants;
    while (k < limit) {
        if(guard)guard->step();
        // Bring a nonzero entry of the remaining submatrix into the pivot position.
        std::size_t pi = input.rows(), pj = input.cols();
        Integer best = 0;
        for (const auto& [key, value] : a.entries()) {
            if(guard)guard->step();
            if (key.first < k || key.second < k || value == 0) continue;
            const auto magnitude = abs_integer(value);
            if (best == 0 || magnitude < best) { best = magnitude; pi = key.first; pj = key.second; }
        }
        if (best == 0) break;
        if (pi != k) detail::row_swap(a,u,k,pi,guard);
        if (pj != k) detail::col_swap(a,v_inverse,k,pj,guard);

        // Reduce the pivot to the gcd of the entire remaining block.  Whenever
        // an off-pivot entry is not divisible by the pivot, mix its row into
        // the pivot row and repeat Euclidean row/column reduction.
        while (true) {
            if(guard)guard->step();
            detail::zero_out_pivot_row_col(a,u,v_inverse,k,guard);
            if (a.get(k,k) == 0) throw std::logic_error("smith pivot unexpectedly zero");

            // A Euclidean column swap can reintroduce entries below the pivot.
            // Finish those reductions before testing the remaining block.
            bool cross_is_zero = true;
            for (const auto& [key, value] : a.entries()) {
                if(guard)guard->step();
                if (value != 0 && ((key.first == k && key.second > k) ||
                                  (key.second == k && key.first > k))) {
                    cross_is_zero = false; break;
                }
            }
            if (!cross_is_zero) continue;

            bool divides = true;
            std::size_t oi = 0, oj = 0;
            const Integer pivot = abs_integer(a.get(k,k));
            for (const auto& [key, value] : a.entries()) {
                if(guard)guard->step();
                if (key.first > k && key.second > k && value % pivot != 0) {
                    divides = false; oi = key.first; oj = key.second; break;
                }
            }
            if (divides) break;

            // Column k is zero below the pivot at this point. Adding row oi to
            // the pivot row preserves the pivot entry but exposes the offending
            // a[oi,oj] in the pivot row, so the following Euclidean column
            // reduction replaces the pivot by a proper gcd divisor.
            detail::row_add(a,u,k,oi,Integer{1},guard);
            if (a.get(k,oj) == 0) throw std::logic_error("smith reduction failed to expose offending entry");
        }

        if (a.get(k,k) < 0) detail::row_negate(a,u,k,guard);
        if(guard)guard->entries(machine::checked_size_add(machine::checked_size_add(u.operations().size(),v_inverse.operations().size()),
            machine::checked_size_add(a.stored_entries(),machine::checked_size_add(invariants.size(),1))));
        invariants.push_back(a.get(k,k));
        ++k;
    }

    for (const auto& [key, value] : a.entries()) {
        if(guard)guard->step();
        if (value != 0 && (key.first != key.second || key.first >= k || value < 0))
            throw std::logic_error("smith diagonal certificate failed");
    }
    for (std::size_t i = 1; i < invariants.size(); ++i) {
        if(guard)guard->step();
        if (invariants[i] % invariants[i-1] != 0) throw std::logic_error("smith invariant divisibility failure");
    }
    const auto transformed=guard?v_inverse.apply_inverse_on_right(u.apply(input,*guard),*guard):v_inverse.apply_inverse_on_right(u.apply(input));
    if (!(transformed == a))
        throw std::logic_error("smith action factorization certificate failed");
    return SmithNormalFormActions{std::move(a), std::move(u), std::move(v_inverse), k, std::move(invariants)};
}

inline SmithNormalFormActions smith_normal_form_actions(const IntegerMatrix& input){
    return smith_normal_form_actions_impl(input,nullptr);
}
inline SmithNormalFormActions smith_normal_form_actions(const IntegerMatrix& input,ExactActionGuard& guard){
    return smith_normal_form_actions_impl(input,&guard);
}

inline SmithNormalForm smith_normal_form(const IntegerMatrix& input) {
    auto actions = smith_normal_form_actions(input);
    auto left = actions.left.apply(IntegerMatrix::identity(input.rows()));
    auto right = actions.right_inverse.apply_inverse(IntegerMatrix::identity(input.cols()));
    if (!(left * input * right == actions.diagonal)) throw std::logic_error("smith factorization certificate failed");
    return SmithNormalForm{std::move(actions.diagonal), std::move(left), std::move(right),
                           actions.rank, std::move(actions.invariant_factors)};
}

} // namespace rfc::numeric
