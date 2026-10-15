#include "../../examples/native_frames/fixture.hpp"
#include "rfc/inference/native_moments.hpp"
#include <array>
#include <iostream>
using namespace rfc;
using namespace native_frame_example;
using namespace inference;
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"required rejection");}
Q dot(const Vector& a,const Vector& b){Q q=0;for(std::size_t i=0;i<a.size();++i)q+=a[i]*b[i];return q;}
Q trace(const Matrix& a){Q q=0;for(std::size_t i=0;i<a.rows();++i)q+=a.get(i,i);return q;}
PreparedNativeExactTower tower(const storage::NativeAccessionDirectory& d){
    const auto r=request(d,core::Grade{0},2);NativeExactTowerRequest q;q.metrics=r.metrics;q.grade_carriers=r.grade_carriers;q.metric_tensors=r.metric_tensors;
    q.profile=field::PreparedExactProfile::GlobalActions;return PreparedNativeExactTower::prepare(input(d,core::Grade{0}),q);
}
int main(){try{
    auto old=endpoint(pair(),2),next=endpoint(branch(),2,8),future=endpoint(pair(),2,9);
    const std::vector<Matrix> O{Matrix::from_rows({{1,1},{0,1}}),Matrix::from_rows({{2,0},{1,3}}),Matrix::from_rows({{1,-1},{2,1}})};
    const std::vector<Matrix> N{Matrix::from_rows({{1,0,1,0},{0,2,0,0},{0,1,1,0},{1,0,0,1}}),
        Matrix::from_rows({{2,0,0,1},{1,1,0,0},{0,0,3,0},{0,1,0,1}}),Matrix::from_rows({{1,0,0,0},{0,1,1,0},{0,0,1,1},{1,0,0,1}})};
    std::vector<Matrix> J;for(unsigned i=0;i<4;++i)J.push_back(Matrix::from_rows({{Q{static_cast<unsigned long>(i+1)},0},{0,1},{1,-1},{1,Q{static_cast<unsigned long>(i+2)}}}));
    for(std::size_t i=0;i<3;++i){old.tensors.push_back(native_update_example::map(800+i,10,10,O[i]));next.tensors.push_back(native_update_example::map(800+i,10,10,N[i]));}
    future.tensors.push_back(native_update_example::map(800,10,10,O[0]));
    old.tensors.push_back(native_update_example::field(2000,10,{1,2}));next.tensors.push_back(native_update_example::field(2000,10,{2,-1,3,1}));
    const std::vector<Vector> samples{{1,0},{0,1},{-1,-1}};
    for(std::size_t i=0;i<samples.size();++i)old.tensors.push_back(native_update_example::field(2010+i,10,samples[i]));
    storage::NativeAccessionDirectory od(old,900),nd(next,901),fd(future,903),foreign(old,900);
    const NativeActionAvailability available{{900,7},{901,8}};
    const auto tensor=[](const auto& directory,std::uint64_t key){return directory.pin(access(storage::NativeAccessionKind::Operator,key));};
    const auto native_field=[](const auto& directory,std::uint64_t key){return NativeFieldValue::stored(directory.pin(access(storage::NativeAccessionKind::Field,key)));};
    const auto x=native_field(od,2000),xp=native_field(nd,2000),future_x=NativeFieldValue::stored(input(fd,core::Grade{0}));
    std::vector<NativeFactoredAction> old_word,new_word,maps;
    for(std::size_t i=0;i<3;++i){old_word.push_back(NativeFactoredAction::tensor(tensor(od,800+i)));new_word.push_back(NativeFactoredAction::tensor(tensor(nd,800+i)));}
    for(const auto& j:J)maps.push_back(NativeFactoredAction::correspondence(x.carrier(),xp.carrier(),j));
    const auto words=PreparedNativeWordDefect::prepare(old_word,new_word,maps);
    const std::vector<std::size_t> outputs{0,3,0};const auto receipt=words.execute_selected(x,outputs,available);
    const auto old_product=O[2].multiply(O[1]).multiply(O[0]),new_product=N[2].multiply(N[1]).multiply(N[0]);
    const auto direct=new_product.multiply(J[0]).add(J[3].multiply(old_product).scale(-1)).apply(x.values());
    check(receipt.direct==Vector{direct[0],direct[3],direct[0]} && receipt.expanded==receipt.direct,"selected long-word identity and repeated output");
    for(std::size_t i=0;i<3;++i){
        auto term=N[i].multiply(J[i]).add(J[i+1].multiply(O[i]).scale(-1));
        for(std::size_t j=i;j-->0;)term=term.multiply(O[j]);
        for(std::size_t j=i+1;j<3;++j)term=N[j].multiply(term);
        const auto v=term.apply(x.values());check(receipt.contributions[i]==Vector{v[0],v[3],v[0]},"each exact word attribution");
    }
    check(receipt.plan_owner && receipt.output.same(xp.carrier()),"selected word owns actual output carrier and plan");
    const auto zero_word=PreparedNativeWordDefect::prepare({}, {}, {maps[0]}).execute_selected(x,{1},available);
    check(zero_word.direct==Vector{0} && zero_word.contributions.empty(),"empty word exact identity");
    reject([&]{words.execute_selected(x,{4},available);});
    reject([&]{words.execute_selected(native_field(foreign,2000),{0},available);});
    for(const std::size_t power:{0u,1u,17u}){auto expected=x.values();for(std::size_t i=0;i<power;++i)expected=O[0].apply(expected);
        check(native_power(old_word[0],power).execute(x,available).values()==expected,"retained factored power");}
    const auto future_action=NativeFactoredAction::tensor(tensor(fd,800));
    const auto prefix=native_terminal_channel({old_word[0],future_action},0,1,NativeFactoredAction::identity(x.carrier()),old_word[1]);
    check(prefix.execute(x,available).values()==O[1].apply(O[0].apply(x.values())),"selected terminal prefix ignores future suffix");
    // Selected moments do not execute or inspect the unused future channel.
    const auto M=NativeBilinearForm::tensor(metric_tensor(od,50));
    const PreparedNativeMoments moments({{"a",old_word[0]},{"b",old_word[1]},{"future",future_action}},{{0,1,M},{1,1,M}});
    const auto observed=moments.execute({x,x,future_x},available);const auto a=O[0].apply(x.values()),b=O[1].apply(x.values());
    check(observed.values==Vector{dot(a,metric(2,2,0).apply(b)),dot(b,metric(2,2,0).apply(b))},"selected native operation moments");
    check(observed.channel_indices==std::vector<std::size_t>{0,1} && observed.realized_channels.size()==2 && observed.plan_owner,"only selected channels realized");
    const auto cross=NativeBilinearForm::declared_cross(xp.carrier(),x.carrier(),J[2]);
    const PreparedNativeMoments cross_moment({{"transport",maps[0]},{"old",old_word[0]}},{{0,1,cross}});
    check(cross_moment.execute(x,available).values==Vector{dot(J[0].apply(x.values()),J[2].apply(a))},"cross-publication typed bilinear moment");
    reject([&]{PreparedNativeMoments({{"a",old_word[0]},{"new",maps[0]}},{{0,1,M}});});
    // Native sector and changing-dimension reconstruction/accession remain factors.
    const auto ot=tower(od),nt=tower(nd);const auto of=PreparedNativeOperatorFamily::prepare(ot,available),nf=PreparedNativeOperatorFamily::prepare(nt,available);
    const auto po=of.action({field::OperatorKind::HarmonicProjector,core::Grade{0},0},available),pn=nf.action({field::OperatorKind::HarmonicProjector,core::Grade{0},0},available);
    const auto Ho=field::exact_hodge_projectors(field::ExactHodgeOperator(old.source,core::Grade{0},ot.tower().metrics()));
    const auto Hn=field::exact_hodge_projectors(field::ExactHodgeOperator(next.source,core::Grade{0},nt.tower().metrics()));
    check(native_sector_transport(po,pn,maps[0]).execute(x,available).values()==Hn.project_harmonic(J[0].apply(Ho.project_harmonic(x.values()))),"native sparse sector transport");
    check(native_typed_sector_transport(old_word[0],po,maps[0],pn,new_word[0]).execute(x,available).values()==
        N[0].apply(Hn.project_harmonic(J[0].apply(Ho.project_harmonic(a)))),"native typed reconstruction and accession");
    // A derived field cannot hide an earlier dependency on a future publication.
    const auto into=NativeFactoredAction::correspondence(x.carrier(),future_x.carrier(),Matrix::identity(2));
    const auto back=NativeFactoredAction::correspondence(future_x.carrier(),x.carrier(),Matrix::identity(2));
    const auto historical=back.execute(into.execute(x,{{900,7},{903,9}}),{{900,7},{903,9}});
    check(historical.inputs().size()==1 && historical.publications().size()>1,"derived sparse field retains action dependencies separately from field roots");
    reject([&]{NativeFactoredAction::identity(x.carrier()).execute(historical,available);});
    reject([&]{old_word[0].execute(x,{{900,7},{900,8}});});
    std::vector<NativeFieldValue> native_samples;for(std::size_t i=0;i<3;++i)native_samples.push_back(native_field(od,2010+i));
    const auto ensemble=PreparedNativeEnsemble::prepare(M,native_samples,{1,2,3},available);const auto preparation=ensemble.preparation_steps();
    Matrix covariance(2,2);Vector mean(2);const Vector w{Q{1,6},Q{1,3},Q{1,2}};
    for(std::size_t i=0;i<3;++i)for(std::size_t r=0;r<2;++r)mean[r]+=w[i]*samples[i][r];
    for(std::size_t i=0;i<3;++i)for(std::size_t r=0;r<2;++r)for(std::size_t c=0;c<2;++c)covariance.set(r,c,covariance.get(r,c)+w[i]*(samples[i][r]-mean[r])*(samples[i][c]-mean[c]));
    const auto gs=metric(2,2,0).multiply(covariance);const auto variance=trace(gs),second=trace(gs.multiply(gs));
    for(unsigned i=0;i<2;++i){const auto values=ensemble.contractions(available);check(values.moments.variance==variance && values.moments.second_contraction==second &&
        values.moments.dimension==variance*variance/second && values.covariance_rank==2,"native sample ensemble exact contractions and rank");}
    check(ensemble.preparation_steps()==preparation,"native ensemble factors reused");
    const auto coupling=ensemble.coupling(ensemble,available);
    check(coupling.cross_contraction==second && coupling.coupling==second/(variance*variance) && coupling.left_owner && coupling.right_owner,"paired ensemble coupling reuses centered frames");
    reject([&]{PreparedNativeEnsemble::prepare(M,native_samples,{1,-1,2},available);});
    reject([&]{PreparedNativeEnsemble::prepare(M,native_samples,{0,0,0},available);});
    reject([&]{auto bad=endpoint(pair(),2);bad.tensors.push_back(native_update_example::map(850,10,10,Matrix::from_rows({{1,2},{2,1}}),storage::TemporalTensorRole::Metric));
        storage::NativeAccessionDirectory invalid(bad,900);NativeBilinearForm::tensor(metric_tensor(invalid,850));});
    reject([&]{PreparedNativeEnsemble::prepare(M,{x,x},{1,1},available).contractions(available);});
    reject([&]{ensemble.coupling(PreparedNativeEnsemble::prepare(M,native_samples,{1,1,1},available),available);});
    reject([&]{statistics::effective_dimension_2(Matrix::identity(2),Matrix::from_rows({{1,2},{2,1}}));});
    reject([&]{statistics::cross_sector_coupling(Matrix::identity(1),Matrix::identity(1),Matrix::from_rows({{2}}),Matrix::identity(1),Matrix::identity(1),Matrix::from_rows({{2}}));});
    check(statistics::cross_sector_coupling(Matrix::identity(1),Matrix::identity(1),Matrix::from_rows({{Q{1,2}}}),Matrix::identity(1),Matrix::identity(1),Matrix::from_rows({{Q{1,2}}}))==Q{1,4},"certified joint covariance coupling");
    // The logical carrier is much larger than addressable dense rational storage.
    // Identity, selected action and named-unit moment all keep just one entry.
    const std::size_t huge=std::size_t{1}<<33;auto sparse_endpoint=endpoint(empty(0),0);
    sparse_endpoint.carriers.push_back({999,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},123,numeric::IndexDomain::linear(huge)});
    storage::TemporalTensor sparse_field;sparse_field.key=2400;sparse_field.role=storage::TemporalTensorRole::Field;sparse_field.output=999;
    sparse_field.tensor={numeric::IndexDomain::linear(huge),numeric::StructuralSupport::full(huge),numeric::SparseRationalVector::basis(huge,huge-1,Q{3})};
    sparse_endpoint.tensors.push_back(sparse_field);storage::NativeAccessionDirectory sparse_directory(sparse_endpoint,904);
    const auto sparse=native_field(sparse_directory,2400);const auto identity=NativeFactoredAction::identity(sparse.carrier(),huge);
    runtime::ExecutionHostPolicy tiny;tiny.limits.rational_slots=16;tiny.limits.sparse_entries=16;tiny.limits.scalar_work=1024;
    const auto output=identity.execute(sparse,{{904,7}},runtime::ExecutionLease(tiny));
    check(output.sparse_values().size()==huge && output.sparse_values().nonzeros()==1,"huge native field is never implicitly densified");
    check(identity.selected(sparse,std::array<std::size_t,2>{huge-1,0},{{904,7}},runtime::ExecutionLease(tiny))==Vector{3,0},"huge sparse selected transpose port");
    const auto unit=NativeBilinearForm::named_unit(sparse.carrier(),{huge,16,1024,128});
    check(PreparedNativeMoments({{"identity",identity}},{{0,0,unit}}).execute(sparse,{{904,7}},runtime::ExecutionLease(tiny)).values==Vector{9},"huge sparse named-unit moment has no ambient matrix");
    reject([&]{sparse.values();});
    runtime::ExecutionHostPolicy sparse_ensemble_policy=tiny;sparse_ensemble_policy.limits.rational_slots=128;
    sparse_ensemble_policy.limits.sparse_entries=128;sparse_ensemble_policy.limits.scalar_work=8192;
    numeric::ExactActionGuard sparse_guard({huge,128,8192,128});
    const auto huge_ensemble=PreparedNativeEnsemble::prepare(unit,{sparse,sparse.scaled(-1,sparse_guard)},{1,1},{{904,7}},runtime::ExecutionLease(sparse_ensemble_policy));
    const auto huge_moments=huge_ensemble.contractions({{904,7}},runtime::ExecutionLease(sparse_ensemble_policy));
    check(huge_moments.moments.variance==9 && huge_moments.moments.second_contraction==81 && huge_moments.moments.dimension==1 &&
        huge_moments.covariance_rank==1,"huge native ensemble preserves the logical carrier and reduces only supported rows");
    std::cout<<"native selected moments: "<<checks<<" exact checks passed\n";
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
