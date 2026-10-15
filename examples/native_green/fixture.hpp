#pragma once
#include "rfc/inference/native_green_response.hpp"
#include "../native_updates/fixture.hpp"

namespace native_green_example {
using M=rfc::numeric::ExactMatrix;
using V=rfc::numeric::RationalVector;
using Q=rfc::numeric::Rational;
using Pin=rfc::storage::PinnedNativeAccession;
using native_update_example::access;
inline rfc::storage::TemporalEndpoint endpoint(const M& L,const M& I,const M& A,std::uint64_t sequence) {
    using namespace rfc;
    storage::TemporalEndpoint e(native_update_example::source(L.rows()),sequence);
    e.carriers={{10,storage::TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(L.rows())},
        {20,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},200,numeric::IndexDomain::linear(I.cols())},
        {30,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},300,numeric::IndexDomain::linear(A.rows())},
        {40,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},400,numeric::IndexDomain::linear(I.cols())}};
    e.tensors={native_update_example::map(1,10,10,L),native_update_example::map(2,20,10,I),
        native_update_example::map(3,10,30,A,storage::TemporalTensorRole::Accession)};
    for(unsigned sample=0;sample<2;++sample){V values(I.cols());
        for(std::size_t i=0;i<values.size();++i)values[i]=Q{static_cast<long>(i+sample+sequence)-8,static_cast<unsigned long>(i+2)};
        e.tensors.push_back(native_update_example::field(100+sample,20,values));
    }
    e.tensors.push_back(native_update_example::field(102,40,V(I.cols(),Q{1})));return e;
}
inline Pin op(const rfc::storage::NativeAccessionDirectory& directory,std::uint64_t key) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Operator,key));
}
inline Pin input(const rfc::storage::NativeAccessionDirectory& directory,std::uint64_t key=100) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Field,key));
}
inline rfc::inference::NativeGreenCorrespondence comparison(Pin old_pin,Pin new_pin,std::uint64_t old_key,
                                                            std::uint64_t new_key,const M& map) {
    return {std::move(old_pin),std::move(new_pin),old_key,new_key,rfc::storage::temporal_matrix_tensor(map)};
}
inline rfc::inference::NativeGreenResponseRequest request(const rfc::storage::NativeAccessionDirectory& old_directory,
        const rfc::storage::NativeAccessionDirectory& new_directory,const M& J,const M& K,Q lambda=1) {
    const auto a=op(old_directory,1),b=op(new_directory,1);
    return {a,b,op(old_directory,2),op(new_directory,2),comparison(a,b,10,10,J),comparison(a,b,20,20,K),std::move(lambda)};
}
inline std::pair<rfc::storage::TemporalEndpoint,rfc::storage::TemporalEndpoint> square() {
    using namespace rfc;
    core::RelationSignature signature;const auto type=signature.add_type("native-square",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
    for(std::uint64_t i=0;i<4;++i)builder.add_grade_one(core::RelationKey{100+i},type,
        {{i,true,true,1},{(i+1)%4,true,false,1},{i,true,true,2},{i,true,false,2},{(i+2)%4,false,false,7}});
    const auto old_source=builder.freeze();
    builder.add_higher(core::Grade{2},core::RelationKey{200},type,
        {{0,true,false,1},{1,true,false,1},{2,true,false,1},{3,true,false,1}});
    const auto new_source=builder.freeze();
    const auto B=field::materialize_boundary_matrix(old_source,core::Grade{1}),c=field::materialize_boundary_matrix(new_source,core::Grade{2});
    const auto L=B.transpose().multiply(B);
    auto old_e=endpoint(L,M::from_rows({{1},{1},{0},{0}}),M::from_rows({{1,0,0,0},{0,1,0,0}}),7);
    auto new_e=endpoint(L.add(c.multiply(c.transpose())),M::from_rows({{0},{0},{-1},{-1}}),M::from_rows({{1,0,1,0},{0,1,0,1}}),8);
    old_e.source=old_source;new_e.source=new_source;old_e.carriers[0].grade=core::Grade{1};new_e.carriers[0].grade=core::Grade{1};
    old_e.tensors[3]=native_update_example::field(100,20,{2});new_e.tensors[3]=native_update_example::field(100,20,{3});
    return {std::move(old_e),std::move(new_e)};
}
} // namespace native_green_example
