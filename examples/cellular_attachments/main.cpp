#include "model.hpp"
#include <iostream>

using namespace cellular_example;
using Matrix=rfc::numeric::ExactMatrix;
static void matrix(const Matrix& value) {
    std::cout<<'[';
    for (std::size_t i=0;i<value.rows();++i) {
        if (i) std::cout<<',';
        std::cout<<'[';
        for (std::size_t j=0;j<value.cols();++j) {if (j) std::cout<<','; std::cout<<'"'<<value.get(i,j)<<'"';}
        std::cout<<']';
    }
    std::cout<<']';
}
static void emit(const Field& field,const char* name,const Model& model) {
    const auto prepared=model.prepare(rfc::local::AlgebraicChainPolicy::DeclaredDeformation);
    const auto& source=model.bundle.source(); const auto d=field.degree();
    std::cout<<"{\"case\":\""<<name<<"\",\"degree\":"<<d<<",\"polynomial\":[";
    for (std::size_t i=0;i<field.minimal_polynomial().size();++i) {
        if (i) std::cout<<',';
        std::cout<<'"'<<field.minimal_polynomial()[i]<<'"';
    }
    std::cout<<"],\"flat\":"<<(prepared.realization().flat()?"true":"false")<<",\"ranks\":[";
    for (std::size_t k=0;k<source.grade_count();++k) {
        if (k) std::cout<<',';
        const auto grade=G{static_cast<std::uint16_t>(k)}; std::cout<<'[';
        for (std::uint32_t i=0;i<source.grade(grade).cell_count();++i) {
            if (i) std::cout<<',';
            std::cout<<model.bundle.stalk({grade,i}).rank();
        }
        std::cout<<']';
    }
    std::cout<<"],\"edges\":[";
    for (std::size_t i=0;i<model.edges.size();++i) {
        if (i) std::cout<<',';
        const auto& edge=model.edges[i]; const auto ends=rfc::topology::pairwise_endpoints(source,edge.edge.index);
        const auto slots=rfc::topology::pairwise_incidence_slots(source,edge.edge.index);
        std::cout<<"{\"index\":"<<edge.edge.index<<",\"tail\":"<<ends.tail<<",\"head\":"<<ends.head
                 <<",\"magnitude\":\""<<source.incidence_coefficient(G{1},slots.second)<<"\",\"chart\":";
        matrix(edge.tail_chart.rational_action()); std::cout<<",\"forward\":"; matrix(edge.forward.rational_action()); std::cout<<'}';
    }
    std::cout<<"],\"occurrences\":["; bool first=true;
    for (std::uint16_t k=1;k<source.grade_count();++k) {
        const auto& arena=source.grade(G{k});
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            if (!first) std::cout<<',';
            first=false;
            std::cout<<"{\"grade\":"<<k<<",\"slot\":"<<slot<<",\"upper\":"<<arena.upper_index[slot]
                     <<",\"lower\":"<<arena.lower_index[slot]<<",\"active\":"
                     <<(arena.aspects.get(source.signature().existence_aspect(),slot)?"true":"false")
                     <<",\"weight\":\""<<source.incidence_coefficient(G{k},slot)<<"\",\"map\":";
            matrix(prepared.realization().occurrence_map(G{k},slot).rational_action()); std::cout<<'}';
        }
    }
    std::cout<<"],\"boundaries\":[";
    for (std::uint16_t k=1;k<source.grade_count();++k) {if (k>1) std::cout<<','; matrix(prepared.realization().rational_boundary(G{k}));}
    std::cout<<"],\"defects\":[";
    for (std::uint16_t k=2;k<source.grade_count();++k) {if (k>2) std::cout<<','; matrix(prepared.realization().rational_chain_defect(G{k}));}
    std::cout<<"],\"faces\":[";
    for (std::size_t i=0;i<prepared.faces().size();++i) {
        if (i) std::cout<<',';
        const auto& face=prepared.faces()[i];
        std::cout<<"{\"index\":"<<face.face.index<<",\"base\":"<<face.base_vertex<<",\"chart\":";
        matrix(face.chart.rational_action()); std::cout<<",\"holonomy\":"; matrix(face.holonomy.rational_action());
        std::cout<<",\"holonomy_defect\":"; matrix(face.holonomy_defect.rational_action());
        std::cout<<",\"boundary_defect\":"; matrix(face.boundary_defect.rational_action());
        std::cout<<",\"terms\":[";
        for (std::size_t j=0;j<face.occurrences.size();++j) {
            if (j) std::cout<<',';
            const auto& term=face.occurrences[j];
            std::cout<<"{\"slot\":"<<term.source_slot<<",\"before\":"; matrix(term.prefix_before.rational_action());
            std::cout<<",\"after\":"; matrix(term.prefix_after.rational_action());
            std::cout<<",\"realized\":"; matrix(term.realized.rational_action()); std::cout<<'}';
        }
        std::cout<<"]}";
    }
    const auto path=prepared.path(0,{{0,true},{1,true},{2,true}}),prefix=path.prefix(1);
    const auto receipt=prepared.replay_face_moves(prepared.path(0,{}),{{0,0,false,true,0}});
    std::cout<<"],\"path\":"; matrix(path.transport().rational_action()); std::cout<<",\"prefix\":"; matrix(prefix.transport().rational_action());
    std::cout<<",\"move_after\":"; matrix(receipt.after().transport().rational_action());
    std::cout<<",\"move_preserves\":"<<(receipt.full_transport_preserved()?"true":"false")
             <<",\"used_loops_trivial\":"<<(receipt.every_used_loop_trivial()?"true":"false")<<"}\n";
}
int main() {
    try {
        const auto rational=Field::declare({0,1},rfc::numeric::IrreducibleReduction{2});
        const auto cubic=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
        for (const auto& field:{rational,cubic}) {
            emit(field,"flat_tower",make(field));
            emit(field,"full_curvature",make(field,false,true));
            emit(field,"partial_chart",make(field,false,true,true));
            emit(field,"higher_defect",make(field,true,false,false,Q{1},true));
            emit(field,"weighted_reverse_repetition",make(field,false,true,false,Q{2},false,2,true));
        }
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
