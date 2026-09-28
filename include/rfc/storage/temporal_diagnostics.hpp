#pragma once

#include "rfc/storage/temporal_segments.hpp"
#include "rfc/local/fiber_system.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/local/cellular_sheaf.hpp"
#include <memory>

namespace rfc::storage {

// A supplemental magnitude retains a declared q even while e=0. The Phase-02
// source snapshot itself stores realized coefficients, so it cannot recover a
// latent magnitude already discarded before this endpoint was declared.
inline composite::CompositeCoefficient temporal_occurrence_coefficient_state(
    const TemporalEndpoint& endpoint, StableOccurrenceKey occurrence) {
    const auto slot = resolve_stable_occurrence(endpoint.source, occurrence);
    temporal_source_detail::require(slot.has_value(), "composite state occurrence absent");
    const auto& arena = endpoint.source.grade(occurrence.upper_grade);
    const auto c = endpoint.source.incidence_coefficient(occurrence.upper_grade, *slot);
    numeric::Rational q = c < 0 ? -c : c;
    bool declared = false;
    for (const auto& t : endpoint.tensors) {
        if (t.role != TemporalTensorRole::IncidenceMagnitude || t.owner.occurrence != occurrence) continue;
        temporal_source_detail::require(!declared && t.tensor.domain().scalar() && t.tensor.state(0).has_value(), "invalid/duplicate latent magnitude");
        q = *t.tensor.state(0); declared = true;
    }
    return {arena.aspects.get(endpoint.source.signature().existence_aspect(), *slot),
            arena.aspects.get(endpoint.source.signature().orientation_aspect(), *slot), q};
}
inline temporal::CompositeOccurrenceChange temporal_occurrence_composite_change(
    const TemporalEndpoint& old, const TemporalEndpoint& next, StableOccurrenceKey occurrence) {
    return temporal::composite_coefficient_change(temporal_occurrence_coefficient_state(old, occurrence),
                                                  temporal_occurrence_coefficient_state(next, occurrence));
}

// These diagnostics consume reconstructed endpoints. They are not substitutes
// for the reconstructive records and do not declare a physical dynamics model.
inline numeric::SparseRationalVector temporal_field_innovation(
    const TemporalEndpoint& old, const TemporalEndpoint& next, std::uint64_t tensor_key,
    const std::vector<TemporalCorrespondence>& maps = {}) {
    using namespace temporal_endpoint_detail;
    const auto os = tensors(old), ns = tensors(next);
    require(ns.contains(tensor_key) && ns.at(tensor_key)->role == TemporalTensorRole::Field, "target is not a field");
    const auto& t = *ns.at(tensor_key);
    const auto J = temporal_carrier_correspondence(old, next, *t.output, maps);
    numeric::SparseRationalVector propagated(J.rows());
    if (os.contains(tensor_key)) {
        require(os.at(tensor_key)->role == TemporalTensorRole::Field && os.at(tensor_key)->output == t.output, "field comparison carrier mismatch");
        propagated = sparse_apply(J, os.at(tensor_key)->tensor.values());
    }
    return sparse_sum(t.tensor.values(), propagated, -1);
}
inline numeric::ExactMatrix temporal_tensor_map_defect(
    const TemporalEndpoint& old, const TemporalEndpoint& next, std::uint64_t tensor_key,
    const std::vector<TemporalCorrespondence>& maps = {}, bool realize_incidence = false) {
    using namespace temporal_endpoint_detail;
    const auto os = tensors(old), ns = tensors(next);
    require(os.contains(tensor_key) && ns.contains(tensor_key), "map defect requires persistent payload");
    const auto& a = *os.at(tensor_key); const auto& b = *ns.at(tensor_key);
    require(a.input && a.output && a.input == b.input && a.output == b.output, "map defect requires persistent input/output carrier identities");
    require(a.role != TemporalTensorRole::Metric && a.role != TemporalTensorRole::CrossForm &&
            b.role != TemporalTensorRole::Metric && b.role != TemporalTensorRole::CrossForm, "bilinear forms require pullback comparison");
    const auto Jin = temporal_carrier_correspondence(old, next, *b.input, maps);
    const auto Jout = temporal_carrier_correspondence(old, next, *b.output, maps);
    const auto A = realize_incidence ? temporal_realized_incidence(old, a) : temporal_tensor_matrix(a);
    const auto B = realize_incidence ? temporal_realized_incidence(next, b) : temporal_tensor_matrix(b);
    return B.multiply(Jin).add(Jout.multiply(A).scale(numeric::Rational{-1}));
}
inline numeric::ExactMatrix temporal_form_pullback_change(
    const TemporalEndpoint& old, const TemporalEndpoint& next, std::uint64_t tensor_key,
    const std::vector<TemporalCorrespondence>& maps = {}) {
    using namespace temporal_endpoint_detail;
    const auto os = tensors(old), ns = tensors(next);
    require(os.contains(tensor_key) && ns.contains(tensor_key), "form comparison requires persistent payload");
    const auto& a = *os.at(tensor_key); const auto& b = *ns.at(tensor_key);
    require(a.role == b.role && (a.role == TemporalTensorRole::Metric || a.role == TemporalTensorRole::CrossForm), "not a metric/cross form");
    require(a.input && a.output && a.input == b.input && a.output == b.output, "form carrier identities changed");
    const auto Jin = temporal_carrier_correspondence(old, next, *b.input, maps);
    const auto Jout = temporal_carrier_correspondence(old, next, *b.output, maps);
    return Jout.transpose().multiply(temporal_tensor_matrix(b)).multiply(Jin).add(temporal_tensor_matrix(a).scale(numeric::Rational{-1}));
}

// Recreate the existing linear local-system specialization. The endpoint must
// outlive this view, just as for the native LinearStalkBundle API. Each source
// cell must have exactly one selected stalk carrier; selection is explicit.
inline local::LinearRelationalFiberSystem temporal_linear_fiber_system(
    const TemporalEndpoint& endpoint, const std::vector<std::uint64_t>& stalk_carriers) {
    using namespace temporal_endpoint_detail;
    validate_temporal_endpoint(endpoint);
    const auto cs = carriers(endpoint);
    std::map<StableCellKey, std::uint64_t> selected;
    local::LinearStalkBundle bundle(endpoint.source);
    for (const auto key : stalk_carriers) {
        require(cs.contains(key) && cs.at(key)->kind == TemporalCarrierKind::CellStalk, "selected carrier is not a cell stalk");
        const auto& c = *cs.at(key);
        require(selected.emplace(*c.cell, key).second, "duplicate selected stalk owner");
        bundle.set_dimension(resolve_stable_cell(endpoint.source, *c.cell).value(), c.domain.logical_size());
    }
    std::size_t cell_count = 0;
    for (std::size_t k = 0; k < endpoint.source.grade_count(); ++k)
        cell_count += endpoint.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
    require(selected.size() == cell_count, "linear realization requires one selected stalk for every retained cell");
    local::LinearRelationalFiberSystem result(std::move(bundle));
    for (const auto& t : endpoint.tensors) {
        if (t.role != TemporalTensorRole::RealizedIncidenceMap && t.role != TemporalTensorRole::CoefficientIndependentMap) continue;
        require(selected.at(*cs.at(*t.input)->cell) == *t.input && selected.at(*cs.at(*t.output)->cell) == *t.output,
                "incidence map is bound to a different selected stalk realization");
        const auto slot = resolve_stable_occurrence(endpoint.source, t.owner.occurrence).value();
        result.set_transport(t.owner.occurrence.upper_grade, slot, temporal_realized_incidence(endpoint, t));
    }
    // Force dimension validation for every retained incidence, including zero
    // coefficients. An undeclared rectangular default is not invented.
    for (std::size_t k = 1; k < endpoint.source.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t p = 0; p < endpoint.source.grade(grade).incidence_count(); ++p) (void)result.flag_map(grade, p);
    }
    return result;
}

