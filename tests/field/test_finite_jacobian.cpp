#include "rfc/core/builder.hpp"
#include "rfc/field/finite_jacobian.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace rfc;
using Q=numeric::Rational;
using I=numeric::Integer;
using Z=numeric::IntegerMatrix;
using M=numeric::ExactMatrix;
using A=numeric::SparseAlgebraicMatrix;
using Guard=numeric::ExactActionGuard;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Work> static void rejects(Work work,const char* message){
    bool rejected=false;
    try{work();}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,message);
}

struct PeriodDeclaration {
    core::RelationalComplex source;
    Z cycles;
    M cochains;
    field::ExactMetric metric;
    Z::SparseVector fundamental;
    std::vector<field::PeriodCupTerm> cup;
};
static std::uint32_t vertex(std::uint32_t x,std::uint32_t y){return 3*y+x;}
static std::uint32_t horizontal(std::uint32_t x,std::uint32_t y){return 3*y+x;}
static std::uint32_t vertical(std::uint32_t x,std::uint32_t y){return 9+3*y+x;}

static PeriodDeclaration square_torus(Q horizontal_metric=Q{1},bool fill_fundamental_cycle=false){
    core::RelationSignature signature;
    const auto edge=signature.add_type("torus-edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto square=signature.add_attaching_loop_type("torus-square",4,4);
    const auto filling=signature.add_type("retained-three-cell",core::BoundaryPolicy::DeclaredExact,9,9);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x)
        builder.add_grade_one(edge,{{vertex(x,y),true,true,false,1},{vertex((x+1)%3,y),true,false,false,1}});
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x)
        builder.add_grade_one(edge,{{vertex(x,y),true,true,false,1},{vertex(x,(y+1)%3),true,false,false,1}});
    std::vector<field::PeriodCupTerm> cup;
    Z::SparseVector fundamental;
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x){
        builder.add_higher(core::Grade{2},square,{{horizontal(x,y),true,false,false,1},
            {vertical((x+1)%3,y),true,false,false,1},{horizontal(x,(y+1)%3),true,true,false,1},
            {vertical(x,y),true,true,false,1}});
        const auto face=vertex(x,y);const std::size_t start=4*face;
        fundamental.emplace(face,I{1});
        cup.push_back({face,start,start+1,Q{1}});
        cup.push_back({face,start+3,start+2,Q{-1}});
    }
    if(fill_fundamental_cycle){
        std::vector<core::HigherIncidenceInput> faces;
        for(std::uint32_t face=0;face<9;++face)faces.push_back({face,true,false,false,1});
        builder.add_higher(core::Grade{3},filling,faces);
    }
    Z cycles(18,2);M cochains(18,2);numeric::RationalVector diagonal(18,Q{1});
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x){
        cochains.set(horizontal(x,y),0,Q{1,3});cochains.set(vertical(x,y),1,Q{1,3});
        diagonal[horizontal(x,y)]=horizontal_metric;
    }
    for(std::uint32_t k=0;k<3;++k){cycles.set(horizontal(k,0),0,I{1});cycles.set(vertical(0,k),1,I{1});}
    return {builder.freeze(),std::move(cycles),std::move(cochains),field::ExactMetric::diagonal(std::move(diagonal)),
        std::move(fundamental),std::move(cup)};
}

static field::PreparedIntegralPeriodLattice prepare(PeriodDeclaration declaration,Guard& guard){
    return field::PreparedIntegralPeriodLattice::prepare(std::move(declaration.source),std::move(declaration.cycles),
        std::move(declaration.cochains),std::move(declaration.metric),std::move(declaration.fundamental),
        std::move(declaration.cup),field::PeriodCoordinateBasis::CoordinatedIntegralLattice,91,1,guard);
}

static Z::SparseVector connection(std::uint32_t x,std::uint32_t y){
    Z::SparseVector chain;
    for(std::uint32_t k=0;k<x;++k)chain.emplace(horizontal(k,0),I{1});
    for(std::uint32_t k=0;k<y;++k)chain.emplace(vertical(x,k),I{1});
    return chain;
}

