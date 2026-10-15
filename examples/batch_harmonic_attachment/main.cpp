#include "fixture.hpp"
#include <iostream>

int main() {
    const auto result=batch_attachment_example::run();const auto& report=result.provenance();
    if(report.harmonic_removed!=1 || report.harmonic_remaining!=0 || report.upper_homology_added!=2)return 1;
    std::cout<<"Source publication "<<report.source_namespace<<':'<<report.sequence
        <<" retained; "<<report.appended.size()<<" actual source attachments staged.\n"
        <<report.harmonic_removed<<" harmonic direction moves to curl; "
        <<report.upper_homology_added<<" new upper-cycle witnesses retained.\n";
}
