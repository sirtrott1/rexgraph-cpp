#include "../../examples/native_frames/fixture.hpp"
#include "rfc/inference/native_multitime_moments.hpp"
#include <iostream>
using namespace rfc;
using namespace native_frame_example;
using namespace inference;
std::size_t checks{};
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"required multitime rejection");}
Q dot(const Vector& a,const Vector& b){Q out=0;for(std::size_t i=0;i<a.size();++i)out+=a[i]*b[i];return out;}
Vector add(Vector a,const Vector& b){for(std::size_t i=0;i<a.size();++i)a[i]+=b[i];return a;}
Vector subtract(Vector a,const Vector& b){for(std::size_t i=0;i<a.size();++i)a[i]-=b[i];return a;}
int main(){try{
    auto e=endpoint(pair(),2),future=endpoint(pair(),2,9);
    e.carriers.push_back({20,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},1020,numeric::IndexDomain::linear(3)});
    e.carriers.push_back({21,storage::TemporalCarrierKind::Explicit,core::Grade{0},{},1021,numeric::IndexDomain::linear(1)});
    const Matrix F0=Matrix::from_rows({{1,2},{0,1},{-1,3}}),F1=Matrix::from_rows({{2,-1,3}}),
        B0=Matrix::from_rows({{1},{2},{-1}}),B1=Matrix::from_rows({{3,-2}}),A=Matrix::from_rows({{Q{3,2}}}),
        Jright=Matrix::from_rows({{1},{2}}),O=Matrix::from_rows({{1,2},{-1,3}});
    e.tensors.push_back(native_update_example::map(800,10,20,F0));
    e.tensors.push_back(native_update_example::map(801,20,21,F1));
    e.tensors.push_back(native_update_example::map(802,21,20,B0));
    e.tensors.push_back(native_update_example::map(803,10,21,B1));
    e.tensors.push_back(native_update_example::map(804,21,10,Jright));
    e.tensors.push_back(native_update_example::map(805,21,21,A));
    e.tensors.push_back(native_update_example::map(806,10,10,O));
    e.tensors.push_back(native_update_example::field(900,10,{1,2}));
    e.tensors.push_back(native_update_example::field(901,21,{3}));
    e.tensors.push_back(native_update_example::field(902,10,{2,-1}));
    e.tensors.push_back(native_update_example::field(903,20,{1,4,-2}));
    future.tensors.push_back(native_update_example::map(806,10,10,O));
    storage::NativeAccessionDirectory directory(e,900),later(future,901),foreign(e,900);
    const NativeActionAvailability available{{900,7}};
    const auto field=[](const auto& d,std::uint64_t key){return NativeFieldValue::stored(d.pin(access(storage::NativeAccessionKind::Field,key)));};
    const auto operation=[](const auto& d,std::uint64_t key){return NativeFactoredAction::tensor(d.pin(access(storage::NativeAccessionKind::Operator,key)));};
    const auto x=field(directory,900),u0=field(directory,901),u1=field(directory,902),next_left=field(directory,903);
    const auto f0=operation(directory,800),f1=operation(directory,801),b0=operation(directory,802),b1=operation(directory,803),a=operation(directory,805);
    const auto future_operation=operation(later,806);
    const auto plan=PreparedNativeMultitimeObservations::prepare({f0,f1,future_operation},{b0,b1,future_operation},2,
        NativeFactoredAction::identity(x.carrier()),a);
    const auto result=plan.execute({x,u0,u1},available);
    const auto expected=A.apply(add(F1.apply(add(F0.apply(x.values()),B0.apply(u0.values()))),B1.apply(u1.values())));
    const std::vector<Vector> terms{A.apply(F1.apply(F0.apply(x.values()))),A.apply(F1.apply(B0.apply(u0.values()))),A.apply(B1.apply(u1.values()))};
    check(result.aggregate.values()==expected && result.channels.size()==3 && plan.terminal()==2,"changing-carrier terminal recurrence uses only the selected prefix");
    for(std::size_t i=0;i<terms.size();++i)check(result.channels[i].values()==terms[i],"each transported innovation agrees with the independent recurrence");
    check(result.aggregate.inputs().size()==3 && result.plan_owner && result.aggregate.retained_action_owners()>0,"multitime result retains every actual input and action owner");
    const auto selected=plan.execute_selected({x,u0,u1},{0,0},available);
    check(selected.aggregate==Vector{expected[0],expected[0]} && selected.plan_owner && selected.inputs.size()==3,"selected multitime readouts preserve duplicate selections");
    for(std::size_t i=0;i<terms.size();++i)check(selected.channel_values[i]==Vector{terms[i][0],terms[i][0]},"selected per-innovation readouts");
    const auto unit=NativeBilinearForm::named_unit(a.output());
    const auto moments=plan.moments({{0,0,unit},{0,1,unit},{1,2,unit}}).execute({x,u0,u1},available);
    check(moments.values==Vector{dot(terms[0],terms[0]),dot(terms[0],terms[1]),dot(terms[1],terms[2])},"selected multitime cross moments retain shared suffix factors");
    const auto zero=PreparedNativeMultitimeObservations::prepare({future_operation},{future_operation},0,
        NativeFactoredAction::identity(x.carrier()),NativeFactoredAction::identity(x.carrier()));
    check(zero.execute({x},available).aggregate.values()==x.values(),"empty prefix ignores all future maps");
    reject([&]{PreparedNativeMultitimeObservations::prepare({f0,f1,future_operation},{b0,b1,future_operation},3,NativeFactoredAction::identity(x.carrier()),a);});
    reject([&]{plan.execute({x,u0},available);});
    reject([&]{plan.execute({field(foreign,900),u0,u1},available);});
    reject([&]{plan.execute_selected({x,u0,u1},{1},available);});
    reject([&]{plan.execute({x,u0,u1},{{900,6}});});
    runtime::ExecutionHostPolicy low;low.limits.scalar_work=1;
    reject([&]{plan.execute_selected({x,u0,u1},{0},available,runtime::ExecutionLease(low));});
    const Matrix old_form=Matrix::from_rows({{2},{-1}}),new_form=Matrix::from_rows({{2,1},{-1,3},{1,-2}});
    const auto old_pair=NativeBilinearForm::declared_cross(x.carrier(),u0.carrier(),old_form),
        new_pair=NativeBilinearForm::declared_cross(next_left.carrier(),u1.carrier(),new_form);
    const auto change=PreparedNativeMomentChange::prepare(old_pair,new_pair,f0,operation(directory,804));
    for(long i=-2;i<=2;++i)for(long j=-2;j<=2;++j){
        numeric::ExactActionGuard guard;const auto ox=x.scaled(Q{i,3},guard),oy=u0.scaled(Q{j,2},guard);
        const auto result_change=change.execute(ox,oy,next_left,u1,available);
        const auto tx=F0.apply(ox.values()),ty=Jright.apply(oy.values());
        const auto dx=subtract(next_left.values(),tx),dy=subtract(u1.values(),ty);
        const auto before=dot(ox.values(),old_form.apply(oy.values())),after=dot(next_left.values(),new_form.apply(u1.values()));
        check(result_change.old_moment==before && result_change.new_moment==after && result_change.direct==after-before && result_change.expanded==after-before,"exact typed four-term change against an independent coordinate oracle");
        check(result_change.metric_term==dot(tx,new_form.apply(ty))-before && result_change.left_term==dot(dx,new_form.apply(ty)) &&
            result_change.right_term==dot(tx,new_form.apply(dy)) && result_change.mixed_term==dot(dx,new_form.apply(dy)),"each nonsymmetric cross-form moment-change term");
        check(result_change.transported_left.values()==tx && result_change.transported_right.values()==ty && result_change.plan_owner,"moment-change transport and owner receipt");
    }
    reject([&]{PreparedNativeMomentChange::prepare(old_pair,new_pair,f1,operation(directory,804));});
    reject([&]{change.execute(x,u0,next_left,u1,{{900,6}});});
    reject([&]{change.execute(field(foreign,900),u0,next_left,u1,available);});
    const auto op=operation(directory,806);
    for(std::size_t degree=0;degree<7;++degree){
        Vector coefficients,power=x.values(),polynomial(2);Q factorial=1;
        for(std::size_t i=0;i<=degree;++i){Q coefficient{static_cast<long>(i)-2,static_cast<unsigned long>(i+1)};coefficient.canonicalize();coefficients.push_back(coefficient);
            for(std::size_t j=0;j<2;++j)polynomial[j]+=coefficient*power[j];
            if(i<degree)power=O.apply(power);}
        for(std::size_t i=1;i<=degree;++i)factorial*=Q{static_cast<unsigned long>(i)};
        check(native_polynomial(op,coefficients).execute(x,available).values()==polynomial,"finite native polynomial exact Horner action");
        for(auto& value:power)value/=factorial;
        check(native_exponential_series_coefficient(op,degree).execute(x,available).values()==power,"finite exact exponential formal-series coefficient");
    }
    check(native_polynomial(op,{}).execute(x,available).sparse_values().nonzeros()==0,"zero polynomial has exact sparse output");
    reject([&]{native_polynomial(f0,{1,2});});
    reject([&]{native_exponential_series_coefficient(op,1000,{4096,100,3,128});});
    std::cout<<"native multitime moments: "<<checks<<" exact checks passed\n";
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
