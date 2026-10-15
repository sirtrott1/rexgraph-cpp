#include "../../examples/native_metric_novelty/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace rfc;
using namespace storage;
using namespace native_metric_example;
using Plan=inference::PreparedNativeMetricNovelty;
using Numeric=field::PreparedMetricNovelty;
std::size_t checks{},cases{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F action,const char* message){
    bool caught=false;try{action();}catch(const Error&){caught=true;}check(caught,message);
}
void json(const Vector& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void json(const Matrix& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';Vector row(a.cols());
    for(std::size_t j=0;j<a.cols();++j)row[j]=a.get(i,j);
    json(row);}std::cout<<']';}
Matrix correspondence(std::size_t n,std::size_t m,unsigned family) {
    Matrix j(m,n);
    for(std::size_t i=0;i<n;++i){j.set(i,i,family==1?Q{static_cast<unsigned long>(i+2)}:Q{1});
        if(family==2 && i+1<n)j.set(i,i+1,Q{static_cast<long>(i+1),3});}
    for(std::size_t i=n;i<m;++i)for(std::size_t k=0;k<n;++k)
        j.set(i,k,Q{static_cast<long>((i+2*k+family)%7)-3,static_cast<unsigned long>(k+2)});
    return j;
}
Matrix positive_metric(std::size_t m,unsigned family) {
    Matrix value(m,m);for(std::size_t i=0;i<m;++i){value.set(i,i,family==0?Q{1}:Q{static_cast<unsigned long>(i+2)});
        if(family==2 && i){value.set(i-1,i,-1);value.set(i,i-1,-1);}}
    return value;
}
void sample(const Matrix& J,const Matrix& M,unsigned sample_id,bool emit) {
    Vector old(J.cols()),next(J.rows()),first(J.rows());
    for(std::size_t i=0;i<old.size();++i){old[i]=Q{static_cast<long>(i+sample_id)-2,static_cast<unsigned long>(i+2)};old[i].canonicalize();}
    for(std::size_t i=0;i<next.size();++i){next[i]=Q{static_cast<long>(2*i+sample_id)-3,static_cast<unsigned long>(i+3)};next[i].canonicalize();
        first[i]=Q{static_cast<long>(i+sample_id+1),static_cast<unsigned long>(i+4)};first[i].canonicalize();}
    const auto delta=numeric::sub(next,J.apply(old));const auto negative=numeric::scale(Q{-1},first),remaining=numeric::sub(delta,first);
    NativeAccessionDirectory a(endpoint(Matrix::identity(old.size()),old),900),b(endpoint(M,next,8,{first,negative,remaining,Vector(next.size())}),901);
    const auto old_bytes=encode_temporal_endpoint(*input(a).endpoint()),new_bytes=encode_temporal_endpoint(*input(b).endpoint());
    const auto plan=Plan::prepare(request(a,b,J),available);
    const std::vector<Pin> cause_inputs{input(b,200),input(b,200),input(b,201),input(b,202),input(b,203)};
    const auto result=plan.execute_causes(input(a),input(b),cause_inputs,available);const auto& v=result.values();const auto& c=*result.cause_pairings();
    check(v.innovation==delta && v.reconstructed==delta,"complete innovation reconstruction");
    check(v.persistent==J.apply(v.persistent_coordinates) && numeric::add(v.persistent,v.complement)==delta,"persistent realization and complete complement");
    check(J.transpose().apply(M.apply(v.complement))==Vector(old.size()),"whole new complement is metric orthogonal to old image");
    check(v.pairings.get(0,1)==0 && v.pairings.get(1,0)==0 && v.pairings.get(0,0)+v.pairings.get(1,1)==v.quadrance,"orthogonal novelty budget");
    check(v.quadrance==numeric::dot(delta,M.apply(delta)) && c.quadrance==v.quadrance && c.aggregate==delta,"full cause and novelty contractions");
    check(c.channels==std::vector<Vector>{first,first,negative,remaining,Vector(next.size())} && c.pairings.rows()==5,"ordered repeated cancelling zero causes retained");
    for(std::size_t i=0;i<c.channels.size();++i)for(std::size_t k=0;k<c.channels.size();++k)
        check(c.pairings.get(i,k)==numeric::dot(c.channels[i],M.apply(c.channels[k])),"every off-diagonal cause pairing");
    const auto& p=result.provenance();
    check(p.old_endpoint.source_namespace==900 && p.new_endpoint.source_namespace==901 && p.old_endpoint.sequence==7 && p.new_endpoint.sequence==8 &&
          p.old_endpoint.carrier==10 && p.new_endpoint.carrier==10 && p.new_metric==1 && p.cause_fields==std::vector<std::uint64_t>{200,200,201,202,203} &&
          p.declared_causes && p.old_endpoint.input_field==100 && p.new_endpoint.input_field==100,"actual endpoint and ordered cause witnesses");
    check(result.old_input().tensor_declaration().get()==input(a).tensor_declaration().get() &&
          result.cause_inputs()[0].tensor_declaration().get()==input(b,200).tensor_declaration().get(),"actual field owners retained");
    check(plan.numeric_plan().correspondence()==J && plan.numeric_plan().metric()==M &&
          plan.numeric_plan().pulled_back_metric()==J.transpose().multiply(M).multiply(J),"complete declared comparison and metric retained");
    check(encode_temporal_endpoint(*input(a).endpoint())==old_bytes && encode_temporal_endpoint(*input(b).endpoint())==new_bytes,"native canonical source bytes unchanged");
    if(emit){std::cout<<"{\"n\":"<<J.cols()<<",\"m\":"<<J.rows()<<",\"J\":";json(J);std::cout<<",\"M\":";json(M);
        std::cout<<",\"old\":";json(old);std::cout<<",\"new\":";json(next);std::cout<<",\"innovation\":";json(v.innovation);
        std::cout<<",\"coordinates\":";json(v.persistent_coordinates);std::cout<<",\"persistent\":";json(v.persistent);
        std::cout<<",\"complement\":";json(v.complement);std::cout<<",\"novelty_gram\":";json(v.pairings);
        std::cout<<",\"causes\":[";for(std::size_t i=0;i<c.channels.size();++i){if(i)std::cout<<',';json(c.channels[i]);}
        std::cout<<"],\"cause_gram\":";json(c.pairings);std::cout<<",\"quadrance\":\""<<v.quadrance<<"\"}\n";}
    ++cases;
}
void corpus(bool emit) {
    for(const std::size_t n:std::array<std::size_t,6>{0,1,2,3,4,5})for(const std::size_t extra:std::array<std::size_t,3>{0,1,3})
        for(unsigned j=0;j<3;++j)for(unsigned m=0;m<3;++m)for(unsigned sample_id=0;sample_id<2;++sample_id)
            sample(correspondence(n,n+extra,j),positive_metric(n+extra,m),sample_id,emit);
}
void numeric_contracts() {
    const auto J=Matrix::from_rows({{1,0},{0,1},{1,1}}),M=Matrix::from_rows({{2,0,0},{0,3,0},{0,0,4}});
    const Numeric plan(J,M);const auto value=plan.decompose({2,3},{3,2,8});
    check(value.persistent==Vector{Q{31,13},Q{-1,13},Q{30,13}} && value.complement==Vector{Q{-18,13},Q{-12,13},Q{9,13}},"canonical rectangular paper values");
    check(value.pairings.get(0,0)==Q{425,13} && value.pairings.get(1,1)==Q{108,13} && value.quadrance==41,"canonical rectangular paper quadrances");
    for(std::size_t j=0;j<2;++j){Vector axis(2);axis[j]=1;check(plan.left_inverse(J.apply(axis))==axis,"full left-inverse identity");}
    check(plan.left_inverse({1,0,0})==Vector{Q{7,13},Q{-4,13}},"left inverse is distinct from unsupplied old-metric adjoint");
    const Numeric identity(Matrix::identity(4),Matrix::identity(4));
    const auto causes=identity.pair_causes({{-2,-2,-2,-2},{0,0,-1,-1}},{-2,-2,-3,-3});
    check(causes.pairings==Matrix::from_rows({{16,4},{4,2}}) && causes.quadrance==26,"canonical cross terms cannot be dropped");
    const auto cancelling=identity.pair_causes({{1,2,3,4},{-1,-2,-3,-4}},Vector(4));
    check(cancelling.quadrance==0 && cancelling.pairings.get(0,0)==30 && cancelling.pairings.get(0,1)==-30,"zero aggregate retains nonzero cancelling channels");
    check(identity.pair_causes({},Vector(4)).pairings.rows()==0,"explicit empty causes for zero aggregate");
    rejects<std::invalid_argument>([&]{identity.pair_causes({},Vector{1,0,0,0});},"empty causes do not explain nonzero aggregate");
    rejects<std::invalid_argument>([&]{identity.pair_causes({Vector(3)},Vector(4));},"cause dimensions");
    rejects<std::invalid_argument>([&]{plan.decompose({1},{1,2,3});},"complete old field dimensions");
    rejects<std::invalid_argument>([&]{plan.left_inverse({1,2});},"complete new field dimensions");
    rejects<std::invalid_argument>([&]{Numeric(Matrix::from_rows({{1,1},{2,2},{3,3}}),M);},"rank-losing comparison rejected");
    rejects<std::invalid_argument>([&]{Numeric(Matrix(2,3),Matrix::identity(2));},"wide comparison cannot be injective");
    rejects<std::invalid_argument>([&]{Numeric(J,Matrix::identity(2));},"metric dimensions");
    rejects<std::invalid_argument>([&]{Numeric(J,Matrix::from_rows({{2,1,0},{0,3,0},{0,0,4}}));},"asymmetric metric rejected");
    rejects<std::invalid_argument>([&]{Numeric(Matrix::from_rows({{1},{0},{0}}),Matrix::from_rows({{1,0,0},{0,1,0},{0,0,-1}}));},"negative unrepresented direction is checked");
    rejects<std::invalid_argument>([&]{Numeric(Matrix::from_rows({{1},{0},{0}}),Matrix::from_rows({{1,0,0},{0,1,0},{0,0,0}}));},"singular unrepresented direction is checked");
    const Numeric empty(Matrix(0,0),Matrix(0,0)),birth(Matrix(3,0),M);
    check(empty.decompose({},{}).quadrance==0 && birth.decompose({},{3,2,8}).complement==Vector{3,2,8},"zero old carrier and full new directions");
    numeric::ExactActionLimits one_axis;one_axis.dimension=1;
    check(Numeric(Matrix::identity(1),Matrix::identity(1)).left_inverse({2},one_axis)==Vector{2},"left-inverse-only output does not request split channel axes");
    numeric::ExactActionLimits limits;limits.entries=plan.stored_entries()+plan.workspace_slots()-1;
    rejects<std::length_error>([&]{plan.decompose({2,3},{3,2,8},limits);},"whole action scalar envelope");
    limits={};limits.integer_bits=0;rejects<std::length_error>([&]{empty.decompose({},{},limits);},"empty action still validates exact rational constants");
    limits={};limits.integer_bits=2;rejects<std::length_error>([&]{plan.decompose({0,0},{0,0,0},limits);},"unused retained Gram growth checked on zero field");
    limits={};limits.dimension=2;rejects<std::length_error>([&]{plan.decompose({0,0},{0,0,0},limits);},"complete new dimensions");
    limits={};limits.steps=0;rejects<std::length_error>([&]{plan.decompose({0,0},{0,0,0},limits);},"current work bound");
    numeric::ExactActionGuard measured;(void)plan.decompose({2,3},{3,2,8},measured);limits={};limits.steps=measured.steps()-1;
    rejects<std::length_error>([&]{plan.decompose({2,3},{3,2,8},limits);},"exact work boundary");
    auto handle=plan;Vector old{2,3},next{3,2,8};bool replaced=false;
    const auto captured=handle.decompose(old,next,{},[&]{if(!replaced){replaced=true;handle=empty;old.clear();next.clear();}});
    check(replaced && captured.persistent==value.persistent,"plan and complete input arguments owned before callback");
    auto j=J,m=M;replaced=false;const Numeric captured_plan(j,m,{},[&]{if(!replaced){replaced=true;j=Matrix(0,0);m=Matrix(0,0);}});
    check(captured_plan.decompose({2,3},{3,2,8}).complement==value.complement,"complete preparation arguments owned before callback");
    handle=identity;std::vector<Vector> channel_args{{1,2,3,4},{-1,-2,-3,-4}};Vector expected(4);replaced=false;
    const auto captured_causes=handle.pair_causes(channel_args,expected,{},[&]{if(!replaced){replaced=true;handle=empty;channel_args.clear();expected.clear();}});
    check(captured_causes.pairings==cancelling.pairings,"ordered cause arguments and plan captured before callback");
    rejects<runtime::ExecutionCancelled>([&]{plan.decompose({2,3},{3,2,8},{},[]{throw runtime::ExecutionCancelled();});},"numeric owned cancellation");
    check(plan.decompose({2,3},{3,2,8}).quadrance==41,"cancellation preserves immutable plan");
}
void contracts() {
    const auto J=Matrix::from_rows({{1,0},{0,1},{1,1}}),M=Matrix::from_rows({{2,0,0},{0,3,0},{0,0,4}});
    const auto old_e=endpoint(Matrix::identity(2),{2,3}),new_e=endpoint(M,{3,2,8},8,{{1,-1,3}});
    NativeAccessionDirectory old(old_e,900),next(new_e,901),clone(new_e,901);
    const auto req=request(old,next,J);const auto plan=Plan::prepare(req,available);
    rejects<std::invalid_argument>([&]{auto r=req;r.new_metric=metric(clone);Plan::prepare(r,available);},"equal native bytes do not identify metric owner");
    rejects<std::invalid_argument>([&]{plan.execute(input(old),input(clone),available);},"equal native bytes do not identify field owner");
    rejects<std::invalid_argument>([&]{plan.execute_causes(input(old),input(next),{input(clone,200)},available);},"equal-key cause publication rejected");
    rejects<std::invalid_argument>([&]{plan.execute(input(old,102),input(next),available);},"equal dimensions do not identify old carrier");
    rejects<std::invalid_argument>([&]{plan.execute_causes(input(old),input(next),{input(next,102)},available);},"equal dimensions do not identify cause carrier");
    rejects<std::invalid_argument>([&]{plan.execute(old.pin(access(NativeAccessionKind::Tensor,100)),input(next),available);},"tensor view is not stored Field");
    rejects<std::invalid_argument>([&]{plan.execute_causes(input(old),input(next),{},available);},"explicit empty native causes require zero innovation");
    rejects<std::invalid_argument>([&]{plan.execute_causes(input(old),input(next),{input(next,101)},available);},"native cause sum certified exactly");
    for(unsigned side=0;side<2;++side)for(unsigned kind=0;kind<2;++kind){auto cutoff=available;
        auto& changed=side?cutoff.new_endpoint:cutoff.old_endpoint;if(kind)changed.source_namespace=999;else --changed.max_sequence;
        rejects<std::invalid_argument>([&]{Plan::prepare(req,cutoff);},"preparation has both exact cutoffs");
        rejects<std::invalid_argument>([&]{plan.execute(input(old),input(next),cutoff);},"execution has both exact cutoffs");}
    auto r=req;r.dimension_limit=0;rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"positive dimension limit");
    r=req;r.dimension_limit=2;rejects<std::length_error>([&]{Plan::prepare(r,available);},"new directions bounded");
    NativeAccessionDirectory single_old(endpoint(Matrix::identity(1),{1}),900),single_new(endpoint(Matrix::identity(1),{2},8),901);
    r=request(single_old,single_new,Matrix::identity(1));r.dimension_limit=1;
    rejects<std::length_error>([&]{Plan::prepare(r,available);},"complete split channel axes are bounded at preparation");
    r=req;r.old_carrier=30;rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"explicit carrier is not SourceGrade");
    r=req;r.old_carrier=999;rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"missing actual carrier");
    r=req;r.new_metric=input(next);rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"field is not a bilinear metric");
    r=req;r.comparison=storage::temporal_matrix_tensor(Matrix(3,1));rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"comparison dimensions bind actual carriers");
    r=req;r.comparison={numeric::IndexDomain({3,2}),numeric::StructuralSupport::sparse(6,{}),numeric::SparseRationalVector(6,{})};
    rejects<std::invalid_argument>([&]{Plan::prepare(r,available);},"missing comparison support is not supported zero");
    auto e=new_e;e.tensors[0].input=30;e.tensors[0].output=30;NativeAccessionDirectory foreign_metric(e,901);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(old,foreign_metric,J),available);},"metric binds its actual carrier");
    e=new_e;e.tensors[0].role=TemporalTensorRole::SourceRealization;NativeAccessionDirectory role(e,901);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(old,role,J),available);},"realization does not imply metric");
    e=new_e;e.tensors[0].tensor={numeric::IndexDomain({3,3}),numeric::StructuralSupport::sparse(9,{0,4,8}),
        numeric::SparseRationalVector(9,{{0,2},{4,3},{8,4}})};
    NativeAccessionDirectory absent(e,901);rejects<std::invalid_argument>([&]{Plan::prepare(request(old,absent,J),available);},"absent metric directions rejected");
    e=new_e;e.tensors[1].tensor={numeric::IndexDomain::linear(3),numeric::StructuralSupport::sparse(3,{0}),numeric::SparseRationalVector(3,{})};
    NativeAccessionDirectory partial(e,901);const auto partial_plan=Plan::prepare(request(old,partial,J),available);
    rejects<std::invalid_argument>([&]{partial_plan.execute(input(old),input(partial),available);},"partial field support rejected");
    for(unsigned resource=0;resource<5;++resource){runtime::ExecutionHostPolicy p;
        switch(resource){case 0:p.limits.records=0;break;case 1:p.limits.rational_slots=0;break;case 2:p.limits.sparse_entries=0;break;
            case 3:p.limits.scalar_work=0;break;default:p.limits.integer_bits=0;break;}
        rejects<std::length_error>([&]{Plan::prepare(req,available,runtime::ExecutionLease{p});},"preparation host resource bound");
        rejects<std::length_error>([&]{plan.execute(input(old),input(next),available,runtime::ExecutionLease{p});},"current host resource bound");}
    const auto measured=plan.execute_causes(input(old),input(next),{input(next,200)},available).provenance();runtime::ExecutionHostPolicy p;
    p.limits.records=measured.selected_requirements.records;p.limits.rational_slots=measured.selected_requirements.rational_slots;
    p.limits.sparse_entries=measured.selected_requirements.sparse_entries;
    check(plan.execute_causes(input(old),input(next),{input(next,200)},available,runtime::ExecutionLease{p}).values().quadrance==41,"exact scoped container envelope");
    --p.limits.rational_slots;rejects<std::length_error>([&]{plan.execute_causes(input(old),input(next),{input(next,200)},available,runtime::ExecutionLease{p});},"scalar envelope boundary");
    p={};p.limits.scalar_work=measured.execution_steps-1;
    rejects<std::length_error>([&]{plan.execute_causes(input(old),input(next),{input(next,200)},available,runtime::ExecutionLease{p});},"native complete work boundary");
    p={};p.allow_allocation=false;rejects<std::invalid_argument>([&]{plan.execute(input(old),input(next),available,runtime::ExecutionLease{p});},"owning allocation required");
    p={};p.backend=runtime::ExecutionBackend::ExactGpu;rejects<std::invalid_argument>([&]{Plan::prepare(req,available,runtime::ExecutionLease{p});},"unregistered rational GPU solver rejected");
    p.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(Plan::prepare(req,available,runtime::ExecutionLease{p}).execute(input(old),input(next),available).provenance().fallback,"explicit fallback witnessed");
    auto cancelled=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease lease({},cancelled);
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare(req,available,lease);},"owned preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{plan.execute(input(old),input(next),available,lease);},"owned action cancellation");
    cancelled->store(false);check(plan.execute(input(old),input(next),available,lease).values().quadrance==41,"cancelled call does not corrupt owners");
    NativeAccessionLimits caps;caps.max_action_work=1;
    NativeAccessionDirectory bounded(new_e,901,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},caps);
    rejects<std::length_error>([&]{Plan::prepare(request(old,bounded,J),available);},"actual endpoint work authority bounded");
}
void owners() {
    const auto M=Matrix::identity(2),J=Matrix::identity(2);const auto a=endpoint(M,{1,2}),b=endpoint(M,{3,4},8,{{2,2}});
    NativeAccessionDirectory old(a,900),next(b,901);const auto old_field=input(old),new_field=input(next),cause=input(next,200);
    const auto plan=Plan::prepare(request(old,next,J),available);const auto old_bytes=encode_temporal_endpoint(*new_field.endpoint());
    auto later=b;later.sequence=9;later.tensors[1]=native_update_example::field(100,10,{5,6});
    later.tensors[4]=native_update_example::field(200,10,{4,4});next.advance(make_temporal_segment(b,later));
    check(plan.execute_causes(old_field,new_field,{cause},available).values().innovation==Vector{2,2},"old fields and causes remain old after advance");
    const inference::NativeMetricAvailability current{{900,7},{901,9}};
    rejects<std::invalid_argument>([&]{plan.execute(old_field,input(next),current);},"later equal-key field cannot enter earlier plan");
    rejects<std::invalid_argument>([&]{plan.execute_causes(old_field,new_field,{input(next,200)},current);},"later equal-key causes cannot enter earlier plan");
    check(encode_temporal_endpoint(*new_field.endpoint())==old_bytes,"later publication preserves old canonical bytes");
    std::vector<std::future<Vector>> jobs;for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[plan,old_field,new_field,cause]{
        return plan.execute_causes(old_field,new_field,{cause},available).values().innovation;}));
    for(auto& job:jobs)check(job.get()==Vector{2,2},"concurrent immutable metric actions have separate scratch");
    const auto retained=[](){NativeAccessionDirectory x(endpoint(Matrix::identity(1),{1}),900),y(endpoint(Matrix::identity(1),{4},8,{{3}}),901);
        return Plan::prepare(request(x,y,Matrix::identity(1)),available).execute_causes(input(x),input(y),{input(y,200)},available);}();
    check(retained.values().innovation==Vector{3} && retained.cause_inputs().size()==1 && retained.plan().numeric_plan().metric()==Matrix::identity(1),"result owns endpoints after directories are destroyed");
    const auto sources=square();NativeAccessionDirectory square_a(sources.first,900),square_b(sources.second,901);
    const auto square_plan=Plan::prepare(request(square_a,square_b,Matrix::identity(4)),available);
    const auto square_result=square_plan.execute_causes(input(square_a),input(square_b),{input(square_b,200),input(square_b,201)},available);
    check(square_result.provenance().old_endpoint.grade==core::Grade{1} && square_result.provenance().new_endpoint.grade==core::Grade{1} &&
          square_result.cause_pairings()->pairings==Matrix::from_rows({{16,4},{4,2}}),"paper path causes live on actual primary square relations");
    check(square_result.old_input().endpoint()->source.grade(core::Grade{1}).incidence_count()==20 &&
          square_result.new_input().endpoint()->source.grade(core::Grade{2}).cell_count()==1,"repeated cancelling inactive occurrences and later face retained");
    const auto old_availability=inference::NativeMetricAvailability{{900,6},{901,8}};
    rejects<std::invalid_argument>([&]{square_plan.execute(input(square_a),input(square_b),old_availability);},"later face cannot authorize unavailable old endpoint");
    NativeAccessionDirectory zero_a(endpoint(M,{0,0}),900),zero_b(endpoint(M,{0,0},8),901);
    const auto first=Plan::prepare(request(zero_a,zero_b,J),available),second=Plan::prepare(request(zero_a,zero_b,J.scale(2)),available);
    check(first.execute(input(zero_a),input(zero_b),available).values().innovation==second.execute(input(zero_a),input(zero_b),available).values().innovation &&
          first.numeric_plan().correspondence()!=second.numeric_plan().correspondence(),"equal current outputs do not identify complete comparisons");
    check(!first.execute(input(zero_a),input(zero_b),available).cause_pairings().has_value() &&
          first.execute_causes(input(zero_a),input(zero_b),{},available).cause_pairings()->pairings.rows()==0,"omitted causes and declared empty cause word remain distinct");
}
} // namespace
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";corpus(emit);numeric_contracts();contracts();owners();
    check(cases==324,"complete native corpus");if(!emit)std::cout<<"native_metric_checks="<<checks<<" native_cases="<<cases<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
