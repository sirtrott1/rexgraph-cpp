#include "square_tiled_surface_fixtures.hpp"
#include "period_construction_fixtures.hpp"
#include "rfc/field/finite_jacobian.hpp"
#include <cstdlib>
#include <iostream>
#include <optional>
using namespace surface_fixtures;
using Surface=field::PreparedSquareTiledSurface;using Periods=field::PreparedSquareTiledPeriods;using Symplectic=field::PreparedIntegralSymplecticBasis;
static std::size_t checks{};
static void require(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
template<class Error=std::invalid_argument,class F>static void rejects(F work,const char* why){bool bad=false;try{work();}catch(const Error&){bad=true;}require(bad,why);}
template<class N>concept Subdivision=requires(N n,Guard& g){Surface::prepare({0},{0},n,field::SquareTiledOrientation::Positive,1,1,g);};
static_assert(Subdivision<int> && Subdivision<std::uint32_t>);static_assert(!Subdivision<float> && !Subdivision<double> && !Subdivision<long double>);
static M rational(const Z& a,Guard& g){return field::period_detail::rational(a,g);}
static M product(const M& a,const M& b,Guard& g){return field::period_detail::product(a,b,g);}
template<class Matrix>static void matrix_json(const Matrix& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<a.cols();++j){if(j)std::cout<<',';std::cout<<'"'<<a.get(i,j)<<'"';}std::cout<<']';}std::cout<<']';}
template<class Vector>static void vector_json(const Vector& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}std::cout<<']';}
static void emit(const Case& fixture,const Periods& p,Guard& g){
    const auto& s=p.surface();const auto& l=p.construction().lattice();const auto& sym=p.symplectic();
    std::cout<<"{\"name\":\""<<fixture.name<<"\",\"right\":";vector_json(s.right());std::cout<<",\"up\":";vector_json(s.up());
    std::cout<<",\"n\":"<<s.subdivision()<<",\"orientation\":"<<(s.orientation()==field::SquareTiledOrientation::Positive?1:-1)<<",\"genus\":"<<s.work().genus;
    std::cout<<",\"corner_vertices\":[";for(std::size_t i=0;i<s.corner_vertices().size();++i){if(i)std::cout<<',';vector_json(s.corner_vertices()[i]);}
    std::cout<<"],\"corner_links\":[";for(std::size_t i=0;i<s.corner_links().size();++i){if(i)std::cout<<',';vector_json(s.corner_links()[i]);}std::cout<<']';
    std::cout<<",\"b1\":";matrix_json(topology::integral_boundary_matrix(s.source(),core::Grade{1}));std::cout<<",\"b2\":";matrix_json(topology::integral_boundary_matrix(s.source(),core::Grade{2}));
    std::cout<<",\"cycles\":";matrix_json(l.integral_cycles());std::cout<<",\"cochains\":";matrix_json(l.dual_cochains());std::cout<<",\"gram\":";matrix_json(l.gram());std::cout<<",\"cup\":";matrix_json(l.cup_form());
    std::cout<<",\"symplectic\":";matrix_json(sym.basis());std::cout<<",\"inverse\":";matrix_json(sym.inverse());std::cout<<",\"symplectic_cycles\":";matrix_json(sym.cycles());std::cout<<",\"symplectic_cochains\":";matrix_json(sym.cochains());
    std::cout<<",\"gluing\":";matrix_json(p.gluing_cycles());std::cout<<",\"gluing_coordinates\":";matrix_json(p.gluing_coordinates());std::cout<<",\"cylinders\":";matrix_json(p.cylinder_cycles());
    std::cout<<",\"cup_terms\":[";const auto& a=s.source().grade(core::Grade{2});
    for(std::size_t i=0;i<s.cup_terms().size();++i){if(i)std::cout<<',';const auto& t=s.cup_terms()[i];std::cout<<'['<<t.face<<','<<t.first_slot<<','<<t.second_slot<<",\""<<t.coefficient<<"\",\""<<s.fundamental_cycle().at(t.face)<<"\"]";}
    std::cout<<"],\"face_edges\":[";for(std::size_t i=0;i<s.face_edges().size();++i){if(i)std::cout<<',';vector_json(s.face_edges()[i]);}std::cout<<"],\"points\":[";
    (void)a;
    for(std::size_t v=0;v<s.work().vertices;++v){if(v)std::cout<<',';const auto point=p.construction().point(0,v,g);M path(s.work().edges,1);
        for(const auto& [i,z]:point.connecting_chain().values())path.set(i,0,Q{z});
        std::cout<<"{\"vertex\":"<<v<<",\"path\":";matrix_json(path);std::cout<<",\"coordinates\":[";
        for(std::size_t j=0;j<point.coordinates().size();++j){if(j)std::cout<<',';std::cout<<'"'<<point.coordinates()[j]<<'"';}std::cout<<"]}";}
    std::cout<<"]}";
}
static void checks_for(const Case& fixture,const Periods& p,Guard& g){
    const auto& s=p.surface();const auto& c=p.construction();const auto& l=c.lattice();const auto& sym=p.symplectic();const auto h=l.period_rank();
    require(s.work().genus==fixture.genus && h==2*fixture.genus,"surface/period genus differs");
    require(s.right()==fixture.right && s.up()==fixture.up,"primary gluing permutations changed");
    require(sym.lattice().same_owner(l),"symplectic proof uses a foreign period owner");
    require(p.gluing_smith().rank==h,"gluing loops omit homology directions");
    for(const auto& z:p.gluing_smith().invariant_factors)require(z==1,"gluing loops have a proper integral index");
    Z j(h,h);for(std::size_t i=0;i<h/2;++i){j.set(i,h/2+i,I{1});j.set(h/2+i,i,I{-1});}
    const auto t=rational(sym.basis(),g);
    require(product(t.transpose(),product(l.cup_form(),t,g),g)==rational(j,g),"whole integral frame is not symplectic");
    require(sym.basis().multiply(sym.inverse(),g)==Z::identity(h),"basis inverse residual");
    require(product(rational(sym.cycles(),g).transpose(),sym.cochains(),g)==M::identity(h),"transported cycle/cochain duality failed");
    require(sym.stages().size()==fixture.genus,"missing Smith partner stages");
    for(std::size_t k=0;k<p.gluing_cycles().cols();++k){const auto chain=l.chain(column(p.gluing_cycles(),k),g);const auto v=c.homology_coordinates(chain,g);
        for(std::size_t i=0;i<h;++i)require(v[i]==p.gluing_coordinates().get(i,k),"gluing quotient coordinates changed");}
    const auto cylinders=product(l.dual_cochains().transpose(),rational(p.cylinder_cycles(),g),g);
    if(fixture.name.starts_with("four-square-row"))require(cylinders.rank()==3 && h==4,"deficient cylinder-core case changed");
    for(std::size_t v=0;v<s.work().vertices;++v){const auto point=c.point(0,v,g);auto values=point.connecting_chain().values();
        for(const auto& [i,z]:column(sym.cycles(),0))values[i]+=z;
        const auto shifted=l.point(0,static_cast<std::uint32_t>(v),l.chain(std::move(values),g),g);const auto proof=c.compare_paths(point,shifted,g);
        require(proof.first().coordinates()==proof.second().coordinates(),"symplectic cycle shift moved an actual period point");}
}
int main(int argc,char** argv){try{
    const bool dump=argc==2&&std::string(argv[1])=="--dump";const bool hard_dump=argc==2&&std::string(argv[1])=="--hard";Guard g;const auto fixtures=cases();
    if(dump)std::cout<<'[';
    for(std::size_t i=0;i<fixtures.size();++i){const auto& fixture=fixtures[i];Guard surface_guard;const auto s=surface(fixture,surface_guard);
        require(s.work().preparation_steps==surface_guard.steps(),"surface work clock mismatch");Guard period_guard;const auto p=s.integral_periods(period_guard);
        require(p.work().preparation_steps==period_guard.steps(),"full period work clock mismatch");require(p.surface().same_owner(s),"original surface owner lost");checks_for(fixture,p,g);
        if(dump){if(i)std::cout<<',';emit(fixture,p,g);}
    }
    if(dump)std::cout<<"]\n";
    const auto torus=surface(fixtures[1],g),other=surface(fixtures[1],g);require(!torus.same_owner(other),"equal declarations imply owner identity");
    const auto p=torus.integral_periods(g),q=other.integral_periods(g);
    rejects([&]{(void)p.construction().compare_paths(p.construction().point(0,0,g),q.construction().point(0,0,g),g);},"equal numeric surface keys import foreign period authority");
    const auto weighted=torus.integral_periods(field::ExactMetric::diagonal(numeric::RationalVector(torus.work().edges,Q{2})),g);
    require(weighted.construction().lattice().gram().scale(Q{2})==p.construction().lattice().gram(),"explicit metric did not reach period construction");
    rejects([&]{(void)Surface::prepare({0},{0},1,field::SquareTiledOrientation::Positive,0,1,g);},"zero source key accepted");
    rejects([&]{(void)Surface::prepare({0},{0},1,field::SquareTiledOrientation::Positive,1,0,g);},"zero source revision accepted");
    const auto rational_field=numeric::CertifiedAlgebraicField::declare_cyclotomic(1);
    const auto embedding=geometry::SelectedRealAlgebraicEmbedding::isolate(rational_field,Q{0},Q{2});
    const auto jacobian=field::PreparedFiniteJacobian::normalize(p.construction().lattice(),p.symplectic().basis(),numeric::SparseAlgebraicMatrix::identity(2,rational_field,g),embedding,g);
    const auto point=p.construction().point(0,3,g);(void)jacobian.point(point,g);
    rejects([&]{(void)jacobian.point(q.construction().point(0,3,g),g);},"foreign surface point reached Jacobian");
    auto missing=torus.cup_terms();missing.pop_back();missing.pop_back();
    rejects([&]{(void)field::PreparedIntegralPeriodConstruction::prepare(torus.source(),field::ExactMetric::identity(torus.work().edges),torus.fundamental_cycle(),missing,
        field::PeriodCoordinateBasis::CoordinatedIntegralLattice,103,1,g);},"incomplete derived cup accepted");
    auto reversed=torus.fundamental_cycle();reversed[0]=-reversed[0];
    rejects([&]{(void)field::PreparedIntegralPeriodConstruction::prepare(torus.source(),field::ExactMetric::identity(torus.work().edges),reversed,torus.cup_terms(),
        field::PeriodCoordinateBasis::CoordinatedIntegralLattice,103,1,g);},"incoherent face orientation accepted");
    for(const auto& bad:std::vector<Case>{{"empty",{},{},1,0},{"sizes",{0},{0,1},1,0},{"duplicate",{0,0},{1,0},1,0},{"range",{2,0},{1,0},1,0},{"disconnected",{0,1},{0,1},1,0},{"zero-n",{0},{0},0,0},{"orientation",{0},{0},1,0,static_cast<field::SquareTiledOrientation>(99)}})
        rejects([&]{(void)surface(bad,g);},"invalid gluing/subdivision/orientation admitted");
    rejects<std::out_of_range>([&]{(void)Surface::prepare({0},{0},std::uint64_t{1}<<32,field::SquareTiledOrientation::Positive,1,1,g);},"subdivision silently narrowed");
    rejects<std::out_of_range>([&]{(void)Surface::prepare({0},{0},-1,field::SquareTiledOrientation::Positive,1,1,g);},"negative subdivision silently narrowed");
    rejects([&]{(void)torus.integral_periods(field::ExactMetric::identity(1),g);},"wrong metric carrier admitted");
    // No unit entries in this valid integral alternating form. A bounded
    // search for a +/-1 entry would incorrectly refuse this unimodular frame.
    auto d=period_fixtures::genus_two(false);Z transform(4,4);
    const long raw[4][4]={{1,0,0,2},{0,1,1,0},{0,2,3,5},{0,0,2,9}};
    for(std::size_t i=0;i<4;++i)for(std::size_t j=0;j<4;++j)transform.set(i,j,I{raw[i][j]});
    const auto cycles=topology::unimodular_inverse(transform).transpose();
    const auto hard=field::PreparedIntegralPeriodLattice::prepare(d.source,cycles,rational(transform,g),d.metric,d.fundamental,d.cup,
        field::PeriodCoordinateBasis::CoordinatedIntegralLattice,104,1,g);
    for(const auto& [ij,value]:hard.cup_form().entries()){(void)ij;require(value!=1&&value!=-1,"hard symplectic form has a unit shortcut");}
    const auto hard_basis=Symplectic::prepare(hard,g);require(hard_basis.basis_smith().rank==4,"Smith partner construction failed without unit entries");
    if(hard_dump){std::cout<<"{\"form\":";matrix_json(hard.cup_form());std::cout<<",\"basis\":";matrix_json(hard_basis.basis());std::cout<<",\"inverse\":";matrix_json(hard_basis.inverse());std::cout<<",\"original_cochains\":";matrix_json(hard.dual_cochains());std::cout<<",\"cochains\":";matrix_json(hard_basis.cochains());std::cout<<",\"cycles\":";matrix_json(hard_basis.cycles());std::cout<<"}\n";}
    const auto zero=period_fixtures::construct(period_fixtures::killed_loops(false),g);const auto zero_basis=Symplectic::prepare(zero.lattice(),g);require(zero_basis.basis().rows()==0,"zero symplectic carrier refused");
    const auto retained=[](){Guard local;const auto s=surface(cases()[3],local);return s.integral_periods(local);}();
    require(retained.surface().work().genus==2&&retained.symplectic().lattice().same_owner(retained.construction().lattice()),"surface/period/symplectic owner lifetime lost");
    Guard measured;const auto measured_surface=surface(fixtures[1],measured);const auto surface_steps=measured.steps();Guard period_measure;const auto measured_period=measured_surface.integral_periods(period_measure);const auto period_steps=period_measure.steps();
    Guard sym_measure;const auto measured_sym=Symplectic::prepare(measured_period.construction().lattice(),sym_measure);const auto sym_steps=sym_measure.steps();(void)measured_sym;
    for(int phase=0;phase<3;++phase){const auto steps=phase==0?surface_steps:(phase==1?period_steps:sym_steps);
        for(const auto stop:{std::size_t{1},steps/2,steps}){std::size_t calls=0;Guard cancelled({},[&]{if(++calls==stop)throw std::runtime_error("cancel");});
            std::optional<Surface> source;std::optional<Periods> periods;std::optional<Symplectic> basis;
            rejects<std::runtime_error>([&]{if(phase==0)source=surface(fixtures[1],cancelled);if(phase==1)periods=measured_surface.integral_periods(cancelled);if(phase==2)basis=Symplectic::prepare(measured_period.construction().lattice(),cancelled);},"cancellation did not propagate");
            require(!source&&!periods&&!basis,"cancelled construction published a partial certificate");}}
    for(int kind=0;kind<4;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.dimension=2;if(kind==1)limits.entries=100;if(kind==2)limits.steps=surface_steps-1;if(kind==3)limits.integer_bits=0;
        Guard small(limits);rejects<std::length_error>([&]{(void)surface(fixtures[1],small);},"surface receiving budget bypassed");}
    for(int kind=0;kind<4;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.dimension=1;if(kind==1)limits.entries=100;if(kind==2)limits.steps=sym_steps-1;if(kind==3)limits.integer_bits=0;
        Guard small(limits);rejects<std::length_error>([&]{(void)Symplectic::prepare(measured_period.construction().lattice(),small);},"symplectic receiving budget bypassed");}
    if(!dump&&!hard_dump)std::cout<<checks<<" surface, orientation, cup, complete gluing and integral symplectic checks passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
