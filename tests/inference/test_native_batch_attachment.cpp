#include "../../examples/batch_harmonic_attachment/fixture.hpp"
#include <iostream>
using namespace rfc;
using Frames=inference::PreparedNativeHodgeFrames;
using Batch=inference::PreparedNativeBatchHarmonicAttachment;
namespace {
std::size_t checks{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
void native_receipt() {
    storage::NativeAccessionDirectory directory(batch_attachment_example::endpoint(),900);
    const runtime::WorkAvailability available{900,7};const core::Grade grade{1};
    const auto field=native_frame_example::input(directory,grade);
    const auto original=Frames::prepare(field,native_frame_example::request(directory,grade,2),available);
    const auto before=storage::encode_temporal_endpoint(*field.endpoint());
    const auto plan=Batch::prepare(original,{batch_attachment_example::declarations(),64},available);
    const auto prep=plan.preparation_steps();const auto result=plan.execute(field,available);const auto& report=result.provenance();
    check(report.source_namespace==900 && report.sequence==7 && report.input_field==102 && report.carrier==11,"batch provenance does not retain actual publication/field/carrier");
    check(report.harmonic_removed==1 && report.harmonic_remaining==0 && report.upper_homology_added==2,"native batch homology changes differ");
    check(report.appended.size()==3 && report.appended[0].key==core::RelationKey{200} && report.appended[2].key==core::RelationKey{202},"actual appended relation identities missing");
    check(result.sparse_values().reconstructed.entries()==field.tensor()->values().entries(),"native sparse field reconstruction differs");
    check(plan.preparation_steps()==prep && &plan.numeric_plan().original_frames().gradient_frame()==&original.numeric_plan().gradient_frame(),"batch rebuilt the original frame owner");
    check(result.plan().staged_source().get()==plan.staged_source().get() && plan.staged_source()->grade(core::Grade{2}).cell_count()==3,"derived receipt lost owned staged source");
    check(storage::encode_temporal_endpoint(*field.endpoint())==before && field.endpoint()->source.grade(core::Grade{2}).cell_count()==0,"staging changed the original publication");
    const auto again=result.plan().execute(result.input(),available);
    check(again.sparse_values().curl.entries()==result.sparse_values().curl.entries(),"reusable native batch action differs");
    storage::NativeAccessionDirectory clone(batch_attachment_example::endpoint(),900);
    rejects([&]{(void)plan.execute(native_frame_example::input(clone,grade),available);},"same bytes from another execution authority entered native batch");
    rejects([&]{(void)plan.execute(native_frame_example::input(directory,core::Grade{0}),available);},"wrong actual source grade entered native batch");
    rejects([&]{(void)plan.execute(field,{900,6});},"past availability cutoff accepted");
    auto later=batch_attachment_example::endpoint();later.sequence=8;
    storage::NativeAccessionDirectory future(later,900);
    rejects([&]{(void)plan.execute(native_frame_example::input(future,grade),{900,8});},"new same-key publication substituted for actual original pin");
    for(unsigned mode=0;mode<3;++mode) {
        runtime::ExecutionHostPolicy policy;
        if(mode==0)policy.limits.rational_slots=1;else if(mode==1)policy.limits.scalar_work=0;else policy.limits.integer_bits=0;
        rejects([&]{(void)plan.execute(field,available,runtime::ExecutionLease{policy});},"native batch current execution budget ignored");
    }
    auto stop=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease canceled({},stop);
    rejects([&]{(void)plan.execute(field,available,canceled);},"native batch cancellation ignored");
    stop->store(false);const auto resumed=plan.execute(field,available,canceled);
    check(resumed.sparse_values().reconstructed.entries()==field.tensor()->values().entries(),"cancelled action changed reusable batch");
}
void retained_lifetime() {
    const auto result=batch_attachment_example::run();const auto again=result.plan().execute(result.input(),{900,7});
    check(again.sparse_values().harmonic.entries()==result.sparse_values().harmonic.entries(),"batch receipt lost original pins after directory destruction");
    check(result.plan().numeric_plan().upper_cycle_witnesses().cols()==2,"native receipt lost new upper-cycle witnesses");
}
}
int main(){try{native_receipt();retained_lifetime();std::cout<<checks<<" native batch attachment checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
