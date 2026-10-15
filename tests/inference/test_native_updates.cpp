#include "rfc/inference/native_field_updates.hpp"
#include "../../examples/native_updates/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

using namespace rfc;
using namespace rfc::storage;
using M=numeric::ExactMatrix;
using V=numeric::RationalVector;
using Q=numeric::Rational;
using Plan=inference::PreparedNativeWoodburySolve;
using Request=inference::NativeWoodburyRequest;
using native_update_example::access;
using native_update_example::endpoint;
namespace {
std::size_t checks{},cases{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F function,const char* message){
    bool caught=false;try{function();}catch(const Error&){caught=true;}check(caught,message);
}
Request request(const NativeAccessionDirectory& d){return {d.pin(access(NativeAccessionKind::Operator,1)),
    d.pin(access(NativeAccessionKind::Operator,2)),d.pin(access(NativeAccessionKind::Operator,3)),
    d.pin(access(NativeAccessionKind::Operator,4)),d.pin(access(NativeAccessionKind::Operator,5))};}
auto input(const NativeAccessionDirectory& d,std::uint64_t key=100){return d.pin(access(NativeAccessionKind::Field,key));}
void json(const V& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void json(const M& a){std::cout<<'[';for(std::size_t i=0;i<a.rows();++i){if(i)std::cout<<',';V row(a.cols());for(std::size_t j=0;j<row.size();++j)row[j]=a.get(i,j);json(row);}std::cout<<']';}
void corpus(bool emit){
    for(std::size_t n=0;n<=8;++n)for(std::size_t rank=0;rank<=3;++rank)for(std::size_t seed=0;seed<3;++seed){
        NativeAccessionDirectory d(endpoint(n,rank,seed),900);const auto req=request(d);
        const auto before=encode_temporal_endpoint(*req.base.endpoint());
        const auto fresh=Plan::prepare(req);const auto base=fresh.base_solve();const auto retained=Plan::prepare(req,base);
        check(&fresh.numeric_plan().base_solve().matrix()==&base.numeric_solve().matrix(),"fresh native base actual ownership");
        check(&retained.numeric_plan().base_solve().matrix()==&base.numeric_solve().matrix(),"reused actual native factor");
        const auto A=temporal_tensor_matrix(*req.base.tensor_declaration()),U=temporal_tensor_matrix(*req.directions.tensor_declaration()),
                   C=temporal_tensor_matrix(*req.coupling.tensor_declaration()),R=temporal_tensor_matrix(*req.row_map.tensor_declaration()),
                   target=temporal_tensor_matrix(*req.updated.tensor_declaration());
        const auto prep=retained.preparation_steps();
        if(n>0)check(prep<fresh.preparation_steps(),"native base adoption skips its original factorization");
        for(unsigned sample=0;sample<2;++sample){
            const auto field=input(d,100+sample);const auto rhs=inference::native_field_detail::dense(*field.tensor());
            const auto result=retained.execute(field,runtime::ExecutionLease{},runtime::WorkAvailability{900,7});
            check(result.values()==target.solve(rhs) && target.apply(result.values())==rhs,"full native updated equation");
            check(fresh.execute(field).values()==result.values(),"same canonical fresh/reused result");
            check(result.publication().endpoint().get()==req.base.endpoint().get() && result.input().tensor_declaration().get()==field.tensor_declaration().get(),"actual result publication and field owners");
            const auto& p=result.provenance();
            check(p.source_namespace==900 && p.sequence==7 && p.carrier==10 && p.update_carrier==20 && p.input_field==100+sample &&
                  p.base==1 && p.directions==2 && p.coupling==3 && p.row_map==4 && p.updated==5 && p.reused_base,"complete native provenance");
            check(retained.preparation_steps()==prep && p.execution_steps>0,"retained native factors unchanged");
            if(emit){std::cout<<"{\"n\":"<<n<<",\"rank\":"<<rank<<",\"A\":";json(A);std::cout<<",\"U\":";json(U);
                std::cout<<",\"C\":";json(C);std::cout<<",\"row\":";json(R);std::cout<<",\"updated\":";json(target);
                std::cout<<",\"rhs\":";json(rhs);std::cout<<",\"answer\":";json(result.values());std::cout<<"}\n";}
            ++cases;
        }
        check(encode_temporal_endpoint(*req.base.endpoint())==before,"canonical endpoint bytes untouched");
    }
}
void contracts(){
    NativeAccessionDirectory d(endpoint(3,2),900),clone(endpoint(3,2),900),foreign(endpoint(3,2),901);
    const auto req=request(d);const auto base=Plan::prepare_base(req.base);const auto plan=Plan::prepare(req,base);
    const auto rhs=input(d);auto replacement=req;replacement.directions=request(clone).directions;
    rejects<std::invalid_argument>([&]{Plan::prepare(replacement);},"equal bytes and namespace do not identify publication");
    rejects<std::invalid_argument>([&]{Plan::prepare(req,Plan::prepare_base(request(clone).base));},"foreign authentic base owner");
    rejects<std::invalid_argument>([&]{plan.execute(input(clone));},"equal-valued foreign field");
    rejects<std::invalid_argument>([&]{plan.execute(input(foreign));},"foreign namespace field");
    rejects<std::invalid_argument>([&]{plan.execute(input(d,102));},"equal-dimensional foreign carrier");
    auto wrong=req;wrong.base=d.pin(access(NativeAccessionKind::Tensor,1));
    rejects<std::invalid_argument>([&]{Plan::prepare(wrong);},"tensor view does not declare operator use");
    wrong=req;wrong.dimension_limit=0;rejects<std::invalid_argument>([&]{Plan::prepare(wrong);},"zero dimension policy");
    wrong=req;wrong.dimension_limit=1;rejects<std::length_error>([&]{Plan::prepare(wrong);},"ambient dimension admission");
    wrong=req;wrong.coupling=req.base;rejects<std::invalid_argument>([&]{Plan::prepare(wrong);},"equal keys do not repair carrier directions");
    auto declaration=endpoint(3,2);declaration.tensors.push_back(native_update_example::map(6,30,30,M::identity(3)));
    NativeAccessionDirectory explicit_carrier(declaration,900);
    rejects<std::invalid_argument>([&]{Plan::prepare_base(explicit_carrier.pin(access(NativeAccessionKind::Operator,6)));},
        "explicit coordinates do not identify a source-grade carrier");
    NativeAccessionDirectory large_update(endpoint(2,4),900);auto bounded=request(large_update);bounded.dimension_limit=2;
    rejects<std::length_error>([&]{Plan::prepare(bounded);},"update-carrier dimension admission");
    for(const auto cutoff:std::array<runtime::WorkAvailability,3>{{{900,6},{901,100},{0,100}}}){
        rejects<std::invalid_argument>([&]{Plan::prepare(req,runtime::ExecutionLease{},cutoff);},"prepare availability");
        rejects<std::invalid_argument>([&]{Plan::prepare_base(req.base,runtime::ExecutionLease{},cutoff);},"base availability");
        rejects<std::invalid_argument>([&]{plan.execute(rhs,runtime::ExecutionLease{},cutoff);},"execution availability");
    }
    auto e=endpoint(3,2);e.tensors[5]=native_update_example::field(100,10,V(3));
    e.tensors[5].tensor={numeric::IndexDomain::linear(3),numeric::StructuralSupport::sparse(3,{0}),numeric::SparseRationalVector(3,{})};
    NativeAccessionDirectory partial(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(partial)).execute(input(partial));},"partial supported-zero field");
    e=endpoint(3,2);e.tensors[1].tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain({3,2}),numeric::StructuralSupport::sparse(6,{}));
    NativeAccessionDirectory absent(e,900);rejects<std::invalid_argument>([&]{Plan::prepare(request(absent));},"absent directions cannot be inferred zero");
    e=endpoint(3,2);auto target=temporal_tensor_matrix(e.tensors[4]);target.set(0,2,target.get(0,2)+1);e.tensors[4].tensor=temporal_matrix_tensor(target);
    e.tensors[5]=native_update_example::field(100,10,V(3));NativeAccessionDirectory wrong_map(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(wrong_map));},"complete updated witness despite equal zero response");
    e=endpoint(3,2);e.tensors.push_back(native_update_example::map(6,10,10,temporal_tensor_matrix(e.tensors[0])));
    NativeAccessionDirectory twins(e,900);auto twin=request(twins);twin.base=twins.pin(access(NativeAccessionKind::Operator,6));
    rejects<std::invalid_argument>([&]{Plan::prepare(twin,Plan::prepare_base(request(twins).base));},"equal coefficients do not identify different declared operators");
    const M identity=M::identity(2);
    NativeAccessionDirectory bad_c(endpoint(identity,M(2,1),M(1,1),M(1,2)),900);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(bad_c));},"singular C even for zero update");
    NativeAccessionDirectory bad_a(endpoint(M(2,2),M(2,0),M(0,0),M(0,2)),900);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(bad_a));},"singular base for empty update");
    NativeAccessionDirectory bad_target(endpoint(identity,identity,identity.scale(-1),identity),900);
    rejects<std::invalid_argument>([&]{Plan::prepare(request(bad_target));},"singular updated system");
    for(unsigned resource=0;resource<5;++resource){
        runtime::ExecutionHostPolicy policy;
        switch(resource){case 0:policy.limits.records=0;break;case 1:policy.limits.rational_slots=0;break;
            case 2:policy.limits.sparse_entries=0;break;case 3:policy.limits.scalar_work=0;break;default:policy.limits.integer_bits=0;break;}
        rejects<std::length_error>([&]{Plan::prepare(req,runtime::ExecutionLease{policy});},"prepare current resource admission");
        rejects<std::length_error>([&]{Plan::prepare(req,base,runtime::ExecutionLease{policy});},"reused prepare current resource admission");
        rejects<std::length_error>([&]{plan.execute(rhs,runtime::ExecutionLease{policy});},"execute current resource admission");
    }
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{plan.execute(rhs,runtime::ExecutionLease{policy});},"allocation disabled");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{Plan::prepare(req,runtime::ExecutionLease{policy});},"unregistered exact GPU");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=Plan::prepare(req,runtime::ExecutionLease{policy}).execute(rhs);
    check(fallback.provenance().fallback,"explicit CPU fallback provenance");
    policy={};policy.limits.integer_bits=plan.retained_requirements().integer_bits;
    rejects<std::length_error>([&]{plan.execute(rhs,runtime::ExecutionLease{policy});},"current retained factor and intermediate growth admission");
    NativeAccessionLimits accession;accession.max_action_work=1;
    NativeAccessionDirectory small(endpoint(3,2),900,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},accession);
    rejects<std::length_error>([&]{Plan::prepare(request(small));},"actual pin work limits");
    auto cancel=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease stopped({},cancel);
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare(req,stopped);},"preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare_base(req.base,stopped);},"base cancellation");
    rejects<runtime::ExecutionCancelled>([&]{plan.execute(rhs,stopped);},"execution cancellation");
    cancel->store(false);check(plan.execute(rhs,stopped).values()==plan.execute(rhs).values(),"cancelled scratch leaves immutable owner");
    check(!base.numeric_solve().matrix().entries().empty(),"native base remains reusable");
    runtime::ExecutionHostPolicy base_policy,prepare_policy,execution_policy;
    base_policy.name="native-baseline";prepare_policy.name="native-update";execution_policy.name="native-field";
    const auto named_base=Plan::prepare_base(req.base,runtime::ExecutionLease{base_policy},runtime::WorkAvailability{900,8});
    const auto named_plan=Plan::prepare(req,named_base,runtime::ExecutionLease{prepare_policy},runtime::WorkAvailability{900,9});
    const auto named=named_plan.execute(rhs,runtime::ExecutionLease{execution_policy},runtime::WorkAvailability{900,10});
    const auto& report=named.provenance();
    check(report.base_preparation_policy=="native-baseline" && report.preparation_policy=="native-update" &&
          report.execution_policy=="native-field","separate base, update and field policies");
    check(report.base_preparation_availability->source_namespace==900 && report.base_preparation_availability->max_sequence==8 &&
          report.preparation_availability->max_sequence==9 && report.execution_availability->max_sequence==10,
          "separate complete availability witnesses");
    const auto inherited=named_plan.execute(rhs).provenance();
    check(inherited.execution_availability->source_namespace==900 && inherited.execution_availability->max_sequence==9,
          "omitted current cutoff retains preparation availability");
}
void owners(){
    const auto held=[] {NativeAccessionDirectory local(endpoint(4,2),900);return Plan::prepare(request(local)).execute(input(local));}();
    check(held.plan().execute(held.input()).values()==held.values(),"plan/result survive original directory destruction");
    NativeAccessionDirectory d(endpoint(4,2),900);auto req=request(d);const auto plan=Plan::prepare(req);const auto field=input(d);
    const auto expected=plan.execute(field).values();const auto before=encode_temporal_endpoint(*field.endpoint());
    std::vector<std::future<V>> jobs;for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[plan,field]{return V(plan.execute(field).values());}));
    for(auto& job:jobs)check(job.get()==expected,"concurrent native scratch");
    auto next=endpoint(4,2,1);next.sequence=8;d.advance(make_temporal_segment(*field.endpoint(),next));
    check(plan.execute(field,runtime::ExecutionLease{},runtime::WorkAvailability{900,7}).values()==expected,"old plan keeps old endpoint after publication");
    rejects<std::invalid_argument>([&]{plan.execute(input(d));},"later input cannot enter old plan");
    rejects<std::invalid_argument>([&]{Plan::prepare(request(d),runtime::ExecutionLease{},runtime::WorkAvailability{900,7});},"later plan cannot exceed old cutoff");
    check(encode_temporal_endpoint(*field.endpoint())==before,"past endpoint canonical bytes stay exact");
    const auto base=plan.base_solve();auto moved=std::move(req);
    check(req.base.endpoint().get()==moved.base.endpoint().get(),"rvalue request pins keep original ownership");
    const auto next_base=Plan::prepare_base(request(d).base);auto retained_base=base;retained_base=next_base;
    rejects<std::invalid_argument>([&]{Plan::prepare(moved,retained_base);},"reassigned later base not substituted");
    check(Plan::prepare(moved,base).execute(field).values()==expected,"original base still correct");
}
}
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    corpus(emit);contracts();owners();check(cases==216,"exact native corpus size");
    if(!emit)std::cout<<"native_update_checks="<<checks<<" exact_cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
