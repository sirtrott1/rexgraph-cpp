#include "rfc/core/builder.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/fundamental_group.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_filled_triangle() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("face",3,3);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}}); // 01
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}}); // 12
    b.add_grade_one(edge,{{0,true,true,false,1},{2,true,false,false,1}}); // 02
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    return b.freeze();
}

static rfc::core::RelationalComplex make_one_vertex_surface(bool torus_word) {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("face",4,4);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // a
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // b
    if(torus_word) {
        b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{0,true,true,false,1},{1,true,true,false,1}});
    } else {
        b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{0,true,true,false,1},{1,true,false,false,1},{1,true,true,false,1}});
    }
    return b.freeze();
}

static rfc::core::RelationalComplex make_projective_plane() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("face",2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{0,true,false,false,1}});
    return b.freeze();
}

static rfc::core::RelationalComplex make_klein_bottle() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("face",4,4);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // a
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // b
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{0,true,true,false,1},{1,true,false,false,1}});
    return b.freeze();
}

int main() {
    try {
        using namespace rfc;
        const auto triangle=make_filled_triangle();
        topology::PairwisePathGroupoid g(triangle);
        const auto loop=topology::PairwisePath::compose(topology::PairwisePath::compose(g.generator(0),g.generator(1)),g.generator(2,false));
        const auto presentation=topology::fundamental_groupoid_presentation(triangle);
        const auto identity=g.identity(0u);
        require(topology::certify_relational_homotopy(presentation,loop,identity,{{0u,0u,false,false,0u}}),
                "filled triangle attaching loop should be null-homotopic by one certified face move");

        const auto torus=make_one_vertex_surface(true);
        const auto trivial_face=make_one_vertex_surface(false);
        require(topology::integral_boundary_matrix(torus,core::Grade{2}) == topology::integral_boundary_matrix(trivial_face,core::Grade{2}),
                "comparison surfaces should have identical abelianized B2");
        const auto torus_word=topology::attaching_word(torus,0u).loop.steps();
        const auto trivial_word=topology::attaching_word(trivial_face,0u).loop.steps();
        require(torus_word != trivial_word,"retained attaching words should distinguish equal B2 realizations");

        const auto torus_group=topology::fundamental_group_presentation(torus);
        require(torus_group.generator_relations.size()==2u && torus_group.relators.size()==1u,"torus group presentation size mismatch");
        require(torus_group.relators[0].letters().size()==4u,"torus commutator relator mismatch");
        const auto torus_ab=topology::abelianization(torus_group);
        const auto torus_h1=topology::integral_homology(torus,core::Grade{1});
        require(torus_ab.free_rank==2u && torus_ab.torsion_invariants.empty(),"torus presentation abelianization mismatch");
        require(torus_ab.free_rank==torus_h1.free_rank && torus_ab.torsion_invariants==torus_h1.torsion_invariants,
                "torus group abelianization and H1 disagree");

        const auto rp2=make_projective_plane();
        const auto rp_group=topology::fundamental_group_presentation(rp2);
        const auto rp_ab=topology::abelianization(rp_group);
        require(rp_group.generator_relations.size()==1u && rp_group.relators[0].letters().size()==2u,"RP2 presentation mismatch");
        require(rp_ab.free_rank==0u && rp_ab.torsion_invariants.size()==1u && rp_ab.torsion_invariants[0]==2,"RP2 abelianization mismatch");

        const auto klein=make_klein_bottle();
        const auto klein_group=topology::fundamental_group_presentation(klein);
        const auto klein_ab=topology::abelianization(klein_group);
        const auto klein_h1=topology::integral_homology(klein,core::Grade{1});
        require(klein_ab.free_rank==1u && klein_ab.torsion_invariants.size()==1u && klein_ab.torsion_invariants[0]==2,
                "Klein presentation abelianization mismatch");
        require(klein_ab.free_rank==klein_h1.free_rank && klein_ab.torsion_invariants==klein_h1.torsion_invariants,
                "Klein group abelianization and H1 disagree");

        std::cout<<"relational attaching-word/fundamental-homotopy tests passed\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return EXIT_FAILURE;
    }
}