// The native sheaf borrows its stalk bundle. Keep that bundle at a stable heap
// address so returning/copying/moving this adapter cannot leave a dangling
// pointer to a local or moved-from bundle. The source endpoint is still borrowed.
class TemporalLinearCellularSheaf {
public:
    explicit TemporalLinearCellularSheaf(local::LinearStalkBundle stalks)
        : stalks_(std::make_shared<const local::LinearStalkBundle>(std::move(stalks))), sheaf_(*stalks_) {}
    [[nodiscard]] const local::LinearCellularSheaf& sheaf() const & noexcept { return sheaf_; }
    [[nodiscard]] local::LinearCellularSheaf& sheaf() & noexcept { return sheaf_; }
    const local::LinearCellularSheaf& sheaf() const && = delete;
private:
    std::shared_ptr<const local::LinearStalkBundle> stalks_;
    local::LinearCellularSheaf sheaf_;
};

inline TemporalLinearCellularSheaf temporal_linear_cellular_sheaf(
    const TemporalEndpoint& endpoint, const std::vector<std::uint64_t>& stalk_carriers) {
    using namespace temporal_endpoint_detail;
    validate_temporal_endpoint(endpoint);
    const auto cs = carriers(endpoint);
    const StableSourceIndex index(endpoint.source);
    std::map<StableCellKey, std::uint64_t> selected;
    local::LinearStalkBundle bundle(endpoint.source);
    for (const auto key : stalk_carriers) {
        require(cs.contains(key) && cs.at(key)->kind == TemporalCarrierKind::CellStalk, "selected sheaf carrier is not a cell stalk");
        const auto& carrier = *cs.at(key);
        require(selected.emplace(*carrier.cell, key).second, "duplicate selected sheaf stalk owner");
        bundle.set_dimension(index.cell(*carrier.cell).value(), carrier.domain.logical_size());
    }
    std::size_t cell_count = 0;
    for (std::size_t k = 0; k < endpoint.source.grade_count(); ++k)
        cell_count += endpoint.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
    require(selected.size() == cell_count, "sheaf realization requires one selected stalk for every retained cell");
    TemporalLinearCellularSheaf result(std::move(bundle));
    for (const auto& tensor : endpoint.tensors) {
        if (tensor.role != TemporalTensorRole::SheafRestriction) continue;
        require(selected.at(*cs.at(*tensor.input)->cell) == *tensor.input && selected.at(*cs.at(*tensor.output)->cell) == *tensor.output,
                "restriction belongs to a different selected sheaf realization");
        result.sheaf().set_restriction(tensor.owner.occurrence.upper_grade,
                                      index.occurrence(tensor.owner.occurrence).value(), temporal_tensor_matrix(tensor));
    }
    // Restriction is coefficient-independent: the native coboundary applies
    // the retained coefficient exactly once, not the downward-map convention.
    for (std::size_t k = 1; k < endpoint.source.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        for (std::size_t p = 0; p < endpoint.source.grade(grade).incidence_count(); ++p)
            (void)result.sheaf().restriction(grade, p);
    }
    return result;
}
inline TemporalLinearCellularSheaf temporal_linear_cellular_sheaf(
    const TemporalEndpoint&&, const std::vector<std::uint64_t>&) = delete;
inline local::LinearRelationalFiberSystem temporal_linear_fiber_system(
    const TemporalEndpoint&&, const std::vector<std::uint64_t>&) = delete;

} // namespace rfc::storage
