#pragma once
#include "rfc/topology/cellular_attachment.hpp"
#include "rfc/core/builder.hpp"

namespace cellular_example {
using Q=rfc::numeric::Rational;
using Field=rfc::numeric::CertifiedAlgebraicField;
using E=rfc::numeric::AlgebraicElement;
using Map=rfc::local::AlgebraicModuleMap;
using Bundle=rfc::local::AlgebraicStalkBundle;
using G=rfc::core::Grade;
using Prepared=rfc::topology::PreparedCellularAttachment;
struct Model {
    Bundle bundle;
    std::vector<rfc::topology::CellularEdgeConnection> edges;
    std::vector<rfc::topology::CellularFaceChart> faces;
    std::vector<rfc::local::AlgebraicOccurrenceMap> explicit_maps;
    Prepared prepare(rfc::local::AlgebraicChainPolicy policy,rfc::topology::CellularAttachmentLimits limits={}) const {
        return Prepared::declare(bundle,edges,faces,explicit_maps,policy,limits);
    }
};
inline rfc::core::RelationalComplex source(bool tower,Q magnitude,std::size_t repetitions=1,bool reverse=false) {
    if (!repetitions || repetitions>4) throw std::invalid_argument("example loop repetition count must be between one and four");
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("oriented_edge",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    const auto face=signature.add_attaching_loop_type("ordered_face",3,0);
    const auto higher=signature.add_type("higher_attachment",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    rfc::core::RelationalComplexBuilder builder(std::move(signature));
    for (const auto key:{10u,11u,12u,99u}) builder.declare_witness_identity(key);
    for (std::uint32_t i=0;i<3;++i) {
        std::vector<rfc::core::BoundaryOccurrenceInput> ends{{10+i,true,true,false,magnitude},{10+(i+1)%3,true,false,false,magnitude}};
        if (!i) ends.emplace_back(99,false,false,false,Q{7});
        builder.add_grade_one(edge,ends);
    }
    for (std::size_t i=0;i<(tower?2u:1u);++i) {
        std::vector<rfc::core::HigherIncidenceInput> word{{0,false,false,false,Q{7}}};
        for (std::size_t j=0;j<repetitions;++j)
            for (std::uint32_t k=0;k<3;++k) word.emplace_back(reverse?2-k:k,true,reverse,false,Q{1});
        builder.add_higher(G{2},face,word);
    }
    if (tower) {
        for (std::size_t i=0;i<2;++i) builder.add_higher(G{3},higher,{{0,true,false,false,1},{1,true,true,false,1}});
        builder.add_higher(G{4},higher,{{0,true,false,false,1},{1,true,true,false,1}});
    }
    return builder.freeze();
}
inline Model make(const Field& field,bool tower=true,bool curved=false,bool partial=false,Q magnitude=Q{1},bool break_top=false,
                  std::size_t repetitions=1,bool reverse=false) {
    if (partial && tower) throw std::invalid_argument("partial chart example uses its own grade-two source");
    const auto original=source(tower,magnitude,repetitions,reverse);
    std::vector<std::vector<std::size_t>> ranks;
    for (std::size_t k=0;k<original.grade_count();++k)
        ranks.emplace_back(original.grade(G{static_cast<std::uint16_t>(k)}).cell_count(),k==2 && partial?1:2);
    const auto bundle=Bundle::declare(original,field,std::move(ranks));
    const auto one=field.scalar(Q{1}),zero=field.scalar(Q{0}),two=field.scalar(Q{2}),a=one+field.generator();
    const std::vector<E> identity{one,zero,zero,one},e0{one,one,zero,one},e1{two,zero,one,one},e2{one,zero,one,one};
    const auto v0=bundle.stalk({G{0},0}),v1=bundle.stalk({G{0},1}),v2=bundle.stalk({G{0},2});
    const auto edge0=bundle.stalk({G{1},0}),edge1=bundle.stalk({G{1},1}),edge2=bundle.stalk({G{1},2});
    const auto A=Map::declare(v0,v1,{one,a,zero,one}),B=Map::declare(v1,v2,{one,zero,a,one});
    const auto AB=A.then(B);
    const auto D=Map::declare(v0,v0,curved?std::vector<E>{one,zero,zero,two}:identity);
    const auto C=AB.inverse().then(D);
    std::vector<rfc::topology::CellularEdgeConnection> edges{
        {{G{1},0},Map::declare(edge0,v0,e0),A},
        {{G{1},1},Map::declare(edge1,v0,e1).then(A),B},
        {{G{1},2},Map::declare(edge2,v0,e2).then(AB),C}};
    std::vector<rfc::topology::CellularFaceChart> faces;
    const auto face0=bundle.stalk({G{2},0});
    faces.push_back({{G{2},0},Map::declare(face0,v0,partial?std::vector<E>{one,zero}:identity)});
    if (tower) faces.push_back({{G{2},1},Map::declare(bundle.stalk({G{2},1}),v0,{one,a,zero,one})});
    std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
    for (std::size_t k=1;k<original.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)}; const auto& arena=original.grade(grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            if (arena.aspects.get(original.signature().existence_aspect(),slot) || k>=3) continue;
            const auto upper=bundle.stalk({grade,arena.upper_index[slot]});
            const auto lower=bundle.stalk({G{static_cast<std::uint16_t>(k-1)},arena.lower_index[slot]});
            std::vector<E> coefficients(upper.rank()*lower.rank(),one);
            maps.push_back({grade,slot,Map::declare(upper,lower,std::move(coefficients))});
        }
    }
    if (tower) {
        const auto face1=bundle.stalk({G{2},1}),volume0=bundle.stalk({G{3},0}),volume1=bundle.stalk({G{3},1});
        const auto Z=Map::declare(face1,face0,{one,a,zero,one});
        const auto W=Map::declare(volume1,volume0,{one,zero,a,one});
        const auto V=Map::declare(volume0,face0,identity);
        const std::vector<Map> higher{V,V.then(Z.inverse()),W.then(V),W.then(V).then(Z.inverse())};
        for (std::size_t slot=0;slot<higher.size();++slot) maps.push_back({G{3},slot,higher[slot]});
        const auto top=bundle.stalk({G{4},0}); const auto T=Map::declare(top,volume0,identity);
        maps.push_back({G{4},0,T});
        maps.push_back({G{4},1,break_top?Map::declare(top,volume1,identity):T.then(W.inverse())});
    }
    return {bundle,std::move(edges),std::move(faces),std::move(maps)};
}
} // namespace cellular_example
