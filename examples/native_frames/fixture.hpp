#pragma once
#include "rfc/inference/native_hodge_frames.hpp"
#include "../native_green/fixture.hpp"

namespace native_frame_example {
using Matrix=rfc::numeric::ExactMatrix;
using Vector=rfc::numeric::RationalVector;
using Q=rfc::numeric::Rational;
using Pin=rfc::storage::PinnedNativeAccession;
using native_update_example::access;
inline rfc::core::RelationalComplex empty(std::uint16_t last) {
    rfc::core::RelationSignature signature;
    rfc::core::RelationalComplexBuilder builder(signature);builder.declare_grade(rfc::core::Grade{last});return builder.freeze();
}
inline rfc::core::RelationalComplex isolated() {
    rfc::core::RelationSignature signature;rfc::core::RelationalComplexBuilder builder(signature);
    for(std::uint64_t i=0;i<3;++i)builder.declare_witness_identity(100+i);
    return builder.freeze();
}
inline rfc::core::RelationalComplex pair() {
    rfc::core::RelationSignature signature;const auto type=signature.add_type("pair",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder builder(signature);builder.add_grade_one(rfc::core::RelationKey{1},type,{{10,true,true,Q{2,5}},{11,true,false,Q{3,7}}});
    return builder.freeze();
}
inline rfc::core::RelationalComplex branch() {
    rfc::core::RelationSignature signature;const auto type=signature.add_branch_type("branch",4,4);
    rfc::core::RelationalComplexBuilder builder(signature);
    for(unsigned i=0;i<3;++i)builder.add_canonical_grade_one(type,{10,11,12,13},0);
    return builder.freeze();
}
inline rfc::core::RelationalComplex higher() {
    rfc::core::RelationSignature signature;const auto type=signature.add_type("square",rfc::core::BoundaryPolicy::DeclaredExact,0,8);
    rfc::core::RelationalComplexBuilder builder(signature);
    for(std::uint64_t i=0;i<4;++i)builder.add_grade_one(rfc::core::RelationKey{100+i},type,
        {{i,true,true,1},{(i+1)%4,true,false,1},{i,true,true,2},{i,true,false,2},{(i+2)%4,false,false,7}});
    for(unsigned i=0;i<2;++i)builder.add_higher(rfc::core::Grade{2},rfc::core::RelationKey{200+i},type,
        {{0,true,false,1},{1,true,false,1},{2,true,false,1},{3,true,false,1}});
    builder.add_higher(rfc::core::Grade{3},rfc::core::RelationKey{300},type,{{0,true,false,1},{1,true,true,1}});
    return builder.freeze();
}
inline Matrix metric(std::size_t n,unsigned mode,std::size_t grade) {
    Matrix result(n,n);for(std::size_t i=0;i<n;++i){
        result.set(i,i,mode?Q{static_cast<unsigned long>(4+i+grade),3}:Q{1});
        if(mode==2 && i) {result.set(i,i-1,Q{1,3});result.set(i-1,i,Q{1,3});}
    }
    return result;
}
inline rfc::storage::TemporalEndpoint endpoint(rfc::core::RelationalComplex source,unsigned mode,std::uint64_t sequence=7) {
    rfc::storage::TemporalEndpoint e(std::move(source),sequence);
    for(std::size_t k=0;k<e.source.grade_count();++k){
        const auto grade=rfc::core::Grade{static_cast<std::uint16_t>(k)};const auto n=e.source.grade(grade).cell_count();
        const auto key=10+k;
        e.carriers.push_back({key,rfc::storage::TemporalCarrierKind::SourceGrade,grade,{},0,rfc::numeric::IndexDomain::linear(n)});
        e.carriers.push_back({300+k,rfc::storage::TemporalCarrierKind::Explicit,rfc::core::Grade{0},{},1000+k,rfc::numeric::IndexDomain::linear(n)});
        e.tensors.push_back(native_update_example::map(50+k,key,key,metric(n,mode,k),rfc::storage::TemporalTensorRole::Metric));
        for(unsigned sample=0;sample<2;++sample){Vector values(n);
            for(std::size_t i=0;i<n;++i)values[i]=sample?Q{static_cast<long>(i)-1,static_cast<unsigned long>(i+2)}:Q{i==0?1:0};
            e.tensors.push_back(native_update_example::field(100+2*k+sample,key,values));
        }
        e.tensors.push_back(native_update_example::field(3000+k,300+k,Vector(n,Q{1})));
    }
    return e;
}
inline Pin metric_tensor(const rfc::storage::NativeAccessionDirectory& directory,std::uint64_t key) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Tensor,key));
}
inline Pin input(const rfc::storage::NativeAccessionDirectory& directory,rfc::core::Grade grade,unsigned sample=0) {
    return directory.pin(access(rfc::storage::NativeAccessionKind::Field,100u+2u*static_cast<unsigned>(grade.value)+sample));
}
inline rfc::inference::NativeHodgeFrameRequest request(const rfc::storage::NativeAccessionDirectory& directory,rfc::core::Grade grade,unsigned mode) {
    rfc::inference::NativeHodgeFrameRequest result;result.grade=grade;
    result.metrics=mode==0?rfc::inference::NativeMetricSelection::NamedUnit:
        mode==1?rfc::inference::NativeMetricSelection::DeclaredDiagonal:rfc::inference::NativeMetricSelection::DeclaredPositive;
    const auto owner=input(directory,grade);
    for(std::size_t k=0;k<owner.endpoint()->source.grade_count();++k){
        result.grade_carriers.push_back(10+k);if(mode)result.metric_tensors.push_back(metric_tensor(directory,50+k));
    }
    return result;
}
} // namespace native_frame_example
