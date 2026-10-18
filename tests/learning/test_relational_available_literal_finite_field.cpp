#include "rfc/learning/relational_available_literal_finite_field_codec.hpp"
#include "rfc/core/builder.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string_view>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using Bytes = std::vector<std::byte>;
using Origin = OwnedRelationalValueOrigin;
using Grant = RelationalOriginGrant;
using Context = RelationalAvailabilityContext;
using Role = RelationalDependencyRole;
using Model = PreparedAvailableRelationalLiteralFiniteFieldMap;
namespace {
constexpr std::size_t maximum = 1u << 24;
std::size_t checks{};
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
template<class F> void rejects_origin(F action, const char* message) {
    bool caught{};
    try { action(); } catch (const std::invalid_argument& e) {
        const std::string reason = e.what(); caught = reason.find("origin") != std::string::npos || reason.find("availability") != std::string::npos;
    }
    check(caught, message);
}
std::vector<std::byte> bytes(std::string_view s) {
    std::vector<std::byte> out; for (const auto c : s) out.push_back(static_cast<std::byte>(c)); return out;
}
storage::NativeTemporalSeries history() {
    core::RelationSignature signature;
    const auto type = signature.add_type("original-use-group", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10, true, true, 1}, {20, true, false, 1}, {10, true, false, 1},
         {20, false, false, 1}, {30, true, false, 0}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze()), 904011);
}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series) {
    RelationalFieldProgramDeclaration d; d.key = 1001; d.revision = 1; d.differential = true;
    d.stalks = {{{{0},0},101,{1011,1012},M::identity(2)}, {{{0},1},102,{1021},M::identity(1)},
        {{{0},2},102,{1021},M::identity(1)}, {{{1},0},201,{11,12,13,14,15,16,17,18},M::identity(8)}};
    for (std::size_t p = 0; p < 5; ++p) {
        const auto width = p == 0 || p == 2 ? 2u : 1u;
        M map(width,8); map.set(0,0,numeric::rational(static_cast<std::int64_t>(p+1)));
        if (width == 2) map.set(1,1,numeric::rational(static_cast<std::int64_t>(p+2)));
        d.occurrences.push_back({{1},p,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    d.nodes = {{1,FieldProgramNodeKind::Input,{1},{},{}}, {2,FieldProgramNodeKind::Boundary,{0},1,{}},
        {3,FieldProgramNodeKind::DeclaredMap,{0},1,
         M::from_rows({{2,0,0,1,0,0,0,0},{0,1,3,0,0,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,2,-1}})}};
    return {series.tail(),d};
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series, std::string_view text = "bank?") {
    RelationalInputUseDeclaration d; d.key = 2001; d.channel = 2011; d.encoding = "UTF-8"; d.bytes = bytes(text);
    d.uses = {{111,{{{0},10},{{1},100}},{},{{0,4}}},
        {112,{{{0},20},{{1},100}},{},{{4,5}}},
        {113,{{{0},30},{{1},100}},{},{{0,2},{2,4},{0,0},{0,2}}}};
    const auto publication = series.tail();
    for (std::size_t slot = 0; slot < 5; ++slot) {
        const auto index = slot == 4 ? 2u : slot % 2;
        d.uses[index].occurrences.push_back({{1},publication.endpoint().source.occurrence_identity({1},slot)});
    }
    return {series,d};
}
OwnedRelationalLiteralInput literal(OwnedRelationalInputUseAsset a, OwnedRelationalFieldProgram p) {
    RelationalInputAccessionDeclaration d; d.key = 3001; d.node = 1; d.source_grade = {1}; d.isometric = true;
    d.source_stalks = {p.declaration().stalks.back()};
    d.instances.cells = {{0,1,2},{0}}; d.instances.occurrences = {{},{0,1,2,3,4}};
    d.accession = M::identity(8); d.reconstruction = M::identity(8);
    OwnedRelationalInputAccession accession(a,p,d);
    RelationalLiteralInputDeclaration r; r.key = 4001; r.revision = 1; r.use = 111;
    const auto add = [&](std::uint64_t use, std::size_t ordinal, RelationalByteSpan span,
                         RelationalLiteralPredicateKind kind, std::string_view operand) {
        const auto i = r.coordinates.size(); r.coordinates.push_back({5001+i,{{1},0},11+i,use,ordinal,span,kind,bytes(operand)});
    };
    using K = RelationalLiteralPredicateKind;
    add(111,0,{0,4},K::Equal,"bank"); add(111,0,{0,4},K::Prefix,"ba");
    add(111,0,{0,4},K::Suffix,"nk"); add(111,0,{0,4},K::AsciiPunctuation,"");
    add(112,0,{4,5},K::AsciiPunctuation,""); add(113,0,{0,2},K::Equal,"ba");
    add(113,1,{2,4},K::Equal,"nk"); add(113,2,{0,0},K::Equal,"");
    return {accession,r};
}
RelationalFiniteSourceDeclaration finite_declaration(OwnedRelationalInputUseAsset a) {
    RelationalFiniteSourceDeclaration d; d.key = 6001; d.revision = 1;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto use = 111+i, symbol = 6101+i;
        d.symbols.push_back({symbol,encode_relational_literal_use_value(a,use,maximum)});
        d.variables.push_back({{{0},static_cast<std::uint32_t>(i)},{symbol}});
        d.bindings.push_back({6201+i,use,{{0},static_cast<std::uint32_t>(i)}});
    }
    d.charts = {{6301,"whole-literal-facts",{{{0},0},{{0},1},{{0},2},{{1},0}},
        {{{0},0},{{0},1},{{0},2}},{{6101,6102,6103}}}};
    return d;
}
RelationalLiteralFiniteFieldDeclaration declaration(OwnedRelationalLiteralInput input) {
    return {7001,1,{{7101,{6201,6202,6203},{6101,6102,6103},0,{std::move(input)}, {}}}};
}
void publish(std::vector<Grant>& grants, std::uint64_t key, Role role, Bytes payload, const storage::NativeSeriesPrefix& prefix) {
    grants.push_back({Origin(key, prefix, role, std::move(payload)), {prefix.source_namespace(), prefix.cutoff()}});
}
Context withheld(std::vector<Grant> grants, std::uint64_t key) {
    std::erase_if(grants, [key](const auto& g) { return g.origin.key() == key; }); return Context(std::move(grants));
}
struct Fixture {
    Model model;
    PreparedAvailableRelationalEvidence evidence;
    storage::NativeSeriesPrefix publisher;
    std::vector<Grant> grants;
};
Fixture fixture() {
    auto h = history(); const auto p = program(h); const auto a = asset(h); const auto l = literal(a, p);
    const auto tail = h.tail(); auto endpoint = tail.endpoint(); ++endpoint.sequence; h.append_target(endpoint);
    const auto prefix = storage::NativeSeriesPrefix::capture(h, h.last_sequence()); std::vector<Grant> grants;
    const auto pb = encode_relational_field_program(p);
    publish(grants, 1, Role::Program, pb, prefix); publish(grants, 2, Role::Maps, pb, prefix); publish(grants, 3, Role::Metrics, pb, prefix);
    const PreparedAvailableRelationalProgram prepared(p, {1,2,3}, Context(grants));
    const RelationalFieldParameterDeclaration pd{8001,1,2,{{2,1,1},{1,0,4}},M::identity(2),{8101,8102}};
    publish(grants, 4, Role::Evidence, relational_evidence_root_payload(pb, pd, {}), prefix);
    const PreparedAvailableRelationalEvidence evidence(prepared, pd, 4, Context(grants));
    const auto ab = encode_relational_input_use_asset(a); const auto d = finite_declaration(a);
    publish(grants, 10, Role::InputAsset, ab, prefix);
    publish(grants, 20, Role::Evidence, relational_finite_constraint_payload(ab, d, RelationalFiniteRelationshipStatus::Observed), prefix);
    const PreparedAvailableRelationalFiniteSource finite(a, d, RelationalFiniteRelationshipStatus::Observed, {10,20}, Context(grants));
    const auto accession = l.input().accession();
    const auto vp = relational_input_value_payload(ab, accession.declaration(), l.input().use(), l.input().source_field());
    const auto ap = relational_input_accession_payload(pb, ab, accession.declaration(), l.input().use(), l.input().source_field());
    publish(grants, 11, Role::InputValue, vp, prefix); publish(grants, 12, Role::InputAccession, ap, prefix); publish(grants, 13, Role::Reconstruction, ap, prefix);
    const RelationalAvailableInputSubmission submission{{a, accession.declaration(), l.input().use(), l.input().source_field()}, {10,11,12,13}};
    auto rd = declaration(l); rd.rules.push_back({7102,rd.rules[0].bindings,rd.rules[0].symbols,1,{l},{}});
    rd.rules.push_back({7103,rd.rules[0].bindings,rd.rules[0].symbols,{},{},"unresolved whole interpretation"});
    RelationalAvailableFiniteFieldMapDeclaration md{rd.key, rd.revision, {}};
    for (const auto& r : rd.rules) md.rules.push_back({r.key, r.bindings, r.symbols, r.target,
        r.target ? std::vector<RelationalAvailableInputSubmission>{submission} : std::vector<RelationalAvailableInputSubmission>{}, r.unsupported});
    const std::vector<AvailableRelationalFiniteFieldTarget> targets{prepared, evidence};
    const auto mp = relational_finite_interpretation_payload(finite, targets, md, RelationalFiniteRelationshipStatus::Observed, Context(grants));
    publish(grants, 30, Role::Maps, mp, prefix); publish(grants, 31, Role::Evidence, mp, prefix);
    const PreparedAvailableRelationalFiniteFieldMap base(finite, targets, md, RelationalFiniteRelationshipStatus::Observed, {30,31}, Context(grants));
    const auto native = base.map(); const PreparedRelationalLiteralFiniteFieldMap reader(finite.finite(), native.targets(), rd);
    publish(grants, 41, Role::Interface, encode_relational_literal_input(l, maximum), prefix);
    publish(grants, 42, Role::Interface, encode_relational_literal_input(l, maximum), prefix);
    const RelationalLiteralFiniteFieldOrigins keys{{{41},{42},{}},50};
    publish(grants, 50, Role::Interface, relational_literal_finite_interface_payload(base, reader, keys.inputs, Context(grants), maximum), prefix);
    return {Model(base, reader, keys, Context(grants), maximum), evidence, prefix, grants};
}
Bytes authority_bytes(const std::vector<Grant>& grants) {
    using namespace field_program_codec_detail; Writer w; w.u64(grants.size());
    for (const auto& g : grants) {
        w.u64(g.origin.key()); w.u8(static_cast<std::uint8_t>(g.origin.role())); blob(w, storage::encode_native_series(g.origin.publication().history()));
        blob(w, g.origin.payload_bytes()); w.u64(g.availability.source_namespace); w.u64(g.availability.max_sequence);
    }
    return std::move(w).take_bytes();
}
Context authority(const Bytes& data) {
    const storage::TemporalReadLimits policy; field_program_codec_detail::Reader r(data, policy); std::vector<Grant> grants;
    const auto n = r.count(34);
    for (std::size_t i=0; i<n; ++i) {
        const auto key=r.u64(); const auto role=static_cast<Role>(r.u8()); const auto h=storage::decode_native_series(r.blob());
        const auto raw=r.blob(); Bytes payload(raw.begin(),raw.end());
        const auto prefix=storage::NativeSeriesPrefix::capture(h,h.last_sequence());
        grants.push_back({Origin(key,prefix,role,std::move(payload)),{r.u64(),r.u64()}});
    }
    check(!r.remaining(),"authority trailing bytes"); return Context(grants);
}
void save(const std::filesystem::path& path, const Bytes& data) {
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));check(bool(out),"write failed");
}
Bytes load(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);check(bool(in),"read failed");const std::vector<char> raw{std::istreambuf_iterator<char>(in),{}};
    Bytes out;for(const auto c:raw)out.push_back(static_cast<std::byte>(c));return out;
}
V field_values(const AvailableRelationalLiteralFiniteFieldEvaluation& value, std::uint64_t node) {
    const auto result=value.raw_result().result(); const auto fields=result.result();return fields.field(node).values().dense();
}
void run(const std::filesystem::path& exports = {}) {
    const auto f=fixture();auto grants=f.grants;const Context context(grants);const auto model=f.model;
    const auto result=model.execute(context);const auto raw=result.raw_result();
    check(result.size()==3 && raw.branch(2).status==RelationalFiniteFieldStatus::Unsupported,"complete unresolved alternatives lost");
    check(result.model().same_owner(model),"result dropped complete reader-aware owner");
    const auto reader=model.reader();const auto base=model.base();
    check(reader.source().same_owner(base.source().finite()),"reader replaced actual finite owner");
    const auto& l=reader.declaration().rules[0].inputs[0];
    check(l.input().accession().asset().same_owner(reader.source().asset()),"literal derivation replaced actual byte owner");
    const auto selected=result.select(0,3,context);check(selected[1].parent().model().same_owner(model),"selected branch dropped reader owner");
    const auto mb=encode_available_relational_literal_finite_field_map(model,maximum);
    const auto rb=encode_available_relational_literal_finite_field_result(result,maximum);
    const storage::TemporalReadLimits policy;
    const auto cold=decode_available_relational_literal_finite_field_result(rb,context,policy);
    check(encode_available_relational_literal_finite_field_result(cold,maximum)==rb,"canonical reader-aware family cold replay differs");
    const auto cold_model=cold.model();const auto cold_reader=cold_model.reader();const auto cold_base=cold_model.base();
    check(!cold_model.same_owner(model)&&cold_reader.source().same_owner(cold_base.source().finite()),"cold restoration failed to adopt actual restored finite owner");
    check(encode_available_relational_literal_finite_field_map(decode_available_relational_literal_finite_field_map(mb,context,policy),maximum)==mb,"canonical reader-aware model cold replay differs");
    check(prepare_available_relational_literal_finite_field_map(model,maximum).bytes()==mb&&prepare_available_relational_literal_finite_field_result(result,maximum).bytes()==rb,"prepared streams differ");
    std::set<std::uint64_t> seen;
    for (const auto& obligation:model.obligations()) if(seen.insert(obligation.key()).second) {
        const auto absent=withheld(grants,obligation.key());
        rejects_origin([&]{model.admit(absent);},"live full closure omitted origin");
        rejects_origin([&]{(void)model.execute(absent);},"warm execution ignored missing origin");
        rejects_origin([&]{(void)selected[0].admit(absent);},"escaped selected branch ignored missing origin");
        rejects_origin([&]{(void)decode_available_relational_literal_finite_field_result(rb,absent,policy);},"cold family omitted origin before replay");
        auto early=grants;for(auto& g:early)if(g.origin.key()==obligation.key())g.availability.max_sequence=0;
        rejects_origin([&]{model.admit(Context(early));},"actual future publication admitted under earlier cutoff");
        auto foreign=grants;for(auto& g:foreign)if(g.origin.key()==obligation.key())++g.availability.source_namespace;
        rejects_origin([&]{model.admit(Context(foreign));},"foreign namespace admitted");
        auto clone=grants;for(auto& g:clone)if(g.origin.key()==obligation.key())g.origin=Origin(g.origin.key(),g.origin.publication(),g.origin.role(),g.origin.payload());
        rejects_origin([&]{model.admit(Context(clone));},"equal receipt established actual live origin identity");
    }
    auto wrong=grants;for(auto& g:wrong)if(g.origin.key()==50)g.origin=Origin(50,g.origin.publication(),Role::Maps,g.origin.payload());
    rejects_origin([&]{(void)decode_available_relational_literal_finite_field_result(rb,Context(wrong),policy);},"wrong whole-reader origin role accepted");
    auto keys=model.origins();keys.inputs.pop_back();rejects([&]{(void)Model(base,reader,keys,context,maximum);},"missing complete rule origin row accepted");
    keys=model.origins();keys.inputs[0].clear();rejects([&]{(void)Model(base,reader,keys,context,maximum);},"missing literal derivation accepted");
    auto d=reader.declaration();auto old=d.rules[0].inputs[0];auto recipe=old.declaration();recipe.coordinates[0].kind=RelationalLiteralPredicateKind::Prefix;
    d.rules[0].inputs[0]=OwnedRelationalLiteralInput(old.input().accession(),recipe);
    const auto reader_map=reader.map();const PreparedRelationalLiteralFiniteFieldMap changed(reader.source(),reader_map.targets(),d);
    rejects_origin([&]{(void)Model(base,changed,model.origins(),context,maximum);},"equal numerical field erased changed literal recipe");
    auto changed_grants=grants;
    for(auto& g:changed_grants)if(g.origin.key()==41)g.origin=Origin(41,g.origin.publication(),Role::Interface,encode_relational_literal_input(d.rules[0].inputs[0],maximum));
    rejects_origin([&]{(void)Model(base,changed,model.origins(),Context(changed_grants),maximum);},"updated perliteral origin bypassed unchanged whole reader Interface");
    // Corrupt only the deferred native witness, retaining the complete passive
    // declaration and its original grants. Missing reader authority must be
    // reported before replay encounters the noncanonical native witness.
    const auto base_bytes=encode_available_relational_finite_field_map(base);
    const auto map_bytes=encode_relational_finite_field_map(base.map());
    storage::BytePacketBuilder broken_base(available_finite_detail::model_magic,maximum);
    broken_base.write([&](auto& w){w.raw(std::span<const std::byte>(base_bytes).subspan(32,base_bytes.size()-32-8-map_bytes.size()));});
    broken_base.blob(bytes("invalid deferred native witness"));const auto fake_base=std::move(broken_base).finish().bytes();
    const auto& required=model.obligations();const std::vector<Origin> input_origins{required[required.size()-3],required[required.size()-2]};
    const auto fake_payload=available_literal_detail::interface_payload(fake_base,prepare_relational_literal_finite_field_map(reader,maximum),model.origins().inputs,input_origins,maximum,{}).bytes();
    auto fake_grants=grants;for(auto& g:fake_grants)if(g.origin.key()==50)g.origin=Origin(50,g.origin.publication(),Role::Interface,fake_payload);
    storage::BytePacketBuilder broken_model(available_literal_detail::model_magic,maximum);broken_model.blob(fake_base);broken_model.blob(prepare_relational_literal_finite_field_map(reader,maximum));
    broken_model.u64(3);for(const auto& row:model.origins().inputs){broken_model.u64(row.size());for(const auto key:row)broken_model.u64(key);}
    broken_model.u64(50);broken_model.u64(3);for(const auto& origin:input_origins)broken_model.receipt(origin);broken_model.receipt(fake_grants.back().origin);
    const auto broken_packet=std::move(broken_model).finish().bytes();
    rejects_origin([&]{(void)decode_available_relational_literal_finite_field_map(broken_packet,withheld(fake_grants,50),policy);},"deferred native replay preceded reader authority");
    rejects([&]{(void)decode_available_relational_literal_finite_field_map(broken_packet,Context(fake_grants),policy);},"available but invalid native witness was accepted");
    const auto clone_source=decode_relational_finite_source(encode_relational_finite_source(reader.source()),policy);
    const auto clone_asset=clone_source.asset();auto foreign_d=reader.declaration();
    for(auto& rule:foreign_d.rules)for(auto& input:rule.inputs){const auto a=input.input().accession();input=OwnedRelationalLiteralInput(OwnedRelationalInputAccession(clone_asset,a.program(),a.declaration()),input.declaration());}
    const auto native=reader.map();const PreparedRelationalLiteralFiniteFieldMap foreign_reader(clone_source,native.targets(),foreign_d);
    rejects([&]{(void)Model(base,foreign_reader,model.origins(),context,maximum);},"equal complete bytes established actual finite owner identity");
    auto limits=numeric::ExactActionLimits{};limits.dimension=0;
    rejects_origin([&]{(void)model.execute(withheld(grants,50),limits);},"execution did arithmetic admission before authority");
    rejects([&]{(void)model.execute(context,limits);},"execution dimension policy ignored");
    rejects([&]{(void)result.select(1,3,context);},"selection bounds ignored");
    auto small=policy;small.max_file_bytes=32;rejects([&]{(void)decode_available_relational_literal_finite_field_result(rb,context,small);},"cold byte admission ignored");
    small=policy;small.max_records=1;rejects([&]{(void)decode_available_relational_literal_finite_field_result(rb,context,small);},"cold record admission ignored");
    rejects([&]{(void)encode_available_relational_literal_finite_field_result(result,32);},"encoding byte admission ignored");
    bool stopped{};rejects([&]{(void)model.execute(context,{},[&]{stopped=true;throw std::runtime_error("stop");});},"cancellation ignored");check(stopped,"callback not reached");
    for(const auto cut:{std::size_t{0},std::size_t{31},rb.size()/2,rb.size()-1})rejects([&]{(void)decode_available_relational_literal_finite_field_result(std::span<const std::byte>(rb).first(cut),context,policy);},"truncated composite accepted");
    auto corrupt=rb;corrupt.back()^=std::byte{1};rejects([&]{(void)decode_available_relational_literal_finite_field_result(corrupt,context,policy);},"corrupt composite accepted");
    corrupt=rb;corrupt.push_back(std::byte{0});rejects([&]{(void)decode_available_relational_literal_finite_field_result(corrupt,context,policy);},"trailing composite accepted");
    auto mutable_bytes=rb;bool mutated{};
    const auto frozen=decode_available_relational_literal_finite_field_result(mutable_bytes,context,policy,{},[&]{if(!mutated){mutated=true;mutable_bytes.clear();}});
    check(mutated&&encode_available_relational_literal_finite_field_result(frozen,maximum)==rb,"decoder borrowed mutable caller packet");
    publish(grants,60,Role::Prospective,relational_parameter_setting_payload(f.evidence,{3,-2}),f.publisher);
    publish(grants,61,Role::Prospective,relational_literal_finite_completion_payload(result,1,{3,-2},60,Context(grants),maximum),f.publisher);
    publish(grants,62,Role::Prospective,relational_literal_finite_completion_payload(result,0,{},0,Context(grants),maximum),f.publisher);
    const auto generic_family=base.execute(Context(grants));
    publish(grants,63,Role::Prospective,relational_finite_completion_payload(generic_family,1,{3,-2},60,Context(grants)),f.publisher);
    const Context decisions(grants);const auto evaluation=result.evaluate(1,{3,-2},60,61,decisions,maximum);const auto fixed=result.evaluate(0,{},0,62,decisions,maximum);
    check(field_values(evaluation,2)==V{2,5,0,0}&&field_values(fixed,2)==V{2,2,2,0},"common affine parameters failed to change complete heterogeneous response");
    const auto evaluated=evaluation.raw_result().result();const auto fields=evaluated.result();
    check(fields.occurrence_field(2).domain().logical_size()==7&&evaluated.inputs()[0].accession().asset().same_owner(reader.source().asset()),"complete flags or original use owner lost");
    const auto eb=encode_available_relational_literal_finite_field_evaluation(evaluation,maximum);
    check(prepare_available_relational_literal_finite_field_evaluation(evaluation,maximum).bytes()==eb,"prepared decision differs");
    const auto ec=decode_available_relational_literal_finite_field_evaluation(eb,decisions,policy);
    check(encode_available_relational_literal_finite_field_evaluation(ec,maximum)==eb&&field_values(ec,2)==field_values(evaluation,2),"complete reader-aware decision cold replay differs");
    const auto fb=encode_available_relational_literal_finite_field_evaluation(fixed,maximum);
    check(encode_available_relational_literal_finite_field_evaluation(decode_available_relational_literal_finite_field_evaluation(fb,decisions,policy),maximum)==fb,"fixed reader-aware decision cold replay differs");
    for(const auto key:{41u,42u,50u,60u,61u}){
        rejects_origin([&]{evaluation.admit(withheld(grants,key));},"escaped evaluation dropped reader/setting/choice obligation");
        rejects_origin([&]{(void)decode_available_relational_literal_finite_field_evaluation(eb,withheld(grants,key),policy);},"cold decision replayed before all reader/setting/choice origins");
    }
    rejects_origin([&]{(void)result.evaluate(0,{},0,61,decisions,maximum);},"whole-reader choice reused another branch");
    rejects_origin([&]{(void)result.evaluate(1,{3,-2},60,63,decisions,maximum);},"generic finite choice discarded reader contract");
    rejects([&]{(void)result.evaluate(2,{},0,62,decisions,maximum);},"unsupported alternative fabricated a zero decision");
    check(encode_available_relational_literal_finite_field_result(result,maximum)==rb,"decision mutated complete intermediary family");
    const auto escaped=[] {const auto local=fixture();const Context authority(local.grants);return std::pair{local.model.execute(authority),authority};}();
    escaped.first.admit(escaped.second);check(escaped.first.size()==3,"result borrowed producer lifetime");
    std::vector<std::future<Bytes>> jobs;
    for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[&]{const auto copy=decode_available_relational_literal_finite_field_evaluation(eb,decisions,policy);return encode_available_relational_literal_finite_field_evaluation(copy,maximum);}));
    for(auto& job:jobs)check(job.get()==eb,"immutable concurrent cold restoration differs");
    const auto prepared=std::get<PreparedAvailableRelationalProgram>(base.targets()[0]);
    const auto& submission=base.declaration().rules[0].inputs[0];
    const auto admitted=prepared.admit_input(submission.input,submission.origins,decisions);
    const auto training=prepared.execute({admitted},decisions);
    const auto root_program=prepared.program();const auto query_asset=reader.source().asset();
    auto publisher=f.publisher.history();const auto prior=publisher.tail();auto next=prior.endpoint();++next.sequence;publisher.append_target(next);
    const auto updated_prefix=storage::NativeSeriesPrefix::capture(publisher,publisher.last_sequence());
    numeric::ExactActionGuard target_guard;const auto target=field_interface_detail::tensor(V{2,5,0,0},target_guard);
    publish(grants,210,Role::TargetAsset,encode_relational_input_use_asset(query_asset),updated_prefix);
    publish(grants,211,Role::TargetValue,relational_target_value_payload(encode_relational_field_program(root_program),2,
        encode_relational_input_use_asset(query_asset),111,target),updated_prefix);
    const RelationalAvailableTrainingObservation observation{9101,training,query_asset,111,target,{{{0},0},{{0},1},{{0},2}},{210,211}};
    const auto child=f.evidence.append({observation},Context(grants));const auto child_native=child.evidence();
    check(child_native.kernel().cols()==0,"actual target update did not identify complete two-axis family");
    const std::vector<AvailableRelationalFiniteFieldTarget> child_targets{prepared,child};
    auto child_decl=base.declaration();++child_decl.revision;
    const auto child_payload=relational_finite_interpretation_payload(base.source(),child_targets,child_decl,base.status(),Context(grants));
    publish(grants,130,Role::Maps,child_payload,updated_prefix);publish(grants,131,Role::Evidence,child_payload,updated_prefix);
    const PreparedAvailableRelationalFiniteFieldMap child_base(base.source(),child_targets,child_decl,base.status(),{130,131},Context(grants));
    auto child_recipe=reader.declaration();++child_recipe.revision;const auto child_map=child_base.map();
    const PreparedRelationalLiteralFiniteFieldMap child_reader(child_base.source().finite(),child_map.targets(),child_recipe);
    auto child_keys=model.origins();child_keys.interface=150;
    publish(grants,150,Role::Interface,relational_literal_finite_interface_payload(child_base,child_reader,child_keys.inputs,Context(grants),maximum),updated_prefix);
    const Model child_model(child_base,child_reader,child_keys,Context(grants),maximum);const auto child_result=child_model.execute(Context(grants));
    const auto updated_bytes=encode_available_relational_literal_finite_field_result(child_result,maximum);
    check(encode_available_relational_literal_finite_field_result(decode_available_relational_literal_finite_field_result(updated_bytes,Context(grants),policy),maximum)==updated_bytes,"actual evidence update reader-aware cold replay differs");
    for(const auto key:{210u,211u,130u,131u,150u}){
        rejects_origin([&]{child_model.admit(withheld(grants,key));},"updated reader model lost new target/map/Interface origin");
        rejects_origin([&]{(void)decode_available_relational_literal_finite_field_result(updated_bytes,withheld(grants,key),policy);},"updated cold reader model ignored new origin");
        model.admit(withheld(grants,key));
    }
    auto old_cut=grants;for(auto& g:old_cut)if(g.origin.publication().cutoff()==2)g.availability.max_sequence=1;
    rejects_origin([&]{child_model.admit(Context(old_cut));},"new actual history update admitted at parent cutoff");
    model.admit(Context(old_cut));
    check(encode_available_relational_literal_finite_field_result(result,maximum)==rb,"evidence update rewrote retained parent reader");
    if(!exports.empty()){
        std::filesystem::create_directories(exports);save(exports/"model.packet",mb);save(exports/"result.packet",rb);save(exports/"evaluation.packet",eb);
        save(exports/"fixed-evaluation.packet",fb);save(exports/"authority.bin",authority_bytes(grants));
    }
}
} // namespace
int main(int argc,char** argv){
    try{
        if(argc==3&&std::string_view(argv[1])=="write")run(argv[2]);
        else if(argc==3&&std::string_view(argv[1])=="read"){
            const std::filesystem::path dir(argv[2]);const auto context=authority(load(dir/"authority.bin"));const storage::TemporalReadLimits policy;
            const auto bytes=load(dir/"evaluation.packet");const auto value=decode_available_relational_literal_finite_field_evaluation(bytes,context,policy);
            check(encode_available_relational_literal_finite_field_evaluation(value,maximum)==bytes&&field_values(value,2)==V{2,5,0,0},"fresh standalone whole decision differs");
            const auto data=load(dir/"result.packet");const auto result=decode_available_relational_literal_finite_field_result(data,context,policy);check(result.size()==3&&encode_available_relational_literal_finite_field_result(result,maximum)==data,"fresh standalone family differs");
        }else{check(argc==1,"unknown arguments");run();}
        std::cout<<checks<<" available literal field checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
