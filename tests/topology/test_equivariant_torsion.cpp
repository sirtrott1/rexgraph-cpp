#include "rfc/core/builder.hpp"
#include "rfc/topology/equivariant_torsion.hpp"
#include "rfc/topology/integral_homology.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
using namespace rfc;
using Q=numeric::Rational;
using Guard=numeric::ExactActionGuard;
struct SourceDeclaration {core::RelationalComplex source;std::vector<std::vector<std::uint64_t>> voltages;};

static SourceDeclaration lens_chains(std::uint32_t p,const std::vector<std::uint32_t>& q,bool top=true){
    core::RelationSignature signature;
    const auto edge=signature.add_type("equivariant-loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("equivariant-polygon",p,p);
    const auto three=signature.add_type("equivariant-three-cell",core::BoundaryPolicy::DeclaredExact,2,3);
    core::RelationalComplexBuilder builder(signature);
    std::vector<std::vector<std::uint64_t>> voltages(top?4:3);
    for(std::size_t component=0;component<q.size();++component){
        builder.add_grade_one(edge,{{static_cast<std::uint32_t>(component),true,true,false,1},
                                    {static_cast<std::uint32_t>(component),true,false,false,1}});
        voltages[1].insert(voltages[1].end(),{0,1});
    }
    for(std::size_t component=0;component<q.size();++component){
        std::vector<core::HigherIncidenceInput> occurrences;
        for(std::uint32_t k=0;k<p;++k){occurrences.push_back({static_cast<std::uint32_t>(component),true,false,false,1});voltages[2].push_back(k);}
        builder.add_higher(core::Grade{2},face,occurrences);
    }
    if(top)for(std::size_t component=0;component<q.size();++component){
        std::uint64_t inverse=1;while(inverse<p&&(inverse*q[component])%p!=1)++inverse;
        if(inverse==p)throw std::invalid_argument("lens fixture needs an invertible gluing parameter");
        builder.add_higher(core::Grade{3},three,{{static_cast<std::uint32_t>(component),true,true,false,1},
                                                {static_cast<std::uint32_t>(component),true,false,false,1},
                                                {static_cast<std::uint32_t>(component),false,false,false,17}});
        voltages[3].insert(voltages[3].end(),{0,inverse,p-1});
    }
    return {builder.freeze(),std::move(voltages)};
}
static topology::PreparedCyclicEquivariantChain prepare(SourceDeclaration input,std::uint64_t p,Guard& guard){
    return topology::PreparedCyclicEquivariantChain::prepare(std::move(input.source),p,std::move(input.voltages),
        topology::EquivariantCoefficientBasis::NativeRational,81,1,guard);
}

int main(){try{
    Guard guard;const auto field=numeric::CertifiedAlgebraicField::declare_cyclotomic(7);const auto z=field.generator();
    const auto first=prepare(lens_chains(7,{1}),7,guard);
    const auto second=prepare(lens_chains(7,{2}),7,guard);
    require(first.source().grade_count()==4&&first.source().grade(core::Grade{3}).incidence_count()==3,
            "equivariant consumer discarded higher or inactive source occurrences");
    const auto h1a=topology::integral_homology(first.source(),core::Grade{1});
    const auto h1b=topology::integral_homology(second.source(),core::Grade{1});
    require(h1a.torsion_invariants==std::vector<numeric::Integer>{numeric::Integer{7}}&&h1a.torsion_invariants==h1b.torsion_invariants,
            "lens fixtures should have the same integral first homology");
    require(topology::integral_boundary_matrix(first.source(),core::Grade{3})==topology::integral_boundary_matrix(second.source(),core::Grade{3}),
            "lens fixtures should have equal aggregated untwisted top boundaries");
    const auto a=first.specialize(z,guard),b=second.specialize(z,guard);
    require(a.homology_dimensions(guard)==std::vector<std::size_t>({0,0,0,0})&&b.homology_dimensions(guard)==std::vector<std::size_t>({0,0,0,0}),
            "primitive lens character should give an acyclic full chain");
    const auto ta=a.torsion(guard),tb=b.torsion(guard);
    const auto one=field.scalar(Q{1});
    require(ta.value()==(z-one)*(z-one)&&tb.value()==(z-one)*(z.power(4)-one),"generic based torsion disagrees with the lens formula");
    require(!ta.equivalent_under_cyclotomic_units(tb,guard),"equal homology concealed distinct lens torsions");
    const auto conjugate=prepare(lens_chains(7,{6}),7,guard).specialize(z,guard).torsion(guard);
    require(ta.equivalent_under_cyclotomic_units(conjugate,guard),"allowed deck/lift changes lost torsion equivalence");
    const auto direct_sum=prepare(lens_chains(7,{1,2}),7,guard).specialize(z,guard).torsion(guard);
    require(direct_sum.value()==ta.value()*tb.value(),"generic multi-cell torsion is not multiplicative on the based direct sum");
    require(direct_sum.realization().equivariant_source().source().grade(core::Grade{1}).cell_count()==2,
            "torsion certificate did not pin the full native source");

    auto chain=first.regular_chain(core::Grade{3},numeric::SparseRationalVector(7,{{2,Q{1}}}),guard);
    const auto first_boundary=first.boundary(chain,guard),second_boundary=first.boundary(first_boundary,guard);
    require(first_boundary.values().nonzeros()==2&&second_boundary.values().nonzeros()==0,
            "cyclic regular-module action lost the exact full-chain identity");
    bool rejected=false;try{(void)second.boundary(chain,guard);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"equal-sized cyclic carriers bypassed retained source ownership");
    auto missing=lens_chains(7,{1});missing.voltages[3].pop_back();rejected=false;
    try{(void)prepare(std::move(missing),7,guard);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"equivariant declaration omitted an inactive retained occurrence");
    auto curved=lens_chains(7,{1});curved.voltages[2][1]=0;rejected=false;
    try{(void)prepare(std::move(curved),7,guard);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"nonflat group-ring declaration reached character specialization");
    rejected=false;try{(void)first.specialize(field.scalar(Q{2}),guard);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"character ignored the deck relation");
    rejected=false;try{(void)first.specialize(one,guard).torsion(guard);}catch(const std::domain_error&){rejected=true;}
    require(rejected,"ordinary lens homology received an acyclic torsion certificate");
    const auto moore=prepare(lens_chains(7,{1},false),7,guard).specialize(z,guard);
    require(moore.homology_dimensions(guard)==std::vector<std::size_t>({0,0,1}),"grade-two Moore sector lost its upper homology");
    rejected=false;try{(void)moore.torsion(guard);}catch(const std::domain_error&){rejected=true;}
    require(rejected,"torsion consumer silently truncated a nonacyclic upper grade");

    // Sparse algebraic determinant and selected solve use the existing exact
    // field arithmetic, with no coefficient rectangle or rational norm proxy.
    numeric::SparseAlgebraicMatrix matrix(2,2,field);matrix.set(0,0,z);matrix.set(0,1,one);matrix.set(1,0,one);matrix.set(1,1,z+one);
    require(matrix.determinant(guard)==z*(z+one)-one,"algebraic determinant returned a field norm instead of its scalar");
    numeric::SparseAlgebraicMatrix rhs(2,1,field);rhs.set(0,0,one);
    const auto solution=matrix.solve(rhs,guard);require(matrix.multiply(solution,guard)==rhs,"selected algebraic solve residual failed");
    const auto alien=numeric::CertifiedAlgebraicField::declare_cyclotomic(7);rejected=false;
    try{matrix.set(0,0,alien.generator());}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"algebraic coefficient owner was inferred from equal minimal polynomials");
    std::cout<<"owned cyclic-equivariant chains and generic acyclic torsion passed\n";return EXIT_SUCCESS;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return EXIT_FAILURE;}}
