#include "../../examples/native_frames/fixture.hpp"
#include "rfc/inference/native_hodge_invariants.hpp"
#include <iostream>
using namespace rfc;
using namespace native_frame_example;
using namespace inference;
std::size_t checks{};
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"required invalid determinant/volume rejection");}
PreparedNativeExactTower tower(const storage::NativeAccessionDirectory& d,unsigned mode){
    auto r=request(d,core::Grade{0},mode);NativeExactTowerRequest q;q.metrics=r.metrics;q.grade_carriers=r.grade_carriers;q.metric_tensors=r.metric_tensors;
    q.profile=field::PreparedExactProfile::GlobalActions;return PreparedNativeExactTower::prepare(input(d,core::Grade{0}),q);
}
int main(){try{
    const NativeActionAvailability available{{900,7}};
    for(const auto& source:{pair(),branch(),higher(),isolated(),empty(3)})for(unsigned mode=0;mode<3;++mode){
        storage::NativeAccessionDirectory directory(endpoint(source,mode),900);
        const auto t=tower(directory,mode);const auto operators=PreparedNativeOperatorFamily::prepare(t,available);
        for(std::uint16_t k=0;k<source.grade_count();++k){
            const core::Grade grade{k};const auto frames=PreparedNativeHodgeFrames::prepare(input(directory,grade),request(directory,grade,mode),{900,7});
            const auto plan=PreparedNativeHodgeInvariants::prepare(frames,operators,available);
            const auto& exact=plan.numeric_plan();const auto prep=exact.preparation_steps();const auto H=frames.numeric_plan().harmonic_frame();
            const auto M=metric(source.grade(grade).cell_count(),mode,k);
            const field::ExactHodgeOperator reference(source,grade,t.tower().metrics());
            const auto pdet=field::pseudodeterminant_via_harmonic_completion(reference);
            const auto hvol=H.transpose().multiply(M).multiply(H).determinant();
            for(unsigned repeat=0;repeat<2;++repeat){const auto result=plan.execute(available);
                check(result.pseudodeterminant==pdet && result.harmonic_volume==hvol,"cached source-factor pdet and harmonic volume");
                check(result.source_namespace==900 && result.sequence==7 && result.carrier==std::uint64_t{10}+k && result.plan_owner,"native invariant actual carrier/source receipt");}
            check(exact.preparation_steps()==prep,"pdet and volume reuse their actual factors");
            const auto n=H.cols();Matrix sigma(n,n);for(std::size_t i=0;i<n;++i){sigma.set(i,i,Q{static_cast<unsigned long>(i+2)});if(i+1<n)sigma.set(i,i+1,Q{1,3});}
            for(const auto& coefficients:{sigma,Matrix(n,n)}){
                const field::PreparedHarmonicDeterminant completion(exact,coefficients);
                const auto expected=reference.materialize().add(H.multiply(coefficients).multiply(H.transpose()).multiply(M)).determinant();
                check(completion.determinant()==expected,"cached harmonic completion determinant including singular and empty coefficient matrices");
                const auto native_completion=PreparedNativeHarmonicDeterminant::prepare(plan,coefficients,available);
                const auto cached=native_completion.preparation_steps();const auto receipt=native_completion.execute(available);
                check(receipt.determinant==expected && receipt.plan_owner && native_completion.harmonic_frame()==H &&
                    native_completion.coefficient_matrix()==coefficients && native_completion.preparation_steps()==cached,
                    "native harmonic completion retains actual harmonic coordinates and reuses Hodge factors");
            }
            reject([&]{field::PreparedHarmonicDeterminant bad(exact,Matrix(n+1,n+1));});
        }
    }
    const Matrix Z=Matrix::from_rows({{1,0},{1,1},{0,1}}),R=Matrix::from_rows({{2,1},{1,3}}),M=Matrix::from_rows({{3,1,0},{1,4,1},{0,1,2}});
    const auto FW=Z.multiply(R);Matrix F(3,1),W(3,1);for(std::size_t i=0;i<3;++i){F.set(i,0,FW.get(i,0));W.set(i,0,FW.get(i,1));}
    const field::PreparedMetricVolumeFactorization volume(Z,F,W,R,M);
    const auto reference=field::metric_volume_factorization(Z,F,W,R,field::ExactMetric(M));
    numeric::ExactActionGuard measured;const auto first=volume.result(measured);
    check(first.harmonic_frame==reference.harmonic_frame && first.filled_volume==reference.filled_volume && first.harmonic_volume==reference.harmonic_volume &&
        first.closed_volume==reference.closed_volume && first.coordinate_volume==25 && first.exact_identity,"prepared metric volume factorization");
    check(F.transpose().multiply(M).multiply(first.harmonic_frame).nonzeros()==0,"filled/harmonic frames exactly orthogonal");
    auto limits=numeric::ExactActionLimits{};limits.steps=measured.steps();
    check(volume.result(limits).harmonic_volume==first.harmonic_volume,"volume receipts reuse factors under exact observation budget");
    reject([&]{field::PreparedMetricVolumeFactorization bad(Z,F,W,Matrix::identity(2),M);});
    reject([&]{field::PreparedMetricVolumeFactorization bad(Z,F,W,R,Matrix::from_rows({{1,0,0},{0,-1,0},{0,0,1}}));});
    reject([&]{field::PreparedMetricVolumeFactorization bad(Matrix(3,2),F,W,R,M);});
    storage::NativeAccessionDirectory a(endpoint(pair(),2),900),b(endpoint(pair(),2),900);
    const auto f=PreparedNativeHodgeFrames::prepare(input(a,core::Grade{0}),request(a,core::Grade{0},2),{900,7});
    const auto other=PreparedNativeOperatorFamily::prepare(tower(b,2),available);
    reject([&]{PreparedNativeHodgeInvariants::prepare(f,other,available);});
    const auto own=PreparedNativeOperatorFamily::prepare(tower(a,2),available);const auto native=PreparedNativeHodgeInvariants::prepare(f,own,available);
    reject([&]{native.execute({{900,6}});});
    runtime::ExecutionHostPolicy stopped;stopped.limits.scalar_work=1;
    reject([&]{native.execute(available,runtime::ExecutionLease(stopped));});
    std::cout<<"prepared Hodge invariants: "<<checks<<" exact checks passed\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
