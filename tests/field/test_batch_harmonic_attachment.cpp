#include "rfc/field/batch_harmonic_attachment.hpp"
#include <iostream>
using namespace rfc;
using Q=numeric::Rational;
using M=numeric::ExactMatrix;
using S=numeric::SparseRationalVector;
using Guard=numeric::ExactActionGuard;
using Batch=field::PreparedBatchHarmonicAttachment;
namespace {
std::size_t checks{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
std::shared_ptr<const core::RelationalComplex> triangles(bool old_face) {
    core::RelationSignature signature;const auto type=signature.add_type("exact boundary",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder b(signature);b.declare_grade(core::Grade{3});
    for(int offset:{0,10}) {
        b.add_grade_one(type,{{static_cast<unsigned>(offset),true,true,1},{static_cast<unsigned>(offset+1),true,false,1}});
        b.add_grade_one(type,{{static_cast<unsigned>(offset+1),true,true,1},{static_cast<unsigned>(offset+2),true,false,1}});
        b.add_grade_one(type,{{static_cast<unsigned>(offset),true,true,1},{static_cast<unsigned>(offset+2),true,false,1}});
    }
    if(old_face)b.add_higher(core::Grade{2},core::RelationKey{100},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    return std::make_shared<const core::RelationalComplex>(b.freeze());
}
std::vector<M> metrics(const core::RelationalComplex& source,std::size_t selected) {
    std::vector<M> out;for(std::size_t k=0;k<source.grade_count();++k)out.push_back(M::identity(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()));
    if(out[selected].rows()==6){out[selected]=M::diagonal({2,3,5,7,11,13});out[selected].set(0,3,1);out[selected].set(3,0,1);}
    return out;
}
core::BatchAttachmentCell face(std::uint64_t key,unsigned offset,Q factor=1) {
    return {core::RelationKey{key},0,{{offset,true,false,factor},{offset+1,true,false,factor},{offset+2,true,true,factor}}};
}
void compare(const Batch& batch,Guard& guard) {
    auto declared=metrics(*batch.attachment().after(),batch.original_frames().grade().value);
    declared[batch.original_frames().grade().value]=batch.original_frames().metrics()[batch.original_frames().grade().value];
    const field::PreparedHodgeFrames rebuilt(batch.attachment().after(),declared,batch.original_frames().grade(),guard);
    const auto n=batch.attachment().columns().rows();
    for(std::size_t i=0;i<n;++i) {
        const auto input=S::basis(n,i,Q{static_cast<unsigned long>(i+1),3});const auto changed=batch.decompose(input,guard);
        check(changed.gradient.entries()==rebuilt.project_sector(0,input,guard).entries(),"batch gradient differs from independent rebuilt Hodge frames");
        check(changed.curl.entries()==rebuilt.project_sector(1,input,guard).entries(),"batch curl differs from independent rebuilt Hodge frames");
        check(changed.harmonic.entries()==rebuilt.project_sector(2,input,guard).entries(),"batch harmonic differs from independent rebuilt Hodge frames");
        check(changed.reconstructed.entries()==input.entries(),"sparse batch field reconstruction differs");
    }
    check(rebuilt.harmonic_frame().cols()==batch.harmonic_remaining(),"harmonic decrement differs from rebuilt rank");
}
void simultaneous_batch() {
    Guard guard;const auto source=triangles(false);field::PreparedHodgeFrames old(source,metrics(*source,1),core::Grade{1},guard);
    std::vector<core::BatchAttachmentCell> declarations{face(101,0),face(102,0,2),face(103,3),
        {core::RelationKey{104},0,{{0,true,false,Q{1,7}},{0,true,true,Q{1,7}}}}};
    const auto batch=Batch::prepare(old,declarations,guard);
    check(batch.harmonic_removed()==2 && batch.upper_homology_added()==2 && batch.harmonic_remaining()==0,"batch rank/inheritance statistics differ");
    check(batch.attachment().after()->grade_count()==4 && batch.attachment().after()->grade(core::Grade{3}).cell_count()==0,"batch dropped explicit empty upper grade");
    check(batch.attachment().after()->grade(core::Grade{2}).incidence_count()==11,"canceling attachment occurrences were discarded");
    check(batch.attachment().before().get()==old.source().get() && batch.original_frames().source().get()==old.source().get(),"batch detached original source owner");
    check(batch.attachment().columns().get(0,1)==2 && batch.attachment().columns().get(0,3)==0,"native candidate columns differ");
    check(batch.upper_cycle_witnesses().cols()==2 && batch.upper_cycle_witnesses().rank()==2,"new upper-cycle witnesses are incomplete");
    const auto boundary=field::hodge_frame_detail::boundary(*batch.attachment().after(),core::Grade{2},guard);
    check(numeric::checked_multiply(boundary,batch.upper_cycle_witnesses(),guard).nonzeros()==0,"new upper-cycle boundary does not vanish");
    compare(batch,guard);
    declarations[0].incidences[0].magnitude=99;
    check(batch.attachment().declarations()[0].incidences[0].magnitude==1,"caller mutation changed batch declaration");
    check(source->grade(core::Grade{2}).cell_count()==0,"staging mutated original source");
}
void old_boundary_lift() {
    Guard guard;const auto source=triangles(true);field::PreparedHodgeFrames old(source,metrics(*source,1),core::Grade{1},guard);
    const auto batch=Batch::prepare(old,{face(101,0),face(102,3,2),face(103,3)},guard);
    check(batch.harmonic_removed()==1 && batch.upper_homology_added()==2,"old curl contribution altered harmonic rank increment");
    check(batch.upper_cycle_witnesses().get(0,0)==-1 && batch.upper_cycle_witnesses().get(1,0)==1,"residual-zero attachment failed to lift through old upper-cell identity");
    compare(batch,guard);
}
void grade_zero() {
    Guard guard;core::RelationSignature signature;signature.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);for(const auto key:{10u,20u,30u})builder.declare_witness_identity(key);
    const auto source=std::make_shared<const core::RelationalComplex>(builder.freeze());
    field::PreparedHodgeFrames old(source,{M::diagonal({2,3,5})},core::Grade{0},guard);
    const auto batch=Batch::prepare(old,{{core::RelationKey{1},0,{{0,true,true,1},{1,true,false,1}}},
        {core::RelationKey{2},0,{{1,true,true,1},{2,true,false,1}}}},guard);
    check(batch.harmonic_removed()==2 && batch.harmonic_remaining()==1 && batch.upper_homology_added()==0,"grade-zero attachment dimensions differ");
    compare(batch,guard);
}
void refusals() {
    Guard guard;const auto source=triangles(false);field::PreparedHodgeFrames old(source,metrics(*source,1),core::Grade{1},guard);
    rejects([&]{(void)Batch::prepare(old,{{core::RelationKey{99},0,{{0,true,false,1}}}},guard);},"nonclosed batch column accepted");
    auto bad=face(99,0);bad.incidences[0].lower_index=999;
    rejects([&]{(void)Batch::prepare(old,{bad},guard);},"batch invented a lower cell");
    rejects([&]{(void)Batch::prepare(old,{face(99,0),face(99,3)},guard);},"duplicate appended relation identities accepted");
    const auto other=std::make_shared<const core::RelationalComplex>(*source);
    const auto stage=core::PreparedBatchAttachment::prepare(other,core::Grade{2},{face(99,0)},guard);
    rejects([&]{(void)Batch(old,stage,guard);},"equal-valued source substituted for original frame owner");
    rejects([&]{Guard canceled({},[]{throw std::runtime_error("cancel");});(void)Batch::prepare(old,{face(99,0)},canceled);},"batch cancellation ignored");
    rejects([&]{auto limits=numeric::ExactActionLimits{};limits.entries=1;Guard small(limits);(void)Batch::prepare(old,{face(99,0)},small);},"batch storage admission ignored");
}
}
int main(){try{simultaneous_batch();old_boundary_lift();grade_zero();refusals();std::cout<<checks<<" batch harmonic attachment checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
