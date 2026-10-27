#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/learning/graded_transport_fit.hpp"

namespace sparse_transport_example {
using namespace rfc;
using Q=numeric::Rational; using M=numeric::ExactMatrix;
using V=numeric::RationalVector; using S=numeric::SparseRationalVector;
using Guard=numeric::ExactActionGuard;using Fit=learning::PreparedGradedTransportFit;
using Sample=learning::SparseTransportObservation;
inline numeric::ExactActionLimits limits(std::size_t n=7) {
    numeric::ExactActionLimits l;l.dimension=machine::checked_size_add(machine::checked_size_mul(n,4),16);
    l.entries=16384;l.steps=200000;l.integer_bits=4096;return l;
}
inline learning::PreparedTransportObservations model(std::size_t n,Guard& g) {
    if(n<2)throw std::invalid_argument("example needs two active coordinate directions");
    core::RelationSignature sig;auto type=sig.add_type("transport",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(type,{{10,true,true,1},{20,true,false,1},{20,true,false,0},{20,false,false,3}});
    const auto source=b.freeze();local::LinearStalkBundle stalks(source,n);
    local::LinearRelationalFiberSystem fibers(stalks);
    for(std::size_t p=0;p<4;++p){M m(n,n);if(p==0){m.set(0,0,-1);m.set(1,1,-1);}fibers.set_transport(core::Grade{1},p,std::move(m));}
    // Four active head parameters, plus one present-zero and one absent parameter.
    // Those last two remain genuine free parameters in the original declared family.
    return learning::PreparedTransportObservations::at_grade(local::OwnedLinearFiberSystem(fibers),
        core::Grade{1},{{0,0,0},{0,1,1}},{{1,0,0},{1,1,0},{1,0,1},{1,1,1},{2,0,0},{3,0,0}},g);
}
inline std::vector<Sample> samples(std::size_t n) {
    return {{S::basis(n,0),{{1,0,Q{3,5}},{1,1,Q{4,5}}}},
            {S::basis(n,1),{{1,0,Q{-4,5}},{1,1,Q{3,5}}}}};
}
}
