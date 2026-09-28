#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::field {

inline numeric::ExactMatrix exact_minor(const numeric::ExactMatrix& A,std::size_t row,std::size_t col){
    if(A.rows()!=A.cols() || row>=A.rows() || col>=A.cols()) throw std::invalid_argument("minor requires square matrix and valid index");
    if(A.rows()==0) throw std::invalid_argument("minor of empty matrix");
    numeric::ExactMatrix out(A.rows()-1u,A.cols()-1u);
    for(const auto& [key,value]:A.entries()){
        if(key.first==row || key.second==col) continue;
        const auto i=key.first-(key.first>row?1u:0u);
        const auto j=key.second-(key.second>col?1u:0u);
        out.set(i,j,value);
    }
    return out;
}

// Reference exact adjugate valid at singular matrices. Production Green paths
// do not use this O(n) minor family; it exists because the direct tensor and
// determinant differentials are mathematical objects in the paper.
inline numeric::ExactMatrix exact_adjugate(const numeric::ExactMatrix& A){
    if(A.rows()!=A.cols()) throw std::invalid_argument("adjugate requires square matrix");
    const auto n=A.rows();
    if(n==0) return numeric::ExactMatrix{};
    if(n==1) return numeric::ExactMatrix::from_rows({{1}});
    numeric::ExactMatrix out(n,n);
    for(std::size_t i=0;i<n;++i){
        for(std::size_t j=0;j<n;++j){
            auto cofactor=exact_minor(A,i,j).determinant();
            if(((i+j)&1u)!=0u) cofactor=-cofactor;
            // adj(A)_{j,i}=cofactor_{i,j}
            if(cofactor!=0) out.set(j,i,cofactor);
        }
    }
    return out;
}

inline numeric::Rational exact_trace(const numeric::ExactMatrix& A){
    if(A.rows()!=A.cols()) throw std::invalid_argument("trace requires square matrix");
    numeric::Rational value=0;
    for(std::size_t i=0;i<A.rows();++i) value+=A.get(i,i);
    value.canonicalize();
    return value;
}

inline numeric::Rational determinant_differential(const numeric::ExactMatrix& A,
                                                   const numeric::ExactMatrix& dA){
    if(A.rows()!=A.cols() || dA.rows()!=A.rows() || dA.cols()!=A.cols()) throw std::invalid_argument("determinant differential dimension mismatch");
    return exact_trace(exact_adjugate(A).multiply(dA));
}

inline numeric::ExactMatrix adjugate_differential_invertible(const numeric::ExactMatrix& A,
                                                              const numeric::ExactMatrix& dA){
    if(A.rows()!=A.cols() || dA.rows()!=A.rows() || dA.cols()!=A.cols()) throw std::invalid_argument("adjugate differential dimension mismatch");
    const auto inv=A.inverse();
    const auto adj=exact_adjugate(A);
    const auto scalar=exact_trace(inv.multiply(dA));
    return adj.scale(scalar).add(adj.multiply(dA).multiply(inv).scale(numeric::Rational{-1}));
}

inline numeric::ExactMatrix inverse_difference_identity(const numeric::ExactMatrix& A0,
                                                        const numeric::ExactMatrix& A1){
    if(A0.rows()!=A0.cols() || A1.rows()!=A1.cols() || A0.rows()!=A1.rows()) throw std::invalid_argument("inverse difference dimension mismatch");
    return A1.inverse().multiply(A1.add(A0.scale(numeric::Rational{-1}))).multiply(A0.inverse()).scale(numeric::Rational{-1});
}

} // namespace rfc::field
