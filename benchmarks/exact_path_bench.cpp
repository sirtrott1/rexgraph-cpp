#include "rfc/core/builder.hpp"
#include "rfc/path/objectives.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>

int main(){
    constexpr std::uint32_t n=20000;
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    for(std::uint32_t i=0;i+1<n;++i) b.add_grade_one(edge,{{i,true,true,false,1},{i+1,true,false,false,1}});
    for(std::uint32_t i=0;i+10<n;i+=10) b.add_grade_one(edge,{{i,true,true,false,1},{i+10,true,false,false,1}});
    const auto complex=b.freeze();
    rfc::numeric::RationalVector lengths(complex.grade(rfc::core::Grade{1}).cell_count(),1);
    for(std::size_t i=n-1;i<lengths.size();++i) lengths[i]=9; // shortcuts are useful but not free
    const auto view=rfc::view::project_pairwise_directed(complex,lengths);
    const auto source=view.object_index(0), target=view.object_index(n-1);
    const auto start=std::chrono::steady_clock::now();
    const auto route=rfc::path::dijkstra_exact(view,source,target);
    const auto stop=std::chrono::steady_clock::now();
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();
    std::cout<<"objects="<<view.object_count()<<" relations="<<view.edges().size()<<" distance="<<route.distance
             <<" route_edges="<<route.relations.size()<<" elapsed_ns="<<ns<<"\n";
    return route.reachable?0:1;
}
