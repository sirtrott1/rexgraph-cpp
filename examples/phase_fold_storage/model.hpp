#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/storage/phase_fold_codec.hpp"

namespace phase_fold_example {
using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using M=rfc::numeric::ExactMatrix;
using Z=rfc::numeric::Integer;
using Pose=rfc::geometry::RationalRigidFrame;
using Phase=rfc::numeric::RationalPhase;
using Archive=rfc::storage::PhaseFoldArchive;
namespace temporal=rfc::temporal;
namespace topology=rfc::topology;

inline auto panels() {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("panel_edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("panel",3,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (const auto [a,b]:std::vector<std::pair<unsigned,unsigned>>{{10,20},{20,30},{30,10},{10,40},{40,20},{10,50},{50,40}})
        builder.add_grade_one(edge,{{a,true,true,false,1},{b,true,false,false,1}});
    for (const auto& word:std::vector<std::vector<int>>{{1,2,3},{-1,4,5},{-4,6,7}}) {
        std::vector<rfc::core::HigherIncidenceInput> occurrences;
        for (const int letter:word) occurrences.emplace_back(static_cast<std::uint32_t>((letter>0 ? letter : -letter)-1),true,letter<0,false,Q{1});
        occurrences.emplace_back(0,false,false,false,Q{7});builder.add_higher(rfc::core::Grade{2},face,occurrences);
    }
    const auto source=builder.freeze();std::vector<V> vertices;
    for (const auto key:source.grade(rfc::core::Grade{0}).identities) {
        if (key==10) vertices.push_back({0,0,0});
        if (key==20) vertices.push_back({1,0,0});
        if (key==30) vertices.push_back({0,1,0});
        if (key==40) vertices.push_back({0,-1,0});
        if (key==50) vertices.push_back({-1,0,0});
    }
    const Pose second(M::from_rows({{0,1,0},{-1,0,0},{0,0,1}}),{0,0,0});
    return rfc::geometry::RationalPanelAssembly::declare(source,std::move(vertices),std::vector<Pose>(3,Pose::identity()),
        {{0,0,1,Pose::identity()},{3,1,2,second}});
}
inline auto root() {
    const auto field=rfc::numeric::CertifiedAlgebraicField::declare({1,-1,1},rfc::numeric::IrreducibleReduction{2});
    const auto involution=rfc::numeric::AlgebraicInvolution::declare(field,field.scalar(1)-field.generator());
    return rfc::numeric::AlgebraicUnitPhase(involution,field.generator());
}
inline auto fold(Phase p=Phase::cayley(Q{1,2}),Phase q=Phase::cayley(Q{2,3})) {
    return temporal::RationalFoldHistory::begin(panels()).append({1,0,{2,1},p,true})
        .append({2,1,{2},q,true}).append({1,0,{1,2},p,false});
}
template<temporal::ExactPhase P> inline auto walk(std::vector<rfc::core::IdentityKey> vertices,std::size_t branch,
    P generator,int turns,Z initial=0,bool backtrack=true) {
    const auto cover=topology::InfiniteCyclicCover::declare(vertices,branch);Z position=initial;std::vector<topology::EdgeStep> steps;
    auto add=[&](bool forward) {const Z edge=forward ? position : Z(position-1);steps.push_back({cover.projected_edge(edge),forward});position+=forward ? 1 : -1;};
    if (backtrack) {add(true);add(false);}
    for (std::size_t j=0;j<vertices.size()*static_cast<std::size_t>(turns<0 ? -turns : turns);++j) add(turns>=0);
    return temporal::CyclicPhaseWalk<P>(cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(initial),std::move(steps)),initial),std::move(generator));
}
inline std::vector<std::pair<std::string,Archive>> samples() {
    const auto assembly=panels();const auto p=Phase::cayley(Q{1,2});const auto a=root();
    const auto rational=temporal::RationalPhaseHistory::begin(assembly.source(),{rfc::core::Grade{2},1},p)
        .append({91,p,true}).append({91,p.inverse(),true}).append({0,p,false});
    const auto algebraic=temporal::AlgebraicPhaseHistory::begin(assembly.source(),{rfc::core::Grade{1},0},a.power(0))
        .append({91,a,true}).append({91,a.inverse(),true}).append({0,a,false});
    return {{"rational",Archive::declare(rational).propose(7,0,p.inverse(),true)},
        {"algebraic",Archive::declare(algebraic).propose(7,1,a,true)},
        {"fold",Archive::declare(fold()).propose(7,0,p.inverse(),true)},
        {"rational-walk",Archive::declare(walk({30,10,40,20},2,Phase(0,1),2,Z("-1844674407370955161600")))},
        {"algebraic-walk",Archive::declare(walk({60,10,50,20,40,30},2,a,-2))}};
}
} // namespace phase_fold_example
