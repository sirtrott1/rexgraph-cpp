#include "rfc/storage/temporal_diagnostics.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include "rfc/field/connection.hpp"
#include "rfc/temporal/multitime.hpp"
#include "rfc/temporal/moment_change.hpp"
#include "rfc/temporal/operation_word.hpp"
#include "rfc/temporal/sector_transport.hpp"
#include "rfc/algebra/tensor_product.hpp"

#include <chrono>
#include <iostream>
#include <random>

using namespace rfc;
using namespace rfc::storage;
using R = numeric::Rational;
using M = numeric::ExactMatrix;
namespace {
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
template <class F> void rejects(F&& action, const char* message) {
    bool threw = false; try { action(); } catch (const std::exception&) { threw = true; }
    check(threw, message);
}
numeric::SparseRationalTensor vector_tensor(std::vector<R> values) {
    const auto n = values.size(); std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < n; ++i) if (values[i] != 0) entries.push_back({i, values[i]});
    return {numeric::IndexDomain::linear(n), numeric::StructuralSupport::full(n), numeric::SparseRationalVector(n, entries)};
}
TemporalTensor field_tensor(std::uint64_t key, std::uint64_t carrier, std::vector<R> values) {
    TemporalTensor t; t.key = key; t.role = TemporalTensorRole::Field; t.output = carrier; t.tensor = vector_tensor(std::move(values)); return t;
}
TemporalTensor map_tensor(std::uint64_t key, TemporalTensorRole role, std::uint64_t input, std::uint64_t output, const M& map) {
    TemporalTensor t; t.key = key; t.role = role; t.input = input; t.output = output; t.tensor = temporal_matrix_tensor(map); return t;
}
TemporalTensor& tensor_at(TemporalEndpoint& e, std::uint64_t key) {
    for (auto& t : e.tensors) if (t.key == key) return t;
    throw std::runtime_error("missing test tensor");
}
const TemporalTensorUpdate& update_at(const TemporalSegment& s, std::uint64_t key) {
    for (const auto& t : s.tensors) if (t.target.key == key) return t;
    throw std::runtime_error("missing test update");
}
core::RelationalComplex triangle(bool changed) {
    core::RelationSignature sig;
    const auto flag = sig.aspects().add("retained-tag");
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto alternate = sig.add_type("alternate-edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = sig.add_type("face", core::BoundaryPolicy::DeclaredExact, 3, 3);
    const auto empty = sig.add_type("empty-relation", core::BoundaryPolicy::DeclaredExact, 0, 0);
    core::RelationalComplexBuilder b(sig);
    for (const auto key : changed ? std::vector<std::uint64_t>{12,10,11,13} : std::vector<std::uint64_t>{10,11,12,99}) b.declare_witness_identity(key);
    std::map<std::uint64_t, std::uint32_t> index;
    if (changed) index[150] = b.add_grade_one(core::RelationKey{150}, empty, {}).index;
    const auto order = changed ? std::vector<std::uint64_t>{102,100,101} : std::vector<std::uint64_t>{100,101,102};
    for (const auto key : order) {
        const std::uint64_t lo = key == 100 ? 10 : key == 101 ? 11 : 12;
        const std::uint64_t hi = key == 100 ? 11 : key == 101 ? 12 : 10;
        const bool reverse = changed && key == 100;
        const R magnitude = reverse ? R{2} : R{1};
        index[key] = b.add_grade_one(core::RelationKey{key}, changed && key == 101 ? alternate : edge, {
            {lo,true,!reverse,magnitude,{{flag,changed}},core::OccurrenceKey{key*10}},
            {hi,true,reverse,magnitude,{},core::OccurrenceKey{key*10+1}}}).index;
    }
    b.add_higher(core::Grade{2}, core::RelationKey{200}, face, {
        {index.at(100),true,changed,changed ? R{1,2} : R{1},{},core::OccurrenceKey{2000}},
        {index.at(101),true,false,R{1},{},core::OccurrenceKey{2001}},
        {index.at(102),true,false,R{1},{},core::OccurrenceKey{2002}}});
    return b.freeze();
}
TemporalEndpoint endpoint(bool changed) {
    TemporalEndpoint e(triangle(changed), changed ? 1 : 0);
    e.carriers.push_back({10,TemporalCarrierKind::SourceGrade,core::Grade{1},std::nullopt,0,
                          numeric::IndexDomain::linear(e.source.grade(core::Grade{1}).cell_count())});
    e.carriers.push_back({20,TemporalCarrierKind::Explicit,{},std::nullopt,100,numeric::IndexDomain::linear(changed ? 3 : 2)});
    e.carriers.push_back({30,TemporalCarrierKind::Explicit,{},std::nullopt,101,numeric::IndexDomain::linear(1)});
    e.tensors.push_back(field_tensor(1,10,changed ? std::vector<R>{0,5,2,3} : std::vector<R>{2,3,5}));
    e.tensors.push_back(field_tensor(2,20,changed ? std::vector<R>{2,0,7} : std::vector<R>{2,0}));
    e.tensors.push_back(map_tensor(3,TemporalTensorRole::Observation,20,30,
        changed ? M::from_rows({{1,3,11}}) : M::from_rows({{1,3}})));
    e.tensors.push_back(map_tensor(4,TemporalTensorRole::Metric,20,20,
        changed ? M::from_rows({{2,0,1},{0,3,0},{1,0,5}}) : M::diagonal({2,3})));
    TemporalTensor opaque; opaque.key = 5; opaque.interpretation = "declared sparse block tensor; axes v1";
    opaque.tensor = changed
        ? numeric::SparseRationalTensor(numeric::IndexDomain({2,3}), numeric::StructuralSupport::sparse(6,{0,2,5}), numeric::SparseRationalVector(6,{{2,R{6}}}))
        : numeric::SparseRationalTensor(numeric::IndexDomain({2,2}), numeric::StructuralSupport::sparse(4,{0,1,3}), numeric::SparseRationalVector(4,{{1,R{5}}}));
    e.tensors.push_back(opaque);
    LocalBlockDefinition strict; strict.block_key=7;
    strict.cells={{{0},10},{{0},11},{{0},12},{{1},100},{{1},101}};
    LocalExactTensorPayload payload; payload.payload_key=9; payload.tensor=opaque.tensor;
    strict.payloads.push_back(payload);
    e.local_blocks.push_back(temporal_endpoint_detail::normalize_local(e.source,strict));
    LocalBlockDefinition partial; partial.block_key=8; partial.kind=LocalBlockKind::PartialChart;
    partial.cells={{{0},10},{{0},11},{{1},100}};
    partial.occurrences={{{1},core::OccurrenceKey{changed ? 1001u : 1000u}}};
    e.local_blocks.push_back(temporal_endpoint_detail::normalize_local(e.source,partial));
    return e;
}
M inclusion() { return M::from_rows({{1,0},{0,1},{0,0}}); }

void coupled_roundtrip(const std::filesystem::path& dir) {
    auto old=endpoint(false), next=endpoint(true);
    rejects([&] { (void)make_temporal_segment(old,next); }, "implicit identity accepted changing tensor dimensions");
    const std::vector<TemporalCorrespondence> maps{{20,inclusion()}};
    const auto s=make_temporal_segment(old,next,maps);
    check(!s.source.grades.empty(),"source edits absent");
    check(update_at(s,1).innovation.nonzeros()==0,"zero-amplitude birth induced a field innovation");
    check(update_at(s,2).innovation.nonzeros()==1,"stalk innovation was not sparse");
    check(update_at(s,3).target.tensor.domain()==numeric::IndexDomain({1,3}),"new map direction omitted");
    const auto bytes=encode_temporal_segment(s);
    auto normalized=temporal_segment_detail::canonical_segment(s);
    check(temporal_segment_detail::has_canonical_segment_order(normalized),"normalized frame not recognized");
    check(encode_temporal_segment(normalized)==bytes,"canonical source changed bytes");
    auto unordered=normalized;
    std::reverse(unordered.source.grades.begin(),unordered.source.grades.end());
    std::reverse(unordered.blocks.begin(),unordered.blocks.end());
    std::reverse(unordered.carriers.begin(),unordered.carriers.end());
    std::reverse(unordered.tensors.begin(),unordered.tensors.end());
    for(auto& grade:unordered.source.grades) {
        std::reverse(grade.cells.begin(),grade.cells.end());
        std::reverse(grade.occurrences.begin(),grade.occurrences.end());
        std::reverse(grade.fibers.begin(),grade.fibers.end());
        std::reverse(grade.aspect_xor.begin(),grade.aspect_xor.end());
    }
    for(auto& block:unordered.blocks) {
        std::reverse(block.target.cells.begin(),block.target.cells.end());
        std::reverse(block.target.occurrences.begin(),block.target.occurrences.end());
    }
    check(!temporal_segment_detail::has_canonical_segment_order(unordered),"unordered frame bypassed normalization");
    check(encode_temporal_segment(unordered)==bytes,"unordered frame changed canonical bytes");
    auto invalid=normalized;invalid.blocks.front().target.kind=static_cast<LocalBlockKind>(255);
    rejects([&]{(void)encode_temporal_segment(invalid);},"invalid block kind bypassed normalization refusal");
    const auto decoded=decode_temporal_segment(bytes);
    check(encode_temporal_segment(decoded)==bytes,"temporal encoding is not canonical roundtrip");
    const auto restored=replay_temporal_segment(old,decoded);
    check(temporal_endpoint_hash(restored)==temporal_endpoint_hash(next),"coupled endpoint reconstruction failed");
    check(temporal_form_pullback_change(old,restored,4,maps).nonzeros()==0,"old-space metric pullback changed unexpectedly");
    check(temporal_tensor_matrix(*temporal_endpoint_detail::tensors(restored).at(4)).get(2,2)==R{5},"new metric direction lost");
    check(temporal_tensor_map_defect(old,restored,3,maps).nonzeros()==0,"operator correspondence defect wrong");
    check(temporal_tensor_matrix(*temporal_endpoint_detail::tensors(restored).at(3)).get(0,2)==R{11},"zero-defect target map not reconstructed");
    const auto& block=restored.tensors.back().tensor;
    check(block.domain()==numeric::IndexDomain({2,3}) && block.state(5)==std::optional<R>{R{0}} && !block.state(1),"domain/support/zero distinctions lost");
    check(restored.local_blocks[1].occurrences[0].key.value==1001,"partial chart selection lost");
    check(GlueIndex(restored.local_blocks).shared_cell_count()==3,"shared source identity glue changed");
    const auto base_path=dir/"base.rfcx", target_path=dir/"target.rfcx", local_path=dir/"target.rfcxl";
    check(write_rfcx(old.source,base_path).payload_hash==s.base_source_hash,"RFCX physical base hash mismatch");
    const auto seed_path=dir/"base.rfcxe";
    write_temporal_endpoint(old,seed_path);
    MappedRfcx seed_source(base_path);
    const auto reloaded_base=read_temporal_endpoint(seed_source,seed_path);
    check(temporal_endpoint_hash(reloaded_base)==temporal_endpoint_hash(old),"base endpoint sidecar lost fields/carriers/metrics");
    check(temporal_endpoint_hash(replay_temporal_segment(reloaded_base,decoded))==temporal_endpoint_hash(next),"disk-only seed and segment replay failed");
    rejects([&]{(void)decode_temporal_endpoint(next.source,encode_temporal_endpoint(old));},"seed accepted wrong source");
    write_rfcx(restored.source,target_path);
    write_local_blocks(restored.source,s.target_source_hash,restored.local_blocks,local_path);
    MappedLocalBlocks local(local_path); MappedRfcx target_source(target_path), base_source(base_path);
    local.validate_against(target_source);
    rejects([&] { local.validate_against(base_source); },"target local image accepted old source");
    const auto path=dir/"history.rfcxt";
    check(append_temporal_segment(path,s)==bytes.size(),"append byte count mismatch");
    check(read_temporal_segment(path).target_endpoint_hash==s.target_endpoint_hash,"segment disk roundtrip failed");
    auto third=old; third.sequence=2; tensor_at(third,2).tensor=vector_tensor({3,5});
    const std::vector<TemporalCorrespondence> maps12{{20,inclusion().transpose()}};
    const auto s12=make_temporal_segment(restored,third,maps12);
    append_temporal_segment(path,s12);
    const auto records=read_temporal_segments(path);
    check(records.size()==2,"append stream lost frame");
    const auto last=replay_temporal_segment(replay_temporal_segment(old,records[0]),records[1]);
    check(temporal_endpoint_hash(last)==temporal_endpoint_hash(third),"two-segment replay failed");
    const auto eta01=temporal_field_innovation(old,next,2,maps);
    const auto eta12=temporal_field_innovation(next,third,2,maps12);
    const auto composed=temporal_endpoint_detail::sparse_sum(temporal_endpoint_detail::sparse_apply(inclusion().transpose(),eta01),eta12);
    check(temporal_endpoint_detail::values_equal(composed,temporal_field_innovation(old,third,2)),"multi-time innovations failed exact composition");
    rejects([&] { append_temporal_segment(path,s); },"append accepted wrong predecessor");
    check(std::filesystem::file_size(path)==bytes.size()+encode_temporal_segment(s12).size(),"rejected append changed existing bytes");
    rejects([&] { (void)read_temporal_segment(path); },"single-frame API accepted multiple frames");

    auto wrong=old; tensor_at(wrong,2).tensor=vector_tensor({9,0});
    rejects([&] { (void)replay_temporal_segment(wrong,s); },"source-only hash allowed wrong field base");
    wrong=old; wrong.local_blocks[0].payloads[0].tensor=vector_tensor({9});
    rejects([&] { (void)replay_temporal_segment(wrong,s); },"wrong local payload base accepted");
    auto corrupt=s; corrupt.target_source_hash^=1;
    rejects([&] { (void)replay_temporal_segment(old,corrupt); },"target source hash ignored");
    corrupt=s; corrupt.tensors.push_back(corrupt.tensors.front());
    rejects([&] { (void)replay_temporal_segment(old,corrupt); },"duplicate tensor update accepted");
    corrupt=s; corrupt.source.grades.push_back(corrupt.source.grades.front());
    rejects([&] { (void)replay_temporal_segment(old,corrupt); },"duplicate grade edit accepted");
    corrupt=s; corrupt.correspondences.push_back(corrupt.correspondences.front());
    rejects([&] { (void)replay_temporal_segment(old,corrupt); },"duplicate correspondence accepted");
    corrupt=s; corrupt.correspondences[0].map=M(1,1);
    rejects([&] { (void)replay_temporal_segment(old,corrupt); },"wrong rectangular map shape accepted");
    check(temporal_endpoint_hash(old)==s.base_endpoint_hash,"failed replay mutated base");
    for (const auto cut : std::vector<std::size_t>{0,1,7,16,31,32,60,bytes.size()/2,bytes.size()-1})
        rejects([&] { (void)decode_temporal_segment(std::span<const std::byte>(bytes).first(cut)); },"truncated segment accepted");
    for (const auto pos : std::vector<std::size_t>{0,8,12,16,24,bytes.size()-1}) {
        auto bad=bytes; bad[pos]^=std::byte{1};
        rejects([&] { (void)decode_temporal_segment(bad); },"corrupt segment accepted");
    }
    auto limits=TemporalReadLimits{}; limits.max_records=2;
    rejects([&] { (void)decode_temporal_segment(bytes,limits); },"record allocation budget ignored");
    limits=TemporalReadLimits{}; limits.max_dimension=2;
    rejects([&] { (void)decode_temporal_segment(bytes,limits); },"dimension limit ignored");
    auto no_op=next; no_op.sequence=2;
    const auto idle=make_temporal_segment(next,no_op);
    check(idle.source.grades.empty() && idle.tensors.empty() && idle.carriers.empty() && idle.blocks.empty(),"no-op segment not sparse");
    check(temporal_endpoint_hash(replay_temporal_segment(next,idle))==temporal_endpoint_hash(no_op),"no-op replay failed");
    // Explicitly declared nonidentity transport with eta=0 must still be applied.
    auto transported=old; transported.sequence=1; tensor_at(transported,2).tensor=vector_tensor({4,0});
    const auto moving=make_temporal_segment(old,transported,{{20,M::diagonal({2,1})}});
    check(moving.tensors.empty(),"pure transport introduced an innovation record");
    check(temporal_endpoint_hash(replay_temporal_segment(old,moving))==temporal_endpoint_hash(transported),"record-free pure transport lost J action");
    std::cout<<"coupled_roundtrip bytes="<<bytes.size()<<" no_op_bytes="<<encode_temporal_segment(idle).size()<<'\n';
}

void occurrence_regressions() {
    core::RelationSignature sig;
    const auto tag=sig.aspects().add("masked-tag");
    const auto rel=sig.add_type("retained",core::BoundaryPolicy::DeclaredExact,0,0);
    auto make=[&](bool order_changed) {
        core::RelationalComplexBuilder b(sig);
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for (const auto i : order_changed ? std::vector<int>{0,2,1,3} : std::vector<int>{0,1,2,3})
            occurrences.emplace_back(10,true,i>=2,R{1},std::vector<core::AspectValue>{},core::OccurrenceKey{static_cast<std::uint64_t>(100+i)});
        b.add_grade_one(core::RelationKey{1},rel,occurrences);
        b.add_higher(core::Grade{2},core::RelationKey{2},rel,{{0,true,false,R{1},{},core::OccurrenceKey{200}}});
        return b.freeze();
    };
    const auto a=make(false), b=make(true);
    check(a.boundary_apply(core::Grade{1},{R{3}})==std::vector<R>{0},"native duplicate cancellation wrong");
    check(field::materialize_boundary_matrix(a,core::Grade{1}).nonzeros()==0,"materialization overwrote repeated occurrences");
    check(field::ExactConnectionTower::ideal(a).flat_through(core::Grade{2}),"valid source incorrectly classified curved");
    const temporal::RelationalSourceDelta delta(a,b);
    check(!delta.source_structure_unchanged() && delta.grade(core::Grade{1}).attaching_order_changes.size()==1,"attaching order reported unchanged");
    TemporalEndpoint old(a), next(b,1);
    const auto s=make_temporal_segment(old,next);
    check(temporal_endpoint_hash(replay_temporal_segment(old,decode_temporal_segment(encode_temporal_segment(s))))==temporal_endpoint_hash(next),"ordered occurrence replay failed");
    const auto triangle_source=triangle(false);
    const auto tower=field::ExactConnectionTower::ideal(triangle_source);
    rejects([&] { (void)tower.route_defect_certificate(core::Grade{2},{0},field::RouteCoverage::CompleteInterface); },"partial route selection certified complete");
    rejects([&] { (void)tower.route_defect_certificate(core::Grade{2},{0,0,1},field::RouteCoverage::PartialChart); },"duplicate route counted twice");
    check(tower.route_defect_certificate(core::Grade{2},{0},field::RouteCoverage::PartialChart).partial_nonzero_not_curvature(),"partial route mislabeled curvature");
    check(tower.global_route_defect_certificate(core::Grade{2}).globally_flat(),"complete cancelling routes not flat");
    auto masked=[&](bool changed) {
        core::RelationalComplexBuilder builder(sig);
        std::vector<core::BoundaryOccurrenceInput> fiber;
        for (std::uint64_t i=0;i<130;++i) fiber.emplace_back(10,false,i%2!=0,R{7},std::vector<core::AspectValue>{{tag,changed && i==70}},core::OccurrenceKey{1000+i});
        builder.add_grade_one(core::RelationKey{8},rel,fiber); return builder.freeze();
    };
    TemporalEndpoint m0(masked(false)),m1(masked(true),1);
    const auto ms=make_temporal_segment(m0,m1);
    check(ms.source.grades[0].aspect_xor.size()==1 && ms.source.grades[0].aspect_xor[0].word_index==1,"packed masked aspect word alignment wrong");
    check(ms.source.grades[0].occurrences.empty(),"masked aspect flip invented coefficient change");
    check(temporal_endpoint_hash(replay_temporal_segment(m0,ms))==temporal_endpoint_hash(m1),"masked aspects lost");
    auto bad=ms; bad.source.grades[0].aspect_xor[0].word_index=999;
    rejects([&] { (void)replay_temporal_segment(m0,bad); },"out of range aspect word accepted");
    bad=ms; bad.source.grades[0].aspect_xor[0].word_index=2; bad.source.grades[0].aspect_xor[0].bits=std::uint64_t{1}<<63;
    rejects([&] { (void)replay_temporal_segment(m0,bad); },"nonzero XOR padding accepted");
    bad=ms; bad.source.grades[0].aspect_xor.push_back(bad.source.grades[0].aspect_xor[0]);
    rejects([&] { (void)replay_temporal_segment(m0,bad); },"duplicate XOR word accepted");
}

void tensor_local_evolution() {
    core::RelationSignature sig; const auto rel=sig.add_type("one-leg",core::BoundaryPolicy::DeclaredExact,1,1);
    auto source=[&](R coefficient) {
        core::RelationalComplexBuilder b(sig);
        b.add_grade_one(core::RelationKey{50},rel,{{10,true,coefficient<0,coefficient<0 ? -coefficient:coefficient,{},core::OccurrenceKey{500}}});
        return b.freeze();
    };
    auto make=[&](bool changed) {
        TemporalEndpoint e(source(changed ? R{-3,2}:R{2}),changed ? 1:0);
        e.carriers.push_back({1,TemporalCarrierKind::CellStalk,core::Grade{0},StableCellKey{core::Grade{0},10},10,numeric::IndexDomain::linear(changed?3:2)});
        e.carriers.push_back({2,TemporalCarrierKind::CellStalk,core::Grade{1},StableCellKey{core::Grade{1},50},20,numeric::IndexDomain::linear(changed?2:1)});
        auto t=map_tensor(1,TemporalTensorRole::CoefficientIndependentMap,2,1,
            changed?M::from_rows({{1,4},{3,5},{7,11}}):M::from_rows({{1},{3}}));
        t.owner.kind=TemporalOwnerKind::Occurrence; t.owner.occurrence={core::Grade{1},core::OccurrenceKey{500}}; e.tensors.push_back(t);
        e.tensors.push_back(field_tensor(2,2,changed?std::vector<R>{2,9}:std::vector<R>{2}));
        return e;
    };
    auto old=make(false),next=make(true);
    const std::vector<TemporalCorrespondence> maps{{1,inclusion()},{2,M::from_rows({{1},{0}})}};
    const auto segment=make_temporal_segment(old,next,maps);
    const auto restored=replay_temporal_segment(old,decode_temporal_segment(encode_temporal_segment(segment)));
    const auto fiber=temporal_linear_fiber_system(restored,{1,2});
    const auto T=temporal_realized_incidence(restored,restored.tensors[0]);
    check(fiber.flag_map(core::Grade{1},0).transport==T,"factorized incidence failed rehydration");
    const local::GradeTensorField grade(fiber,core::Grade{1});
    check(grade.apply({2,9})==T.apply({2,9}),"local tensor assembly failed");
    const auto E=temporal_tensor_map_defect(old,restored,1,maps,true);
    const auto Ephi=temporal_tensor_map_defect(old,restored,1,maps,false);
    const auto phi=temporal_tensor_matrix(old.tensors[0]);
    check(E==inclusion().multiply(phi).scale(R{-7,2}).add(Ephi.scale(R{-3,2})),"composite/map finite interaction identity failed");
    const auto oldT=temporal_realized_incidence(old,old.tensors[0]);
    const auto left=numeric::sub(T.apply({2,9}),inclusion().apply(oldT.apply({2})));
    check(left==numeric::add(E.apply({2}),T.apply({0,9})),"rectangular incidence field identity failed");
    auto realized=next; tensor_at(realized,1).role=TemporalTensorRole::RealizedIncidenceMap; tensor_at(realized,1).tensor=temporal_matrix_tensor(T);
    const auto rs=make_temporal_segment(old,realized,maps);
    const auto rr=replay_temporal_segment(old,rs);
    check(temporal_linear_fiber_system(rr,{1,2}).flag_map(core::Grade{1},0).transport==T,"realized override coefficient multiplied twice");
    auto wrong=next; tensor_at(wrong,1).input=1;
    rejects([&] { validate_temporal_endpoint(wrong); },"local map wrong owner/dimension accepted");
    auto support_old=old,support_next=old; support_next.sequence=1;
    tensor_at(support_old,2).tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1)};
    tensor_at(support_next,2).tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::sparse(1,{}),numeric::SparseRationalVector(1)};
    const auto ss=make_temporal_segment(support_old,support_next);
    check(update_at(ss,2).innovation.nonzeros()==0,"zero-support removal invented amplitude change");
    check(!tensor_at(support_next,2).tensor.state(0) && !replay_temporal_segment(support_old,ss).tensors[1].tensor.state(0),"zero support deletion lost");
}

