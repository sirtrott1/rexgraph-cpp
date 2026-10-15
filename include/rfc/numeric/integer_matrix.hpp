#pragma once
#include "rfc/numeric/exact.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::numeric {

class IntegerMatrix {
public:
    using Key = std::pair<std::size_t, std::size_t>;
    using Entries = std::map<Key, Integer>;
    using SparseVector = std::map<std::size_t, Integer>;

    IntegerMatrix() = default;
    IntegerMatrix(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols) {}

    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept { return cols_; }
    [[nodiscard]] const Entries& entries() const noexcept { return entries_; }
    [[nodiscard]] std::size_t stored_entries() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t nonzeros() const noexcept {
        std::size_t count = 0;
        for (const auto& entry : entries_) if (entry.second != 0) ++count;
        return count;
    }

    [[nodiscard]] const Integer& get(std::size_t row, std::size_t col) const {
        check(row, col);
        const auto it = entries_.find({row, col});
        static const Integer zero{0};
        return it == entries_.end() ? zero : it->second;
    }
    // Retained for callers that update a coordinate through an Integer&.
    // Merely touching a zero can leave a stored zero until prune_zeros();
    // arithmetic ignores such entries and set() erases exact zeros.  Coordinate
    // references must not be kept across structural sparse row/column edits.
    [[nodiscard]] Integer& at(std::size_t row, std::size_t col) {
        check(row, col);
        return entries_[{row, col}];
    }
    void set(std::size_t row, std::size_t col, Integer value) {
        check(row, col);
        if (value == 0) entries_.erase({row, col});
        else entries_.insert_or_assign(Key{row, col}, std::move(value));
    }

