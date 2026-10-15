#include "../../examples/native_observations/fixture.hpp"
#include <future>
#include <iostream>
#include <numeric>
#include <string_view>

namespace {
using namespace rfc;
using namespace storage;
using namespace native_observation_example;
using Solve=inference::PreparedNativeWoodburySolve;
using Plan=inference::PreparedNativeSelectedObservation;
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
numeric::ExactSolveOrdering ordering(std::size_t n,unsigned mode){
    numeric::ExactSolveOrdering word;word.rows.resize(n);word.columns.resize(n);
    std::iota(word.rows.begin(),word.rows.end(),0);std::iota(word.columns.begin(),word.columns.end(),0);
    if(mode==2){std::reverse(word.rows.begin(),word.rows.end());if(n)std::rotate(word.columns.begin(),word.columns.begin()+1,word.columns.end());}
    return word;
}
void sample(const Matrix& A,const std::vector<std::size_t>& coordinates,unsigned mode,bool emit){
    NativeAccessionDirectory directory(endpoint(A,coordinates),900);
    const auto original=encode_temporal_endpoint(*op(directory).endpoint());
    const auto word=ordering(A.rows(),mode);
    const auto base=mode?Solve::prepare_base_ordered(op(directory),word,runtime::ExecutionLease{},available):
                         Solve::prepare_base(op(directory),runtime::ExecutionLease{},available);
    const auto plan=Plan::prepare(base,request(directory,coordinates),available);
    const auto receipt=base.numeric_solve().factor_receipt();
    check(&plan.numeric_plan().solve_owner().matrix()==&base.numeric_solve().matrix(),"actual factor adopted");
    check(receipt.has_declared_ordering()==(mode!=0),"natural and explicit identity order remain distinct");
    for(std::size_t i=0;i<A.rows();++i)check(receipt.row_coordinate(i)==word.rows[i] && receipt.column_coordinate(i)==word.columns[i],"original coordinate receipt");
    for(std::uint64_t key=100;key<102;++key){
        const auto field=input(directory,key);const auto result=plan.execute(field,available);
        const auto rhs=field.tensor()->values().dense(),full=base.numeric_solve().apply(rhs);
        Vector expected;for(const auto i:coordinates)expected.push_back(full[i]);
        check(result.values()==expected,"exact native selected result in original order");
        check(A.apply(full)==rhs,"complete native solve residual");
        check(result.carrier().get()==op(directory,2).carrier().get() && result.input().tensor_declaration().get()==field.tensor_declaration().get(),"actual output and input owners");
        const auto& p=result.provenance();
        check(p.coordinates==coordinates && p.source_namespace==900 && p.sequence==7 && p.operation==1 && p.accession==2 &&
              p.input_field==key && p.input_carrier==10 && p.output_carrier==20 && p.source_cells==plan.source_cells(),"complete selected native provenance");
        for(std::size_t i=0;i<coordinates.size();++i)check(p.source_cells[i]==StableCellKey{core::Grade{0},10+coordinates[i]},"stable source identity witnesses");
        if(emit){std::cout<<"{\"n\":"<<A.rows()<<",\"mode\":"<<mode<<",\"full\":"<<(plan.numeric_plan().uses_full_solve()?"true":"false")<<",\"A\":";json(A);
            std::cout<<",\"S\":";json(selector(A.rows(),coordinates));std::cout<<",\"coordinates\":";json(coordinates);
            std::cout<<",\"row_word\":";json(word.rows);std::cout<<",\"column_word\":";json(word.columns);
            std::cout<<",\"rhs\":";json(rhs);std::cout<<",\"result\":";json(result.values());std::cout<<"}\n";}
        ++cases;
    }
    check(encode_temporal_endpoint(*op(directory).endpoint())==original,"canonical native bytes and source attaching order unchanged");
}
void corpus(bool emit){
    for(unsigned mode=0;mode<3;++mode)sample(Matrix(0,0),{},mode,emit);
    for(std::size_t n=1;n<=6;++n)for(unsigned family=0;family<3;++family){
        Matrix A(n,n);
        if(family==0){for(std::size_t i=0;i<n;++i){A.set(i,i,2);if(i){A.set(i-1,i,-1);A.set(i,i-1,-1);}}}
        else if(family==1){for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)
            A.set(i,j,i==j?Q{static_cast<unsigned long>(3*n+i+4)}:Q{static_cast<long>((i+2*j)%5)-2,17});}
        else for(std::size_t i=0;i<n;++i)A.set(i,n-i-1,Q{static_cast<unsigned long>(i+2)});
        std::vector<std::size_t> reverse(n);std::iota(reverse.begin(),reverse.end(),0);std::reverse(reverse.begin(),reverse.end());
        auto long_word=reverse;long_word.push_back(n-1);
        const std::array<std::vector<std::size_t>,5> words{{{},{n-1},{n-1,0,n-1},reverse,long_word}};
        for(unsigned mode=0;mode<3;++mode)for(const auto& word:words)sample(A,word,mode,emit);
    }
}
void contracts(){
    const auto A=Matrix::from_rows({{4,1,0},{0,3,1},{1,0,2}});const std::vector<std::size_t> word{2,0,2};
    NativeAccessionDirectory directory(endpoint(A,word),900),clone(endpoint(A,word),900);
    const auto base=Solve::prepare_base(op(directory));const auto req=request(directory,word);
    const auto plan=Plan::prepare(base,req,available);const auto field=input(directory);
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(clone,word),available);},"equal foreign publication and authority");
    rejects<std::invalid_argument>([&]{plan.execute(input(clone),available);},"foreign equal input");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory,102),available);},"equal dimensions do not identify source carrier");
    rejects<std::invalid_argument>([&]{plan.execute(directory.pin(access(NativeAccessionKind::Tensor,100)),available);},"tensor view does not declare field");
    auto wrong=req;wrong.accession=op(directory);
    rejects<std::invalid_argument>([&]{Plan::prepare(base,wrong,available);},"source realization does not declare accession");
    wrong=req;wrong.coordinates[0]=3;rejects<std::out_of_range>([&]{Plan::prepare(base,wrong,available);},"all original coordinates in range");
    wrong=req;wrong.dimension_limit=2;rejects<std::length_error>([&]{Plan::prepare(base,wrong,available);},"complete original dimensions bounded");
    wrong=req;wrong.dimension_limit=0;rejects<std::invalid_argument>([&]{Plan::prepare(base,wrong,available);},"positive dimension bound");
    wrong=req;wrong.coordinates.pop_back();rejects<std::invalid_argument>([&]{Plan::prepare(base,wrong,available);},"actual output carrier must match whole request");
    for(const auto cutoff:std::array<runtime::WorkAvailability,3>{{{900,6},{901,99},{0,99}}}){
        rejects<std::invalid_argument>([&]{Plan::prepare(base,req,cutoff);},"preparation cutoff");
        rejects<std::invalid_argument>([&]{plan.execute(field,cutoff);},"execution cutoff");
        rejects<std::invalid_argument>([&]{Solve::prepare_base_ordered(op(directory),ordering(3,2),runtime::ExecutionLease{},cutoff);},"ordered base cutoff");
    }
    auto e=endpoint(A,word);e.tensors[1].tensor=temporal_matrix_tensor(Matrix(3,3));e.tensors[2]=native_update_example::field(100,10,Vector(3));
    NativeAccessionDirectory zero_wrong(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(Solve::prepare_base(op(zero_wrong)),request(zero_wrong,word),available);},"equal zero response cannot certify whole selector");
    e=endpoint(A,word);e.tensors[1].tensor={numeric::IndexDomain({3,3}),numeric::StructuralSupport::sparse(9,{}),numeric::SparseRationalVector(9,{})};
    NativeAccessionDirectory absent(e,900);
    rejects<std::invalid_argument>([&]{Plan::prepare(Solve::prepare_base(op(absent)),request(absent,word),available);},"absent selector directions cannot be inferred zero");
    e=endpoint(A,word);e.tensors[2].tensor={numeric::IndexDomain::linear(3),numeric::StructuralSupport::sparse(3,{0}),numeric::SparseRationalVector(3,{})};
    NativeAccessionDirectory partial(e,900);
    const auto partial_plan=Plan::prepare(Solve::prepare_base(op(partial)),request(partial,word),available);
    rejects<std::invalid_argument>([&]{partial_plan.execute(input(partial),available);},"full supported zero and partial field differ");
    auto invalid_order=ordering(3,2);invalid_order.rows[1]=invalid_order.rows[0];
    rejects<std::invalid_argument>([&]{Solve::prepare_base_ordered(op(directory),invalid_order);},"native full row bijection");
    invalid_order=ordering(3,2);invalid_order.columns.pop_back();
    rejects<std::invalid_argument>([&]{Solve::prepare_base_ordered(op(directory),invalid_order);},"native full column bijection");
    for(unsigned resource=0;resource<5;++resource){runtime::ExecutionHostPolicy policy;
        switch(resource){case 0:policy.limits.records=0;break;case 1:policy.limits.rational_slots=0;break;
            case 2:policy.limits.sparse_entries=0;break;case 3:policy.limits.scalar_work=0;break;default:policy.limits.integer_bits=0;break;}
        rejects<std::length_error>([&]{Plan::prepare(base,req,available,runtime::ExecutionLease{policy});},"current preparation resource caps");
        rejects<std::length_error>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"current action resource caps");
    }
    e=endpoint(Matrix::from_rows({{1,1},{1,-1}}),{});NativeAccessionDirectory empty(e,900);
    const auto empty_plan=Plan::prepare(Solve::prepare_base(op(empty)),request(empty,{}),available);
    runtime::ExecutionHostPolicy policy;policy.limits.integer_bits=1;
    rejects<std::length_error>([&]{empty_plan.execute(input(empty),available,runtime::ExecutionLease{policy});},"empty readout validates unused growing factors");
    e=endpoint(Matrix::identity(2),{});e.tensors[2]=native_update_example::field(100,10,{1,8});NativeAccessionDirectory empty_input(e,900);
    const auto empty_input_plan=Plan::prepare(Solve::prepare_base(op(empty_input)),request(empty_input,{}),available);
    policy={};policy.limits.integer_bits=2;
    rejects<std::length_error>([&]{empty_input_plan.execute(input(empty_input),available,runtime::ExecutionLease{policy});},"empty readout validates unobserved input coordinates");
    policy={};policy.allow_allocation=false;rejects<std::invalid_argument>([&]{plan.execute(field,available,runtime::ExecutionLease{policy});},"owning allocations required");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{Plan::prepare(base,req,available,runtime::ExecutionLease{policy});},"unsupported GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(Plan::prepare(base,req,available,runtime::ExecutionLease{policy}).execute(field,available).provenance().fallback,"explicit fallback retained");
    auto cancel=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease stopped({},cancel);
    rejects<runtime::ExecutionCancelled>([&]{Plan::prepare(base,req,available,stopped);},"preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{plan.execute(field,available,stopped);},"action cancellation");
    cancel->store(false);check(plan.execute(field,available,stopped).values()==plan.execute(field,available).values(),"cancelled call preserves owner");
    NativeAccessionLimits caps;caps.max_action_work=1;
    NativeAccessionDirectory small(endpoint(A,word),900,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},caps);
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(small,word),available);},"foreign pin cannot substitute current work authority");
    rejects<std::length_error>([&]{Solve::prepare_base_ordered(op(small),ordering(3,2));},"actual pin work cap");
    runtime::ExecutionHostPolicy a,b,c;a.name="baseline";b.name="selection";c.name="field";
    const auto named=Plan::prepare(Solve::prepare_base_ordered(op(directory),ordering(3,2),runtime::ExecutionLease{a},available),req,{900,8},runtime::ExecutionLease{b});
    const auto p=named.execute(field,{900,9},runtime::ExecutionLease{c}).provenance();
    check(p.base_preparation_policy=="baseline" && p.preparation_policy=="selection" && p.execution_policy=="field" &&
          p.base_preparation_availability->max_sequence==7 && p.preparation_availability.max_sequence==8 && p.execution_availability.max_sequence==9,"separate current and preparation policies and witnesses");
}
void owners(){
    const auto endpoints=square();NativeAccessionDirectory directory(endpoints.first,900);
    const auto old=input(directory);const auto base=Solve::prepare_base_ordered(op(directory),ordering(4,2));
    const auto plan=Plan::prepare(base,request(directory,{2,0,2}),available);const auto before=encode_temporal_endpoint(*old.endpoint());
    directory.advance(make_temporal_segment(endpoints.first,endpoints.second));
    check(plan.execute(old,available).values()==Vector{Q{2,15},Q{7,15},Q{2,15}},"old plan remains old after face birth");
    check(plan.source_cells()==std::vector<StableCellKey>{{core::Grade{1},102},{core::Grade{1},100},{core::Grade{1},102}},"positive grade primary identity order");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory),{900,8});},"later equal-key field cannot enter old endpoint");
    rejects<std::invalid_argument>([&]{Plan::prepare(base,request(directory,{2,0,2}),{900,8});},"later accession cannot enter old factor");
    const auto fresh=Plan::prepare(Solve::prepare_base(op(directory)),request(directory,{2,0,2}),{900,8});
    check(fresh.execute(input(directory),{900,8}).values()==Vector{Q{-1,15},Q{4,15},Q{-1,15}},"new actual endpoint response");
    check(encode_temporal_endpoint(*old.endpoint())==before,"no future or mutation leakage");
    auto retained=[](){NativeAccessionDirectory d(endpoint(Matrix::from_rows({{3,1},{1,2}}),{1,1}),900);
        return Plan::prepare(Solve::prepare_base(op(d)),request(d,{1,1}),available).execute(input(d),available);}();
    check(retained.values()==Vector{Q{-1,5},Q{-1,5}},"derived result owns directory-independent field and factor");
    std::vector<std::future<Vector>> jobs;
    for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[plan,old]{return plan.execute(old,available).values();}));
    for(auto& job:jobs)check(job.get()==plan.execute(old,available).values(),"concurrent immutable selected owner");
    const auto source=native_update_example::source(32);const auto B=field::materialize_boundary_matrix(source,core::Grade{1});
    const auto A=Matrix::identity(32).add(B.multiply(B.transpose()));NativeAccessionDirectory star(endpoint(A,{31,0,3,31}),900);
    const auto natural=Solve::prepare_base(op(star)),ordered=Solve::prepare_base_ordered(op(star),numeric::minimum_degree_ordering(A));
    check(natural.numeric_solve().preparation_stats().elimination_factor_entries==496 && ordered.numeric_solve().preparation_stats().elimination_factor_entries==31,"star retained elimination count");
    check(source.grade(core::Grade{1}).incidence_count()==155,"all repeated and inactive star occurrences retained");
    check(Plan::prepare(ordered,request(star,{31,0,3,31}),available).execute(input(star),available).values()==Vector{Q{1,33},Q{2,33},Q{1,33},Q{1,33}},"selected star exact response");
    auto update_endpoint=native_update_example::endpoint(4,2);NativeAccessionDirectory updates(update_endpoint,900);
    const auto pin=[&](std::uint64_t key){return op(updates,key);};const auto shared=Solve::prepare_base_ordered(pin(1),ordering(4,2));
    const inference::NativeWoodburyRequest req{pin(1),pin(2),pin(3),pin(4),pin(5)};
    const auto update=Solve::prepare(req,shared);
    check(&update.numeric_plan().base_solve().matrix()==&shared.numeric_solve().matrix(),"ordered native base also serves Woodbury without refactor");
    const auto rhs=input(updates);check(temporal_tensor_matrix(*pin(5).tensor_declaration()).apply(update.execute(rhs).values())==rhs.tensor()->values().dense(),"complete updated solve after shared ordering");
}
} // namespace
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    corpus(emit);contracts();owners();check(cases==546,"exact native corpus size");
    if(!emit)std::cout<<"native_selected_checks="<<checks<<" native_cases="<<cases<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
