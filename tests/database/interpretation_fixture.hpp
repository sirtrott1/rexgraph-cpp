#pragma once
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/logical_manifest.hpp"
#include "rfc/models/native_context.hpp"
namespace f5_fixture {
using namespace rfc;
using namespace rfc::database;
using V=composite::DeclaredValue;
using Q=numeric::Rational;
inline composite::DeclarationId id(std::uint64_t key){return {503,key,1};}
inline InterpretationSchema schema(){
    using D=composite::Declaration;
    InterpretationSchema s;s.source_namespace=501;s.binding_namespace=502;
    s.values=composite::DeclarationRegistry({D::boolean(id(1)),D::rational(id(2)),D::optional(id(3),id(2)),
        D::finite(id(4),{7,8,9}),D::repeated(id(5),id(3),0,16),D::optional(id(6),id(3)),
        D::rational_tensor(id(7),{{1,2,{}},{2,3,{}}}),
        D::product(id(8),{{1,id(6),0},{2,id(4),0},{3,id(5),0},{4,id(7),0}})});
    s.subjects={{1,"people"},{2,"unused subjects"}};
    s.assertions={{10,"event",id(8),{{11,1,0,4,true,true},{12,1,0,4,true,true}},{{1,"quantity"},{2,"category"},{3,"nested"},{4,"tensor"}}}};
    return s;
}
inline V tensor(){return V::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({2,3}),numeric::StructuralSupport::sparse(6,{0,1,5}),numeric::SparseRationalVector(6,{{0,Q{3}},{5,Q{-2}}})));}
inline V payload(int state=2,Q quantity=Q{5}){
    const auto q=state==0?V::absent():state==1?V::present(V::absent()):V::present(V::present(V::rational(quantity)));
    return V::product({q,V::symbol(8),V::repeated({V::absent(),V::present(V::rational(Q{0})),V::present(V::rational(Q{1,3}))}),tensor()});
}
inline LogicalSnapshot snapshot(){
    LogicalSnapshot x;x.revision="snapshot/0";x.subjects={{1,"p1"},{1,"p2"},{1,"p3"},{2,"unused"}};
    x.assertions={{{10,"a1"},payload(),{{11,ReferenceState::Present,{{"a",{1,"p1"},true},{"b",{1,"p1"},false}}},{12,ReferenceState::Present,{{"c",{1,"p1"},true}}}}},
                  {{10,"a2"},payload(),{{11,ReferenceState::Null,{}},{12,ReferenceState::Present,{}}}},
                  {{10,"a3"},payload(0),{{11,ReferenceState::Missing,{}},{12,ReferenceState::Null,{}}}},
                  {{10,"a4"},payload(1),{{11,ReferenceState::Present,{{"a",{1,"p2"},true}}},{12,ReferenceState::Missing,{}}}}};
    return x;
}
inline bool same(const LogicalSnapshot& a,const LogicalSnapshot& b){
    if(a.revision!=b.revision || a.subjects!=b.subjects || a.assertions.size()!=b.assertions.size())return false;
    for(std::size_t i=0;i<a.assertions.size();++i){const auto& x=a.assertions[i];const auto& y=b.assertions[i];
        if(x.origin!=y.origin || x.roles!=y.roles || !composite::declared_value_equal(x.payload,y.payload))return false;}return true;
}
inline LogicalSnapshot next(){auto x=snapshot();x.revision="snapshot/1";x.subjects.push_back({1,"a-new-earlier-token"});
    x.assertions.erase(x.assertions.begin()+1);x.assertions[0].payload=payload(2,Q{0});
    std::reverse(x.assertions[0].roles[0].occurrences.begin(),x.assertions[0].roles[0].occurrences.end());
    x.assertions.push_back({{10,"a5"},payload(2,Q{-17,19}),{{11,ReferenceState::Present,{{"new",{1,"a-new-earlier-token"},true}}},{12,ReferenceState::Present,{}}}});return x;}
}
