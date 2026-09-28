#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::approximation {

using Vector = std::vector<double>;

class DenseMatrix {
public:
    DenseMatrix() = default;
    DenseMatrix(std::size_t rows,std::size_t cols) : rows_(rows), cols_(cols), data_(rows*cols,0.0) {}
    static DenseMatrix identity(std::size_t n){ DenseMatrix I(n,n); for(std::size_t i=0;i<n;++i) I(i,i)=1.0; return I; }
    static DenseMatrix from_exact(const numeric::ExactMatrix& exact){
        DenseMatrix out(exact.rows(),exact.cols());
        for(const auto& [key,value]:exact.entries()) out(key.first,key.second)=value.get_d();
        return out;
    }
    [[nodiscard]] std::size_t rows() const noexcept{return rows_;}
    [[nodiscard]] std::size_t cols() const noexcept{return cols_;}
    double& operator()(std::size_t i,std::size_t j){return data_.at(i*cols_+j);}
    [[nodiscard]] double operator()(std::size_t i,std::size_t j) const{return data_.at(i*cols_+j);}
    [[nodiscard]] DenseMatrix transpose() const{ DenseMatrix out(cols_,rows_); for(std::size_t i=0;i<rows_;++i)for(std::size_t j=0;j<cols_;++j)out(j,i)=(*this)(i,j); return out; }
    [[nodiscard]] Vector apply(const Vector& x) const{
        if(x.size()!=cols_)throw std::invalid_argument("approx matrix apply size mismatch");
        Vector out(rows_,0.0); for(std::size_t i=0;i<rows_;++i)for(std::size_t j=0;j<cols_;++j)out[i]+=(*this)(i,j)*x[j]; return out;
    }
    [[nodiscard]] DenseMatrix multiply(const DenseMatrix& other) const{
        if(cols_!=other.rows_)throw std::invalid_argument("approx multiply shape mismatch");
        DenseMatrix out(rows_,other.cols_); for(std::size_t i=0;i<rows_;++i)for(std::size_t k=0;k<cols_;++k){const double a=(*this)(i,k);if(a==0.0)continue;for(std::size_t j=0;j<other.cols_;++j)out(i,j)+=a*other(k,j);}return out;
    }
    [[nodiscard]] DenseMatrix add(const DenseMatrix& other) const {
        if (rows_ != other.rows_ || cols_ != other.cols_) throw std::invalid_argument("approx add shape mismatch");
        DenseMatrix out(rows_, cols_);
        for (std::size_t i = 0; i < data_.size(); ++i) out.data_[i] = data_[i] + other.data_[i];
        return out;
    }
    [[nodiscard]] DenseMatrix scale(double scalar) const{DenseMatrix out=*this;for(auto& v:out.data_)v*=scalar;return out;}
    [[nodiscard]] Vector solve(Vector rhs) const{
        if(rows_!=cols_||rhs.size()!=rows_)throw std::invalid_argument("approx solve shape mismatch");
        const std::size_t n=rows_; DenseMatrix A=*this;
        for(std::size_t k=0;k<n;++k){
            std::size_t pivot=k; double best=std::abs(A(k,k));
            for(std::size_t i=k+1;i<n;++i){const double c=std::abs(A(i,k));if(c>best){best=c;pivot=i;}}
            if(best<=1e-15)throw std::runtime_error("singular approximate system");
            if(pivot!=k){for(std::size_t j=0;j<n;++j)std::swap(A(k,j),A(pivot,j));std::swap(rhs[k],rhs[pivot]);}
            for(std::size_t i=k+1;i<n;++i){const double ratio=A(i,k)/A(k,k);A(i,k)=0.0;for(std::size_t j=k+1;j<n;++j)A(i,j)-=ratio*A(k,j);rhs[i]-=ratio*rhs[k];}
        }
        Vector x(n,0.0); for(std::size_t ii=n;ii-->0;){double sum=0.0;for(std::size_t j=ii+1;j<n;++j)sum+=A(ii,j)*x[j];x[ii]=(rhs[ii]-sum)/A(ii,ii);}return x;
    }
private:
    std::size_t rows_{}; std::size_t cols_{}; std::vector<double> data_;
};

inline Vector add(const Vector& a,const Vector& b){if(a.size()!=b.size())throw std::invalid_argument("approx vector size mismatch");Vector o(a.size());for(std::size_t i=0;i<a.size();++i)o[i]=a[i]+b[i];return o;}
inline Vector scale(double s,const Vector& a){Vector o(a.size());for(std::size_t i=0;i<a.size();++i)o[i]=s*a[i];return o;}
inline double max_abs_error(const Vector& a,const numeric::RationalVector& exact){if(a.size()!=exact.size())throw std::invalid_argument("approx error size mismatch");double e=0.0;for(std::size_t i=0;i<a.size();++i)e=std::max(e,std::abs(a[i]-exact[i].get_d()));return e;}

} // namespace rfc::approximation
