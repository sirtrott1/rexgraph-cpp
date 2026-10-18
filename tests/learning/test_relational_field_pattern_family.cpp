#include "rfc/learning/relational_field_family_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
namespace {
std::size_t checks{};
void check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f, const char* why) {
    bool caught{}; try { f(); } catch (const std::exception&) { caught = true; } check(caught, why);
}
numeric::SparseRationalTensor tensor(const V& v) {
    numeric::ExactActionGuard guard; return field_interface_detail::tensor(v, guard);
}
struct Chart { std::size_t size; OwnedRelationalFieldProgram program; OwnedRelationalInputAccession accession; };
Chart chart(std::size_t size) {
    core::RelationSignature signature;
    const auto group = signature.add_type("literal group", core::BoundaryPolicy::DeclaredExact, 1, 128);
    const auto word = signature.add_type("ordered word pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto root = signature.add_type("ordered root pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature, core::SemanticScale{3});
    for (std::size_t i = 0; i <= size; ++i) builder.declare_witness_identity(100 + i);
    std::vector<core::BoundaryOccurrenceInput> flags;
    for (std::size_t i = 0; i < size; ++i) flags.emplace_back(100 + i, true, false, 1);
    flags.emplace_back(100, false, false, 1); flags.emplace_back(100, true, false, 0);
    builder.add_grade_one(core::RelationKey{1000}, group, flags);
    for (std::size_t h = 0; h <= size; ++h) for (std::size_t d = 0; d < size; ++d) {
        if (h == d) continue;
        builder.add_grade_one(h == size ? root : word, {{100+h,true,true,1},{100+d,true,false,1}});
    }
    const auto publication = storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze(), 3), 2000+size).tail();
    const auto& source = publication.endpoint().source;
    RelationalFieldProgramDeclaration declaration; declaration.key = 3000; declaration.revision = 1; declaration.differential = true;
    for (std::size_t i = 0; i <= size; ++i) declaration.stalks.push_back({{{0},static_cast<std::uint32_t>(i)},
        i == size ? 4001u : 4000u, i == size ? std::vector<std::uint64_t>{4103} : std::vector<std::uint64_t>{4100,4101,4102}, M::identity(i == size ? 1 : 3)});
    const auto& upper = source.grade({1});
    for (std::size_t i = 0; i < upper.cell_count(); ++i) {
        const auto width = i == 0 ? size*2 : (upper.relation_types[i] == root ? 2u : 4u);
        std::vector<std::uint64_t> directions; for (std::size_t j = 0; j < width; ++j) directions.push_back(4300+j);
        declaration.stalks.push_back({{{1},static_cast<std::uint32_t>(i)},i == 0 ? 4200u : (upper.relation_types[i] == root ? 4202u : 4201u),directions,M::identity(width)});
    }
    for (std::size_t p = 0; p < upper.incidence_count(); ++p) {
        const auto upper_cell = upper.upper_index[p], lower_cell = upper.lower_index[p];
        const auto width = upper_cell == 0 ? size*2 : (upper.relation_types[upper_cell] == root ? 2u : 4u);
        declaration.occurrences.push_back({{1},p,storage::TemporalTensorRole::CoefficientIndependentMap,M(lower_cell == size ? 1 : 3,width)});
    }
    declaration.nodes = {{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    const OwnedRelationalFieldProgram program(publication,declaration);
    RelationalInputUseDeclaration input; input.key = 5000; input.channel = 5001; input.encoding = "ASCII";
    for (std::size_t i = 0; i < size; ++i) {
        input.bytes.push_back(static_cast<std::byte>('a'+i));
        input.uses.push_back({i+1,{{{0},100+i}},{{{1},source.occurrence_identity({1},i)}},{{i,i+1}}});
    }
    const OwnedRelationalInputUseAsset asset(storage::NativeTemporalSeries(publication.endpoint(),6000+size),input);
    RelationalInputAccessionDeclaration accession; accession.key = 7000; accession.node = 1; accession.source_grade = {1}; accession.isometric = true;
    for (const auto& stalk : declaration.stalks) if (stalk.cell.grade == core::Grade{1}) accession.source_stalks.push_back(stalk);
    accession.instances.cells.resize(2); accession.instances.occurrences.resize(2);
    for (std::size_t i = 0; i <= size; ++i) accession.instances.cells[0].push_back(static_cast<std::uint32_t>(i));
    for (std::size_t i = 0; i < upper.cell_count(); ++i) accession.instances.cells[1].push_back(static_cast<std::uint32_t>(i));
    for (std::size_t p = 0; p < upper.incidence_count(); ++p) accession.instances.occurrences[1].push_back(p);
    const auto width = program.fibers().fibers().stalks().total_dimension({1});
    accession.accession = M::identity(width); accession.reconstruction = M::identity(width);
    return {size,program,OwnedRelationalInputAccession(asset,program,accession)};
}
RelationalFieldParameterDeclaration parameters(const Chart& c) {
    RelationalFieldParameterDeclaration d; d.key = 8000; d.revision = 1; d.node = 2;
    const auto count = c.size == 1 ? 6u : 18u;
    for (std::size_t i = 0; i < count; ++i) d.parameters.push_back(8100+i);
    const auto publication = c.program.publication(); const auto& source = publication.endpoint().source;
    const auto& upper = source.grade({1}); std::vector<std::size_t> columns;
    for (std::size_t i = 1; i < upper.cell_count(); ++i) {
        const bool root = upper.relation_types[i] == core::RelationTypeId{2};
        const auto flag = upper.incidence_range(i).first+1; const auto width = root ? 2u : 4u;
        for (std::size_t r = 0; r < 3; ++r) for (std::size_t j = 0; j < width; ++j) {
            d.variables.push_back({flag,r,j}); columns.push_back((root && c.size != 1 ? 12u : 0u)+r*width+j);
        }
    }
    d.basis = M(columns.size(),count); for (std::size_t i = 0; i < columns.size(); ++i) d.basis.set(i,columns[i],1); return d;
}
OwnedAccessionFieldResult input(const Chart& c, std::size_t relation, V local) {
    const auto offsets = c.program.fibers().fibers().stalks().offsets({1}); V field(offsets.back());
    check(local.size() == offsets[relation+1]-offsets[relation], "complete typed local input");
    std::copy(local.begin(),local.end(),field.begin()+static_cast<std::ptrdiff_t>(offsets[relation]));
    return execute_relational_inputs(c.program,{c.accession.admit(1,tensor(field))});
}
RelationalFieldFamilyInterface interface(const Chart& left, const Chart& right, std::size_t member, bool root) {
    const auto l = root ? 1+left.size*(left.size-1) : 1u;
    const auto r = root ? 1+right.size*(right.size-1) : 1u;
    const auto publication = left.program.publication(); const auto& s = publication.endpoint().source; const auto& upper = s.grade({1});
    const auto lp = upper.incidence_range(l).first;
    const auto other = right.program.publication(); const auto& ru = other.endpoint().source.grade({1}); const auto rp = ru.incidence_range(r).first;
    core::RelationalComplexBuilder builder(s.signature(),s.semantic_scale());
    builder.add_grade_one(upper.relation_types[l],{{1,true,true,1},{2,true,false,1}});
    const auto common = storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze(),3),9000+(root?1u:0u)).tail();
    RelationalFieldFamilyInterface d{9100+member*2+(root?1u:0u),1,0,member,common,
        {{{upper.lower_index[lp],upper.lower_index[lp+1]},{static_cast<std::uint32_t>(l)}},{{},{lp,lp+1}}},
        {{{ru.lower_index[rp],ru.lower_index[rp+1]},{static_cast<std::uint32_t>(r)}},{{},{rp,rp+1}}},{},{}};
    const auto width = root ? 2u : 4u;
    d.left_views = {{{root?9201u:9200u,M::identity(root?1:3),M::identity(root?1:3)},{9202,M::identity(3),M::identity(3)}},
        {{root?9204u:9203u,M::identity(width),M::identity(width)}}}; d.right_views = d.left_views; return d;
}
void run() {
    const auto reference = chart(2), single = chart(1), held = chart(3);
    const PreparedRelationalFieldEvidence root(reference.program,parameters(reference));
    const auto x = input(reference,1,{1,1,0,0}); V y(7); y[3]=2; y[4]=3;
    const auto trained = root.append({{10000,x,reference.accession.asset(),1,tensor(y),{{{0},1}}}});
    check(root.kernel().cols() == 18 && trained.kernel().cols() == 15, "complete correlated maps retained after observation");
    const auto z = input(reference,3,{1,1}); V t(7); t[0]=5; t[1]=7; t[2]=11;
    const auto updated = trained.append({{10001,z,reference.accession.asset(),1,tensor(t),{{{0},0}}}});
    const PreparedRelationalFieldEvidence one(single.program,parameters(single)), three(held.program,parameters(held));
    const RelationalFieldFamilyDeclaration d{11000,1,{interface(reference,single,1,true),interface(reference,held,2,false),interface(reference,held,2,true)}};
    const PreparedRelationalFieldFamily family(d,{updated,one,three});
    check(family.gluing().consistent() && family.gluing().coordinate_kernel().cols() == 12, "one common family across unequal charts");
    check(family.members()[0].same_owner(updated) && family.members()[2].same_owner(three), "actual evidence owners retained");
    check(updated.parent()->same_owner(trained) && trained.parent()->same_owner(root), "updates retain actual parent lineage");
    // The same declared word map acts at a different flag and different use.
    const auto supplied = input(held,5,{1,1,0,0}); const auto result = family.execute(2,supplied);
    check(result.family().same_owner(family), "whole result retains common owner");
    const auto dense = result.field(2).particular.values().dense(); V expected(10); expected[0]=2; expected[1]=3;
    check(dense == expected && !result.field(2).directions.nonzeros(), "learned full response transfers to distinct actual uses");
    const auto fields = result.evaluate(V(12,2)).result().result();
    const auto original = supplied.result();
    check(fields.field(1).values().dense() == original.field(1).values().dense(), "complete parent input retained");
    check(fields.field(2).values().dense() == expected, "native materialization agrees before decoding");
    check(fields.occurrence_field(2).domain().logical_size() == 63, "repeated inactive and supported zero complements retained");
    const auto open = family.execute(2,input(held,5,{1,-1,0,0}));
    check(open.field(2).directions.nonzeros() != 0 && !open.identified(2,0), "unresolved correlated alternatives retained");
    const auto first = open.evaluate(V(12,0)), second = open.evaluate(V(12,1));
    const auto first_fields = first.result().result(), second_fields = second.result().result();
    check(first_fields.field(2).values().dense() != second_fields.field(2).values().dense(), "common free setting changes complete held response");
    const auto root_result = family.execute(1,input(single,1,{1,1}));
    check(root_result.field(2).particular.values().dense() == V({5,7,11,0}), "typed structural root shares without a literal use");
    const auto bytes = encode_relational_field_family(family,1u<<24);
    const auto cold = decode_relational_field_family(bytes,{});
    check(encode_relational_field_family(cold,1u<<24) == bytes && cold.gluing().coordinate_kernel().cols() == 12, "cross-size family canonical restoration");
    const auto result_bytes = encode_relational_joint_result(open,1u<<24);
    check(encode_relational_joint_result(decode_relational_joint_result(result_bytes,{}),1u<<24) == result_bytes, "complete unresolved result canonical restoration");
    auto invalid = d; invalid.interfaces[1].common_right.occurrences[1][1]--;
    rejects([&]{(void)PreparedRelationalFieldFamily(invalid,{updated,one,three});}, "wrong original occurrence refused");
    invalid = d; invalid.interfaces[0].right_views[0][0].frame = 9200;
    rejects([&]{(void)PreparedRelationalFieldFamily(invalid,{updated,one,three});}, "heterogeneous root frame cannot borrow word identity");
    rejects([&]{(void)family.execute(2,input(chart(3),5,{1,1,0,0}));}, "numerically equal foreign program refused");
    auto task = [&]{ const auto value = family.execute(2,supplied); return value.field(2).particular.values().dense(); };
    auto a = std::async(std::launch::async,task), b = std::async(std::launch::async,task);
    check(a.get() == expected && b.get() == expected, "immutable joint reads retain actual family");
}
}
int main() { try { run(); std::cout << checks << " cross-size pattern family checks passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
