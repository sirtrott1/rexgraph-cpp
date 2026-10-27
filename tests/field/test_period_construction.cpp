#include "period_construction_fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <optional>
using namespace period_fixtures;
using Guard=numeric::ExactActionGuard;
using Construction=field::PreparedIntegralPeriodConstruction;
static std::size_t checks{};
static void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Error=std::invalid_argument,class F> static void rejects(F work,const char* message){bool caught=false;try{work();}catch(const Error&){caught=true;}require(caught,message);}
template<class B,class V> concept PointInput=requires(const Construction& c,Guard& g,B b,V v){c.point(b,v,g);c.connecting_chain(b,v,g);};
static_assert(PointInput<std::uint32_t,std::uint32_t>);static_assert(!PointInput<double,int>);static_assert(!PointInput<int,float>);
static Z::SparseVector column(const Z& a,std::size_t j){Z::SparseVector v;for(const auto& [ij,z]:a.entries())if(ij.second==j)v.emplace(ij.first,z);return v;}
template<class Matrix> static void json_matrix(const Matrix& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<a.cols();++j){if(j)std::cout<<',';std::cout<<'"'<<a.get(i,j)<<'"';}std::cout<<']';}std::cout<<']';}
static void emit(const char* name,const PeriodDeclaration& d,const Construction& c,Guard& g){
    const auto& l=c.lattice();std::cout<<"{\"name\":\""<<name<<"\",\"b1\":";json_matrix(topology::integral_boundary_matrix(d.source,core::Grade{1}));
    std::cout<<",\"b2\":";json_matrix(topology::integral_boundary_matrix(d.source,core::Grade{2}));std::cout<<",\"cycles\":";json_matrix(l.integral_cycles());
    std::cout<<",\"cochains\":";json_matrix(l.dual_cochains());std::cout<<",\"metric\":";json_matrix(d.metric.materialize());std::cout<<",\"gram\":";json_matrix(l.gram());
    std::cout<<",\"cup\":";json_matrix(l.cup_form());std::cout<<",\"cup_terms\":[";
    const auto& faces=d.source.grade(core::Grade{2});
    for(std::size_t i=0;i<d.cup.size();++i){if(i)std::cout<<',';const auto& t=d.cup[i];
        std::cout<<'['<<t.face<<','<<faces.lower_index[t.first_slot]<<','<<faces.lower_index[t.second_slot]<<",\""<<t.coefficient<<"\",\""
            <<faces.lattice_coefficient[t.first_slot]<<"\",\""<<faces.lattice_coefficient[t.second_slot]<<"\",\""<<d.fundamental.at(t.face)<<"\"]";}
    std::cout<<"],\"points\":[";
    const auto vertices=d.source.grade(core::Grade{0}).cell_count();
    for(std::size_t v=0;v<vertices;++v){if(v)std::cout<<',';const auto point=c.point(0,static_cast<std::uint32_t>(v),g);
        M path(l.integral_cycles().rows(),1);for(const auto& [i,z]:point.connecting_chain().values())path.set(i,0,Q{z});
        std::cout<<"{\"vertex\":"<<v<<",\"path\":";json_matrix(path);std::cout<<",\"coordinates\":[";
        for(std::size_t j=0;j<point.coordinates().size();++j){if(j)std::cout<<',';std::cout<<'"'<<point.coordinates()[j]<<'"';}std::cout<<"]}";
    }
    std::cout<<"]}";
}
int main(int argc,char** argv){try{
    const bool dump=argc==2&&std::string(argv[1])=="--dump";Guard g;
    std::vector<std::pair<const char*,PeriodDeclaration>> cases;
    cases.emplace_back("torus-unit",square_torus());cases.emplace_back("torus-anisotropic",square_torus(Q{1,3}));
    cases.emplace_back("torus-stretched",square_torus(Q{7,5}));cases.emplace_back("genus-two-unit",genus_two(false));
    cases.emplace_back("genus-two-coupled",genus_two(true));cases.emplace_back("four-square-rank-four",four_square_row());
    cases.emplace_back("zero-H1",killed_loops(false));
    cases.emplace_back("torus-coordinated-native-scales",square_torus(Q{1},false,Q{1,3}));
    cases.emplace_back("zero-H1-empty-higher-grades",killed_loops(false,false,true));
    auto sparse_torus=square_torus();M sparse_form(18,18);
    for(std::size_t i=0;i<18;++i){sparse_form.set(i,i,Q{2});if(i){sparse_form.set(i-1,i,Q{-1,3});sparse_form.set(i,i-1,Q{-1,3});}}
    sparse_torus.metric=field::ExactMetric(std::move(sparse_form));cases.emplace_back("torus-sparse-coupled-projection",std::move(sparse_torus));
    if(dump)std::cout<<'[';
    for(std::size_t n=0;n<cases.size();++n){auto& [name,d]=cases[n];Guard build;const auto c=construct(d,build);const auto& l=c.lattice();
        require(c.work().preparation_steps==build.steps(),"step receipt differs");
        require(l.period_rank()==(std::string(name).starts_with("torus")?2:(std::string(name).starts_with("zero")?0:4)),"complete integral period rank differs");
        require(l.source().grade(core::Grade{1}).relation_identities==d.source.grade(core::Grade{1}).relation_identities,"source identities lost");
        for(std::size_t grade=0;grade<d.source.grade_count();++grade){
            const auto& before=d.source.grade(core::Grade{static_cast<std::uint16_t>(grade)});
            const auto& after=l.source().grade(core::Grade{static_cast<std::uint16_t>(grade)});
            require(before.cell_scales==after.cell_scales && before.lattice_coefficient==after.lattice_coefficient &&
                before.occurrence_identities==after.occurrence_identities && before.lower_index==after.lower_index && before.upper_index==after.upper_index,
                "retained lattice/source occurrence data changed");
        }
        if(std::string(name)=="torus-coordinated-native-scales"){
            require(l.source().grade(core::Grade{1}).cell_scales.front()==Q{3},"native edge scale collapsed into lattice");
            require(l.source().grade(core::Grade{2}).cell_scales.front()==Q{9},"adjacent native scale lost");
            require(l.source().incidence_coefficient(core::Grade{1},0)==Q{-1,3},"native boundary confused with lattice boundary");
        }
        const auto z=l.integral_cycles();
        for(std::size_t j=0;j<z.cols();++j){const auto coords=c.homology_coordinates(l.chain(column(z,j),g),g);
            for(std::size_t i=0;i<coords.size();++i)require(coords[i]==(i==j?1:0),"Smith quotient/period coordinates disagree");}
        if(std::string(name)=="four-square-rank-four"){
            Z cores(8,4);for(std::size_t i=0;i<4;++i)cores.set(i,0,I{1});
            cores.set(4,1,I{1});cores.set(5,1,I{1});cores.set(6,2,I{1});cores.set(7,3,I{1});
            M coordinates(4,4);
            for(std::size_t j=0;j<4;++j){const auto x=c.homology_coordinates(l.chain(column(cores,j),g),g);
                for(std::size_t i=0;i<4;++i)coordinates.set(i,j,Q{x[i]});}
            require(coordinates.rank()==3,"four-square cylinder span regression changed");
            require(l.period_rank()==4,"incomplete cylinder list used as full integral basis");
        }
        const auto b2=topology::integral_boundary_matrix(d.source,core::Grade{2});
        for(std::size_t j=0;j<b2.cols();++j){const auto coords=c.homology_coordinates(l.chain(column(b2,j),g),g);for(const auto& q:coords)require(q==0,"face boundary has nonzero homology coordinate");}
        const auto vertices=d.source.grade(core::Grade{0}).cell_count();
        for(std::size_t v=0;v<vertices;++v){const auto p=c.point(0,static_cast<std::uint32_t>(v),g);auto shifted=p.connecting_chain().values();
            for(std::size_t j=0;j<z.cols();++j){for(const auto& [i,value]:column(z,j))shifted[i]+=value;
                const auto q=l.point(0,static_cast<std::uint32_t>(v),l.chain(shifted,g),g);const auto proof=c.compare_paths(p,q,g);
                for(std::size_t i=0;i<proof.coordinates().size();++i)require(proof.coordinates()[i]==(i<=j?1:0),"path change has wrong full-lattice integer shift");
                require(proof.first().coordinates()==proof.second().coordinates(),"closed path change moved point");}
            for(const auto& [i,value]:column(b2,0))shifted[i]+=value;
            const auto q=l.point(0,static_cast<std::uint32_t>(v),l.chain(shifted,g),g);(void)c.compare_paths(p,q,g);
            if(v)rejects([&]{(void)c.homology_coordinates(p.connecting_chain(),g);},"open path has homology coordinates");
        }
        if(dump){if(n)std::cout<<',';emit(name,d,c,g);}
    }
    if(dump)std::cout<<"]\n";
    // Primitive cohomology is measured on the complete integral H1 basis.
    // An integer coboundary can make edge gcd=1 while period gcd remains 2.
    const auto d=square_torus();const auto c=construct(d,g);const auto foreign=construct(d,g);
    const auto b1=topology::integral_boundary_matrix(d.source,core::Grade{1});
    M cut(18,1),potential(9,1);potential.set(0,0,Q{1});for(std::size_t y=0;y<3;++y)cut.set(3*y,0,Q{1});
    const auto delta=field::period_detail::product(field::period_detail::rational(b1.transpose(),g),potential,g);
    for(const auto multiplier:{0,1,2}){
        M cochain=delta;for(const auto& [ij,q]:cut.entries())cochain.set(ij.first,ij.second,g.add(cochain.get(ij.first,ij.second),g.mul(Q{multiplier},q)));
        const auto values=field::period_detail::product(field::period_detail::rational(c.lattice().integral_cycles().transpose(),g),cochain,g);
        I period_gcd{0},edge_gcd{0};for(const auto& [ij,q]:values.entries()){(void)ij;require(q.get_den()==1,"class period lost integrality");mpz_gcd(period_gcd.get_mpz_t(),period_gcd.get_mpz_t(),q.get_num().get_mpz_t());}
        for(const auto& [ij,q]:cochain.entries()){(void)ij;mpz_gcd(edge_gcd.get_mpz_t(),edge_gcd.get_mpz_t(),q.get_num().get_mpz_t());}
        require(period_gcd==multiplier,"coboundary changed primitive class period gcd");require(edge_gcd==1,"edge gcd counterexample lost");
    }
    rejects([&]{(void)c.homology_coordinates(foreign.lattice().chain({},g),g);},"foreign chain owner accepted");
    rejects([&]{(void)c.compare_paths(c.point(0,0,g),foreign.point(0,0,g),g);},"foreign point owner accepted");
    rejects([&]{(void)c.compare_paths(c.point(0,0,g),c.point(0,1,g),g);},"different endpoints accepted");
    rejects<std::out_of_range>([&]{(void)c.point(0,999,g);},"unknown vertex accepted");
    rejects<std::out_of_range>([&]{(void)c.point(0,std::uint64_t{1}<<32,g);},"large index silently narrowed");
    rejects<std::out_of_range>([&]{(void)c.point(-1,0,g);},"negative vertex narrowed");
    auto bad=d;bad.cycles.col_scale(0,I{2});for(std::size_t e=0;e<9;++e)bad.cochains.set(e,0,Q{1,6});
    rejects([&]{(void)field::PreparedIntegralPeriodLattice::prepare(bad.source,bad.cycles,bad.cochains,bad.metric,bad.fundamental,bad.cup,
        field::PeriodCoordinateBasis::CoordinatedIntegralLattice,93,1,g);},"nonprimitive supplied lattice accepted");
    rejects([&]{(void)construct(killed_loops(true),g);},"integral H1 torsion accepted");
    rejects([&]{(void)construct(killed_loops(false,true),g);},"inconsistent higher chain accepted");
    rejects([&]{(void)construct(square_torus(Q{1},true),g);},"higher cup obstruction omitted");
    bad=d;bad.cup.front().second_slot=4;rejects([&]{(void)construct(bad,g);},"foreign face occurrence accepted");
    bad=d;bad.cup.pop_back();bad.cup.pop_back();rejects([&]{(void)construct(bad,g);},"missing fundamental face accepted");
    bad=d;bad.fundamental[0]=2;rejects([&]{(void)construct(bad,g);},"nonclosed fundamental declaration accepted");
    bad=d;bad.metric=field::ExactMetric::identity(2);rejects([&]{(void)construct(bad,g);},"wrong metric carrier accepted");
    bad=d;bad.metric=field::ExactMetric::block_diagonal({field::ExactMetric::identity(18)});rejects([&]{(void)construct(bad,g);},"unsupported metric silently admitted");
    bad=d;bad.metric=field::ExactMetric::certified_external(18,[](const numeric::RationalVector& x){return x;},[](const numeric::RationalVector& x){return x;});
    rejects([&]{(void)construct(bad,g);},"caller-certified metric silently promoted to owned qualification");
    for(int mode=0;mode<3;++mode){const auto extra=construct(decorated(mode),g);require(extra.lattice().period_rank()==4,"extra acyclic component changed H1");
        if(mode==0){(void)extra.point(1,2,g);rejects([&]{(void)extra.point(0,1,g);},"disconnected vertices joined");}
        else rejects([&]{(void)extra.point(0,0,g);},"decorated/nonunit edge inferred as an ordinary pair");}
    // Every successful result owns all data after preparing wrappers disappear.
    const auto retained=[](){Guard local;const auto x=construct(square_torus(),local);return x.compare_paths(x.point(0,8,local),x.point(0,8,local),local);}();
    require(retained.difference().owner().period_rank()==2,"path evidence lost its actual owner");
    Guard measured;const auto prepared=construct(square_torus(),measured);
    const auto steps=measured.steps();
    for(const auto stop:{std::size_t{1},steps/2,steps}){
        std::size_t calls=0;Guard cancelled({},[&]{if(++calls==stop)throw std::runtime_error("cancel");});std::optional<Construction> published;
        rejects<std::runtime_error>([&]{published=construct(square_torus(),cancelled);},"preparation cancellation did not propagate");require(!published,"partial result published");}
    for(int kind=0;kind<4;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.dimension=2;if(kind==1)limits.entries=100;if(kind==2)limits.steps=steps-1;if(kind==3)limits.integer_bits=1;
        Guard small(limits);rejects<std::length_error>([&]{(void)construct(square_torus(),small);},"receiving budget bypassed");}
    for(const auto operation:{0,1,2}){std::size_t calls=0;Guard stop({},[&]{if(++calls==3)throw std::runtime_error("cancel");});
        rejects<std::runtime_error>([&]{if(operation==0)(void)prepared.point(0,8,stop);if(operation==1)(void)prepared.homology_coordinates(prepared.lattice().chain({},g),stop);
            if(operation==2)(void)prepared.compare_paths(prepared.point(0,8,g),prepared.point(0,8,g),stop);},"action cancellation bypassed");}
    if(!dump)std::cout<<checks<<" integral basis, metric dual, owner, path, refusal and budget checks passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
