#include "../../examples/native_frames/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace rfc;
using namespace storage;
using namespace native_frame_example;
using Frames=inference::PreparedNativeHodgeFrames;
std::size_t checks{},numeric_cases{},native_cases{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F function,const char* message){
    bool caught=false;try{function();}catch(const Error&){caught=true;}check(caught,message);
}
void json(const Vector& v){std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<'"'<<v[i]<<'"';}std::cout<<']';}
void json(const Matrix& m){std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';Vector row(m.cols());
    for(std::size_t j=0;j<row.size();++j)row[j]=m.get(i,j);
    json(row);}std::cout<<']';}
void numeric_corpus(bool emit){
    for(std::size_t rows=0;rows<6;++rows)for(std::size_t cols=0;cols<6;++cols)for(unsigned mode=0;mode<4;++mode){
        Matrix a(rows,cols);
        for(std::size_t i=0;i<rows;++i)for(std::size_t j=0;j<cols;++j){
            const auto value=mode==0?Q{0}:mode==1?Q{static_cast<unsigned long>((i+1)*(j+1)),7}:
                mode==2?Q{i==j?1:0}:Q{static_cast<long>((2*i+3*j+1)%7)-3,static_cast<unsigned long>(i+j+2)};
            a.set(i,j,value);
        }
        const numeric::PreparedExactFrames frame(a);
        check(frame.original()==a && frame.image().multiply(frame.coordinates())==a,"whole image coordinate factorization");
        check(a.multiply(frame.kernel()).nonzeros()==0,"complete kernel residual");
        check(frame.rank()+frame.kernel().cols()==cols && frame.image().rank()==frame.rank() &&
              frame.kernel().rank()==frame.kernel().cols(),"rank, nullity and independent frames");
        check(frame.image()==a.image_frame() && frame.kernel()==a.kernel_frame(),"canonical reference frame ordering");
        if(emit){std::cout<<"{\"kind\":\"numeric\",\"rows\":"<<rows<<",\"cols\":"<<cols<<",\"rank\":"<<frame.rank();
            for(const auto& [name,map]:std::array<std::pair<const char*,const Matrix*>,4>{{{"A",&a},{"F",&frame.image()},{"R",&frame.coordinates()},{"N",&frame.kernel()}}}){
                std::cout<<",\""<<name<<"\":";json(*map);}
            std::cout<<"}\n";
        }
        ++numeric_cases;
    }
}
void native_corpus(bool emit){
    const auto squares=native_green_example::square();
    const std::vector<core::RelationalComplex> sources{empty(0),empty(3),isolated(),pair(),branch(),squares.first.source,squares.second.source,higher()};
    for(unsigned mode=0;mode<3;++mode)for(const auto& source:sources){
        NativeAccessionDirectory directory(endpoint(source,mode),900);const runtime::WorkAvailability available{900,7};
        for(std::size_t k=0;k<source.grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            const auto plan=Frames::prepare(input(directory,grade),request(directory,grade,mode),available);
            const auto& canonical=plan.numeric_plan();const auto& G=canonical.gradient_frame();
            const auto& C=canonical.curl_frame();const auto& H=canonical.harmonic_frame();
            const auto n=source.grade(grade).cell_count();const auto M=metric(n,mode,k);
            const auto B=k?field::materialize_boundary_matrix(source,grade):Matrix(0,n);
            const auto U=k<source.max_grade().value?field::materialize_boundary_matrix(source,core::Grade{static_cast<std::uint16_t>(k+1u)}):Matrix(n,0);
            check(G.cols()+C.cols()+H.cols()==n && G.rank()==G.cols() && C.rank()==C.cols() && H.rank()==H.cols(),"complete independent Hodge sector frames");
            check(B.multiply(H).nonzeros()==0 && U.transpose().multiply(M).multiply(H).nonzeros()==0,"whole harmonic simultaneous kernel");
            check(G.transpose().multiply(M).multiply(C).nonzeros()==0 && G.transpose().multiply(M).multiply(H).nonzeros()==0 &&
                  C.transpose().multiply(M).multiply(H).nonzeros()==0,"full declared metric orthogonality");
            const auto before=encode_temporal_endpoint(*plan.publication().endpoint());const auto prep=plan.preparation_steps();
            for(unsigned sample=0;sample<2;++sample){
                const auto field_pin=input(directory,grade,sample);const auto x=inference::native_field_detail::dense(*field_pin.tensor());
                const auto result=plan.execute(field_pin,available);const auto& values=result.values();const auto& report=result.provenance();
                check(values.reconstructed==x && numeric::add(numeric::add(values.gradient,values.curl),values.harmonic)==x,"complete field reconstruction");
                check(G.apply(values.gradient_coordinates)==values.gradient && C.apply(values.curl_coordinates)==values.curl &&
                      H.apply(values.harmonic_coordinates)==values.harmonic,"each exact coordinate realization");
                check(report.sequence==7 && report.source_namespace==900 && report.carrier==10+k && report.grade==grade &&
                      report.input_field==100+2*k+sample && report.chain_interfaces_checked==(source.grade_count()>2?source.grade_count()-2:0),
                      "actual field, grade, carrier, complete family and endpoint witnesses");
                check(plan.preparation_steps()==prep && report.execution_steps>0 &&
                      &result.plan().numeric_plan().gradient_frame()==&G,"actual reusable prepared frame owner");
                if(emit){
                    std::cout<<"{\"kind\":\"native\",\"n\":"<<n<<",\"lower\":"<<B.rows()<<",\"upper\":"<<U.cols()
                             <<",\"g\":"<<G.cols()<<",\"c\":"<<C.cols()<<",\"h\":"<<H.cols();
                    for(const auto& [name,map]:std::array<std::pair<const char*,const Matrix*>,6>{{{"B",&B},{"U",&U},{"M",&M},{"G",&G},{"C",&C},{"H",&H}}}){
                        std::cout<<",\""<<name<<"\":";json(*map);}
                    for(const auto& [name,value]:std::array<std::pair<const char*,const Vector*>,8>{{{"x",&x},{"gradient",&values.gradient},{"curl",&values.curl},
                        {"harmonic",&values.harmonic},{"gc",&values.gradient_coordinates},{"cc",&values.curl_coordinates},{"hc",&values.harmonic_coordinates},{"reconstructed",&values.reconstructed}}}){
                        std::cout<<",\""<<name<<"\":";json(*value);}
                    std::cout<<"}\n";
                }
                ++native_cases;
            }
            check(encode_temporal_endpoint(*plan.publication().endpoint())==before,"all source assertions and retained occurrences remain unchanged");
        }
    }
}
void contracts(){
    const auto e=endpoint(higher(),2);NativeAccessionDirectory directory(e,900),clone(e,900);
    const core::Grade grade{1};const runtime::WorkAvailability available{900,7};const auto field_pin=input(directory,grade);
    const auto req=request(directory,grade,2);const auto plan=Frames::prepare(field_pin,req,available);
    auto bad=req;bad.grade_carriers[0]=300;
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"equal dimension explicit carrier is not SourceGrade");
    bad=req;std::swap(bad.grade_carriers[0],bad.grade_carriers[1]);
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"equal-size different grade carriers cannot substitute");
    bad=req;bad.grade_carriers.pop_back();rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"full grade family is required");
    bad=req;bad.metric_tensors.pop_back();rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"all grade metrics required");
    bad=req;bad.metric_tensors[0]=metric_tensor(clone,50);
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"equal foreign metric bytes do not identify publication or authority");
    bad=req;bad.metric_tensors[0]=input(directory,core::Grade{0});
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"field view is not a bilinear-form metric tensor");
    bad=req;bad.metrics=static_cast<inference::NativeMetricSelection>(255);
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"unknown metric choice");
    bad=req;bad.dimension_limit=0;rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,bad,available);},"positive dimension bound");
    bad=req;bad.dimension_limit=4;rejects<std::length_error>([&]{Frames::prepare(field_pin,bad,available);},"whole stacked harmonic map dimension bound");
    rejects<std::invalid_argument>([&]{plan.execute(input(clone,grade),available);},"equal foreign fields reject");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory,core::Grade{0}),available);},"equal dimension field grade remains distinct");
    rejects<std::invalid_argument>([&]{plan.execute(directory.pin(access(NativeAccessionKind::Tensor,102)),available);},"field tensor view rejects");
    rejects<std::invalid_argument>([&]{plan.execute(directory.pin(access(NativeAccessionKind::Field,3001)),available);},"foreign coordinate carrier rejects");
    for(const auto cutoff:std::array<runtime::WorkAvailability,3>{{{900,6},{901,7},{0,7}}}){
        rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,req,cutoff);},"explicit preparation cutoff required");
        rejects<std::invalid_argument>([&]{plan.execute(field_pin,cutoff);},"current cutoff still binds prepared owner");
    }
    for(unsigned resource=0;resource<5;++resource){runtime::ExecutionHostPolicy policy;
        switch(resource){case 0:policy.limits.records=0;break;case 1:policy.limits.rational_slots=0;break;
            case 2:policy.limits.sparse_entries=0;break;case 3:policy.limits.scalar_work=0;break;default:policy.limits.integer_bits=0;break;}
        rejects<std::length_error>([&]{Frames::prepare(field_pin,req,available,runtime::ExecutionLease{policy});},"current frame preparation bounds");
        rejects<std::length_error>([&]{plan.execute(field_pin,available,runtime::ExecutionLease{policy});},"current frame application bounds");
    }
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{plan.execute(field_pin,available,runtime::ExecutionLease{policy});},"allocation-disabled exact output rejects");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{Frames::prepare(field_pin,req,available,runtime::ExecutionLease{policy});},"unavailable exact GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(Frames::prepare(field_pin,req,available,runtime::ExecutionLease{policy}).execute(field_pin,available).provenance().fallback,"explicit CPU fallback is recorded");
    auto stop=std::make_shared<std::atomic_bool>(true);runtime::ExecutionLease cancelled({},stop);
    rejects<runtime::ExecutionCancelled>([&]{Frames::prepare(field_pin,req,available,cancelled);},"owned preparation cancellation");
    rejects<runtime::ExecutionCancelled>([&]{plan.execute(field_pin,available,cancelled);},"owned frame application cancellation");
    stop->store(false);check(plan.execute(field_pin,available,cancelled).values().reconstructed.size()==4,"cancelled work does not mutate reusable frames");
    for(unsigned fault=0;fault<4;++fault){auto broken=e;auto& last=broken.tensors[12];
        if(fault==0)last.tensor=temporal_matrix_tensor(Matrix::from_rows({{-1}}));
        else if(fault==1)last.tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain({1,1}),numeric::StructuralSupport::sparse(1,{}));
        else if(fault==2)last.role=TemporalTensorRole::OpaqueExact;
        else last.tensor=temporal_matrix_tensor(Matrix(1,1));
        rejects<std::invalid_argument>([&]{NativeAccessionDirectory invalid(broken,900);
            Frames::prepare(input(invalid,core::Grade{0}),request(invalid,core::Grade{0},2),available);},
            "unselected grade metric/support/role remains binding");
    }
    for(const auto& pivot:{Q{-1},Q{0}}){
        std::vector<Matrix> metrics;for(std::size_t k=0;k<e.source.grade_count();++k)
            metrics.push_back(metric(e.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(),2,k));
        metrics.back()=Matrix::from_rows({{pivot}});
        rejects<std::invalid_argument>([&]{field::PreparedHodgeFrames(std::make_shared<const core::RelationalComplex>(e.source),metrics,core::Grade{0});},
            "frame preparation independently validates an unselected positive metric");
    }
    auto partial=e;partial.tensors[5].tensor=numeric::SparseRationalTensor::zero(numeric::IndexDomain::linear(4),numeric::StructuralSupport::sparse(4,{0}));
    NativeAccessionDirectory incomplete(partial,900);
    const auto incomplete_plan=Frames::prepare(input(incomplete,grade),request(incomplete,grade,2),available);
    rejects<std::invalid_argument>([&]{incomplete_plan.execute(input(incomplete,grade),available);},"supported zero is not absent field support");
    auto capped=e;NativeAccessionLimits native_limits;native_limits.max_action_work=1;
    NativeAccessionDirectory limited(capped,900,{},[](auto bytes){return rfcx_detail::fnv1a64(bytes);},native_limits);
    rejects<std::length_error>([&]{Frames::prepare(input(limited,grade),request(limited,grade,2),available);},"native accession work cap binds setup");
    auto corrupt=higher();auto& top=const_cast<core::GradeArena&>(corrupt.grade(core::Grade{3}));top.lattice_coefficient[0]+=1;
    std::vector<Matrix> unit;for(std::size_t k=0;k<corrupt.grade_count();++k)unit.push_back(Matrix::identity(corrupt.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()));
    rejects<std::invalid_argument>([&]{field::PreparedHodgeFrames(std::make_shared<const core::RelationalComplex>(corrupt),unit,core::Grade{0});},
        "fault-injected remote boundary interface is checked outside selected grade");
    // Every raw value fits one bit; the complete kernel requires 1/2.
    numeric::PreparedExactFrames grown(Matrix::from_rows({{1,1,0},{1,-1,1}}));numeric::ExactActionLimits bit_limit;bit_limit.integer_bits=1;
    numeric::ExactActionGuard guard(bit_limit);rejects<std::length_error>([&]{grown.validate(guard);},"retained frame bits are current guarded values");
    NativeAccessionDirectory metric_growth(endpoint(isolated(),2),900);const auto metric_field=input(metric_growth,core::Grade{0});
    const auto metric_plan=Frames::prepare(metric_field,request(metric_growth,core::Grade{0},2),available);
    policy={};policy.limits.integer_bits=3;
    rejects<std::length_error>([&]{metric_plan.execute(metric_field,available,runtime::ExecutionLease{policy});},"unused retained metric factor growth remains current guarded");
    numeric::PreparedExactFrames retained_map(Matrix::identity(2));bool replaced=false;
    numeric::ExactActionLimits retained_limits;retained_limits.dimension=2;
    numeric::ExactActionGuard map_checkpoint(retained_limits,[&]{if(!replaced){replaced=true;retained_map=numeric::PreparedExactFrames(Matrix::identity(3));}});
    retained_map.validate(map_checkpoint);
    check(retained_map.rank()==3,"frame validation pins its original owner before a checkpoint changes the public handle");
    field::PreparedHodgeFrames retained_fields(std::make_shared<const core::RelationalComplex>(isolated()),{Matrix::identity(3)},core::Grade{0});
    const Vector expected{1,2,3};Vector mutable_input=expected;replaced=false;
    numeric::ExactActionGuard field_checkpoint({},[&]{if(!replaced){replaced=true;mutable_input.clear();
        retained_fields=field::PreparedHodgeFrames(std::make_shared<const core::RelationalComplex>(empty(0)),{Matrix(0,0)},core::Grade{0});}});
    const auto captured=retained_fields.decompose(mutable_input,field_checkpoint);
    check(captured.reconstructed==expected && captured.harmonic==expected && retained_fields.harmonic_frame().rows()==0,
          "field action captures its input and owner before checkpoints");
}
void lifetime(){
    const auto squares=native_green_example::square();auto e=endpoint(squares.first.source,0);
    NativeAccessionDirectory directory(e,900);const runtime::WorkAvailability available{900,7};const auto field_pin=input(directory,core::Grade{1});
    const auto plan=Frames::prepare(field_pin,request(directory,core::Grade{1},0),available);const auto old=plan.execute(field_pin,available);
    const auto bytes=encode_temporal_endpoint(*field_pin.endpoint());auto later=endpoint(squares.second.source,0,8);
    rejects<std::invalid_argument>([&]{make_temporal_segment(e,later);},"new explicit test coordinates require their complete declared comparison");
    directory.advance(make_temporal_segment(e,later,{{302,Matrix(1,0)}}));
    check(plan.execute(field_pin,available).values().harmonic==old.values().harmonic,"old frame survives face birth");
    rejects<std::invalid_argument>([&]{plan.execute(input(directory,core::Grade{1}),{900,8});},"later same-key field cannot enter old frame");
    check(encode_temporal_endpoint(*field_pin.endpoint())==bytes,"past source bytes and empty upper grade remain retained");
    const auto new_plan=Frames::prepare(input(directory,core::Grade{1}),request(directory,core::Grade{1},0),{900,8});
    check(plan.numeric_plan().harmonic_frame().cols()==1 && new_plan.numeric_plan().harmonic_frame().cols()==0 &&
          new_plan.numeric_plan().curl_frame().cols()==1,"attachment replaces exactly the harmonic direction with curl");
    std::vector<std::future<Vector>> jobs;for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[plan,field_pin,available]{
        return plan.execute(field_pin,available).values().reconstructed;}));
    for(auto& job:jobs)check(job.get()==old.values().reconstructed,"independent concurrent frame actions");
    const auto retained=[&]{NativeAccessionDirectory local(e,900);const auto input_pin=input(local,core::Grade{1});
        return Frames::prepare(input_pin,request(local,core::Grade{1},0),available).execute(input_pin,available);}();
    check(retained.plan().execute(retained.input(),available).values().reconstructed==old.values().reconstructed,"result owns frame, field and publication after directory destruction");
}
} // namespace
int main(int argc,char** argv){try{
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    numeric_corpus(emit);native_corpus(emit);contracts();lifetime();
    if(!emit)std::cout<<"native_frame_checks="<<checks<<" numeric_cases="<<numeric_cases<<" native_cases="<<native_cases<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
