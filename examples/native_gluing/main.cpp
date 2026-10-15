#include "fixture.hpp"
#include <iostream>
int main() {
    try {
        using namespace native_gluing_example;
        const auto plan=prepare(); const auto source=plan.constructed_source();
        std::cout<<"glued_cells=";
        for (std::uint16_t k=0;k<4;++k) { if (k) std::cout<<','; std::cout<<source->source.grade(grade(k)).cell_count(); }
        std::cout<<"\nretained_edge_occurrences="<<source->source.grade(grade(1)).incidence_count()<<'\n';
        const Values section{{0,Q{0}},{1,Q{5}},{2,Q{7}}};
        const auto output=plan.glue_sections(42,grade(0),local_chain(plan,false,grade(0),section),local_chain(plan,true,grade(0),section),available);
        std::cout<<"glued_section_support="<<output.chain().values().size()<<'\n';
        storage::NativeAccessionDirectory target(storage::TemporalEndpoint(triangle(),10),903);
        const auto map=Map::prepare(plan,map_request(plan,target),map_available);
        const auto chain=plan.chain(grade(2),{{0,Q{1}},{1,Q{-1}}},available);
        const auto mapped=map.execute(chain,map_available);
        std::cout<<"mediated_face_coefficient="<<mapped.values().at(0)<<"\nmediated_face_support="<<mapped.values().size()<<'\n';
        const auto d2=plan.boundary(plan.boundary(chain,available),available);
        bool zero=true; for (const auto& [i,q]:d2.chain().values()) { (void)i; zero=zero && q==0; }
        std::cout<<"boundary_squared="<<(zero?0:1)<<'\n'; return zero?0:1;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
