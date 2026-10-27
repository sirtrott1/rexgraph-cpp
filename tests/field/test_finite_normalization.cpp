#include "finite_normalization_fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <optional>
using namespace normalization_fixtures;
static_assert(!std::is_constructible_v<Frequency,double,double>);
static std::size_t checks{};
static void require(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
template<class Error=std::invalid_argument,class Work>static void rejects(Work work,const char* why){bool bad=false;try{work();}catch(const Error&){bad=true;}require(bad,why);}
static void q_json(const Q& value){std::cout<<'"'<<value<<'"';}
static void e_json(const E& value){std::cout<<'[';for(std::size_t i=0;i<value.coordinates().size();++i){if(i)std::cout<<',';q_json(value.coordinates()[i]);}std::cout<<']';}
template<class Matrix>static void q_matrix(const Matrix& m){std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<m.cols();++j){if(j)std::cout<<',';q_json(Q{m.get(i,j)});}std::cout<<']';}std::cout<<']';}
static void e_matrix(const A& m){std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<m.cols();++j){if(j)std::cout<<',';e_json(m.get(i,j));}std::cout<<']';}std::cout<<']';}
static void emit(const Case& c,const Normalization& n,Guard& g){
    const auto& j=n.jacobian();const auto& l=j.lattice();const auto& f=j.embedding().field();
    std::cout<<"{\"name\":\""<<c.name<<"\",\"polynomial\":[";for(std::size_t i=0;i<f.minimal_polynomial().size();++i){if(i)std::cout<<',';q_json(f.minimal_polynomial()[i]);}
    std::cout<<"],\"embedding\":[";q_json(j.embedding().certificate().interval.lower);std::cout<<',';q_json(j.embedding().certificate().interval.upper);std::cout<<']';
    std::cout<<",\"b1\":";q_matrix(topology::integral_boundary_matrix(l.source(),core::Grade{1}));std::cout<<",\"cochains\":";q_matrix(l.dual_cochains());std::cout<<",\"gram\":";q_matrix(l.gram());std::cout<<",\"cup\":";q_matrix(l.cup_form());std::cout<<",\"basis\":";q_matrix(n.symplectic().basis());
    std::cout<<",\"star\":";q_matrix(j.star());std::cout<<",\"square\":";q_matrix(n.star_square());std::cout<<",\"sectors\":[";
    for(std::size_t i=0;i<n.sectors().size();++i){if(i)std::cout<<',';const auto& s=n.sectors()[i];std::cout<<"{\"mu\":";e_json(s.declaration.squared_frequency);std::cout<<",\"root\":";e_json(s.declaration.positive_root);std::cout<<",\"dimension\":"<<s.dimension<<",\"projector\":";e_matrix(s.projector);std::cout<<'}';}
    std::cout<<"],\"normalization\":";e_matrix(n.normalization());std::cout<<",\"J\":";e_matrix(j.complex_structure());std::cout<<",\"real\":";e_matrix(j.real_periods());std::cout<<",\"imaginary\":";e_matrix(j.imaginary_periods());std::cout<<",\"points\":[";
    const auto vertices=l.source().grade(core::Grade{0}).cell_count();
    const auto b1=topology::integral_boundary_matrix(l.source(),core::Grade{1});
    for(std::size_t v=0;v<vertices;++v){if(v)std::cout<<',';
        // Use the actual integral-period lattice chain receipt; solve paths from the retained
        // unit pairwise source, independently of a new surface wrapper owner.
        const auto& source=l.source();const auto& edges=source.grade(core::Grade{1});
        std::vector<std::size_t> parent(vertices,vertices),edge(vertices);std::vector<int> signs(vertices);parent[0]=0;std::vector<std::size_t> todo{0};
        for(std::size_t k=0;k<todo.size()&&parent[v]==vertices;++k)for(std::size_t e=0;e<edges.cell_count();++e){
            const auto [begin,end]=edges.incidence_range(e);require(end-begin==2,"fixture path source is not pairwise");const auto a=edges.lower_index[begin],b=edges.lower_index[begin+1];
            if(a==todo[k]&&parent[b]==vertices){parent[b]=a;edge[b]=e;signs[b]=1;todo.push_back(b);}
            if(b==todo[k]&&parent[a]==vertices){parent[a]=b;edge[a]=e;signs[a]=-1;todo.push_back(a);}}
        require(parent[v]!=vertices,"fixture vertex disconnected");Z::SparseVector values;for(auto x=v;x!=0;x=parent[x])values.emplace(edge[x],I{signs[x]});
        const auto point=l.point(0,static_cast<std::uint32_t>(v),l.chain(values,g),g);const auto image=n.point(point,g);
        M path(edges.cell_count(),1);for(const auto& [i,z]:values)path.set(i,0,Q{z});
        std::cout<<"{\"vertex\":"<<v<<",\"path\":";q_matrix(path);std::cout<<",\"period_coordinates\":[";for(std::size_t i=0;i<point.coordinates().size();++i){if(i)std::cout<<',';q_json(point.coordinates()[i]);}
        std::cout<<"],\"lattice_coordinates\":[";for(std::size_t i=0;i<image.real_lattice_coordinates().size();++i){if(i)std::cout<<',';q_json(image.real_lattice_coordinates()[i]);}
        std::cout<<"],\"coordinates\":[";for(std::size_t i=0;i<image.coordinates().size();++i){if(i)std::cout<<',';std::cout<<'[';e_json(image.coordinates()[i].real);std::cout<<',';e_json(image.coordinates()[i].imaginary);std::cout<<']';}std::cout<<"]}";
    }
    (void)b1;std::cout<<"]}";
}
int main(int argc,char** argv){try{
    const bool dump=argc==2&&std::string(argv[1])=="--dump";Guard g;const auto fixtures=cases(g);
    if(dump)std::cout<<'[';
    for(std::size_t k=0;k<fixtures.size();++k){const auto& c=fixtures[k];Guard build;const auto n=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,build);
        require(n.work().preparation_steps==build.steps(),"normalization work clock differs");require(n.symplectic().lattice().same_owner(c.symplectic.lattice()),"original period owner lost");
        const auto& j=n.jacobian();const auto& f=j.embedding().field();const auto h=j.lattice().period_rank();const auto identity=A::identity(h,f,g);
        require(j.complex_structure().multiply(j.complex_structure(),g)==identity.scale(f.scalar(Q{-1}),g),"whole complex structure square differs");
        A sum(h,h,f);std::size_t dimension=0;
        for(const auto& sector:n.sectors()){sum=sum.add(sector.projector,g);dimension+=sector.dimension;
            require(sector.projector.multiply(sector.projector,g)==sector.projector,"spectral projector is not idempotent");
            require(c.embedding.sign(sector.declaration.positive_root)>0,"nonpositive selected root");}
        require(sum==identity&&dimension==h,"spectral family is incomplete");
        if(c.name=="native-repeated-rank-four")require(n.sectors().size()==1&&n.sectors()[0].dimension==4,"repeated eigenspace split into false distinct frequencies");
        if(c.name=="native-coupled-genus-two"){
            M expected(4,4);for(std::size_t i=0;i<4;i+=2){expected.set(i,i,Q{2,3});expected.set(i+1,i+1,Q{2,3});expected.set(i,i+1,Q{-1,3});expected.set(i+1,i,Q{-1,3});}
            require(n.normalization()==A::from_rational(expected,f,g),"finite-frequency differs from the prior supplied genus-two normalizer");}
        const auto base=j.lattice().point(0,0,j.lattice().chain({},g),g);auto shifted_values=Z::SparseVector{};
        for(const auto& [ij,z]:j.lattice().integral_cycles().entries())if(ij.second==0)shifted_values.emplace(ij.first,z);
        const auto shifted=j.lattice().point(0,0,j.lattice().chain(shifted_values,g),g);
        require(n.point(base,g).same_point(n.point(shifted,g)),"integral path change moved normalized point");
        if(dump){if(k)std::cout<<',';emit(c,n,g);}
    }
    if(dump)std::cout<<"]\n";
    const auto& c=fixtures[5];
    for(int kind=0;kind<8;++kind){auto frequencies=c.frequencies;
        if(kind==0)frequencies.pop_back();
        if(kind==1)frequencies.push_back(frequencies[0]);
        if(kind==2)frequencies[1].positive_root=c.embedding.field().scalar(Q{-2});
        if(kind==3)frequencies[1].positive_root=c.embedding.field().scalar(Q{3});
        if(kind==4)frequencies[1]={c.embedding.field().scalar(Q{0}),c.embedding.field().scalar(Q{0})};
        if(kind==5)frequencies[1].squared_frequency=c.embedding.field().scalar(Q{-4});
        if(kind==6){const auto alien=rational_field();frequencies[1].positive_root=alien.scalar(Q{2});}
        if(kind==7){const auto alien=rational_field();frequencies[1].squared_frequency=alien.scalar(Q{4});}
        rejects([&]{(void)Normalization::prepare(c.symplectic,frequencies,c.embedding,g);},"invalid/incomplete/duplicate/foreign/root frequency admitted");}
    const auto& repeated=fixtures[15];auto extra=repeated.frequencies;extra.push_back({repeated.embedding.field().scalar(Q{4}),repeated.embedding.field().scalar(Q{2})});
    rejects([&]{(void)Normalization::prepare(repeated.symplectic,extra,repeated.embedding,g);},"empty extraneous eigenspace admitted");
    const auto& radical=fixtures[7];const auto negative=Embedding::isolate(radical.embedding.field(),Q{-2},Q{-1});
    rejects([&]{(void)Normalization::prepare(radical.symplectic,radical.frequencies,negative,g);},"wrong selected real root accepted");
    const auto alien_field=rational_field();rejects([&]{(void)Normalization::prepare(c.symplectic,c.frequencies,rational_embedding(alien_field),g);},"foreign coefficient field embedding accepted");
    const auto n=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,g);const auto foreign=Normalization::prepare(fixtures[6].symplectic,fixtures[6].frequencies,fixtures[6].embedding,g);
    const auto point=n.jacobian().lattice().point(0,0,n.jacobian().lattice().chain({},g),g);const auto foreign_point=foreign.jacobian().lattice().point(0,0,foreign.jacobian().lattice().chain({},g),g);
    rejects([&]{(void)n.point(foreign_point,g);},"foreign period point accepted");
    const auto retained=[&](){Guard local;const auto owned=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,local);return owned.point(point,local);}();require(retained.owner().lattice().same_owner(c.symplectic.lattice()),"native point lost actual normalization owner");
    auto reordered=c.frequencies;std::reverse(reordered.begin(),reordered.end());const auto reverse=Normalization::prepare(c.symplectic,reordered,c.embedding,g);
    require(reverse.normalization()==n.normalization(),"declared list order changed full normalizer");
    Guard measured;const auto prepared=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,measured);const auto steps=measured.steps();
    for(const auto stop:{std::size_t{1},steps/2,steps}){std::size_t calls=0;Guard cancelled({},[&]{if(++calls==stop)throw std::runtime_error("cancel");});std::optional<Normalization> published;
        rejects<std::runtime_error>([&]{published=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,cancelled);},"normalization cancellation bypassed");require(!published,"cancelled normalization published a partial result");}
    for(int kind=0;kind<4;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.dimension=1;if(kind==1)limits.entries=100;if(kind==2)limits.steps=steps-1;if(kind==3)limits.integer_bits=1;
        Guard small(limits);rejects<std::length_error>([&]{(void)Normalization::prepare(c.symplectic,c.frequencies,c.embedding,small);},"receiving normalization budget bypassed");}
    for(int kind=0;kind<3;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.entries=100;if(kind==1)limits.integer_bits=1;std::size_t calls=0;Guard small(limits,[&]{if(kind==2&&++calls==3)throw std::runtime_error("cancel");});
        if(kind==2)rejects<std::runtime_error>([&]{(void)prepared.point(point,small);},"point cancellation bypassed");else rejects<std::length_error>([&]{(void)prepared.point(point,small);},"receiving point budget bypassed");}
    if(!dump)std::cout<<checks<<" complete spectral normalization, roots, Riemann, owner and budget checks passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}}
