#include "../../examples/native_linear_observations/fixture.hpp"
#include <future>
#include <iostream>
#include <numeric>
#include <string_view>

namespace {
using namespace rfc;
using namespace storage;
using namespace native_linear_observation_example;
using Solve=inference::PreparedNativeWoodburySolve;
using Plan=inference::PreparedNativeLinearObservation;
using Numeric=numeric::PreparedExactLinearObservation;
constexpr runtime::WorkAvailability available{900,7};
std::size_t checks{},cases{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F function,const char* message){
    bool caught=false;try{function();}catch(const Error&){caught=true;}check(caught,message);
}
void json(const Vector& values){std::cout<<'[';for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<'"'<<values[i]<<'"';}std::cout<<']';}
void json(const Matrix& value){std::cout<<'[';for(std::size_t i=0;i<value.rows();++i){if(i)std::cout<<',';Vector row(value.cols());
    for(std::size_t j=0;j<row.size();++j)row[j]=value.get(i,j);
    json(row);}std::cout<<']';}
void json(const std::vector<std::size_t>& values){std::cout<<'[';for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<values[i];}std::cout<<']';}
numeric::ExactSolveOrdering ordering(std::size_t n){
    numeric::ExactSolveOrdering result;result.rows.resize(n);result.columns.resize(n);
    std::iota(result.rows.begin(),result.rows.end(),0);std::iota(result.columns.begin(),result.columns.end(),0);
    std::reverse(result.rows.begin(),result.rows.end());if(n)std::rotate(result.columns.begin(),result.columns.begin()+1,result.columns.end());
    return result;
}
Matrix accession(std::size_t n,std::size_t q,unsigned family){
    Matrix S(q,n);
    for(std::size_t i=0;i<q;++i)for(std::size_t j=0;j<n;++j){
        Q value;
        if(family==0)value=Q{static_cast<long>((i+2*j)%7)-3,static_cast<unsigned long>(i+j+2)};
        else if(family==1){
            const auto row=i%5;
            if(row==0 || row==2)value=Q{static_cast<long>(j+1),static_cast<unsigned long>(j+2)};
            else if(row==1)value=Q{static_cast<long>(2*j)-1,static_cast<unsigned long>(j+3)};
            else if(row==4)value=Q{static_cast<long>(2*(j+1)),static_cast<unsigned long>(j+2)};
        }else if(n && j==(n-i%n-1))value=1;
        S.set(i,j,value);
    }
    return S;
}
void sample(const Matrix& A,const Matrix& S,unsigned mode,bool emit){
    NativeAccessionDirectory directory(endpoint(A,S),900);
    const auto original=encode_temporal_endpoint(*op(directory).endpoint());
    const auto base=mode?Solve::prepare_base_ordered(op(directory),ordering(A.rows()),runtime::ExecutionLease{},available):
                         Solve::prepare_base(op(directory),runtime::ExecutionLease{},available);
    const auto plan=Plan::prepare(base,request(directory),available);
    check(&plan.numeric_plan().solve_owner().matrix()==&base.numeric_solve().matrix(),"actual native factor owner adopted");
    check(plan.numeric_plan().accession()==S,"complete original accession retained");
    std::vector<Vector> rows;std::vector<std::size_t> word;std::size_t nonzero{};
    for(std::size_t i=0;i<S.rows();++i){Vector row(S.cols());for(std::size_t j=0;j<S.cols();++j)row[j]=S.get(i,j);
        const auto found=std::find(rows.begin(),rows.end(),row);const auto index=static_cast<std::size_t>(found-rows.begin());
        if(found==rows.end()){if(std::any_of(row.begin(),row.end(),[](const Q& value){return value!=0;}))++nonzero;rows.push_back(row);}
        word.push_back(index);
    }
    check(plan.numeric_plan().row_reuse_word()==word && plan.numeric_plan().distinct_rows()==rows.size(),"only complete exact rows share execution work");
    const bool full=nonzero>=A.rows();
    check(plan.numeric_plan().uses_full_solve()==full && plan.numeric_plan().certified_nonzero_rows()==(full?0:nonzero),"row count primal or transpose policy");
    for(std::uint64_t key=100;key<102;++key){
        const auto field=input(directory,key);const auto rhs=field.tensor()->values().dense(),whole=base.numeric_solve().apply(rhs);
        const auto result=plan.execute(field,available);check(A.apply(whole)==rhs && result.values()==S.apply(whole),"complete exact native linear response");
        const auto& p=result.provenance();
        check(p.source_namespace==900 && p.sequence==7 && p.operation==1 && p.accession==2 && p.input_field==key &&
              p.input_carrier==10 && p.output_carrier==20 && p.source_grade==core::Grade{0} && p.output_rows==S.rows() &&
              p.distinct_coefficient_rows==rows.size() && p.certified_nonzero_rows==(full?0:nonzero) && p.uses_full_solve==full,
              "actual native linear provenance");
        check(result.carrier().get()==op(directory,2).carrier().get() && result.input().tensor_declaration().get()==field.tensor_declaration().get(),"actual declared owners");
        if(emit){std::cout<<"{\"n\":"<<A.rows()<<",\"q\":"<<S.rows()<<",\"mode\":"<<mode<<",\"full\":"<<(full?"true":"false")<<",\"A\":";json(A);
            std::cout<<",\"S\":";json(S);std::cout<<",\"reuse_word\":";json(word);
            std::cout<<",\"distinct_rows\":"<<rows.size()<<",\"certified_nonzero_rows\":"<<p.certified_nonzero_rows;
            std::cout<<",\"rhs\":";json(rhs);std::cout<<",\"result\":";json(result.values());std::cout<<"}\n";}
        ++cases;
    }
    check(encode_temporal_endpoint(*op(directory).endpoint())==original,"canonical source and native bytes unchanged");
}
void corpus(bool emit){
    for(const std::size_t n:std::array<std::size_t,6>{0,1,2,3,5,6})for(const std::size_t q:std::array<std::size_t,5>{0,1,2,4,7})
    for(unsigned family=0;family<3;++family){
        Matrix A(n,n);
        if(family==0){for(std::size_t i=0;i<n;++i){A.set(i,i,2);if(i){A.set(i-1,i,-1);A.set(i,i-1,-1);}}}
        else if(family==1){for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)
            A.set(i,j,i==j?Q{static_cast<unsigned long>(3*n+i+4)}:Q{static_cast<long>((i+2*j)%5)-2,17});}
        else for(std::size_t i=0;i<n;++i)A.set(i,n-i-1,Q{static_cast<unsigned long>(i+2)});
        for(unsigned kind=0;kind<3;++kind)for(unsigned mode=0;mode<2;++mode)sample(A,accession(n,q,kind),mode,emit);
    }
}
void numeric_contracts(){
    const auto A=Matrix::from_rows({{4,1,0},{0,3,1},{1,0,2}});
    const auto S=Matrix::from_rows({{1,2,0},{1,2,0},{0,0,0},{2,4,0}});
    const auto base=numeric::PreparedExactSolve::prepare_ordered(A,ordering(3));const auto plan=Numeric::prepare(base,S);
    check(!plan.uses_full_solve() && plan.distinct_rows()==3 && plan.certified_nonzero_rows()==2 &&
          plan.row_reuse_word()==std::vector<std::size_t>{0,0,1,2},"zero/repeated/proportional row distinction");
    const Vector rhs{1,2,3};const auto expected=S.apply(base.apply(rhs));
    check(plan.apply(rhs)==expected,"exact mixed linear readout");
    rejects<std::invalid_argument>([&]{Numeric::prepare(base,Matrix(2,2));},"whole accession input dimensions");
    rejects<std::invalid_argument>([&]{plan.apply(Vector(2));},"whole input dimensions");
    numeric::ExactActionLimits limits;limits.dimension=3;
    rejects<std::length_error>([&]{Numeric::prepare(base,S,limits);},"output dimension includes all repeated rows");
    limits={};limits.entries=0;rejects<std::length_error>([&]{plan.apply(rhs,limits);},"retained coefficient/index cap");
    limits.entries=plan.stored_entries()+plan.workspace_slots()-1;
    rejects<std::length_error>([&]{Numeric::prepare(base,S,limits);},"preparation includes retained coefficients and named scratch");
    rejects<std::length_error>([&]{plan.apply(rhs,limits);},"application includes retained coefficients and named scratch");
    limits={};limits.steps=0;rejects<std::length_error>([&]{plan.apply(rhs,limits);},"current work cap");
    numeric::ExactActionGuard guard;check(plan.apply(rhs,guard)==expected,"instrumented action");limits.steps=guard.steps()-1;
    rejects<std::length_error>([&]{plan.apply(rhs,limits);},"exact work boundary");
    const auto empty=Numeric::prepare(numeric::PreparedExactSolve(Matrix::from_rows({{1,1},{1,-1}})),Matrix(0,2));
    limits={};limits.integer_bits=1;rejects<std::length_error>([&]{empty.apply({0,0},limits);},"empty output checks unused factors");
    const auto zero=Numeric::prepare(numeric::PreparedExactSolve(Matrix::identity(3)),Matrix(5,3));
    check(zero.apply({1,2,3})==Vector(5) && zero.distinct_rows()==1 && !zero.uses_full_solve(),"supported zero rows retain all axes");
    limits={};limits.integer_bits=2;rejects<std::length_error>([&]{zero.apply({0,0,8},limits);},"zero observation still checks full input");
    const auto no_input=Numeric::prepare(numeric::PreparedExactSolve(Matrix(0,0)),Matrix(5,0));
    check(no_input.apply({})==Vector(5) && no_input.uses_full_solve() && no_input.row_reuse_word()==std::vector<std::size_t>(5,0),"zero dimensional domain and declared outputs");
    limits={};limits.integer_bits=0;
    rejects<std::length_error>([&]{no_input.apply({},limits);},"declared zero output checks its rational denominator bit");
    auto changing=plan;Vector external=rhs;bool replaced=false;
    const auto untouched=changing.apply(external,{},[&]{if(!replaced){replaced=true;external.assign(1,99);changing=zero;}});
    check(replaced && untouched==expected && external==Vector{99},"plan and whole input pinned before first callback");
    auto mutable_base=base;auto mutable_map=S;replaced=false;
    const auto prepared=Numeric::prepare(mutable_base,mutable_map,{},[&]{if(!replaced){replaced=true;
        mutable_base=numeric::PreparedExactSolve(Matrix::identity(1));mutable_map=Matrix(1,1);}});
    check(prepared.apply(rhs)==expected && mutable_map.rows()==1,"preparation owns complete arguments before first callback");
    auto changed_full=Numeric::prepare(base,Matrix::identity(3));external=rhs;replaced=false;
    const auto whole=changed_full.apply(external,{},[&]{if(!replaced){replaced=true;external.clear();changed_full=zero;}});
    check(whole==base.apply(rhs) && replaced,"primal plan and input pinned before callback");
    rejects<runtime::ExecutionCancelled>([&]{plan.apply(rhs,{},[]{throw runtime::ExecutionCancelled();});},"numeric checkpoint cancellation");
    check(plan.apply(rhs)==expected,"cancelled action leaves retained plan intact");
}
void contracts(){
    const auto A=Matrix::from_rows({{4,1,0},{0,3,1},{1,0,2}}),S=Matrix::from_rows({{1,2,0},{1,2,0},{0,0,0},{2,4,0}});
    NativeAccessionDirectory directory(endpoint(A,S),900),clone(endpoint(A,S),900);
    const auto base=Solve::prepare_base(op(directory));const auto req=request(directory);const auto field=input(directory);
    const auto plan=Plan::prepare(base,req,available);
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(clone),available);},"foreign equal publication and authority");
    rejects<std::invalid_argument>([&]{plan.execute(input(clone),available);},"foreign equal field");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory,102),available);},"equal dimensions do not identify carrier");
    rejects<std::invalid_argument>([&]{plan.execute(directory.pin(native_update_example::access(NativeAccessionKind::Tensor,100)),available);},"tensor view is not a field");
    auto wrong=req;wrong.accession=op(directory);rejects<std::invalid_argument>([&]{Plan::prepare(base,wrong,available);},"source realization is not accession");
    wrong=req;wrong.dimension_limit=3;rejects<std::length_error>([&]{Plan::prepare(base,wrong,available);},"complete output rows bounded");
    wrong=req;wrong.dimension_limit=0;rejects<std::invalid_argument>([&]{Plan::prepare(base,wrong,available);},"positive native dimension bound");
    for(const auto cutoff:std::array<runtime::WorkAvailability,3>{{{900,6},{901,99},{0,99}}}){
        rejects<std::invalid_argument>([&]{Plan::prepare(base,req,cutoff);},"preparation cutoff");
        rejects<std::invalid_argument>([&]{plan.execute(field,cutoff);},"execution cutoff");
    }
    auto e=endpoint(A,S);e.tensors[1].input=30;NativeAccessionDirectory wrong_input(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(Solve::prepare_base(op(wrong_input)),request(wrong_input),available);},"actual declared accession input");
    e=endpoint(A,S);e.carriers[1].domain=numeric::IndexDomain({2,2});NativeAccessionDirectory shaped(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(Solve::prepare_base(op(shaped)),request(shaped),available);},"equal output count does not identify linear domain");
    e=endpoint(A,S);e.tensors[1].tensor={numeric::IndexDomain({4,3}),numeric::StructuralSupport::sparse(12,{}),numeric::SparseRationalVector(12,{})};
    NativeAccessionDirectory absent(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(Solve::prepare_base(op(absent)),request(absent),available);},"absent map directions do not declare supported zero");
    e=endpoint(A,S);e.tensors[2].tensor={numeric::IndexDomain::linear(3),numeric::StructuralSupport::sparse(3,{0}),numeric::SparseRationalVector(3,{})};
    NativeAccessionDirectory partial(e,900);const auto partial_plan=Plan::prepare(Solve::prepare_base(op(partial)),request(partial),available);
    rejects<std::invalid_argument>([&]{partial_plan.execute(input(partial),available);},"complete field support required");
    for(unsigned resource=0;resource<5;++resource){runtime::ExecutionHostPolicy policy;
        switch(resource){case 0:policy.limits.records=0;break;case 1:policy.limits.rational_slots=0;break;
            case 2:policy.limits.sparse_entries=0;break;case 3:policy.limits.scalar_work=0;break;default:policy.limits.integer_bits=0;break;}
        rejects<std::length_error>([&]{Plan::prepare(base,req,available,runtime::ExecutionLease{policy});},"preparation resource cap");
        rejects<std::length_error>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"current resource cap");
    }
    const auto measured=plan.execute(field,available).provenance();runtime::ExecutionHostPolicy policy;
    policy.limits.rational_slots=plan.retained_requirements().rational_slots+plan.numeric_plan().workspace_slots()-1;
    rejects<std::length_error>([&]{Plan::prepare(base,req,available,runtime::ExecutionLease{policy});},"native preparation includes named scratch");
    policy={};
    policy.limits.rational_slots=measured.selected_requirements.rational_slots;
    policy.limits.sparse_entries=measured.selected_requirements.sparse_entries;
    policy.limits.records=measured.selected_requirements.records;
    check(plan.execute(field,available,runtime::ExecutionLease{policy}).values()==plan.execute(field,available).values(),"exact declared allocation envelope admitted");
    --policy.limits.rational_slots;rejects<std::length_error>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"scalar allocation boundary");
    policy={};policy.limits.sparse_entries=measured.selected_requirements.sparse_entries-1;
    rejects<std::length_error>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"index allocation boundary");
    policy={};policy.limits.scalar_work=measured.execution_steps-1;
    rejects<std::length_error>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"native current work boundary");
    NativeAccessionDirectory empty(endpoint(Matrix::from_rows({{1,1},{1,-1}}),Matrix(0,2)),900);
    const auto empty_plan=Plan::prepare(Solve::prepare_base(op(empty)),request(empty),available);policy={};policy.limits.integer_bits=1;
    rejects<std::length_error>([&]{empty_plan.execute(input(empty),available,runtime::ExecutionLease{policy});},"empty native output checks unused factors");
    e=endpoint(Matrix::identity(3),Matrix(5,3));e.tensors[2]=native_update_example::field(100,10,{1,0,8});
    NativeAccessionDirectory zero(e,900);const auto zero_plan=Plan::prepare(Solve::prepare_base(op(zero)),request(zero),available);
    policy={};policy.limits.integer_bits=2;rejects<std::length_error>([&]{zero_plan.execute(input(zero),available,runtime::ExecutionLease{policy});},"zero native map checks unobserved input");
    policy={};policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"owning allocation required");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{Plan::prepare(base,req,available,runtime::ExecutionLease{policy});},"rational GPU profile not declared");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(Plan::prepare(base,req,available,runtime::ExecutionLease{policy}).execute(field,available).provenance().fallback,"explicit CPU fallback retained");
    auto cancel=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease stopped({},cancel);
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare(base,req,available,stopped);},"native preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{plan.execute(field,available,stopped);},"native action cancellation");
    cancel->store(false);check(plan.execute(field,available,stopped).values()==plan.execute(field,available).values(),"cancelled native call leaves owners intact");
    NativeAccessionLimits caps;caps.max_action_work=1;
    NativeAccessionDirectory small(endpoint(A,S),900,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},caps);
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(small),available);},"foreign work authority cannot substitute");
    rejects<std::length_error>([&]{Solve::prepare_base(op(small));},"actual pin work limits");
    runtime::ExecutionHostPolicy a,b,c;a.name="base";b.name="accession";c.name="field";
    const auto named=Plan::prepare(Solve::prepare_base_ordered(op(directory),ordering(3),runtime::ExecutionLease{a},available),req,{900,8},runtime::ExecutionLease{b});
    const auto p=named.execute(field,{900,9},runtime::ExecutionLease{c}).provenance();
    check(p.base_preparation_policy=="base" && p.preparation_policy=="accession" && p.execution_policy=="field" &&
          p.base_preparation_availability->max_sequence==7 && p.preparation_availability.max_sequence==8 && p.execution_availability.max_sequence==9,
          "independent preparation and current policy/cutoff witnesses");
}
void owners(){
    const auto endpoints=square();NativeAccessionDirectory directory(endpoints.first,900);const auto old=input(directory);
    const auto base=Solve::prepare_base_ordered(op(directory),ordering(4));const auto plan=Plan::prepare(base,request(directory),available);
    const auto before=encode_temporal_endpoint(*old.endpoint());directory.advance(make_temporal_segment(endpoints.first,endpoints.second));
    const Vector expected{Q{2,3},Q{1,15},Q{2,3},0,Q{4,3}};
    check(plan.execute(old,available).values()==expected,"old observation remains old after face birth");
    check(plan.execute(old,available).provenance().source_grade==core::Grade{1},"actual positive source grade retained");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory),{900,8});},"later equal-key field cannot enter old plan");
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(directory),{900,8});},"later equal-key accession cannot enter old factors");
    const auto fresh=Plan::prepare(Solve::prepare_base(op(directory)),request(directory),{900,8});
    check(fresh.execute(input(directory),{900,8}).values()==Vector{Q{4,15},Q{1,15},Q{4,15},0,Q{8,15}},"complete new endpoint response");
    check(encode_temporal_endpoint(*old.endpoint())==before,"canonical old bytes unchanged by later endpoint");
    auto retained=[](){NativeAccessionDirectory d(endpoint(Matrix::from_rows({{3,1},{1,2}}),Matrix::from_rows({{1,1},{1,1},{0,0}})),900);
        return Plan::prepare(Solve::prepare_base(op(d)),request(d),available).execute(input(d),available);}();
    check(retained.values()==Vector{Q{1,5},Q{1,5},0},"result owns directory-independent complete map and field");
    std::vector<std::future<Vector>> jobs;
    for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[plan,old]{return plan.execute(old,available).values();}));
    for(auto& job:jobs)check(job.get()==expected,"concurrent immutable observation with independent scratch");
    const auto A=Matrix::identity(3);auto e=endpoint(A,Matrix::from_rows({{1,0,0}}));e.tensors[2]=native_update_example::field(100,10,Vector(3));
    auto next=e;next.sequence=8;next.tensors[1]=native_update_example::map(2,10,20,Matrix::from_rows({{1,0,2}}),TemporalTensorRole::Accession);
    NativeAccessionDirectory history(e,900);const auto zero_input=input(history);
    const auto original=Plan::prepare(Solve::prepare_base(op(history)),request(history),available);
    history.advance(make_temporal_segment(e,next));const auto current=Plan::prepare(Solve::prepare_base(op(history)),request(history),{900,8});
    check(original.execute(zero_input,available).values()==current.execute(input(history),{900,8}).values(),"equal current outputs allowed");
    check(original.numeric_plan().accession()!=current.numeric_plan().accession(),"equal output does not identify complete operations");
    rejects<std::invalid_argument>([&]{original.execute(input(history),{900,8});},"equal zero output cannot authorize later source");
    NativeAccessionDirectory selected(native_observation_example::endpoint(Matrix::from_rows({{3,1},{1,2}}),{1,0,1}),900);
    const auto shared=Solve::prepare_base(op(selected));
    const auto selected_plan=inference::PreparedNativeSelectedObservation::prepare(shared,native_observation_example::request(selected,{1,0,1}),available);
    const auto linear_plan=Plan::prepare(selected_plan.base_solve(),request(selected),available);
    check(&linear_plan.numeric_plan().solve_owner().matrix()==&selected_plan.numeric_plan().solve_owner().matrix() &&
          linear_plan.execute(input(selected),available).values()==selected_plan.execute(input(selected),available).values(),"existing actual selected factor reused without refactor");
}
} // namespace
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    corpus(emit);numeric_contracts();contracts();owners();check(cases==1080,"complete native corpus size");
    if(!emit)std::cout<<"native_linear_checks="<<checks<<" native_cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
