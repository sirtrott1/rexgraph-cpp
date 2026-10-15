#include "fixture.hpp"
#include <iostream>

int main(){try{
    using namespace native_linear_observation_example;
    using Solve=inference::PreparedNativeWoodburySolve;
    using Observation=inference::PreparedNativeLinearObservation;
    const auto endpoints=square();storage::NativeAccessionDirectory directory(endpoints.first,900);
    const runtime::WorkAvailability available{900,7};const auto old_input=input(directory);
    const auto A=storage::temporal_tensor_matrix(*op(directory).tensor_declaration());
    const auto base=Solve::prepare_base_ordered(op(directory),numeric::minimum_degree_ordering(A),runtime::ExecutionLease{},available);
    const auto plan=Observation::prepare(base,request(directory),available);
    const auto before=plan.execute(old_input,available);
    directory.advance(storage::make_temporal_segment(endpoints.first,endpoints.second));
    const auto next=Observation::prepare(Solve::prepare_base(op(directory)),request(directory),{900,8});
    const auto after=next.execute(input(directory),{900,8});
    const auto print=[](const char* name,const Vector& values){std::cout<<name<<'=';for(const auto& value:values)std::cout<<' '<<value;std::cout<<'\n';};
    print("before_face_attachment",before.values());print("after_face_attachment",after.values());
    if(before.values()!=Vector{Q{2,3},Q{1,15},Q{2,3},0,Q{4,3}} ||
       after.values()!=Vector{Q{4,15},Q{1,15},Q{4,15},0,Q{8,15}})throw std::logic_error("complete square observation");
    if(plan.execute(old_input,available).values()!=before.values())throw std::logic_error("retained old observation");
    const auto& p=before.provenance();
    std::cout<<"source_occurrences="<<endpoints.first.source.grade(core::Grade{1}).incidence_count()
        <<" output_rows="<<p.output_rows<<" distinct_coefficient_rows="<<p.distinct_coefficient_rows
        <<" transpose_certificates="<<p.certified_nonzero_rows<<'\n';
    std::cout<<"row_reuse_word=";for(const auto row:plan.numeric_plan().row_reuse_word())std::cout<<' '<<row;std::cout<<'\n';
    if(p.uses_full_solve || p.certified_nonzero_rows!=3 || p.output_rows!=5)throw std::logic_error("observation work policy");
    storage::NativeAccessionDirectory complete(endpoint(endpoints.first.source,core::Grade{1},A,Matrix::identity(4)),901);
    const auto full=Observation::prepare(Solve::prepare_base(op(complete)),request(complete),{901,7});
    const auto result=full.execute(input(complete),{901,7});print("complete_source_field",result.values());
    if(!result.provenance().uses_full_solve || result.provenance().certified_nonzero_rows!=0 ||
       result.values()!=Vector{Q{7,15},Q{1,5},Q{2,15},Q{1,5}})throw std::logic_error("complete primal solve");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
