#pragma once
#include "rfc/database/query_tensor_work.hpp"
#include "rfc/database/interpretation_codec.hpp"
#include <iomanip>
#include <sstream>

namespace query_tensor_sample {
using namespace rfc;
using namespace rfc::database;
using V=composite::DeclaredValue;
using Q=numeric::Rational;
inline composite::DeclarationId id(std::uint64_t key){return {6403,key,1};}
inline InterpretationSchema schema() {
    using D=composite::Declaration;
    InterpretationSchema s;s.source_namespace=6401;s.binding_namespace=6402;
    s.values=composite::DeclarationRegistry({D::rational(id(1)),D::optional(id(2),id(1)),
        D::boolean(id(3)),D::finite(id(4),{7,9}),D::integer(id(5)),
        D::product(id(6),{{11,id(2),0},{12,id(3),0},{13,id(4),0},{14,id(5),0}}),
        D::product(id(7),{{21,id(6),0}})});
    s.assertions={{10,"measurements",id(7),{}, {}}};return s;
}
inline std::string token(std::size_t i) {std::ostringstream out;out<<std::setfill('0')<<std::setw(12)<<i;return out.str();}
inline V payload(std::optional<Q> q,bool flag,std::uint64_t category,numeric::Integer n) {
    if (q) q->canonicalize();
    return V::product({V::product({q?V::present(V::rational(*q)):V::absent(),V::boolean(flag),V::symbol(category),V::integer(std::move(n))})});
}
inline LogicalSnapshot input(std::size_t rows) {
    LogicalSnapshot in;in.revision="measurements/0";
    for (std::size_t i=0;i<rows;++i) {
        Q q{static_cast<long>(i%13)-6,static_cast<unsigned long>(i%7+1)};q.canonicalize();
        const numeric::Integer n=numeric::Integer("123456789012345678901234567890")+numeric::Integer(std::to_string(i%5));
        in.assertions.push_back({{10,token(i)},payload(i%5?std::optional<Q>{q}:std::nullopt,i%2==0,i%3?7:9,n),{}});
    }
    return in;
}
inline NativeQueryRequest request(NativeQueryOutput output=NativeQueryOutput::Keys) {
    return {{{{21,11},ExactPredicateKind::Greater,V::rational(Q{0})},
             {{21,12},ExactPredicateKind::Equal,V::boolean(true)},
             {{21,13},ExactPredicateKind::NotEqual,V::symbol(9)}},std::vector<std::uint64_t>{21,11},output};
}
inline InterpretedSnapshot tensor_source(numeric::SparseRationalTensor tensor) {
    using D=composite::Declaration;
    std::vector<composite::DeclarationAxis> axes;
    for (std::size_t i=0;i<tensor.domain().rank();++i) axes.push_back({i+1,tensor.domain().extents()[i],{}});
    InterpretationSchema s;s.source_namespace=6411;s.binding_namespace=6412;
    s.values=composite::DeclarationRegistry({D::rational_tensor(id(8),std::move(axes))});
    s.assertions={{20,"tensor",id(8),{}, {}}};
    LogicalSnapshot in;in.revision="tensor/0";in.assertions={{{20,"tensor"},V::tensor(std::move(tensor)),{}}};
    return InterpretedSnapshot::interpret(s,in);
}
inline runtime::WorkAvailability available(const InterpretedSnapshot& source) {return {source.schema().source_namespace,source.sequence()};}
} // namespace query_tensor_sample