void exact_and_random_source() {
    core::RelationSignature sig; const auto rel=sig.add_type("general",core::BoundaryPolicy::DeclaredExact,0,0);
    const numeric::Integer large("1427247692705959881058285969449495136382746624");
    auto build=[&](bool changed) {
        core::RelationalComplexBuilder b(sig);
        b.add_grade_one(core::RelationKey{1},rel,{{10,true,changed,changed?R(large,large+1):R{1,3},{},core::OccurrenceKey{100}}}); return b.freeze();
    };
    TemporalEndpoint a(build(false)),b(build(true),1);
    const auto large_segment=make_temporal_segment(a,b);
    const auto restored=replay_temporal_segment(a,decode_temporal_segment(encode_temporal_segment(large_segment)));
    check(restored.source.incidence_coefficient(core::Grade{1},0)==-R(large,large+1),"large rational truncated");
    auto limits=TemporalReadLimits{};limits.max_integer_bytes=1;
    rejects([&] { (void)decode_temporal_segment(encode_temporal_segment(large_segment),limits); },"integer-byte limit ignored");
    core::RelationSignature wrong_sig=sig; wrong_sig.aspects().add("new-aspect");
    core::RelationalComplexBuilder wrong_builder(wrong_sig);
    TemporalEndpoint wrong(wrong_builder.freeze(),1);
    rejects([&] { (void)make_temporal_segment(a,wrong); },"signature migration silently accepted");
    std::mt19937 random(20260926);
    for (std::size_t trial=0;trial<40;++trial) {
        auto random_source=[&]() {
            core::RelationalComplexBuilder builder(sig);
            for (const auto key : {10u,11u,12u,13u}) builder.declare_witness_identity(key);
            std::vector<std::uint64_t> relations{1,2,3,4}; std::shuffle(relations.begin(),relations.end(),random);
            for (const auto key : relations) {
                if (random()%5==0) continue;
                std::vector<core::BoundaryOccurrenceInput> occurrences;
                for (std::uint64_t i=0;i<4;++i) {
                    if (random()%4==0) continue;
                    const bool exists=random()%3!=0;
                    occurrences.emplace_back(10+random()%4,exists,random()%2!=0,R{static_cast<long>(random()%9),static_cast<long>(1+random()%7)},
                                             std::vector<core::AspectValue>{},core::OccurrenceKey{key*100+i});
                }
                std::shuffle(occurrences.begin(),occurrences.end(),random);
                builder.add_grade_one(core::RelationKey{key},rel,occurrences);
            }
            return builder.freeze();
        };
        TemporalEndpoint old(random_source()),next(random_source(),1);
        const auto segment=make_temporal_segment(old,next);
        check(temporal_endpoint_hash(replay_temporal_segment(old,decode_temporal_segment(encode_temporal_segment(segment))))==temporal_endpoint_hash(next),"random source reconstruction failed");
    }
}

