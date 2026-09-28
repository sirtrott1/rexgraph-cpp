#pragma once
#include "rfc/numeric/exact.hpp"
namespace rfc {
using Exact = numeric::Rational;
using ExactVector = numeric::RationalVector;
inline Exact exact(long n) { return numeric::rational(n); }
inline Exact exact(long n, long d) { return numeric::rational(n,d); }
inline std::string str(const Exact& q) { return numeric::str(q); }
inline Exact dot(const ExactVector&a,const ExactVector&b){return numeric::dot(a,b);} 
inline ExactVector add(const ExactVector&a,const ExactVector&b){return numeric::add(a,b);} 
inline ExactVector sub(const ExactVector&a,const ExactVector&b){return numeric::sub(a,b);} 
inline ExactVector scale(const Exact&s,const ExactVector&x){return numeric::scale(s,x);} 
inline bool is_zero(const ExactVector&x){return numeric::is_zero(x);} 
}
