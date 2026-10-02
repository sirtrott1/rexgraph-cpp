#pragma once

#include "rfc/database/native_execution.hpp"
#include "rfc/storage/native_field_dependencies.hpp"

namespace rfc::database {
struct NativeFieldLayoutFacts {
    std::size_t source_coordinates{}, target_coordinates{}, retained_occurrences{}, active_occurrences{};
    std::size_t field_support{}, map_support{}, stored_field_values{}, stored_map_values{};
    std::size_t scalar_payload_bits{}, scalar_materialized_bytes{}, output_cpp_object_bytes{};
    bool variable_output_encoding{true};
};

// Owning action-first inspection. Counts preserve full native coordinates and
// distinguish encoded scalar payloads, C++ objects and unmeasured GMP heap use.
class NativeFieldAnalysis {
    struct State {
        storage::NativeFieldPlan plan;
        NativeExecutionReport report;
        storage::NativeFieldDependencyReceipt receipt;
        NativeFieldLayoutFacts facts;
        State(storage::NativeFieldPlan p, NativeExecutionReport r, storage::NativeFieldDependencyReceipt d,
              NativeFieldLayoutFacts f) : plan(std::move(p)),report(std::move(r)),receipt(std::move(d)),facts(f) {}
    };
public:
    static NativeFieldAnalysis inspect(const storage::PinnedNativeAccession& publication,
        const storage::NativeFieldRequest& request, const storage::NativeFieldRealization& realization = {},
        const runtime::ExecutionHostPolicy& host = {}, storage::NativeFieldDependencyLimits limits = {}) {
        auto inspection = native_field_detail::inspect_field_action(publication,request,realization,host);
        auto receipt = storage::NativeFieldDependencyReceipt::inspect(inspection.plan,publication,limits);
        const auto& plan = inspection.plan;
        NativeFieldLayoutFacts f;
        f.source_coordinates = plan.source_dimension(); f.target_coordinates = plan.target_dimension();
        f.retained_occurrences = plan.occurrences().size();
        for (const auto& occurrence : plan.occurrences()) if (occurrence.active) ++f.active_occurrences;
        f.output_cpp_object_bytes = machine::checked_size_mul(f.target_coordinates,sizeof(numeric::Rational));
        // A named diagnostic rational payload layout, not a source declaration.
        const composite::DeclarationId diagnostic{0x3144464e,1,1};
        composite::DeclarationLimits encoding;
        encoding.max_integer_bits = host.limits.integer_bits;
        encoding.max_encoded_bytes = limits.descriptor_bytes;
        encoding.max_materialized_bytes = limits.descriptor_bytes;
        const composite::CompiledLayout scalar(composite::DeclarationRegistry({composite::Declaration::rational(diagnostic)},encoding),
            diagnostic,composite::BooleanLayoutProfile::Lsb0OctetsV1,encoding);
        const auto measure = [&](const numeric::SparseRationalTensor& tensor) {
            for (const auto& value : tensor.values().entries()) {
                const auto allocation = scalar.allocation(composite::DeclaredValue::rational(value.value));
                f.scalar_payload_bits = machine::checked_size_add(f.scalar_payload_bits,allocation.bits);
                f.scalar_materialized_bytes = machine::checked_size_add(f.scalar_materialized_bytes,allocation.materialized_bytes);
            }
        };
        const auto tensors = storage::temporal_endpoint_detail::tensors(*plan.endpoint());
        for (const auto& input : plan.inputs()) {
            const auto& tensor = tensors.at(input.field)->tensor;
            f.field_support = machine::checked_size_add(f.field_support,tensor.support().count());
            f.stored_field_values = machine::checked_size_add(f.stored_field_values,tensor.values().nonzeros()); measure(tensor);
        }
        for (const auto& [key,t] : tensors) {
            (void)key;
            if (t->role == storage::TemporalTensorRole::CoefficientIndependentMap || t->role == storage::TemporalTensorRole::RealizedIncidenceMap) {
                f.map_support = machine::checked_size_add(f.map_support,t->tensor.support().count());
                f.stored_map_values = machine::checked_size_add(f.stored_map_values,t->tensor.values().nonzeros()); measure(t->tensor);
            }
        }
        return NativeFieldAnalysis(std::make_shared<const State>(std::move(inspection.plan),std::move(inspection.report),std::move(receipt),f));
    }
    const storage::NativeFieldPlan& binding() const & { return state().plan; }
    const NativeExecutionReport& report() const & { return state().report; }
    const storage::NativeFieldDependencyReceipt& receipt() const & { return state().receipt; }
    const NativeFieldLayoutFacts& layout() const & { return state().facts; }
    const storage::NativeFieldPlan& binding() const && = delete;
    const NativeExecutionReport& report() const && = delete;
    const storage::NativeFieldDependencyReceipt& receipt() const && = delete;
    const NativeFieldLayoutFacts& layout() const && = delete;
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return state().plan.endpoint(); }
private:
    explicit NativeFieldAnalysis(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    const State& state() const {
        if (!state_) throw std::invalid_argument("moved-from native field analysis");
        return *state_;
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::database
