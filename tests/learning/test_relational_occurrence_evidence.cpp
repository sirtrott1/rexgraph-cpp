#include "rfc/core/builder.hpp"
#include "rfc/learning/relational_available_evidence_codec.hpp"
#include "rfc/learning/relational_field_family_codec.hpp"
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
using G = numeric::ExactActionGuard;
using Program = OwnedRelationalFieldProgram;
using Evidence = PreparedRelationalFieldEvidence;
using Port = OwnedRelationalOccurrenceTargetPort;
using Available = PreparedAvailableRelationalEvidence;
using Bytes = std::vector<std::byte>;
using Origin = OwnedRelationalValueOrigin;
using Grant = RelationalOriginGrant;
using Context = RelationalAvailabilityContext;
using Role = RelationalDependencyRole;
namespace {
std::size_t checks{};
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
template <class F> void rejects(F action, const char *why) {
    bool caught{};
    try {
        action();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, why);
}
template <class F> void rejects_origin(F action, const char *why) {
    bool caught{};
    try {
        action();
    } catch (const std::invalid_argument &e) {
        const std::string message = e.what();
        caught =
            message.find("origin") != std::string::npos || message.find("availability") != std::string::npos;
        if (!caught)
            std::cerr << "unexpected refusal: " << message << '\n';
    }
    check(caught, why);
}
numeric::SparseRationalTensor tensor(const V &v) {
    G g;
    return field_interface_detail::tensor(v, g);
}
Bytes text(std::string_view s) {
    Bytes out;
    for (const auto c : s)
        out.push_back(static_cast<std::byte>(c));
    return out;
}
M product(const M &a, const M &b) {
    G g;
    return numeric::checked_multiply(a, b, g);
}
V act(const M &a, const V &x) {
    G g;
    return numeric::checked_apply(a, x, g);
}
Program program() {
    core::RelationSignature signature;
    const auto t = signature.add_type("retained", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, t,
                          {{10, true, false, 1},
                           {10, true, false, 1},
                           {20, true, false, 1},
                           {20, false, false, 1},
                           {10, true, false, 0}});
    const storage::NativeTemporalSeries history(storage::TemporalEndpoint(builder.freeze(), 7), 991);
    RelationalFieldProgramDeclaration d;
    d.key = 51;
    d.revision = 1;
    d.differential = true;
    d.stalks = {{{{0}, 0}, 1001, {101, 102}, M::from_rows({{2, 1}, {1, 3}})},
                {{{0}, 1}, 1002, {201, 202, 203}, M::identity(3)},
                {{{1}, 0}, 1003, {301, 302}, M::identity(2)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{{1}, 0, phi, M::identity(2)},
                     {{1}, 1, phi, M::identity(2).scale(-1)},
                     {{1}, 2, phi, M::from_rows({{1, 2}, {0, 1}, {-1, 1}})},
                     {{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {{1}, 4, phi, M::identity(2)}};
    d.nodes = {{1, FieldProgramNodeKind::Input, {1}, {}, {}},
               {2, FieldProgramNodeKind::Boundary, {0}, 1, {}}};
    return {history.tail(), d};
}
OwnedRelationalInputUseAsset asset(const Program &p, std::uint64_t ns = 992) {
    const auto publication = p.publication();
    const auto &c = publication.endpoint().source;
    const storage::NativeTemporalSeries h(storage::TemporalEndpoint(c, 7), ns);
    RelationalInputUseDeclaration d;
    d.key = 600;
    d.channel = 700;
    d.encoding = "UTF-8";
    d.bytes = text("one");
    d.uses = {
        {111, {storage::stable_cell_key(c, {{1}, 0}), storage::stable_cell_key(c, {{0}, 0})}, {}, {{0, 3}}},
        {112, {storage::stable_cell_key(c, {{1}, 0})}, {}, {{0, 3}}}};
    return {h, d};
}
RelationalInputSubmission submission(const Program &p, const OwnedRelationalInputUseAsset &a, const V &x,
                                     std::uint64_t use = 111) {
    RelationalInputAccessionDeclaration d;
    d.key = 901;
    d.node = 1;
    d.source_grade = {1};
    d.isometric = true;
    d.source_stalks = {p.declaration().stalks.back()};
    d.instances.cells = {{0, 1}, {0}};
    d.instances.occurrences = {{}, {0, 1, 2, 3, 4}};
    d.accession = M::identity(2);
    d.reconstruction = M::identity(2);
    return {a, d, use, tensor(x)};
}
OwnedAccessionFieldResult supplied(const Program &p, const OwnedRelationalInputUseAsset &a, const V &x) {
    const auto spec = submission(p, a, x);
    const OwnedRelationalInputAccession input(a, p, spec.declaration);
    return execute_relational_inputs(p, {input.admit(spec.use, spec.source_field)});
}
RelationalFieldParameterDeclaration parameters() {
    return {100, 1, 2, {{0, 0, 0}, {1, 0, 0}, {3, 0, 0}}, M::identity(3), {10001, 10002, 10003}};
}
RelationalOccurrenceTargetDeclaration port_declaration(const Program &p, bool rectangle = true) {
    const auto publication = p.publication();
    const auto &c = publication.endpoint().source;
    RelationalOccurrenceTargetDeclaration d;
    d.key = 11001;
    d.revision = 1;
    d.node = 2;
    d.frame = 11002;
    d.coordinates = {{11101, {{1}, c.occurrence_identity({1}, 0)}, 1001, 101},
                     {11102, {{1}, c.occurrence_identity({1}, 1)}, 1001, 101}};
    d.accession = rectangle ? M::from_rows({{1, 0}, {0, 1}, {1, 1}}) : M::identity(2);
    d.reconstruction = rectangle ? M::from_rows({{1, 0, 0}, {0, 1, 0}}) : M::identity(2);
    return d;
}
RelationalFieldTrainingObservation measured(const OwnedAccessionFieldResult &input,
                                            const OwnedRelationalInputUseAsset &a, const Port &p,
                                            std::uint64_t key, V target = {3, -3, 0},
                                            std::vector<std::uint64_t> mask = {11101}) {
    return {key, input, a, 111, tensor(target), {}, p, std::move(mask)};
}
RelationalFieldTrainingObservation lower(const OwnedAccessionFieldResult &input,
                                         const OwnedRelationalInputUseAsset &a, std::uint64_t key = 20001) {
    return {key, input, a, 111, tensor({0, 0, 0, 0, 0}), {{{0}, 0}}};
}
void replace_u64(Bytes &bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255u);
}
Bytes forged_mask(const Bytes &image, const Evidence &root, bool available) {
    using namespace relational_affine_codec_detail;
    const storage::TemporalReadLimits read_policy;
    Reader r(image, read_policy);
    header(r, image,
           available ? available_evidence_codec_detail::occurrence_evidence_magic
                     : occurrence_evidence_magic);
    if (available) {
        (void)r.blob();
        (void)relational_affine_codec_detail::parameters(r);
        for (int i = 0; i < 4; ++i)
            (void)r.u64();
        (void)r.u64();
        (void)r.blob();
    } else {
        for (int i = 0; i < 4; ++i)
            (void)r.u64();
        (void)r.blob();
        (void)relational_affine_codec_detail::parameters(r);
        witness(r, root, true);
    }
    check(r.count(8) == 1 && r.count(8) == 1, "single original observation codec fixture");
    (void)r.u64();
    (void)r.blob();
    (void)r.blob();
    (void)r.u64();
    (void)storage::temporal_codec_detail::tensor(r);
    check(r.count(6) == 0 && r.flag(), "typed occurrence mask is distinct from lower-cell mask");
    (void)occurrence_port(r);
    check(r.count(8) == 1, "one named original coordinate");
    auto changed = image;
    replace_u64(changed, image.size() - r.remaining(), 11102);
    replace_u64(changed, 24, storage::rfcx_detail::fnv1a64(std::span<const std::byte>(changed).subspan(32)));
    return changed;
}
void native() {
    const auto p = program();
    const auto a = asset(p);
    const auto input = supplied(p, a, {1, 0});
    const Port port(p, port_declaration(p));
    const Evidence root(p, parameters());
    check(port.selection().rows() == 2 && port.selection().cols() == 12,
          "complete heterogeneous repeated/inactive/zero original flag layout");
    check(port.coordinate_metric() == M::from_rows({{2, 0}, {0, 2}}) &&
              port.represented_metric().rows() == 3 && port.represented_metric().get(2, 2) == 0,
          "actual per-flag stalk metric and singular represented metric");
    check(product(port.declaration().reconstruction, port.declaration().accession) == M::identity(2),
          "faithful rectangular selected target carrier");
    const auto first = root.append({measured(input, a, port, 20002)});
    const auto response = first.execute(input);
    check(first.kernel().cols() == 2 && first.observation_system().constraints[0].domain ==
                                            TransportObservationDomain::OriginalOccurrenceImage,
          "partial original coordinate keeps other active and inactive map directions");
    const auto &f = response.occurrence_field(2);
    check(product(port.action(), f.directions).nonzeros() != 0,
          "complete correlated target direction retained");
    const auto residual = first.residual(0);
    check(residual.target_port()->same_owner(port) &&
              residual.field().particular.domain().logical_size() == 3,
          "residual retains actual target image/port and complete native response");
    const auto next = first.append({lower(input, a)});
    const auto out = next.execute(input);
    const auto theta = next.particular();
    check(theta == V({2, -2, 0}) && next.kernel().cols() == 1,
          "flag and lower evidence combine in original common parameter carrier");
    check(product(port.action(), out.occurrence_field(2).directions).nonzeros() == 0,
          "target identified while inactive original map remains unknown");
    const auto inactive = next.coefficient_independent_map({1}, 3);
    check(inactive.directions().size() == 1 && inactive.directions()[0].get(0, 0) == 1,
          "unobserved inactive transport state survives target identification");
    const auto parent = next.parent();
    check(parent && parent->same_owner(first) && next.parent_relation().has_value(),
          "actual immutable parent and exact correlated embedding retained");
    const auto reverse = root.append({lower(input, a)}).append({measured(input, a, port, 20002)});
    check(reverse.particular() == next.particular() && reverse.kernel() == next.kernel(),
          "mixed append order retains exact feasible original family");
    const auto bulk = root.append({measured(input, a, port, 20002), lower(input, a)});
    check(bulk.particular() == next.particular() && bulk.kernel() == next.kernel(),
          "mixed suffix uses same native incremental evidence path");
    const auto selected = out.evaluate({5});
    const auto actual = selected.result();
    const auto fields = actual.result();
    check(fields.field(1).domain().logical_size() == 2 &&
              fields.field(2).values().dense() == V({0, 0, 1, 0, -1}) &&
              fields.occurrence_field(2).domain().logical_size() == 12,
          "whole fields and original flag state independently inspectable after fit");
    check(act(port.action(), fields.occurrence_field(2).values().dense()) == V({3, -3, 0}),
          "complete admitted held target image");
    const auto bytes = encode_relational_field_evidence(next);
    check(bytes[5] == std::byte{2}, "new/mixed history uses explicit v2 envelope");
    const auto cold = decode_relational_field_evidence(bytes, {});
    check(encode_relational_field_evidence(cold) == bytes && cold.observations()[0].port.has_value() &&
              cold.observations()[1].port == std::nullopt,
          "mixed history canonical cold bytes and typed masks retained");
    const auto cold_program = cold.program();
    const auto &o = cold.observations()[0];
    const auto target_program = o.port->program();
    const auto supplied_result = o.input.result();
    const auto supplied_program = supplied_result.program();
    check(cold_program.fibers().same_owner(target_program.fibers()) &&
              cold_program.fibers().same_owner(supplied_program.fibers()),
          "cold port/input/evidence adopt one actual serialized parent");
    const auto pb = encode_relational_occurrence_target_port(port);
    check(encode_relational_occurrence_target_port(decode_relational_occurrence_target_port(pb, {})) == pb,
          "standalone owning port canonical bytes");
    rejects(
        [&] {
            (void)decode_relational_field_evidence(
                forged_mask(encode_relational_field_evidence(first), root, false), {});
        },
        "valid-envelope forged new mask fails original family witness");
    auto unsupported = bytes;
    unsupported[5] = std::byte{3};
    rejects([&] { (void)decode_relational_field_evidence(unsupported, {}); },
            "unsupported observation wire version refuses");
    auto truncated = bytes;
    truncated.pop_back();
    rejects([&] { (void)decode_relational_field_evidence(truncated, {}); }, "truncated new history refuses");
    rejects([&] { (void)PreparedRelationalFieldFamily({51001, 1, {}}, {next, first}); },
            "existing family zero-origin scope is not silently relaxed by target observations");
    const auto family = encode_relational_affine_result(out);
    check(encode_relational_affine_result(decode_relational_affine_result(family, {})) == family,
          "full new-kind affine result canonical replay");
    const auto completion = encode_relational_affine_evaluation(selected);
    check(encode_relational_affine_evaluation(decode_relational_affine_evaluation(completion, {})) ==
              completion,
          "native correlated completion retains new-kind observation lineage");
    Bytes streamed;
    stream_relational_field_evidence(next, [&](std::span<const std::byte> chunk) {
        streamed.insert(streamed.end(), chunk.begin(), chunk.end());
    });
    check(streamed == bytes, "streamed new-kind bytes equal canonical encoding");
    const auto plain = root.append({lower(input, a)});
    check(encode_relational_field_evidence(plain)[5] == std::byte{1},
          "unchanged whole-lower history remains v1");
    rejects([&] { (void)root.append({measured(input, a, port, 20003, {0, 0, 1})}); },
            "partial represented-row zero completion is not a complete admitted target");
    rejects([&] { (void)root.append({measured(input, a, port, 20003, {1, 0, 1}, {11103})}); },
            "represented row identity is not an original coordinate mask");
    const auto partial_complete = root.append({measured(input, a, port, 20003, {1, 7, 8})});
    check(partial_complete.kernel().cols() == 2, "complete admitted target plus partial original mask does "
                                                 "not constrain unobserved nonzero coordinate");
    rejects([&] { (void)first.append({measured(input, a, port, 20004, {4, -3, 1})}); },
            "inconsistent direct flag label refuses without altering parent");
    rejects([&] { (void)root.append({measured(input, a, port, 20003, {3, -3, 0}, {})}); },
            "missing target produces no mathematical append");
    rejects([&] { (void)root.append({measured(input, a, port, 20003, {3, -3, 0}, {11101, 11101})}); },
            "duplicate original target mask refuses");
    auto d = port_declaration(p);
    d.coordinates[0].basis += 1;
    rejects([&] { (void)Port(p, d); }, "equal dimensional foreign stalk basis refuses");
    d = port_declaration(p);
    d.coordinates[0].direction = 201;
    rejects([&] { (void)Port(p, d); }, "foreign stalk direction refuses");
    d = port_declaration(p);
    d.coordinates[0].occurrence.key.value = 999;
    rejects([&] { (void)Port(p, d); }, "absent original flag refuses");
    d = port_declaration(p);
    d.coordinates[1] = d.coordinates[0];
    d.coordinates[1].key += 1;
    rejects([&] { (void)Port(p, d); }, "duplicate original flag coordinate refuses");
    d = port_declaration(p);
    d.reconstruction = M(2, 3);
    rejects([&] { (void)Port(p, d); }, "nonfaithful reconstruction refuses");
    const Program equal(p.publication(), p.declaration());
    const Port foreign(equal, port_declaration(equal));
    rejects([&] { (void)root.append({measured(input, a, foreign, 20003)}); },
            "same-byte foreign actual program owner refuses");
    auto policy = numeric::ExactActionLimits{};
    policy.entries = 1;
    rejects([&] { (void)Port(p, port_declaration(p), policy); }, "port full resource admission");
    G guard;
    const auto transported = PreparedTransportObservations::at_grade(
        p.fibers(), {1},
        {{0, 0, 0}, {0, 1, 1}, {1, 0, 0}, {1, 1, 1}, {2, 0, 0}, {2, 0, 1}, {2, 1, 1}, {2, 2, 0}, {2, 2, 1}},
        {{0, 0, 1}}, guard);
    PreparedTransportEvidence mapped(transported, guard);
    M projection(1, 12);
    projection.set(0, 0, 1);
    projection.set(0, 2, 1);
    const auto fitted =
        mapped.append_fields({TransportMappedObservation{{1, 1}, {1}, projection, {77}}}, guard);
    check(fitted.original_observations().size() == 1 && fitted.system().constraints[0].cell == 77,
          "low-level explicit linear projection preserves original row attribution");
    rejects([&] { (void)fitted.observations(); },
            "mapped history cannot silently flatten to lower observations");
    rejects([&] { (void)fitted.observed_stalk(0, 0, guard); },
            "mapped row cannot masquerade as observed lower stalk");
    check(fitted.input_stalk(0, 0, guard) == V({1, 1}) &&
              fitted.active_input_stalks(0, guard) == std::vector<std::size_t>{0},
          "input metadata inspection preserves complete mapped input history");
    const auto escaped = [] {
        const auto p0 = program();
        const auto a0 = asset(p0);
        const Port port0(p0, port_declaration(p0));
        const auto x0 = supplied(p0, a0, {1, 0});
        return Evidence(p0, parameters()).append({measured(x0, a0, port0, 25001)}).residual(0);
    }();
    const auto escaped_response = escaped.response();
    check(escaped.target_port().has_value() &&
              escaped_response.occurrence_field(2).particular.domain().logical_size() == 12,
          "escaped residual owns original port, model, input and complete field family");
    std::vector<std::future<Bytes>> jobs;
    for (int i = 0; i < 3; ++i)
        jobs.push_back(
            std::async(std::launch::async, [next] { return encode_relational_field_evidence(next); }));
    for (auto &job : jobs)
        check(job.get() == bytes, "three joined immutable mixed-history readers");
}
storage::NativeSeriesPrefix publication(const OwnedRelationalInputUseAsset &a, bool late = false) {
    auto h = a.history();
    if (late) {
        const auto tail = h.tail();
        auto e = tail.endpoint();
        ++e.sequence;
        h.append_target(e);
    }
    return storage::NativeSeriesPrefix::capture(h, h.last_sequence());
}
void grant(std::vector<Grant> &grants, std::uint64_t key, Role role, const Bytes &payload,
           storage::NativeSeriesPrefix prefix) {
    const runtime::WorkAvailability availability{prefix.source_namespace(), prefix.cutoff()};
    grants.push_back({Origin(key, prefix, role, payload), availability});
}
Context withheld(const std::vector<Grant> &grants, std::uint64_t key) {
    auto values = grants;
    bool found{};
    for (auto &g : values)
        if (g.origin.key() == key) {
            g.availability.max_sequence = g.origin.publication().cutoff() - 1;
            found = true;
        }
    check(found, "withheld actual grant exists");
    return Context(values);
}
void available() {
    const auto p = program();
    const auto a = asset(p);
    const auto pb = encode_relational_field_program(p);
    const auto ab = encode_relational_input_use_asset(a);
    std::vector<Grant> grants;
    grant(grants, 1, Role::Program, pb, publication(a));
    grant(grants, 2, Role::Maps, pb, publication(a));
    grant(grants, 3, Role::Metrics, pb, publication(a));
    const PreparedAvailableRelationalProgram prepared(p, {1, 2, 3}, Context(grants));
    const auto params = parameters();
    grant(grants, 4, Role::Evidence, relational_evidence_root_payload(pb, params, {}), publication(a));
    const Available root(prepared, params, 4, Context(grants));
    const auto spec = submission(p, a, {1, 0});
    const auto vp = relational_input_value_payload(ab, spec.declaration, spec.use, spec.source_field);
    const auto ip = relational_input_accession_payload(pb, ab, spec.declaration, spec.use, spec.source_field);
    grant(grants, 10, Role::InputAsset, ab, publication(a));
    grant(grants, 11, Role::InputValue, vp, publication(a));
    grant(grants, 12, Role::InputAccession, ip, publication(a));
    grant(grants, 13, Role::Reconstruction, ip, publication(a));
    const auto admitted = prepared.admit_input(spec, {10, 11, 12, 13}, Context(grants));
    const auto input = prepared.execute({admitted}, Context(grants));
    const Port port(p, port_declaration(p));
    const auto target = tensor({3, -3, 0});
    grant(grants, 20, Role::TargetAsset, ab, publication(a));
    grant(
        grants, 21, Role::TargetValue,
        relational_occurrence_target_value_payload(pb, port.declaration(), ab, 111, target, {11101},
                                                   available_evidence_detail::target_inputs(input.result())),
        publication(a, true));
    grant(grants, 22, Role::Interface, encode_relational_occurrence_target_port(port), publication(a, true));
    const RelationalAvailableTrainingObservation observation{30001, input,        a,    111,    target,
                                                             {},    {20, 21, 22}, port, {11101}};
    const auto child = root.append({observation}, Context(grants));
    const auto bytes = encode_available_relational_evidence(child);
    const auto child_native = child.evidence();
    check(bytes[5] == std::byte{2} && child_native.kernel().cols() == 2,
          "available original occurrence observation fitted natively");
    const auto dependencies = child.observation_dependencies(0, Context(grants));
    check(dependencies.size() == 10 && dependencies.back().key() == 22 &&
              dependencies.back().role() == Role::Interface,
          "native kind-aware complete observation dependencies, no lower-only count formula");
    rejects([&] { (void)child.observation_dependencies(0, Context(grants), 9); },
            "native dependency cap refuses without clipping");
    rejects_origin(
        [&] {
            (void)decode_available_relational_evidence(forged_mask(bytes, root.evidence(), true),
                                                       Context(grants), {});
        },
        "valid-envelope changed target mask denied by actual target origin before fit");
    const auto cold = decode_available_relational_evidence(bytes, Context(grants), {});
    check(encode_available_relational_evidence(cold) == bytes &&
              cold.observation_dependencies(0, Context(grants)).size() == 10,
          "new available history canonical cold bytes and dependencies");
    const auto immutable = std::make_shared<const Bytes>(bytes);
    const storage::TemporalReadLimits read_policy;
    const auto segmented =
        storage::ByteRange::indexed(immutable, {{0, 5}, {5, 1}, {6, bytes.size() - 6}}, {0, 1, 2},
                                    bytes.size(), bytes.size() - 1, 64 * bytes.size(), 3);
    const auto parsed = available_evidence_codec_detail::preflight_evidence_image(segmented, Context(grants),
                                                                                  read_policy, {});
    const auto segmented_cold =
        available_evidence_codec_detail::replay_evidence(parsed, Context(grants), read_policy);
    check(encode_available_relational_evidence(segmented_cold) == bytes,
          "segmented new history admits without materializing full outer image");
    const RelationalAvailableObservationSubmission raw{
        30001, {{spec, {10, 11, 12, 13}}}, a, 111, target, {}, {20, 21, 22}, port, {11101}};
    check(encode_available_relational_evidence(root.append_inputs({raw}, Context(grants))) == bytes,
          "raw preflight and admitted input append agree exactly");
    for (const auto key : {1u, 2u, 3u, 4u, 10u, 11u, 12u, 13u, 20u, 21u, 22u}) {
        const auto unavailable = withheld(grants, key);
        rejects_origin([&] { child.admit(unavailable); },
                       "every original root/input/target/port grant rechecked");
        rejects_origin([&] { (void)child.observation_dependencies(0, unavailable); },
                       "dependency inspection rechecks complete actual authority");
        rejects_origin([&] { (void)decode_available_relational_evidence(bytes, unavailable, {}); },
                       "cold replay rechecks complete actual authority before fit");
        rejects_origin([&] { (void)child.execute(input, unavailable); },
                       "execution cannot use revoked original evidence");
    }
    auto changed = observation;
    changed.key += 1;
    changed.observed_coordinates = {11102};
    rejects([&] { (void)root.append({changed}, Context(grants)); },
            "target value proof binds exact original coordinate mask");
    auto moved = port.declaration();
    moved.frame += 1;
    changed = observation;
    changed.port = Port(p, moved);
    rejects([&] { (void)root.append({changed}, Context(grants)); },
            "target proof binds actual port/frame/metric declaration");
    auto other = submission(p, a, {1, 0}, 112);
    const auto other_value =
        relational_input_value_payload(ab, other.declaration, other.use, other.source_field);
    const auto other_accession =
        relational_input_accession_payload(pb, ab, other.declaration, other.use, other.source_field);
    grant(grants, 30, Role::InputAsset, ab, publication(a));
    grant(grants, 31, Role::InputValue, other_value, publication(a));
    grant(grants, 32, Role::InputAccession, other_accession, publication(a));
    grant(grants, 33, Role::Reconstruction, other_accession, publication(a));
    changed = observation;
    changed.input =
        prepared.execute({prepared.admit_input(other, {30, 31, 32, 33}, Context(grants))}, Context(grants));
    rejects([&] { (void)root.append({changed}, Context(grants)); },
            "equal numerical input with different original use cannot reuse target proof");
    auto raw_other = raw;
    raw_other.inputs = {{other, {30, 31, 32, 33}}};
    rejects([&] { (void)root.append_inputs({raw_other}, Context(grants)); },
            "raw target/input binding checked before accession actions");
    auto bad_raw = raw;
    bad_raw.inputs[0].input.declaration.reconstruction = M(2, 2);
    rejects([&] { (void)root.append_inputs({bad_raw}, withheld(grants, 22)); },
            "missing port grant rejects before invalid raw accession could execute");
    auto alternate = port.declaration();
    alternate.frame += 10;
    const Port other_port(p, alternate);
    grant(grants, 52, Role::Interface, encode_relational_occurrence_target_port(other_port),
          publication(a, true));
    const auto new_target = tensor({4, -3, 1});
    grant(
        grants, 41, Role::TargetValue,
        relational_occurrence_target_value_payload(pb, port.declaration(), ab, 111, new_target, {11101},
                                                   available_evidence_detail::target_inputs(input.result())),
        publication(a));
    grant(
        grants, 51, Role::TargetValue,
        relational_occurrence_target_value_payload(pb, other_port.declaration(), ab, 111, target, {11101},
                                                   available_evidence_detail::target_inputs(input.result())),
        publication(a));
    const RelationalAvailableTrainingObservation inconsistent{30002, input,        a,    111,    new_target,
                                                              {},    {20, 41, 22}, port, {11101}};
    const RelationalAvailableTrainingObservation unavailable_suffix{
        30003, input, a, 111, target, {}, {20, 51, 52}, other_port, {11101}};
    rejects_origin([&] { (void)child.append({inconsistent, unavailable_suffix}, withheld(grants, 52)); },
                   "entire new-kind suffix admitted before inconsistent first observation can fit");
    auto broken = spec;
    broken.declaration.reconstruction = M(2, 2);
    const auto broken_accession =
        relational_input_accession_payload(pb, ab, broken.declaration, broken.use, broken.source_field);
    grant(grants, 70, Role::InputAccession, broken_accession, publication(a));
    grant(grants, 71, Role::Reconstruction, broken_accession, publication(a));
    const std::vector<RelationalAvailableInputSubmission> broken_inputs{{broken, {10, 11, 70, 71}}};
    grant(grants, 60, Role::TargetValue,
          relational_occurrence_target_value_payload(
              pb, port.declaration(), ab, 111, target, {11101},
              available_evidence_detail::target_inputs(pb, broken_inputs)),
          publication(a));
    const RelationalAvailableObservationSubmission broken_first{30004, broken_inputs, a,    111,    target,
                                                                {},    {20, 60, 22},  port, {11101}};
    const RelationalAvailableObservationSubmission unavailable_second{
        30005, {{spec, {10, 11, 12, 13}}}, a, 111, target, {}, {20, 51, 52}, other_port, {11101}};
    rejects_origin(
        [&] { (void)root.append_inputs({broken_first, unavailable_second}, withheld(grants, 52)); },
        "entire raw suffix target/port checks precede first invalid A/R action");
    const auto lower_target = tensor({0, 0, 0, 0, 0});
    grant(grants, 81, Role::TargetValue, relational_target_value_payload(pb, 2, ab, 111, lower_target),
          publication(a));
    const RelationalAvailableTrainingObservation legacy{30006,        input,      a,       111,
                                                        lower_target, {{{0}, 0}}, {20, 81}};
    const auto mixed = child.append({legacy}, Context(grants));
    const auto mixed_bytes = encode_available_relational_evidence(mixed);
    const auto mixed_cold = decode_available_relational_evidence(mixed_bytes, Context(grants), {});
    check(encode_available_relational_evidence(mixed_cold) == mixed_bytes &&
              mixed_cold.observation_dependencies(0, Context(grants)).size() == 10 &&
              mixed_cold.observation_dependencies(1, Context(grants)).size() == 9,
          "mixed native dependency traversal retains each original kind and complete parent history");
    check(encode_available_relational_evidence(child) == bytes,
          "failed changes preserve admitted history and original model");
    const auto dep0 = cold.observation_dependencies(0, Context(grants));
    check(dep0.back().same_owner(dependencies.back()), "cold context retains actual external origin owner");
}
void files(const std::filesystem::path &directory, bool cold) {
    std::filesystem::create_directories(directory);
    const auto p = program();
    const auto a = asset(p);
    const Port port(p, port_declaration(p));
    const auto input = supplied(p, a, {1, 0});
    const Evidence root(p, parameters());
    const auto e = root.append({lower(input, a)}).append({measured(input, a, port, 40001)});
    const auto result = e.execute(input);
    const auto evaluation = result.evaluate({3});
    const auto packet = [&](std::string_view name, const Bytes &bytes) {
        const auto path = directory / name;
        if (cold) {
            std::ifstream f(path, std::ios::binary);
            check(bool(f), "cold packet exists");
            const std::vector<char> chars{std::istreambuf_iterator<char>(f), {}};
            Bytes actual;
            for (auto c : chars)
                actual.push_back(static_cast<std::byte>(c));
            check(actual == bytes, "fresh-process canonical packet bytes");
        } else {
            std::ofstream f(path, std::ios::binary);
            f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            check(bool(f), "write canonical packet");
        }
    };
    packet("port.packet", encode_relational_occurrence_target_port(port));
    packet("mixed-evidence.packet", encode_relational_field_evidence(e));
    packet("result.packet", encode_relational_affine_result(result));
    packet("evaluation.packet", encode_relational_affine_evaluation(evaluation));
}
} // namespace
int main(int argc, char **argv) {
    try {
        native();
        available();
        if (argc == 3)
            files(argv[2], std::string_view(argv[1]) == "--read");
        else if (argc != 1)
            throw std::runtime_error("expected --write/--read directory");
        std::cout << checks << " occurrence evidence checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
