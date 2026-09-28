#include "rfc/storage/relational_codes.hpp"
#include "rfc/storage/temporal_seed.hpp"
#include <iostream>
#include <random>

using namespace rfc;
using namespace rfc::storage;
using R = numeric::Rational;
using M = numeric::ExactMatrix;
using Tensor = numeric::SparseRationalTensor;
using Value = RelationalCodeValue;
using Type = RelationalCodeType;
using TypeKind = RelationalCodeTypeKind;
using Op = RelationalCodeOperation;
using Opcode = RelationalCodeOpcode;
namespace {
std::size_t checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F &&action, const char *message) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, message);
}
Tensor tensor(const numeric::IndexDomain &domain, const std::vector<R> &values,
              std::optional<std::vector<std::size_t>> support = std::nullopt) {
    std::vector<numeric::SparseRationalEntry> entries;
    check(domain.logical_size() == values.size(), "test tensor size");
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] != 0)
            entries.push_back({i, values[i]});
    return Tensor(domain,
                  support ? numeric::StructuralSupport::sparse(values.size(), *support)
                          : numeric::StructuralSupport::full(values.size()),
                  numeric::SparseRationalVector(values.size(), std::move(entries)));
}
Value vector(std::initializer_list<R> values) {
    return Value{tensor(numeric::IndexDomain({values.size()}), {values})};
}
Op linear(std::uint64_t id, std::uint64_t input, std::uint64_t output, std::size_t rows, std::size_t cols,
          std::vector<R> values, std::optional<std::vector<std::size_t>> support = std::nullopt) {
    Op op;
    op.key = id;
    op.input = input;
    op.output = output;
    op.opcode = Opcode::Linear;
    op.kernel = tensor(numeric::IndexDomain({rows, cols}), values, support);
    return op;
}
Op identity(std::uint64_t id, std::uint64_t type) {
    Op op;
    op.key = id;
    op.input = op.output = type;
    return op;
}
Op finite(std::uint64_t id, std::uint64_t input, std::uint64_t output, std::vector<std::uint64_t> map) {
    Op op;
    op.key = id;
    op.input = input;
    op.output = output;
    op.opcode = Opcode::FiniteMap;
    op.word = std::move(map);
    return op;
}
Op word(std::uint64_t id, std::uint64_t input, std::uint64_t output, std::vector<std::uint64_t> terms) {
    Op op;
    op.key = id;
    op.input = input;
    op.output = output;
    op.opcode = Opcode::Word;
    op.word = std::move(terms);
    return op;
}
core::RelationalComplex source() {
    core::RelationSignature sig;
    const auto edge = sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(sig);
    builder.declare_witness_identity(10);
    builder.declare_witness_identity(11);
    builder.add_grade_one(core::RelationKey{100}, edge,
                          {{10, true, true, R{1}, {}, core::OccurrenceKey{1000}},
                           {11, true, false, R{1}, {}, core::OccurrenceKey{1001}}});
    return builder.freeze();
}
RelationalCodeSection section(std::uint64_t key, std::uint64_t field, std::uint64_t type, Value value) {
    RelationalCodeSection s;
    s.key = key;
    s.field = field;
    s.type = type;
    s.cell = StableCellKey{core::Grade{0}, 10};
    s.value = std::move(value);
    return s;
}
RelationalCodeState fixture() {
    RelationalCodeState s;
    s.namespace_key = 1234;
    s.types = {{10, TypeKind::Rational, {101, 102}, {}},
               {20, TypeKind::Rational, {201, 202, 203}, {}},
               {30, TypeKind::Rational, {301}, {}},
               {40, TypeKind::Finite, {0, 1}, {}},
               {41, TypeKind::Finite, {0, 1, 2, 3, 4, 5}, {}}};
    s.operations = {linear(1, 10, 30, 1, 2, {1, 0}, std::vector<std::size_t>{0}),
                    linear(2, 20, 30, 1, 3, {1, 0, 0}, std::vector<std::size_t>{0}),
                    linear(3, 10, 10, 2, 2, {2, 0, 0, 1}, std::vector<std::size_t>{0, 3}),
                    linear(4, 20, 20, 3, 3, {2, 0, 0, 0, 1, 0, 0, 0, 1}, std::vector<std::size_t>{0, 4, 8}),
                    linear(5, 30, 30, 1, 1, {2}),
                    identity(6, 40),
                    finite(7, 41, 40, {0, 1, 0, 1, 0, 1}),
                    finite(8, 40, 40, {1, 0}),
                    finite(9, 41, 41, {1, 2, 3, 4, 5, 0}),
                    identity(10, 10),
                    identity(11, 20)};
    s.fields = {1001, 1002};
    s.sections = {section(1, 1001, 10, vector({3, 7})), section(2, 1001, 20, vector({3, 8, 9})),
                  section(3, 1002, 40, Value{std::uint64_t{1}}),
                  section(4, 1002, 41, Value{std::uint64_t{3}})};
    s.interfaces = {{1, 1, 2, 1, 2}, {2, 3, 4, 6, 7}};
    s.descents = {{1, 1, 3, 4, 5}, {2, 2, 8, 9, 8}};
    return s;
}
TemporalEndpoint endpoint(std::uint64_t sequence = 0) {
    TemporalEndpoint e(source(), sequence);
    e.codes = fixture();
    return e;
}
std::vector<std::byte> bytes_at(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("missing fixture");
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}
void write_bytes(const std::filesystem::path &path, const std::vector<std::byte> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out)
        throw std::runtime_error("test fixture write failed");
}
void patch_u64(std::vector<std::byte> &bytes, std::size_t at, std::uint64_t n) {
    for (unsigned i = 0; i < 8; ++i)
        bytes.at(at + i) = static_cast<std::byte>((n >> (8 * i)) & 255u);
}
void rehash(std::vector<std::byte> &bytes) {
    patch_u64(bytes, 24, rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32)));
}

