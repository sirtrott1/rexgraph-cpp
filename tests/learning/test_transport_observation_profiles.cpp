#include "../../examples/sparse_transport_data.hpp"
#include "../storage/compact_tensor_fixture.hpp"
#include "rfc/learning/transport_evidence.hpp"
#include <iostream>

namespace {
using namespace sparse_transport_example;
std::size_t checks{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F&& operation, const char* message) {
    bool rejected = false;
    try { operation(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}
numeric::SparseRationalTensor tensor(const V& values) {
    std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] != 0) entries.push_back({i, values[i]});
    return {numeric::IndexDomain({values.size()}), numeric::StructuralSupport::full(values.size()),
        S(values.size(), std::move(entries))};
}
void observation_profiles() {
    const auto admitted = limits(3);
    Guard preparation(admitted);
    const auto owner = model(3, preparation);
    const learning::PreparedTransportEvidence root(owner, preparation);
    const V input{1, 0, 0}, parameters{Q{3,5}, Q{4,5}, Q{-4,5}, Q{3,5}, 0, 0};
    auto target = numeric::checked_apply(owner.observation_map(input, true, preparation), parameters, preparation);
    const auto fixed = owner.fixed_response(input, preparation).boundary;
    for (std::size_t i = 0; i < target.size(); ++i) target[i] += fixed[i];
    const learning::TransportObservation dense{input, target, {1, 1}};
    learning::SparseTransportObservation sparse{S::basis(3, 0), {}};
    for (std::size_t cell = 0; cell < 2; ++cell)
        for (std::size_t axis = 0; axis < 3; ++axis)
            sparse.targets.push_back({cell, axis, target[cell * 3 + axis]});
    M projection(6, 12);
    const auto& arena = owner.owner().fibers().complex().grade(core::Grade{1});
    for (std::size_t occurrence = 0; occurrence < arena.incidence_count(); ++occurrence)
        for (std::size_t axis = 0; axis < 3; ++axis)
            projection.set(arena.lower_index[occurrence] * 3 + axis, occurrence * 3 + axis, 1);
    const learning::TransportMappedObservation mapped{input, target, projection, {0,1,2,3,4,5}};
    const learning::TransportColumnObservation columns{owner.owner(), core::Grade{1},
        {{0, tensor(input)}}, {{0, tensor(V(target.begin(), target.begin() + 3))},
                              {1, tensor(V(target.begin() + 3, target.end()))}}};
    Guard dg(admitted), sg(admitted), mg(admitted), cg(admitted);
    const auto d = root.append({dense}, dg);
    const auto s = root.append_sparse({sparse}, sg);
    const auto m = root.append_fields({mapped}, mg);
    const auto c = root.append_columns({columns}, cg);
    for (const auto* result : {&s, &m, &c}) {
        check(result->system().design == d.system().design, "observation profile changed original equations");
        check(result->particular() == d.particular() && result->kernel() == d.kernel(),
            "observation profile lost complete affine freedom");
        check(result->free_columns() == d.free_columns(), "observation profile changed parameter identities");
    }
    check(m.system().constraints.front().domain == learning::TransportObservationDomain::OriginalOccurrenceImage,
        "mapped observation lost its original occurrence domain");
    check(s.sparse_observations().size() == 1 && root.observation_count() == 0,
        "sparse append mutated its parent or lost its history");
    check(c.column_observation_count() == 1 && c.materialized_field_entries() == 0,
        "column observations were eagerly materialized");
    check(c.observations().front().target == dense.target, "column field materialization changed values");
    rejects([&] { (void)s.observations(); }, "sparse history became an empty dense history");
    rejects([&] { (void)s.original_observations(); }, "sparse history disappeared from the original-history interface");
    rejects([&] { Guard g(admitted); (void)s.observed_stalk(0, 1, g); },
        "selected sparse coordinates became whole-stalk coverage");
    rejects([&] { Guard g(admitted); (void)s.append_fields({mapped}, g); },
        "sparse and mapped histories were implicitly converted");
    rejects([&] { Guard g(admitted); (void)m.append_sparse({sparse}, g); },
        "mapped and sparse histories were implicitly converted");
    Guard mix(admitted);
    const auto mixed = d.append_columns({columns}, mix).append_fields({mapped}, mix);
    check(mixed.observation_count() == 3 && mixed.kernel() == d.kernel(),
        "existing dense/column/mapped mixed history regressed");
}
void compact_streaming() {
    using namespace compact_fixture;
    for (const auto profile : {composite::BooleanLayoutProfile::Lsb0OctetsV1,
                               composite::BooleanLayoutProfile::Lsb0OctetsV2}) {
        for (const auto& value : {compact_fixture::tensor(), compact_fixture::tensor(3, false), nested()}) {
            const auto root_id = std::holds_alternative<DeclaredValue::Product>(value.data) ? id(6) : id(2);
            const composite::CompiledLayout layout(registry(), root_id, profile);
            const auto expected = storage::encode_declared_bits(layout, value);
            std::vector<std::byte> streamed;
            std::size_t bits{};
            storage::encode_declared_bits_to(layout, value, [&](std::size_t count, std::size_t bytes) {
                bits = count;
                streamed.resize(bytes);
                return std::span<std::byte>(streamed);
            });
            check(streamed == expected.bytes && bits == expected.bit_count,
                "streaming and owning codecs disagree under an explicit wire profile");
            const auto restored = storage::decode_declared_bits(layout, expected);
            check(composite::declared_value_equal(value, restored), "compact streaming lost structural support");
        }
    }
}
}
int main() {
    try {
        observation_profiles(); compact_streaming();
        std::cout << checks << " observation-profile and streaming checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
