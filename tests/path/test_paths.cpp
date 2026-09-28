#include "rfc/core/builder.hpp"
#include "rfc/field/exact_hodge.hpp"
#include "rfc/path/objectives.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

static rfc::core::RelationalComplex make_graph(){
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    auto add=[&](std::uint64_t s,std::uint64_t t){
        b.add_grade_one(edge,{{s,true,true,false,1},{t,true,false,false,1}});
    };
    add(0,1); add(1,3); add(0,2); add(2,3); add(0,3);
    return b.freeze();
}

int main(){
try{
    using rfc::numeric::Rational;
    using rfc::numeric::RationalVector;
    const auto complex=make_graph();
    const RationalVector lengths{2,2,1,5,5};
    const auto view=rfc::view::project_pairwise_directed(complex, lengths);
    const auto source=view.object_index(0); const auto target=view.object_index(3);
    const auto bfs=rfc::path::bfs_min_hops(view,source,target);
    require(bfs.reachable && bfs.relations.size()==1 && bfs.relations.front()==4,"BFS did not select direct edge");
    const auto dij=rfc::path::dijkstra_exact(view,source,target);
    require(dij.reachable && dij.distance==4 && dij.relations==std::vector<std::uint32_t>({0,1}),"Dijkstra exact path mismatch");

    const auto B=rfc::field::boundary_matrix(complex,rfc::core::Grade{1});
    RationalVector demand(complex.grade(rfc::core::Grade{0}).cell_count(),0); demand[source]=-1; demand[target]=1;
    RationalVector potential(complex.grade(rfc::core::Grade{0}).cell_count(),0);
    potential[view.object_index(0)]=0; potential[view.object_index(1)]=2; potential[view.object_index(2)]=1; potential[view.object_index(3)]=4;
    require(rfc::path::shortest_path_certificate(B,lengths,dij.materialize_source_field(),demand,potential),"shortest path certificate failed");

    const auto metric=rfc::field::ExactMetric::diagonal(lengths);
    const auto minimum=rfc::path::least_quadrance_feasible(B,metric,dij.materialize_source_field());
    const RationalVector expected{Rational{15,37},Rational{15,37},Rational{10,37},Rational{10,37},Rational{12,37}};
    require(minimum==expected,"least-quadrance field mismatch");
    require(B.apply(minimum)==demand,"least-quadrance field lost boundary demand");
    require(metric.inner(minimum,minimum)==Rational{60,37},"least-quadrance value mismatch");

    const auto p=rfc::path::realize(complex,rfc::core::Grade{1},{{0,1},{1,1}});
    const auto q=rfc::path::realize(complex,rfc::core::Grade{1},{{2,1},{3,1}});
    const auto gram=rfc::path::inherited_gram({p,q},rfc::field::ExactMetric::identity(5));
    require(gram.get(0,0)==2 && gram.get(1,1)==2,"path inherited gram mismatch");

    std::cout<<"exact path/objective tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
