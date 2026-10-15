#include "rfc/learning/native_metric_layer.hpp"
#include "rfc/learning/native_exact_trainer.hpp"
#include "rfc/storage/prepared_source_program.hpp"
#include "rfc/runtime/query_mask.hpp"
#include "../storage/native_accession_fixture.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <future>

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
void rational_batch_tests(){
    const auto before = checks;
    for (unsigned seed : {0u, 7u, 22u, 23u}) {
        auto source = endpoint(seed);
        for (unsigned k = 0; k < 3; ++k) for (unsigned s = 0; s < 8; ++s) {
            auto tensor = *std::find_if(source.tensors.begin(), source.tensors.end(),
                [k](const auto& t) { return t.key == 1000 + k; });
            tensor.key = 5000 + k * 100 + s;
            std::vector<numeric::SparseRationalEntry> entries;
            for (std::size_t i = 0; i < tensor.tensor.domain().logical_size(); ++i) {
                Q value = *tensor.tensor.state(i) + Q(static_cast<long>(s) - 3, static_cast<unsigned long>(i + 2));
                if ((i + s) % 5 == 0) value = 0;
                if (value != 0) entries.push_back({i, value});
            }
            const auto n = tensor.tensor.domain().logical_size();
            tensor.tensor = {numeric::IndexDomain::linear(n), numeric::StructuralSupport::full(n),
                             numeric::SparseRationalVector(n, std::move(entries))};
            source.tensors.push_back(std::move(tensor));
        }
        validate_temporal_endpoint(source);
        NativeAccessionDirectory directory(source, 88);
        for (bool weighted : {false, true}) {
            const auto tower = inference::PreparedNativeExactTower::prepare(anchor(directory), request(directory, weighted));
            const auto reference = field::ExactExecutionTower::weighted(tower.publication().endpoint()->source, tower.tower().metrics());
            for (unsigned k = 0; k < 3; ++k) {
                const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
                auto workspace = tower.tower().make_batch_workspace(grade, 8);
                const auto slots = workspace.retained_slots();
                check(slots == tower.tower().batch_workspace_slots(grade, 8), "rational scratch accounting");
                for (std::size_t b : {1u, 3u, 8u, 2u}) {
                    for (auto kind : {K::Boundary, K::Adjoint, K::HodgeDown, K::HodgeUp, K::Hodge}) {
                        if (!k && (kind == K::Boundary || kind == K::Adjoint)) continue;
                        const auto input_grade = kind == K::Adjoint ? k - 1 : k;
                        std::vector<PinnedNativeAccession> inputs;
                        for (std::size_t s = 0; s < b; ++s)
                            inputs.push_back(directory.pin(tensor_request(NativeAccessionKind::Field, 5000 + input_grade * 100 + s)));
                        const auto result = tower.execute_batch({kind, grade, 0}, inputs, workspace);
                        check(result.samples() == b && result.inputs().size() == b && result.provenance().input_field == 0,
                              "rational batch provenance");
                        for (std::size_t s = 0; s < b; ++s) {
                            const auto input = tower.input_values(inputs[s], core::Grade{static_cast<std::uint16_t>(input_grade)});
                            const auto expected = reference.apply({kind, grade, 0}, input);
                            const auto scalar = tower.execute({kind, grade, 0}, inputs[s]);
                            for (std::size_t i = 0; i < expected.size(); ++i)
                                check(result.values()[i * b + s] == expected[i] && result.values()[i * b + s] == scalar.values()[i],
                                      "rational batch scalar/reference mismatch");
                        }
                    }
                    std::vector<PinnedNativeAccession> inputs;
                    for (std::size_t s = 0; s < b; ++s)
                        inputs.push_back(directory.pin(tensor_request(NativeAccessionKind::Field, 5000 + k * 100 + s)));
                    const learning::PreparedNativeMetricLayer layer(tower, grade, channels(),
                        directory.pin(tensor_request(NativeAccessionKind::Field, 3000)),
                        directory.pin(tensor_request(NativeAccessionKind::Field, 3001)), {"batch-channels", 900, 901});
                    const auto prediction = layer.forward_batch(inputs, workspace);
                    const learning::ExactMetricChannelContext context(tower.publication().endpoint()->source, grade, tower.tower().metrics());
                    const learning::CompiledExactMetricLayer expected(context, channels(), {Q{2}, Q{-3}, Q{4}, Q{5}, Q{0}}, Q{2,3});
                    for (std::size_t s = 0; s < b; ++s) {
                        const auto oracle = expected.forward(tower.input_values(inputs[s], grade));
                        for (std::size_t i = 0; i < oracle.size(); ++i)
                            check(prediction.values()[i * b + s] == oracle[i], "rational metric batch channel mismatch");
                    }
                    check(prediction.provenance().original_channels.size() == 5 && workspace.retained_slots() == slots,
                          "rational batch changed channels or scratch capacity");
                }
            }
        }
    }
    NativeAccessionDirectory directory(endpoint(7), 88), equal(endpoint(7), 88);
    const auto tower = inference::PreparedNativeExactTower::prepare(anchor(directory), request(directory));
    auto workspace = tower.tower().make_batch_workspace(core::Grade{1}, 2);
    std::vector<PinnedNativeAccession> inputs{anchor(directory), anchor(directory)};
    rejects([&]{ tower.tower().make_batch_workspace(core::Grade{1}, 0); }, "zero rational batch capacity accepted");
    rejects([&]{ tower.tower().batch_workspace_slots(core::Grade{1}, SIZE_MAX); }, "rational batch size overflow accepted");
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, {}, workspace); }, "empty rational batch accepted");
    inputs.push_back(anchor(directory));
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, workspace); }, "rational batch exceeded fixed capacity");
    inputs.pop_back();
    inputs[1] = anchor(equal);
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, workspace); }, "rational batch inferred publication identity");
    inputs[1] = inputs[0];
    auto wrong = tower.tower().make_batch_workspace(core::Grade{0}, 2);
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, wrong); }, "rational batch used another grade workspace");
    const auto other = inference::PreparedNativeExactTower::prepare(anchor(equal), request(equal));
    auto foreign = other.tower().make_batch_workspace(core::Grade{1}, 2);
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, foreign); }, "rational batch used another owner workspace");
    for (auto kind : {K::Green, K::HarmonicProjector, K::Dirac})
        rejects([&]{ tower.execute_batch({kind, core::Grade{1}, 0}, inputs, workspace); }, "unsupported rational batch operator accepted");
    runtime::ExecutionHostPolicy small; small.limits.rational_slots = 1;
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, workspace, runtime::ExecutionLease(small)); },
            "rational batch ignored slot admission");
    auto flag = std::make_shared<std::atomic_bool>(true);
    rejects([&]{ tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, workspace, runtime::ExecutionLease({}, flag)); },
            "rational batch ignored cancellation");
    auto packed = tower.input_batch_values(inputs, core::Grade{1});
    numeric::RationalVector output(packed.size());
    unsigned calls = 0;
    rejects([&]{ tower.tower().apply_batch_into({K::Hodge, core::Grade{1}, 0}, packed, output, 2, workspace,
                  [&]{ if (++calls == 2) throw runtime::ExecutionCancelled(); }); }, "rational batch ignored channel cancellation");
    const auto recovered = tower.execute_batch({K::Hodge, core::Grade{1}, 0}, inputs, workspace);
    check(recovered.samples() == 2, "rational batch scratch failed to recover");
    NativeAccessionDirectory replacement_source(endpoint(22), 88);
    const auto replacement = inference::PreparedNativeExactTower::prepare(anchor(replacement_source), request(replacement_source));
    auto replaceable = tower.tower();
    replaceable.apply_batch_into({K::Hodge, core::Grade{1}, 0}, packed, output, 2, workspace,
                                 [&]{ replaceable = replacement.tower(); });
    check(output == recovered.values(), "rational batch callback replaced the active source or metrics");
    rejects([&]{ tower.tower().apply_batch_into({K::Hodge, core::Grade{1}, 0}, packed, packed, 2, workspace); },
            "rational batch accepted aliased fields");
    const field::BoundaryOperator boundary(tower.publication().endpoint()->source, core::Grade{1});
    numeric::RationalVector scratch(6);
    rejects([&]{ boundary.apply_batch_into(packed, packed, 2, scratch); }, "rational boundary accepted aliased fields");
    rejects([&]{ boundary.transpose_batch_into(packed, output, 2, packed); }, "rational transpose accepted aliased scratch");
    const auto metric = field::ExactMetric::diagonal({Q{2}, Q{3}, Q{4}});
    auto measured = packed;
    metric.apply_batch_into(measured, measured, 2); metric.solve_batch_into(measured, measured, 2);
    check(measured == packed, "rational metric in place roundtrip mismatch");
    measured.resize(7);
    rejects([&]{ metric.apply_batch_into(std::span<const Q>(measured).first(6), std::span<Q>(measured).subspan(1, 6), 2); },
            "rational metric partial overlap accepted");
    const field::ExactMetric matrix(numeric::ExactMatrix::identity(3));
    rejects([&]{ matrix.apply_batch_into(packed, output, 2); }, "rational batch inferred a diagonal metric profile");
    std::cout << "rational batch checks=" << checks - before << '\n';
}
void finite_admission_scaling(){
    std::size_t prior_bits = 0;
    for (std::size_t n : {16u, 256u, 512u}) {
        core::RelationSignature signature;
        const auto type = signature.add_type("ring", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        for (std::size_t i = 0; i < n; ++i)
            builder.add_grade_one(type, {{i, true, false, Q{1}}, {(i + 1) % n, true, true, Q{1}}});
        TemporalEndpoint source(builder.freeze());
        for (unsigned k = 0; k < 2; ++k) {
            source.carriers.push_back({10+k, TemporalCarrierKind::SourceGrade, core::Grade{static_cast<std::uint16_t>(k)},
                                       {}, 0, numeric::IndexDomain::linear(n)});
            TemporalTensor input; input.key = 1000+k; input.role = TemporalTensorRole::Field; input.output = 10+k;
            std::vector<numeric::SparseRationalEntry> entries;
            for (std::size_t i = 0; i < n; ++i) {
                const Q value(static_cast<long>(i % 11) - 5, 7);
                if (value != 0) entries.push_back({i, value});
            }
            input.tensor = {numeric::IndexDomain::linear(n), numeric::StructuralSupport::full(n),
                            numeric::SparseRationalVector(n, std::move(entries))};
            source.tensors.push_back(input);
            input.key = 2000+k; input.role = TemporalTensorRole::Metric; input.input = 10+k;
            numeric::ExactMatrix metric(n, n);
            for (std::size_t i = 0; i < n; ++i) metric.set(i, i, Q{static_cast<unsigned long>(i % 5 + 2), 3});
            input.tensor = temporal_matrix_tensor(metric); source.tensors.push_back(input);
        }
        validate_temporal_endpoint(source);
        NativeAccessionDirectory directory(source, 99);
        inference::NativeExactTowerRequest request; request.grade_carriers = {10, 11};
        request.metrics = inference::NativeMetricSelection::DeclaredDiagonal;
        for (unsigned k = 0; k < 2; ++k)
            request.metric_tensors.push_back(directory.pin(tensor_request(NativeAccessionKind::Tensor, 2000+k)));
        const auto input = directory.pin(tensor_request(NativeAccessionKind::Field, 1001));
        runtime::ExecutionHostPolicy policy; policy.limits.rational_slots = 26*n;
        policy.limits.sparse_entries = 4*n;
        const auto prepared = inference::PreparedNativeExactTower::prepare(input, request, policy);
        const auto required = prepared.requirements(3, K::Hodge);
        check(required.rational_slots == 26*n && required.sparse_entries == 4*n,
              "finite diagonal admission charged a square metric allocation");
        check(required.scalar_work == 66*n+4, "finite diagonal work envelope is not linear");
        if (prior_bits) check(required.integer_bits == prior_bits, "finite row degree bound grew with unrelated cells");
        prior_bits = required.integer_bits;
        const auto scalar = prepared.execute({K::Hodge, core::Grade{1}, 0}, input);
        const auto reference = field::ExactExecutionTower::weighted(prepared.publication().endpoint()->source, prepared.tower().metrics());
        check(scalar.values() == reference.apply({K::Hodge, core::Grade{1}, 0}, prepared.input_values(input, core::Grade{1})),
              "bounded finite ring differs from scalar reference");
        auto workspace = prepared.tower().make_batch_workspace(core::Grade{1}, 8);
        const auto batch = prepared.execute_batch({K::Hodge, core::Grade{1}, 0}, std::vector<PinnedNativeAccession>(8, input), workspace);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t s = 0; s < 8; ++s)
                check(batch.values()[i*8+s] == scalar.values()[i], "finite local growth envelope violated exact batch parity");
    }
}
void retained_empty_grade_tests(){
    auto empty=native_accession_fixture::endpoint(2);
    empty.carriers.push_back({3,TemporalCarrierKind::SourceGrade,core::Grade{2},{},0,numeric::IndexDomain::linear(0)});
    empty.tensors.push_back(native_accession_fixture::field(104,3,0,0));
    const auto width=empty.source.grade(core::Grade{1}).cell_count();
    for(const auto key:{std::uint64_t{203},std::uint64_t{204}}){
        TemporalTensor metric;metric.key=key;metric.role=TemporalTensorRole::Metric;
        metric.input=metric.output=key==203?1:3;
        metric.tensor=temporal_matrix_tensor(numeric::ExactMatrix::identity(key==203?width:0));
        empty.tensors.push_back(std::move(metric));
    }
    const auto canonical=encode_temporal_endpoint(empty);
    NativeAccessionDirectory directory(empty,800);
    const auto input=directory.pin(tensor_request(NativeAccessionKind::Field,104));
    const auto lower_input=directory.pin(tensor_request(NativeAccessionKind::Field,101));
    for(const auto profile:{field::PreparedExactProfile::FiniteActions,field::PreparedExactProfile::GlobalActions}){
        for(const auto selection:{inference::NativeMetricSelection::NamedUnit,inference::NativeMetricSelection::DeclaredDiagonal,
                                 inference::NativeMetricSelection::DeclaredPositive}){
            if(profile==field::PreparedExactProfile::FiniteActions && selection==inference::NativeMetricSelection::DeclaredPositive)continue;
            inference::NativeExactTowerRequest req;req.grade_carriers={2,1,3};req.metrics=selection;req.profile=profile;
            if(selection!=inference::NativeMetricSelection::NamedUnit)
                for(const auto key:{std::uint64_t{202},std::uint64_t{203},std::uint64_t{204}})
                    req.metric_tensors.push_back(directory.pin(tensor_request(NativeAccessionKind::Tensor,key)));
            const auto prepared=inference::PreparedNativeExactTower::prepare(lower_input,req);
            check(prepared.tower().source().get()==&input.endpoint()->source && prepared.tower().source()->grade_count()==3,
                  "empty grade lost its actual source owner or retained grade position");
            check(prepared.grade_carrier(core::Grade{2})==3 && prepared.grade_size(core::Grade{2})==0 &&
                  prepared.tower().metrics().size()==3 && prepared.tower().metrics()[2].size()==0,
                  "empty grade carrier or zero-dimensional metric discarded");
            check(prepared.input_values(input,core::Grade{2}).empty(),"declared empty field lost its carrier");
            const auto keys=selection==inference::NativeMetricSelection::NamedUnit?std::vector<std::uint64_t>{}:
                std::vector<std::uint64_t>{202,203,204};
            check(prepared.metric_selection()==selection && prepared.metric_keys()==keys,
                  "empty grade changed the selected metric contract");
            if(!keys.empty())check(prepared.metric_pins()[2].tensor_declaration().get()==req.metric_tensors[2].tensor_declaration().get() &&
                                  prepared.metric_pins()[2].endpoint().get()==input.endpoint().get(),
                                  "empty declared metric lost its actual publication pin");
            for(const auto kind:{K::HodgeDown,K::HodgeUp,K::Hodge}){
                const auto result=prepared.execute({kind,core::Grade{2},0},input);
                check(result.values().empty() && result.publication().endpoint().get()==input.endpoint().get() &&
                      result.provenance().input_field==104 && result.provenance().input_carrier==3 &&
                      result.provenance().output_carrier==3 && result.provenance().metrics==keys,
                      "empty same-grade action lost native carrier or metric provenance");
            }
            const auto boundary=prepared.execute({K::Boundary,core::Grade{2},0},input);
            check(boundary.values()==numeric::RationalVector(width,Q{0}) && boundary.provenance().input_carrier==3 &&
                  boundary.provenance().output_carrier==1,"empty boundary lost its nonempty codomain");
            const auto adjoint=prepared.execute({K::Adjoint,core::Grade{2},0},lower_input);
            check(adjoint.values().empty() && adjoint.provenance().input_carrier==1 && adjoint.provenance().output_carrier==3,
                  "empty adjoint lost its declared output carrier");
            if(profile==field::PreparedExactProfile::GlobalActions)
                for(const auto kind:{K::Green,K::HarmonicProjector,K::HarmonicInverse}){
                    const auto action=prepared.prepare_action({kind,core::Grade{2},kind==K::Green?Q{2,7}:Q{0}});
                    const auto result=action.execute(input);
                    check(result.values().empty() && result.provenance().output_carrier==3 && result.provenance().metrics==keys,
                          "reusable global action discarded the empty grade");
                }
            rejects([&]{prepared.execute({K::Hodge,core::Grade{2},0},lower_input);},"empty grade accepted a different field carrier");
            req.grade_carriers.pop_back();
            rejects([&]{inference::PreparedNativeExactTower::prepare(lower_input,req);},"empty SourceGrade declaration was optional");
        }
    }
    check(encode_temporal_endpoint(*input.endpoint())==canonical,"empty-grade preparation changed its source publication");
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
            for(std::size_t j=0;j<m.cols();++j){row.push_back(m.get(i,j));}vector_json(row);}std::cout<<']';
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
void reusable_global_tests(){
    auto e=endpoint(4);NativeAccessionDirectory d(e,88);const auto pub=anchor(d);
    auto req=request(d);req.profile=field::PreparedExactProfile::GlobalActions;
    const auto tower=inference::PreparedNativeExactTower::prepare(pub,req);
    const auto original_bytes=encode_temporal_endpoint(*pub.endpoint());
    for(const auto kind:{K::Green,K::HarmonicProjector,K::HarmonicInverse,K::GradientPotential}){
        const field::OperatorRequest operation{kind,core::Grade{1},kind==K::Green?Q{2,7}:Q{0}};
        runtime::ExecutionHostPolicy preparation;preparation.name="reusable-action-preparation";
        const auto action=tower.prepare_action(operation,runtime::ExecutionLease(preparation));
        const auto expected=tower.execute(operation,pub).values();
        const auto result=action.execute(pub);
        check(result.values()==expected && result.publication().endpoint().get()==pub.endpoint().get(),"prepared native action pin or value");
        const auto out_grade=kind==K::GradientPotential?core::Grade{0}:core::Grade{1};
        check(result.provenance().output_carrier==tower.grade_carrier(out_grade) && result.values().size()==tower.grade_size(out_grade),
            "prepared inverse or potential output carrier");
        check(result.values()==field::ExactExecutionTower::weighted(pub.endpoint()->source,tower.tower().metrics())
            .apply(operation,tower.input_values(pub,core::Grade{1})),"native potential independent field action");
        check(result.provenance().preparation_policy==tower.preparation_policy().name &&
            result.provenance().action_preparation_policy==preparation.name &&
            result.provenance().implementation=="PreparedGlobalAction/GMP/prepared-solves-v1","prepared native action provenance");
        const auto first_preparation=action.preparation_steps();
        std::vector<std::future<numeric::RationalVector>> jobs;
        for(unsigned i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[action,pub]{return numeric::RationalVector(action.execute(pub).values());}));
        for(auto& job:jobs)check(job.get()==expected,"prepared native action concurrency");
        check(first_preparation==action.preparation_steps() && first_preparation>0,"prepared native action refactored on apply");
        for(unsigned resource=0;resource<5;++resource){
            runtime::ExecutionHostPolicy limited;
            switch(resource){case 0:limited.limits.records=0;break;case 1:limited.limits.rational_slots=0;break;
                case 2:limited.limits.sparse_entries=0;break;case 3:limited.limits.scalar_work=0;break;default:limited.limits.integer_bits=0;}
            rejects([&]{action.execute(pub,runtime::ExecutionLease(limited));},"prepared native current admission bypass");
            rejects([&]{tower.prepare_action(operation,runtime::ExecutionLease(limited));},"prepared native preparation admission bypass");
        }
        auto cancelled=std::make_shared<std::atomic_bool>(true);
        rejects([&]{action.execute(pub,runtime::ExecutionLease({},cancelled));},"prepared native cancellation bypass");
        rejects([&]{tower.prepare_action(operation,runtime::ExecutionLease({},cancelled));},"prepared native preparation cancellation bypass");
        runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
        rejects([&]{action.execute(pub,runtime::ExecutionLease(policy));},"prepared native no-allocation policy bypass");
        policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
        rejects([&]{action.execute(pub,runtime::ExecutionLease(policy));},"prepared native unsupported backend bypass");
        policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;policy.name="declared-preparation-fallback";
        const auto fallback_action=tower.prepare_action(operation,runtime::ExecutionLease(policy));
        const auto fallback_result=fallback_action.execute(pub);
        check(fallback_result.values()==expected && fallback_result.provenance().fallback &&
            fallback_result.provenance().action_preparation_policy==policy.name &&
            fallback_result.provenance().execution_policy==runtime::ExecutionHostPolicy{}.name,
            "preparation-only backend fallback receipt lost");
        policy={};policy.name="current-one-shot-preparation";
        const auto one_shot=tower.execute(operation,pub,runtime::ExecutionLease(policy));
        check(one_shot.provenance().preparation_policy==tower.preparation_policy().name &&
            one_shot.provenance().action_preparation_policy==policy.name,
            "one-shot preparation policy receipt replaced original tower policy");
        NativeAccessionDirectory equal(e,88);rejects([&]{action.execute(anchor(equal));},"equal bytes replaced authentic publication");
        NativeAccessionDirectory foreign(e,89);rejects([&]{action.execute(anchor(foreign));},"equal carrier keys replaced namespace");
        auto later=e;later.sequence=1;
        later.tensors[2].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(3),numeric::StructuralSupport::full(3),
            numeric::SparseRationalVector(3,{{0,Q{17}},{1,Q{19}},{2,Q{23}}}));
        NativeAccessionDirectory future(later,88);
        rejects([&]{action.execute(anchor(future));},"future field used by historical action");
        check(action.execute(pub).values()==expected && encode_temporal_endpoint(*pub.endpoint())==original_bytes,
            "failed/future application changed historical source");
    }
    const auto held=[] {
        NativeAccessionDirectory temporary(endpoint(5),88);const auto p=anchor(temporary);auto r=request(temporary);
        r.profile=field::PreparedExactProfile::GlobalActions;
        const auto prepared=inference::PreparedNativeExactTower::prepare(p,r);
        return std::pair{prepared.prepare_action({K::Green,core::Grade{1},Q{1}}),p};
    }();
    check(held.first.execute(held.second).values().size()==3,"prepared action outlived directory and tower");
    rejects([&]{tower.prepare_action({K::Hodge,core::Grade{1},0});},"non-solve native action accepted");
    rejects([&]{tower.prepare_action({K::GradientPotential,core::Grade{0},0});},"native grade-zero potential accepted");
    core::RelationSignature signature;
    const auto branch_type=signature.add_type("native-branch-potential",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{700},branch_type,
        {{0,true,true,Q{1}},{1,true,false,Q{1,2}},{2,true,false,Q{1,2}},
         {1,true,true,Q{3}},{1,true,false,Q{3}},{2,false,false,Q{11}}});
    TemporalEndpoint branch_endpoint(builder.freeze());
    branch_endpoint.carriers.push_back({710,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(3)});
    branch_endpoint.carriers.push_back({711,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(1)});
    TemporalTensor input;input.key=720;input.role=TemporalTensorRole::Field;input.output=711;
    input.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{1}}})};
    branch_endpoint.tensors.push_back(std::move(input));
    const NativeAccessionDirectory branch_directory(branch_endpoint,730);
    const auto branch_pin=branch_directory.pin(tensor_request(NativeAccessionKind::Field,720));
    inference::NativeExactTowerRequest branch_request;branch_request.grade_carriers={710,711};
    branch_request.profile=field::PreparedExactProfile::GlobalActions;
    const auto branch_tower=inference::PreparedNativeExactTower::prepare(branch_pin,branch_request);
    const auto potential=branch_tower.prepare_action({K::GradientPotential,core::Grade{1},0}).execute(branch_pin);
    check(potential.values()==numeric::RationalVector({Q{-2,3},Q{1,3},Q{1,3}}) &&
        potential.provenance().input_carrier==711 && potential.provenance().output_carrier==710,
        "native branch potential changed grade, shape or full gauge");
    check(field::ExactHodgeOperator(branch_endpoint.source,core::Grade{0},branch_tower.tower().metrics()).harmonic_frame().cols()==2,
        "native branch potential collapsed complete grade-zero kernel");
}
void training_tests(){
    NativeAccessionDirectory d(endpoint(7),88);const auto input=anchor(d);
    const auto tower=inference::PreparedNativeExactTower::prepare(input,request(d));
    const auto gains=d.pin(tensor_request(NativeAccessionKind::Field,3000)),bias=d.pin(tensor_request(NativeAccessionKind::Field,3001));
    const learning::NativeExactTrainingContract contract{learning::NativeTrainingObjective::HalfMetricSquaredError,
        {"original-ordered-gradient-test",900,901},700,9900,9901,9902,2,0,9903};
    std::vector<learning::NativeExactTrainingSample> samples{{input,input,learning::NativeSamplePartition::Training,3,4,5}};
    const auto initial=learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,samples,contract);
    check(initial.parameter_knowledge_time()==2,"initial fitted parameter knowledge time mismatch");
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
        check(trained.parameter_knowledge_time()==5+step,"updated fitted parameter knowledge time mismatch");
        check(trained.parameters()==expected && *trained.last_loss()==loss(before),"declared exact gradient central difference mismatch");
    }
    check(initial_archive==encode_native_series(initial.history()),"training changed a prior checkpoint");
    const auto archive=encode_native_series(trained.history());
    const auto resumed=learning::NativeExactTrainer::resume(initial,archive);
    check(resumed.parameter_knowledge_time()==trained.parameter_knowledge_time(),"resumed fitted parameter knowledge time mismatch");
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

