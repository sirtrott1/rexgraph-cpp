#include "elliptic_factor_fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <optional>
using namespace factor_fixtures;
static std::size_t checks{};
static void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Error=std::invalid_argument,class Work>static void rejects(Work work,const char* why){bool bad=false;try{work();}catch(const Error&){bad=true;}require(bad,why);}
static M rational(const Z& m,Guard& g){return field::period_detail::rational(m,g);}
static void q_json(const Q& q){std::cout<<'"'<<q<<'"';}
static void e_json(const E& x){std::cout<<'[';for(std::size_t i=0;i<x.coordinates().size();++i){if(i)std::cout<<',';q_json(x.coordinates()[i]);}std::cout<<']';}
template<class Matrix>static void matrix_json(const Matrix& m){std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<m.cols();++j){if(j)std::cout<<',';q_json(Q{m.get(i,j)});}std::cout<<']';}std::cout<<']';}
static void algebraic_json(const A& m){std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<m.cols();++j){if(j)std::cout<<',';e_json(m.get(i,j));}std::cout<<']';}std::cout<<']';}
static void saturated_in_kernel(const Factors::Kernel& kernel,const Z& lattice,Guard& g){
    const auto coordinates=kernel.smith.right_inverse.apply(lattice,g);const auto h=coordinates.rows();
    for(const auto& [ij,z]:coordinates.entries())if(ij.first<kernel.smith.rank)require(z==0,"accepted lattice left full source kernel");
    const auto w=topology::select_rows(coordinates,h-2,2);const I determinant=w.get(0,0)*w.get(1,1)-w.get(0,1)*w.get(1,0);
    require(determinant==1||determinant==-1,"accepted lattice is a proper sublattice of the full integer kernel");
    require(kernel.lattice_basis.multiply(w,g)==lattice,"integer kernel frame transport residual");
}
static void emit(const Case& input,const Normalization& n,const std::optional<Factors>& result){
    const auto& j=n.jacobian();const auto& f=input.embedding.field();
    std::cout<<"{\"name\":\""<<input.name<<"\",\"status\":\""<<(result?"constructed":"unsupported_profile")<<"\",\"polynomial\":[";
    for(std::size_t i=0;i<f.minimal_polynomial().size();++i){if(i)std::cout<<',';q_json(f.minimal_polynomial()[i]);}std::cout<<"],\"embedding\":[";q_json(input.embedding.certificate().interval.lower);std::cout<<',';q_json(input.embedding.certificate().interval.upper);std::cout<<']';
    std::cout<<",\"gram\":";matrix_json(j.lattice().gram());std::cout<<",\"cup\":";matrix_json(j.lattice().cup_form());std::cout<<",\"square\":";matrix_json(n.star_square());std::cout<<",\"J\":";algebraic_json(j.complex_structure());
    std::cout<<",\"sectors\":[";for(std::size_t i=0;i<n.sectors().size();++i){if(i)std::cout<<',';const auto& s=n.sectors()[i];std::cout<<"{\"mu\":";e_json(s.declaration.squared_frequency);std::cout<<",\"root\":";e_json(s.declaration.positive_root);std::cout<<",\"dimension\":"<<s.dimension<<'}';}std::cout<<']';
    if(result){const auto& cert=result->certificate();std::cout<<",\"index\":\""<<cert.index()<<"\",\"inclusion\":";matrix_json(cert.lattice_inclusion());std::cout<<",\"factors\":[";
        for(std::size_t i=0;i<cert.factors().size();++i){if(i)std::cout<<',';const auto& factor=cert.factors()[i];const auto& kernel=result->kernels()[i];
            std::cout<<"{\"mu\":";q_json(factor.squared_frequency);std::cout<<",\"denominator\":\""<<kernel.denominator<<"\",\"equation\":";matrix_json(kernel.integer_equation);std::cout<<",\"kernel\":";matrix_json(kernel.lattice_basis);std::cout<<",\"declared\":";matrix_json(factor.declared_rational_basis);std::cout<<",\"saturated\":";matrix_json(factor.saturated_lattice);std::cout<<",\"intersection\":\""<<factor.intersection<<"\",\"gram\":";matrix_json(factor.gram);std::cout<<",\"modulus\":[";e_json(factor.modulus.real);std::cout<<',';e_json(factor.modulus.imaginary);std::cout<<"]}";
        }std::cout<<']';}
    std::cout<<'}';
}
int main(int argc,char** argv){try{
    const bool dump=argc==2&&std::string(argv[1])=="--dump";Guard g;const auto inputs=factor_fixtures::cases(g);std::size_t admitted_count=0;
    if(dump)std::cout<<'[';
    for(std::size_t k=0;k<inputs.size();++k){const auto& c=inputs[k];const auto n=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,g);std::optional<Factors> result;
        if(admitted(c)){Guard receiving;result=Factors::prepare(n,receiving);++admitted_count;require(result->work().preparation_steps==receiving.steps(),"factor work clock differs");
            require(result->normalization().jacobian().same_owner(n.jacobian())&&result->certificate().owner().same_owner(n.jacobian()),"factor output lost actual normalization owner");
            const auto& cert=result->certificate();require(cert.factors().size()==n.sectors().size(),"incomplete factor carrier");std::vector<field::DeclaredPeriodPlane> scaled;
            for(std::size_t i=0;i<cert.factors().size();++i){const auto& kernel=result->kernels()[i];const auto& f=cert.factors()[i];saturated_in_kernel(kernel,f.saturated_lattice,g);
                require(kernel.smith.rank+2==n.work().period_rank,"incomplete kernel certificate");require(kernel.integer_equation.multiply(kernel.lattice_basis,g).nonzeros()==0,"constructed plane is not in source kernel");
                auto basis=f.declared_rational_basis;for(std::size_t row=0;row<basis.rows();++row){basis.set(row,0,g.mul(basis.get(row,0),Q{2}));basis.set(row,1,g.mul(basis.get(row,1),Q{1,3}));}
                scaled.push_back({f.squared_frequency,n.sectors()[i].declaration.positive_root,std::move(basis)});
            }
            const auto repeated=n.jacobian().declared_elliptic_factors(scaled,g);require(repeated.index()==cert.index(),"scaled rational generators changed full isogeny index");
            for(std::size_t i=0;i<repeated.factors().size();++i){saturated_in_kernel(result->kernels()[i],repeated.factors()[i].saturated_lattice,g);require(repeated.factors()[i].intersection==cert.factors()[i].intersection,"scaling changed primitive factor polarization");}
            if(c.name=="native-coupled-genus-two")require(cert.index()==4,"genus-two index4 regression failed");
            if(c.name.starts_with("symmetric-L"))require(cert.index()==9,"symmetric L isogeny index differs from original experiments");
            if(c.name=="native-three-factor")require(cert.index()==1&&cert.factors().size()==3,"three-factor direct product differs");
            if(c.name=="zero-H1")require(cert.index()==1&&cert.factors().empty(),"empty product convention differs");
        }else{
            rejects([&]{result=Factors::prepare(n,g);},"unsupported frequency/repeated sector received complete automatic factors");require(!result,"unsupported profile published partial factors");
            const auto h=n.work().period_rank;require(n.jacobian().complex_structure().multiply(n.jacobian().complex_structure(),g)==A::identity(h,c.embedding.field(),g).scale(c.embedding.field().scalar(Q{-1}),g),"factor refusal damaged valid finite-frequency normalization");
        }
        if(dump){if(k)std::cout<<',';emit(c,n,result);}
    }
    if(dump)std::cout<<"]\n";
    require(admitted_count==16,"admitted fixture count differs");
    const auto& c=inputs[16];const auto n=Normalization::prepare(c.symplectic,c.frequencies,c.embedding,g);const auto result=Factors::prepare(n,g);
    std::vector<field::DeclaredPeriodPlane> declarations;for(std::size_t i=0;i<result.kernels().size();++i)declarations.push_back({result.kernels()[i].squared_frequency,n.sectors()[i].declaration.positive_root,rational(result.kernels()[i].lattice_basis,g)});
    auto missing=declarations;missing.pop_back();rejects([&]{(void)n.jacobian().declared_elliptic_factors(missing,g);},"missing factor accepted complete isogeny");
    auto noninvariant=declarations;std::size_t coordinate=0;const auto& equation=result.kernels()[0].integer_equation;
    for(const auto& [ij,z]:equation.entries())if(z!=0){coordinate=ij.second;break;}
    noninvariant[0].rational_basis.set(coordinate,0,g.add(noninvariant[0].rational_basis.get(coordinate,0),Q{1}));
    rejects([&]{(void)n.jacobian().declared_elliptic_factors(noninvariant,g);},"noninvariant supplied plane bypassed final validator");
    auto alien=declarations;const auto field=rational_field();alien[0].positive_root=field.scalar(Q{1});rejects([&]{(void)n.jacobian().declared_elliptic_factors(alien,g);},"foreign factor root owner accepted");
    auto incomplete=c.frequencies;incomplete.pop_back();rejects([&]{(void)Normalization::prepare(c.symplectic,incomplete,c.embedding,g);},"incomplete normalization reached factor construction");
    // Preserve the old explicitly supplied repeated-sector plane workflow.
    const auto& repeat=inputs[15];const auto repeated=Normalization::prepare(repeat.symplectic,repeat.frequencies,repeat.embedding,g);M first(4,2),second(4,2);first.set(0,0,Q{1});first.set(2,1,Q{1});second.set(1,0,Q{1});second.set(3,1,Q{1});
    const auto supplied=repeated.jacobian().declared_elliptic_factors({{Q{1},repeat.frequencies[0].positive_root,first},{Q{1},repeat.frequencies[0].positive_root,second}},g);require(supplied.index()==1,"supplied repeated-sector workflow regressed");
    const auto retained=[&](){Guard local;const auto all=Factors::prepare(n,local);return all.certificate();}();require(retained.owner().same_owner(n.jacobian()),"retained isogeny certificate lost owner lifetime");
    Guard measured;const auto prepared=Factors::prepare(n,measured);const auto steps=measured.steps();(void)prepared;
    for(const auto stop:{std::size_t{1},steps/2,steps}){std::size_t calls=0;Guard cancelled({},[&]{if(++calls==stop)throw std::runtime_error("cancel");});std::optional<Factors> published;
        rejects<std::runtime_error>([&]{published=Factors::prepare(n,cancelled);},"factor cancellation bypassed");require(!published,"cancelled factor construction published partial authority");}
    for(int kind=0;kind<4;++kind){numeric::ExactActionLimits limits;if(kind==0)limits.dimension=1;if(kind==1)limits.entries=100;if(kind==2)limits.steps=steps-1;if(kind==3)limits.integer_bits=1;
        Guard small(limits);rejects<std::length_error>([&]{(void)Factors::prepare(n,small);},"receiving factor budget bypassed");}
    if(!dump)std::cout<<checks<<" complete rational factor, saturation, isogeny, owner and budget checks passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return EXIT_FAILURE;}}
