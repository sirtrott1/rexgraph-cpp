#include "../../examples/action_moment_data.hpp"
#include <future>
#include <iostream>
using namespace action_moment_example;
std::size_t checks{};
void check(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"required rejection");}
int main(){try {
    const auto endpoint=make(); storage::NativeAccessionDirectory d(endpoint,900), foreign(endpoint,900);
    const auto x=input(d), fx=input(foreign); const NativeActionAvailability avail{{900,7}};
    const auto id=NativeFactoredAction::identity(x.carrier()); const auto m=unit(x.carrier());
    const std::vector<NativeActionInteractionRequest> pairs{{0,1},{0,2},{1,2}};
    // Independent scalar reference for signed interactions and exact cancellation.
    for(int a=-3;a<=3;++a)for(int b=-3;b<=3;++b)for(int c=-3;c<=3;++c){
        PreparedNativeActionMoments plan({{"A",id.scaled(a)},{"B",id.scaled(b)},{"C",id.scaled(c)}},m,id.scaled(a+b+c));
        const auto result=plan.execute(x,avail,pairs);
        check(result.total==Q{(a+b+c)*(a+b+c)},"aggregate quadrance");
        check(result.diagonal_sum==Q{a*a+b*b+c*c},"individual quadrances");
        check(result.cross_sum==Q{2*(a*b+a*c+b*c)},"all signed cross terms");
        check(result.unreported_cross_sum==0,"complete pair list");
        check(result.interactions[0].pairing==Q{a*b} && result.interactions[1].pairing==Q{a*c} && result.interactions[2].pairing==Q{b*c},"selected pairings");
        check(result.components[2].increment==Q{c*c+2*c*(a+b)},"conditional increment");
        check(result.matches_reference==true && result.residual->sparse_values().nonzeros()==0,"exact field agreement");
        const auto selected=plan.execute(x,avail,std::span(pairs).first(1));
        check(selected.unreported_cross_sum==Q{2*(a*c+b*c)},"unrequested interaction retained in total");
    }
    const auto p=plans(d);
    const auto before=p.old_third.execute(x,avail),after=p.new_third.execute(x,avail,pairs);
    check(before.total==4 && after.total==5,"bridge changes third moment");
    check(after.components[0].self==4 && after.components[1].self==0 && after.components[2].self==1,"local attribution 4,0,1");
    check(after.cross_sum==0 && after.matches_reference==true,"grade-one channels are disjoint and reconstruct");
    for(std::size_t order=1;order<=3;++order){
        const auto a=PreparedNativeMoments({{"x",id},{"old",native_power(p.old_laplacian,order)},{"new",native_power(p.new_laplacian,order)}},{{0,1,m},{0,2,m}}).execute(x,avail);
        check(a.values[0]==Q{order==1?1:order==2?2:4} && a.values[1]==Q{order==1?1:order==2?2:5},"actual power moment reference");
    }
    // All assembly permutations retain total; prefix effects retain declared order.
    std::array<std::size_t,3> order{0,1,2};
    do {auto channels=p.new_third.channels();std::vector<NativeMomentChannel> reordered;
        for(auto i:order)reordered.push_back(channels[i]);
        auto result=PreparedNativeActionMoments(reordered,p.new_third.metric()).execute(x,avail);
        check(result.total==5 && !result.matches_reference.has_value(),"order-independent complete sum; no invented certificate");
    }while(std::next_permutation(order.begin(),order.end()));
    // Same quadrance is not equality of the decomposed action.
    auto wrong=PreparedNativeActionMoments({{"x",id}},m,id.scaled(-1)).execute(x,avail);
    check(wrong.total==1 && wrong.matches_reference==false && wrong.residual->sparse_values().entries()[0].value==2,"nonzero field witness despite equal moments");
    // Build ordered operation-defect channels using the existing finite word rule.
    const auto word=PreparedNativeWordDefect::prepare({id.scaled(2),id.scaled(3)},{id.scaled(4),id.scaled(-1)},{id,id,id});
    std::vector<NativeMomentChannel> terms;for(std::size_t i=0;i<word.contributions().size();++i)terms.push_back({"position "+std::to_string(i),word.contributions()[i]});
    const auto delta=PreparedNativeActionMoments(terms,m,word.direct_action()).execute(x,avail);
    check(delta.matches_reference==true && delta.total==100 && delta.diagonal_sum==68 && delta.cross_sum==32,"finite word defects feed actual moment decomposition");
    // A genuine non-diagonal target metric is used by every measured channel.
    auto weighted=endpoint;
    Matrix form=Matrix::identity(4);form.set(0,0,2);form.set(1,1,3);form.set(0,1,1);form.set(1,0,1);
    storage::TemporalTensor mt;mt.key=500;mt.role=storage::TemporalTensorRole::Metric;mt.input=10;mt.output=10;
    mt.tensor=storage::temporal_matrix_tensor(form);weighted.tensors.push_back(mt);
    storage::NativeAccessionDirectory wd(weighted,902);const auto wx=input(wd);const auto wp=plans(wd);
    const auto wi=NativeFactoredAction::identity(wx.carrier());
    const auto wm=NativeBilinearForm::tensor(wd.pin(access(storage::NativeAccessionKind::Tensor,500)));
    const auto wr=PreparedNativeActionMoments({{"action",wp.new_laplacian},{"input",wi}},wm,wp.new_laplacian.plus(wi)).execute(wx,{{902,7}});
    check(wr.total==7 && wr.diagonal_sum==5 && wr.cross_sum==2 && wr.matches_reference==true,"non-diagonal declared pairing");
    runtime::ExecutionHostPolicy small_bits;small_bits.limits.integer_bits=2;
    reject([&]{p.new_third.execute(x,avail,{},runtime::ExecutionLease(small_bits));});
    // Invalid type/owner/direction and request metadata never become moment results.
    reject([&]{p.new_third.execute(fx,avail);});
    reject([&]{PreparedNativeActionMoments({{"x",id},{"foreign",NativeFactoredAction::identity(fx.carrier())}},m);});
    reject([&]{PreparedNativeActionMoments({{"x",id}},unit(fx.carrier()));});
    reject([&]{PreparedNativeActionMoments({{"x",id}},unit(read(d,200).output()));});
    reject([&]{PreparedNativeActionMoments({{"x",id},{"x",id}},m);});
    reject([&]{PreparedNativeActionMoments({{"",id}},m);});
    reject([&]{PreparedNativeActionMoments({},m);});
    reject([&]{PreparedNativeActionMoments({{"x",id}},m,{},NativeActionMomentLimits{0,0,0});});
    reject([&]{PreparedNativeActionMoments({{"long",id}},m,{},NativeActionMomentLimits{1,1,1});});
    reject([&]{PreparedNativeActionMoments({{"x",id}},m,NativeFactoredAction::identity(fx.carrier()));});
    reject([&]{PreparedNativeActionMoments({{"x",id}},NativeBilinearForm::declared_cross(x.carrier(),x.carrier(),Matrix::identity(4)));});
    for(auto bad:std::vector<std::vector<NativeActionInteractionRequest>>{{{0,0}},{{1,0}},{{0,3}},{{0,1},{0,1}}})reject([&]{p.new_third.execute(x,avail,bad);});
    reject([&]{PreparedNativeActionMoments({{"a",id},{"b",id}},m,{},NativeActionMomentLimits{2,0,10}).execute(x,avail,std::span(pairs).first(1));});
    reject([&]{p.new_third.execute(x,{});});reject([&]{p.new_third.execute(x,{{900,6}});});reject([&]{p.new_third.execute(x,{{900,7},{900,8}});});
    auto flag=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease cancelled({},flag);
    reject([&]{p.new_third.execute(x,avail,{},cancelled);});flag->store(false);
    check(p.new_third.execute(x,avail,{},cancelled).total==5,"reuse after cancellation");
    for(unsigned which=0;which<4;++which){runtime::ExecutionHostPolicy h;
        if(which==0)h.limits.scalar_work=1;
        if(which==1)h.limits.rational_slots=1;
        if(which==2)h.limits.sparse_entries=1;
        if(which==3)h.allow_allocation=false;
        reject([&]{p.new_third.execute(x,avail,{},runtime::ExecutionLease(h));});}
    auto future=std::async(std::launch::async,[p,x,avail]{return p.new_third.execute(x,avail).total;});
    check(p.new_third.execute(x,avail).total==future.get(),"parallel independent invocations");
    // Strong lifetime: returned fields retain owners after all preparations die.
    auto retained=[&]{const auto tmp=plans(d);return tmp.new_third.execute(x,avail);}();
    check(retained.plan_owner && retained.aggregate.retained_action_owners()>0 && retained.components[2].field.publications().size()>1,"result owns provenance");
    // Source/target logical extent changes alone must not cause dense field work.
    std::size_t steps=0,slots=0;
    for(const std::size_t n:{std::size_t{7},std::size_t{1000003},std::size_t{1000000000000000000ull}}){
        const auto huge=make(n);
        storage::NativeAccessionDirectory hd(huge,901);
        const auto hx=input(hd); const auto hp=NativeFactoredAction::identity(hx.carrier(),n);
        PreparedNativeActionMoments huge_plan({{"positive",hp},{"cancel",hp.scaled(-1)},{"double",hp.scaled(2)}},unit(hx.carrier(),n),hp.scaled(2));
        runtime::ExecutionHostPolicy host;host.limits.rational_slots=128;host.limits.sparse_entries=128;
        const auto value=huge_plan.execute(hx,{{901,7}},pairs,runtime::ExecutionLease(host));
        check(value.total==4 && value.matches_reference==true,"large full-support sparse field");
        if(steps)check(value.execution_steps==steps && value.retained_sparse_entries==slots,"work independent of unused extent");
        steps=value.execution_steps;slots=value.retained_sparse_entries;
    }
    std::cout<<"native_action_moments checks="<<checks<<" steps="<<steps<<" retained_entries="<<slots<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
