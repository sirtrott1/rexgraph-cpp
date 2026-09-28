#include "rfc/core/attachment_update.hpp"
#include "rfc/field/hodge_certificate.hpp"
#include "rfc/geometry/structural_certificate.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TriangleFixture {
    rfc::core::RelationalComplex complex;
    rfc::core::RelationTypeId face{};
};

static TriangleFixture make_open_triangle() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 3, 3);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_grade_one(edge, {{1,true,true,false,1},{2,true,false,false,1}});
    builder.add_grade_one(edge, {{0,true,true,false,1},{2,true,false,false,1}});
    return {builder.freeze(), face};
}


struct UpperRowFixture {
    rfc::core::RelationalComplex complex;
    rfc::core::RelationTypeId face{};
};

static UpperRowFixture make_three_grade_parallel_fixture() {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto volume = signature.add_type("volume", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_grade_one(edge, {{0,true,true,false,1},{1,true,false,false,1}});
    builder.add_higher(rfc::core::Grade{2}, face, {{0,true,false,false,1},{1,true,true,false,1}});
    builder.add_higher(rfc::core::Grade{2}, face, {{0,true,false,false,1},{1,true,true,false,1}});
    builder.add_higher(rfc::core::Grade{3}, volume, {{0,true,false,false,1},{1,true,true,false,1}});
    return {builder.freeze(), face};
}

static std::vector<rfc::field::ExactMetric> identity_metrics(const rfc::core::RelationalComplex& complex) {
    std::vector<rfc::field::ExactMetric> metrics;
    metrics.reserve(complex.grade_count());
    for (std::size_t k = 0; k < complex.grade_count(); ++k) {
        metrics.push_back(rfc::field::ExactMetric::identity(
            complex.grade(rfc::core::Grade{static_cast<std::uint16_t>(k)}).cell_count()));
    }
    return metrics;
}

static bool same_subspace(const rfc::numeric::ExactMatrix& a,
                          const rfc::numeric::ExactMatrix& b) {
    if (a.rows() != b.rows()) return false;
    const auto joined = rfc::numeric::ExactMatrix::horizontal_concat(a,b);
    return joined.rank() == a.rank() && joined.rank() == b.rank();
}

int main() {
try {
    using namespace rfc;
    const auto fixture = make_open_triangle();
    const auto structural = geometry::ChainStructureCertificate::build(fixture.complex);

    require(structural.bianchi_satisfied(), "open triangle certificate is not Bianchi-valid");
    require(structural.boundary_rank(core::Grade{1}) == 2u, "triangle B1 rank mismatch");
    require(structural.betti(core::Grade{0}) == 1u, "triangle beta0 mismatch");
    require(structural.betti(core::Grade{1}) == 1u, "open triangle beta1 mismatch");
    require(structural.hodge_zero_multiplicity(core::Grade{1}) == 1u, "open triangle zero multiplicity mismatch");

    const auto open_metrics = identity_metrics(fixture.complex);
    const auto old_hodge = field::build_hodge_certificate(
        fixture.complex, structural, core::Grade{1}, open_metrics);
    require(old_hodge.harmonic_frame.cols() == 1u, "open triangle harmonic frame mismatch");
    require(old_hodge.curl_frame.cols() == 0u, "open triangle curl frame should be empty");

    // Attach the exact closed face z=(1,1,-1).  The attachment builder checks
    // B1 z=0 before the primary grade-two identity is admitted.
    const auto filled = geometry::append_higher_attachment_certified(
        fixture.complex, structural, core::Grade{2}, fixture.face,
        {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    const auto& increment_certificate = filled.structural;
    const auto& increment_delta = filled.delta;

    require(increment_delta.boundary_rank_gain == 1u, "face should add one B2 image direction");
    require(increment_delta.lower_betti_change == -1, "face should remove one beta1 direction");
    require(increment_delta.attached_betti_change == 0, "single independent face should not create beta2");
    require(increment_certificate.betti(core::Grade{1}) == 0u, "filled triangle beta1 mismatch");
    require(increment_certificate.betti(core::Grade{2}) == 0u, "filled triangle beta2 mismatch");

    const auto rebuilt_filled = geometry::ChainStructureCertificate::build(filled.complex);
    require(increment_certificate.boundary_rank(core::Grade{1}) == rebuilt_filled.boundary_rank(core::Grade{1}), "incremental B1 rank disagrees with full certificate");
    require(increment_certificate.boundary_rank(core::Grade{2}) == rebuilt_filled.boundary_rank(core::Grade{2}), "incremental B2 rank disagrees with full certificate");
    require(same_subspace(increment_certificate.boundary(core::Grade{2}).image_frame,
                          rebuilt_filled.boundary(core::Grade{2}).image_frame),
            "incremental B2 image frame disagrees with full certificate");
    require(same_subspace(increment_certificate.boundary(core::Grade{2}).kernel_frame,
                          rebuilt_filled.boundary(core::Grade{2}).kernel_frame),
            "incremental B2 kernel frame disagrees with full certificate");

    const auto hodge_update = field::update_hodge_certificate_after_attachment(
        old_hodge, increment_certificate, increment_delta);
    require(hodge_update.residual_rank == 1u, "face harmonic residual should have rank one");
    require(hodge_update.certificate.curl_frame.cols() == 1u, "filled triangle curl rank mismatch");
    require(hodge_update.certificate.harmonic_frame.cols() == 0u, "filled triangle harmonic space should vanish");
    require(hodge_update.certificate.zero_multiplicity == 0u, "filled triangle zero multiplicity mismatch");

    const auto filled_metrics = identity_metrics(filled.complex);
    const auto rebuilt_hodge = field::build_hodge_certificate(
        filled.complex, rebuilt_filled, core::Grade{1}, filled_metrics);
    require(same_subspace(hodge_update.certificate.curl_frame, rebuilt_hodge.curl_frame),
            "incremental curl frame disagrees with rebuilt Hodge certificate");
    require(same_subspace(hodge_update.certificate.harmonic_frame, rebuilt_hodge.harmonic_frame),
            "incremental harmonic frame disagrees with rebuilt Hodge certificate");

    // A second distinct primary face with the same boundary has zero B2 rank
    // gain.  It preserves the grade-one curl subspace but creates one grade-two
    // multiplicity homology direction.
    const auto duplicate = geometry::append_higher_attachment_certified(
        filled.complex, increment_certificate, core::Grade{2}, fixture.face,
        {{0,true,false,false,1},{1,true,false,false,1},{2,true,true,false,1}});
    const auto& duplicate_certificate = duplicate.structural;
    const auto& duplicate_delta = duplicate.delta;

    require(duplicate_delta.boundary_rank_gain == 0u, "duplicate face must not change B2 rank");
    require(duplicate_delta.lower_betti_change == 0, "duplicate face must not change beta1");
    require(duplicate_delta.attached_betti_change == 1, "duplicate face should create beta2 multiplicity mode");
    require(duplicate_certificate.betti(core::Grade{1}) == 0u, "duplicate face changed beta1");
    require(duplicate_certificate.betti(core::Grade{2}) == 1u, "duplicate face beta2 mismatch");
    require(duplicate_certificate.boundary(core::Grade{2}).kernel_frame.cols() == 1u,
            "duplicate face should add one B2 kernel direction");

    const auto duplicate_hodge = field::update_hodge_certificate_after_attachment(
        hodge_update.certificate, duplicate_certificate, duplicate_delta);
    require(duplicate_hodge.residual_rank == 0u, "duplicate face should have zero old harmonic residual");
    require(duplicate_hodge.certificate.curl_frame.cols() == 1u, "duplicate face changed curl subspace rank");
    require(duplicate_hodge.certificate.harmonic_frame.cols() == 0u, "duplicate face created false grade-one harmonic mode");

    const auto rebuilt_duplicate = geometry::ChainStructureCertificate::build(duplicate.complex);
    require(rebuilt_duplicate.betti(core::Grade{2}) == duplicate_certificate.betti(core::Grade{2}),
            "incremental duplicate beta2 disagrees with full certificate");
    require(same_subspace(rebuilt_duplicate.boundary(core::Grade{2}).kernel_frame,
                          duplicate_certificate.boundary(core::Grade{2}).kernel_frame),
            "incremental duplicate kernel frame disagrees with full certificate");

    // Changing the metric invalidates only the metric-dependent Hodge
    // representative data. The structural ranks/Betti values remain valid.
    auto weighted_metrics = identity_metrics(filled.complex);
    weighted_metrics[1] = field::ExactMetric::diagonal({2,3,5});
    const auto weighted_hodge = field::build_hodge_certificate(
        filled.complex, increment_certificate, core::Grade{1}, weighted_metrics);
    require(weighted_hodge.zero_multiplicity == increment_certificate.betti(core::Grade{1}),
            "metric change altered structural zero multiplicity");
    const field::ExactHodgeOperator weighted_reference(filled.complex, core::Grade{1}, weighted_metrics);
    require(same_subspace(weighted_hodge.gradient_frame, weighted_reference.gradient_frame()),
            "structural gradient certificate disagrees with exact adjoint reference");
    require(same_subspace(weighted_hodge.harmonic_frame, weighted_reference.harmonic_frame()),
            "structural harmonic certificate disagrees with exact Hodge reference");


    // Appending a lower-grade relation beneath an existing coface must not
    // force recomputation of the upper boundary certificate. Existing B3 gains
    // one all-zero codomain row, while its rank/kernel remain unchanged.
    const auto upper_fixture = make_three_grade_parallel_fixture();
    const auto upper_structural = geometry::ChainStructureCertificate::build(upper_fixture.complex);
    const auto upper_update = geometry::append_higher_attachment_certified(
        upper_fixture.complex, upper_structural, core::Grade{2}, upper_fixture.face,
        {{0,true,false,false,1},{1,true,true,false,1}});
    require(upper_update.delta.boundary_rank_gain == 0u, "parallel grade-two attachment changed B2 rank");
    require(upper_update.structural.boundary(core::Grade{3}).rank == upper_structural.boundary(core::Grade{3}).rank,
            "existing B3 rank changed under zero-row extension");
    require(upper_update.structural.boundary(core::Grade{3}).image_frame.rows() == 3u,
            "existing B3 image ambient was not extended for new grade-two cell");
    const auto upper_rebuilt = geometry::ChainStructureCertificate::build(upper_update.complex);
    require(same_subspace(upper_update.structural.boundary(core::Grade{3}).image_frame,
                          upper_rebuilt.boundary(core::Grade{3}).image_frame),
            "incremental upper image frame disagrees with full rebuild");
    require(same_subspace(upper_update.structural.boundary(core::Grade{3}).kernel_frame,
                          upper_rebuilt.boundary(core::Grade{3}).kernel_frame),
            "incremental upper kernel frame disagrees with full rebuild");

    std::cout << "incremental structural/Hodge certificate tests passed\n";
    return EXIT_SUCCESS;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return EXIT_FAILURE;
}
}
