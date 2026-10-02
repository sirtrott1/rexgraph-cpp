#include "exact_allocation_hooks.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/database/native_execution.hpp"
#include <chrono>
#include <iostream>

namespace {
using namespace rfc;
using namespace rfc::storage;
using Clock = std::chrono::steady_clock;
struct Sample {
    Counts allocations;
    long long nanoseconds;
};
void start() {
    counts = {};
    accounting = true;
}
Sample stop(Clock::time_point begin) {
    const auto end = Clock::now();
    accounting = false;
    return {counts, std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
}
TemporalEndpoint endpoint(std::size_t arity, bool rectangular, bool wide) {
    core::RelationSignature signature;
    const auto type = signature.add_type("retained-profile", core::BoundaryPolicy::DeclaredExact, 0, 100000);
    core::RelationalComplexBuilder builder(signature);
    std::vector<core::BoundaryOccurrenceInput> legs;
    for (std::size_t i = 0; i < arity; ++i)
        legs.push_back({10 + (rectangular ? i : 0),
                        i % 7 != 6,
                        i % 3 == 1,
                        numeric::Rational{1, 3},
                        {},
                        core::OccurrenceKey{1000 + i}});
    builder.add_grade_one(core::RelationKey{100}, type, legs);
    TemporalEndpoint e(builder.freeze());
    const auto lower_count = rectangular ? arity : 1;
    for (std::size_t i = 0; i < lower_count; ++i)
        e.carriers.push_back({10000 + i, TemporalCarrierKind::CellStalk, core::Grade{0},
                              StableCellKey{core::Grade{0}, 10 + i}, rectangular ? 8000 + i : 900,
                              numeric::IndexDomain::linear(rectangular ? 3 : 1)});
    e.carriers.push_back({20000, TemporalCarrierKind::CellStalk, core::Grade{1},
                          StableCellKey{core::Grade{1}, 100}, 900,
                          numeric::IndexDomain::linear(rectangular ? 2 : 1)});
    TemporalTensor field;
    field.key = 200;
    field.role = TemporalTensorRole::Field;
    field.output = 20000;
    field.owner.kind = TemporalOwnerKind::Cell;
    field.owner.cell = {core::Grade{1}, 100};
    field.interpretation = "exact prepared profiling field";
    numeric::Rational x{7, 13};
    if (wide) {
        numeric::Integer n = 1;
        mpz_mul_2exp(n.get_mpz_t(), n.get_mpz_t(), 521);
        n += 17;
        x = numeric::Rational(n, numeric::Integer{13});
    }
    const auto dimension = rectangular ? 2u : 1u;
    std::vector<numeric::SparseRationalEntry> values{{0, x}};
    if (rectangular)
        values.push_back({1, numeric::Rational{-11, 17}});
    field.tensor = {numeric::IndexDomain::linear(dimension), numeric::StructuralSupport::full(dimension),
                    numeric::SparseRationalVector(dimension, values)};
    e.tensors.push_back(field);
    if (rectangular)
        for (std::size_t i = 0; i < arity; ++i) {
            numeric::ExactMatrix matrix(3, 2);
            matrix.set(0, 0, numeric::Rational{2, 7});
            matrix.set(1, 1, numeric::Rational{-3, 11});
            matrix.set(2, 0, numeric::Rational{5, 19});
            matrix.set(2, 1, numeric::Rational{1, 23});
            TemporalTensor map;
            map.key = 30000 + i;
            map.role = TemporalTensorRole::CoefficientIndependentMap;
            map.owner.kind = TemporalOwnerKind::Occurrence;
            map.owner.occurrence = {core::Grade{1}, core::OccurrenceKey{1000 + i}};
            map.input = 20000;
            map.output = 10000 + i;
            map.interpretation = "explicit rectangular profile map";
            map.tensor = temporal_matrix_tensor(matrix);
            e.tensors.push_back(map);
        }
    return e;
}
void print(const char *workload, std::size_t arity, const char *phase, const std::vector<Sample> &samples,
           const local::PreparedGradeFieldAction::Statistics &stats) {
    std::cout << "{\"workload\":\"" << workload << "\",\"arity\":" << arity << ",\"phase\":\"" << phase
              << "\",\"samples\":[";
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (i)
            std::cout << ',';
        const auto &s = samples[i];
        const auto &c = s.allocations;
        std::cout << "{\"nanoseconds\":" << s.nanoseconds << ",\"cpp_allocation_calls\":" << c.cpp_calls
                  << ",\"cpp_requested_bytes\":" << c.cpp_bytes << ",\"gmp_allocation_calls\":" << c.gmp_calls
                  << ",\"gmp_requested_bytes\":" << c.gmp_bytes
                  << ",\"gmp_reallocation_calls\":" << c.gmp_reallocations
                  << ",\"gmp_reallocation_requested_bytes\":" << c.gmp_reallocation_bytes << '}';
    }
    std::cout << "],\"retained_occurrences\":" << stats.retained_occurrences
              << ",\"active_occurrences\":" << stats.active_occurrences
              << ",\"prepared_entries\":" << stats.entries
              << ",\"preparation_map_materializations\":" << stats.map_materializations
              << ",\"preparation_coefficient_copies\":" << stats.coefficient_copies
              << ",\"compiled_apply_map_materializations\":0,\"compiled_apply_source_key_lookups\":0"
              << ",\"compiled_apply_map_coefficient_copies\":0,\"compiled_multiply_add_pairs\":"
              << stats.entries << ",\"scratch_rational_slots\":" << stats.scratch_slots
              << ",\"scratch_object_bytes\":" << stats.scratch_slots * sizeof(numeric::Rational)
              << ",\"output_object_bytes\":" << stats.target_dimension * sizeof(numeric::Rational) << "}\n";
}
void run(const char *workload, std::size_t arity, bool rectangular, bool wide) {
    const auto e = endpoint(arity, rectangular, wide);
    start();
    const auto pin_start = Clock::now();
    NativeAccessionDirectory directory(e, 77);
    NativeAccessionRequest field;
    field.kind = NativeAccessionKind::Field;
    field.subject = 200;
    const auto pin = directory.pin(field);
    NativeFieldRequest request;
    request.fields = {pin};
    for (const auto &carrier : e.carriers)
        request.stalk_carriers.push_back(carrier.key);
    const auto pins = stop(pin_start);
    const auto realization =
        rectangular ? NativeFieldRealization{} : NativeFieldRealization::constant_fiber(900);
    std::vector<Sample> preparation;
    for (std::size_t i = 0; i < 7; ++i) {
        start();
        const auto begin = Clock::now();
        const auto op = database::prepare_field_action(pin, request, realization);
        preparation.push_back(stop(begin)); // Stop accounting before sample storage.
    }
    const auto op = database::prepare_field_action(pin, request, realization);
    const auto &stats = op.kernel().statistics();
    print(workload, arity, "publication-and-pins", {pins}, stats);
    print(workload, arity, "prepare", preparation, stats);
    start();
    const auto scratch_start = Clock::now();
    auto workspace = database::make_workspace(op);
    const auto scratch = stop(scratch_start);
    print(workload, arity, "workspace", {scratch}, stats);
    start();
    const auto reference_start = Clock::now();
    const auto first_reference = database::execute(op);
    const auto reference_first = stop(reference_start);
    print(workload, arity, "reference-first", {reference_first}, stats);
    const auto &expected = first_reference.values();
    start();
    const auto compiled_start = Clock::now();
    const auto first = database::execute_prepared(op, workspace);
    const auto compiled_first = stop(compiled_start);
    print(workload, arity, "compiled-first", {compiled_first}, stats);
    if (first.values() != expected)
        throw std::runtime_error("profile reference mismatch");
    std::vector<Sample> references, compiled;
    for (std::size_t i = 0; i < 31; ++i) {
        // Alternate measurement order to reduce systematic ordering bias.
        const auto reference = [&]() {
            start();
            const auto begin = Clock::now();
            const auto result = database::execute(op);
            const auto sample = stop(begin);
            if (result.values() != expected)
                throw std::runtime_error("profile reference changed");
            references.push_back(sample);
        };
        const auto prepared = [&]() {
            start();
            const auto begin = Clock::now();
            const auto result = database::execute_prepared(op, workspace);
            const auto sample = stop(begin);
            if (result.values() != expected)
                throw std::runtime_error("profile compiled changed");
            compiled.push_back(sample);
        };
        if (i % 2 == 0) {
            reference();
            prepared();
        } else {
            prepared();
            reference();
        }
    }
    print(workload, arity, "reference-warm", references, stats);
    print(workload, arity, "compiled-warm", compiled, stats);
}
} // namespace
int main() {
    mp_set_memory_functions(gmp_allocate, gmp_reallocate, gmp_free);
    for (const auto arity : {16u, 256u, 4096u}) {
        run("shared-scalar", arity, false, false);
        run("shared-wide-521-bit", arity, false, true);
    }
    for (const auto arity : {16u, 128u, 512u})
        run("distinct-rectangular-3x2", arity, true, false);
}
