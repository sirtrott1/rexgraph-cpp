#include "rfc/core/builder.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/attaching_relation.hpp"
#include "rfc/topology/cylinder.hpp"
#include "rfc/topology/fundamental_group.hpp"
#include "rfc/topology/integral_homology.hpp"
#include "rfc/topology/modular.hpp"
#include <iostream>

static rfc::core::RelationalComplex projective_plane() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("projective-face",2,2);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{0,true,false,false,1}});
    return b.freeze();
}

static rfc::core::RelationalComplex torus() {
    rfc::core::RelationSignature sig;
    const auto edge=sig.add_type("loop",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_attaching_loop_type("torus-face",4,4);
    rfc::core::RelationalComplexBuilder b(sig);
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    b.add_higher(rfc::core::Grade{2},face,{{0,true,false,false,1},{1,true,false,false,1},{0,true,true,false,1},{1,true,true,false,1}});
    return b.freeze();
}


static rfc::core::RelationalComplex nested_branch(bool fill) {
    using namespace rfc;
    core::RelationSignature sig;
    const auto branch=sig.add_branch_type("branch",2,0);
    const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(branch,{{100,true,true,true,numeric::Rational{1}},
                            {101,true,false,false,numeric::Rational{1,3}},
                            {102,true,false,false,numeric::Rational{1,3}},
                            {103,true,false,false,numeric::Rational{1,3}}});
    b.add_grade_one(edge,{{100,true,true,numeric::Rational{1}},{101,true,false,numeric::Rational{1}}});
    b.add_grade_one(edge,{{100,true,true,numeric::Rational{1}},{102,true,false,numeric::Rational{1}}});
    b.add_grade_one(edge,{{100,true,true,numeric::Rational{1}},{103,true,false,numeric::Rational{1}}});
    if(fill) b.add_canonical_higher(core::Grade{2},branch,{0,1,2,3},0);
    return b.freeze();
}

static rfc::core::RelationalComplex unequal_branch() {
    using namespace rfc;
    core::RelationSignature sig;
    const auto branch=sig.add_branch_type("weighted-branch",4,4);
    core::RelationalComplexBuilder b(sig);
    b.add_weighted_branch_grade_one(branch,{10,11,12,13},0,
        {numeric::Rational{1,4},numeric::Rational{1,2},numeric::Rational{1,4}});
    return b.freeze();
}

int main() {
    using namespace rfc;
    const auto rp2=projective_plane();
    const auto h1=topology::integral_homology(rp2,core::Grade{1});
    std::cout<<"RP2_H1_free="<<h1.free_rank<<" torsion=";
    for(const auto& d:h1.torsion_invariants) std::cout<<d<<' ';
    std::cout<<"\nRP2_mod2_H1="<<topology::modular_homology_dimension(rp2,core::Grade{1},2u).dimension
             <<" RP2_mod2_H2="<<topology::modular_homology_dimension(rp2,core::Grade{2},2u).dimension<<'\n';

    const auto t=torus();
    const auto presentation=topology::fundamental_group_presentation(t);
    const auto ab=topology::abelianization(presentation);
    std::cout<<"torus_generators="<<presentation.generator_relations.size()
             <<" relator_length="<<presentation.relators.at(0).letters().size()
             <<" abelian_free_rank="<<ab.free_rank<<'\n';
    std::cout<<"torus_relator=";
    for(const auto& letter:presentation.relators.at(0).letters())
        std::cout<<(letter.positive?"+":"-")<<letter.generator<<' ';
    std::cout<<'\n';

    const auto branch_open=nested_branch(false);
    const auto branch_filled=nested_branch(true);
    const auto open_bh1=topology::integral_homology(branch_open,core::Grade{1});
    const auto filled_bh1=topology::integral_homology(branch_filled,core::Grade{1});
    std::cout<<"branch_open_H1="<<open_bh1.free_rank
             <<" branch_filled_H1="<<filled_bh1.free_rank<<'\n';

    const auto weighted=unequal_branch();
    topology::RelationalCylinder cylinder(weighted);
    const auto terms=cylinder.prism_boundary(core::CellId{core::Grade{1},0u});
    std::cout<<"weighted_branch_prism_terms="<<terms.size()<<" coefficients=";
    for(const auto& term:terms) std::cout<<term.coefficient<<' ';
    std::cout<<'\n';
}