void retained_magnitude_and_parser() {
    core::RelationSignature sig; const auto rel=sig.add_type("masked",core::BoundaryPolicy::DeclaredExact,1,1);
    auto build=[&](bool active) {
        core::RelationalComplexBuilder b(sig);
        b.add_grade_one(core::RelationKey{1},rel,{{10,active,false,R{5},{},core::OccurrenceKey{100}}});
        TemporalEndpoint e(b.freeze(),active?1:0);
        TemporalTensor q; q.key=1; q.role=TemporalTensorRole::IncidenceMagnitude;
        q.owner.kind=TemporalOwnerKind::Occurrence; q.owner.occurrence={core::Grade{1},core::OccurrenceKey{100}};
        q.tensor={numeric::IndexDomain{},numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,R{5}}})};
        e.tensors.push_back(q); return e;
    };
    const auto old=build(false),next=build(true); const auto segment=make_temporal_segment(old,next);
    const auto bytes=encode_temporal_segment(segment);
    const auto restored=replay_temporal_segment(old,decode_temporal_segment(bytes));
    const auto change=temporal_occurrence_composite_change(old,restored,{core::Grade{1},core::OccurrenceKey{100}});
    check(change.existence==R{5} && change.magnitude==0 && change.aggregate==5,"masked declared magnitude misattributed");
    auto wrong=next; tensor_at(wrong,1).tensor={numeric::IndexDomain{},numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,R{9}}})};
    rejects([&]{validate_temporal_endpoint(wrong);},"active magnitude inconsistent with source accepted");
    // Fuzz bounded parsing with integrity recomputed, so malformed payloads
    // exercise length, enum, rational and semantic checks, not only checksum.
    std::mt19937 random(35003);
    TemporalReadLimits limits; limits.max_records=10000; limits.max_dimension=10000; limits.max_integer_bytes=1024;
    for(std::size_t trial=0;trial<500;++trial){
        auto mutated=bytes;
        const auto pos=32+random()%(bytes.size()-32);
        mutated[pos]^=static_cast<std::byte>(1u<<(random()%8));
        const auto hash=rfcx_detail::fnv1a64(std::span<const std::byte>(mutated).subspan(32));
        for(unsigned i=0;i<8;++i)mutated[24+i]=static_cast<std::byte>((hash>>(8*i))&0xffu);
        std::optional<std::uint64_t> accepted_hash;
        try {
            const auto decoded=decode_temporal_segment(mutated,limits);
            const auto replayed=replay_temporal_segment(old,decoded);
            accepted_hash=temporal_endpoint_hash(replayed);
        } catch(const std::exception&) { }
        if(accepted_hash)check(*accepted_hash==temporal_endpoint_hash(next),"mutated frame published a wrong endpoint");
    }
    check(temporal_endpoint_hash(old)==segment.base_endpoint_hash,"parser/replay mutation changed base");
}

