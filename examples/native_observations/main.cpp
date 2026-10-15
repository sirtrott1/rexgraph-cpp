#include "fixture.hpp"
#include <iostream>

int main(){try{
    using namespace native_observation_example;
    using Solve=inference::PreparedNativeWoodburySolve;
    using Observation=inference::PreparedNativeSelectedObservation;
    const runtime::WorkAvailability available{900,7};const std::size_t n=32;
    const auto source=native_update_example::source(n);const auto B=field::materialize_boundary_matrix(source,core::Grade{1});
    const auto A=Matrix::identity(n).add(B.multiply(B.transpose()));const std::vector<std::size_t> coordinates{31,0,3,31};
    storage::NativeAccessionDirectory directory(endpoint(source,core::Grade{0},A,coordinates),900);
    const auto natural=Solve::prepare_base(op(directory),runtime::ExecutionLease{},available);
    const auto ordering=numeric::minimum_degree_ordering(A);
    const auto ordered=Solve::prepare_base_ordered(op(directory),ordering,runtime::ExecutionLease{},available);
    const auto plan=Observation::prepare(ordered,request(directory,coordinates),available);
    const auto result=plan.execute(input(directory),available);
    std::cout<<"source_occurrences="<<source.grade(core::Grade{1}).incidence_count()
             <<" natural_eliminations="<<natural.numeric_solve().preparation_stats().elimination_factor_entries
             <<" ordered_eliminations="<<ordered.numeric_solve().preparation_stats().elimination_factor_entries<<'\n';
    const auto print=[](const char* name,const Vector& values){std::cout<<name<<'=';for(const auto& value:values)std::cout<<' '<<value;std::cout<<'\n';};
    print("selected_star_field",result.values());
    if(result.values()!=Vector{Q{1,33},Q{2,33},Q{1,33},Q{1,33}})throw std::logic_error("star response");
    const auto endpoints=square();storage::NativeAccessionDirectory history(endpoints.first,900);
    const auto old_input=input(history);const auto old_plan=Observation::prepare(
        Solve::prepare_base(op(history),runtime::ExecutionLease{},available),request(history,{2,0,2}),available);
    history.advance(storage::make_temporal_segment(endpoints.first,endpoints.second));
    const auto new_plan=Observation::prepare(Solve::prepare_base(op(history)),request(history,{2,0,2}),{900,8});
    const auto old_result=old_plan.execute(old_input,available),new_result=new_plan.execute(input(history),{900,8});
    print("before_face_attachment",old_result.values());print("after_face_attachment",new_result.values());
    if(old_result.values()!=Vector{Q{2,15},Q{7,15},Q{2,15}} || new_result.values()!=Vector{Q{-1,15},Q{4,15},Q{-1,15}})
        throw std::logic_error("square response");
    std::cout<<"selected_relation_keys=";for(const auto& cell:old_plan.source_cells())std::cout<<' '<<cell.key;std::cout<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
