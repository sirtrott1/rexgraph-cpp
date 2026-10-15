#pragma once
#include "rfc/algebra/finite_algebra.hpp"
#include "rfc/core/builder.hpp"

namespace finite_algebra_example {
struct Declaration {
    rfc::core::RelationalComplex source;
    std::vector<rfc::algebra::FiniteBinaryOperation> operations;
    std::vector<rfc::core::IdentityKey> values;
};
// Explicit finite relation tables used by this example. The law checker reads
// the retained roles and values, and does not perform this modular arithmetic.
inline Declaration modular(unsigned modulus,bool duplicate=false,bool conflict=false,
        bool partial=false,bool nonassociative=false,bool malformed=false) {
    using namespace rfc;
    if(modulus<2 || modulus>32)throw std::invalid_argument("example modulus range");
    core::RelationSignature signature;
    const auto left=signature.aspects().ensure("left input",composite::AspectRole::RelationRole,false);
    const auto right=signature.aspects().ensure("right input",composite::AspectRole::RelationRole,false);
    const auto output=signature.aspects().ensure("output",composite::AspectRole::RelationRole,false);
    const auto add=signature.add_type("Add",core::BoundaryPolicy::Algebraic,3,3);
    const auto multiply=signature.add_type("Mul",core::BoundaryPolicy::Algebraic,3,3);
    core::RelationalComplexBuilder builder(signature);
    std::vector<core::IdentityKey> values;
    for(unsigned i=0;i<modulus;++i){values.push_back(100+17*i);builder.declare_witness_identity(values.back());}
    std::uint64_t relation=1000;
    const auto append=[&](core::RelationTypeId type,unsigned a,unsigned b,unsigned c,bool malformed_row=false) {
        // Deliberately output-first; operands may repeat the same identity.
        builder.add_grade_one(core::RelationKey{++relation},type,
            {{values[c],true,true,numeric::Rational{1},{{output,true}}},
             {values[a],true,false,numeric::Rational{1},{{left,true},{right,malformed_row}}},
             {values[b],true,false,numeric::Rational{1},{{right,true}}}});
    };
    for(unsigned a=0;a<modulus;++a)for(unsigned b=0;b<modulus;++b) {
        if(!(partial && a==modulus-1 && b==modulus-1))
            append(add,a,b,nonassociative?(a+modulus-b)%modulus:(a+b)%modulus,malformed && a==0 && b==0);
        append(multiply,a,b,(a*b)%modulus);
    }
    if(duplicate)append(add,0,0,0);
    if(conflict)append(add,0,0,1);
    return {builder.freeze(),{{1,1,add,left,right,output},{2,1,multiply,left,right,output}},std::move(values)};
}
} // namespace finite_algebra_example