void prepared_training_tests() {
    const auto before = checks;
    for (const auto seed : {0u, 7u, 22u, 23u}) for (const bool weighted : {false, true}) {
        auto source = endpoint(seed);
        auto target = source.tensors[2];
        target.key = 5001;
        target.tensor = {numeric::IndexDomain::linear(3), numeric::StructuralSupport::full(3),
                        numeric::SparseRationalVector(3, {{0, Q{1,7}}, {2, Q{-3,11}}})};
        source.tensors.push_back(target);
        NativeAccessionDirectory directory(source, 88);
        const auto input = anchor(directory);
        const auto other = directory.pin(tensor_request(NativeAccessionKind::Field, 5001));
        const auto tower = inference::PreparedNativeExactTower::prepare(input, request(directory, weighted));
        const auto gains = directory.pin(tensor_request(NativeAccessionKind::Field, 3000));
        const auto bias = directory.pin(tensor_request(NativeAccessionKind::Field, 3001));
        const learning::NativeExactTrainingContract contract{learning::NativeTrainingObjective::HalfMetricSquaredError,
            {"prepared-original-gradient",900,901},700,9900,9901,9902,2,0,9903};
        const std::vector<learning::NativeExactTrainingSample> samples{
            {input, other, learning::NativeSamplePartition::Training,3,4,5},
            {other, input, learning::NativeSamplePartition::Training,6,7,8}};
        const auto initial = learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,samples,contract);
        const auto original = encode_native_series(initial.history());
        for (const auto capacity : {1u, 2u}) {
            auto workspace = initial.make_training_workspace(capacity);
            check(workspace.response_count() == 4, "finite training response count mismatch");
            check(workspace.parameter_coordinates() == std::vector<std::vector<std::size_t>>{
                {0}, {1}, {2}, {1,2}, {1}, {3}}, "finite training original coordinates mismatch");
            auto scalar = initial, prepared = initial;
            auto reference_history = initial.history();
            for (unsigned step = 0; step < 8; ++step) {
                const auto rate = numeric::rational(1, 1000000 + 17*step);
                scalar = scalar.advance(rate, 10, 20+step);
                prepared = prepared.advance_prepared(rate, 10, 20+step, workspace);
                check(prepared.parameters() == scalar.parameters(), "prepared training changed an original parameter");
                check(prepared.last_loss() == scalar.last_loss(), "prepared training changed the exact metric loss");
                check(prepared.parameter_knowledge_time() == scalar.parameter_knowledge_time(), "prepared training changed parameter time");
                check(encode_native_series(prepared.history()) == encode_native_series(scalar.history()), "prepared training changed canonical history bytes");
                const auto target_endpoint = prepared.history().tail();
                std::vector<TemporalCorrespondence> reference_maps;
                for (const auto& carrier : target_endpoint.endpoint().carriers)
                    reference_maps.push_back({carrier.key,numeric::ExactMatrix::identity(carrier.domain.logical_size())});
                reference_history.append(make_temporal_segment(*reference_history.tail().pin(),
                    target_endpoint.endpoint(),std::move(reference_maps)));
                check(encode_native_series(reference_history) == encode_native_series(prepared.history()), "training target append differs from reference publication");
                check(workspace.retained_slots() == workspace.cached_samples()*21, "prepared training cache shape mismatch");
            }
            check(workspace.misses() == (capacity == 1 ? 8 : 2), "prepared training miss count mismatch");
            check(workspace.hits() == (capacity == 1 ? 0 : 6), "prepared training hit count mismatch");
            const auto resumed = learning::NativeExactTrainer::resume(initial, encode_native_series(prepared.history()));
            auto replay_workspace = initial.make_training_workspace(capacity);
            const auto replayed = learning::NativeExactTrainer::resume_prepared(initial, encode_native_series(prepared.history()), replay_workspace);
            check(encode_native_series(replayed.history()) == encode_native_series(scalar.history()), "prepared replay changed canonical history bytes");
            check(replayed.parameters() == scalar.parameters() && replayed.last_loss() == scalar.last_loss(), "prepared replay changed optimizer state");
            check(replay_workspace.hits() == workspace.hits() && replay_workspace.misses() == workspace.misses(), "prepared replay cache order mismatch");
            auto broken = encode_native_series(prepared.history()); broken.back() ^= std::byte{1};
            rejects([&] { learning::NativeExactTrainer::resume_prepared(initial, broken, replay_workspace); }, "prepared replay accepted corrupt history");
            check(resumed.advance_prepared(Q{1,1000000},10,30,workspace).parameters() == scalar.advance(Q{1,1000000},10,30).parameters(),
                  "prepared training resumed update mismatch");
            runtime::ExecutionHostPolicy policy; policy.limits.scalar_work = 1;
            const auto saved_hits = workspace.hits(), saved_misses = workspace.misses();
            rejects([&] { prepared.advance_prepared(Q{1},10,30,workspace,runtime::ExecutionLease(policy)); }, "prepared warm work budget ignored");
            policy = {}; policy.limits.integer_bits = 1;
            rejects([&] { prepared.advance_prepared(Q{1},10,30,workspace,runtime::ExecutionLease(policy)); }, "prepared warm bit budget ignored");
            auto cancel = std::make_shared<std::atomic_bool>(true);
            rejects([&] { prepared.advance_prepared(Q{1},10,30,workspace,runtime::ExecutionLease({},cancel)); }, "prepared cancellation ignored");
            rejects([&] { prepared.advance_prepared(Q{1},10,5,workspace); }, "prepared cached future parameters accepted");
            check(workspace.hits() == saved_hits && workspace.misses() == saved_misses, "rejected prepared training changed cache counters");
        }
        check(encode_native_series(initial.history()) == original, "prepared training changed initial history");
        auto workspace = initial.make_training_workspace(2);
        rejects([&] { initial.advance_prepared(Q{1},2,20,workspace); }, "prepared training used a future event");
        rejects([&] { initial.advance_prepared(Q{1},10,4,workspace); }, "prepared training used a future label");
        check(workspace.cached_samples() == 0, "unavailable observations populated training cache");
        auto evaluation_samples = samples; evaluation_samples[0].partition = learning::NativeSamplePartition::Evaluation;
        const auto evaluation = learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,evaluation_samples,contract);
        auto evaluation_workspace = evaluation.make_training_workspace(2);
        rejects([&] { evaluation.advance_prepared(Q{1},10,20,evaluation_workspace); }, "prepared evaluation label fit allowed");
        check(evaluation_workspace.cached_samples() == 0, "evaluation labels populated training cache");
        const auto foreign = learning::NativeExactTrainer::prepare(tower,core::Grade{1},channels(),gains,bias,samples,contract);
        rejects([&] { foreign.advance_prepared(Q{1},10,20,workspace); }, "equal declaration established training cache identity");
        rejects([&] { learning::NativeExactTrainer::resume_prepared(foreign, original, workspace); }, "prepared replay accepted another lineage cache");
        rejects([&] { initial.make_training_workspace(0); }, "zero training cache capacity accepted");
        rejects([&] { initial.make_training_workspace(3); }, "oversized training cache accepted");
        runtime::ExecutionHostPolicy policy; policy.limits.rational_slots = 1;
        rejects([&] { initial.make_training_workspace(2,runtime::ExecutionLease(policy)); }, "training cache reservation budget ignored");
        policy = {}; policy.limits.scalar_work = 1;
        rejects([&] { initial.advance_prepared(Q{1},10,20,workspace,runtime::ExecutionLease(policy)); }, "cold training work budget ignored");
        check(workspace.retained_slots() == 0, "rejected cold training installed statistics");
        auto moved = std::move(workspace);
        rejects([&] { initial.advance_prepared(Q{1},10,20,workspace); }, "moved training workspace accepted");
        check(initial.advance_prepared(Q{1,1000000},10,20,moved).parameters() == initial.advance(Q{1,1000000},10,20).parameters(),
              "prepared cache recovery mismatch");
    }
    std::cout << "prepared training checks=" << checks-before << '\n';
}