void moments_words_and_topology() {
    auto old=endpoint(false),next=endpoint(true); const std::vector<TemporalCorrespondence> maps{{20,inclusion()}};
    const auto replayed=replay_temporal_segment(old,decode_temporal_segment(encode_temporal_segment(make_temporal_segment(old,next,maps))));
    const auto oldM=temporal_tensor_matrix(tensor_at(old,4)),newM=temporal_tensor_matrix(*temporal_endpoint_detail::tensors(replayed).at(4));
    const std::vector<R> x{2,0},xp{2,0,7};
    const auto moment=temporal::changing_dimension_cross_moment(x,x,oldM,xp,xp,newM,inclusion(),inclusion());
    check(moment.total==numeric::dot(xp,newM.apply(xp))-numeric::dot(x,oldM.apply(x)),"persisted changing-dimension moment failed");
    check(moment.left_innovation!=0 && moment.right_innovation!=0,"mixed new-direction metric cross terms omitted");
    const field::ExactMetric metric(newM);
    const std::vector<temporal::MultiTimeChannel> channels{{"structural",{1,0,2}},{"innovation",{-1,0,3}},{"metric observation",{0,2,0}}};
    const auto gram=temporal::multi_time_channel_gram(channels,metric);
    R full=0,diagonal=0; std::vector<R> total(3,0);
    for(std::size_t i=0;i<channels.size();++i){total=numeric::add(total,channels[i].target_field); diagonal+=gram.get(i,i); for(std::size_t j=0;j<channels.size();++j)full+=gram.get(i,j);}
    check(full==metric.inner(total,total) && diagonal!=full,"typed channel cross moments not retained");
    // Persist noncommuting typed operators as distinct roles, then evaluate the
    // existing exact finite-word law from reconstructed endpoint data.
    TemporalEndpoint a(triangle(false)),b(triangle(false),1);
    a.carriers={{1,TemporalCarrierKind::Explicit,{},std::nullopt,1,numeric::IndexDomain::linear(2)}}; b.carriers=a.carriers;
    const std::vector<TemporalTensorRole> roles{TemporalTensorRole::Accession,TemporalTensorRole::Reconstruction,TemporalTensorRole::PhysicalRealization,
        TemporalTensorRole::SourceRealization,TemporalTensorRole::Observation,TemporalTensorRole::SemanticScaleMap};
    for(std::size_t i=0;i<roles.size();++i){
        a.tensors.push_back(map_tensor(i+10,roles[i],1,1,M::from_rows({{1,R{static_cast<long>(i+1)}},{0,1}})));
        b.tensors.push_back(map_tensor(i+10,roles[i],1,1,M::from_rows({{1,0},{R{static_cast<long>(i+2)},1}})));
    }
    const auto bb=replay_temporal_segment(a,decode_temporal_segment(encode_temporal_segment(make_temporal_segment(a,b))));
    std::vector<M> old_ops,new_ops,js(roles.size()+1,M::identity(2));
    for(std::size_t i=0;i<roles.size();++i){old_ops.push_back(temporal_tensor_matrix(a.tensors[i]));new_ops.push_back(temporal_tensor_matrix(bb.tensors[i]));check(bb.tensors[i].role==roles[i],"typed map roles conflated");}
    const auto defect=temporal::finite_word_defect(temporal::OperationWord(old_ops),temporal::OperationWord(new_ops),js);
    check(defect.exact_identity(),"replayed noncommuting word finite identity failed");

    core::RelationSignature sig;const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=sig.add_type("face",core::BoundaryPolicy::DeclaredExact,3,3);
    auto ring=[&](bool filled){
        core::RelationalComplexBuilder builder(sig);
        for(std::uint64_t i=0;i<3;++i)builder.add_grade_one(core::RelationKey{100+i},edge,{
            {10+i,true,true,R{1},{},core::OccurrenceKey{1000+2*i}},
            {10+(i+1)%3,true,false,R{1},{},core::OccurrenceKey{1001+2*i}}});
        if(filled)builder.add_higher(core::Grade{2},core::RelationKey{200},face,{
            {0,true,false,R{1},{},core::OccurrenceKey{2000}},{1,true,false,R{1},{},core::OccurrenceKey{2001}},{2,true,false,R{1},{},core::OccurrenceKey{2002}}});
        TemporalEndpoint e(builder.freeze(),filled?1:0);
        for(std::size_t k=0;k<e.source.grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)}; const auto dim=e.source.grade(grade).cell_count();
            e.carriers.push_back({k,TemporalCarrierKind::SourceGrade,grade,std::nullopt,0,numeric::IndexDomain::linear(dim)});
            e.tensors.push_back(map_tensor(100+k,TemporalTensorRole::Metric,k,k,M::identity(dim)));
        }
        e.tensors.push_back(field_tensor(500,1,{1,1,1}));return e;
    };
    const auto r0=ring(false),r1=ring(true);const auto sr=make_temporal_segment(r0,r1);const auto r2=replay_temporal_segment(r0,sr);
    check(temporal_field_innovation(r0,r2,500).nonzeros()==0,"face birth forced field innovation");
    auto metrics=[](const TemporalEndpoint& e){std::vector<field::ExactMetric> ms;const auto ts=temporal_endpoint_detail::tensors(e);for(std::size_t k=0;k<e.source.grade_count();++k)ms.emplace_back(temporal_tensor_matrix(*ts.at(100+k)));return ms;};
    const auto m0=metrics(r0),m1=metrics(r2);
    const field::ExactHodgeOperator h0(r0.source,core::Grade{1},m0),h1(r2.source,core::Grade{1},m1);
    const auto p0=field::exact_hodge_projectors(h0),p1=field::exact_hodge_projectors(h1);
    check(p0.project_harmonic({1,1,1})==std::vector<R>({1,1,1}) && p1.project_curl({1,1,1})==std::vector<R>({1,1,1}),"replayed face birth failed harmonic-to-curl transport");
    const temporal::RelationalSourceDelta delta(r0.source,r2.source);
    const auto B1=field::boundary_matrix(r0.source,core::Grade{1}),B1p=field::boundary_matrix(r2.source,core::Grade{1});
    const M B2(3,0);const auto B2p=field::boundary_matrix(r2.source,core::Grade{2});
    const auto E1=B1p.multiply(delta.correspondence(core::Grade{1})).add(delta.correspondence(core::Grade{0}).multiply(B1).scale(R{-1}));
    const auto E2=B2p.multiply(delta.correspondence(core::Grade{2})).add(delta.correspondence(core::Grade{1}).multiply(B2).scale(R{-1}));
    check(B1p.multiply(E2).add(E1.multiply(B2)).nonzeros()==0,"cross-grade exact compatibility failed after persistence");
    // Native products remain lazy: reconstruct factors, then use their retained
    // pair identities and occurrence/Koszul terms, not a flattened graph.
    algebra::RelationalTensorProduct product(r2.source,r0.source);
    const auto product_cell=product.cell(core::Grade{1},0,core::Grade{1},0);
    const auto identity=product.source_identity(product_cell);
    check(identity.left.key==100 && identity.right.key==100,"product factor identities lost");
    bool saw_right=false;
    for(const auto& term:product.boundary_terms(product_cell))if(term.side==algebra::ProductBoundarySide::Right){saw_right=true;check(term.koszul_flip,"product Koszul sign lost");}
    check(saw_right,"product right-factor occurrences missing");
    const auto first=product.boundary_apply_lazy(algebra::SparseProductField::basis(product_cell));
    check(product.boundary_apply_lazy(first).empty(),"replayed-factor product violates graded chain identity");
}

