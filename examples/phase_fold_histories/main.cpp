#include "rfc/core/builder.hpp"
#include "rfc/temporal/fold_history.hpp"
#include <iostream>

using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using M=rfc::numeric::ExactMatrix;
using Z=rfc::numeric::Integer;
using Pose=rfc::geometry::RationalRigidFrame;
using Phase=rfc::numeric::RationalPhase;
namespace temporal=rfc::temporal;
namespace topology=rfc::topology;

static void vector(const V& values) {
    std::cout<<'[';
    for (std::size_t i=0;i<values.size();++i) {if (i) std::cout<<',';std::cout<<'"'<<values[i]<<'"';}
    std::cout<<']';
}
static void matrix(const M& value) {
    std::cout<<'[';
    for (std::size_t i=0;i<value.rows();++i) {
        if (i) std::cout<<',';
        V row;
        for (std::size_t j=0;j<value.cols();++j) row.push_back(value.get(i,j));
        vector(row);
    }
    std::cout<<']';
}
static auto panels() {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("panel_edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("panel",3,3);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (const auto [a,b]:std::vector<std::pair<unsigned,unsigned>>{{10,20},{20,30},{30,10},{10,40},{40,20},{10,50},{50,40}})
        builder.add_grade_one(edge,{{a,true,true,false,1},{b,true,false,false,1}});
    for (const auto& word:std::vector<std::vector<int>>{{1,2,3},{-1,4,5},{-4,6,7}}) {
        std::vector<rfc::core::HigherIncidenceInput> occurrences;
        for (const int letter:word) occurrences.emplace_back(static_cast<std::uint32_t>((letter>0 ? letter : -letter)-1),true,letter<0,false,Q{1});
        builder.add_higher(rfc::core::Grade{2},face,occurrences);
    }
    const auto source=builder.freeze(); std::vector<V> vertices;
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
static void walks() {
    const auto field=rfc::numeric::CertifiedAlgebraicField::declare({1,-1,1},rfc::numeric::IrreducibleReduction{2});
    const auto involution=rfc::numeric::AlgebraicInvolution::declare(field,field.scalar(1)-field.generator());
    const rfc::numeric::AlgebraicUnitPhase phase(involution,field.generator());
    const auto cover=topology::InfiniteCyclicCover::declare({10,20,30,40,50,60},2);
    for (const int turns:{0,1,2,-1}) {
        Z position=0; std::vector<topology::EdgeStep> steps;
        for (int j=0;j<6*(turns<0 ? -turns : turns);++j) {
            const bool forward=turns>=0; const Z edge=forward ? position : Z(position-1);
            steps.push_back({cover.projected_edge(edge),forward}); position+=forward ? 1 : -1;
        }
        const temporal::CyclicPhaseWalk walk(cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(0),steps),0),phase);
        std::cout<<"{\"kind\":\"walk\",\"period\":6,\"branch\":2,\"winding\":\""<<walk.lift().winding()<<"\",\"positions\":[";
        for (std::size_t i=0;i<walk.lift().positions().size();++i) {
            if (i) std::cout<<',';
            std::cout<<'"'<<walk.lift().positions()[i]<<'"';
        }
        std::cout<<"],\"edges\":[";
        for (std::size_t i=0;i<steps.size();++i) {
            if (i) std::cout<<',';
            std::cout<<'['<<steps[i].relation<<','<<(steps[i].forward ? 1 : -1)<<']';
        }
        std::cout<<"],\"phases\":[";
        for (std::size_t i=0;i<=steps.size();++i) {if (i) std::cout<<',';const auto value=walk.phase_at(i);vector(value.value().coordinates());}
        std::cout<<"]}\n";
    }
}
int main() {
    walks(); const auto begin=temporal::RationalFoldHistory::begin(panels());
    for (int i=-3;i<=3;++i) for (int j=-3;j<=3;++j) {
        const auto p=Phase::cayley(Q(i,3)),q=Phase::cayley(Q(j,2));
        const auto old=begin.append({1,0,{1,2},p,true}).append({2,1,{2},q,true}).append({1,0,{1,2},p,false});
        auto edited=old.apply(old.propose(0,p.inverse(),true)); edited=edited.apply(edited.propose(1,q.inverse(),true));
        std::cout<<"{\"kind\":\"fold\",\"i\":"<<i<<",\"j\":"<<j<<",\"p\":";vector({p.real(),p.imaginary()});
        std::cout<<",\"q\":";vector({q.real(),q.imaginary()});
        std::cout<<",\"poses\":[";
        for (std::size_t panel=0;panel<old.poses().size();++panel) {if (panel) std::cout<<',';matrix(old.poses()[panel].affine());}
        std::cout<<"],\"actions\":[";
        const auto actions=old.actions(2);
        for (std::size_t k=0;k<actions.size();++k) {if (k) std::cout<<',';matrix(actions[k]);}
        std::cout<<"],\"edited_pose\":";matrix(edited.poses()[2].affine());
        std::cout<<",\"interactions\":["; const auto terms=old.attribution(edited,2);
        for (std::size_t k=0;k<terms.size();++k) {
            if (k) std::cout<<',';
            std::cout<<"{\"positions\":[";
            for (std::size_t s=0;s<terms[k].changed_positions.size();++s) {if (s) std::cout<<',';std::cout<<terms[k].changed_positions[s];}
            std::cout<<"],\"map\":";matrix(terms[k].contribution);std::cout<<'}';
        }
        std::cout<<"],\"word_defect\":";matrix(old.defect(edited,2).direct);std::cout<<"}\n";
    }
}