void binary_boundary() {
    composite::BinaryProgram p(3);
    rejects([&] { p.append({static_cast<composite::BinaryOpcode>(255)}); }, "unknown binary opcode accepted");
    check(p.instruction_count() == 0, "failed append mutated program");
    p.append({composite::BinaryOpcode::LoadOne, 0, 65535, 65535, 65535, 65535});
    check(p.instructions().front().a == 0 && p.instructions().front().aspect == 0,
          "unused operands not canonical");
    check(p.execute_word_with_provider(1, 0, [](auto, auto) { return 0ULL; }) == ~std::uint64_t{0},
          "raw machine contract changed");
    check(p.execute_domain_word_with_provider(65, 1, 0, [](auto, auto) { return 0ULL; }) == 1,
          "tail bits not masked");
    for (auto slots : {1u, 63u, 64u, 65u, 127u, 128u, 129u}) {
        const auto last = (slots - 1) / 64;
        const auto bits = slots % 64;
        check(p.execute_domain_word_with_provider(slots, last, 0, [](auto, auto) { return 0ULL; }) ==
                  (bits ? ((std::uint64_t{1} << bits) - 1) : ~std::uint64_t{0}),
              "domain tail mismatch");
    }
    rejects([&] { (void)p.execute_domain_word_with_provider(0, 0, 0, [](auto, auto) { return 0ULL; }); },
            "empty domain has word");
    temporal_codec_detail::Writer w;
    temporal_codec_detail::binary_program(w, p);
    TemporalReadLimits limits;
    temporal_codec_detail::Reader r(w.bytes(), limits);
    check(temporal_codec_detail::binary_program(r).instructions() == p.instructions(),
          "binary word roundtrip");
    auto malformed = w.bytes();
    malformed[10] = std::byte{255};
    rejects(
        [&] {
            temporal_codec_detail::Reader rr(malformed, limits);
            (void)temporal_codec_detail::binary_program(rr);
        },
        "bad opcode decoded");
    malformed = w.bytes();
    malformed[13] = std::byte{1};
    rejects(
        [&] {
            temporal_codec_detail::Reader rr(malformed, limits);
            (void)temporal_codec_detail::binary_program(rr);
        },
        "unused wire operand accepted");
}
void gluing_and_descent() {
    auto s = fixture();
    validate_relational_code_state(s);
    const auto glue = relational_code_glue(s);
    check(glue.size() == 2 && glue[0].compatible && glue[1].compatible, "valid unlike-carrier gluing failed");
    check(matched_relational_code_sections(s).size() == 4, "matching lost sections");
    const auto certificates = certify_relational_code_descents(s);
    check(certificates.size() == 2 && certificates[0].certified() && certificates[1].certified(),
          "rational/finite operation descent not certified");
    check(!certify_relational_code_descents(s, 0)[0].certified(), "zero-budget proof certified");
    auto bad = s;
    bad.sections[1].value = vector({4, 8, 9});
    check(!relational_code_glue(bad)[0].compatible, "incompatible local values glued");
    rejects([&] { (void)matched_relational_code_sections(bad); }, "incompatible matching published");
    auto e = endpoint();
    e.codes = bad;
    check(encode_temporal_endpoint(decode_temporal_endpoint(e.source, encode_temporal_endpoint(e))) ==
              encode_temporal_endpoint(e),
          "obstruction was repaired or rejected on disk");
    bad = s;
    bad.operations[3] =
        linear(4, 20, 20, 3, 3, {3, 0, 0, 0, 1, 0, 0, 0, 1}, std::vector<std::size_t>{0, 4, 8});
    check(certify_relational_code_descents(bad)[0].right == RelationalCodeCertification::Obstructed,
          "noncommuting rational square certified");
    bad = s;
    bad.operations[8] = finite(9, 41, 41, {0, 1, 2, 3, 4, 5});
    check(certify_relational_code_descents(bad)[1].right == RelationalCodeCertification::Obstructed,
          "noncommuting finite square certified");
    bad = s;
    bad.sections[1].field = 1002;
    rejects([&] { validate_relational_code_state(bad); }, "unrelated fields inferred equal from owner");
    bad = s;
    bad.sections[1].type = 10;
    rejects([&] { validate_relational_code_state(bad); }, "incorrect section carrier accepted");
    bad = s;
    bad.sections[0].cell = StableCellKey{core::Grade{0}, 999};
    e.codes = bad;
    rejects([&] { validate_temporal_endpoint(e); }, "dangling code owner accepted");
    // Independent declarations unite, but conflicts on shared IDs cannot.
    auto other = s;
    other.types.push_back({100, TypeKind::Finite, {20, 30}, {}});
    check(glue_relational_code_states(s, other).types.size() == 6, "schema union lost added type");
    other.types[0].atoms = {101, 103};
    rejects([&] { (void)glue_relational_code_states(s, other); }, "schema identity conflict accepted");
    other = s;
    other.namespace_key = 999;
    rejects([&] { (void)glue_relational_code_states(s, other); }, "namespace equality inferred from content");
    // Different local carriers can reside in separate declaration collections.
    auto a = s, b = s;
    a.interfaces.clear();
    a.descents.clear();
    b.interfaces.clear();
    b.descents.clear();
    a.sections = {s.sections[0], s.sections[2]};
    b.sections = {s.sections[1], s.sections[3]};
    const auto united = glue_relational_code_states(a, b, s.interfaces);
    check(relational_code_glue(united)[0].compatible && united.sections.size() == 4,
          "explicit cross-collection bridges failed");
}
void declaration_edges() {
    auto s = fixture();
    auto bad = s;
    bad.types[0].kind = static_cast<TypeKind>(255);
    rejects([&] { validate_relational_code_state(bad); }, "unknown type constructor accepted");
    bad = s;
    bad.operations[0].opcode = static_cast<Opcode>(255);
    rejects([&] { validate_relational_code_state(bad); }, "unknown operational constructor accepted");
    bad = s;
    bad.operations[0].word = {1};
    rejects([&] { validate_relational_code_state(bad); }, "unused operation lane changed identity");
    bad = s;
    bad.operations[6].word.pop_back();
    rejects([&] { validate_relational_code_state(bad); }, "partial finite map accepted");
    bad = s;
    bad.operations[6].word[0] = 42;
    rejects([&] { validate_relational_code_state(bad); }, "finite map leaves target domain");
    bad = s;
    bad.operations.push_back(word(99, 10, 30, {1, 3}));
    rejects([&] { validate_relational_code_state(bad); }, "ill-typed word accepted");
    bad = s;
    bad.types[0].atoms[1] = bad.types[0].atoms[0];
    rejects([&] { validate_relational_code_state(bad); }, "duplicate coordinate identity accepted");
    bad = s;
    bad.fields.push_back(bad.fields[0]);
    rejects([&] { validate_relational_code_state(bad); }, "duplicate field identity accepted");
    s.types.push_back({99, TypeKind::Rational, {}, {}});
    s.operations.push_back(identity(99, 99));
    s.operations.push_back(linear(100, 10, 99, 0, 2, {}));
    RelationalCodeAlgebra empty(s);
    check(std::get<0>(empty.apply(100, vector({1, 2})).data).values().size() == 0,
          "empty target carrier not supported");
    auto e = endpoint();
    LocalBlockDefinition local;
    local.block_key = 77;
    local.kind = LocalBlockKind::StrictSubcomplex;
    local.cells = {{core::Grade{0}, 10}};
    e.local_blocks = {local};
    e.codes->sections[0].block = 77;
    validate_temporal_endpoint(e);
    auto wrong = e;
    wrong.codes->sections[0].cell = StableCellKey{core::Grade{0}, 11};
    rejects([&] { validate_temporal_endpoint(wrong); }, "code owner outside declared block accepted");
    wrong = e;
    wrong.codes->sections[0].block = 78;
    rejects([&] { validate_temporal_endpoint(wrong); }, "unknown block binding accepted");
    // Same numeric operator, different retained support: universal agreement
    // must include support, rather than only tests on full-support states.
    s = fixture();
    s.operations[3] =
        linear(4, 20, 20, 3, 3, {2, 0, 0, 0, 1, 0, 0, 0, 1}, std::vector<std::size_t>{0, 1, 4, 8});
    check(certify_relational_code_descents(s)[0].right == RelationalCodeCertification::Obstructed,
          "support-only noncommuting square certified");
    // Nonadditive finite transport stays explicitly nonadditive.
    auto next = fixture();
    next.sections[2].value = Value{std::uint64_t{0}};
    const auto finite_change = relational_code_innovation(fixture(), next, 3, {{3, 8}});
    check(!finite_change.rational && std::get<1>(finite_change.propagated.data) == 0,
          "finite transport invented rational innovation");
}
void factorized_and_words() {
    auto s = fixture();
    Type product;
    product.key = 100;
    product.kind = TypeKind::Product;
    Value::Product values;
    for (std::uint64_t i = 0; i < 129; ++i) {
        product.factors.push_back({1000 + i, 40, static_cast<std::uint16_t>(i % 3)});
        values.push_back(Value{i % 2});
    }
    s.types.push_back(product);
    s.sections.push_back(section(5, 1001, 100, Value{values}));
    Op binary;
    binary.key = 100;
    binary.input = 100;
    binary.output = 40;
    binary.opcode = Opcode::Binary;
    binary.binary = composite::BinaryProgram(3);
    binary.binary->append({composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, 0});
    binary.binary->append({composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, 128});
    binary.binary->append({composite::BinaryOpcode::Xor, 2, 0, 1});
    binary.registers = {2};
    s.operations.push_back(binary);
    RelationalCodeAlgebra algebra(s);
    check(std::get<1>(algebra.apply(100, Value{values}).data) == 0, "wide-factor XOR");
    values[128] = Value{std::uint64_t{1}};
    check(std::get<1>(algebra.apply(100, Value{values}).data) == 1, "wide-factor update");
    auto e = endpoint();
    e.codes = s;
    const auto bytes = encode_temporal_endpoint(e);
    check(bytes.size() < 20000, "factorized representation unexpectedly expanded");
    check(encode_temporal_endpoint(decode_temporal_endpoint(e.source, bytes)) == bytes,
          "129 factor wire roundtrip");
    rejects(
        [&] {
            numeric::IndexDomain huge(std::vector<std::size_t>(129, 2));
            (void)huge;
        },
        "flat exponential domain should overflow");
    rejects([&] { (void)algebra.apply(100, Value{values}, 0); }, "zero execution budget ignored");
    auto bad = s;
    bad.types.back().factors[0].type = 100;
    rejects([&] { validate_relational_code_state(bad); }, "cyclic product type accepted");
    bad = s;
    bad.operations.back().binary->append({composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, 129});
    rejects([&] { validate_relational_code_state(bad); }, "binary aspect outside factor domain");
    bad = s;
    bad.operations.push_back(word(200, 10, 10, {200}));
    rejects([&] { validate_relational_code_state(bad); }, "cyclic operation word accepted");
    // A wide product is not exhaustively expanded to manufacture a proof.
    auto proof = s;
    proof.operations.push_back(identity(200, 100));
    proof.sections.push_back(section(6, 1001, 40, Value{std::uint64_t{0}}));
    proof.interfaces.push_back({3, 5, 6, 100, 6});
    proof.descents.push_back({3, 3, 200, 6, 6});
    check(certify_relational_code_descents(proof).back().left == RelationalCodeCertification::NotCertified,
          "exponential Boolean carrier incorrectly certified");
    // Typed factorwise nonlinear action and projection without Cartesian flattening.
    s.types.push_back({101, TypeKind::Product, {}, {{21, 40, 1}, {22, 41, 1}}});
    s.types.push_back({102, TypeKind::Product, {}, {{22, 41, 1}, {21, 40, 1}}});
    Op swap;
    swap.key = 101;
    swap.input = 101;
    swap.output = 102;
    swap.opcode = Opcode::FactorMap;
    swap.legs = {{22, 9}, {21, 8}};
    s.operations.push_back(swap);
    Op project;
    project.key = 102;
    project.input = 101;
    project.output = 40;
    project.opcode = Opcode::FactorProjection;
    project.legs = {{22, 7}};
    s.operations.push_back(project);
    RelationalCodeAlgebra factor_algebra(s);
    Value tuple{Value::Product{Value{std::uint64_t{1}}, Value{std::uint64_t{3}}}};
    const auto changed = factor_algebra.apply(101, tuple);
    check(std::get<1>(std::get<2>(changed.data)[0].data) == 4 &&
              std::get<1>(std::get<2>(changed.data)[1].data) == 0,
          "factor leg action/order");
    check(std::get<1>(factor_algebra.apply(102, tuple).data) == 1, "factor projection parity");
    check(relational_code_permutation_sign(s.types[s.types.size() - 2], s.types.back()) == -1,
          "Koszul sign lost");
    // Noncommuting word order is retained, not sorted as set-like metadata.
    s.operations.push_back(linear(300, 10, 10, 2, 2, {1, 1, 0, 1}, std::vector<std::size_t>{0, 1, 3}));
    s.operations.push_back(word(301, 10, 10, {3, 300}));
    s.operations.push_back(word(302, 10, 10, {300, 3}));
    RelationalCodeAlgebra words(s);
    check(!relational_code_detail::value_equal(words.apply(301, vector({2, 3})),
                                               words.apply(302, vector({2, 3}))),
          "noncommuting words collapsed");
    check(relational_code_operation_bytes(s, 301) != relational_code_operation_bytes(s, 302),
          "operation identity lost word order");
}
void support_and_random_linear() {
    auto s = fixture();
    s.operations.push_back(linear(100, 10, 10, 2, 2, {1, 0, 0, 0}, std::vector<std::size_t>{0, 3}));
    RelationalCodeAlgebra algebra(s);
    auto input = vector({2, 0});
    auto output = std::get<0>(algebra.apply(100, input).data);
    check(output.support().count() == 2 && output.values().nonzeros() == 1, "supported numerical zero lost");
    input = Value{tensor(numeric::IndexDomain({2}), {2, 0}, std::vector<std::size_t>{0})};
    output = std::get<0>(algebra.apply(100, input).data);
    check(output.support().count() == 1, "support-only change ignored");
    std::mt19937 gen(93841);
    for (unsigned trial = 0; trial < 160; ++trial) {
        const std::size_t n = gen() % 5 + 1, m = gen() % 5 + 1;
        RelationalCodeState model;
        model.namespace_key = 44;
        Type in;
        in.key = 1;
        Type out;
        out.key = 2;
        for (std::size_t i = 0; i < n; ++i)
            in.atoms.push_back(i + 1);
        for (std::size_t i = 0; i < m; ++i)
            out.atoms.push_back(i + 101);
        model.types = {in, out};
        std::vector<R> kernel(m * n), x(n), expected(m);
        std::vector<std::size_t> ks, xs;
        std::set<std::size_t> es;
        for (std::size_t i = 0; i < n; ++i)
            if (gen() % 3) {
                xs.push_back(i);
                x[i] = R(static_cast<int>(gen() % 9) - 4, gen() % 5 + 1);
            }
        for (std::size_t i = 0; i < m * n; ++i)
            if (gen() % 3) {
                ks.push_back(i);
                kernel[i] = R(static_cast<int>(gen() % 9) - 4, gen() % 5 + 1);
            }
        for (auto &q : kernel)
            q.canonicalize();
        for (auto &q : x)
            q.canonicalize();
        for (auto i : ks)
            for (auto j : xs)
                if (i % n == j) {
                    es.insert(i / n);
                    expected[i / n] += kernel[i] * x[j];
                }
        model.operations = {linear(1, 1, 2, m, n, kernel, ks)};
        RelationalCodeAlgebra engine(model);
        const auto result =
            std::get<0>(engine.apply(1, Value{tensor(numeric::IndexDomain({n}), x, xs)}).data);
        check(result.support().count() == es.size(), "independent Boolean-support product mismatch");
        for (std::size_t i = 0; i < m; ++i) {
            check(result.support().contains(i) == es.contains(i), "structural support mismatch");
            const auto state = result.state(i);
            if ((state ? *state : R{0}) != expected[i]) {
                std::cerr << "trial=" << trial << " row=" << i << " n=" << n << " m=" << m
                          << " actual=" << (state ? *state : R{0}) << " expected=" << expected[i] << "\n";
                for (const auto &q : kernel)
                    std::cerr << q << ",";
                std::cerr << " x=";
                for (const auto &q : x)
                    std::cerr << q << ",";
                std::cerr << "\n";
            }
            check((state ? *state : R{0}) == expected[i], "independent rational product mismatch");
        }
        temporal_codec_detail::Writer w;
        temporal_codec_detail::code_state(w, model);
        TemporalReadLimits limits;
        temporal_codec_detail::Reader reader(w.bytes(), limits);
        const auto read = temporal_codec_detail::code_state(reader);
        check(temporal_codec_detail::code_state_bytes(read) == w.bytes(), "random operation wire mismatch");
    }
}
void temporal_and_operations(const std::filesystem::path &dir) {
    auto base = endpoint(), target = base;
    target.sequence = 1;
    target.codes->operations.push_back(
        linear(900, 10, 20, 3, 2, {1, 0, 0, 1, 1, 1}, std::vector<std::size_t>{0, 3, 4, 5}));
    target.codes->sections[0].type = 20;
    target.codes->sections[0].value = vector({3, 7, 10});
    target.codes->interfaces[0].left_restriction = 2;
    target.codes->descents[0].left_operation = 4;
    rejects([&] { (void)make_temporal_segment(base, target); }, "unmapped dimension change accepted");
    const auto segment = make_temporal_segment(base, target, {}, {{1, 900}});
    const auto bytes = encode_temporal_segment(segment);
    const auto replayed = replay_temporal_segment(base, decode_temporal_segment(bytes));
    check(encode_temporal_endpoint(replayed) == encode_temporal_endpoint(target),
          "typed rectangular disk replay mismatch");
    const auto eta = relational_code_innovation(*base.codes, *target.codes, 1, {{1, 900}});
    check(eta.rational && eta.rational->nonzeros() == 0, "transport generated spurious innovation");
    auto changed = target;
    changed.codes->sections[0].value = vector({3, 7, 11});
    check(relational_code_innovation(*base.codes, *changed.codes, 1, {{1, 900}}).rational->nonzeros() == 1,
          "true innovation lost");
    auto bad = target;
    bad.codes->types[0].atoms[1] = 777;
    rejects([&] { (void)make_temporal_segment(base, bad, {}, {{1, 900}}); },
            "type identity redefinition accepted");
    // Operation change invisible on this field still survives the history.
    auto old = endpoint();
    old.codes->sections[0].value = vector({3, 0});
    old.codes->operations.push_back(linear(901, 10, 30, 1, 2, {1, 1}, std::vector<std::size_t>{0, 1}));
    auto next = old;
    next.sequence = 1;
    next.codes->operations.back().kernel = tensor(numeric::IndexDomain({1, 2}), {1, 9});
    RelationalCodeAlgebra oa(*old.codes), na(*next.codes);
    check(relational_code_detail::value_equal(oa.apply(901, old.codes->sections[0].value),
                                              na.apply(901, next.codes->sections[0].value)),
          "fixture does not mask operation column");
    check(relational_code_operation_bytes(*old.codes, 901) !=
              relational_code_operation_bytes(*next.codes, 901),
          "operation identity reduced to observed endpoint");
    const auto operation_segment = make_temporal_segment(old, next);
    check(operation_segment.codes.has_value(), "operation-only update omitted");
    check(encode_temporal_endpoint(replay_temporal_segment(
              old, decode_temporal_segment(encode_temporal_segment(operation_segment)))) ==
              encode_temporal_endpoint(next),
          "unused column not replayed");
    // Distinct transports with identical target states remain distinct segments.
    old = endpoint();
    old.codes->sections[0].value = vector({3, 0});
    old.codes->operations.push_back(linear(902, 10, 10, 2, 2, {1, 0, 0, 9}, std::vector<std::size_t>{0, 3}));
    next = old;
    next.sequence = 1;
    const auto s1 = make_temporal_segment(old, next, {}, {{1, 10}}),
               s2 = make_temporal_segment(old, next, {}, {{1, 902}});
    check(encode_temporal_segment(s1) != encode_temporal_segment(s2), "history operation difference lost");
    check(encode_temporal_endpoint(replay_temporal_segment(old, s1)) ==
              encode_temporal_endpoint(replay_temporal_segment(old, s2)),
          "valid same-endpoint histories differ");
    // Birth/removal of the entire optional channel, including empty fields.
    TemporalEndpoint plain(source());
    auto born = plain;
    born.sequence = 1;
    born.codes = fixture();
    born.codes->fields.push_back(8888);
    const auto birth = make_temporal_segment(plain, born);
    auto removed = born;
    removed.sequence = 2;
    removed.codes.reset();
    const auto removal = make_temporal_segment(born, removed);
    check(!replay_temporal_segment(replay_temporal_segment(plain, birth), removal).codes,
          "code-layer removal failed");
    // Persist real source/sidecar/frame fixtures for independent compiler comparison.
    write_rfcx(base.source, dir / "code_base.rfcx");
    write_bytes(dir / "code_base.rfcxe", encode_temporal_endpoint(base));
    write_bytes(dir / "code_next.rfcxe", encode_temporal_endpoint(target));
    write_bytes(dir / "code_delta.rfcxt", bytes);
    write_bytes(dir / "history_a.rfcxt", encode_temporal_segment(s1));
    write_bytes(dir / "history_b.rfcxt", encode_temporal_segment(s2));
    MappedRfcx mapped(dir / "code_base.rfcx");
    const auto disk_base = read_temporal_endpoint(mapped, dir / "code_base.rfcxe");
    check(encode_temporal_endpoint(
              replay_temporal_segment(disk_base, read_temporal_segment(dir / "code_delta.rfcxt"))) ==
              encode_temporal_endpoint(target),
          "disk-only code replay");
}
core::RelationalComplex wide_source(bool changed) {
    core::RelationSignature sig;
    std::vector<composite::AspectId> aspects;
    for (unsigned a = 0; a < 129; ++a)
        aspects.push_back(sig.aspects().add("factor_" + std::to_string(a)));
    const auto type = sig.add_type("retained", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(sig);
    builder.declare_witness_identity(10);
    builder.declare_witness_identity(11);
    for (unsigned edge = 0; edge < 65; ++edge) {
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for (unsigned side = 0; side < 2; ++side) {
            const unsigned slot = edge * 2 + side;
            std::vector<core::AspectValue> values;
            for (unsigned a = 0; a < 129; ++a) {
                bool bit = (slot + a) % 3 == 0;
                if (changed && a == 128 && (slot == 63 || slot == 64 || slot == 129))
                    bit = !bit;
                values.push_back({aspects[a], bit});
            }
            occurrences.push_back({10 + side, slot % 7 != 0, side == 0, slot % 5 == 0 ? R{0} : R{1},
                                   std::move(values), core::OccurrenceKey{1000 + slot}});
        }
        builder.add_grade_one(core::RelationKey{100 + edge}, type, occurrences);
    }
    return builder.freeze();
}
void native_aspect_views(const std::filesystem::path &dir) {
    TemporalEndpoint old(wide_source(false)), next(wide_source(true), 1);
    RelationalCodeState state;
    state.namespace_key = 98123;
    state.types = {{1, TypeKind::Finite, {0, 1}, {}}};
    state.fields = {1};
    std::vector<std::pair<std::uint64_t, composite::AspectId>> bindings;
    for (unsigned a = 0; a < 131; ++a)
        bindings.push_back({a + 1, static_cast<composite::AspectId>(a)});
    state.types.push_back(relational_aspect_code_type(old.source, 2, 1, bindings));
    for (unsigned slot = 0; slot < 130; ++slot)
        state.sections.push_back(relational_aspect_code_section(
            slot + 1, 1, 2, {core::Grade{1}, core::OccurrenceKey{1000 + slot}}));
    Op op;
    op.key = 1;
    op.input = 2;
    op.output = 1;
    op.opcode = Opcode::Binary;
    op.binary = composite::BinaryProgram(3);
    op.binary->append({composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, 0});
    op.binary->append({composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, 130});
    op.binary->append({composite::BinaryOpcode::Xor, 2, 0, 1});
    op.registers = {2};
    state.operations = {op};
    old.codes = state;
    next.codes = state;
    const auto segment = make_temporal_segment(old, next);
    check(!segment.codes.has_value(), "source-bound aspect changes duplicated as code replacements");
    const auto replayed =
        replay_temporal_segment(old, decode_temporal_segment(encode_temporal_segment(segment)));
    check(encode_temporal_endpoint(replayed) == encode_temporal_endpoint(next), "source-view replay");
    const auto fields = materialize_relational_code_views(replayed);
    RelationalCodeAlgebra algebra(fields);
    const auto packed = execute_relational_aspect_code(replayed, 1, core::Grade{1});
    check(packed.size() == 1 && packed[0].size() == 130, "packed query carrier mismatch");
    for (unsigned slot = 0; slot < 130; ++slot) {
        const auto &value = fields.sections[slot].value;
        for (unsigned a = 0; a < 131; ++a)
            check(std::get<1>(std::get<2>(value.data)[a].data) ==
                      replayed.source.grade(core::Grade{1})
                          .aspects.get(static_cast<composite::AspectId>(a), slot),
                  "retained aspect view bit mismatch");
        check(packed[0].get(slot) == static_cast<bool>(std::get<1>(algebra.apply(1, value).data)),
              "packed/scalar relational operation mismatch");
    }
    check(packed[0].word(2) < 4, "query tail padding retained");
    check(relational_code_section_bytes(old, 64) != relational_code_section_bytes(next, 64),
          "temporal aspect view observation stale");
    check(relational_code_section_bytes(old, 1) == relational_code_section_bytes(next, 1),
          "unaffected code observation changed");
    auto bad = old;
    bad.codes->sections[0].value = Value{std::uint64_t{1}};
    rejects([&] { validate_temporal_endpoint(bad); }, "source view accepted duplicated state");
    bad = old;
    bad.codes->types[1].factors[0].source_aspect = "undeclared";
    rejects([&] { validate_temporal_endpoint(bad); }, "undeclared source aspect binding accepted");
    rejects([&] { (void)relational_code_section_bytes(*old.codes, 1); },
            "unresolved source view hashed as sentinel");
    write_bytes(dir / "aspect_views.rfcxe", encode_temporal_endpoint(old));
    write_bytes(dir / "aspect_change.rfcxt", encode_temporal_segment(segment));
    check(encode_temporal_endpoint(decode_temporal_endpoint(
              old.source, bytes_at(dir / "aspect_views.rfcxe"))) == encode_temporal_endpoint(old),
          "source view wire identity");
    // Multi-output lowering must agree with full algebra evaluation, including
    // repeated output registers. Add this after frozen wire fixtures above.
    auto multi = next;
    multi.codes->types.push_back({3,TypeKind::Product,{},{{1,1,0,{}},{2,1,0,{}},{3,1,0,{}},{4,1,0,{}}}});
    multi.codes->operations[0].output=3;
    multi.codes->operations[0].registers={0,1,2,2};
    validate_temporal_endpoint(multi);
    const auto fused=execute_relational_aspect_code(multi,1,core::Grade{1});
    check(fused.size()==4,"multi-output source query arity");
    const auto materialized=materialize_relational_code_views(multi);
    RelationalCodeAlgebra multi_algebra(materialized);
    for(unsigned slot=0;slot<130;++slot) {
        const auto actual=multi_algebra.apply(1,materialized.sections[slot].value);
        const auto& product=std::get<2>(actual.data);
        for(std::size_t j=0;j<4;++j)
            check(fused[j].get(slot)==static_cast<bool>(std::get<1>(product[j].data)),"multi-output packed/scalar mismatch");
    }
    std::cout << "source_view_bits=" << 131 * 130 << " packed_words=" << packed[0].word_count() << "\n";
}
void observations_and_canonical() {
    auto s = fixture();
    auto duplicate = s.sections[0];
    duplicate.key = 100;
    s.sections.push_back(duplicate);
    const auto first = relational_code_section_bytes(s, 1), same = relational_code_section_bytes(s, 100),
               other = relational_code_section_bytes(s, 2);
    check(first == same && first != other, "typed observation equivalence");
    RelationalCodeObservationIndex index([](auto) { return 0ULL; });
    index.insert({s.namespace_key, 1}, first);
    index.insert({s.namespace_key, 100}, same);
    index.insert({s.namespace_key, 2}, other);
    check(index.find(first).size() == 2 && index.find(other).size() == 1,
          "fingerprint collision merged assertions");
    rejects([&] { index.insert({s.namespace_key, 1}, other); }, "duplicate retained identity inserted");
    s.namespace_key += 1;
    check(relational_code_section_bytes(s, 1) != first, "observation lost namespace");
    auto e = endpoint();
    const auto bytes = encode_temporal_endpoint(e);
    std::reverse(e.codes->types.begin(), e.codes->types.end());
    std::reverse(e.codes->operations.begin(), e.codes->operations.end());
    std::reverse(e.codes->sections.begin(), e.codes->sections.end());
    std::reverse(e.codes->interfaces.begin(), e.codes->interfaces.end());
    check(encode_temporal_endpoint(e) == bytes, "declaration record order changes canonical bytes");
    // Build the same sidecar with an externally reordered code type directory.
    using namespace temporal_codec_detail;
    Writer w;
    const auto image = encode_rfcx(e.source);
    w.raw(temporal_seed_detail::magic);
    w.u32(1);
    w.u32(relational_codes_flag);
    w.u64(0);
    w.u64(0);
    w.u64(e.sequence);
    w.u64(image.stats.payload_hash);
    w.u64(rfcx_detail::fnv1a64(image.bytes));
    w.u64(temporal_endpoint_hash(e));
    w.u64(0);
    w.u64(0);
    w.u64(0);
    auto canonical_codes = code_state_bytes(*e.codes);
    // Simpler direct canonical-order mutation: swap the two field IDs.
    Writer prefix;
    prefix.u64(e.codes->namespace_key);
    auto ordered_types = relational_code_detail::index(e.codes->types);
    prefix.u64(ordered_types.size());
    for (const auto &[id, t] : ordered_types) {
        (void)id;
        code_type(prefix, *t);
    }
    auto ordered_ops = relational_code_detail::index(e.codes->operations);
    prefix.u64(ordered_ops.size());
    for (const auto &[id, op] : ordered_ops) {
        (void)id;
        code_operation(prefix, *op);
    }
    const auto field_offset = prefix.size() + 8;
    for (unsigned j = 0; j < 8; ++j)
        std::swap(canonical_codes[field_offset + j], canonical_codes[field_offset + 8 + j]);
    w.raw(canonical_codes);
    w.patch_u64(16, w.size());
    w.patch_u64(24, rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));
    rejects([&] { (void)decode_temporal_endpoint(e.source, w.bytes()); },
            "noncanonical accepted by strict reader");
    TemporalReadLimits legacy;
    legacy.require_canonical = false;
    check(encode_temporal_endpoint(decode_temporal_endpoint(e.source, w.bytes(), legacy)) == bytes,
          "explicit permissive ingestion changed meaning");
    TemporalReadLimits tiny;
    tiny.max_records = 1;
    rejects([&] { (void)decode_temporal_endpoint(e.source, bytes, tiny); },
            "cumulative decode budget ignored");
}
void streaming_and_mutations(const std::filesystem::path &dir) {
    auto base = endpoint(), one = base;
    one.sequence = 1;
    one.codes->sections[0].value = vector({3, 9});
    auto two = one;
    two.sequence = 2;
    two.codes->sections[0].value = vector({3, 11});
    const auto a = encode_temporal_segment(make_temporal_segment(base, one));
    const auto b = encode_temporal_segment(make_temporal_segment(one, two));
    auto stream = a;
    stream.insert(stream.end(), b.begin(), b.end());
    for (auto width : {1u, 2u, 3u, 7u, 16u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 255u, 1024u, 8192u, 65536u}) {
        TemporalSegmentStreamDecoder decoder;
        auto current = base;
        std::vector<std::byte> emitted;
        for (std::size_t off = 0; off < stream.size(); off += width)
            decoder.feed(std::span<const std::byte>(stream).subspan(
                             off, std::min<std::size_t>(width, stream.size() - off)),
                         [&](const auto &s, auto raw) {
                             current = replay_temporal_segment(current, s);
                             emitted.insert(emitted.end(), raw.begin(), raw.end());
                         });
        decoder.finish();
        decoder.finish();
        check(decoder.frame_count() == 2 && decoder.peak_buffered_bytes() <= std::max(a.size(), b.size()),
              "stream decoder retained whole history");
        check(emitted == stream && encode_temporal_endpoint(current) == encode_temporal_endpoint(two),
              "chunking changed raw bytes/replay");
        rejects([&] { decoder.feed({}, [](const auto &, auto) {}); }, "feed after finish allowed");
    }
    // Every nonempty strict prefix is incomplete; an empty stream is valid.
    for (std::size_t n = 1; n < a.size(); ++n) {
        rejects(
            [&] {
                TemporalSegmentStreamDecoder d;
                d.feed(std::span<const std::byte>(a).first(n), [](const auto &, auto) {});
                d.finish();
            },
            "truncated frame accepted");
    }
    TemporalSegmentStreamDecoder failed;
    rejects([&] { failed.feed(a, [](const auto &, auto) { throw std::runtime_error("consumer failed"); }); },
            "callback exception swallowed");
    rejects([&] { failed.feed(a, [](const auto &, auto) {}); }, "failed stream reused");
    TemporalSegmentStreamDecoder reentered;
    rejects(
        [&] {
            reentered.feed(a, [&](const auto &, auto) { reentered.feed({}, [](const auto &, auto) {}); });
        },
        "callback reentered stream");
    TemporalSegmentStreamDecoder chain;
    chain.feed(a, [](const auto &, auto) {});
    rejects([&] { chain.feed(a, [](const auto &, auto) {}); }, "duplicate frame chain accepted");
    TemporalReadLimits limited;
    limited.max_records = 0;
    rejects(
        [&] {
            TemporalSegmentStreamDecoder d(limited);
            d.feed(a, [](const auto &, auto) {});
        },
        "frame budget ignored");
    auto corrupt = a;
    patch_u64(corrupt, 16, std::numeric_limits<std::uint64_t>::max());
    rejects(
        [&] {
            TemporalSegmentStreamDecoder d;
            d.feed(corrupt, [](const auto &, auto) {});
        },
        "oversized stream allocation allowed");
    // Rehashed mutations may be valid distinct histories. Every accepted parse
    // must be canonical; every accepted replay must reconstitute full endpoint.
    std::mt19937 gen(44311);
    std::size_t parsed = 0, replayed = 0;
    for (unsigned trial = 0; trial < 500; ++trial) {
        auto changed = a;
        const auto at = 32 + gen() % (changed.size() - 32);
        changed[at] ^= static_cast<std::byte>(1u << (gen() % 8));
        rehash(changed);
        std::optional<TemporalSegment> accepted;
        try {
            accepted = decode_temporal_segment(changed);
        } catch (const std::exception &) {
        }
        if (accepted) {
            ++parsed;
            check(encode_temporal_segment(*accepted) == changed, "accepted mutation not canonical");
            std::optional<TemporalEndpoint> replay;
            try {
                replay = replay_temporal_segment(base, *accepted);
            } catch (const std::exception &) {
            }
            if (replay) {
                ++replayed;
                check(encode_temporal_endpoint(*replay) == encode_temporal_endpoint(one),
                      "mutated history changed complete endpoint");
            }
        }
    }
    std::cout << "mutations=500 parsed=" << parsed << " replayed=" << replayed << '\n';
    write_bytes(dir / "code_stream.rfcxt", stream);
    std::size_t frames = 0;
    visit_temporal_segments(dir / "code_stream.rfcxt", [&](const auto &, auto) { ++frames; });
    check(frames == 2, "file streaming visitor");
}
} // namespace
int main(int argc, char **argv) {
    try {
        const auto dir = argc > 1 ? std::filesystem::path(argv[1])
                                  : std::filesystem::temp_directory_path() / "rfc_relational_code_tests";
        std::filesystem::create_directories(dir);
        const auto run = [](const char *name, auto action) {
            std::cout << "BEGIN " << name << "\n";
            action();
            std::cout << "PASS " << name << " checks=" << checks << "\n";
        };
        run("binary", binary_boundary);
        run("gluing/descent", gluing_and_descent);
        run("declaration edges", declaration_edges);
        run("factorization/words", factorized_and_words);
        run("support/random linear", support_and_random_linear);
        run("temporal operations", [&] { temporal_and_operations(dir); });
        run("native aspect views", [&] { native_aspect_views(dir); });
        run("observations/canonical", observations_and_canonical);
        run("streaming/mutations", [&] { streaming_and_mutations(dir); });
        std::cout << "relational_code_checks=" << checks << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
