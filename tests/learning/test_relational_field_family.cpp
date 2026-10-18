#include "rfc/learning/relational_field_family_codec.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using Q = numeric::Rational;
using Evidence = PreparedRelationalFieldEvidence;
using Family = PreparedRelationalFieldFamily;
namespace {
std::size_t checks{};
void check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f, const char* why) {
    bool caught{}; try { f(); } catch (const std::exception&) { caught = true; } check(caught, why);
}
numeric::SparseRationalTensor tensor(const V& v) { numeric::ExactActionGuard g; return field_interface_detail::tensor(v, g); }
storage::NativeSeriesEndpoint source(bool inactive = false) {
    core::RelationSignature s; const auto t = s.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder b(s);
    if (inactive) b.add_grade_one(core::RelationKey{100}, t, {{10,true,false,1},{20,true,false,1},{10,false,false,1},{20,true,false,0}});
    else b.add_grade_one(core::RelationKey{100}, t, {{10,true,false,1},{20,true,false,1}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(), 7), 1000).tail();
}
OwnedRelationalFieldProgram program(bool inactive = false, Q origin = 0) {
    const auto p = source(inactive); RelationalFieldProgramDeclaration d; d.key = 1; d.revision = 1; d.differential = true;
    d.stalks = {{{{0},0},100,{101,102},M::from_rows({{2,1},{1,3}})},
                {{{0},1},200,{201,202},M::identity(2)}, {{{1},0},300,{301,302},M::identity(2)}};
    for (std::size_t i = 0; i < (inactive ? 4u : 2u); ++i)
        d.occurrences.push_back({{1},i,storage::TemporalTensorRole::CoefficientIndependentMap, i == 0 ? M::identity(2).scale(origin) : M(2,2)});
    d.nodes = {{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}},
        {3,FieldProgramNodeKind::DeclaredMap,{0},2,M::from_rows({{1,1,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}})}};
    return {p,d};
}
RelationalFieldParameterDeclaration parameters(bool inactive = false) {
    RelationalFieldParameterDeclaration d; d.key = 2; d.revision = 1; d.node = 2;
    for (std::size_t p = 0; p < (inactive ? 3u : 1u); ++p) for (std::size_t r = 0; r < 2; ++r) for (std::size_t c = 0; c < 2; ++c) {
        if (inactive && p == 1) continue;
        d.variables.push_back({p,r,c}); d.parameters.push_back(1001+d.parameters.size());
    }
    d.basis = M::identity(d.parameters.size()); return d;
}
OwnedRelationalInputUseAsset asset(bool inactive = false) {
    const auto p = source(inactive); storage::NativeTemporalSeries h(p.endpoint(), 2000);
    RelationalInputUseDeclaration d; d.key = 3; d.channel = 4; d.encoding = "UTF-8";
    d.bytes = {std::byte{'a'},std::byte{'z'}}; d.uses = {{5,{{{1},100}},{},{{0,2}}}};
    for (std::size_t i = 0; i < (inactive ? 4u : 2u); ++i) d.uses[0].occurrences.push_back({{1},p.endpoint().source.occurrence_identity({1},i)});
    return {h,d};
}
algebra::RetainedSourceMap identity(bool inactive = false) {
    return {{{0,1},{0}},{{}, inactive ? std::vector<std::size_t>{0,1,2,3} : std::vector<std::size_t>{0,1}}};
}
OwnedAccessionFieldResult input(const OwnedRelationalFieldProgram& p, V x, bool inactive = false) {
    RelationalInputAccessionDeclaration d; d.key = 6; d.node = 1; d.source_grade = {1}; d.isometric = true;
    d.source_stalks = {p.declaration().stalks[2]}; d.instances = identity(inactive); d.accession = M::identity(2); d.reconstruction = M::identity(2);
    const OwnedRelationalInputAccession a(asset(inactive),p,d);
    return execute_relational_inputs(p,{a.admit(5,tensor(x))});
}
Evidence trained(const OwnedRelationalFieldProgram& p, bool right, Q offset = 0) {
    const Evidence root(p,parameters()); const auto x = right ? V{0,1} : V{1,0};
    auto supplied = input(p,x); const auto original = supplied.inputs().front().accession();
    return root.append({{right?9u:8u,supplied,original.asset(),5,tensor(right?V{-1+offset,3,0,0}:V{2+offset,4,0,0}),{{{0},0},{{0},1}}}});
}
RelationalFieldFamilyDeclaration declaration(bool inactive = false) {
    RelationalFieldFamilyInterface i{10,1,0,1,source(inactive),identity(inactive),identity(inactive),{}, {}};
    i.left_views = {{{10000,M::identity(2),M::identity(2)},{10000,M::identity(2),M::identity(2)}},{{10001,M::identity(2),M::identity(2)}}};
    i.right_views = i.left_views; return {11,1,{i}};
}
void write(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    std::ofstream out(path,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(out),"packet write");
}
std::vector<std::byte> read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary); check(bool(in),"packet read");
    const std::vector<char> chars{std::istreambuf_iterator<char>(in),{}}; std::vector<std::byte> out;
    for (const auto c : chars) out.push_back(static_cast<std::byte>(c));
    return out;
}
void run(const std::filesystem::path& directory, bool cold) {
    const auto p = program(); const auto left = trained(p,false), right = trained(p,true);
    check(left.kernel().cols() == 2 && right.kernel().cols() == 2,"independent members leave correlated directions");
    const auto packet_path = directory / "family.packet";
    const auto family = cold ? decode_relational_field_family(read(packet_path),{}) : Family(declaration(),{left,right});
    check(family.gluing().consistent() && family.gluing().coordinate_kernel().cols() == 0,"joint interface identifies maps");
    check(family.members()[0].observations().size() == 1 && family.members()[0].parent().has_value(),"original proof lineage retained");
    if (!cold) check(family.members()[0].same_owner(left) && family.members()[1].same_owner(right),"actual evidence owners reused");
    const auto owner = family.members()[0].program(); const auto supplied = input(owner,{1,1});
    const auto result = family.execute(0,supplied); check(result.field(1).particular.values().dense() == V({1,1}),"whole original input retained");
    check(result.field(2).particular.values().dense() == V({1,7,0,0}),"independent held full boundary");
    check(result.field(3).particular.values().dense() == V({8,7,0,0}),"complete downstream field");
    check(result.occurrence_field(2).particular.values().dense() == V({1,7,0,0}),"all original flags retained");
    for (std::size_t i = 0; i < 4; ++i) check(result.identified(2,i),"joint boundary identification");
    const auto map = family.coefficient_independent_map(0,{1},0);
    check(map.particular() == M::from_rows({{2,-1},{4,3}}) && map.directions().empty(),"full intrinsic map jointly identified");
    const auto evaluation = result.evaluate({}); const auto materialized = evaluation.result(); const auto fields = materialized.result();
    check(fields.field(2).values().dense() == result.field(2).particular.values().dense(),"native prospective materialization");
    const auto family_bytes = encode_relational_field_family(family,1u<<24);
    const auto result_bytes = encode_relational_joint_result(result,1u<<24);
    const auto evaluation_bytes = encode_relational_joint_evaluation(evaluation,1u<<24);
    if (cold) {
        check(family_bytes == read(packet_path),"cold family canonical bytes");
        const auto restored = decode_relational_joint_result(read(directory/"result.packet"),{});
        check(encode_relational_joint_result(restored,1u<<24) == result_bytes,"cold whole-field result");
        const auto restored_evaluation = decode_relational_joint_evaluation(read(directory/"evaluation.packet"),{});
        check(encode_relational_joint_evaluation(restored_evaluation,1u<<24) == evaluation_bytes,"cold complete materialization");
    } else {
        write(packet_path,family_bytes); write(directory/"result.packet",result_bytes); write(directory/"evaluation.packet",evaluation_bytes);
    }
    std::vector<std::byte> streamed;
    prepare_relational_joint_result(result,1u<<24).stream([&](std::span<const std::byte> part) { streamed.insert(streamed.end(),part.begin(),part.end()); });
    check(streamed == result_bytes,"bounded prepared streaming canonical");
    rejects([&]{(void)encode_relational_field_family(family,family_bytes.size()-1);},"exact byte cap");
    rejects([&]{(void)encode_relational_field_family(family,1u<<24,[]{throw std::runtime_error("stop");});},"prepare cancellation");
    rejects([&]{(void)decode_relational_field_family(family_bytes,{}, {},[]{throw std::runtime_error("stop");});},"read cancellation");
    auto corrupt = family_bytes; corrupt.back() ^= std::byte{1};
    rejects([&]{(void)decode_relational_field_family(corrupt,{});},"corruption rejection");
    rejects([&]{(void)decode_relational_field_family(std::span(family_bytes).first(family_bytes.size()-1),{});},"truncation rejection");
    auto cap = family.limits(); --cap.entries;
    rejects([&]{(void)decode_relational_field_family(family_bytes,{},cap);},"archive policy cap");
    rejects([&]{(void)family.execute(0,supplied,cap);},"policy mismatch");
    rejects([&]{(void)family.execute(0,input(program(),{1,1}));},"foreign owner refusal");
    rejects([&]{(void)result.evaluate({1});},"complete joint coordinate shape");
    rejects([&]{(void)result.field(999);},"unknown field");
    rejects([&]{(void)result.occurrence_field(1);},"input has no invented flags");
    rejects([&]{(void)result.identified(2,4);},"field coordinate bounds");
    auto invalid = declaration(); invalid.interfaces[0].right_views[1][0].reconstruction = M(2,2);
    rejects([&]{(void)Family(invalid,{left,right});},"reconstruction validation");
    invalid = declaration(); invalid.interfaces[0].common_right.occurrences[1][0] = 1;
    rejects([&]{(void)Family(invalid,{left,right});},"complete occurrence identity validation");
    invalid = declaration(); invalid.interfaces.push_back(invalid.interfaces[0]);
    rejects([&]{(void)Family(invalid,{left,right});},"duplicate interface key");
    invalid = declaration(); invalid.interfaces[0].right = 2;
    rejects([&]{(void)Family(invalid,{left,right});},"member bounds validation");
    rejects([&]{(void)Family({11,1,{}},{});},"nonempty family");
    rejects([&]{(void)Family({11,1,{}},{Evidence(program(false,1),parameters())});},"nonzero original Phi refuses tangent coercion");
    const auto inactive_program = program(true); const Evidence unknown(inactive_program,parameters(true));
    const Family independent({11,1,{}},{unknown});
    check(independent.gluing().coordinate_kernel().cols() == 8,"unshared inactive map alternatives retained");
    const auto independent_result = independent.execute(0,input(inactive_program,{1,1},true));
    check(independent_result.occurrence_field(2).directions.rows() == 8,"inactive and supported zero flag slots retained");
    const auto independent_evaluation = independent_result.evaluate(V(8,1));
    const auto independent_actual = independent_evaluation.result(); const auto independent_fields = independent_actual.result();
    check(independent_fields.field(2).values().dense() == V({2,2,0,0}),"nontrivial joint free evaluation and inactive preservation");
    const Family disconnected({11,1,{}},{left,right});
    check(disconnected.gluing().coordinate_kernel().cols() == 4,"disconnected member alternatives retained");
    const auto disconnected_result = disconnected.execute(1,input(p,{1,1}));
    const V disconnected_free{5,6,7,8}; const auto disconnected_evaluation = disconnected_result.evaluate(disconnected_free);
    const auto disconnected_execution = disconnected_evaluation.result(); const auto disconnected_fields = disconnected_execution.result();
    const auto expected_local = right.execute(input(p,{1,1})).evaluate({7,8});
    const auto local_execution = expected_local.result(); const auto local_fields = local_execution.result();
    check(disconnected_fields.field(2).values().dense() == local_fields.field(2).values().dense(),"member shift and joint coordinate block action");
    const auto inactive_map = independent.coefficient_independent_map(0,{1},2);
    check(inactive_map.directions().size() == 8,"inactive intrinsic maps remain unknown");
    rejects([&]{(void)Family(declaration(true),{unknown,unknown});},"shared inactive variable requires intrinsic comparison");
    const auto incompatible = trained(p,false,1); const Family contradiction(declaration(),{left,incompatible});
    check(!contradiction.gluing().consistent() && !contradiction.gluing().contradiction().empty(),"exact contradiction witness");
    rejects([&]{(void)contradiction.execute(0,supplied);},"inconsistent family cannot execute");
    const auto restored_contradiction = decode_relational_field_family(encode_relational_field_family(contradiction,1u<<24),{});
    check(restored_contradiction.gluing().contradiction() == contradiction.gluing().contradiction(),"contradiction cold preservation");
    auto task = [&] { const auto owned = family.execute(0,supplied); return encode_relational_joint_result(owned,1u<<24); };
    auto a = std::async(std::launch::async,task), b = std::async(std::launch::async,task);
    check(a.get() == result_bytes && b.get() == result_bytes,"immutable concurrent reads");
    const auto retained_result = [&] { Family temporary(declaration(),{left,right}); return temporary.execute(0,input(p,{1,1})); }();
    check(retained_result.field(2).particular.values().dense() == V({1,7,0,0}),"result retains family and source lifetime");
}
} // namespace
int main(int argc, char** argv) {
    try {
        const bool cold = argc == 3 && std::string_view(argv[1]) == "--read";
        const auto directory = argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::temp_directory_path()/("rfc-field-family-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory); run(directory,cold);
        if (argc == 1) std::filesystem::remove_all(directory);
        std::cout << checks << " field family checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