bool positive_by_minors(const M& form) {
    if (form.rows() != form.cols() || form != form.transpose()) return false;
    for (std::size_t n = 1; n <= form.rows(); ++n) {
        M principal(n,n);
        for (const auto& [key,value] : form.entries())
            if (key.first < n && key.second < n) principal.set(key.first,key.second,value);
        if (principal.determinant() <= 0) return false;
    }
    return true;
}
void metric_certification() {
    const auto first = checks;
    const auto source = triangle(false);
    const auto endpoint_for = [&](const M& form) {
        TemporalEndpoint endpoint(source);
        endpoint.carriers = {{700,TemporalCarrierKind::Explicit,{}, {},701,
                              numeric::IndexDomain::linear(form.rows())}};
        endpoint.tensors = {map_tensor(702,TemporalTensorRole::Metric,700,700,form)};
        return endpoint;
    };
    const auto verify = [&](const M& form) {
        const bool expected = positive_by_minors(form);
        bool reference = true;
        try { (void)field::ExactMetric(form); } catch (const std::invalid_argument&) { reference = false; }
        auto endpoint = endpoint_for(form);
        const auto original = endpoint.tensors[0].tensor;
        bool accepted = true;
        try { validate_temporal_endpoint(endpoint); } catch (const std::invalid_argument&) { accepted = false; }
        check(accepted == expected, "stored metric certification disagrees with exact leading minors");
        check(accepted == reference, "stored metric certification differs from the general exact certificate");
        check(temporal_endpoint_detail::tensor_equal(original,endpoint.tensors[0].tensor), "metric certification changed support or values");
        if (accepted) {
            const auto bytes = encode_temporal_endpoint(endpoint);
            const auto restored = decode_temporal_endpoint(source,bytes);
            check(encode_temporal_endpoint(restored) == bytes, "metric certification changed endpoint roundtrip");
        }
    };
    for (const auto& form : std::vector<M>{M{0,0},M{2,3},M::diagonal({R{1,3}}),M::diagonal({R{0}}),
            M::diagonal({R{-1,3}}),M::diagonal({R{2,3},R{5,7},R{11,13}}),
            M::diagonal({R{2,3},R{0},R{11,13}}),M::diagonal({R{2,3},R{-5,7},R{11,13}}),
            M::from_rows({{3,2},{2,3}}),M::from_rows({{1,1},{1,1}}),M::from_rows({{1,2},{2,1}}),
            M::from_rows({{2,1},{0,2}}),M::from_rows({{0,1},{1,2}})}) verify(form);
    numeric::Integer huge; mpz_ui_pow_ui(huge.get_mpz_t(),2,521);
    verify(M::diagonal({R{huge},R{1,huge},R{huge,huge+1}}));
    auto partial = endpoint_for(M::diagonal({R{2,3},R{5,7},R{11,13}}));
    auto& tensor = partial.tensors[0].tensor;
    tensor = {tensor.domain(),numeric::StructuralSupport::sparse(9,{0,4,8}),tensor.values()};
    validate_temporal_endpoint(partial);
    check(partial.tensors[0].tensor.state(0) == R{2,3} && !partial.tensors[0].tensor.state(1),
          "metric certificate completed absent off-diagonal support");
    const auto bytes = encode_temporal_endpoint(partial);
    check(encode_temporal_endpoint(decode_temporal_endpoint(source,bytes)) == bytes, "partial metric support roundtrip changed");
    auto foreign = partial;
    foreign.carriers.push_back({703,TemporalCarrierKind::Explicit,{}, {},704,numeric::IndexDomain::linear(3)});
    foreign.tensors[0].input = 703;
    rejects([&] { validate_temporal_endpoint(foreign); }, "equal-sized metric carriers identified");
    foreign = partial; foreign.tensors[0].input.reset();
    rejects([&] { validate_temporal_endpoint(foreign); }, "missing metric carrier accepted");
    foreign = partial; foreign.tensors[0].owner.kind = TemporalOwnerKind::Cell;
    foreign.tensors[0].owner.cell = {core::Grade{0},123456};
    rejects([&] { validate_temporal_endpoint(foreign); }, "metric certificate accepted an absent owner");
    std::mt19937 random(874);
    for (std::size_t trial = 0; trial < 240; ++trial) {
        const auto n = 1+trial%6; M form(n,n);
        for (std::size_t i = 0; i < n; ++i) for (std::size_t j = 0; j <= i; ++j) {
            if (trial%4 == 0 && i != j) continue;
            const R value{static_cast<long>(random()%9)-4,static_cast<unsigned long>(1+random()%7)};
            form.set(i,j,value); form.set(j,i,value);
        }
        if (trial%4 == 1) form = form.transpose().multiply(form).add(M::identity(n));
        if (trial%4 == 3 && n > 1) form.set(0,1,form.get(1,0)+1);
        verify(form);
    }
    M large(4096,4096);
    for (std::size_t i = 0; i < large.rows(); ++i) large.set(i,i,R{1,3});
    auto declared = endpoint_for(large);
    validate_temporal_endpoint(declared);
    check(temporal_tensor_matrix(declared.tensors[0]) == large, "large diagonal certificate changed a direction");
    large.set(4095,4095,0);
    declared = endpoint_for(large);
    rejects([&] { validate_temporal_endpoint(declared); }, "large diagonal certificate omitted the final missing pivot");
    std::cout << "metric_certification_checks=" << checks-first << '\n';
}
} // namespace

