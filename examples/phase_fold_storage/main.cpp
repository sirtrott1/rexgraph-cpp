#include "model.hpp"
#include <iostream>
using namespace phase_fold_example;
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
        const auto retained=Archive::declare(temporal::CyclicPhaseWalk(cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(0),steps),0),phase));
        const auto restored=rfc::storage::decode_phase_fold_archive(rfc::storage::encode_phase_fold_archive(retained));
        const auto& walk=std::get<rfc::storage::AlgebraicPhaseWalk>(restored.word());
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
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--export") {
        const std::filesystem::path directory(argv[2]);std::filesystem::create_directories(directory);
        for (const auto& [name,archive]:samples()) rfc::storage::write_phase_fold_archive(archive,directory/(name+".rfcxw"),rfc::storage::FilePublication::CreateNew);
    } else if (argc!=1) {std::cerr<<"usage: phase_fold_storage [--export directory]\n";return 1;}
    walks(); const auto begin=temporal::RationalFoldHistory::begin(panels());
    for (int i=-3;i<=3;++i) for (int j=-3;j<=3;++j) {
        const auto p=Phase::cayley(Q(i,3)),q=Phase::cayley(Q(j,2));
        const auto original=begin.append({1,0,{1,2},p,true}).append({2,1,{2},q,true}).append({1,0,{1,2},p,false});
        const auto pending=Archive::declare(original).propose(1,0,p.inverse(),true);
        const auto restored=rfc::storage::decode_phase_fold_archive(rfc::storage::encode_phase_fold_archive(pending));
        const auto& old=std::get<temporal::RationalFoldHistory>(restored.word());
        const auto changed=restored.apply(1).propose(2,1,q.inverse(),true).apply(2);
        const auto& edited=std::get<temporal::RationalFoldHistory>(changed.word());
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
