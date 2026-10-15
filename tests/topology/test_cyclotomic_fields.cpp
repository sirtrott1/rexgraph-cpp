#include "rfc/core/builder.hpp"
#include "rfc/numeric/algebraic_phase.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/storage/algebraic_archive_codec.hpp"
#include <iostream>
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using F=numeric::CertifiedAlgebraicField;
namespace {
std::size_t checks{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class C>void rejects(C call,const char* why){bool caught=false;try{call();}catch(const std::exception&){caught=true;}check(caught,why);}
V product(const V& a,const V& b) {
    V out(a.size()+b.size()-1);for(std::size_t i=0;i<a.size();++i)for(std::size_t j=0;j<b.size();++j)out[i+j]+=a[i]*b[j];return out;
}
void identities() {
    static_assert(!std::constructible_from<numeric::CyclotomicIrreducibility,double>);
    for(unsigned n=1;n<=32;++n) {
        const auto f=F::declare_cyclotomic(n);const auto a=f.generator(),one=f.scalar(Q{1});
        check(a.power(n)==one,"cyclotomic generator does not have declared period");
        V p{1};for(unsigned d=1;d<=n;++d)if(n%d==0) {
            p=product(p,numeric::cyclotomic_polynomial(d));
            if(d<n)check(a.power(d)!=one,"cyclotomic generator has smaller order");
        }
        V expected(n+1);expected.front()=-1;expected.back()=1;
        check(p==expected,"divisor polynomial product differs from x^n-1");
        const auto alternate=F::declare(f.minimal_polynomial(),numeric::CyclotomicIrreducibility{n});
        check(alternate.minimal_polynomial()==f.minimal_polynomial() && !alternate.same_owner(f),"declaration bypassed field ownership");
    }
    const auto f=F::declare_cyclotomic(12);
    check(f.minimal_polynomial()==V({1,0,-1,0,1}),"Phi12 coefficient certificate differs");
    const auto involution=numeric::AlgebraicInvolution::declare(f,f.generator().inverse());
    const numeric::AlgebraicUnitPhase phase(involution,f.generator());const auto gram=involution.trace_form();
    (void)field::ExactMetric(gram);
    const auto r=phase.regular_action();check(r.transpose().multiply(gram).multiply(r)==gram,"Phi12 trace isometry failed");
    rejects([]{(void)F::declare({1,0,1},numeric::CyclotomicIrreducibility{12});},"wrong polynomial accepted as Phi12");
    rejects([]{(void)F::declare_cyclotomic(0);},"zero order accepted");
    rejects([]{numeric::AlgebraicFieldLimits l;l.max_degree=3;(void)F::declare_cyclotomic(12,l);},"degree budget ignored");
    rejects([]{numeric::AlgebraicFieldLimits l;l.max_input_coefficients=12;(void)F::declare_cyclotomic(12,l);},"intermediate coefficient budget ignored");
    rejects([]{numeric::AlgebraicFieldLimits l;l.max_scalar_steps=1;(void)F::declare_cyclotomic(12,l);},"construction work budget ignored");
}
void archive() {
    core::RelationSignature signature;auto type=signature.add_type("edge",core::BoundaryPolicy::DeclaredExact,0,4);
    core::RelationalComplexBuilder builder(signature);builder.add_grade_one(type,{{1,true,true,1},{2,true,false,1}});
    const auto source=builder.freeze();const auto field=F::declare_cyclotomic(12);
    const auto bundle=local::AlgebraicStalkBundle::declare(source,field,{{1,1},{1}});
    storage::AlgebraicArchiveContents contents;contents.involution=numeric::AlgebraicInvolution::declare(field,field.generator().inverse());
    contents.records={{7,bundle.stalk({core::Grade{0},0}).vector({field.generator().power(5)})}};
    const auto original=storage::AlgebraicArchive::declare(bundle,contents);
    const auto bytes=storage::encode_algebraic_archive(original),restored_bytes=storage::encode_algebraic_archive(storage::decode_algebraic_archive(bytes));
    check(bytes==restored_bytes,"cyclotomic archive replay bytes differ");
    const auto restored=storage::decode_algebraic_archive(bytes);
    check(std::get<numeric::CyclotomicIrreducibility>(restored.bundle().field().certificate()).order==12,"cyclotomic order not retained");
    check(!restored.bundle().same_owner(bundle),"archive reused an undeclared owner");
    const auto& value=std::get<local::AlgebraicModuleVector>(restored.at(7));
    check(value.coordinates().front()==restored.bundle().field().generator().power(5),"restored field value differs");
}
}
int main(){try{identities();archive();std::cout<<checks<<" cyclotomic checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