int main() {
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir=std::filesystem::temp_directory_path()/("rfc_phase03_"+std::to_string(stamp));
    try {
        std::filesystem::create_directories(dir);
        coupled_roundtrip(dir); occurrence_regressions(); tensor_local_evolution(); exact_and_random_source();
        retained_magnitude_and_parser(); moments_words_and_topology(); metric_certification();
        const numeric::SparseRationalEntry half{3,R{1,2}}, one{3,R{1}}, prior{2,R{100}};
        check(half == numeric::SparseRationalEntry{3,R{1,2}}, "exact sparse entry equality");
        check(half < one, "exact sparse entry rational ordering");
        check(prior < half, "exact sparse entry coordinate-first ordering");
        const algebra::ProductCell pc{{core::Grade{1},0},{core::Grade{0},0}};
        check(algebra::SparseProductEntry{pc,R{1,3}} < algebra::SparseProductEntry{pc,R{1,2}}, "exact product entry rational ordering");
        check(algebra::SparseProductEntry{pc,R{1,2}} == algebra::SparseProductEntry{pc,R{1,2}}, "exact product entry equality");
        std::filesystem::remove_all(dir);
        std::cout<<"Phase03 exact temporal checks passed: "<<checks<<'\n'; return 0;
    } catch (const std::exception& e) {
        std::filesystem::remove_all(dir);
        std::cerr<<"Phase03 check "<<checks<<" failed: "<<e.what()<<'\n'; return 1;
    }
}
