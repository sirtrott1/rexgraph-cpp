#include "rfc/learning/assertion_diagnosis.hpp"
#include "rfc/core/builder.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
std::size_t checks{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class E,class F> void rejects(F&& fn,const char* why){
    bool hit=false;try{fn();}catch(const E&){hit=true;}check(hit,why);
}
std::shared_ptr<const core::RelationalComplex> triangle(bool filled) {
    core::RelationSignature signature;
    const auto edge=signature.add_type("assertion",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_type("declared closure",core::BoundaryPolicy::DeclaredExact,3,3);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{101},edge,{{10,true,true,Q{1}},{11,true,false,Q{1}}});
    builder.add_grade_one(core::RelationKey{102},edge,{{11,true,true,Q{1}},{12,true,false,Q{1}}});
    builder.add_grade_one(core::RelationKey{103},edge,{{10,true,true,Q{1}},{12,true,false,Q{1}}});
    if(filled)builder.add_higher(core::Grade{2},core::RelationKey{201},face,
        {{0,true,false,Q{1}},{1,true,false,Q{1}},{2,true,true,Q{1}}});
    return std::make_shared<const core::RelationalComplex>(builder.freeze());
}
std::shared_ptr<const core::RelationalComplex> tree() {
    core::RelationSignature signature;
    const auto edge=signature.add_type("assertion",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge,{{10,true,true,Q{1}},{11,true,false,Q{1}}});
    builder.add_grade_one(edge,{{11,true,true,Q{1}},{12,true,false,Q{1}}});
    return std::make_shared<const core::RelationalComplex>(builder.freeze());
}
std::shared_ptr<const core::RelationalComplex> branching() {
    core::RelationSignature signature;
    const auto relation=signature.add_type("mean relation",core::BoundaryPolicy::DeclaredExact,3,3);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(relation,{{10,true,false,Q{1,2}},{11,true,false,Q{1,2}},
                                   {12,true,true,Q{1}}});
    return std::make_shared<const core::RelationalComplex>(builder.freeze());
}
}
int main(){
    try {
        Guard guard;
        const auto loop=triangle(false),filled=triangle(true);
        const learning::PreparedAssertionDiagnosis unfilled(loop),closed(filled);
        const V correct{3,4,7},bad{8,4,7},defect{Q{5,3},Q{5,3},Q{-5,3}};
        const auto clean=unfilled.analyze(correct,guard);
        check(clean.exact && clean.consistent,"consistent assertion certificate");
        check(clean.minimum_potential==V{Q{-10,3},Q{-1,3},Q{11,3}},"unique minimum potential with complete gauge");
        check(unfilled.potential_gauge_frame().cols()==1,"connected pairwise potential kernel");
        const auto a=unfilled.analyze(bad,guard),b=closed.analyze(bad,guard);
        check(a.source==loop && a.grade==core::Grade{1},"diagnosis owns actual source carrier");
        check(!a.consistent && a.exact && a.residual==defect,"exact cycle contradiction");
        check(a.residual_squared_norm==Q{25,3} && a.contradiction_pairing==Q{25,3},"positive contradiction witness pairing");
        check(numeric::is_zero(a.contradiction_boundary),"contradiction annihilates all potentials");
        check(a.harmonic==defect && numeric::is_zero(a.curl),"unfilled cycle harmonic obstruction");
        check(b.curl==defect && numeric::is_zero(b.harmonic),"declared filling converts harmonic to curl");
        check(a.gradient==b.gradient && a.residual==b.residual,"filling does not hide assertion inconsistency");
        const std::array<std::size_t,3> all{0,1,2};
        const auto candidates=unfilled.candidates(bad,all,guard);
        for(std::size_t i=0;i<all.size();++i){
            check(candidates[i].signature.retained_key==core::RelationKey{101+i},"signature retains primary assertion identity");
            check(candidates[i].signature.detectable && candidates[i].explains_residual,
                  "one cycle detects corruption but cannot choose its location");
            check(candidates[i].score==a.residual_squared_norm && candidates[i].unexplained_squared_norm==0,
                  "all parallel signatures retain equal maximum score");
            check(candidates[i].compatible_with_nonzero_single_corruption,"ambiguity must remain represented");
            check(unfilled.signature(i,guard).residual==closed.signature(i,guard).residual,
                  "corruption signatures independent of declared two-cell filling");
        }
        check(candidates[0].amplitude==Q{5} && candidates[1].amplitude==Q{5}
              && candidates[2].amplitude==Q{-5},"conditional amplitudes retain their signs");
        const auto repaired=unfilled.repair_known(bad,0,guard);
        check(repaired.identifiable && repaired.compatible && repaired.corrected.has_value(),"known detectable corruption can be repaired");
        check(repaired.amplitude==Q{5} && repaired.corrected->input==correct && repaired.corrected->consistent,
              "known corruption correction agrees with original values");
        const std::array<std::size_t,1> selected{2};
        check(unfilled.candidates(bad,selected,guard).size()==1,"only explicitly requested signatures returned");
        const learning::PreparedAssertionDiagnosis acyclic(tree());
        const auto invisible=acyclic.candidates({100,-71},std::array<std::size_t,2>{0,1},guard);
        check(acyclic.analyze({100,-71},guard).consistent,"tree assertions have no redundant contradiction detector");
        for(const auto& item:invisible){
            check(!item.signature.detectable && !item.amplitude && item.compatible_with_nonzero_single_corruption,
                  "zero signature preserves undetectable corruption ambiguity");
        }
        const auto no_repair=acyclic.repair_known({100,-71},0,guard);
        check(!no_repair.identifiable && no_repair.compatible && !no_repair.corrected,
              "undetectable corruption is not falsely corrected");
        const learning::PreparedAssertionDiagnosis branch(branching());
        const auto branch_result=branch.analyze({3},guard);
        check(branch.potential_gauge_frame().cols()==2,"single branching relation retains full two-dimensional kernel");
        check(branch_result.minimum_potential==V{1,1,-2} && branch_result.consistent,
              "branching minimum potential uses full gauge rather than a vertex anchor");

        const M m0=M::from_rows({{2,1,0},{1,3,1},{0,1,2}});
        const M m1=M::from_rows({{4,1,0},{1,3,1},{0,1,5}});
        const learning::PreparedAssertionDiagnosis weighted(filled,{m0,m1,M::from_rows({{7}})},core::Grade{1},guard);
        const auto boundary=field::hodge_frame_detail::boundary(*filled,core::Grade{1},guard);
        const V original_potential{2,5,9};
        const auto covectors=boundary.transpose().apply(m0.apply(original_potential));
        const auto weighted_clean=weighted.analyze_covectors(covectors,guard);
        check(weighted_clean.consistent && weighted_clean.exact,"non-diagonal weighted covector equations");
        check(boundary.transpose().apply(m0.apply(weighted_clean.minimum_potential))==covectors,
              "weighted minimum potential satisfies the literal original equations");
        check(numeric::is_zero(weighted.potential_gauge_frame().transpose().apply(m0.apply(weighted_clean.minimum_potential))),
              "weighted gauge removes the complete metric kernel");
        auto weighted_bad=weighted_clean.input;weighted_bad[1]+=Q{7,2};
        const auto weighted_fix=weighted.repair_known(weighted_bad,1,guard);
        check(weighted_fix.corrected && weighted_fix.amplitude==Q{7,2}
              && weighted_fix.corrected->input==weighted_clean.input,"weighted known-field corruption repair");
        check(weighted.analyze(weighted_bad,guard).exact,"weighted contradiction certificate uses its metric");
        rejects<std::out_of_range>([&]{(void)unfilled.signature(3,guard);},"undeclared assertion index rejects");
        rejects<std::invalid_argument>([&]{(void)unfilled.analyze({1},guard);},"wrong assertion carrier rejects");
        rejects<std::invalid_argument>([&]{(void)learning::PreparedAssertionDiagnosis(loop,core::Grade{0});},"grade-zero assertion equations reject");
        rejects<std::invalid_argument>([]{(void)learning::PreparedAssertionDiagnosis(nullptr);},"missing source owner rejects");
        std::cout<<"assertion diagnosis: "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
