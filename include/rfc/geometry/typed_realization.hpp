#pragma once
#include "rfc/geometry/contract.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rfc::geometry {

struct TypedAccession {
    [[nodiscard]] static constexpr GeometryDescriptor geometry_descriptor() noexcept {
        return {GeometryKind::TypedRealization, "typed accession and physical realization", true};
    }
    std::string name;
    numeric::ExactMatrix accession;   // A: ambient -> visible
    numeric::ExactMatrix realization; // F: visible -> ambient physical probe

    TypedAccession(std::string name_value,
                   numeric::ExactMatrix A,
                   numeric::ExactMatrix F)
        : name(std::move(name_value)), accession(std::move(A)), realization(std::move(F)) {
        if (accession.cols() != realization.rows() || accession.rows() != realization.cols()) {
            throw std::invalid_argument("typed accession/realization dimensions do not share one ambient/visible pair");
        }
    }

    [[nodiscard]] std::size_t ambient_dim() const noexcept { return accession.cols(); }
    [[nodiscard]] std::size_t visible_dim() const noexcept { return accession.rows(); }
    [[nodiscard]] numeric::RationalVector observe(const numeric::RationalVector& x) const { return accession.apply(x); }
    [[nodiscard]] numeric::RationalVector realize(const numeric::RationalVector& u) const { return realization.apply(u); }
    [[nodiscard]] numeric::ExactMatrix ambient_view() const { return realization.multiply(accession); }
};

struct ReconstructingTypedView {
    TypedAccession view;
    numeric::ExactMatrix reconstruction; // R_tau: visible -> ambient

    ReconstructingTypedView(TypedAccession view_value, numeric::ExactMatrix R)
        : view(std::move(view_value)), reconstruction(std::move(R)) {
        if (reconstruction.rows() != view.ambient_dim() || reconstruction.cols() != view.visible_dim()) {
            throw std::invalid_argument("typed reconstruction dimensions mismatch");
        }
    }
};

inline numeric::ExactMatrix vertical_stack(const std::vector<numeric::ExactMatrix>& matrices) {
    if (matrices.empty()) return numeric::ExactMatrix(0,0);
    std::size_t cols = matrices.front().cols();
    std::size_t rows = 0;
    for (const auto& M : matrices) {
        if (M.cols() != cols) throw std::invalid_argument("vertical stack column mismatch");
        rows += M.rows();
    }
    numeric::ExactMatrix out(rows, cols);
    std::size_t offset = 0;
    for (const auto& M : matrices) {
        for (std::size_t i = 0; i < M.rows(); ++i) for (std::size_t j = 0; j < M.cols(); ++j) {
            const auto value = M.get(i,j); if (value != 0) out.set(offset+i,j,value);
        }
        offset += M.rows();
    }
    return out;
}

inline numeric::ExactMatrix horizontal_stack(const std::vector<numeric::ExactMatrix>& matrices) {
    if (matrices.empty()) return numeric::ExactMatrix(0,0);
    std::size_t rows = matrices.front().rows();
    std::size_t cols = 0;
    for (const auto& M : matrices) {
        if (M.rows() != rows) throw std::invalid_argument("horizontal stack row mismatch");
        cols += M.cols();
    }
    numeric::ExactMatrix out(rows, cols);
    std::size_t offset = 0;
    for (const auto& M : matrices) {
        for (std::size_t i = 0; i < M.rows(); ++i) for (std::size_t j = 0; j < M.cols(); ++j) {
            const auto value = M.get(i,j); if (value != 0) out.set(i,offset+j,value);
        }
        offset += M.cols();
    }
    return out;
}

class ReconstructingTypedFamily {
public:
    explicit ReconstructingTypedFamily(std::vector<ReconstructingTypedView> views)
        : views_(std::move(views)) {
        if (views_.empty()) throw std::invalid_argument("reconstructing typed family must be nonempty");
        ambient_dim_ = views_.front().view.ambient_dim();
        for (const auto& entry : views_) if (entry.view.ambient_dim() != ambient_dim_) {
            throw std::invalid_argument("all typed views must share one ambient dimension");
        }
        std::vector<numeric::ExactMatrix> As, Rs;
        As.reserve(views_.size()); Rs.reserve(views_.size());
        for (const auto& entry : views_) { As.push_back(entry.view.accession); Rs.push_back(entry.reconstruction); }
        A_ = vertical_stack(As);
        R_ = horizontal_stack(Rs);
        if (R_.multiply(A_) != numeric::ExactMatrix::identity(ambient_dim_)) {
            throw std::invalid_argument("typed reconstruction family does not satisfy R A = I");
        }
    }

    [[nodiscard]] std::size_t ambient_dim() const noexcept { return ambient_dim_; }
    [[nodiscard]] std::size_t represented_dim() const noexcept { return A_.rows(); }
    [[nodiscard]] const numeric::ExactMatrix& stacked_accession() const noexcept { return A_; }
    [[nodiscard]] const numeric::ExactMatrix& stacked_reconstruction() const noexcept { return R_; }
    [[nodiscard]] const std::vector<ReconstructingTypedView>& views() const noexcept { return views_; }

