#include "rfc/algebra/morphism.hpp"
#include "rfc/algebra/subcomplex.hpp"
#include "rfc/algebra/tensor_product.hpp"
#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/numeric/index_domain.hpp"
#include "rfc/numeric/sparse_tensor.hpp"
#include "rfc/numeric/structural_support.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <class F> static void require_throws(F&& fn, const char* message) {
    bool threw = false;
    try { fn(); } catch (const std::exception&) { threw = true; }
    if (!threw) throw std::runtime_error(message);
}

static rfc::core::RelationalComplex interval(std::size_t extra_witnesses = 0) {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("interval-edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    for (std::size_t i = 0; i < extra_witnesses; ++i) {
        builder.declare_witness_identity(static_cast<rfc::core::IdentityKey>(1000u + i));
    }
    return builder.freeze();
}

int main() {
    try {
        // Shape, structural support, and numerical state are independent.  A huge
        // declared shape must not allocate its absent coordinates.
        const rfc::numeric::IndexDomain huge_shape({100000u, 10000u});
        require(huge_shape.logical_size() == 1000000000u, "large index-domain size mismatch");
        const auto sparse_support = rfc::numeric::StructuralSupport::sparse(
            huge_shape.logical_size(), {7u, 999999999u});
        require(sparse_support.storage() == rfc::numeric::StructuralSupport::Storage::SparseIndices,
                "large sparse support should remain index-sparse");
        require(sparse_support.count() == 2u, "sparse structural support count mismatch");
        rfc::numeric::SparseRationalVector sparse_values(
            huge_shape.logical_size(), {{7u, rfc::numeric::Rational{3}}});
        const rfc::numeric::SparseRationalTensor sparse_tensor(huge_shape, sparse_support, sparse_values);
        require(sparse_tensor.stored_nonzeros() == 1u, "sparse tensor stored-value count mismatch");
        require(sparse_tensor.state(7u).has_value() && *sparse_tensor.state(7u) == 3,
                "supported nonzero tensor state mismatch");
        require(sparse_tensor.state(999999999u).has_value() &&
                    *sparse_tensor.state(999999999u) == 0,
                "supported zero tensor state must remain distinct from absence");
        require(!sparse_tensor.state(8u).has_value(),
                "structurally absent tensor coordinate must remain absent");

        const auto packed_support = rfc::numeric::StructuralSupport::packed(130u, {1u, 64u, 129u});
        require(packed_support.storage() == rfc::numeric::StructuralSupport::Storage::PackedBits,
                "packed structural support storage mismatch");
        require(packed_support.count() == 3u && packed_support.contains(129u),
                "packed structural support contents mismatch");

        const auto I = interval();
        rfc::algebra::SubcomplexMask left_endpoint(I);
        left_endpoint.select({rfc::core::Grade{0}, 0});
        require(left_endpoint.boundary_closed(I), "endpoint subcomplex should be closed");

        rfc::algebra::SubcomplexMask whole(I);
        whole.select({rfc::core::Grade{1}, 0});
        require(!whole.boundary_closed(I), "unclosed edge selection should not be a subcomplex");
        whole.close_boundaries(I);
        require(whole.boundary_closed(I), "boundary closure failed");
        require(whole.grade(rfc::core::Grade{0}).count() == 2u, "edge closure should contain both endpoints");

        const auto meet_result = meet(left_endpoint, whole);
        const auto join_result = join(left_endpoint, whole);
        require(meet_result.grade(rfc::core::Grade{0}).count() == 1u, "subcomplex meet failed");
        require(join_result.grade(rfc::core::Grade{1}).count() == 1u, "subcomplex join failed");

        std::vector<std::vector<std::uint32_t>> identity_maps{{0,1},{0}};
        rfc::algebra::RelationalMorphism identity(I, I, identity_maps);
        require(identity.is_strict() && identity.is_chain_map(), "identity relational morphism must be structurally strict");
        require(identity.occurrence_image(rfc::core::Grade{1},0u) == 0u &&
                identity.occurrence_image(rfc::core::Grade{1},1u) == 1u,
                "strict morphism must retain an occurrence-level attachment map");

        rfc::core::RelationSignature reversed_signature;
        const auto reversed_edge = reversed_signature.add_type("interval-edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        rfc::core::RelationalComplexBuilder reversed_builder(reversed_signature);
        reversed_builder.add_grade_one(reversed_edge, {{0,true,false,false,1},{1,true,true,false,1}});
        const auto reversed = reversed_builder.freeze();
        require_throws([&] { rfc::algebra::RelationalMorphism bad(I, reversed, identity_maps); },
                       "strict morphism must reject changed occurrence orientation/boundary semantics");

        rfc::core::RelationSignature other_signature;
        const auto other_edge = other_signature.add_type("different-edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        rfc::core::RelationalComplexBuilder other_builder(other_signature);
        other_builder.add_grade_one(other_edge, {{0,true,true,false,1},{1,true,false,false,1}});
        const auto other = other_builder.freeze();
        require_throws([&] { rfc::algebra::RelationalMorphism bad(I, other, identity_maps); },
                       "strict morphism must reject changed relation-type semantics");

        rfc::algebra::RelationalTensorProduct square(I, I);
        require(square.cell_count(rfc::core::Grade{0}) == 4u, "I tensor I grade-zero count mismatch");
        require(square.cell_count(rfc::core::Grade{1}) == 4u, "I tensor I grade-one count mismatch");
        require(square.cell_count(rfc::core::Grade{2}) == 1u, "I tensor I grade-two count mismatch");
        const auto grade_one_blocks = square.blocks(rfc::core::Grade{1});
        require(grade_one_blocks.size() == 2u, "tensor product block count mismatch");
        require(grade_one_blocks.front().domain().extents() == std::vector<std::size_t>({2u, 1u}),
                "tensor block product shape mismatch");
        const auto top = square.elementary_tensor(rfc::core::Grade{1}, 0, rfc::core::Grade{1}, 0);
        const auto boundary = square.boundary_apply(rfc::core::Grade{2}, top);
        const auto boundary2 = square.boundary_apply(rfc::core::Grade{1}, boundary);
        require(rfc::numeric::is_zero(boundary2), "tensor product chain condition failed");

        // Native product cells are exact identity pairs, not flat Cartesian
        // coordinates.  The Leibniz boundary retains factor occurrence identity
        // and records the Koszul sign separately.
        const auto lazy_top = square.elementary_product(rfc::core::Grade{1}, 0,
                                                         rfc::core::Grade{1}, 0);
        require(lazy_top.nonzeros() == 1u, "lazy elementary product should have one active cell");
        const auto product_cell = lazy_top.entries().front().cell;
        const auto product_identity = square.source_identity(product_cell);
        require(product_identity.left.grade.value == 1u && product_identity.right.grade.value == 1u,
                "product source identity grade mismatch");
        const auto top_terms = square.boundary_terms(product_cell);
        require(top_terms.size() == 4u, "I tensor I top cell should have four lazy boundary occurrences");
        std::size_t left_terms = 0;
        std::size_t right_terms = 0;
        for (const auto& term : top_terms) {
            if (term.side == rfc::algebra::ProductBoundarySide::Left) {
                ++left_terms;
                require(!term.koszul_flip, "left product boundary must not carry a Koszul flip");
            } else {
                ++right_terms;
                require(term.koszul_flip, "odd left grade must flip the right product boundary");
            }
        }
        require(left_terms == 2u && right_terms == 2u, "product boundary side accounting mismatch");

        const auto lazy_boundary = square.boundary_apply_lazy(lazy_top);
        const auto lazy_boundary2 = square.boundary_apply_lazy(lazy_boundary);
        require(lazy_boundary2.empty(), "lazy tensor product chain condition failed");
        require(square.materialize(lazy_boundary).dense() == boundary,
                "lazy product boundary disagrees with materialized reference");

        const auto sparse_top = square.elementary_tensor_sparse(rfc::core::Grade{1}, 0,
                                                                 rfc::core::Grade{1}, 0);
        require(sparse_top.nonzeros() == 1u, "elementary sparse tensor should have one active coordinate");
        const auto sparse_boundary = square.boundary_apply_sparse(rfc::core::Grade{2}, sparse_top);
        require(sparse_boundary.dense() == boundary, "sparse tensor boundary disagrees with dense reference");
        rfc::numeric::WorkCounters tensor_work;
        const auto counted_sparse_boundary = square.boundary_apply_sparse_counted(
            rfc::core::Grade{2}, sparse_top.view(), tensor_work);
        require(counted_sparse_boundary.dense() == boundary, "counted sparse tensor boundary mismatch");
        require(tensor_work.active_coordinates == 1u && tensor_work.incidences_touched == 4u,
                "sparse tensor work must follow active support and local incidence only");
        const auto sparse_boundary2 = square.boundary_apply_sparse(rfc::core::Grade{1}, sparse_boundary);
        require(sparse_boundary2.empty(), "sparse tensor product chain condition failed");

        // Fixed-support scaling regression: pad both factors with many unrelated
        // grade-zero identities.  One active edge x vertex product still touches
        // only the two incidences of that edge and produces two lazy coordinates,
        // even though the grade-zero Cartesian projection now has > 1M cells.
        const auto padded_left = interval(1024u);
        const auto padded_right = interval(1024u);
        rfc::algebra::RelationalTensorProduct padded(padded_left, padded_right);
        require(padded.cell_count(rfc::core::Grade{0}) > 1000000u,
                "padded product fixture should have a large materializable grade-zero projection");
        const auto padded_source = padded.elementary_product(rfc::core::Grade{1}, 0,
                                                              rfc::core::Grade{0}, 0);
        rfc::numeric::WorkCounters padded_work;
        const auto padded_boundary = padded.boundary_apply_lazy_counted(padded_source.view(), padded_work);
        require(padded_boundary.nonzeros() == 2u, "padded lazy product boundary support mismatch");
        require(padded_work.active_coordinates == 1u && padded_work.incidences_touched == 2u,
                "lazy product work must not scale with Cartesian ambient size");

        std::cout << "native algebra tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
