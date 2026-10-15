#include "../../examples/native_green/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace rfc;
using namespace storage;
using namespace native_green_example;
using Green=inference::PreparedNativeGreenResponseDelta;
using Observed=inference::PreparedNativeObservedGreenResponseDelta;
using Availability=inference::NativeGreenAvailability;
std::size_t checks{},cases{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F function,const char* message){
    bool caught=false;try{function();}catch(const Error&){caught=true;}check(caught,message);
}
M rectangular(std::size_t n,std::size_t m,std::size_t seed){M value(n,m);
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<m;++j)value.set(i,j,Q{static_cast<long>((i+2*j+seed)%7)-3,7});
    return value;
}
M resolving(std::size_t n){auto value=rectangular(n,n,1);
    for(std::size_t i=0;i<n;++i)value.set(i,i,Q{static_cast<unsigned long>(3*n+i+3)});
    return value;
}
void json(const V& values){std::cout<<'[';for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<'"'<<values[i]<<'"';}std::cout<<']';}
void json(const M& value){std::cout<<'[';for(std::size_t i=0;i<value.rows();++i){if(i)std::cout<<',';V row(value.cols());
    for(std::size_t j=0;j<row.size();++j)row[j]=value.get(i,j);
    json(row);}std::cout<<']';}
void corpus(bool emit){
    for(const auto& [n,m]:std::array<std::pair<std::size_t,std::size_t>,8>{{{0,0},{0,3},{3,0},{1,2},{2,1},{2,4},{4,2},{3,3}}})
    for(const auto& [p,q]:std::array<std::pair<std::size_t,std::size_t>,3>{{{0,0},{1,2},{3,1}}})
    for(const auto& [o,on]:std::array<std::pair<std::size_t,std::size_t>,3>{{{0,0},{2,3},{3,2}}})
    for(const Q& lambda:std::array<Q,3>{Q{0},Q{1,2},Q{-1,3}}){
        const auto L=resolving(n),Lp=resolving(m),I=rectangular(n,p,0),Ip=rectangular(m,q,1),
                   A=rectangular(o,n,2),Ap=rectangular(on,m,3),J=rectangular(m,n,4),K=rectangular(q,p,5),KA=rectangular(on,o,6);
        NativeAccessionDirectory old_directory(endpoint(L,I,A,7),900),new_directory(endpoint(Lp,Ip,Ap,8),901);
        const Availability available{{900,7},{901,8}};const auto req=request(old_directory,new_directory,J,K,lambda);
        const auto old_bytes=encode_temporal_endpoint(*req.old_operator.endpoint()),new_bytes=encode_temporal_endpoint(*req.new_operator.endpoint());
        const auto green=Green::prepare(req,available);
        const auto observed=Observed::prepare(green,op(old_directory,3),op(new_directory,3),
            comparison(req.old_operator,req.new_operator,30,30,KA),available);
        const auto reused=observed.green_plan();
        check(&reused.numeric_plan().ambient_correspondence()==&green.numeric_plan().ambient_correspondence(),"observations reuse actual Green factors");
        const auto prep=green.preparation_steps(),obs_prep=observed.preparation_steps();
        for(unsigned sample=0;sample<2;++sample){
            const auto old_input=input(old_directory,100+sample),new_input=input(new_directory,100+sample);
            const auto result=green.execute(old_input,new_input,available);
            const auto output=observed.execute(old_input,new_input,available);
            const auto& g=result.values();const auto& out=output.values();
            const auto u=inference::native_field_detail::dense(*old_input.tensor()),up=inference::native_field_detail::dense(*new_input.tensor());
            check(M::identity(n).add(L.scale(lambda)).apply(g.old_response)==I.apply(u) &&
                  M::identity(m).add(Lp.scale(lambda)).apply(g.new_response)==Ip.apply(up),"full original shifted equations");
            check(g.injection_defect==Ip.multiply(K).add(J.multiply(I).scale(-1)) &&
                  g.operator_defect==Lp.multiply(J).add(J.multiply(L).scale(-1)),"complete rectangular defects");
            check(g.aggregate==numeric::sub(g.new_response,J.apply(g.old_response)) &&
                  g.aggregate==numeric::add(numeric::add(g.realization_term,g.innovation_term),g.operator_term),"all three full response terms");
            check(out.aggregate==numeric::sub(Ap.apply(g.new_response),KA.apply(A.apply(g.old_response))) &&
                  out.aggregate==numeric::add(numeric::add(out.accession_term,out.realization_term),numeric::add(out.innovation_term,out.operator_term)),
                  "all four full observation terms");
            const auto& report=output.provenance();
            check(report.old_endpoint.source_namespace==900 && report.old_endpoint.sequence==7 && report.new_endpoint.source_namespace==901 &&
                  report.new_endpoint.sequence==8 && report.old_endpoint.ambient_carrier==10 && report.new_endpoint.source_carrier==20 &&
                  report.old_endpoint.input_field==100+sample && report.new_endpoint.output_carrier==30 && report.old_endpoint.accession==3 &&
                  report.observed && report.reused_green,"both actual endpoint/carrier/field witnesses");
            check(output.old_input().endpoint().get()==old_input.endpoint().get() && output.new_input().endpoint().get()==new_input.endpoint().get(),
                  "result owns both input publications");
            check(green.preparation_steps()==prep && observed.preparation_steps()==obs_prep && report.execution_steps>0,"immutable reusable factors");
            if(emit){
                std::cout<<"{\"n\":"<<n<<",\"m\":"<<m<<",\"p\":"<<p<<",\"q\":"<<q<<",\"o\":"<<o<<",\"on\":"<<on<<",\"lambda\":\""<<lambda<<'"';
                for(const auto& [name,map]:std::array<std::pair<const char*,const M*>,12>{{{"L",&L},{"Lp",&Lp},{"I",&I},{"Ip",&Ip},
                    {"J",&J},{"K",&K},{"A",&A},{"Ap",&Ap},{"KA",&KA},{"EI",&g.injection_defect},{"EL",&g.operator_defect},{"EA",&out.accession_defect}}}){
                    std::cout<<",\""<<name<<"\":";json(*map);}
                for(const auto& [name,value]:std::array<std::pair<const char*,const V*>,14>{{{"u",&u},{"up",&up},{"g",&g.old_response},{"gp",&g.new_response},
                    {"zeta",&g.coordinate_innovation},{"realization",&g.realization_term},{"innovation",&g.innovation_term},{"operator",&g.operator_term},
                    {"aggregate",&g.aggregate},{"accession",&out.accession_term},{"obs_realization",&out.realization_term},{"obs_innovation",&out.innovation_term},
                    {"obs_operator",&out.operator_term},{"observed",&out.aggregate}}}){std::cout<<",\""<<name<<"\":";json(*value);}
                std::cout<<"}\n";
            }
            ++cases;
        }
        check(encode_temporal_endpoint(*req.old_operator.endpoint())==old_bytes && encode_temporal_endpoint(*req.new_operator.endpoint())==new_bytes,
              "both original endpoint bytes stay exact");
    }
}
void contracts(){
    NativeAccessionDirectory old_directory(endpoint(M::identity(2),M::identity(2),M::identity(2),7),900),
        new_directory(endpoint(M::identity(3),M::from_rows({{1,0},{0,1},{0,0}}),M(0,3),8),900),
        clone(endpoint(M::identity(2),M::identity(2),M::identity(2),7),900);
    const Availability available{{900,7},{900,8}};
    const auto req=request(old_directory,new_directory,M::from_rows({{1,0},{0,1},{0,0}}),M::identity(2));
    const auto green=Green::prepare(req,available);const auto old_field=input(old_directory),new_field=input(new_directory);
    const auto output_map=comparison(req.old_operator,req.new_operator,30,30,M(0,2));
    const auto observed=Observed::prepare(green,op(old_directory,3),op(new_directory,3),output_map,available);
    auto wrong=req;wrong.ambient.old_publication=op(clone,1);
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"equal endpoint bytes do not identify comparison owners");
    wrong=req;wrong.source.old_carrier=40;
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"same-size source carrier is not identity");
    wrong=req;wrong.old_injection=op(clone,2);
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"foreign injection publication");
    wrong=req;wrong.old_injection=op(old_directory,3);
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"accession is not a source realization");
    wrong=req;wrong.old_operator=old_directory.pin(access(NativeAccessionKind::Tensor,1));
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"stored tensor view does not declare operator use");
    wrong=req;wrong.dimension_limit=0;rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"positive dimension policy");
    wrong=req;wrong.dimension_limit=2;rejects<std::length_error>([&]{Green::prepare(wrong,available);},"complete new ambient dimension admission");
    wrong=req;wrong.ambient.map=numeric::SparseRationalTensor::zero(numeric::IndexDomain({3,2}),numeric::StructuralSupport::sparse(6,{0}));
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"absent correspondence directions cannot be inferred zero");
    wrong=req;wrong.source.map=temporal_matrix_tensor(M(2,1));
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"all correspondence dimensions bind actual carriers");
    rejects<std::invalid_argument>([&]{green.execute(input(clone),new_field,available);},"equal foreign field");
    rejects<std::invalid_argument>([&]{green.execute(old_directory.pin(access(NativeAccessionKind::Tensor,100)),new_field,available);},
        "stored tensor view does not declare field use");
    rejects<std::invalid_argument>([&]{observed.execute(input(old_directory,102),new_field,available);},"empty output still checks actual source carrier");
    rejects<std::invalid_argument>([&]{Observed::prepare(green,op(clone,3),op(new_directory,3),output_map,available);},"foreign accession endpoint");
    auto bad_output=output_map;bad_output.old_carrier=20;
    rejects<std::invalid_argument>([&]{Observed::prepare(green,op(old_directory,3),op(new_directory,3),bad_output,available);},"output comparison requires actual output carriers");
    rejects<std::invalid_argument>([&]{Observed::prepare(green,op(old_directory,1),op(new_directory,3),output_map,available);},"resolving operator is not an accession");
    for(const auto cutoff:std::array<Availability,4>{{{{900,6},{900,8}},{{900,7},{900,7}},{{901,7},{900,8}},{{900,7},{901,8}}}}){
        rejects<std::invalid_argument>([&]{Green::prepare(req,cutoff);},"separate prepare cutoffs");
        rejects<std::invalid_argument>([&]{green.execute(old_field,new_field,cutoff);},"separate execute cutoffs");
        rejects<std::invalid_argument>([&]{Observed::prepare(green,op(old_directory,3),op(new_directory,3),output_map,cutoff);},"separate observation cutoffs");
        rejects<std::invalid_argument>([&]{observed.execute(old_field,new_field,cutoff);},"empty output cannot bypass either cutoff");
    }
    for(unsigned resource=0;resource<5;++resource){runtime::ExecutionHostPolicy policy;
        switch(resource){case 0:policy.limits.records=0;break;case 1:policy.limits.rational_slots=0;break;
            case 2:policy.limits.sparse_entries=0;break;case 3:policy.limits.scalar_work=0;break;default:policy.limits.integer_bits=0;break;}
        const runtime::ExecutionLease lease{policy};
        rejects<std::length_error>([&]{Green::prepare(req,available,lease);},"current prepare limits");
        rejects<std::length_error>([&]{Observed::prepare(green,op(old_directory,3),op(new_directory,3),output_map,available,lease);},"current adopted owner limits");
        rejects<std::length_error>([&]{green.execute(old_field,new_field,available,lease);},"current field limits");
        rejects<std::length_error>([&]{observed.execute(old_field,new_field,available,lease);},"current empty observation limits");
    }
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{green.execute(old_field,new_field,available,runtime::ExecutionLease{policy});},"allocation disabled");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{Green::prepare(req,available,runtime::ExecutionLease{policy});},"unregistered exact GPU");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(Green::prepare(req,available,runtime::ExecutionLease{policy}).execute(old_field,new_field,available).provenance().fallback,"explicit CPU fallback recorded");
    auto stop=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease cancelled({},stop);
    rejects<runtime::ExecutionCancelled>([&]{Green::prepare(req,available,cancelled);},"preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{Observed::prepare(green,op(old_directory,3),op(new_directory,3),output_map,available,cancelled);},"observation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{green.execute(old_field,new_field,available,cancelled);},"field cancellation");
    rejects<runtime::ExecutionCancelled>([&]{observed.execute(old_field,new_field,available,cancelled);},"empty observation cancellation");
    stop->store(false);check(observed.execute(old_field,new_field,available,cancelled).values().aggregate.empty(),"cancelled scratch leaves reusable immutable owner");
    wrong=req;wrong.lambda=-1;rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"singular shifted operators");
    wrong=req;mpz_set_ui(mpq_denref(wrong.lambda.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{Green::prepare(wrong,available);},"malformed shift before zero/empty fast paths");
    auto e=endpoint(M::identity(2),M(2,3),M(0,2),7);e.tensors[3]=native_update_example::field(100,20,{0,0,32768});
    NativeAccessionDirectory unused(e,900);const auto zero_req=request(unused,new_directory,M::from_rows({{1,0},{0,1},{0,0}}),M(2,3),0);
    const auto zero=Green::prepare(zero_req,available);policy={};policy.limits.integer_bits=8;
    rejects<std::length_error>([&]{zero.execute(input(unused),new_field,available,runtime::ExecutionLease{policy});},"unused source coordinate bit bound at zero shift");
    auto partial=e;partial.tensors[3].tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(3),numeric::StructuralSupport::sparse(3,{0}));
    NativeAccessionDirectory absent(partial,900);const auto absent_req=request(absent,new_directory,M::from_rows({{1,0},{0,1},{0,0}}),M(2,3),0);
    rejects<std::invalid_argument>([&]{Green::prepare(absent_req,available).execute(input(absent),new_field,available);},"partial zero source fields are not complete");
    NativeAccessionLimits limit;limit.max_action_work=1;
    NativeAccessionDirectory limited(e,900,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},limit);
    rejects<std::length_error>([&]{Green::prepare(request(limited,new_directory,M::from_rows({{1,0},{0,1},{0,0}}),M(2,3)),available);},"actual native work caps");
}
void owners_and_paper(){
    auto endpoints=square();NativeAccessionDirectory directory(endpoints.first,900);const auto old_L=op(directory,1),old_I=op(directory,2),old_A=op(directory,3),old_input=input(directory);
    directory.advance(make_temporal_segment(endpoints.first,endpoints.second));const auto new_L=op(directory,1),new_I=op(directory,2),new_A=op(directory,3),new_input=input(directory);
    const Availability available{{900,7},{900,8}};
    inference::NativeGreenResponseRequest req{old_L,new_L,old_I,new_I,comparison(old_L,new_L,10,10,M::identity(4)),comparison(old_L,new_L,20,20,M::identity(1)),1};
    runtime::ExecutionHostPolicy prepare_policy,observe_policy,execute_policy;
    prepare_policy.name="native-green";observe_policy.name="native-accession";execute_policy.name="native-comparison";
    const auto green=Green::prepare(req,available,runtime::ExecutionLease{prepare_policy});
    const auto observed=Observed::prepare(green,old_A,new_A,comparison(old_L,new_L,30,30,M::identity(2)),available,runtime::ExecutionLease{observe_policy});
    const auto result=green.execute(old_input,new_input,available);
    const auto output=observed.execute(old_input,new_input,available,runtime::ExecutionLease{execute_policy});
    check(result.values().old_response==V{Q{4,3},Q{4,3},Q{2,3},Q{2,3}} && result.values().new_response==V{Q{1,5},Q{1,5},Q{-4,5},Q{-4,5}},"original paper shifted responses");
    check(output.values().aggregate==V{Q{-29,15},Q{-29,15}} && output.values().accession_term==V{Q{2,3},Q{2,3}} &&
          output.values().operator_term==V{Q{-8,5},Q{-8,5}},"paper complete observed contribution identity");
    const auto& report=output.provenance();
    check(report.green_preparation_policy=="native-green" && report.preparation_policy=="native-accession" && report.execution_policy=="native-comparison" &&
          report.green_preparation_availability.old_endpoint.max_sequence==7 && report.preparation_availability.new_endpoint.max_sequence==8,
          "separate preparation/execution policy and cutoff witnesses");
    runtime::ExecutionHostPolicy bounded;bounded.limits.integer_bits=2;
    rejects<std::length_error>([&]{observed.execute(old_input,new_input,available,runtime::ExecutionLease{bounded});},
        "current shifted factors and intermediate growth exceed operand minima");
    const auto before=encode_temporal_endpoint(*old_L.endpoint());
    auto later=endpoints.second;later.sequence=9;later.tensors[3]=native_update_example::field(100,20,{10});
    directory.advance(make_temporal_segment(endpoints.second,later));
    check(observed.execute(old_input,new_input,available).values().aggregate==output.values().aggregate,"old comparison retained after future publication");
    rejects<std::invalid_argument>([&]{observed.execute(old_input,input(directory),Availability{{900,7},{900,9}});},"later same-key fields cannot substitute actual endpoint");
    check(encode_temporal_endpoint(*old_L.endpoint())==before,"past source endpoint bytes unchanged");
    std::vector<std::future<V>> jobs;for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[observed,old_input,new_input,available]{
        return observed.execute(old_input,new_input,available).values().aggregate;}));
    for(auto& job:jobs)check(job.get()==output.values().aggregate,"independent concurrent scratch");
    const auto retained=[] {auto pair=square();NativeAccessionDirectory old_dir(pair.first,900),new_dir(pair.second,900);
        const Availability a{{900,7},{900,8}};return Green::prepare(request(old_dir,new_dir,M::identity(4),M::identity(1)),a).execute(input(old_dir),input(new_dir),a);}();
    check(retained.plan().execute(retained.old_input(),retained.new_input(),available).values().aggregate==retained.values().aggregate,"both publications survive directory destruction");
    auto changed=req;changed.ambient.map=temporal_matrix_tensor(M(4,4));
    check(green.execute(old_input,new_input,available).values().aggregate==result.values().aggregate,"caller comparison replacement does not mutate owner");
}
} // namespace
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    corpus(emit);contracts();owners_and_paper();check(cases==432,"complete native Green corpus size");
    if(!emit)std::cout<<"native_green_checks="<<checks<<" exact_cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
