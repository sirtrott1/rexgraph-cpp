#include "rfc/inference/relational_field_interface_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <string_view>

using namespace rfc;
using namespace rfc::inference;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using T = numeric::SparseRationalTensor;
using Program = OwnedRelationalFieldProgram;
using Declaration = RelationalFieldProgramDeclaration;
namespace {
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
T tensor(const V& values) {
    numeric::ExactActionGuard guard; return field_interface_detail::tensor(values, guard);
}
storage::NativeSeriesEndpoint source() {
    core::RelationSignature signature;
    const auto relation = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, relation,
        {{10, true, false, Q{1}}, {20, true, true, Q{1}}, {10, true, true, Q{1}},
         {20, false, false, Q{1}}, {30, true, false, Q{0}}});
    builder.add_grade_one(core::RelationKey{101}, relation,
        {{10, true, false, Q{1}}, {20, true, false, Q{1, 2}}, {30, true, true, Q{1}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 7);
    return storage::NativeTemporalSeries(endpoint, 991).tail();
}
Declaration declaration() {
    Declaration d; d.key = 51; d.revision = 2; d.differential = true;
    d.stalks = {{{core::Grade{0}, 0}, 1001, {101, 102}, M::from_rows({{2, 1}, {1, 3}})},
                {{core::Grade{0}, 1}, 1002, {201, 202, 203}, M::identity(3)},
                {{core::Grade{0}, 2}, 1001, {101, 102}, M::identity(2)},
                {{core::Grade{1}, 0}, 1003, {301, 302}, M::from_rows({{3, 1}, {1, 2}})},
                {{core::Grade{1}, 1}, 1003, {301, 302}, M::identity(2)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{core::Grade{1}, 0, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 1, phi, M::from_rows({{1, 0}, {0, 1}, {1, 1}})},
                     {core::Grade{1}, 2, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {core::Grade{1}, 4, phi, M::identity(2)},
                     {core::Grade{1}, 5, phi, M::from_rows({{2, 1}, {1, 3}})},
                     {core::Grade{1}, 6, phi, M::from_rows({{0, 1}, {1, 0}, {2, -1}})},
                     {core::Grade{1}, 7, storage::TemporalTensorRole::RealizedIncidenceMap,
                         M::from_rows({{-1, 1}, {2, -2}})}};
    d.nodes = {{1, inference::FieldProgramNodeKind::Input, core::Grade{1}, {}, {}},
               {2, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 1, {}},
               {3, inference::FieldProgramNodeKind::DeclaredMap, core::Grade{1}, 2,
                   M::from_rows({{1, 0, 1, 0, 0, 0, 0}, {0, 1, 0, 1, 0, 0, 0},
                                 {0, 0, 0, 0, 1, 1, 0}, {1, 0, 0, 0, 0, 0, 1}})},
               {4, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 3, {}}};
    return d;
}
void json(const V& values) {
    std::cout << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << '"' << values[i] << '"'; } std::cout << ']';
}
void json(const M& matrix) {
    std::cout << '['; for (std::size_t i = 0; i < matrix.rows(); ++i) {
        if (i) std::cout << ',';
        V row(matrix.cols());
        for (std::size_t j = 0; j < row.size(); ++j) row[j] = matrix.get(i, j);
        json(row);
    } std::cout << ']';
}

RelationalInputUseDeclaration asset_declaration(const core::RelationalComplex& s) {
    RelationalInputUseDeclaration d; d.key = 600; d.channel = 700; d.encoding = "UTF-8";
    const std::string raw = "caf\xc3\xa9 / caf\xc3\xa9";
    for (const auto c : raw) d.bytes.push_back(static_cast<std::byte>(c));
    RelationalSpanUse first{111, {{core::Grade{1}, 100}}, {}, {{0,5}}};
    for (std::size_t p = 0; p < s.grade(core::Grade{1}).incidence_count(); ++p)
        first.occurrences.push_back({core::Grade{1}, s.occurrence_identity(core::Grade{1}, p)});
    d.uses = {first, {112, {{core::Grade{1}, 101}}, {}, {{8,13}}},
                    {113, {{core::Grade{1}, 100}, {core::Grade{1}, 100}}, {}, {{8,13}, {1,4}, {0,0}, {0,5}}}};
    return d;
}
OwnedRelationalInputUseAsset asset(std::uint64_t ns = 992) {
    const auto p = source(); storage::NativeTemporalSeries history(p.endpoint(), ns);
    auto next = p.endpoint(); next.sequence = 8; history.append_target(next);
    next.sequence = 9; history.append_target(next); history.checkpoint(8);
    return {history, asset_declaration(p.endpoint().source)};
}
RelationalInputAccessionDeclaration accession_declaration() {
    RelationalInputAccessionDeclaration a; a.key = 901; a.node = 1; a.source_grade = {1};
    a.source_stalks = {{{core::Grade{1},0}, 910, {911}, M::from_rows({{3}})},
                      {{core::Grade{1},1}, 920, {921}, M::from_rows({{1}})}};
    a.instances.cells = {{0,1,2},{0,1}}; a.instances.occurrences = {{}, {0,1,2,3,4,5,6,7}};
    a.accession = M::from_rows({{1,1},{0,1},{1,2},{0,0}});
    a.reconstruction = M::from_rows({{1,-1,0,0},{0,1,0,0}}); a.isometric = false; return a;
}
OwnedAccessionFieldResult execute(const Program& program, const OwnedRelationalInputUseAsset& bytes, const V& x, std::uint64_t use = 111) {
    const OwnedRelationalInputAccession a(bytes, program, accession_declaration());
    return execute_relational_inputs(program, {a.admit(use, tensor(x))});
}
RelationalFieldViewDeclaration redundant() {
    return {3001, 3101, {3111,3112,3113}, M::from_rows({{1,1},{0,1},{1,2}}),
        M::from_rows({{1,-1,0},{0,1,0}}), {}};
}
RelationalFieldViewDeclaration oblique() {
    return {3002, 3102, {3121,3122}, M::from_rows({{1,2},{0,1}}), M::from_rows({{1,-2},{0,1}}), M::from_rows({{1,0},{1,2}})};
}
RelationalFieldInterface interface(const OwnedAccessionFieldResult& parent, std::uint64_t branch = 41) {
    return {parent, {4001, branch, {{2, {core::Grade{0},0}}}}};
}
void corpus(bool emit) {
    const auto publication = source(); const Program program(publication, declaration()); const auto bytes = asset();
    const auto image = encode_relational_input_use_asset(bytes); const auto cold_asset = decode_relational_input_use_asset(image, {});
    check(encode_relational_input_use_asset(cold_asset) == image, "asset canonical full history bytes");
    check(storage::encode_native_series(bytes.history()) == storage::encode_native_series(cold_asset.history()), "original full history cold replay");
    check(cold_asset.history().first_sequence() == 7 && cold_asset.history().last_sequence() == 9 && cold_asset.history().segment_count() == 2, "source history/cutoff preserved");
    const auto first = bytes.recover(111, 5); const auto second = bytes.recover(112, 5);
    check(first == second && first.front().size() == 5, "multibyte literal recovery equal without use identification");
    const auto fragments = bytes.recover(113, 13);
    check(fragments.size() == 4 && fragments[0] == second[0] && fragments[2].empty() && fragments[3] == first[0], "ordered repeated/discontinuous/empty byte recovery");
    check(cold_asset.declaration().uses[0].occurrences.size() == 8, "inactive/zero original flag accessions retained");
    for (std::size_t trial = 0; trial < 40; ++trial) {
        V x{Q{static_cast<long>(trial)-7, 3}, Q{static_cast<long>(trial%11)-4, 5}};
        for (auto& q : x) q.canonicalize();
        const auto parent = execute(program, bytes, x, trial%2 ? 111 : 112);
        const auto result = parent.result(); const auto represented = parent.inputs().front().represented().values().dense();
        check(represented == V({x[0]+x[1],x[1],x[0]+2*x[1],0}), "declared whole source/model accession");
        check(result.occurrence_field(2).domain().logical_size() == 19, "complete original occurrence carrier");
        const auto i = interface(parent); const auto left = i.observe(redundant()); const auto right = i.observe(oblique());
        const auto glued = glue_relational_readings({left,right}); const auto summed = sum_relational_contributions({left,right});
        const auto original = i.original().values().dense();
        check(left.reconstructed().values().dense() == original && right.reconstructed().values().dense() == original, "both oblique faithful reconstructions");
        check(left.visible().state(2) == left.visible().state(0).value() + left.visible().state(1).value(), "redundant admitted image");
        check(glued.section().values().at({core::Grade{0},0}) == original && glued.section().values().size() == 1, "compatible interface occurs once");
        check(summed.section().values().at({core::Grade{0},0}) == V({2*original[0],2*original[1]}) && summed.kind() == FieldSectionKind::ContributionSum, "contribution assembly distinct full witnesses");
        const auto N = M::from_rows({{2,1},{1,3}}); const auto R = redundant().reconstruction;
        check(left.metric() == R.transpose().multiply(N).multiply(R) && left.metric().rank() == 2, "singular ambient represented metric retains positive image");
        check(redundant().accession.transpose().multiply(left.metric()).multiply(redundant().accession) == N, "faithful metric pullback");
        const auto archive = encode_relational_field_section(glued); const auto cold = decode_relational_field_section(archive, {});
        check(encode_relational_field_section(cold) == archive && cold.section().values() == glued.section().values(), "cold section complete reconstructed replay");
        check(cold.readings()[0].interface().same_owner(cold.readings()[1].interface()), "cold interface shared identity restored once");
        check(!cold.readings()[0].interface().same_owner(i), "cold independent owner is not silently old owner");
        const auto cold_parent = cold.readings()[0].interface().parent(); const auto cold_result = cold_parent.result();
        for (const auto key : {1u,2u,3u,4u}) check(cold_result.field(key).values().dense() == result.field(key).values().dense(), "complete intermediary retained before decode");
        check(cold_parent.inputs()[0].use() == (trial%2 ? 111u : 112u), "source use distinct despite equal encoding");
        const auto sum_archive = encode_relational_field_section(summed);
        check(encode_relational_field_section(decode_relational_field_section(sum_archive, {})) == sum_archive, "cold contribution kind/witnesses");
        if (emit) {
            std::cout << "{\"source\":"; json(x); std::cout << ",\"input\":"; json(represented);
            std::cout << ",\"boundary\":"; json(result.field(2).values().dense());
            std::cout << ",\"occurrences\":"; json(result.occurrence_field(2).values().dense());
            std::cout << ",\"middle\":"; json(result.field(3).values().dense()); std::cout << ",\"final\":"; json(result.field(4).values().dense());
            std::cout << ",\"original\":"; json(original); std::cout << ",\"visible\":"; json(left.visible().values().dense());
            std::cout << ",\"oblique\":"; json(right.visible().values().dense()); std::cout << ",\"metric\":"; json(left.metric());
            std::cout << ",\"glued\":"; json(glued.section().values().at({core::Grade{0},0}));
            std::cout << ",\"sum\":"; json(summed.section().values().at({core::Grade{0},0})); std::cout << "}\n";
        }
    }
}
void admission_cases() {
    const auto publication = source(); const Program program(publication, declaration()); const auto bytes = asset();
    const auto a = accession_declaration(); const auto parent = execute(program, bytes, {1,2}); const auto i = interface(parent);
    const auto left = i.observe(redundant()); const auto right = i.observe(oblique());
    auto d = bytes.declaration(); d.uses[0].spans[0].end = 14;
    rejects([&] { OwnedRelationalInputUseAsset bad(bytes.history(), d); }, "out of channel byte span rejected");
    d = bytes.declaration(); d.uses[0].cells[0].key = 999;
    rejects([&] { OwnedRelationalInputUseAsset bad(bytes.history(), d); }, "wrong actual source cell rejected");
    d = bytes.declaration(); d.uses[0].occurrences[0].key = {999};
    rejects([&] { OwnedRelationalInputUseAsset bad(bytes.history(), d); }, "wrong original occurrence rejected");
    rejects([&] { bytes.recover(113, 12); }, "bounded duplicated recovery rejected before materialization");
    auto map = a; map.instances.occurrences[1].pop_back();
    rejects([&] { OwnedRelationalInputAccession bad(bytes, program, map); }, "missing original inactive flag map rejected");
    map = a; map.instances.occurrences[1][3] = 4;
    rejects([&] { OwnedRelationalInputAccession bad(bytes, program, map); }, "zero/inactive source flags cannot be exchanged");
    map = a; map.reconstruction.set(0,0,2);
    rejects([&] { OwnedRelationalInputAccession bad(bytes, program, map); }, "false reconstruction rejected");
    map = a; map.isometric = true;
    rejects([&] { OwnedRelationalInputAccession bad(bytes, program, map); }, "false metric pullback rejected");
    map = a; map.accession = M::from_rows({{1,0},{0,0},{0,1},{0,0}}); map.reconstruction = M::from_rows({{1,0,0,0},{0,0,1,0}}); map.isometric = true;
    const OwnedRelationalInputAccession isometric(bytes, program, map);
    check(isometric.source_metric() == M::from_rows({{3,0},{0,1}}), "actual isometric source metric");
    const OwnedRelationalInputAccession accession(bytes, program, a);
    check(accession.source_metric() != a.accession.transpose().multiply(accession.target_metric()).multiply(a.accession), "non-isometric profile preserves independent actual metrics");
    const T partial(numeric::IndexDomain({2}), numeric::StructuralSupport::sparse(2, {0}), numeric::SparseRationalVector(2, {{0,1}}));
    rejects([&] { accession.admit(111, partial); }, "source absence does not become zero");
    rejects([&] { accession.admit(999, tensor({1,2})); }, "unknown original use rejected");
    const Program other(publication, declaration());
    rejects([&] { execute_relational_inputs(other, {accession.admit(111,tensor({1,2}))}); }, "matching program bytes do not identify actual owner");
    const auto v = left.visible().values().dense(); auto changed = v; changed[2] += 1;
    rejects([&] { i.admit(redundant(), tensor(changed), FieldReadingStatus::Observed); }, "offimage reading rejected");
    changed = redundant().accession.apply({left.reconstructed().state(0).value()+1, left.reconstructed().state(1).value()});
    const auto conflict = i.admit(redundant(), tensor(changed), FieldReadingStatus::Observed);
    rejects([&] { glue_relational_readings({left, conflict}); }, "onimage actual interface disagreement rejected");
    rejects([&] { i.admit(redundant(), tensor(changed), FieldReadingStatus::Derived); }, "false parent-derived reading rejected");
    const auto conflict_sum = sum_relational_contributions({left, conflict});
    check(conflict_sum.readings()[1].status() == FieldReadingStatus::Observed, "measured conflicting contribution is distinct operation");
    const auto independent = interface(parent); const auto branch = interface(parent,42);
    rejects([&] { glue_relational_readings({left, independent.observe(redundant())}); }, "same result/key does not declare same interface");
    rejects([&] { glue_relational_readings({left, branch.observe(redundant())}); }, "literal source shared across interpretation branches does not force agreement");
    const auto new_use = execute(program, bytes, {1,2},112);
    check(encode_accession_field_result(new_use) != encode_accession_field_result(parent), "equal numeric encoding keeps different byte use provenance");
    const auto new_asset = execute(program,asset(993), {1,2});
    check(encode_accession_field_result(new_asset) != encode_accession_field_result(parent), "equal literal/source keys keep different actual source namespace");
    rejects([&] { RelationalFieldInterface bad(parent,{1,1,{{1,{core::Grade{1},0}}}}); }, "interface patch cannot omit complete lower boundary");
    const RelationalFieldInterface closed(parent,{1,1,{{2,{core::Grade{0},0}},{2,{core::Grade{0},1}},{2,{core::Grade{0},2}},{1,{core::Grade{1},0}}}});
    const auto closed_section = closed.section(closed.original());
    check(closed.original().domain().logical_size()==9 && closed_section.values().size()==4, "whole closed patch includes inactive and zero participants");
    auto view = redundant(); view.physical = M(1,3);
    rejects([&] { i.observe(view); }, "physical map not inferred or mistyped");
    view = redundant(); view.directions[1] = view.directions[0];
    rejects([&] { i.observe(view); }, "visible basis identity admission");
    view = redundant(); view.key = 3003; view.directions = {3221,3222,3223};
    const auto false_basis = i.observe(view);
    rejects([&] { glue_relational_readings({left,false_basis}); }, "same visible basis cannot change direction identities");
    view = redundant(); view.physical = M::from_rows({{1,0,0},{0,1,0}});
    const auto false_view = i.observe(view);
    rejects([&] { glue_relational_readings({left,false_view}); }, "same view identity cannot change physical declaration");
    const auto archive = encode_relational_field_section(glue_relational_readings({left,right})); auto damaged = archive; damaged.back() ^= std::byte{1};
    rejects([&] { decode_relational_field_section(damaged,{}); }, "section integrity enforced");
    auto policy = storage::TemporalReadLimits{}; policy.max_records = 2;
    rejects([&] { decode_relational_field_section(archive,policy); }, "cold aggregate container admission");
    auto limits = numeric::ExactActionLimits{}; limits.dimension = 1;
    rejects([&] { i.observe(redundant(),limits); }, "view action dimension admission");
    limits = {}; limits.entries = 1;
    rejects([&] { bytes.recover(111,5,limits); }, "original byte action container admission");
    rejects([&] { i.observe(redundant(),{},[] { throw std::runtime_error("cancel"); }); }, "view cancellation joins synchronous operation");
    using namespace field_interface_codec_detail;
    auto w = envelope(accession_magic); blob(w,encode_relational_field_result(parent.result())); w.u64(1);
    blob(w,encode_relational_input_use_asset(bytes)); field_interface_codec_detail::accession(w,a); w.u64(111);
    storage::temporal_codec_detail::tensor(w,tensor({4,9})); const auto false_witness = finish(std::move(w));
    rejects([&] { decode_accession_field_result(false_witness,{}); }, "valid envelope cannot certify false source/intermediary relationship");
    w = envelope(section_magic); blob(w,encode_accession_field_result(parent)); w.u64(4001); w.u64(41); w.u64(1); w.u64(2); local_cell(w,{core::Grade{0},0});
    w.u8(0); w.u64(2);
    for (const auto& reading : {left,right}) { field_interface_codec_detail::view(w,reading.declaration()); w.u8(0); storage::temporal_codec_detail::tensor(w,reading.visible()); }
    w.u64(1); local_cell(w,{core::Grade{0},0}); w.u64(2); storage::temporal_codec_detail::rational(w,Q{999}); storage::temporal_codec_detail::rational(w,Q{1});
    const auto false_section = finish(std::move(w));
    rejects([&] { decode_relational_field_section(false_section,{}); }, "valid envelope cannot certify false reconstructed stalk field");
}
void owners_and_covariance() {
    const auto publication = source(); const Program program(publication,declaration());
    auto d=asset_declaration(publication.endpoint().source); auto& raw=d.bytes[0]; auto& span=d.uses[0].spans[0];
    bool asset_mutated{};
    const OwnedRelationalInputUseAsset bytes(storage::NativeTemporalSeries(publication.endpoint(),992),std::move(d),{},[&] { if (!asset_mutated) { asset_mutated=true; raw=std::byte{'X'}; span={1,2}; } });
    check(bytes.recover(111,5).front().size()==5 && bytes.recover(111,5).front()[0]==std::byte{'c'}, "moved original asset buffers frozen");
    const auto parent=execute(program,bytes,{2,3}); auto i=interface(parent); const auto expected=i.original().values().dense();
    const auto old=i; bool replaced{};
    const auto reading=i.observe(redundant(),{},[&] { if(!replaced) { replaced=true; i=interface(execute(program,bytes,{7,8})); } });
    check(reading.interface().same_owner(old) && reading.reconstructed().values().dense()==expected, "actual interface pinned before callback wrapper replacement");
    auto view=redundant(); auto& direction=view.directions[0]; bool view_mutated{};
    const auto stable=old.observe(std::move(view),{},[&] { if (!view_mutated) { view_mutated=true; direction=999; } });
    check(stable.declaration().directions[0]==3111 && stable.declaration().accession.get(0,0)==1, "moved view declarations frozen");
    std::vector<std::future<std::vector<std::byte>>> work;
    for(std::size_t k=0;k<6;++k) work.push_back(std::async(std::launch::async,[old] { return encode_relational_field_section(glue_relational_readings({old.observe(redundant()),old.observe(oblique())})); }));
    const auto archive=work[0].get(); for(std::size_t k=1;k<work.size();++k) check(work[k].get()==archive, "independent concurrent exact view/glue scratch");
    const auto S=M::from_rows({{1,1},{0,1}}); const auto Sinv=S.inverse(); auto framed=declaration();
    framed.stalks[0].basis=777; framed.stalks[0].directions={778,779}; framed.stalks[0].metric=Sinv.transpose().multiply(framed.stalks[0].metric).multiply(Sinv);
    const auto& arena=publication.endpoint().source.grade(core::Grade{1});
    for(std::size_t p=0;p<framed.occurrences.size();++p) if(arena.lower_index[p]==0) framed.occurrences[p].map=S.multiply(framed.occurrences[p].map);
    // Both subsequent fixed actions also change their declared bases.
    M lower=M::identity(7); for(std::size_t r=0;r<2;++r) for(std::size_t c=0;c<2;++c) lower.set(r,c,S.get(r,c));
    framed.nodes[2].map=framed.nodes[2].map.multiply(lower.inverse());
    const Program covariant(publication,framed); const auto transformed=interface(execute(covariant,bytes,{2,3}));
    auto coview=redundant(); coview.basis=999; coview.directions={997,998,999}; coview.accession=coview.accession.multiply(Sinv); coview.reconstruction=S.multiply(coview.reconstruction);
    const auto value=transformed.observe(coview);
    check(value.visible().values().dense()==stable.visible().values().dense() && value.metric()==stable.metric(), "whole field basis/metric/accession covariance");
    const auto parent_result=parent.result(); const auto transformed_parent=execute(covariant,bytes,{2,3}); const auto transformed_result=transformed_parent.result();
    for(const auto key:{2u,4u}) { const auto x=parent_result.field(key).values().dense(); check(transformed_result.field(key).values().dense()==lower.apply(x), "complete adjacent intermediate field basis covariance"); }
}
}
int main(int argc,char** argv) {
    try { const bool emit=argc>1 && std::string_view(argv[1])=="--emit"; corpus(emit); admission_cases(); owners_and_covariance();
        if(!emit) std::cout << "relational_field_interface_checks=" << checks << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr << "interface failure after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
