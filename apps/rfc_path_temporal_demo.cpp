#include "rfc/rfc.hpp"
#include <iostream>

int main(){
    using namespace rfc;
    core::RelationSignature sig;
    const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(sig);
    auto add=[&](std::uint64_t s,std::uint64_t t){b.add_grade_one(edge,{{s,true,true,false,1},{t,true,false,false,1}});};
    add(0,1); add(1,3); add(0,2); add(2,3); add(0,3);
    const auto complex=b.freeze();
    const numeric::RationalVector lengths{2,2,1,5,5};
    const auto graph=view::project_pairwise_directed(complex,lengths);
    const auto s=graph.object_index(0), t=graph.object_index(3);
    const auto bfs=path::bfs_min_hops(graph,s,t);
    const auto dij=path::dijkstra_exact(graph,s,t);
    std::cout<<"bfs_hops="<<bfs.distance<<" dijkstra_length="<<dij.distance<<"\n";

    const auto B=field::boundary_matrix(complex,core::Grade{1});
    const auto pstar=path::least_quadrance_feasible(B,field::ExactMetric::diagonal(lengths),dij.materialize_source_field());
    std::cout<<"least_quadrance_field=";
    for(const auto& x:pstar) std::cout<<' '<<x;
    std::cout<<"\n";

    const auto R0=numeric::ExactMatrix::from_rows({{1,0,0},{0,numeric::Rational{1,2},0},{0,numeric::Rational{1,2},1}});
    const auto R1=numeric::ExactMatrix::from_rows({{numeric::Rational{1,2},0,0},{numeric::Rational{1,2},1,0},{0,0,1}});
    temporal::ChronologicalEvolution original({1,0,0},{R0,R1},{numeric::RationalVector(3,0),numeric::RationalVector(3,0)});
    temporal::ChronologicalEvolution reversed({1,0,0},{R1,R0},{numeric::RationalVector(3,0),numeric::RationalVector(3,0)});
    std::cout<<"chronological_original=";
    for(const auto& x:original.terminal()) std::cout<<' '<<x;
    std::cout<<"\nchronological_reversed=";
    for(const auto& x:reversed.terminal()) std::cout<<' '<<x;
    std::cout<<"\n";
    return 0;
}
