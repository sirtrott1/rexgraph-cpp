#include "rfc/inference/native_field_updates.hpp"
#include "fixture.hpp"
#include <iostream>

int main(){try{
    using namespace rfc;using namespace native_update_example;
    // The paper's exact three-participant head-change Gram update. Both
    // operators and complete directions are declared on this retained endpoint.
    const auto b=Matrix::from_rows({{-1},{Q{1,2}},{Q{1,2}}});
    const auto change=Matrix::from_rows({{Q{3,2}},{Q{-3,2}},{0}});
    Matrix u(3,2);for(std::size_t i=0;i<3;++i){u.set(i,0,b.get(i,0));u.set(i,1,change.get(i,0));}
    const auto a=Matrix::identity(3).add(b.multiply(b.transpose())),c=Matrix::from_rows({{0,1},{1,1}});
    auto e=endpoint(a,u,c,u.transpose());
    core::RelationSignature signature;
    const auto type=signature.add_type("head-change",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100},type,{{10,true,true,1},{11,true,false,Q{1,2}},
        {12,true,false,Q{1,2}},{11,true,true,2},{11,true,false,2},{10,false,false,7}});
    e.source=builder.freeze();e.tensors[5]=native_update_example::field(100,10,{1,0,0});
    e.tensors[6]=native_update_example::field(101,10,{0,1,0});
    storage::NativeAccessionDirectory directory(e,900);
    const auto operator_pin=[&](auto key){return directory.pin(access(storage::NativeAccessionKind::Operator,key));};
    inference::NativeWoodburyRequest request{operator_pin(1),operator_pin(2),operator_pin(3),operator_pin(4),operator_pin(5)};
    const auto base=inference::PreparedNativeWoodburySolve::prepare_base(request.base);
    const auto plan=inference::PreparedNativeWoodburySolve::prepare(request,base,runtime::ExecutionLease{},runtime::WorkAvailability{900,7});
    std::cout<<"namespace=900 sequence=7 source_occurrences="<<e.source.grade(core::Grade{1}).incidence_count()
             <<" native_preparation_steps="<<plan.preparation_steps()<<'\n';
    for(const auto key:{100u,101u}){
        const auto input=directory.pin(access(storage::NativeAccessionKind::Field,key));
        const auto result=plan.execute(input,runtime::ExecutionLease{},runtime::WorkAvailability{900,7});
        const Vector expected=key==100u?Vector{Q{9,10},Q{1,5},Q{-1,10}}:Vector{Q{1,5},Q{3,5},Q{1,5}};
        if(result.values()!=expected)throw std::logic_error("exact head-change response mismatch");
        std::cout<<"field="<<key<<" response=";for(const auto& value:result.values())std::cout<<' '<<value;
        std::cout<<" execution_steps="<<result.provenance().execution_steps<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
