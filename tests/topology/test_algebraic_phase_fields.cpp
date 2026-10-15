#include "rfc/geometry/quadratic_embedding.hpp"
#include "rfc/numeric/rational_phase.hpp"
#include "rfc/topology/algebraic_attachment.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/exact_metric.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include "rfc/storage/rfcx.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using M=rfc::numeric::ExactMatrix;
using Field=rfc::numeric::CertifiedAlgebraicField;
using Element=rfc::numeric::AlgebraicElement;
using Involution=rfc::numeric::AlgebraicInvolution;
using Phase=rfc::numeric::AlgebraicUnitPhase;
using Embedding=rfc::geometry::SelectedQuadraticEmbedding;
using G=rfc::core::Grade;
using Prepared=rfc::topology::PreparedAlgebraicAttachingRealization;
constexpr auto convention=rfc::topology::AttachingTransportConvention::ChronologicalColumns;
static std::size_t checks{},certificate_candidates{};
static void require(bool value,const char* reason) {
    if (!value) throw std::runtime_error(reason);
    ++checks;
}
template<class Exception,class Action> static void rejects(Action action,const char* reason) {
    bool caught=false; try {action();} catch(const Exception&) {caught=true;}
    require(caught,reason);
}
template<class T> concept ScalarAccepted=requires(const Field& f,T value) {f.scalar(value);};
template<class T> concept PowerAccepted=requires(const Element& v,T exponent) {v.power(exponent);};
static_assert(!ScalarAccepted<double> && !PowerAccepted<double>);
static_assert(!std::default_initializable<Field> && !std::default_initializable<Element>);
static_assert(!std::constructible_from<rfc::numeric::EisensteinShift,unsigned,double>);
static_assert(!std::constructible_from<rfc::numeric::IrreducibleReduction,double>);
template<class T> concept TemporaryCoordinates=requires(T value) {std::move(value).coordinates();};
static_assert(!TemporaryCoordinates<Element>);