void checkpoint_capacity_tests() {
    const auto before = checks;
    for (const bool weighted : {false, true}) {
        NativeAccessionDirectory directory(endpoint(7), 88);
        const auto input = anchor(directory);
        const auto tower = inference::PreparedNativeExactTower::prepare(input, request(directory, weighted));
        const auto gains = directory.pin(tensor_request(NativeAccessionKind::Field, 3000));
        const auto bias = directory.pin(tensor_request(NativeAccessionKind::Field, 3001));
        learning::NativeExactTrainingContract contract{learning::NativeTrainingObjective::HalfMetricSquaredError,
            {"checkpoint-capacity",900,901},700,9900,9901,9902,2,0,9903};
        std::vector<learning::NativeExactTrainingSample> samples{
            {input,input,learning::NativeSamplePartition::Training,3,4,5}};
        const auto prepare = [&](const auto& data, const auto& declaration) {
            return learning::NativeExactTrainer::prepare(tower, core::Grade{1}, channels(), gains, bias, data, declaration);
        };
        const auto original = prepare(samples, contract);
        const auto reference = original.advance(Q{1,1000000}, 3, 5);
        for (const std::size_t capacity : {1024u,2003u,4096u,8192u,65536u}) {
            auto selected = contract; selected.metadata_capacity = capacity;
            const auto initial = prepare(samples, selected);
            const auto first = encode_native_series(initial.history());
            const auto pin = initial.history().tail();
            const auto metadata = temporal_endpoint_detail::tensors(pin.endpoint()).at(9901);
            check(metadata->tensor.domain() == numeric::IndexDomain::linear(capacity), "checkpoint capacity lost its declared shape");
            check(metadata->tensor.support().count() == capacity && metadata->tensor.state(capacity-1) == Q{0},
                  "checkpoint capacity lost supported zero padding");
            auto workspace = initial.make_training_workspace(1);
            const auto direct = initial.advance(Q{1,1000000}, 3, 5);
            const auto cached = initial.advance_prepared(Q{1,1000000}, 3, 5, workspace);
            check(direct.parameters() == reference.parameters() && direct.last_loss() == reference.last_loss(),
                  "checkpoint capacity changed the exact update");
            const auto archive = encode_native_series(direct.history());
            check(archive == encode_native_series(cached.history()), "checkpoint capacity changed direct/prepared history parity");
            const auto restored = learning::NativeExactTrainer::resume(initial, archive);
            check(restored.history().same_history(direct.history()), "checkpoint capacity failed complete native replay");
            check(encode_native_series(initial.history()) == first, "checkpoint publication changed its parent");
            if (capacity == 4096) check(archive == encode_native_series(reference.history()), "explicit default changed native archive bytes");
            else rejects([&] { learning::NativeExactTrainer::resume(original, archive); }, "resume ignored checkpoint capacity identity");
        }
        for (const auto capacity : {std::size_t{0}, std::size_t{7}, std::size_t{65537}, std::size_t(-1)}) {
            auto invalid = contract; invalid.metadata_capacity = capacity;
            rejects([&] { prepare(samples, invalid); }, "invalid checkpoint capacity accepted");
        }
        auto small = contract; small.metadata_capacity = 8;
        rejects([&] { prepare(samples, small); }, "incomplete initial checkpoint metadata accepted");

        samples.resize(1024, samples.front());
        rejects([&] { prepare(samples, contract); }, "large sample declaration fit the default checkpoint");
        auto large = contract; large.metadata_capacity = 65536;
        const auto initial = prepare(samples, large);
        auto workspace = initial.make_training_workspace(8);
        const auto trained = initial.advance_prepared(Q{1,1000000}, 3, 5, workspace);
        check(trained.parameters() == reference.parameters() && trained.last_loss() == reference.last_loss(),
              "large ordered sample declaration changed its first update");
        const auto archive = encode_native_series(trained.history());
        auto restored_workspace = initial.make_training_workspace(8);
        const auto restored = learning::NativeExactTrainer::resume_prepared(initial, archive, restored_workspace);
        check(restored.history().same_history(trained.history()), "large sample checkpoint failed prepared replay");
        samples.back().label_available_time = 6;
        const auto changed = prepare(samples, large);
        rejects([&] { learning::NativeExactTrainer::resume(changed, archive); }, "resume omitted a later sample declaration");
        samples.push_back(samples.front());
        rejects([&] { prepare(samples, large); }, "checkpoint capacity bypassed the native sample limit");

        auto compact = contract; compact.metadata_capacity = 1024;
        const auto bounded = prepare(std::vector<learning::NativeExactTrainingSample>{samples.front()}, compact);
        auto bounded_workspace = bounded.make_training_workspace(1);
        const auto parent = encode_native_series(bounded.history());
        numeric::Integer numerator; mpz_ui_pow_ui(numerator.get_mpz_t(),2,8192);
        rejects([&] { bounded.advance_prepared(Q{numerator},3,5,bounded_workspace); }, "oversized update metadata accepted");
        check(encode_native_series(bounded.history()) == parent && bounded_workspace.cached_samples() == 0 &&
              bounded_workspace.hits() == 0 && bounded_workspace.misses() == 0,
              "failed checkpoint publication changed its parent or cache");
        check(bounded.advance_prepared(Q{1,1000000},3,5,bounded_workspace).parameters() == reference.parameters(),
              "failed checkpoint publication poisoned the workspace");
        for (const unsigned fault : {0u,1u,2u}) {
            auto history = bounded.history();
            const auto tail = history.tail();
            auto target = tail.endpoint(); target.sequence = 1;
            for (auto& tensor : target.tensors) if (tensor.key == 9901) {
                auto entries = tensor.tensor.values().entries();
                if (fault < 2) entries.push_back({900,fault == 0 ? Q{256} : Q{1,2}});
                tensor.tensor = {tensor.tensor.domain(), fault == 2 ? numeric::StructuralSupport::sparse(1024,[] {
                    std::vector<std::size_t> indices(1023); std::iota(indices.begin(),indices.end(),0); return indices;
                }()) : tensor.tensor.support(), numeric::SparseRationalVector(1024,std::move(entries))};
            }
            std::vector<TemporalCorrespondence> maps;
            for (const auto& carrier : target.carriers) maps.push_back({carrier.key,numeric::ExactMatrix::identity(carrier.domain.logical_size())});
            history.append_target(target, std::move(maps));
            const auto bad = encode_native_series(history);
            rejects([&] { learning::NativeExactTrainer::resume(bounded,bad); }, "invalid checkpoint byte or support accepted");
        }
    }
    std::cout << "checkpoint capacity checks=" << checks-before << '\n';
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
    exact_tests(emit);if(!emit){rational_batch_tests();finite_admission_scaling();admission_lifetime();retained_empty_grade_tests();global_tests(false);reusable_global_tests();training_tests();prepared_training_tests();checkpoint_capacity_tests();packed_tests();std::cout<<"checks="<<checks<<"\n";}return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
