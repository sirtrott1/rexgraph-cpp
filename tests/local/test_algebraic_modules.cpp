#include "rfc/geometry/real_algebraic_embedding.hpp"
#include "rfc/local/algebraic_fiber_system.hpp"
#include "rfc/algebra/polygon.hpp"
#include <iostream>

using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using Field=rfc::numeric::CertifiedAlgebraicField;
using Element=rfc::numeric::AlgebraicElement;
using Root=rfc::geometry::SelectedRealAlgebraicEmbedding;
using Embedding=rfc::numeric::AlgebraicFieldEmbedding;
using Bundle=rfc::local::AlgebraicStalkBundle;
using Map=rfc::local::AlgebraicModuleMap;
using Fibers=rfc::local::AlgebraicRelationalFiberSystem;
using G=rfc::core::Grade;
using Policy=rfc::local::AlgebraicChainPolicy;
static std::size_t checks{};
static void require(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
template<class Error,class F> static void rejects(F action,const char* message) {
    bool caught=false; try {action();} catch (const Error&) {caught=true;}
    require(caught,message);
}
template<class T> concept FloatIsolation=requires(const Field& field,T value) { Root::isolate(field,value,value); };
template<class T> concept TemporaryCoordinates=requires(T value) { std::move(value).coordinates(); };
static_assert(!FloatIsolation<double>);
static_assert(!TemporaryCoordinates<rfc::local::AlgebraicModuleVector>);
static_assert(!std::default_initializable<rfc::local::AlgebraicModule>);

static void roots() {
    const auto cubic=Field::declare({1,-3,0,1},rfc::numeric::IrreducibleReduction{2});
    const auto alpha=cubic.generator();
    require(Root::root_count(cubic,-3,3)==3,"three-root exact Sturm count");
    require(Root::root_count(cubic,0,2)==2,"same endpoint signs do not imply root isolation");
    std::vector<Root> roots{Root::isolate(cubic,-2,-1),Root::isolate(cubic,0,1),Root::isolate(cubic,1,2)};
    const std::vector<int> signs{-1,1,1};
    for (std::size_t i=0;i<roots.size();++i) {
        require(roots[i].sign(alpha)==signs[i],"selected cubic generator sign");
        require(roots[i].sign(cubic.scalar(Q{0}))==0,"exact zero sign");
        require(roots[i].sign(cubic.evaluate(cubic.minimal_polynomial(),alpha))==0,"exact polynomial zero");
        const auto& certificate=roots[i].certificate();
        require(certificate.lower_variations-certificate.upper_variations==1,"retained complete isolation certificate");
        const auto refined=roots[i].refined_interval(40);
        require(refined.lower>=certificate.interval.lower && refined.upper<=certificate.interval.upper,"refinement stays in selected interval");
        require(refined.upper-refined.lower==Q(1,rfc::numeric::Integer{1}<<40),"exact dyadic width");
        require(roots[i].certificate().interval.upper-refined.lower>0,"refinement leaves immutable source certificate");
        const auto identity=Embedding::identity(cubic);
        for (std::size_t j=0;j<roots.size();++j)
            require(roots[i].agrees_with(identity,roots[j])==(i==j),"same field does not identify different selected roots");
        for (long k=-6;k<=6;++k) {
            const auto p=cubic.element({Q{k},Q{1},Q{k%3}});
            require(roots[i].sign(-p)==-roots[i].sign(p),"exact real sign reversal");
            if (!p.is_zero()) {
                require(roots[i].sign(p*p)==1,"positive real square");
                require(roots[i].sign(p.inverse())==roots[i].sign(p),"inverse preserves selected sign");
            }
        }
    }
    const auto quartic=Field::declare({-2,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
    require(Root::root_count(quartic,-3,3)==2,"quartic separates real and nonreal roots");
    require(Root::isolate(quartic,-2,-1).sign(quartic.generator())==-1,"negative quartic selection");
    const auto no_real=Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    require(Root::root_count(no_real,-10,10)==0,"nonreal roots are not projected into real coordinates");
    const auto linear=Field::declare({Q{-3,2},1},rfc::numeric::IrreducibleReduction{5});
    const auto rational_root=Root::isolate(linear,1,2); const auto point=rational_root.refined_interval(1);
    require(point.lower==Q{3,2} && point.upper==point.lower,"rational midpoint root terminates refinement");
    require(rational_root.sign(linear.generator()-linear.scalar(Q{3,2}))==0,"degree-one exact root zero");
    rejects<std::invalid_argument>([&]{(void)Root::isolate(cubic,-3,3);},"multiple roots reject selection");
    rejects<std::invalid_argument>([&]{(void)Root::isolate(no_real,-3,3);},"empty real-root interval rejects selection");
    rejects<std::invalid_argument>([&]{(void)Root::isolate(linear,Q{3,2},2);},"endpoint root rejection");
    rejects<std::invalid_argument>([&]{(void)Root::isolate(cubic,2,1);},"unordered interval rejection");
    const auto other=Field::declare({1,-3,0,1},rfc::numeric::IrreducibleReduction{2});
    rejects<std::invalid_argument>([&]{(void)roots[0].sign(other.generator());},"foreign exact field owner rejection");
    const auto cube2=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto bounded=Root::isolate(cube2,1,2,rfc::geometry::RealRootLimits{0});
    rejects<std::length_error>([&]{(void)bounded.sign(cube2.generator()-cube2.scalar(Q{5,4}));},"sign exhaustion is not zero");
    rejects<std::length_error>([&]{(void)bounded.refined_interval(1);},"bisection admission bound");
    rejects<std::invalid_argument>([]{(void)Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{2});},"failed sufficient certificate is refused");
    require(Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3}).degree()==2,"certificate failure does not prove rational reducibility");
    Q invalid{1}; mpz_set_ui(mpq_denref(invalid.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{(void)Root::isolate(cubic,invalid,1);},"invalid interval rational rejected before arithmetic");
}
static void embeddings() {
    const auto source=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto target=Field::declare({-2,0,0,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto image=target.generator().power(2);
    const auto embedding=Embedding::declare(source,target,image);
    require(embedding.rational_action().rows()==6 && embedding.rational_action().cols()==3,"rectangular field comparison stays rectangular");
    require(embedding.rational_action().rank()==3,"field comparison is faithful");
    for (long a=-2;a<=2;++a) for (long b=-2;b<=2;++b) {
        const auto x=source.element({Q{a},Q{b},Q{1}}),y=source.element({Q{b},Q{a},Q{-1}});
        require(embedding.apply(x+y)==embedding.apply(x)+embedding.apply(y),"field embedding addition");
        require(embedding.apply(x*y)==embedding.apply(x)*embedding.apply(y),"field embedding multiplication");
        const auto output=embedding.apply(x);
        require(output.coordinates()==embedding.rational_action().apply(x.coordinates()),"full rectangular action");
        if (!x.is_zero()) require(embedding.apply(x.inverse())==embedding.apply(x).inverse(),"embedding exact inverse compatibility");
    }
    const auto left=Root::isolate(source,1,2),right=Root::isolate(target,1,2);
    require(left.agrees_with(embedding,right),"declared higher-degree selected-root comparison");
    rejects<std::invalid_argument>([&]{(void)embedding.inverse();},"rectangular embedding refuses inverse");
    rejects<std::invalid_argument>([&]{(void)Embedding::declare(source,target,target.generator());},"generator image must satisfy source relation");
    rejects<std::invalid_argument>([&]{(void)embedding.apply(target.generator());},"embedding source owner binding");
    const auto another=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto comparison=Embedding::declare(source,another,another.generator()); const auto back=comparison.inverse();
    require(comparison.then(back).apply(source.generator())==source.generator(),"explicit isomorphism inverse");
    const auto roundtrip=comparison.then(back);
    require(roundtrip.rational_action()==rfc::numeric::ExactMatrix::identity(3),"complete isomorphism action");
    rejects<std::invalid_argument>([&]{(void)comparison.then(Embedding::identity(source));},"equal polynomials do not identify intermediate field owners");
    rejects<std::invalid_argument>([&]{(void)left.agrees_with(comparison,right);},"selected comparison owner mismatch");
}
static rfc::core::RelationalComplex tower_source() {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("edge",rfc::core::BoundaryPolicy::DeclaredExact,2,4);
    const auto face=signature.add_type("face",rfc::core::BoundaryPolicy::DeclaredExact,3,5);
    const auto volume=signature.add_type("volume",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    rfc::core::RelationalComplexBuilder builder(std::move(signature));
    for (std::uint32_t i=0;i<3;++i) {
        std::vector<rfc::core::BoundaryOccurrenceInput> occurrences{{10+i,true,true,false,1},{10+(i+1)%3,true,false,false,1}};
        if (!i) {occurrences.emplace_back(10,false,false,false,Q{7}); occurrences.emplace_back(10,true,false,false,Q{0});}
        builder.add_grade_one(edge,occurrences);
    }
    for (std::size_t i=0;i<2;++i) builder.add_higher(G{2},face,{{0,true,false,false,1},{1,true,false,false,1},{2,true,false,false,1}});
    builder.add_higher(G{3},volume,{{0,true,false,false,1},{1,true,true,false,1}});
    return builder.freeze();
}
static std::vector<rfc::local::AlgebraicOccurrenceMap> all_maps(const Bundle& bundle) {
    std::vector<rfc::local::AlgebraicOccurrenceMap> result; const auto& source=bundle.source();
    for (std::size_t k=1;k<source.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)}; const auto& arena=source.grade(grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            const auto upper=bundle.stalk({grade,arena.upper_index[slot]});
            const auto lower=bundle.stalk({G{static_cast<std::uint16_t>(k-1)},arena.lower_index[slot]});
            std::vector<Element> block(upper.rank()*lower.rank(),bundle.field().scalar(Q{0}));
            for (std::size_t i=0;i<std::min(upper.rank(),lower.rank());++i) block[i*upper.rank()+i]=bundle.field().scalar(Q{1});
            auto bridge=Map::declare(upper,lower,std::move(block));
            if (upper.rank()==2 && lower.rank()==2) {
                const auto one=bundle.field().scalar(Q{1}),zero=bundle.field().scalar(Q{0});
                const auto upper_shear=bundle.field().generator()*bundle.field().scalar(Q{static_cast<long>(k+arena.upper_index[slot]+1)});
                const auto lower_shear=bundle.field().generator()*bundle.field().scalar(Q{static_cast<long>(k+arena.lower_index[slot])});
                const auto u=Map::declare(upper,upper,{one,upper_shear,zero,one});
                const auto l=Map::declare(lower,lower,{one,lower_shear,zero,one});
                bridge=u.inverse().then(bridge).then(l);
            }
            result.push_back({grade,slot,std::move(bridge)});
        }
    }
    return result;
}
static void modules() {
    const auto field=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0}); const auto alpha=field.generator();
    const auto source=rfc::algebra::oriented_polygon({10,20,30},rfc::algebra::PolygonFilling::Disk);
    const auto bundle=Bundle::declare(source,field,{{3,2,2},{2,2,2},{1}});
    const auto domain=bundle.stalk({G{0},0}),codomain=bundle.stalk({G{0},1});
    const auto zero=field.scalar(Q{0}),one=field.scalar(Q{1});
    const auto map=Map::declare(domain,codomain,{alpha,one,zero,zero,alpha,one});
    require(map.rank_over_field()==2 && map.nullity_over_field()==1,"rectangular K rank and nullity");
    require(map.rational_action().rank()==6,"rational rank is degree times K rank");
    for (long a=-2;a<=2;++a) for (long b=-2;b<=2;++b) for (long c=-2;c<=2;++c) {
        const auto value=domain.vector({field.element({Q{a},Q{b}}),field.element({Q{b},Q{c}}),field.element({Q{c},Q{a}})});
        require(map.apply(value).rational_coordinates()==map.rational_action().apply(value.rational_coordinates()),"module/native full action");
    }
    const auto a=Map::declare(codomain,codomain,{alpha,one,one,zero}),b=Map::declare(codomain,codomain,{one,alpha,zero,one});
    const auto ab=a.then(b),ba=b.then(a),roundtrip=a.then(a.inverse()),identity=Map::identity(codomain);
    require(ab.rational_action()==b.rational_action().multiply(a.rational_action()),"chronological complete module composition");
    require(ab.rational_action()!=ba.rational_action(),"noncommuting module order preserved");
    require(roundtrip.rational_action()==identity.rational_action(),"K inverse uses full native rational solve");
    const auto singular=Map::declare(codomain,codomain,{alpha,one,alpha*alpha,alpha});
    require(singular.rank_over_field()==1 && singular.nullity_over_field()==1,"K singular rank agrees with restriction");
    rejects<std::runtime_error>([&]{(void)singular.inverse();},"singular square map refuses inverse");
    const auto input=codomain.vector({alpha,one});
    const auto restored=a.inverse().apply(a.apply(input));
    require(restored.coordinates()==input.coordinates(),"owned module inverse output");
    rejects<std::invalid_argument>([&]{(void)map.inverse();},"rectangular map inverse refusal");
    rejects<std::invalid_argument>([&]{(void)map.apply(codomain.vector({zero,zero}));},"zero input still requires correct carrier");
    rejects<std::invalid_argument>([&]{(void)Map::declare(domain,codomain,{one});},"incomplete block refusal");
    const auto duplicate=Bundle::declare(source,field,{{3,2,2},{2,2,2},{1}});
    rejects<std::invalid_argument>([&]{(void)a.then(Map::identity(duplicate.stalk({G{0},1})));},"equal source and dimensions do not identify module owners");
    const auto foreign=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    rejects<std::invalid_argument>([&]{(void)codomain.vector({foreign.scalar(Q{0}),one});},"foreign supported zero rejected");
    rejects<std::invalid_argument>([&]{(void)Map::declare(codomain,codomain,{one,one,one,foreign.scalar(Q{0})});},"every block coefficient owner validated");
    rejects<std::invalid_argument>([&]{(void)Bundle::declare(source,field,{{1},{1},{1}});},"complete rank declarations required");
    rejects<std::invalid_argument>([&]{(void)Bundle::declare(source,field,{{0,1,1},{1,1,1},{1}});},"zero rank refusal");
    auto limits=rfc::local::AlgebraicModuleLimits{}; limits.max_matrix_coordinates=1;
    const auto small=Bundle::declare(source,field,{{1,1,1},{1,1,1},{1}},limits);
    const auto module=small.stalk({G{0},0});
    rejects<std::length_error>([&]{(void)Map::identity(module);},"full rational block budget");
    limits={}; limits.max_retained_coordinates=1;
    rejects<std::length_error>([&]{(void)Bundle::declare(source,field,{{1,1,1},{1,1,1},{1}},limits);},"retained module coordinate admission");
    const auto retained=[&] {const auto ephemeral=Bundle::declare(source,field,{{1,1,1},{1,1,1},{1}}); return ephemeral.stalk({G{0},0});}();
    const auto retained_value=retained.vector({alpha});
    require(retained_value.coordinates()[0]==alpha,"module pins source after original bundle destruction");
}
static void fibers() {
    const auto source=tower_source(); const auto field=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto bundle=Bundle::declare(source,field,{{2,2,2},{2,2,2},{2,2},{2}}); auto maps=all_maps(bundle);
    const auto fibers=Fibers::declare(bundle,maps,Policy::RequireFlat);
    require(fibers.flat(),"all adjacent K chain laws checked");
    require(fibers.homology_dimensions_over_field()==std::vector<std::size_t>({2,0,0,0}),"higher tower homology over K");
    const auto grade=G{1}; const auto prepared=fibers.prepared_action(grade);
    require(prepared.statistics().retained_occurrences==8 && prepared.statistics().active_occurrences==7,"inactive and supported zero legs remain retained");
    require(fibers.occurrence_map(grade,2).coefficients()[0]==field.scalar(Q{1}),"inactive underlying map remains complete");
    std::vector<rfc::local::AlgebraicModuleVector> values;
    for (std::uint32_t i=0;i<3;++i) values.push_back(bundle.stalk({grade,i}).vector({field.generator()+field.scalar(Q{static_cast<long>(i)}),field.scalar(Q{1})}));
    V flat; for (const auto& value:values) { const auto q=value.rational_coordinates(); flat.insert(flat.end(),q.begin(),q.end()); }
    V result; for (const auto& value:fibers.apply(grade,values)) { const auto q=value.rational_coordinates(); result.insert(result.end(),q.begin(),q.end()); }
    auto scratch=prepared.make_workspace();
    require(result==prepared.apply(flat,scratch) && result==fibers.rational_boundary(grade).apply(flat),"K/native/sparse prepared field equality");
    require(fibers.rational_chain_defect(G{2}).entries().empty() && fibers.rational_chain_defect(G{3}).entries().empty(),"complete grade-two and grade-three defects");
    auto missing=maps; missing.pop_back();
    rejects<std::invalid_argument>([&]{(void)Fibers::declare(bundle,missing,Policy::RequireFlat);},"missing map including inactive legs refused");
    auto repeated=maps; repeated[1]=repeated[0];
    rejects<std::invalid_argument>([&]{(void)Fibers::declare(bundle,repeated,Policy::RequireFlat);},"duplicate occurrence refusal");
    const auto another=Bundle::declare(source,field,{{2,2,2},{2,2,2},{2,2},{2}});
    auto foreign=maps; const auto foreign_maps=all_maps(another); foreign[2]=foreign_maps[2];
    rejects<std::invalid_argument>([&]{(void)Fibers::declare(bundle,foreign,Policy::RequireFlat);},"inactive map still requires actual carrier owners");
    for (auto& map:maps) if (map.grade==G{2} && map.underlying.source().cell().index==1) {
        auto coefficients=map.underlying.coefficients(); for (auto& q:coefficients) q=q*field.scalar(Q{2});
        map.underlying=Map::declare(map.underlying.source(),map.underlying.target(),std::move(coefficients));
    }
    rejects<std::invalid_argument>([&]{(void)Fibers::declare(bundle,maps,Policy::RequireFlat);},"grade-three defect cannot hide behind flat grade two");
    const auto deformed=Fibers::declare(bundle,maps,Policy::DeclaredDeformation);
    require(!deformed.flat() && deformed.rational_chain_defect(G{2}).entries().empty() &&
        !deformed.rational_chain_defect(G{3}).entries().empty(),"explicit higher deformed realization with retained source");
    rejects<std::logic_error>([&]{(void)deformed.homology_dimensions_over_field();},"nonflat realization has no chain homology");
    auto limits=rfc::local::AlgebraicFiberLimits{}; limits.max_retained_map_coordinates=1;
    rejects<std::length_error>([&]{(void)Fibers::declare(bundle,maps,Policy::DeclaredDeformation,limits);},"complete native boundary and defect admission");
    rejects<std::out_of_range>([&]{(void)fibers.rational_boundary(G{0});},"grade-zero boundary rejection");
    const auto detached=[&] {const auto temporary=Fibers::declare(bundle,all_maps(bundle),Policy::RequireFlat); return temporary.prepared_action(G{1});}();
    auto workspace=detached.make_workspace(); require(detached.apply(flat,workspace)==result,"prepared action survives fiber wrapper destruction");
    const auto triangle=rfc::algebra::oriented_polygon({10,20,30},rfc::algebra::PolygonFilling::Disk);
    const auto rectangular=Bundle::declare(triangle,field,{{2,2,2},{1,1,1},{1}});
    auto rectangular_maps=all_maps(rectangular);
    for (auto& map:rectangular_maps) if (map.grade==G{1})
        map.underlying=Map::declare(map.underlying.source(),map.underlying.target(),{field.scalar(Q{1}),field.generator()});
    const auto rectangular_fibers=Fibers::declare(rectangular,rectangular_maps,Policy::RequireFlat);
    require(rectangular_fibers.homology_dimensions_over_field()==std::vector<std::size_t>({4,0,0}),"rectangular algebraic stalks preserve exact chain and K dimensions");
}
int main() {
    try { roots(); embeddings(); modules(); fibers(); std::cout<<"{\"checks\":"<<checks<<",\"passed\":true}\n"; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
