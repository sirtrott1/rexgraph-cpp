#pragma once
#include "rfc/algebra/tensor_product.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/core/relational_complex.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::topology {

// Canonical relational interval I with two derived endpoint identities and one
// asserted grade-one relation. It is a relational complex, not a graph object.
class RelationalInterval {
public:
    RelationalInterval() : complex_(build()) {}

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return complex_; }
    [[nodiscard]] core::CellId start() const noexcept { return core::CellId{core::Grade{0}, 0u}; }
    [[nodiscard]] core::CellId finish() const noexcept { return core::CellId{core::Grade{0}, 1u}; }
    [[nodiscard]] core::CellId edge() const noexcept { return core::CellId{core::Grade{1}, 0u}; }

private:
    static core::RelationalComplex build() {
        core::RelationSignature signature;
        const auto edge_type = signature.add_type(
            "relational-interval", core::BoundaryPolicy::DeclaredExact, 2u, 2u);
        core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(edge_type, {
            {0u, true, true, numeric::Rational{1}},
            {1u, true, false, numeric::Rational{1}}});
        return builder.freeze();
    }

    core::RelationalComplex complex_;
};

enum class CylinderEndpoint : std::uint8_t {
    Start,
    Finish,
};

// Lazy native cylinder X \boxtimes I. The product is never flattened merely to
// express the cylinder. In particular, a branching relation of X remains one
// primary factor cell at each endpoint and one prism factor cell during the
// homotopy direction; its boundary participants become side product cells.
class RelationalCylinder {
public:
    explicit RelationalCylinder(const core::RelationalComplex& source)
        : source_(&source) {}

    [[nodiscard]] const core::RelationalComplex& source() const noexcept { return *source_; }
    [[nodiscard]] const RelationalInterval& interval() const noexcept { return interval_; }

    [[nodiscard]] algebra::ProductCell endpoint_cell(core::CellId source_cell,
                                                     CylinderEndpoint endpoint) const {
        validate_source_cell(source_cell);
        const auto interval_cell = endpoint == CylinderEndpoint::Start ? interval_.start() : interval_.finish();
        return product().cell(source_cell.grade, source_cell.index,
                              interval_cell.grade, interval_cell.index);
    }

    [[nodiscard]] algebra::ProductCellIdentity endpoint_identity(core::CellId source_cell,
                                                                 CylinderEndpoint endpoint) const {
        return product().source_identity(endpoint_cell(source_cell, endpoint));
    }

    [[nodiscard]] algebra::ProductCell prism_cell(core::CellId source_cell) const {
        validate_source_cell(source_cell);
        const auto e = interval_.edge();
        return product().cell(source_cell.grade, source_cell.index, e.grade, e.index);
    }

    [[nodiscard]] algebra::ProductCellIdentity prism_identity(core::CellId source_cell) const {
        return product().source_identity(prism_cell(source_cell));
    }

    [[nodiscard]] std::vector<algebra::ProductBoundaryTerm> prism_boundary(core::CellId source_cell) const {
        return product().boundary_terms(prism_cell(source_cell));
    }

private:
    [[nodiscard]] algebra::RelationalTensorProduct product() const {
        return algebra::RelationalTensorProduct(*source_, interval_.complex());
    }

    void validate_source_cell(core::CellId source_cell) const {
        if (source_cell.grade.value >= source_->grade_count() ||
            source_cell.index >= source_->grade(source_cell.grade).cell_count()) {
            throw std::out_of_range("cylinder source cell");
        }
    }

    const core::RelationalComplex* source_{};
    RelationalInterval interval_{};
};

} // namespace rfc::topology
