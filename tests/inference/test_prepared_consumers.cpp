#include "rfc/learning/native_metric_layer.hpp"
#include "rfc/learning/native_exact_trainer.hpp"
#include "rfc/storage/prepared_source_program.hpp"
#include "rfc/runtime/query_mask.hpp"
#include "../storage/native_accession_fixture.hpp"
#include <chrono>
#include <iostream>
#include <random>

using namespace rfc;
using namespace rfc::storage;
using Q=numeric::Rational;
using K=field::OperatorKind;
using C=learning::ExactMetricChannelKind;
namespace {
std::size_t checks=0;
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,text);}
NativeAccessionRequest tensor_request(NativeAccessionKind kind,std::uint64_t key){NativeAccessionRequest r;r.kind=kind;r.subject=key;return r;}
TemporalEndpoint endpoint(unsigned seed){
    core::RelationSignature s;const auto type=s.add_type("finite-bridge",core::BoundaryPolicy::DeclaredExact,0,10);
    core::RelationalComplexBuilder b(s);
    const Q left=seed==22?Q{1,101}:Q{1},right=seed==22?Q{1,103}:Q{1},cancel=seed==22?Q{1,107}:Q{2};
    b.add_grade_one(core::RelationKey{100},type,{{10,true,false,left},{11,true,true,right},
        {10,true,false,cancel},{10,true,true,cancel},{12,false,false,Q{7}}});
    b.add_grade_one(core::RelationKey{101},type,{{10,true,false,left},{11,true,true,right}});
    b.add_grade_one(core::RelationKey{102},type,{{11,true,false,Q{1}},{12,true,true,Q{1}}});
    b.add_higher(core::Grade{2},core::RelationKey{200},type,{{0,true,false,Q{2}},{1,true,true,Q{2}}});
    b.add_higher(core::Grade{2},core::RelationKey{201},type,{});
    TemporalEndpoint e(b.freeze());
    for(unsigned k=0;k<3;++k){
        const auto n=e.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
        e.carriers.push_back({10+k,TemporalCarrierKind::SourceGrade,core::Grade{static_cast<std::uint16_t>(k)},{},0,numeric::IndexDomain::linear(n)});
        TemporalTensor x;x.key=1000+k;x.role=TemporalTensorRole::Field;x.output=10+k;x.interpretation="full native grade test field";
        std::vector<numeric::SparseRationalEntry> entries;
        for(std::size_t i=0;i<n;++i){Q q{static_cast<long>((seed+static_cast<unsigned>(i)*3)%13)-6,static_cast<unsigned long>(i+3)};if(seed==23){numeric::Integer numerator=1,denominator=1;mpz_mul_2exp(numerator.get_mpz_t(),numerator.get_mpz_t(),257);mpz_mul_2exp(denominator.get_mpz_t(),denominator.get_mpz_t(),131);q=Q(numerator+static_cast<unsigned long>(i+1),denominator+17);}
            q.canonicalize();if(q!=0)entries.push_back({i,q});}
        x.tensor={numeric::IndexDomain::linear(n),numeric::StructuralSupport::full(n),numeric::SparseRationalVector(n,entries)};e.tensors.push_back(x);
        x.key=2000+k;x.role=TemporalTensorRole::Metric;x.input=10+k;numeric::ExactMatrix m(n,n);
        for(std::size_t i=0;i<n;++i){Q q{static_cast<unsigned long>((seed+static_cast<unsigned>(i))%7+1),static_cast<unsigned long>(i+1)};q.canonicalize();m.set(i,i,q);}
        x.tensor=temporal_matrix_tensor(m);e.tensors.push_back(x);
    }
    e.carriers.push_back({20,TemporalCarrierKind::Explicit,core::Grade{0},{},900,numeric::IndexDomain::linear(5)});
    e.carriers.push_back({21,TemporalCarrierKind::Explicit,core::Grade{0},{},901,numeric::IndexDomain::linear(1)});
    TemporalTensor p;p.key=3000;p.role=TemporalTensorRole::Field;p.output=20;p.interpretation="original ordered channel gains";
    p.tensor={numeric::IndexDomain::linear(5),numeric::StructuralSupport::full(5),numeric::SparseRationalVector(5,{{0,Q{2}},{1,Q{-3}},{2,Q{4}},{3,Q{5}}})};e.tensors.push_back(p);
    p.key=3001;p.output=21;p.interpretation="declared uniform scalar bias";
    p.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{2,3}}})};e.tensors.push_back(p);
    validate_temporal_endpoint(e);return e;
}
inference::NativeExactTowerRequest request(const NativeAccessionDirectory& d,bool weighted=true){
    inference::NativeExactTowerRequest r;r.grade_carriers={10,11,12};
    r.metrics=weighted?inference::NativeMetricSelection::DeclaredDiagonal:inference::NativeMetricSelection::NamedUnit;
    if(weighted)for(unsigned k=0;k<3;++k)r.metric_tensors.push_back(d.pin(tensor_request(NativeAccessionKind::Tensor,2000+k)));
    return r;
}
auto anchor(const NativeAccessionDirectory& d){return d.pin(tensor_request(NativeAccessionKind::Field,1001));}
std::vector<learning::ExactMetricChannel> channels(){return {{C::Identity,0},{C::HodgeDown,0},{C::HodgeUp,0},{C::Hodge,0},{C::HodgeDown,0}};}
void vector_json(const numeric::RationalVector& x){std::cout<<'[';for(std::size_t i=0;i<x.size();++i){if(i)std::cout<<',';std::cout<<'"'<<x[i]<<'"';}std::cout<<']';}
void emit_case(const inference::PreparedNativeExactTower& prepared,const storage::PinnedNativeAccession& input,K kind,core::Grade grade,const numeric::RationalVector& values){
    std::cout<<"{\"kind\":"<<static_cast<unsigned>(kind)<<",\"grade\":"<<grade.value<<",\"input\":";
    vector_json(prepared.input_values(input,kind==K::Adjoint?core::Grade{static_cast<std::uint16_t>(grade.value-1u)}:grade));
    std::cout<<",\"metrics\":[";
    const auto& source=prepared.publication().endpoint()->source;
    for(unsigned k=0;k<3;++k){if(k)std::cout<<',';const auto n=source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();numeric::RationalVector diag;
        const auto m=prepared.tower().metrics()[k].materialize();for(std::size_t i=0;i<n;++i)diag.push_back(m.get(i,i));vector_json(diag);}
    std::cout<<"],\"source\":[";
    for(unsigned k=0;k<3;++k){
        if(k)std::cout<<',';
        const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
        std::cout<<"{\"scales\":";vector_json(arena.cell_scales);std::cout<<",\"occurrences\":[";
        for(std::size_t i=0;i<arena.incidence_count();++i){
            if(i)std::cout<<',';
            std::cout<<"{\"upper\":"<<arena.upper_index[i]<<",\"lower\":"<<arena.lower_index[i]
                <<",\"lattice\":\""<<arena.lattice_coefficient[i]<<"\",\"exists\":"
                <<(arena.aspects.get(source.signature().existence_aspect(),i)?"true":"false")<<'}';
        }
        std::cout<<"]}";
    }
    std::cout<<"],\"output\":";vector_json(values);std::cout<<"}\n";
}
void exact_tests(bool emit){
    for(unsigned seed=0;seed<24;++seed){
        NativeAccessionDirectory d(endpoint(seed),88,{},[](auto){return 0;});const auto pub=anchor(d);
        for(bool weighted:{false,true}){
            const auto prepared=inference::PreparedNativeExactTower::prepare(pub,request(d,weighted));
            const auto ref=field::ExactExecutionTower::weighted(pub.endpoint()->source,prepared.tower().metrics());
            for(unsigned k=0;k<3;++k)for(auto kind:{K::Boundary,K::Adjoint,K::HodgeDown,K::HodgeUp,K::Hodge}){
                if(!k && (kind==K::Boundary || kind==K::Adjoint))continue;
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                const auto input=d.pin(tensor_request(NativeAccessionKind::Field,1000+(kind==K::Adjoint?k-1:k)));
                const auto result=prepared.execute({kind,grade,0},input);
                check(result.values()==ref.apply({kind,grade,0},prepared.input_values(input,kind==K::Adjoint?core::Grade{static_cast<std::uint16_t>(k-1)}:grade)),"native finite reference mismatch");
                check(result.provenance().input_field==input.tensor_declaration()->key && result.provenance().metric_selection==request(d,weighted).metrics,"native finite provenance");
                if(emit)emit_case(prepared,input,kind,grade,result.values());
            }
            const auto gains=d.pin(tensor_request(NativeAccessionKind::Field,3000)),bias=d.pin(tensor_request(NativeAccessionKind::Field,3001));
            const learning::PreparedNativeMetricLayer model(prepared,core::Grade{1},channels(),gains,bias,{"original-five-channel-model",900,901});
            const auto proposal=model.forward(pub);
            const learning::ExactMetricChannelContext ctx(pub.endpoint()->source,core::Grade{1},prepared.tower().metrics());
            const learning::CompiledExactMetricLayer ref_model(ctx,channels(),{Q{2},Q{-3},Q{4},Q{5},Q{0}},Q{2,3});
            check(proposal.values()==ref_model.forward(prepared.input_values(pub,core::Grade{1})),"model bridge reference mismatch");
            check(proposal.provenance().prospective && proposal.provenance().original_channels.size()==5 && proposal.gains().tensor_declaration()->key==3000,"original parameter provenance");
            if(emit)emit_case(prepared,pub,static_cast<K>(99),core::Grade{1},proposal.values());
        }
    }
}
void admission_lifetime(){
    NativeAccessionDirectory d(endpoint(7),88);const auto pub=anchor(d);const auto req=request(d);
    const auto prepared=inference::PreparedNativeExactTower::prepare(pub,req);
    NativeAccessionDirectory equal(endpoint(7),88);
    rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},anchor(equal));},"equal bytes established publication identity");
    rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},d.pin(tensor_request(NativeAccessionKind::Field,3000)));},"equal dimensions identified carrier");
    rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},d.pin(tensor_request(NativeAccessionKind::Field,1000)));},"equal grade dimensions established carrier identity");
    for(auto kind:{K::Green,K::Dirac,K::HarmonicProjector,static_cast<K>(255)})rejects([&]{prepared.execute({kind,core::Grade{1},0},pub);},"unsupported native operator accepted");
    rejects([&]{prepared.execute({K::Hodge,core::Grade{1},Q{1}},pub);},"unused lambda accepted");
    NativeAccessionLimits small_limits;small_limits.max_action_work=0;
    NativeAccessionDirectory bounded(endpoint(7),88,{},[](auto){return 0;},small_limits);
    rejects([&]{inference::PreparedNativeExactTower::prepare(anchor(bounded),request(bounded));},"F3 finite work cap ignored");
    auto bad=req;bad.grade_carriers={11,10,12};rejects([&]{inference::PreparedNativeExactTower::prepare(pub,bad);},"wrong carrier order accepted");
    bad=req;bad.metric_tensors[0]=req.metric_tensors[1];rejects([&]{inference::PreparedNativeExactTower::prepare(pub,bad);},"wrong metric identity accepted");
    runtime::ExecutionHostPolicy policy;policy.limits.integer_bits=1;
    rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},pub,runtime::ExecutionLease(policy));},"growth cap ignored");
    policy={};policy.limits.scalar_work=0;rejects([&]{inference::PreparedNativeExactTower::prepare(pub,req,policy);},"preparation cap ignored");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},pub,runtime::ExecutionLease(policy));},"absent backend accepted");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    check(prepared.execute({K::Hodge,core::Grade{1},0},pub,runtime::ExecutionLease(policy)).provenance().fallback,"fallback provenance missing");
    auto cancelled=std::make_shared<std::atomic_bool>(true);rejects([&]{prepared.execute({K::Hodge,core::Grade{1},0},pub,runtime::ExecutionLease({},cancelled));},"cancellation ignored");
    const auto gains=d.pin(tensor_request(NativeAccessionKind::Field,3000)),bias=d.pin(tensor_request(NativeAccessionKind::Field,3001));
    const auto fallback_tower=inference::PreparedNativeExactTower::prepare(pub,req,policy);
    const learning::PreparedNativeMetricLayer fallback_model(fallback_tower,core::Grade{1},channels(),gains,bias,{"inherited-fallback",900,901});
    const auto fallback_result=fallback_model.forward(pub);
    check(fallback_result.provenance().fallback && fallback_result.provenance().tower_preparation_policy.backend==runtime::ExecutionBackend::ExactGpu,
          "model lost inherited tower fallback provenance");
    auto invalid=channels();invalid.back()={C::GreenResolvent,Q{1}};
    rejects([&]{learning::PreparedNativeMetricLayer m(prepared,core::Grade{1},invalid,gains,bias,{"zero-gain-unsupported",900,901});},"unsupported zero gain channel hidden");
    rejects([&]{learning::PreparedNativeMetricLayer m(prepared,core::Grade{1},channels(),gains,bias,{"wrong",901,901});},"parameter coordinate identity ignored");
    auto offdiag=endpoint(7);auto matrix=temporal_tensor_matrix(offdiag.tensors[1]);
    matrix.set(0,1,Q{1,100});matrix.set(1,0,Q{1,100});offdiag.tensors[1].tensor=temporal_matrix_tensor(matrix);
    NativeAccessionDirectory offdiag_directory(offdiag,88);
    rejects([&]{inference::PreparedNativeExactTower::prepare(anchor(offdiag_directory),request(offdiag_directory));},"non-diagonal metric accepted by finite profile");
    auto incomplete=endpoint(7);auto& stored=incomplete.tensors[2].tensor;
    stored={stored.domain(),numeric::StructuralSupport::sparse(3,{0,1}),numeric::SparseRationalVector(3,{{0,Q{1}}})};
    NativeAccessionDirectory partial(incomplete,88);const auto partial_tower=inference::PreparedNativeExactTower::prepare(anchor(partial),request(partial));
    rejects([&]{partial_tower.execute({K::Hodge,core::Grade{1},0},anchor(partial));},"absent field direction became zero");
    learning::PreparedNativeMetricLayer kept_parameters(prepared,core::Grade{1},channels(),gains,bias,{"parameter-pins",900,901});
    policy={};policy.allow_allocation=false;
    rejects([&]{kept_parameters.forward(pub,runtime::ExecutionLease(policy));},"model owning allocation policy ignored");
    rejects([&]{kept_parameters.forward(pub,runtime::ExecutionLease({},cancelled));},"model cancellation ignored");
    NativeAccessionDirectory scaled(endpoint(22),88);const auto scaled_input=anchor(scaled);
    policy={};policy.limits.integer_bits=10;
    rejects([&]{inference::PreparedNativeExactTower::prepare(scaled_input,request(scaled),policy);},"retained lattice factor growth cap ignored");
    auto empty=native_accession_fixture::endpoint();
    empty.carriers.push_back({3,TemporalCarrierKind::SourceGrade,core::Grade{2},{},0,numeric::IndexDomain::linear(0)});
    NativeAccessionDirectory empty_directory(empty,800);inference::NativeExactTowerRequest empty_request;
    empty_request.grade_carriers={2,1,3};
    rejects([&]{inference::PreparedNativeExactTower::prepare(empty_directory.pin(tensor_request(NativeAccessionKind::Field,101)),empty_request);},"empty grade silently discarded");
    const auto kept=[&]{NativeAccessionDirectory temporary(endpoint(7),88);const auto p=anchor(temporary);
        return std::pair{learning::PreparedNativeMetricLayer(inference::PreparedNativeExactTower::prepare(p,request(temporary)),core::Grade{1},channels(),
            temporary.pin(tensor_request(NativeAccessionKind::Field,3000)),temporary.pin(tensor_request(NativeAccessionKind::Field,3001)),{"lifetime",900,901}),p};}();
    const auto old=kept.first.forward(kept.second);
    check(old.values().size()==3 && old.provenance().metrics.size()==3,"owning model lifetime lost");
    auto moved=std::move(kept.first);
    check(moved.forward(kept.second).values()==old.values(),"model rvalue lost pin");
    rejects([&]{kept.first.forward(pub);},"model accepted different publication");
    check(kept.first.forward(kept.second).values()==old.values(),"failed model call changed previous output");
}
void global_tests(bool emit){
    const auto emit_matrix=[](const numeric::ExactMatrix& m){
        std::cout<<'[';for(std::size_t i=0;i<m.rows();++i){if(i)std::cout<<',';numeric::RationalVector row;
            for(std::size_t j=0;j<m.cols();++j)row.push_back(m.get(i,j));vector_json(row);}std::cout<<']';
    };
    for(unsigned seed=0;seed<12;++seed){
        auto e=endpoint(seed);
        for(auto& tensor:e.tensors)if(tensor.role==TemporalTensorRole::Metric){
            const auto n=tensor.tensor.domain().extents()[0];numeric::ExactMatrix metric(n,n);
            for(std::size_t i=0;i<n;++i){metric.set(i,i,Q{4});if(i){metric.set(i-1,i,Q{-1,3});metric.set(i,i-1,Q{-1,3});}}
            tensor.tensor=temporal_matrix_tensor(metric);
        }
        NativeAccessionDirectory d(e,88);const auto pub=anchor(d);auto req=request(d);
        req.metrics=inference::NativeMetricSelection::DeclaredPositive;req.profile=field::PreparedExactProfile::GlobalActions;
        const auto prepared=inference::PreparedNativeExactTower::prepare(pub,req);
        const auto canonical=encode_temporal_endpoint(*pub.endpoint());
        const auto reference=field::ExactExecutionTower::weighted(pub.endpoint()->source,prepared.tower().metrics());
        for(unsigned k=0;k<3;++k)for(auto kind:{K::Hodge,K::Green,K::HarmonicProjector}) {
            const core::Grade grade{static_cast<std::uint16_t>(k)};
            const auto input=d.pin(tensor_request(NativeAccessionKind::Field,1000+k));
            for(const auto& lambda:kind==K::Green?numeric::RationalVector{Q{0},Q{2,7},Q{-1,997}}:numeric::RationalVector{Q{0}}) {
                const field::OperatorRequest operation{kind,grade,lambda};const auto result=prepared.execute(operation,input);
                const auto x=prepared.input_values(input,grade);
                if(kind!=K::Green || lambda>0)check(result.values()==reference.apply(operation,x),"global owning reference mismatch");
                if(kind==K::Green) {
                    field::ExactHodgeOperator hodge(pub.endpoint()->source,grade,prepared.tower().metrics());
                    check(numeric::add(result.values(),numeric::scale(lambda,hodge.apply(result.values())))==x,"global exact Green residual");
                }
                check(result.provenance().profile=="source-grade-global-exact-v1","global profile provenance absent");
                if(emit) {
                    std::cout<<"{\"kind\":"<<static_cast<unsigned>(kind)<<",\"grade\":"<<k<<",\"lambda\":\""<<lambda<<"\",\"input\":";vector_json(x);
                    std::cout<<",\"metrics\":[";for(std::size_t j=0;j<3;++j){if(j)std::cout<<',';emit_matrix(prepared.tower().metrics()[j].materialize());}
                    std::cout<<"],\"boundaries\":[";for(std::uint16_t j=1;j<3;++j){if(j>1)std::cout<<',';emit_matrix(field::materialize_boundary_matrix(pub.endpoint()->source,core::Grade{j}));}
                    std::cout<<"],\"output\":";vector_json(result.values());std::cout<<"}\n";
                }
            }
        }
        check(canonical==encode_temporal_endpoint(*pub.endpoint()),"global execution changed retained native bytes");
        auto limited=req;limited.global_grade_dimension_limit=1;
        rejects([&]{inference::PreparedNativeExactTower::prepare(pub,limited);},"global dimension cap ignored");
        runtime::ExecutionHostPolicy policy;policy.limits.scalar_work=1;
        rejects([&]{prepared.execute({K::Green,core::Grade{1},Q{1}},pub,runtime::ExecutionLease(policy));},"global current work cap ignored");
    }
    auto e=endpoint(0);auto m=temporal_tensor_matrix(e.tensors[1]);m.set(0,0,Q{-1});e.tensors[1].tensor=temporal_matrix_tensor(m);
    rejects([&]{NativeAccessionDirectory invalid(e,88);auto req=request(invalid);
        req.metrics=inference::NativeMetricSelection::DeclaredPositive;req.profile=field::PreparedExactProfile::GlobalActions;
        inference::PreparedNativeExactTower::prepare(anchor(invalid),req);},"indefinite native metric accepted");
    // The negative resolvent parameter must reject an actually singular system.
    core::RelationSignature s;const auto type=s.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(s);b.add_grade_one(type,{{0,true,false,false,1},{1,true,true,false,1}});
    const auto source=std::make_shared<const core::RelationalComplex>(b.freeze());
    field::PreparedExactExecutionTower tower(source,{field::ExactMetric::identity(2),field::ExactMetric::identity(1)},field::PreparedExactProfile::GlobalActions);
    rejects([&]{tower.apply({K::Green,core::Grade{0},Q{-1,2}},{Q{1},Q{0}});},"singular Green system accepted");
    check(tower.apply({K::Green,core::Grade{0},Q{0}},{Q{1},Q{0}})==numeric::RationalVector({Q{1},Q{0}}),"zero Green is not identity");
}
void training_tests(){
    NativeAccessionDirectory d(endpoint(7),88);const auto input=anchor(d);
    const auto tower=inference::PreparedNativeExactTower::prepare(input,request(d));
    const auto gains=d.pin(tensor_request(NativeAccessionKind::Field,3000)),bias=d.pin(tensor_request(NativeAccessionKind::Field,3001));
    const learning::NativeExactTrainingContract contract{learning::NativeTrainingObjective::HalfMetricSquaredError,
        {"original-ordered-gradient-test",900,901},700,9900,9901,9902,2,0,9903};
    std::vector<learning::NativeExactTrainingSample> samples{{input,input,learning::NativeSamplePartition::Training,3,4,5}};
    const auto initial=learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,samples,contract);
    auto trained=initial;const auto initial_archive=encode_native_series(initial.history());
    // Differentiate the declared scalar quadrance independently by its exact
    // central difference; the loss is quadratic, so this derivative is exact.
    const auto x=tower.input_values(input,core::Grade{1});const auto& metric=tower.tower().metrics()[1];
    std::vector<numeric::RationalVector> responses;
    for(const auto& channel:channels()) {
        const auto kind=channel.kind==C::HodgeDown?K::HodgeDown:channel.kind==C::HodgeUp?K::HodgeUp:K::Hodge;
        responses.push_back(channel.kind==C::Identity?x:tower.tower().apply({kind,core::Grade{1},0},x));
    }
    const auto loss=[&](const numeric::RationalVector& parameters){
        numeric::RationalVector prediction(x.size(),parameters.back());
        for(std::size_t c=0;c<responses.size();++c)for(std::size_t i=0;i<x.size();++i)prediction[i]+=parameters[c]*responses[c][i];
        const auto error=numeric::sub(prediction,x);return Q{metric.inner(error,error)/2};
    };
    for(unsigned step=0;step<4;++step){
        const auto before=trained.parameters();auto expected=before;const Q rate{1,1000000};
        for(std::size_t p=0;p<before.size();++p){auto plus=before,minus=before;plus[p]+=Q{1,13};minus[p]-=Q{1,13};
            expected[p]-=rate*(loss(plus)-loss(minus))/Q{2,13};}
        trained=trained.advance(rate,3,5+step);
        check(trained.parameters()==expected && *trained.last_loss()==loss(before),"declared exact gradient central difference mismatch");
    }
    check(initial_archive==encode_native_series(initial.history()),"training changed a prior checkpoint");
    const auto archive=encode_native_series(trained.history());
    const auto resumed=learning::NativeExactTrainer::resume(initial,archive);
    check(resumed.parameters()==trained.parameters() && resumed.history().same_history(trained.history()),"complete native training resume mismatch");
    check(resumed.advance(Q{1,1000000},3,9).parameters()==trained.advance(Q{1,1000000},3,9).parameters(),"resumed next update mismatch");
    rejects([&]{initial.advance(Q{1},2,5);},"future event used for fit");
    rejects([&]{initial.advance(Q{1},3,4);},"future label used for fit");
    rejects([&]{trained.advance(Q{1},3,5);},"future fitted parameters used at old cutoff");
    samples[0].partition=learning::NativeSamplePartition::Evaluation;
    const auto evaluation=learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,samples,contract);
    rejects([&]{evaluation.advance(Q{1},3,5);},"evaluation label fit allowed");
    rejects([&]{learning::NativeExactTrainer::resume(evaluation,archive);},"changed data-order/partition accepted on resume");
    auto broken=archive;broken.back()^=std::byte{1};rejects([&]{learning::NativeExactTrainer::resume(initial,broken);},"corrupt training checkpoint accepted");
    auto changed=channels();std::swap(changed[0],changed[1]);
    const auto reordered=learning::NativeExactTrainer::prepare(tower,core::Grade{1},changed,gains,bias,
        {{input,input,learning::NativeSamplePartition::Training,3,4,5}},contract);
    rejects([&]{learning::NativeExactTrainer::resume(reordered,archive);},"same-width reordered original channels accepted on resume");
}
void packed_tests(){
    for(const auto n:{2u,21u,65u,130u}){
        NativeAccessionDirectory d(native_accession_fixture::endpoint(n),800,{},[](auto){return 0;});
        NativeAccessionRequest r;r.kind=NativeAccessionKind::SourceAspects;r.subject=400;r.occurrence={{core::Grade{1},core::OccurrenceKey{10000}}};
        const auto pin=d.resolve(d.intern(r));
        const auto collision=d.intern(tensor_request(NativeAccessionKind::Tensor,200));
        check(d.stats().bucket_comparisons>0,"forced accession collision was not exercised");
        rejects([&]{PreparedNativeSourceProgram wrong(d.resolve(collision),401);},"collision confused program accession kind");
        const PreparedNativeSourceProgram prepared(pin,401);composite::BinaryExecutionWorkspace ws;
        const auto ref=pin.execute_source_program(401);check(prepared.materialize(ws).words()==ref.words(),"prepared native packed mismatch");
        for(unsigned i=0;i<100;++i)check(prepared.count(ws)==ref.count(),"prepared native packed count mismatch");
        const auto growth=ws.growth_events();(void)prepared.materialize(ws);check(ws.growth_events()==growth,"warm binary scratch grew");
        rejects([&]{prepared.for_each_word([&](auto,auto){(void)prepared.count(ws);},ws);},"sink workspace reentry accepted");
        check(prepared.count(ws)==ref.count(),"throw poisoned binary scratch");
        std::size_t word=0;prepared.for_each_word([&](auto index,auto){check(index==word++,"sink callback order changed");},ws);
        const auto held=[&]{NativeAccessionDirectory temp(native_accession_fixture::endpoint(n),800);return PreparedNativeSourceProgram(temp.pin(r),401);}();
        check(held.materialize(ws).words()==ref.words(),"source pin lifetime lost");
        rejects([&]{PreparedNativeSourceProgram bad(pin,401,{0,1,1});},"binary preparation instruction cap ignored");
        auto replaceable=prepared;
        std::size_t callbacks=0;
        replaceable.for_each_word([&](auto,auto){++callbacks;replaceable=held;},ws);
        check(callbacks==ref.word_count(),"callback replacement changed pinned execution");
        auto moved=std::move(replaceable);
        check(moved.count(ws)==ref.count() && replaceable.count(ws)==ref.count(),"source program rvalue lost ownership");
        rejects([&]{PreparedNativeSourceProgram bad(pin,999);},"missing program accepted");
    }
    for(const auto n:{0u,1u,63u,64u,65u,130u}){
        machine::PackedBits a(n,true),b(n,false),c(n,true);for(std::size_t i=0;i<n;i+=3)b.set(i);
        runtime::PreparedMaskIntersection prepared(3);composite::BinaryExecutionWorkspace ws;
        check(prepared.apply({&a,&b,&c},ws).words()==(a&b&c).words(),"packed mask reference mismatch");
        rejects([&]{prepared.apply({&a,nullptr,&c},ws);},"null mask accepted");
        machine::PackedBits wrong(n+1);rejects([&]{prepared.apply({&a,&b,&wrong},ws);},"mask dimension mismatch accepted");
    }
}
}
int main(int argc,char** argv){try{
    if(argc>1 && std::string_view(argv[1])=="--global-oracle"){global_tests(true);return 0;}
    const bool emit=argc>1 && std::string_view(argv[1])=="--oracle";
    exact_tests(emit);if(!emit){admission_lifetime();global_tests(false);training_tests();packed_tests();std::cout<<"checks="<<checks<<"\n";}return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