    [[nodiscard]] numeric::RationalVector observe(const numeric::RationalVector& x) const { return A_.apply(x); }
    [[nodiscard]] numeric::RationalVector reconstruct(const numeric::RationalVector& v) const { return R_.apply(v); }
    [[nodiscard]] numeric::ExactMatrix observation_projector() const { return A_.multiply(R_); }
    [[nodiscard]] bool consistent(const numeric::RationalVector& v) const {
        return v.size() == represented_dim() && observation_projector().apply(v) == v;
    }

private:
    std::vector<ReconstructingTypedView> views_;
    std::size_t ambient_dim_{};
    numeric::ExactMatrix A_;
    numeric::ExactMatrix R_;
};

inline numeric::ExactMatrix represented_map(const ReconstructingTypedFamily& target,
                                             const numeric::ExactMatrix& ambient_map,
                                             const ReconstructingTypedFamily& source) {
    if (ambient_map.rows() != target.ambient_dim() || ambient_map.cols() != source.ambient_dim()) {
        throw std::invalid_argument("represented map ambient dimensions mismatch");
    }
    return target.stacked_accession().multiply(ambient_map).multiply(source.stacked_reconstruction());
}

struct DescentCertificate {
    bool descends{false};
    numeric::ExactMatrix obstruction; // A_target B ker(A_source)
};

inline DescentCertificate descent_certificate(const numeric::ExactMatrix& source_accession,
                                               const numeric::ExactMatrix& target_accession,
                                               const numeric::ExactMatrix& ambient_map) {
    if (ambient_map.cols() != source_accession.cols() || ambient_map.rows() != target_accession.cols()) {
        throw std::invalid_argument("descent certificate ambient dimensions mismatch");
    }
    const auto obstruction = target_accession.multiply(ambient_map).multiply(source_accession.kernel_frame());
    return DescentCertificate{obstruction.rank() == 0, obstruction};
}


inline numeric::ExactMatrix represented_block(const ReconstructingTypedView& target,
                                               const numeric::ExactMatrix& ambient_map,
                                               const ReconstructingTypedView& source) {
    if (ambient_map.rows() != target.view.ambient_dim() || ambient_map.cols() != source.view.ambient_dim()) {
        throw std::invalid_argument("represented typed block ambient dimensions mismatch");
    }
    return target.view.accession.multiply(ambient_map).multiply(source.reconstruction);
}

struct EffectiveImageCoordinates {
    numeric::ExactMatrix frame;          // visible coordinates spanning im A
    numeric::ExactMatrix preimage;       // ambient representatives of frame columns; A preimage = frame
    numeric::ExactMatrix left_inverse;   // effective coordinates from visible image; L frame = I
};

inline EffectiveImageCoordinates effective_image_coordinates(const numeric::ExactMatrix& accession) {
    const std::size_t r = accession.rank();
    numeric::ExactMatrix frame(accession.rows(), r);
    numeric::ExactMatrix preimage(accession.cols(), r);
    std::size_t chosen = 0;
    numeric::ExactMatrix trial(accession.rows(), 0);
    for (std::size_t j = 0; j < accession.cols() && chosen < r; ++j) {
        numeric::ExactMatrix column(accession.rows(), 1);
        for (std::size_t i = 0; i < accession.rows(); ++i) {
            const auto v = accession.get(i,j); if (v != 0) column.set(i,0,v);
        }
        const auto candidate = trial.cols() == 0 ? column : numeric::ExactMatrix::horizontal_concat(trial,column);
        if (candidate.rank() > trial.rank()) {
            for (std::size_t i = 0; i < accession.rows(); ++i) {
                const auto v = accession.get(i,j); if (v != 0) frame.set(i,chosen,v);
            }
            preimage.set(j,chosen,numeric::Rational{1});
            trial = candidate;
            ++chosen;
        }
    }
    if (chosen != r) throw std::logic_error("failed to construct exact effective image frame");
    numeric::ExactMatrix left_inverse(r, accession.rows());
    if (r != 0) {
        const auto gram = frame.transpose().multiply(frame);
        left_inverse = gram.inverse().multiply(frame.transpose());
    }
    return EffectiveImageCoordinates{std::move(frame),std::move(preimage),std::move(left_inverse)};
}

struct EffectiveInducedMap {
    EffectiveImageCoordinates source;
    EffectiveImageCoordinates target;
    numeric::ExactMatrix map; // effective target coordinates <- effective source coordinates
};

inline EffectiveInducedMap induced_effective_map(const numeric::ExactMatrix& source_accession,
                                                  const numeric::ExactMatrix& target_accession,
                                                  const numeric::ExactMatrix& ambient_map) {
    const auto certificate = descent_certificate(source_accession,target_accession,ambient_map);
    if (!certificate.descends) throw std::invalid_argument("accession does not admit a well-defined induced map");
    auto source = effective_image_coordinates(source_accession);
    auto target = effective_image_coordinates(target_accession);
    const auto visible_action = target_accession.multiply(ambient_map).multiply(source.preimage);
    const auto effective = target.left_inverse.multiply(visible_action);
    if (target.frame.multiply(effective) != visible_action) {
        throw std::logic_error("induced quotient map failed exact image reconstruction");
    }
    return EffectiveInducedMap{std::move(source),std::move(target),effective};
}

} // namespace rfc::geometry
