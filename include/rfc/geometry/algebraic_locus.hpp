#pragma once
#include "rfc/numeric/exact.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::geometry {

using Monomial = std::vector<std::uint16_t>;

class ExactPolynomial {
public:
    ExactPolynomial()=default;
    explicit ExactPolynomial(std::size_t variables):variables_(variables){}
    static ExactPolynomial constant(std::size_t variables,numeric::Rational value){
        ExactPolynomial p(variables); value.canonicalize(); if(value!=0) p.terms_[Monomial(variables,0)]=std::move(value); return p;
    }
    static ExactPolynomial variable(std::size_t variables,std::size_t index){
        if(index>=variables) throw std::out_of_range("polynomial variable index");
        ExactPolynomial p(variables); Monomial m(variables,0); m[index]=1; p.terms_[std::move(m)]=1; return p;
    }
    [[nodiscard]] std::size_t variable_count() const noexcept{return variables_;}
    [[nodiscard]] bool is_zero() const noexcept{return terms_.empty();}
    [[nodiscard]] const std::map<Monomial,numeric::Rational>& terms()const noexcept{return terms_;}
    [[nodiscard]] ExactPolynomial add(const ExactPolynomial& other)const{
        require_ring(other); ExactPolynomial out=*this;
        for(const auto& [m,c]:other.terms_){ numeric::Rational v=out.terms_[m]+c; v.canonicalize(); if(v==0) out.terms_.erase(m); else out.terms_[m]=std::move(v); }
        return out;
    }
    [[nodiscard]] ExactPolynomial scale(numeric::Rational scalar)const{
        scalar.canonicalize(); ExactPolynomial out(variables_); if(scalar==0)return out;
        for(const auto& [m,c]:terms_){ numeric::Rational v=scalar*c; v.canonicalize(); if(v!=0)out.terms_[m]=std::move(v); } return out;
    }
    [[nodiscard]] ExactPolynomial multiply(const ExactPolynomial& other)const{
        require_ring(other); ExactPolynomial out(variables_);
        for(const auto& [a,ca]:terms_) {
            for(const auto& [b,cb]:other.terms_){
                Monomial m(variables_,0); for(std::size_t i=0;i<variables_;++i){ const auto e=static_cast<unsigned>(a[i])+static_cast<unsigned>(b[i]); if(e>65535u)throw std::overflow_error("polynomial exponent overflow"); m[i]=static_cast<std::uint16_t>(e); }
                numeric::Rational v=out.terms_[m]+ca*cb; v.canonicalize(); if(v==0)out.terms_.erase(m); else out.terms_[std::move(m)]=std::move(v);
            }
        }
        return out;
    }
    [[nodiscard]] numeric::Rational evaluate(const numeric::RationalVector& point)const{
        if(point.size()!=variables_)throw std::invalid_argument("polynomial evaluation dimension mismatch");
        numeric::Rational result=0;
        for(const auto& [m,c]:terms_){ numeric::Rational term=c; for(std::size_t i=0;i<variables_;++i) for(std::uint16_t e=0;e<m[i];++e) term*=point[i]; result+=term; }
        result.canonicalize(); return result;
    }
    [[nodiscard]] bool operator==(const ExactPolynomial& other)const noexcept{return variables_==other.variables_ && terms_==other.terms_;}
private:
    void require_ring(const ExactPolynomial& other)const{if(variables_!=other.variables_)throw std::invalid_argument("polynomial ring mismatch");}
    std::size_t variables_{};
    std::map<Monomial,numeric::Rational> terms_;
};

class PolynomialMatrix {
public:
    using Key = std::pair<std::size_t,std::size_t>;
    PolynomialMatrix(std::size_t rows,std::size_t cols,std::size_t variables)
        :rows_(rows),cols_(cols),variables_(variables),zero_(variables){}
    [[nodiscard]] std::size_t rows()const noexcept{return rows_;}
    [[nodiscard]] std::size_t cols()const noexcept{return cols_;}
    [[nodiscard]] std::size_t variable_count()const noexcept{return variables_;}
    [[nodiscard]] std::size_t nonzeros()const noexcept{return entries_.size();}
    [[nodiscard]] const std::map<Key,ExactPolynomial>& entries()const noexcept{return entries_;}
    [[nodiscard]] const ExactPolynomial& get(std::size_t i,std::size_t j)const{
        if(i>=rows_||j>=cols_)throw std::out_of_range("polynomial matrix index");
        const auto it=entries_.find({i,j});
        return it==entries_.end()?zero_:it->second;
    }
    void set(std::size_t i,std::size_t j,ExactPolynomial value){
        if(i>=rows_||j>=cols_||value.variable_count()!=variables_)throw std::invalid_argument("polynomial matrix set mismatch");
        if(value.is_zero())entries_.erase({i,j});
        else entries_.insert_or_assign(Key{i,j},std::move(value));
    }
    [[nodiscard]] PolynomialMatrix multiply(const PolynomialMatrix& other)const{
        if(cols_!=other.rows_||variables_!=other.variables_)throw std::invalid_argument("polynomial matrix multiply mismatch");
        PolynomialMatrix out(rows_,other.cols_,variables_);
        // Join only retained coefficients through a shared intermediate index.
        // Neither storage nor traversal forms the ambient rows*cols rectangle.
        for(const auto& [ik,a]:entries_){
            auto it=other.entries_.lower_bound(Key{ik.second,0u});
            for(;it!=other.entries_.end()&&it->first.first==ik.second;++it){
                const auto j=it->first.second;
                out.set(ik.first,j,out.get(ik.first,j).add(a.multiply(it->second)));
            }
        }
        return out;
    }
private:
    std::size_t rows_{},cols_{},variables_{};
    ExactPolynomial zero_;
    std::map<Key,ExactPolynomial> entries_;
};

using PolynomialIdealGenerators = std::vector<ExactPolynomial>;

inline PolynomialIdealGenerators chain_ideal_generators(const PolynomialMatrix& lower,
                                                        const PolynomialMatrix& upper){
    const auto defect=lower.multiply(upper); PolynomialIdealGenerators out;
    out.reserve(defect.nonzeros());
    // The ordered sparse keys retain the former row-major generator order.
    for(const auto& [key,polynomial]:defect.entries())out.push_back(polynomial);
    return out;
}

inline PolynomialIdealGenerators bianchi_ideal_generators(const std::vector<PolynomialMatrix>& boundaries){
    PolynomialIdealGenerators out;
    for(std::size_t k=1;k<boundaries.size();++k){ auto local=chain_ideal_generators(boundaries[k-1u],boundaries[k]); out.insert(out.end(),local.begin(),local.end()); }
    return out;
}

inline bool lies_on_flat_locus(const PolynomialIdealGenerators& generators,
                               const numeric::RationalVector& point){
    for(const auto& polynomial:generators) {
        if(polynomial.evaluate(point)!=0) return false;
    }
    return true;
}

} // namespace rfc::geometry