static PeriodDeclaration genus_two(bool coupled){
    core::RelationSignature signature;
    const auto edge=signature.add_type("genus-two-loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto polygon=signature.add_attaching_loop_type("genus-two-face",8,8);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint32_t k=0;k<4;++k)builder.add_grade_one(edge,{{0,true,true,false,1},{0,true,false,false,1}});
    builder.add_higher(core::Grade{2},polygon,{{0,true,false,false,1},{2,true,false,false,1},
        {0,true,true,false,1},{2,true,true,false,1},{1,true,false,false,1},{3,true,false,false,1},
        {1,true,true,false,1},{3,true,true,false,1}});
    M metric=M::identity(4);
    if(coupled)for(std::size_t block=0;block<2;++block){
        const auto start=2*block;
        metric.set(start,start,Q{2,3});metric.set(start+1,start+1,Q{2,3});
        metric.set(start,start+1,Q{-1,3});metric.set(start+1,start,Q{-1,3});
    }
    return {builder.freeze(),Z::identity(4),M::identity(4),field::ExactMetric(std::move(metric)),{{0,I{1}}},
        {{0,0,1,Q{1}},{0,3,2,Q{-1}},{0,4,5,Q{1}},{0,7,6,Q{-1}}}};
}
static M declared_plane(long sign,Q multiple=Q{1}){
    M out(4,2);out.set(0,0,multiple);out.set(1,0,Q{sign}*multiple);
    out.set(2,1,multiple);out.set(3,1,Q{sign}*multiple);return out;
}
static M coordinate_plane(std::size_t handle){
    M out(4,2);out.set(handle,0,Q{1});out.set(2+handle,1,Q{1});return out;
}

int main(){try{
    Guard guard;
    const auto rational_field=numeric::CertifiedAlgebraicField::declare_cyclotomic(1);
    const auto rational_embedding=geometry::SelectedRealAlgebraicEmbedding::isolate(rational_field,Q{0},Q{2});
    const auto one=rational_field.scalar(Q{1});
    const auto lattice=prepare(square_torus(),guard);
    require(lattice.period_rank()==2&&lattice.gram()==M::identity(2),"complete square-torus harmonic period pairing failed");
    const auto torus=field::PreparedFiniteJacobian::torus(lattice,one,rational_embedding,guard);
    require(torus.real_periods().nonzeros()==0&&torus.imaginary_periods().get(0,0)==one,
        "square torus did not produce the exact modulus i");
    std::vector<field::IntegralPeriodPoint> points;
    for(std::uint32_t y=0;y<3;++y)for(std::uint32_t x=0;x<3;++x){
        auto point=lattice.point(0,vertex(x,y),lattice.chain(connection(x,y),guard),guard);
        const auto image=torus.point(point,guard);
        const Q expected_x=Q{x}/Q{3},expected_y=Q{y}/Q{3};
        require(point.coordinates()==numeric::RationalVector({expected_x,expected_y}),"source-chain Abel-Jacobi periods differ");
        require(image.coordinates()[0].real==rational_field.scalar(expected_x)&&image.coordinates()[0].imaginary==rational_field.scalar(expected_y),
            "exact complex vertex coordinates differ from the complete source-chain pairing");
        points.push_back(std::move(point));
    }
    require(torus.supplied_vertices_are_distinct(points,guard),"finite square-torus vertex injectivity certificate failed");
    auto shifted=connection(2,2);
    for(std::uint32_t k=0;k<3;++k)shifted[horizontal(k,0)]+=1;
    const auto loop_shift=lattice.point(0,vertex(2,2),lattice.chain(shifted,guard),guard);
    require(torus.point(points.back(),guard).same_point(torus.point(loop_shift,guard)),"integral closed-path change altered the torus point");
    const auto d2=topology::integral_boundary_matrix(lattice.source(),core::Grade{2});
    for(const auto& [ij,value]:d2.entries())if(ij.second==0)shifted[ij.first]+=value;
    const auto face_shift=lattice.point(0,vertex(2,2),lattice.chain(shifted,guard),guard);
    require(torus.point(points.back(),guard).same_point(torus.point(face_shift,guard)),"retained face boundary altered the Abel-Jacobi point");
    points.push_back(points.front());
    require(!torus.supplied_vertices_are_distinct(points,guard),"finite vertex test accepted repeated source points");
    rejects([&]{(void)lattice.point(0,vertex(2,2),lattice.chain({},guard),guard);},"vertex chain omitted its actual source-boundary certificate");
    const auto foreign=prepare(square_torus(),guard);
    rejects([&]{(void)lattice.periods(foreign.chain({},guard),guard);},"equal-sized foreign period source accepted");
    const auto foreign_point=foreign.point(0,0,foreign.chain({},guard),guard);
    rejects([&]{(void)torus.point(foreign_point,guard);},"foreign period owner reached finite Jacobian coordinates");
    auto nonprimitive=square_torus();nonprimitive.cycles.col_scale(0,I{2});
    for(std::uint32_t k=0;k<9;++k)nonprimitive.cochains.set(k,0,Q{1,6});
    rejects([&]{(void)prepare(std::move(nonprimitive),guard);},"rationally complete cycles bypassed the primitive integral-lattice requirement");
    auto missed_face=square_torus();missed_face.cup.pop_back();missed_face.cup.pop_back();
    rejects([&]{(void)prepare(std::move(missed_face),guard);},"cup declaration omitted a retained fundamental face");
    auto foreign_flag=square_torus();foreign_flag.cup.front().second_slot=4;
    rejects([&]{(void)prepare(std::move(foreign_flag),guard);},"cup term borrowed an occurrence from another face");
    auto not_closed=square_torus();not_closed.cochains.set(horizontal(0,1),0,Q{2,3});
    rejects([&]{(void)prepare(std::move(not_closed),guard);},"nonclosed source cochain received a period certificate");
    rejects([&]{(void)prepare(square_torus(Q{1},true),guard);},"higher retained boundary was silently omitted from the cellular cup certificate");

    const auto radical=numeric::CertifiedAlgebraicField::declare(numeric::RationalVector{Q{-3},Q{0},Q{1}},numeric::EisensteinShift{3,I{0}});
    const auto positive=geometry::SelectedRealAlgebraicEmbedding::isolate(radical,Q{1},Q{2});
    const auto negative=geometry::SelectedRealAlgebraicEmbedding::isolate(radical,Q{-2},Q{-1});
    const auto root=radical.generator();
    const auto stretched=prepare(square_torus(Q{1,3}),guard);
    const auto algebraic=field::PreparedFiniteJacobian::torus(stretched,root,positive,guard);
    require(algebraic.real_periods().nonzeros()==0&&algebraic.imaginary_periods().get(0,0)==root,
        "anisotropic exact metric did not produce modulus i sqrt(3)");
    const auto scalar_point=stretched.point(0,vertex(1,2),stretched.chain(connection(1,2),guard),guard);
    const auto algebraic_point=algebraic.point(scalar_point,guard);
    require(algebraic_point.coordinates()[0].imaginary==root*radical.scalar(Q{2,3}),
        "algebraic vertex coordinate lost its selected-root field");
    rejects([&]{(void)field::PreparedFiniteJacobian::torus(stretched,radical.scalar(Q{1}),positive,guard);},"incorrect square-root declaration accepted");
    rejects([&]{(void)field::PreparedFiniteJacobian::torus(stretched,root,negative,guard);},"opposite real root accepted as positive normalization");
    rejects([&]{(void)field::PreparedFiniteJacobian::torus(stretched,one,positive,guard);},"alien coefficient owner accepted by exact period normalization");
    const auto simple_factor=algebraic.declared_elliptic_factors({{Q{3},root,M::identity(2)}},guard);
    require(simple_factor.index()==1&&simple_factor.factors().size()==1&&simple_factor.factors()[0].modulus.imaginary==root,
        "rank-two exact saturated factor certificate failed");
    auto scaled=Z::identity(2);scaled.set(0,0,I{2});
    rejects([&]{(void)field::PreparedFiniteJacobian::normalize(lattice,scaled,A::identity(2,rational_field,guard),rational_embedding,guard);},
        "nonunimodular symplectic coordinates altered the lattice silently");
    rejects([&]{(void)field::PreparedFiniteJacobian::normalize(lattice,Z::identity(2),A::identity(2,rational_field,guard).scale(rational_field.scalar(Q{-1}),guard),rational_embedding,guard);},
        "negative polarization accepted a normalized complex-structure square alone");

    const auto higher=prepare(genus_two(true),guard);
    M normalizer(4,4);
    for(std::size_t block=0;block<2;++block){
        const auto start=2*block;
        normalizer.set(start,start,Q{2,3});normalizer.set(start+1,start+1,Q{2,3});
        normalizer.set(start,start+1,Q{-1,3});normalizer.set(start+1,start,Q{-1,3});
    }
    const auto higher_jacobian=field::PreparedFiniteJacobian::normalize(higher,Z::identity(4),A::from_rational(normalizer,rational_field,guard),rational_embedding,guard);
    require(higher_jacobian.real_periods().nonzeros()==0&&higher_jacobian.imaginary_periods()==A::identity(2,rational_field,guard),
        "generic genus-two normalization did not produce the declared finite period matrix");
    const auto factors=higher_jacobian.declared_elliptic_factors({{Q{1},one,declared_plane(-1)},
        {Q{9},rational_field.scalar(Q{3}),declared_plane(1)}},guard);
    require(factors.index()==4&&factors.lattice_inclusion().rows()==4&&factors.factors().size()==2,
        "saturated rational period planes lost the finite isogeny index four");
    for(const auto& factor:factors.factors())require(factor.intersection==2&&factor.modulus.real.is_zero()&&factor.modulus.imaginary==one,
        "elliptic factor intersection or exact modulus differs");
    const auto saturated=higher_jacobian.declared_elliptic_factors({{Q{1},one,declared_plane(-1,Q{2})},
        {Q{9},rational_field.scalar(Q{3}),declared_plane(1,Q{1,3})}},guard);
    require(saturated.index()==4&&saturated.factors()[0].intersection==2&&saturated.factors()[1].intersection==2,
        "explicit nonsaturated rational generators changed the saturated integral factor lattice");
    rejects([&]{(void)higher_jacobian.declared_elliptic_factors({{Q{1},one,declared_plane(-1)}},guard);},"partial factor declaration claimed the complete carrier");
    rejects([&]{(void)higher_jacobian.declared_elliptic_factors({{Q{1},one,declared_plane(-1)},
        {Q{1},one,declared_plane(-1)}},guard);},"dependent declared planes claimed the complete period carrier");
    rejects([&]{(void)higher_jacobian.declared_elliptic_factors({{Q{1},one,declared_plane(1)},
        {Q{9},rational_field.scalar(Q{3}),declared_plane(-1)}},guard);},"wrong supplied planes bypassed the declared source polynomial identity");
    const auto repeated=prepare(genus_two(false),guard);
    const auto repeated_jacobian=field::PreparedFiniteJacobian::normalize(repeated,Z::identity(4),A::identity(4,rational_field,guard),rational_embedding,guard);
    rejects([&]{(void)repeated_jacobian.declared_elliptic_factors({{Q{1},one,M(4,0)},{Q{1},one,M(4,0)}},guard);},
        "missing plane declarations triggered automatic eigenspace discovery");
    const auto declared_repeat=repeated_jacobian.declared_elliptic_factors({{Q{1},one,coordinate_plane(0)},
        {Q{1},one,coordinate_plane(1)}},guard);
    require(declared_repeat.index()==1,"explicit invariant planes in a repeated normalization sector were not certified");
    auto wrong_plane=coordinate_plane(0);wrong_plane.set(1,1,Q{1});
    rejects([&]{(void)repeated_jacobian.declared_elliptic_factors({{Q{1},one,wrong_plane},
        {Q{1},one,coordinate_plane(1)}},guard);},"a supplied noninvariant plane received an elliptic factor certificate");
    std::cout<<"owned finite periods, exact normalized Jacobians, and saturated isogeny factors passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return EXIT_FAILURE;}}