// Independent exhaustive trial factors over tiny finite fields, rather than
// the production certificate's Frobenius/gcd algorithm.
static bool trial_irreducible(const std::vector<unsigned>& f,unsigned p) {
    const auto degree=f.size()-1;
    for (std::size_t d=1;d<=degree/2;++d) {
        std::size_t count=1; for(std::size_t i=0;i<d;++i) count*=p;
        for (std::size_t code=0;code<count;++code) {
            auto digits=code; std::vector<unsigned> factor(d+1); factor[d]=1;
            for (std::size_t i=0;i<d;++i) {factor[i]=static_cast<unsigned>(digits%p); digits/=p;}
            auto remainder=f;
            while (remainder.size()>=factor.size()) {
                const auto k=remainder.size()-factor.size(); const auto lead=remainder.back();
                for (std::size_t i=0;i<factor.size();++i) remainder[k+i]=(remainder[k+i]+p-lead*factor[i]%p)%p;
                while (!remainder.empty() && !remainder.back()) remainder.pop_back();
            }
            if (remainder.empty()) return false;
        }
    }
    return true;
}
static void certificates() {
    for (const unsigned p:{2u,3u,5u}) {
        const std::size_t top=p==2?6:4;
        for (std::size_t degree=1,count=p;degree<=top;++degree,count*=p) {
            for (std::size_t code=0;code<count;++code) {
                auto digits=code; std::vector<unsigned> coefficients(degree+1); V polynomial(degree+1);
                coefficients[degree]=1; polynomial[degree]=1;
                for(std::size_t i=0;i<degree;++i) {
                    coefficients[i]=static_cast<unsigned>(digits%p); polynomial[i]=coefficients[i]; digits/=p;
                }
                bool admitted=true;
                try {(void)Field::declare(polynomial,rfc::numeric::IrreducibleReduction{p});}
                catch(const std::invalid_argument&) {admitted=false;}
                require(admitted==trial_irreducible(coefficients,p),"irreducibility certificate disagrees with trial factors");
                ++certificate_candidates;
            }
        }
    }
    const auto phi8=Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    require(phi8.generator().power(4)==phi8.scalar(Q{-1}),"shifted Eisenstein exact eighth-root relation");
    const auto phi5=Field::declare({1,1,1,1,1},rfc::numeric::EisensteinShift{5,1});
    require(phi5.generator().power(5)==phi5.scalar(Q{1}),"shifted Eisenstein fifth-root relation");
    const auto scaled=Field::declare({Q{1,2},0,1},rfc::numeric::IrreducibleReduction{5});
    require(scaled.generator()*scaled.generator()==scaled.scalar(Q{-1,2}),"rational coefficient reduction certificate");
    rejects<std::invalid_argument>([]{(void)Field::declare({-1,0,1},rfc::numeric::IrreducibleReduction{3});},"reducible quotient cannot be a field");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{2});},"repeated modular factors reject this certificate");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{9});},"composite certificate prime refusal");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{65537});},"certificate prime admission domain");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{4294967299ULL});},"certificate prime rejects before narrowing");
    rejects<std::invalid_argument>([]{(void)Field::declare({-2,0,1},rfc::numeric::EisensteinShift{4294967298ULL,0});},"Eisenstein prime rejects before narrowing");
    rejects<std::invalid_argument>([]{(void)Field::declare({Q{1,3},0,1},rfc::numeric::IrreducibleReduction{3});},"bad certificate denominator");
    rejects<std::invalid_argument>([]{(void)Field::declare({4,0,1},rfc::numeric::EisensteinShift{2,0});},"Eisenstein prime-square constant refusal");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,1,1},rfc::numeric::EisensteinShift{2,0});},"Eisenstein divisibility refusal");
    rejects<std::invalid_argument>([]{(void)Field::declare({Q{1,2},0,1},rfc::numeric::EisensteinShift{2,0});},"nonintegral Eisenstein refusal");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,2},rfc::numeric::IrreducibleReduction{3});},"nonmonic minimal polynomial refusal");
    rejects<std::invalid_argument>([]{(void)Field::declare({1},rfc::numeric::IrreducibleReduction{3});},"constant minimal polynomial refusal");
    rfc::numeric::AlgebraicFieldLimits limits; limits.max_degree=1;
    rejects<std::length_error>([&]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3},limits);},"degree budget");
    limits={}; limits.max_input_coefficients=2;
    rejects<std::length_error>([&]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3},limits);},"coefficient budget");
    limits={}; limits.max_scalar_steps=0;
    rejects<std::length_error>([&]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3},limits);},"certificate work budget");
}
static void arithmetic(const Field& field) {
    const auto zero=field.scalar(Q{0}),one=field.scalar(Q{1}),alpha=field.generator();
    require(field.evaluate(field.minimal_polynomial(),alpha)==zero,"exact minimal polynomial relation");
    std::size_t count=1; for(std::size_t i=0;i<field.degree();++i) count*=3;
    std::vector<Element> values; values.reserve(count);
    for(std::size_t code=0;code<count;++code) {
        auto digits=code; V coordinates(field.degree());
        for(auto& coefficient:coordinates) {coefficient=static_cast<long>(digits%3)-1; digits/=3;}
        values.push_back(field.element(coordinates));
        const auto& value=values.back(); const auto action=value.regular_action();
        require(action.apply(one.coordinates())==coordinates,"faithful action retains full coefficient vector");
        require(value+zero==value && value-value==zero,"field additive identities");
        if (!value.is_zero()) {
            require(value*value.inverse()==one,"nonzero exact algebraic inverse");
            require(value.inverse().regular_action()==action.inverse(),"inverse agrees with existing rational native solve");
        }
    }
    for (const auto& a:values) for (const auto& b:values) {
        require((a*b).regular_action()==a.regular_action().multiply(b.regular_action()),"faithful multiplication action");
        require(a*(b+alpha)==a*b+a*alpha,"exact algebraic distributivity");
        require((a+b).regular_action()==a.regular_action().add(b.regular_action()),"faithful addition action");
    }
    rejects<std::domain_error>([&]{(void)zero.inverse();},"zero inverse rejection");
    require(alpha.power(0)==one && alpha.power(7)==alpha.power(3)*alpha.power(4),"exact bounded power");
    const rfc::numeric::RationalFunction z=rfc::numeric::RationalFunction::parameter();
    require(field.specialize(z*z+z,alpha)==alpha*alpha+alpha,"formal specialization without floating roots");
    rejects<std::domain_error>([&]{(void)field.specialize(rfc::numeric::RationalFunction({1},field.minimal_polynomial()),alpha);},"specialization pole refusal");
    const auto independent=Field::declare(field.minimal_polynomial(),field.certificate());
    require(!field.same_owner(independent) && field.same_owner(alpha.field()),"actual algebraic domain ownership");
    require(alpha!=independent.generator(),"equal polynomial values do not identify domains");
    rejects<std::invalid_argument>([&]{(void)(alpha+independent.generator());},"foreign addition refusal");
    rejects<std::invalid_argument>([&]{(void)(zero*independent.scalar(Q{0}));},"foreign zero multiplication refusal");
    rejects<std::invalid_argument>([&]{(void)field.evaluate({1,1},independent.generator());},"foreign evaluation refusal");
}
static void roots_and_involutions() {
    const auto real=Field::declare({-2,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto imaginary=Field::declare({1,-1,1},rfc::numeric::IrreducibleReduction{2});
    const auto larger=Embedding::real_root(real,rfc::geometry::QuadraticRealRoot::Larger,1,2);
    const auto smaller=Embedding::real_root(real,rfc::geometry::QuadraticRealRoot::Smaller,-2,-1);
    require(larger.coordinates(real.generator()).radical_coefficient==1 && smaller.coordinates(real.generator()).radical_coefficient==-1,"exact real root branch selection");
    require(larger.coordinates(real.element({3,2})).radicand==2,"exact positive radical retained");
    require(larger.conjugation().apply(real.generator())==real.generator(),"complex conjugation fixes a selected real root");
    const auto galois=Involution::declare(real,-real.generator());
    require(galois.apply(real.generator())==-real.generator(),"Galois involution remains separate from complex conjugation");
    rejects<std::invalid_argument>([&]{(void)rfc::field::ExactMetric(galois.trace_form());},"indefinite involution form is not a positive metric");
    const rfc::field::ExactMetric real_trace(larger.conjugation().trace_form());
    require(real_trace.inner({1,2},{1,2})==9,"positive real trace form measures all embeddings");
    const auto upper=Embedding::complex_root(imaginary,rfc::geometry::QuadraticHalfPlane::Upper);
    const auto lower=Embedding::complex_root(imaginary,rfc::geometry::QuadraticHalfPlane::Lower);
    const auto coordinates=upper.coordinates(imaginary.generator());
    require(coordinates.rational_part==Q{1,2} && coordinates.radicand==Q{3,4} && coordinates.radical_coefficient==1,"exact sixth-root complex coordinates");
    require(lower.coordinates(imaginary.generator()).radical_coefficient==-1,"lower complex root remains separately selected");
    const auto conjugation=upper.conjugation(); const Phase phase(conjugation,imaginary.generator());
    const auto six_turns=phase.power(6),three_turns=phase.power(3),cancelled=phase*phase.inverse();
    require(six_turns.value()==imaginary.scalar(Q{1}) && three_turns.value()==imaginary.scalar(Q{-1}),"sixth-root exact period without pi");
    require(cancelled.value()==imaginary.scalar(Q{1}),"involutive phase inverse");
    require(conjugation.trace_form()==upper.complex_norm_form(),"declared trace form agrees with selected complex norm");
    const auto metric=upper.complex_norm_form(),action=phase.regular_action();
    require(action.transpose().multiply(metric).multiply(action)==metric,"exact nonorthonormal phase isometry");
    require(action.transpose()!=action.inverse(),"inverse is not inferred from coordinate transpose");
    require(metric.inverse().multiply(action.transpose()).multiply(metric)==action.inverse(),"declared metric adjoint agrees for unit phase");
    require(upper.phase_coordinates(phase).axis==rfc::geometry::RadicalAxis::Imaginary,"compatible declared complex phase");
    const Phase formal_identity(Involution::identity(imaginary),imaginary.scalar(Q{1}));
    rejects<std::invalid_argument>([&]{(void)upper.phase_coordinates(formal_identity);},"incompatible involution cannot silently become complex conjugation");
    rejects<std::invalid_argument>([&]{(void)Phase(conjugation,imaginary.scalar(Q{2}));},"nonunit algebraic phase refusal");
    rejects<std::invalid_argument>([&]{(void)(phase*Phase(upper.conjugation(),imaginary.generator()));},"separate involution owners remain separate declarations");
    rejects<std::invalid_argument>([&]{(void)Involution::declare(imaginary,imaginary.scalar(Q{0}));},"invalid polynomial image refusal");
    rejects<std::invalid_argument>([&]{(void)Involution::declare(imaginary,real.generator());},"foreign involution image refusal");
    rejects<std::invalid_argument>([&]{(void)Embedding::complex_root(real,rfc::geometry::QuadraticHalfPlane::Upper);},"real field is not imaginary by declaration");
    rejects<std::invalid_argument>([&]{(void)Embedding::real_root(imaginary,rfc::geometry::QuadraticRealRoot::Larger,0,2);},"imaginary quadratic has no real isolating interval");
    rejects<std::invalid_argument>([&]{(void)Embedding::real_root(real,rfc::geometry::QuadraticRealRoot::Larger,-2,-1);},"wrong real root interval");
    rejects<std::invalid_argument>([&]{(void)Embedding::real_root(real,rfc::geometry::QuadraticRealRoot::Larger,1,1);},"empty interval refusal");
    rejects<std::invalid_argument>([&]{(void)Embedding::complex_root(imaginary,static_cast<rfc::geometry::QuadraticHalfPlane>(9));},"invalid root selection enum");
    rejects<std::invalid_argument>([&]{(void)upper.coordinates(real.generator());},"foreign embedding element refusal");
    rejects<std::domain_error>([&]{(void)larger.complex_norm_form();},"real trace action is not a complex plane norm");
    const auto phi5=Field::declare({1,1,1,1,1},rfc::numeric::IrreducibleReduction{2});
    const auto phi5_bar=Involution::declare(phi5,phi5.generator().inverse());
    const Phase fifth(phi5_bar,phi5.generator()); const auto fifth_metric=phi5_bar.trace_form();
    (void)rfc::field::ExactMetric(fifth_metric);
    const auto five_turns=fifth.power(5),one_turn=fifth.power(1);
    require(five_turns.value()==phi5.scalar(Q{1}) && one_turn.value()!=phi5.scalar(Q{1}),"higher-degree phase exact period");
    require(fifth.regular_action().transpose().multiply(fifth_metric).multiply(fifth.regular_action())==fifth_metric,"higher-degree trace isometry");
    rejects<std::invalid_argument>([&]{(void)Embedding::complex_root(phi5,rfc::geometry::QuadraticHalfPlane::Upper);},"regular degree-four action is not silently a quadratic spatial root");
    // In the fifth-root field alpha -> alpha^2 is an automorphism of order four.
    rejects<std::invalid_argument>([&]{(void)Involution::declare(phi5,phi5.generator().power(2));},"noninvolutive field automorphism refusal");
    for(const auto& field:{imaginary,phi5}) {
        const auto star=Involution::declare(field,field.generator().inverse());
        const auto gram=star.trace_form();
        for(int k=-4;k<=4;++k) {
            const auto v=field.scalar(Q{1})+field.scalar(Q(k,3))*field.generator();
            const Phase unit(star,v/star.apply(v)); const auto R=unit.regular_action();
            require(star.apply(star.apply(v))==v,"full involution action squares to identity");
            require(star.apply(v*v)==star.apply(v)*star.apply(v),"involution preserves multiplication");
            require(R.transpose().multiply(gram).multiply(R)==gram,"aperiodic algebraic unit preserves the declared trace form");
        }
    }
    const auto qi=Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto qi_root=Embedding::complex_root(qi,rfc::geometry::QuadraticHalfPlane::Upper);
    const auto qi_star=qi_root.conjugation();
    for(int k=-6;k<=6;++k) {
        const Q parameter(k,5);const auto v=qi.scalar(Q{1})+qi.scalar(parameter)*qi.generator();
        const Phase unit(qi_star,v/qi_star.apply(v));
        require(unit.regular_action()==rfc::numeric::RationalPhase::cayley(parameter).real_action(),"existing Q(i) Cayley phase remains identical");
    }
    const auto copied_field=std::move(qi);
    require(qi.same_owner(copied_field),"owning field handle remains pinned across rvalue copies");
}
static rfc::core::RelationalComplex source(const std::vector<int>& word) {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("meridian",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    const auto face=signature.add_attaching_loop_type("attaching_word",1,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge,{{7,true,true,false,1},{7,true,false,false,1}});
    builder.add_grade_one(edge,{{7,true,true,false,1},{7,true,false,false,1}});
    builder.declare_grade(G{2}); std::vector<rfc::core::HigherIncidenceInput> occurrences;
    for(const auto letter:word) {
        occurrences.emplace_back(0,false,false,false,Q{7});
        occurrences.emplace_back(static_cast<std::uint32_t>((letter>0?letter:-letter)-1),true,letter<0,false,Q{1});
    }
    builder.add_higher(G{2},face,occurrences); return builder.freeze();
}
static void attachments() {
    using namespace rfc;
    const auto original=source({1,2,1,-2,-1,-2}); const auto bytes=storage::encode_rfcx(original).bytes;
    const topology::PreparedCyclicAttachingRealization formal(original);
    const auto sixth=Field::declare({1,-1,1},numeric::IrreducibleReduction{2});
    const auto third=Field::declare({1,1,1},numeric::IrreducibleReduction{2});
    const auto exceptional=Prepared::common_generator(formal,sixth.generator(),convention);
    const auto regular=Prepared::common_generator(formal,third.generator(),convention);
    require(exceptional.formal().same_owner(formal),"original formal Laurent profile stays pinned");
    require(exceptional.flat() && regular.flat(),"complete flat trefoil specializations");
    require(exceptional.homology_dimensions_over_field()==std::array<std::size_t,3>{0,1,1},"exceptional exact phase changes field homology");
    require(regular.homology_dimensions_over_field()==std::array<std::size_t,3>{0,0,0},"regular exact phase has different homology");
    require(exceptional.native().homology_dimensions()==std::array<std::size_t,3>{0,2,2},"rational restriction dimensions stay separate from algebraic dimensions");
    require(!formal.faces()[0].derivatives[0].is_zero() && exceptional.faces()[0].derivatives[0].is_zero(),"formal operation is not erased by equal specialized zero");
    require(exceptional.faces()[0].occurrences.size()==12,"inactive/repeated attaching slots retained");
    for(const auto& term:exceptional.faces()[0].occurrences)
        require(term.active || term.contribution.is_zero(),"inactive specialized slots contribute no arithmetic");
    for(const auto* prepared:{&exceptional,&regular}) {
        const auto& terms=prepared->faces()[0].occurrences;
        const auto& native_terms=prepared->native().faces()[0].occurrences;
        for(std::size_t i=0;i<terms.size();++i) {
            require(terms[i].prefix_before.regular_action()==native_terms[i].prefix_before,"every specialized prefix agrees with native transport");
            require(terms[i].contribution.regular_action()==native_terms[i].contribution,"every specialized occurrence agrees with its realized native fiber");
        }
    }
    require(storage::encode_rfcx(exceptional.source()).bytes==bytes && storage::encode_rfcx(original).bytes==bytes,"canonical source bytes remain unchanged");
    const auto& native=regular.native();
    const local::PreparedGradeFieldAction action(native.fibers(),G{2});
    auto workspace=action.make_workspace(); const V input{3,2};
    require(action.apply(input,workspace)==native.boundary_two().apply(input),"existing prepared native field execution consumes algebraic restriction");
    const topology::PreparedCyclicAttachingRealization nonflat(source({1,1}));
    const auto curved=Prepared::common_generator(nonflat,sixth.generator(),convention);
    require(!curved.flat(),"nontrivial full algebraic holonomy detects curvature");
    rejects<std::domain_error>([&]{(void)curved.homology_dimensions_over_field();},"curved algebraic family cannot claim homology");
    const auto pinned=[] {
        const topology::PreparedCyclicAttachingRealization temporary(source({1,2,1,-2,-1,-2}));
        const auto field=Field::declare({1,-1,1},numeric::IrreducibleReduction{2});
        return Prepared::common_generator(temporary,field.generator(),convention);
    }();
    require(pinned.homology_dimensions_over_field()[1]==1,"source and coefficient owners survive caller scope");
    rejects<std::invalid_argument>([&]{(void)Prepared::common_generator(formal,sixth.scalar(Q{0}),convention);},"zero Laurent parameter refusal");
    topology::AlgebraicAttachmentLimits limits; limits.max_algebraic_coordinates=1;
    rejects<std::length_error>([&]{(void)Prepared::common_generator(formal,sixth.generator(),convention,limits);},"specialization retained coordinate budget");
}
static void arithmetic_limits() {
    using namespace rfc::numeric;
    AlgebraicFieldLimits limits; limits.max_integer_bits=8;
    const auto field=Field::declare({-2,0,1},EisensteinShift{2,0},limits);
    rejects<std::length_error>([&]{(void)field.scalar(Q{256});},"coefficient integer bit budget");
    rejects<std::length_error>([&]{(void)field.scalar(Q{16}).power(4);},"power growth bit budget");
    rejects<std::length_error>([&]{(void)(field.scalar(Q{200})+field.scalar(Q{200}));},"sum bit budget");
    rejects<std::length_error>([&]{(void)field.element(V(257));},"input polynomial degree budget");
    rejects<std::invalid_argument>([&]{Q bad; mpz_set_ui(mpq_denref(bad.get_mpq_t()),0); (void)field.scalar(bad);},"invalid rational denominator refusal");
    limits={}; limits.max_matrix_coordinates=1;
    const auto matrix_limited=Field::declare({1,0,1},IrreducibleReduction{3},limits);
    rejects<std::length_error>([&]{(void)matrix_limited.generator().regular_action();},"regular matrix coordinate budget");
    rejects<std::length_error>([&]{(void)Involution::identity(matrix_limited);},"involution matrix coordinate budget");
    auto raw=Q{1}; mpz_set_si(mpq_numref(raw.get_mpq_t()),2); mpz_set_ui(mpq_denref(raw.get_mpq_t()),2);
    require(field.scalar(raw)==field.scalar(Q{1}),"canonical input coefficients");
    const auto pinned=[] {const auto temporary=Field::declare({1,0,1},IrreducibleReduction{3}); return temporary.generator();}();
    require(pinned.power(2)==pinned.field().scalar(Q{-1}),"coefficient domain owner survives caller scope");
    limits={};limits.max_integer_bits=1;
    rejects<std::length_error>([&]{(void)Field::declare({-2,0,1},EisensteinShift{2,0},limits);},"minimal polynomial bit admission");
    limits={};limits.max_integer_bits=8;
    rejects<std::length_error>([&]{(void)Field::declare({1,0,1},EisensteinShift{2,Integer{256}},limits);},"Eisenstein shift bit admission");
}
int main() {
    try {
        certificates();
        arithmetic(Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3}));
        arithmetic(Field::declare({1,-1,1},rfc::numeric::IrreducibleReduction{2}));
        arithmetic(Field::declare({-2,0,1},rfc::numeric::EisensteinShift{2,0}));
        arithmetic(Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0}));
        arithmetic(Field::declare({1,1,1,1,1},rfc::numeric::IrreducibleReduction{2}));
        arithmetic(Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1}));
        roots_and_involutions(); attachments(); arithmetic_limits();
        std::cout<<"{\"checks\":"<<checks<<",\"failed\":0,\"certificate_candidates\":"<<certificate_candidates<<"}\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n'; return EXIT_FAILURE;}
}
