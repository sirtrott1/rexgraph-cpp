#include "../../examples/complex_root_certificates/model.hpp"
#include "../../examples/cellular_attachments/model.hpp"
#include "rfc/numeric/algebraic_phase.hpp"
#include <iostream>

using namespace complex_example;
using Z=rfc::geometry::ExactComplexCoordinates;
using Diamond=rfc::geometry::ExactComplexDiamond;
static std::size_t checks{};
static void require(bool value,const char* message) {if (!value) throw std::runtime_error(message); ++checks;}
template<class Error,class F> static void rejects(F action,const char* message) {
    bool caught=false;try {action();} catch (const Error&) {caught=true;}require(caught,message);
}
static Q norm(const Z& value) {return Q(abs(value.real)+abs(value.imaginary));}
static Z difference(const Z& a,const Z& b) {return {Q(a.real-b.real),Q(a.imaginary-b.imaginary)};}
template<class T> concept FloatingCoordinates=requires {T{0.1,0.2};};
template<class T> concept FloatingRadius=requires(const Field& f) {T::isolate(f,Z{0,1},0.1);};
template<class T> concept FloatingRefinement=requires(const T& value) {value.refine(0.1);};
template<class T> concept TemporaryRegion=requires(T value) {std::move(value).region();};
static_assert(!FloatingCoordinates<Z> && !FloatingRadius<Embedding> && !FloatingRefinement<Embedding>);
static_assert(!std::default_initializable<Embedding> && !std::default_initializable<Embedding::Observation>);
static_assert(!std::default_initializable<Embedding::Refinement> && !TemporaryRegion<Embedding::Refinement>);