    void prune_zeros() noexcept {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second == 0) it = entries_.erase(it);
            else ++it;
        }
    }

    [[nodiscard]] static IntegerMatrix identity(std::size_t n) {
        IntegerMatrix out(n, n);
        for (std::size_t i = 0; i < n; ++i) out.set(i, i, 1);
        return out;
    }

    [[nodiscard]] IntegerMatrix transpose() const {
        IntegerMatrix out(cols_, rows_);
        for (const auto& [key, value] : entries_)
            if (value != 0) out.set(key.second, key.first, value);
        return out;
    }

    [[nodiscard]] IntegerMatrix operator*(const IntegerMatrix& rhs) const {
        if (cols_ != rhs.rows_) throw std::invalid_argument("integer matrix multiply shape mismatch");
        IntegerMatrix out(rows_, rhs.cols_);
        for (const auto& [ik, a] : entries_) {
            if (a == 0) continue;
            auto it = rhs.entries_.lower_bound(Key{ik.second, 0u});
            for (; it != rhs.entries_.end() && it->first.first == ik.second; ++it) {
                if (it->second == 0) continue;
                const auto j = it->first.second;
                out.set(ik.first, j, out.get(ik.first, j) + a * it->second);
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<Integer> apply(const std::vector<Integer>& x) const {
        if (x.size() != cols_) throw std::invalid_argument("integer matrix vector shape mismatch");
        std::vector<Integer> out(rows_, Integer{0});
        for (const auto& [key, value] : entries_)
            if (value != 0 && x[key.second] != 0) out[key.first] += value * x[key.second];
        return out;
    }

    // Sparse-domain action: unlike the explicitly requested full vector above,
    // this returns only nonzero output coordinates, even for enormous domains.
    [[nodiscard]] SparseVector apply_sparse(const SparseVector& x) const {
        for (const auto& entry : x)
            if (entry.first >= cols_) throw std::out_of_range("integer sparse vector index");
        SparseVector out;
        for (const auto& [key, value] : entries_) {
            const auto it = x.find(key.second);
            if (value == 0 || it == x.end() || it->second == 0) continue;
            Integer updated = value * it->second;
            const auto old = out.find(key.first);
            if (old != out.end()) updated += old->second;
            if (updated == 0) out.erase(key.first);
            else out.insert_or_assign(key.first, std::move(updated));
        }
        return out;
    }

    [[nodiscard]] bool operator==(const IntegerMatrix& rhs) const noexcept {
        if (rows_ != rhs.rows_ || cols_ != rhs.cols_) return false;
        auto a = entries_.begin(), b = rhs.entries_.begin();
        for (;;) {
            while (a != entries_.end() && a->second == 0) ++a;
            while (b != rhs.entries_.end() && b->second == 0) ++b;
            if (a == entries_.end() || b == rhs.entries_.end()) return a == entries_.end() && b == rhs.entries_.end();
            if (*a != *b) return false;
            ++a; ++b;
        }
    }

    void swap_rows(std::size_t a, std::size_t b) {
        if (a >= rows_ || b >= rows_) throw std::out_of_range("integer matrix row swap");
        if (a == b) return;
        Entries out;
        for (const auto& [key, value] : entries_) {
            if (value == 0) continue;
            const auto row = key.first == a ? b : key.first == b ? a : key.first;
            out.emplace(Key{row, key.second}, value);
        }
        entries_ = std::move(out);
    }
    void swap_cols(std::size_t a, std::size_t b) {
        if (a >= cols_ || b >= cols_) throw std::out_of_range("integer matrix column swap");
        if (a == b) return;
        Entries out;
        for (const auto& [key, value] : entries_) {
            if (value == 0) continue;
            const auto col = key.second == a ? b : key.second == b ? a : key.second;
            out.emplace(Key{key.first, col}, value);
        }
        entries_ = std::move(out);
    }
    void row_add_multiple(std::size_t target, std::size_t source, const Integer& multiple) {
        if (target >= rows_ || source >= rows_) throw std::out_of_range("integer matrix row add");
        if (multiple == 0) return;
        const Integer coefficient = multiple;
        if (target == source) { row_scale(target, Integer{1} + coefficient); return; }
        std::vector<std::pair<std::size_t, Integer>> source_entries;
        for (auto it = entries_.lower_bound(Key{source, 0u}); it != entries_.end() && it->first.first == source; ++it)
            if (it->second != 0) source_entries.emplace_back(it->first.second, it->second);
        for (const auto& [col, value] : source_entries)
            set(target, col, get(target, col) + coefficient * value);
    }
    void col_add_multiple(std::size_t target, std::size_t source, const Integer& multiple) {
        if (target >= cols_ || source >= cols_) throw std::out_of_range("integer matrix col add");
        if (multiple == 0) return;
        const Integer coefficient = multiple;
        if (target == source) { col_scale(target, Integer{1} + coefficient); return; }
        std::vector<std::pair<std::size_t, Integer>> source_entries;
        for (const auto& [key, value] : entries_)
            if (key.second == source && value != 0) source_entries.emplace_back(key.first, value);
        for (const auto& [row, value] : source_entries)
            set(row, target, get(row, target) + coefficient * value);
    }
    void row_scale(std::size_t row, const Integer& factor) {
        if (row >= rows_) throw std::out_of_range("integer matrix row scale");
        const Integer coefficient = factor;
        for (auto it = entries_.lower_bound(Key{row, 0u}); it != entries_.end() && it->first.first == row;) {
            it->second *= coefficient;
            if (it->second == 0) it = entries_.erase(it);
            else ++it;
        }
    }
    void col_scale(std::size_t col, const Integer& factor) {
        if (col >= cols_) throw std::out_of_range("integer matrix col scale");
        const Integer coefficient = factor;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->first.second == col) it->second *= coefficient;
            if (it->second == 0) it = entries_.erase(it);
            else ++it;
        }
    }

    // Explicitly guarded counterparts keep compatibility calls unchanged.
    // Every visited support entry and scalar product/sum is charged; storage
    // limits cover these containers, not allocator bytes or GMP internals.
    void validate(ExactActionGuard& guard) const {
        guard.dimension(rows_); guard.dimension(cols_); guard.entries(entries_.size());
        for(const auto& [key,value]:entries_){(void)key;guard.step();guard.value(Rational{value});}
    }
    [[nodiscard]] static IntegerMatrix identity(std::size_t n,ExactActionGuard& guard) {
        guard.dimension(n);guard.entries(n);IntegerMatrix out(n,n);
        for(std::size_t i=0;i<n;++i){guard.step();out.set(i,i,Integer{1});}
        return out;
    }
    [[nodiscard]] IntegerMatrix multiply(const IntegerMatrix& rhs,ExactActionGuard& guard) const {
        if(cols_!=rhs.rows_)throw std::invalid_argument("guarded integer product shape mismatch");
        validate(guard);rhs.validate(guard);IntegerMatrix out(rows_,rhs.cols_);
        const auto retained=machine::checked_size_add(stored_entries(),rhs.stored_entries());
        guard.entries(retained);
        for(const auto& [ik,a]:entries_){
            guard.step();if(a==0)continue;
            auto it=rhs.entries_.lower_bound(Key{ik.second,0});
            for(;it!=rhs.entries_.end()&&it->first.first==ik.second;++it){
                guard.step();if(it->second==0)continue;
                const auto product=checked_product(a,it->second,guard);
                out.checked_set(ik.first,it->first.second,checked_sum(out.get(ik.first,it->first.second),product,guard),guard,retained);
            }
        }
        return out;
    }
    [[nodiscard]] SparseVector apply_sparse(const SparseVector& input,ExactActionGuard& guard) const {
        validate(guard);guard.entries(machine::checked_size_add(stored_entries(),input.size()));
        for(const auto& [index,value]:input){guard.step();if(index>=cols_)throw std::out_of_range("guarded integer vector index");guard.value(Rational{value});}
        SparseVector out;const auto retained=machine::checked_size_add(stored_entries(),input.size());
        for(const auto& [key,value]:entries_){
            guard.step();const auto it=input.find(key.second);if(value==0||it==input.end()||it->second==0)continue;
            const auto product=checked_product(value,it->second,guard);
            const auto old=out.find(key.first);const auto sum=checked_sum(old==out.end()?Integer{0}:old->second,product,guard);
            if(sum==0)out.erase(key.first);
            else {guard.entries(machine::checked_size_add(retained,machine::checked_size_add(out.size(),old==out.end()?1:0)));out.insert_or_assign(key.first,sum);}
        }
        return out;
    }
    void swap_rows(std::size_t a,std::size_t b,ExactActionGuard& guard){checked_swap(true,a,b,guard);}
    void swap_cols(std::size_t a,std::size_t b,ExactActionGuard& guard){checked_swap(false,a,b,guard);}
    void row_add_multiple(std::size_t target,std::size_t source,const Integer& multiple,ExactActionGuard& guard){checked_add(true,target,source,multiple,guard);}
    void col_add_multiple(std::size_t target,std::size_t source,const Integer& multiple,ExactActionGuard& guard){checked_add(false,target,source,multiple,guard);}
    void row_scale(std::size_t row,const Integer& factor,ExactActionGuard& guard){checked_scale(true,row,factor,guard);}
    void col_scale(std::size_t col,const Integer& factor,ExactActionGuard& guard){checked_scale(false,col,factor,guard);}

private:
    std::size_t rows_{};
    std::size_t cols_{};
    Entries entries_;

    void check(std::size_t row, std::size_t col) const {
        if (row >= rows_ || col >= cols_) throw std::out_of_range("integer matrix index");
    }
    static Integer checked_product(const Integer& a,const Integer& b,ExactActionGuard& guard){
        guard.step();Integer out=a*b;guard.value(Rational{out});return out;
    }
    static Integer checked_sum(const Integer& a,const Integer& b,ExactActionGuard& guard){
        guard.step();Integer out=a+b;guard.value(Rational{out});return out;
    }
    void checked_set(std::size_t row,std::size_t col,Integer value,ExactActionGuard& guard,std::size_t retained){
        const auto added=value!=0&&!entries_.contains(Key{row,col})?1u:0u;
        guard.entries(machine::checked_size_add(retained,machine::checked_size_add(entries_.size(),added)));
        set(row,col,std::move(value));
    }
    void checked_swap(bool rows,std::size_t a,std::size_t b,ExactActionGuard& guard){
        const auto dimension=rows?rows_:cols_;if(a>=dimension||b>=dimension)throw std::out_of_range("guarded integer axis swap");
        validate(guard);if(a==b)return;
        guard.entries(machine::checked_size_mul(entries_.size(),2));Entries out;
        for(const auto& [key,value]:entries_){
            guard.step();if(value==0)continue;
            auto target=key;auto& axis=rows?target.first:target.second;axis=axis==a?b:axis==b?a:axis;
            out.emplace(target,value);
        }
        entries_=std::move(out);
    }
    void checked_scale(bool rows,std::size_t index,const Integer& factor,ExactActionGuard& guard){
        const auto dimension=rows?rows_:cols_;if(index>=dimension)throw std::out_of_range("guarded integer axis scale");
        validate(guard);guard.step();const Integer coefficient=factor;guard.value(Rational{coefficient});
        for(auto it=entries_.begin();it!=entries_.end();){
            guard.step();if((rows?it->first.first:it->first.second)==index)it->second=checked_product(it->second,coefficient,guard);
            if(it->second==0)it=entries_.erase(it);else ++it;
        }
    }
    void checked_add(bool rows,std::size_t target,std::size_t source,const Integer& multiple,ExactActionGuard& guard){
        const auto dimension=rows?rows_:cols_;if(target>=dimension||source>=dimension)throw std::out_of_range("guarded integer axis addition");
        validate(guard);guard.step();const Integer coefficient=multiple;guard.value(Rational{coefficient});if(coefficient==0)return;
        if(target==source){checked_scale(rows,target,checked_sum(Integer{1},coefficient,guard),guard);return;}
        std::vector<std::pair<std::size_t,Integer>> support;
        for(const auto& [key,value]:entries_){
            guard.step();if((rows?key.first:key.second)!=source||value==0)continue;
            guard.entries(machine::checked_size_add(entries_.size(),machine::checked_size_add(support.size(),1)));
            support.emplace_back(rows?key.second:key.first,value);
        }
        for(const auto& [index,value]:support){
            const auto row=rows?target:index,col=rows?index:target;
            checked_set(row,col,checked_sum(get(row,col),checked_product(coefficient,value,guard),guard),guard,support.size());
        }
    }
};

inline Integer abs_integer(Integer value) {
    if (value < 0) value = -value;
    return value;
}

} // namespace rfc::numeric
