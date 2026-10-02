#pragma once

#include "rfc/storage/native_execution_binding.hpp"

namespace rfc::storage {
struct NativeFieldDependencyLimits {
    std::size_t descriptor_bytes{64u * 1024u * 1024u};
};
struct NativeFieldReadScope {
    std::size_t selected_carriers{}, downward_maps{}, upper_field_members{};
    bool complete_source{true}, complete_declarations{true}, negative_membership{true};
};
namespace native_field_dependency_detail {
using Bytes = std::vector<std::byte>;
using Writer = temporal_codec_detail::Writer;
inline void metadata(Writer& w, const TemporalTensor& t) {
    using namespace temporal_codec_detail;
    w.u64(t.key); w.u8(static_cast<std::uint8_t>(t.role)); w.u8(static_cast<std::uint8_t>(t.owner.kind));
    w.u64(t.owner.block); cell(w,t.owner.cell); occurrence(w,t.owner.occurrence);
    optional_key(w,t.input); optional_key(w,t.output); string(w,t.interpretation);
    domain(w,t.tensor.domain()); w.u64(t.tensor.support().count());
    t.tensor.support().for_each_active([&](auto i) { w.u64(i); });
}
} // namespace native_field_dependency_detail

// Exact in-process witnesses for the named rational final-field action. They
// retain authentic authority/context and are not persisted mathematical identities.
class NativeFieldDependencyReceipt {
public:
    static NativeFieldDependencyReceipt inspect(const NativeFieldPlan& plan,
        const PinnedNativeAccession& publication, NativeFieldDependencyLimits limits = {}) {
        using namespace temporal_codec_detail;
        using native_field_dependency_detail::metadata;
        temporal_source_detail::require(plan.endpoint() == publication.endpoint() &&
            plan.source_namespace() == publication.source_namespace(),"field receipt belongs to another publication");
        Writer w; NativeFieldReadScope scope;
        const auto check = [&] { native_accession_detail::budget(w.size(),limits.descriptor_bytes,"field dependency descriptor budget"); };
        // Versioned ephemeral contract: rational complete lower field; no metric.
        w.u64(0x314e414c5044464eULL); w.u16(plan.upper_grade().value);
        w.u64(plan.source_namespace()); w.u8(static_cast<std::uint8_t>(plan.realization().domain));
        w.u8(static_cast<std::uint8_t>(plan.realization().maps)); w.u64(plan.realization().common_coordinates);
        const auto& e = *plan.endpoint(); const auto source = encode_rfcx(e.source);
        w.u64(source.bytes.size()); w.raw(source.bytes); check();
        const auto carriers = temporal_endpoint_detail::carriers(e);
        auto selected = plan.selected_carriers(); std::sort(selected.begin(),selected.end());
        w.u64(selected.size()); scope.selected_carriers = selected.size();
        for (auto id : selected) { carrier(w,*carriers.at(id)); check(); }
        const auto tensors = temporal_endpoint_detail::tensors(e);
        for (const auto& [key,t] : tensors) {
            (void)key;
            if (t->role == TemporalTensorRole::CoefficientIndependentMap || t->role == TemporalTensorRole::RealizedIncidenceMap)
                ++scope.downward_maps;
            if (t->role == TemporalTensorRole::Field && t->owner.kind == TemporalOwnerKind::Cell &&
                t->owner.cell.grade == plan.upper_grade()) ++scope.upper_field_members;
        }
        // Complete membership includes absence, inactive maps and future additions.
        w.u64(scope.downward_maps);
        for (const auto& [key,t] : tensors) {
            (void)key;
            if (t->role == TemporalTensorRole::CoefficientIndependentMap || t->role == TemporalTensorRole::RealizedIncidenceMap) {
                tensor_record(w,*t); check();
            }
        }
        w.u64(scope.upper_field_members);
        for (const auto& [key,t] : tensors) {
            (void)key;
            if (t->role == TemporalTensorRole::Field && t->owner.kind == TemporalOwnerKind::Cell &&
                t->owner.cell.grade == plan.upper_grade()) { metadata(w,*t); check(); }
        }
        w.u64(plan.inputs().size());
        for (const auto& input : plan.inputs()) { metadata(w,*tensors.at(input.field)); check(); }
        w.u8(e.declarations.has_value()); if (e.declarations) declaration_state(w,*e.declarations); check();
        auto plan_bytes = w.bytes(); Writer result;
        result.u64(0x31544c535244464eULL); result.u64(plan_bytes.size()); result.raw(plan_bytes);
        result.u64(plan.inputs().size());
        for (const auto& input : plan.inputs()) {
            tensor_record(result,*tensors.at(input.field));
            native_accession_detail::budget(machine::checked_size_add(plan_bytes.size(),result.size()),
                limits.descriptor_bytes,"field dependency aggregate byte budget");
        }
        native_accession_detail::budget(machine::checked_size_add(plan_bytes.size(),result.size()),
            limits.descriptor_bytes,"field dependency aggregate byte budget");
        return {publication,std::move(plan_bytes),result.bytes(),scope};
    }
    const auto& plan_bytes() const & noexcept { return plan_; }
    const auto& result_bytes() const & noexcept { return result_; }
    const auto& scope() const & noexcept { return scope_; }
    const std::vector<std::byte>& plan_bytes() const && = delete;
    const std::vector<std::byte>& result_bytes() const && = delete;
    const NativeFieldReadScope& scope() const && = delete;
    std::uint64_t plan_fingerprint() const noexcept { return plan_hash_; }
    std::uint64_t result_fingerprint() const noexcept { return result_hash_; }
    bool same_authority(const NativeFieldDependencyReceipt& other) const noexcept {
        return publication_.same_execution_authority(other.publication_);
    }
    bool same_plan(const NativeFieldDependencyReceipt& other) const {
        return this == &other || (same_authority(other) && plan_ == other.plan_);
    }
    bool same_result(const NativeFieldDependencyReceipt& other) const {
        return this == &other || (same_authority(other) && result_ == other.result_);
    }
private:
    NativeFieldDependencyReceipt(PinnedNativeAccession p, std::vector<std::byte> plan,
        std::vector<std::byte> result, NativeFieldReadScope scope)
        : publication_(std::move(p)),plan_(std::move(plan)),result_(std::move(result)),scope_(scope),
          plan_hash_(rfcx_detail::fnv1a64(plan_)),result_hash_(rfcx_detail::fnv1a64(result_)) {}
    PinnedNativeAccession publication_;
    std::vector<std::byte> plan_, result_;
    NativeFieldReadScope scope_;
    std::uint64_t plan_hash_{}, result_hash_{};
};
} // namespace rfc::storage