static void profile(const Case& sample) {
    const auto& e=sample.embedding; const auto original=e.certificate().region;
    require(e.field().same_owner(sample.field) && e.certificate().root_count==1,"selected certificate actual field and exact root count");
    require(e.certificate().margin>0 && e.certificate().contraction_bound<1,"strict root certificate");
    require(e.certificate().taylor_coefficients.size()==sample.field.degree()+1,"complete native Taylor coefficients");
    const auto generator=sample.field.generator(); const auto coordinates=generator.coordinates();
    const auto observation=e.observe(generator); const auto refinement=e.refine(16);
    require(refinement.steps().size()==16 && refinement.region().radius<original.radius,"owned finite refinement strictly shrinks");
    auto region=original;
    for (const auto& step:refinement.steps()) {
        require(step.before==region,"refinement actual chronological source");
        require(norm(difference(step.after.center,step.before.center))+step.after.radius<step.before.radius,"strict nested enclosure");
        require(step.after.radius==e.certificate().contraction_bound*step.before.radius+step.rounding_error,"complete certified rounding error");
        require(step.rounding_error>=0 && 4*step.rounding_error<=step.slack,"declared exact rational grid error bound");
        require(abs(step.after.center.imaginary)>step.after.radius,"nonreal branch preserved throughout refinement");
        region=step.after;
    }
    require(e.certificate().region==original && generator.coordinates()==coordinates,"refinement never changes selected source or field coefficients");
    require(observation.enclosure(refinement)==refinement.region(),"generator observation uses the owned refined region");
    const auto prefix=e.refine(4); require(prefix.region()==refinement.steps()[3].after,"requested prefix contains no later refinement steps");
    require(prefix.steps().size()==4,"refinement prefix cardinality");
    const auto zero=e.observe(sample.field.scalar(Q{0})).enclosure(refinement);
    const auto scalar=e.observe(sample.field.scalar(Q{7,3})).enclosure(refinement);
    require(zero.center==Z{0,0} && zero.radius==0,"retained native zero has an exact zero observation");
    require(scalar.center==Z{Q{7,3},0} && scalar.radius==0,"native rational scalar observation is exact");
    const auto alias=e; require(alias.same_owner(e) && alias.observe(generator).enclosure(prefix)==observation.enclosure(prefix),"copied selected owner can reuse refinement");
    const auto redeclared=Embedding::isolate(sample.field,original.center,original.radius);
    require(!e.same_owner(redeclared) && e.same_selected_root(redeclared),"equal roots do not identify separate embedding declarations");
    rejects<std::invalid_argument>([&]{(void)redeclared.observe(generator).enclosure(prefix);},"equal selected root does not authorize a foreign refinement");
    const auto conjugate=e.conjugated(); require(!e.same_selected_root(conjugate),"distinct nonreal conjugate root");
    const auto back=conjugate.conjugated(); require(e.same_selected_root(back),"explicit conjugate observation returns the original selected root");
    const auto foreign=Field::declare(sample.field.minimal_polynomial(),sample.field.certificate());
    rejects<std::invalid_argument>([&]{(void)e.observe(foreign.scalar(Q{0}));},"foreign supported zero does not bypass field owner");
    const auto foreign_embedding=Embedding::isolate(foreign,original.center,original.radius);
    rejects<std::invalid_argument>([&]{(void)e.same_selected_root(foreign_embedding);},"matching polynomials do not supply field identity");
    const auto owned=[&]{const auto temporary=Embedding::isolate(sample.field,original.center,original.radius);
        return std::pair{temporary.observe(generator),temporary.refine(3)};}();
    require(owned.first.enclosure(owned.second).radius>0,"observation and refinement pin a destroyed wrapper");
    const auto known=sample.field.evaluate(sample.field.minimal_polynomial(),generator);
    require(e.observe(known).is_zero(),"polynomial relation remains exact in native field");
}
static void comparisons(const std::vector<Case>& samples) {
    const auto& q=samples[0]; const auto& quartic=samples[4];
    const auto image=quartic.field.generator().power(2);
    const auto comparison=rfc::numeric::AlgebraicFieldEmbedding::declare(q.field,quartic.field,image);
    require(q.embedding.agrees_with(comparison,quartic.embedding),"rectangular field comparison has checked selected nonreal agreement");
    require(!q.embedding.agrees_with(comparison,quartic.embedding.conjugated()),"complete field map does not identify opposite selected roots");
    rejects<std::invalid_argument>([&]{(void)comparison.inverse();},"rectangular comparison retains no inferred inverse");
    const auto beta=quartic.field.generator();
    const auto conjugation=rfc::numeric::AlgebraicFieldEmbedding::declare(quartic.field,quartic.field,beta.inverse());
    require(quartic.embedding.agrees_with(conjugation,quartic.embedding.conjugated()),"declared generator map can certify physical conjugation agreement");
    const auto involution=rfc::numeric::AlgebraicInvolution::declare(quartic.field,beta.inverse());
    const auto phase=rfc::numeric::AlgebraicUnitPhase(involution,beta);
    require(phase.value()*involution.apply(phase.value())==quartic.field.scalar(1),"norm-one phase remains a separate exact native declaration");
    const auto phase_observation=quartic.embedding.observe(phase.value());
    const auto refinement=quartic.embedding.refine(5);
    require(phase_observation.enclosure(refinement).center.real>0 && phase_observation.enclosure(refinement).center.imaginary>0,"declared phase observed in its selected quadrant");
    rejects<std::invalid_argument>([&]{(void)samples[2].embedding.agrees_with(comparison,quartic.embedding);},"comparison source field owner must match");
    const auto& octic=samples[10];
    const auto extension=rfc::numeric::AlgebraicFieldEmbedding::declare(quartic.field,octic.field,octic.field.generator().power(2));
    const auto composite=comparison.then(extension);
    require(quartic.embedding.agrees_with(extension,octic.embedding) && q.embedding.agrees_with(composite,octic.embedding),
        "complete chronological field comparisons retain selected complex agreement");
    auto no_refinement=rfc::geometry::ComplexRootLimits{};no_refinement.max_refinements=0;
    const auto bounded=Embedding::isolate(q.field,q.embedding.certificate().region.center,q.embedding.certificate().region.radius,no_refinement);
    rejects<std::length_error>([&]{(void)bounded.agrees_with(comparison,quartic.embedding);},"inconclusive complex agreement exhausts the declared budget");
    const auto cubic=samples[2].field; const auto model=cellular_example::make(cubic);
    const auto cellular=model.prepare(rfc::local::AlgebraicChainPolicy::RequireFlat);
    const auto& map=cellular.realization().occurrence_map(rfc::core::Grade{2},1);
    const auto input=map.source().vector({cubic.generator(),cubic.scalar(1)}); const auto output=map.apply(input);
    require(output.coordinates().size()==2 && cellular.realization().flat(),"new observation uses existing exact cellular modules and full chain engine");
    const auto refined=samples[2].embedding.refine(6);
    for (const auto& coefficient:output.coordinates()) {
        const auto enclosure=samples[2].embedding.observe(coefficient).enclosure(refined);
        require(enclosure.radius>=0,"cellular field output retains native coefficients with separate bounded observation");
    }
}
static void rejection() {
    const auto field=Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    for (const auto& radius:{Q{0},Q{-1},Q{1}})
        rejects<std::invalid_argument>([&]{(void)Embedding::isolate(field,{0,1},radius);},"zero, negative or real-axis-touching root region rejection");
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(field,{10,10},Q{1,8});},"region without root fails certificate");
    const auto real=Field::declare({-2,0,1},rfc::numeric::EisensteinShift{2,0});
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(real,{0,1},Q{1,4});},"nonreal observation cannot invent a complex root of a real-root-only field");
    const auto multiple=Field::declare({7,0,6,0,1},rfc::numeric::EisensteinShift{2,1});
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(multiple,{0,Q{7,4}},Q{1});},"a region with two upper roots cannot pass unique-root certificate");
    const auto derivative_zero=Field::declare({2,0,2,0,1},rfc::numeric::EisensteinShift{2,0});
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(derivative_zero,{0,1},Q{1,8});},"zero nonreal center derivative rejection");
    auto limits=rfc::geometry::ComplexRootLimits{};limits.max_refinements=2;
    const auto bounded=Embedding::isolate(field,{0,1},Q{1,4},limits);
    rejects<std::length_error>([&]{(void)bounded.refine(3);},"prospective refinement count admission");
    limits={};limits.max_rounding_bits=0;const auto no_grid=Embedding::isolate(field,{0,1},Q{1,4},limits);
    rejects<std::length_error>([&]{(void)no_grid.refine(1);},"grid certificate rejects missing precision budget");
    limits={};limits.max_trace_coordinates=1;
    rejects<std::length_error>([&]{(void)Embedding::isolate(field,{0,1},Q{1,4},limits);},"complete certificate coordinate budget");
    limits={};limits.max_trace_coordinates=44;const auto small=Embedding::isolate(field,{0,1},Q{1,4},limits);
    rejects<std::length_error>([&]{(void)small.refine(2);},"complete refinement trace admission");
    auto arithmetic=rfc::numeric::AlgebraicFieldLimits{};arithmetic.max_scalar_steps=20;
    const auto limited=Field::declare({1,0,1},rfc::numeric::EisensteinShift{2,1},arithmetic);
    rejects<std::length_error>([&]{(void)Embedding::isolate(limited,{0,1},Q{1,4});},"native exact scalar work admission");
    arithmetic={};arithmetic.max_integer_bits=32;
    const auto bounded_integer=Field::declare({1,0,1},rfc::numeric::EisensteinShift{2,1},arithmetic);
    rfc::numeric::Integer huge{1};huge<<=64;
    rejects<std::length_error>([&]{(void)Embedding::isolate(bounded_integer,{Q{huge},1},Q{1,4});},"coordinate bits checked before complex arithmetic");
    Q malformed{1};mpz_set_ui(mpq_denref(malformed.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(field,{0,1},malformed);},"invalid rational denominator rejection");
    rejects<std::invalid_argument>([&]{(void)Embedding::isolate(field,{malformed,1},Q{1,4});},"invalid rational center rejection");
}
int main() {
    try {
        const auto samples=cases(); for (const auto& sample:samples) profile(sample);
        comparisons(samples); rejection();
        std::cout<<"{\"checks\":"<<checks<<",\"passed\":true}\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
