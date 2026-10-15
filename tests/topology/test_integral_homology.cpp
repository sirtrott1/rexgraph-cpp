#include "rfc/core/builder.hpp"
#include "rfc/numeric/smith_normal_form.hpp"
#include "rfc/topology/integral_homology.hpp"
#include "rfc/topology/modular.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::core::RelationalComplex make_circle() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{1,true,false,false,1}});
    b.add_grade_one(edge,{{1,true,true,false,1},{2,true,false,false,1}});
    b.add_grade_one(edge,{{2,true,true,false,1},{0,true,false,false,1}});
    return b.freeze();
}

static rfc::core::RelationalComplex make_projective_plane() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("projective-face",2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{0,true,false,false,1}});
    return b.freeze();
}

static rfc::core::RelationalComplex make_klein_bottle() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("klein-face",4,4);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // a
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}}); // b
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{0,true,true,false,1},{1,true,false,false,1}});
    return b.freeze();
}

int main() {
    try {
        using namespace rfc;
        numeric::IntegerMatrix sample(3,3);
        const long values[3][3]={{1,1,0},{1,0,1},{0,1,1}};
        for(std::size_t i=0;i<3;++i)for(std::size_t j=0;j<3;++j)sample.set(i,j,numeric::Integer{values[i][j]});
        const auto snf=numeric::smith_normal_form(sample);
        require(snf.rank==3u && snf.invariant_factors.size()==3u,"SNF rank mismatch");
        require(snf.invariant_factors[0]==1 && snf.invariant_factors[1]==1 && snf.invariant_factors[2]==2,"SNF invariant mismatch");
        require(snf.left*sample*snf.right==snf.diagonal,"SNF factorization mismatch");

        const auto circle=make_circle();
        const auto h0=topology::integral_homology(circle,core::Grade{0});
        const auto h1=topology::integral_homology(circle,core::Grade{1});
        require(h0.free_rank==1u && h0.torsion_invariants.empty(),"circle H0 mismatch");
        require(h1.free_rank==1u && h1.torsion_invariants.empty(),"circle H1 mismatch");
        require(topology::integral_chain_certificate(circle,core::Grade{1},{1,1,1}).represents_nonzero_homology_class(),
                "primitive circle cycle lost its nonzero integral class");
        require(!topology::integral_chain_certificate(circle,core::Grade{1},{1,0,0}).cycle,
                "an open edge received an integral cycle certificate");
        require(topology::integral_boundary_matrix(circle,core::Grade{1})*h1.cycle_basis==numeric::IntegerMatrix(3,1),
                "reported integral cycle basis is not closed");

        const auto rp2=make_projective_plane();
        const auto rp_h1=topology::integral_homology(rp2,core::Grade{1});
        const auto rp_h2=topology::integral_homology(rp2,core::Grade{2});
        require(rp_h1.free_rank==0u && rp_h1.torsion_invariants.size()==1u && rp_h1.torsion_invariants[0]==2,
                "projective-plane H1 torsion mismatch");
        require(rp_h2.free_rank==0u && rp_h2.torsion_invariants.empty(),"projective-plane H2 mismatch");
        const auto torsion_generator=topology::integral_chain_certificate(rp2,core::Grade{1},{1});
        require(torsion_generator.represents_nonzero_homology_class(),"RP2 torsion generator was mistaken for a boundary");
        for(const long multiple:{-2L,0L,2L}) {
            const auto certificate=topology::integral_chain_certificate(rp2,core::Grade{1},{numeric::Integer{multiple}});
            require(certificate.cycle && certificate.boundary && certificate.boundary_preimage==std::vector<numeric::Integer>{numeric::Integer{multiple/2}},
                    "Smith action lost an exact integral torsion boundary witness");
        }
        numeric::Integer large{1}; large<<=160;
        const auto large_certificate=topology::integral_chain_certificate(rp2,core::Grade{1},{numeric::Integer{2}*large});
        require(large_certificate.boundary && large_certificate.boundary_preimage==std::vector<numeric::Integer>{large},
                "integral torsion certificate narrowed an arbitrary-precision coefficient");
        require(rp_h1.cycle_basis*rp_h1.boundaries_in_cycle_coordinates==topology::integral_boundary_matrix(rp2,core::Grade{2}),
                "RP2 boundary cycle coordinates do not reconstruct the retained boundary");
        require(topology::modular_homology_dimension(rp2,core::Grade{1},2u).dimension==1u,
                "projective-plane mod-2 H1 mismatch");
        require(topology::modular_homology_dimension(rp2,core::Grade{2},2u).dimension==1u,
                "projective-plane mod-2 H2 mismatch");
        require(topology::modular_homology_dimension(rp2,core::Grade{1},3u).dimension==0u &&
                topology::modular_homology_dimension(rp2,core::Grade{2},3u).dimension==0u,
                "projective-plane mod-3 homology mismatch");

        const auto klein=make_klein_bottle();
        const auto k_h1=topology::integral_homology(klein,core::Grade{1});
        require(k_h1.free_rank==1u && k_h1.torsion_invariants.size()==1u && k_h1.torsion_invariants[0]==2,
                "Klein-bottle H1 mismatch");
        require(topology::integral_chain_certificate(klein,core::Grade{1},{0,1}).represents_nonzero_homology_class() &&
                topology::integral_chain_certificate(klein,core::Grade{1},{0,2}).boundary,
                "Klein-bottle free/torsion lattice membership changed");

        numeric::IntegerMatrix unimodular(2,2);
        unimodular.set(0,0,2); unimodular.set(0,1,1); unimodular.set(1,0,1); unimodular.set(1,1,1);
        const auto inverse=topology::unimodular_inverse(unimodular);
        require(inverse.get(0,0)==1 && inverse.get(0,1)==-1 && inverse.get(1,0)==-1 && inverse.get(1,1)==2,
                "explicit unimodular inverse changed under integral action replay");
        numeric::IntegerMatrix nonunimodular(2,2); nonunimodular.set(0,0,2); nonunimodular.set(1,1,1);
        bool rejected=false;
        try{(void)topology::unimodular_inverse(nonunimodular);}catch(const std::logic_error&){rejected=true;}
        require(rejected,"explicit inverse accepted a nonunimodular integral matrix");

        std::cout<<"integral Smith/torsion homology tests passed\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return EXIT_FAILURE;
    }
}
