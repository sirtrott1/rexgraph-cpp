#pragma once
#include "rfc/numeric/algebraic_field.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/numeric/integer_matrix.hpp"
#include <map>
#include <utility>
#include <vector>

namespace rfc::numeric {

// Sparse coefficient maps over one actually declared algebraic field.  The
// power-basis scalar engine remains AlgebraicElement; no spatial root or
// approximation is selected here.  Elimination visits coefficient support.
class SparseAlgebraicMatrix {
public:
    using Key = std::pair<std::size_t,std::size_t>;
    using Entries = std::map<Key,AlgebraicElement>;
    using SparseVector = std::map<std::size_t,AlgebraicElement>;
    struct ColumnBasis {
        std::vector<std::size_t> columns, pivot_rows;
        AlgebraicElement pivot_product;
    };

    SparseAlgebraicMatrix(std::size_t rows,std::size_t cols,const CertifiedAlgebraicField& field)
        :rows_(rows),cols_(cols),field_(field),zero_(field.scalar(Rational{0})){}
    std::size_t rows() const noexcept{return rows_;}
    std::size_t cols() const noexcept{return cols_;}
    std::size_t nonzeros() const noexcept{return entries_.size();}
    const CertifiedAlgebraicField& field() const & noexcept{return field_;}
    const CertifiedAlgebraicField& field() const &&=delete;
    const Entries& entries() const & noexcept{return entries_;}
    const Entries& entries() const &&=delete;
    const AlgebraicElement& get(std::size_t row,std::size_t col) const &{
        check(row,col);const auto it=entries_.find({row,col});return it==entries_.end()?zero_:it->second;
    }
    const AlgebraicElement& get(std::size_t,std::size_t) const &&=delete;
    void set(std::size_t row,std::size_t col,AlgebraicElement value){
        check(row,col);if(!field_.owns(value))throw std::invalid_argument("sparse algebraic coefficient owner differs");
        if(value.is_zero())entries_.erase({row,col});else entries_.insert_or_assign(Key{row,col},std::move(value));
    }
    void validate(ExactActionGuard& guard) const{
        guard.dimension(rows_);guard.dimension(cols_);admit_storage(entries_.size(),guard);
        for(const auto& [key,value]:entries_){(void)key;admit(value,guard);}
    }
    static SparseAlgebraicMatrix identity(std::size_t n,const CertifiedAlgebraicField& field,ExactActionGuard& guard){
        guard.dimension(n);guard.entries(machine::checked_size_mul(n,field.degree()));
        SparseAlgebraicMatrix out(n,n,field);const auto one=field.scalar(Rational{1});
        for(std::size_t i=0;i<n;++i){guard.step();out.set(i,i,one);}return out;
    }
    static SparseAlgebraicMatrix from_rational(const ExactMatrix& matrix,const CertifiedAlgebraicField& field,ExactActionGuard& guard){
        guard.matrix(matrix);SparseAlgebraicMatrix out(matrix.rows(),matrix.cols(),field);
        for(const auto& [key,value]:matrix.entries()){guard.step();out.set(key.first,key.second,field.scalar(value));}
        out.validate(guard);return out;
    }
    static SparseAlgebraicMatrix from_integer(const IntegerMatrix& matrix,const CertifiedAlgebraicField& field,ExactActionGuard& guard){
        guard.dimension(matrix.rows());guard.dimension(matrix.cols());SparseAlgebraicMatrix out(matrix.rows(),matrix.cols(),field);
        for(const auto& [key,value]:matrix.entries())if(value!=0){guard.step();out.set(key.first,key.second,field.scalar(Rational{value}));}
        out.validate(guard);return out;
    }
    SparseAlgebraicMatrix transpose(ExactActionGuard& guard) const{
        validate(guard);SparseAlgebraicMatrix out(cols_,rows_,field_);
        for(const auto& [key,value]:entries_){guard.step();out.set(key.second,key.first,value);}return out;
    }
    SparseAlgebraicMatrix scale(const AlgebraicElement& scalar,ExactActionGuard& guard) const{
        validate(guard);admit(scalar,guard);SparseAlgebraicMatrix out(rows_,cols_,field_);
        for(const auto& [key,value]:entries_){guard.step();out.set(key.first,key.second,checked(value*scalar,guard));}out.validate(guard);return out;
    }
    SparseAlgebraicMatrix add(const SparseAlgebraicMatrix& other,ExactActionGuard& guard) const{
        require_field(other);if(rows_!=other.rows_||cols_!=other.cols_)throw std::invalid_argument("sparse algebraic addition shape");
        validate(guard);other.validate(guard);auto out=*this;
        for(const auto& [key,value]:other.entries_){guard.step();out.set(key.first,key.second,checked(out.get(key.first,key.second)+value,guard));out.admit_storage(out.nonzeros(),guard);}
        out.validate(guard);return out;
    }
    SparseAlgebraicMatrix multiply(const SparseAlgebraicMatrix& other,ExactActionGuard& guard) const{
        require_field(other);if(cols_!=other.rows_)throw std::invalid_argument("sparse algebraic product shape");
        validate(guard);other.validate(guard);SparseAlgebraicMatrix out(rows_,other.cols_,field_);
        for(const auto& [ik,a]:entries_){
            auto it=other.entries_.lower_bound(Key{ik.second,0});
            for(;it!=other.entries_.end()&&it->first.first==ik.second;++it){
                guard.step();const auto j=it->first.second;
                const auto product=checked(a*it->second,guard);
                out.set(ik.first,j,checked(out.get(ik.first,j)+product,guard));out.admit_storage(out.nonzeros(),guard);
            }
        }
        out.validate(guard);return out;
    }
    SparseAlgebraicMatrix select_columns(const std::vector<std::size_t>& indices,ExactActionGuard& guard) const{
        validate(guard);guard.dimension(indices.size());SparseAlgebraicMatrix out(rows_,indices.size(),field_);
        std::map<std::size_t,std::vector<std::size_t>> selected;
        for(std::size_t j=0;j<indices.size();++j){guard.step();if(indices[j]>=cols_)throw std::out_of_range("algebraic selected column");selected[indices[j]].push_back(j);}
        for(const auto& [key,value]:entries_){
            const auto it=selected.find(key.second);if(it==selected.end())continue;
            for(const auto j:it->second){guard.step();out.set(key.first,j,value);out.admit_storage(out.nonzeros(),guard);}
        }
        return out;
    }
    SparseVector apply_sparse(const SparseVector& input,ExactActionGuard& guard) const{
        validate(guard);SparseVector out;
        for(const auto& [index,value]:input){if(index>=cols_)throw std::out_of_range("algebraic vector coordinate");admit(value,guard);}
        for(const auto& [key,value]:entries_){
            const auto it=input.find(key.second);if(it==input.end())continue;guard.step();
            const auto product=checked(value*it->second,guard);
            sparse_set(out,key.first,checked(sparse_get(out,key.first)+product,guard));admit_storage(out.size(),guard);
        }
        for(const auto& [index,value]:out){(void)index;admit(value,guard);}return out;
    }
    ColumnBasis column_basis(ExactActionGuard& guard) const{
        validate(guard);std::map<std::size_t,SparseVector> columns,basis;
        for(const auto& [key,value]:entries_)columns[key.second].emplace(key.first,value);
        ColumnBasis result{{},{},field_.scalar(Rational{1})};std::size_t basis_entries=0;
        for(const auto& [index,input]:columns){
            auto column=input;
            for(const auto& [pivot,earlier]:basis){
                const auto it=column.find(pivot);if(it==column.end())continue;
                const auto multiple=it->second;
                for(const auto& [row,value]:earlier){guard.step();const auto product=checked(multiple*value,guard);sparse_set(column,row,checked(sparse_get(column,row)-product,guard));}
                admit_storage(machine::checked_size_add(machine::checked_size_mul(nonzeros(),2),machine::checked_size_add(basis_entries,column.size())),guard);
            }
            if(column.empty())continue;
            const auto pivot=column.begin()->first;const auto coefficient=column.begin()->second;
            for(auto& [row,value]:column){(void)row;guard.step();value=value/coefficient;admit(value,guard);}
            guard.step();result.pivot_product=result.pivot_product*coefficient;admit(result.pivot_product,guard);
            result.columns.push_back(index);result.pivot_rows.push_back(pivot);
            basis_entries=machine::checked_size_add(basis_entries,column.size());basis.emplace(pivot,std::move(column));
            admit_storage(machine::checked_size_add(machine::checked_size_mul(nonzeros(),2),basis_entries),guard);
        }
        return result;
    }
    std::size_t rank(ExactActionGuard& guard) const{return column_basis(guard).columns.size();}
    AlgebraicElement determinant(ExactActionGuard& guard) const{
        if(rows_!=cols_)throw std::invalid_argument("algebraic determinant requires square map");
        auto basis=column_basis(guard);if(basis.columns.size()!=cols_)return zero_;
        bool negative=false;
        for(std::size_t i=0;i<basis.pivot_rows.size();++i)for(std::size_t j=i+1;j<basis.pivot_rows.size();++j){guard.step();if(basis.pivot_rows[i]>basis.pivot_rows[j])negative=!negative;}
        return negative?-basis.pivot_product:basis.pivot_product;
    }
    // Solve only the declared right-hand sides.  All work remains over K and
    // sparse support; requesting an inverse is an explicit identity RHS.
    SparseAlgebraicMatrix solve(const SparseAlgebraicMatrix& rhs,ExactActionGuard& guard) const{
        require_field(rhs);if(rows_!=cols_||rhs.rows_!=rows_)throw std::invalid_argument("algebraic solve shape");
        validate(guard);rhs.validate(guard);(void)machine::checked_size_add(cols_,rhs.cols_);
        std::map<std::size_t,SparseVector> rows;
        for(const auto& [key,value]:entries_)rows[key.first].emplace(key.second,value);
        for(const auto& [key,value]:rhs.entries_)rows[key.first].emplace(cols_+key.second,value);
        for(std::size_t pivot=0;pivot<cols_;++pivot){
            auto chosen=rows.end();
            for(auto it=rows.lower_bound(pivot);it!=rows.end();++it){guard.step();if(it->second.contains(pivot)){chosen=it;break;}}
            if(chosen==rows.end())throw std::domain_error("singular algebraic solve");
            if(chosen->first!=pivot)std::swap(rows[pivot],chosen->second);
            auto& pivot_row=rows[pivot];const auto divisor=pivot_row.at(pivot);
            for(auto& [col,value]:pivot_row){(void)col;guard.step();value=value/divisor;admit(value,guard);}
            for(auto& [row,values]:rows){
                if(row==pivot)continue;
                const auto it=values.find(pivot);if(it==values.end())continue;
                const auto multiple=it->second;
                for(const auto& [col,value]:pivot_row){guard.step();const auto product=checked(multiple*value,guard);sparse_set(values,col,checked(sparse_get(values,col)-product,guard));}
            }
            std::size_t stored=0;for(const auto& [row,values]:rows){(void)row;stored=machine::checked_size_add(stored,values.size());}
            admit_storage(stored,guard);
        }
        SparseAlgebraicMatrix out(cols_,rhs.cols_,field_);
        for(const auto& [row,values]:rows)for(const auto& [col,value]:values)if(col>=cols_)out.set(row,col-cols_,value);
        out.validate(guard);if(!(multiply(out,guard)==rhs))throw std::logic_error("algebraic solve residual certificate failed");return out;
    }
    bool operator==(const SparseAlgebraicMatrix& other) const noexcept{
        return rows_==other.rows_&&cols_==other.cols_&&field_.same_owner(other.field_)&&entries_==other.entries_;
    }
private:
    void check(std::size_t row,std::size_t col) const{if(row>=rows_||col>=cols_)throw std::out_of_range("sparse algebraic matrix coordinate");}
    void require_field(const SparseAlgebraicMatrix& other) const{if(!field_.same_owner(other.field_))throw std::invalid_argument("sparse algebraic matrix field owners differ");}
    void admit_storage(std::size_t coefficients,ExactActionGuard& guard) const{guard.entries(machine::checked_size_mul(coefficients,field_.degree()));}
    void admit(const AlgebraicElement& value,ExactActionGuard& guard) const{
        if(!field_.owns(value))throw std::invalid_argument("sparse algebraic value field owner differs");
        for(const auto& q:value.coordinates()){guard.step();guard.value(q);}
    }
    AlgebraicElement checked(AlgebraicElement value,ExactActionGuard& guard) const{admit(value,guard);return value;}
    const AlgebraicElement& sparse_get(const SparseVector& values,std::size_t index) const{
        const auto it=values.find(index);return it==values.end()?zero_:it->second;
    }
    static void sparse_set(SparseVector& values,std::size_t index,AlgebraicElement value){
        if(value.is_zero())values.erase(index);else values.insert_or_assign(index,std::move(value));
    }
    std::size_t rows_,cols_;
    CertifiedAlgebraicField field_;
    AlgebraicElement zero_;
    Entries entries_;
};
} // namespace rfc::numeric
