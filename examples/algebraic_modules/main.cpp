#include "rfc/geometry/real_algebraic_embedding.hpp"
#include "rfc/local/algebraic_fiber_system.hpp"
#include "rfc/algebra/polygon.hpp"
#include <iostream>

using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using Field=rfc::numeric::CertifiedAlgebraicField;
using Root=rfc::geometry::SelectedRealAlgebraicEmbedding;
using Bundle=rfc::local::AlgebraicStalkBundle;
using Map=rfc::local::AlgebraicModuleMap;
using G=rfc::core::Grade;
static void vector(const V& values) {
    std::cout<<'[';
    for (std::size_t i=0;i<values.size();++i) {if (i) std::cout<<','; std::cout<<'"'<<values[i].get_str()<<'"';}
    std::cout<<']';
}
static void matrix(const rfc::numeric::ExactMatrix& value) {
    std::cout<<'[';
    for (std::size_t i=0;i<value.rows();++i) {
        if (i) std::cout<<',';
        V row(value.cols()); for (std::size_t j=0;j<row.size();++j) row[j]=value.get(i,j);
        vector(row);
    }
    std::cout<<']';
}
static void root(const Field& field,Q lower,Q upper) {
    const auto selected=Root::isolate(field,lower,upper); const auto refined=selected.refined_interval(20);
    const auto& proof=selected.certificate();
    std::cout<<"{\"kind\":\"root\",\"polynomial\":"; vector(field.minimal_polynomial());
    std::cout<<",\"interval\":"; vector({lower,upper});
    std::cout<<",\"refined\":"; vector({refined.lower,refined.upper});
    std::cout<<",\"variations\":["<<proof.lower_variations<<','<<proof.upper_variations<<"],\"sturm\":[";
    for (std::size_t i=0;i<proof.sturm_sequence.size();++i) {if (i) std::cout<<','; vector(proof.sturm_sequence[i]);}
    std::cout<<"],\"signs\":[";
    const std::vector<V> probes{{0},{0,1},{Q{-5,4},1},{-2,0,1},{1,-1,1},{1,-3,0,1}};
    for (std::size_t i=0;i<probes.size();++i) {
        if (i) std::cout<<',';
        const auto value=field.element(probes[i]);
        std::cout<<"{\"coordinates\":"; vector(value.coordinates()); std::cout<<",\"sign\":"<<selected.sign(value)<<'}';
    }
    std::cout<<"]}\n";
}
int main() {
    try {
        const auto cubic=Field::declare({1,-3,0,1},rfc::numeric::IrreducibleReduction{2});
        root(cubic,-2,-1); root(cubic,0,1); root(cubic,1,2);
        const auto quartic=Field::declare({-2,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
        root(quartic,-2,-1); root(quartic,1,2);
        const auto linear=Field::declare({Q{-3,2},1},rfc::numeric::IrreducibleReduction{5}); root(linear,1,2);
        const auto field=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
        const auto sextic=Field::declare({-2,0,0,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
        const auto embedding=rfc::numeric::AlgebraicFieldEmbedding::declare(field,sextic,sextic.generator().power(2));
        std::cout<<"{\"kind\":\"field_embedding\",\"source_polynomial\":"; vector(field.minimal_polynomial());
        std::cout<<",\"target_polynomial\":"; vector(sextic.minimal_polynomial());
        std::cout<<",\"generator_image\":"; vector(embedding.generator_image().coordinates());
        std::cout<<",\"action\":"; matrix(embedding.rational_action()); std::cout<<"}\n";
        const auto source=rfc::algebra::oriented_polygon({10,20,30},rfc::algebra::PolygonFilling::Disk);
        const auto bundle=Bundle::declare(source,field,{{3,2,2},{2,2,2},{1}});
        const auto domain=bundle.stalk({G{0},0}),codomain=bundle.stalk({G{0},1});
        const auto alpha=field.generator(),one=field.scalar(Q{1}),zero=field.scalar(Q{0});
        const auto map=Map::declare(domain,codomain,{alpha,one,zero,zero,alpha,one});
        for (long a=-2;a<=2;++a) for (long b=-2;b<=2;++b) {
            const auto input=domain.vector({field.element({Q{a},Q{b}}),field.element({Q{b},Q{a}}),alpha});
            std::cout<<"{\"kind\":\"module\",\"polynomial\":"; vector(field.minimal_polynomial());
            std::cout<<",\"source_rank\":3,\"target_rank\":2,\"coefficients\":[";
            for (std::size_t i=0;i<map.coefficients().size();++i) {if (i) std::cout<<','; vector(map.coefficients()[i].coordinates());}
            std::cout<<"],\"action\":"; matrix(map.rational_action());
            std::cout<<",\"input\":"; vector(input.rational_coordinates());
            std::cout<<",\"output\":"; vector(map.apply(input).rational_coordinates());
            std::cout<<",\"rank_over_field\":"<<map.rank_over_field()<<",\"nullity_over_field\":"<<map.nullity_over_field()<<"}\n";
        }
        const auto stalks=Bundle::declare(source,field,{{2,2,2},{1,1,1},{1}});
        std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto grade=G{static_cast<std::uint16_t>(k)}; const auto& arena=source.grade(grade);
            for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
                const auto upper=stalks.stalk({grade,arena.upper_index[slot]});
                const auto lower=stalks.stalk({G{static_cast<std::uint16_t>(k-1)},arena.lower_index[slot]});
                maps.push_back({grade,slot,Map::declare(upper,lower,k==1?std::vector<rfc::numeric::AlgebraicElement>{one,alpha}:
                    std::vector<rfc::numeric::AlgebraicElement>{one})});
            }
        }
        const auto fibers=rfc::local::AlgebraicRelationalFiberSystem::declare(stalks,maps,rfc::local::AlgebraicChainPolicy::RequireFlat);
        std::cout<<"{\"kind\":\"fibers\",\"polynomial\":"; vector(field.minimal_polynomial());
        std::cout<<",\"ranks\":[6,3,1],\"boundary_one\":"; matrix(fibers.rational_boundary(G{1}));
        std::cout<<",\"boundary_two\":"; matrix(fibers.rational_boundary(G{2}));
        std::cout<<",\"chain_defect\":"; matrix(fibers.rational_chain_defect(G{2}));
        std::cout<<",\"homology_over_field\":[";
        const auto homology=fibers.homology_dimensions_over_field();
        for (std::size_t i=0;i<homology.size();++i) {if (i) std::cout<<','; std::cout<<homology[i];}
        std::cout<<"]}\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
