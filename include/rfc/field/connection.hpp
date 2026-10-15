#pragma once
#include "rfc/field/exact_hodge.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::field {

struct CurvatureInterface {
    core::Grade upper_grade{};
    numeric::ExactMatrix defect;
    [[nodiscard]] bool flat() const noexcept { return defect.nonzeros()==0; }
};

struct RouteDefectChannel {
    std::size_t middle_cell{};
    numeric::ExactMatrix contribution;
};

enum class RouteCoverage : std::uint8_t {
    CompleteInterface,
    PartialChart
};

// Retains the route index before contraction. A partial chart with a nonzero
// assembled contribution is not called curved: it may simply omit cancelling
// routes that exist elsewhere in the complete retained interface.
struct RouteDefectCertificate {
    core::Grade upper_grade{};
    RouteCoverage coverage{RouteCoverage::CompleteInterface};
    std::vector<RouteDefectChannel> channels;
    numeric::ExactMatrix assembled;

    [[nodiscard]] bool has_nonzero_channel() const noexcept {
        for (const auto& channel : channels) if (channel.contribution.nonzeros() != 0) return true;
        return false;
    }
    [[nodiscard]] bool assembled_zero() const noexcept { return assembled.nonzeros() == 0; }
    [[nodiscard]] bool globally_flat() const noexcept {
        return coverage == RouteCoverage::CompleteInterface && assembled_zero();
    }
    [[nodiscard]] bool curved() const noexcept {
        return coverage == RouteCoverage::CompleteInterface && !assembled_zero();
    }
    [[nodiscard]] bool local_channels_cancel() const noexcept {
        return globally_flat() && has_nonzero_channel();
    }
    [[nodiscard]] bool partial_nonzero_not_curvature() const noexcept {
        return coverage == RouteCoverage::PartialChart && !assembled_zero();
    }
};

class ExactConnectionTower {
public:
    static ExactConnectionTower ideal(const core::RelationalComplex& complex) {
        std::vector<numeric::ExactMatrix> maps;
        maps.reserve(complex.grade_count());
        maps.emplace_back(0,complex.grade(core::Grade{0}).cell_count());
        for (std::size_t k=1;k<complex.grade_count();++k) maps.push_back(boundary_matrix(complex,core::Grade{static_cast<std::uint16_t>(k)}));
        return ExactConnectionTower(complex,std::move(maps));
    }

    ExactConnectionTower(const core::RelationalComplex& complex,
                         std::vector<numeric::ExactMatrix> grade_maps)
        : complex_(&complex), maps_(std::move(grade_maps)) {
        if (maps_.size()!=complex.grade_count()) throw std::invalid_argument("connection map count mismatch");
        for (std::size_t k=1;k<maps_.size();++k) {
            const auto lower=complex.grade(core::Grade{static_cast<std::uint16_t>(k-1u)}).cell_count();
            const auto upper=complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();
            if (maps_[k].rows()!=lower || maps_[k].cols()!=upper) throw std::invalid_argument("connection map shape mismatch");
        }
    }

    [[nodiscard]] const core::RelationalComplex& complex() const noexcept { return *complex_; }
    [[nodiscard]] std::size_t grade_count() const noexcept { return maps_.size(); }

    [[nodiscard]] const numeric::ExactMatrix& map(core::Grade upper_grade) const {
        if (upper_grade.value==0 || upper_grade.value>=maps_.size()) throw std::out_of_range("connection grade");
        return maps_[upper_grade.value];
    }

    [[nodiscard]] ExactConnectionTower with_map(core::Grade grade, numeric::ExactMatrix replacement) const {
        if (grade.value==0 || grade.value>=maps_.size()) throw std::out_of_range("connection replacement grade");
        auto copy=maps_;
        copy[grade.value]=std::move(replacement);
        return ExactConnectionTower(*complex_,std::move(copy));
    }

    [[nodiscard]] ExactConnectionTower entrywise_scaled(core::Grade grade,
                                                         const numeric::ExactMatrix& multipliers) const {
        const auto& original=map(grade);
        if (multipliers.rows()!=original.rows() || multipliers.cols()!=original.cols()) throw std::invalid_argument("connection multiplier shape mismatch");
        numeric::ExactMatrix deformed(original.rows(),original.cols());
        for (const auto& [key,value] : original.entries()) {
            const auto factor=multipliers.get(key.first,key.second);
            if (factor!=0) deformed.set(key.first,key.second,value*factor);
        }
        return with_map(grade,std::move(deformed));
    }

    [[nodiscard]] CurvatureInterface curvature(core::Grade upper_grade) const {
        if (upper_grade.value<2 || upper_grade.value>=maps_.size()) throw std::out_of_range("curvature requires grade >= 2");
        return CurvatureInterface{upper_grade,maps_[upper_grade.value-1u].multiply(maps_[upper_grade.value])};
    }

    [[nodiscard]] bool flat_through(core::Grade through_grade) const {
        if (through_grade.value>=maps_.size()) through_grade=core::Grade{static_cast<std::uint16_t>(maps_.size()-1u)};
        for (std::uint16_t k=2;k<=through_grade.value;++k) if (!curvature(core::Grade{k}).flat()) return false;
        return true;
    }


    [[nodiscard]] RouteDefectCertificate route_defect_certificate(
        core::Grade upper_grade,
        const std::vector<std::size_t>& middle_cells,
        RouteCoverage coverage) const {
        if (upper_grade.value < 2 || upper_grade.value >= maps_.size()) {
            throw std::out_of_range("route defect certificate requires grade >= 2");
        }
        const auto& lower = maps_[upper_grade.value - 1u];
        const auto& upper = maps_[upper_grade.value];
        if (coverage != RouteCoverage::CompleteInterface && coverage != RouteCoverage::PartialChart)
            throw std::invalid_argument("unknown route coverage");
        auto selected = middle_cells;
        std::sort(selected.begin(), selected.end());
        if (std::adjacent_find(selected.begin(), selected.end()) != selected.end())
            throw std::invalid_argument("duplicate route middle cell");
        if (coverage == RouteCoverage::CompleteInterface && selected.size() != lower.cols())
            throw std::invalid_argument("complete interface requires every middle cell exactly once");
        RouteDefectCertificate out;
        out.upper_grade = upper_grade;
        out.coverage = coverage;
        out.assembled = numeric::ExactMatrix(lower.rows(), upper.cols());
        out.channels.reserve(middle_cells.size());
        for (const auto middle : middle_cells) {
            if (middle >= lower.cols() || middle >= upper.rows()) {
                throw std::out_of_range("route defect middle cell");
            }
            auto contribution = route_outer_product(lower,upper,middle);
            out.assembled = out.assembled.add(contribution);
            out.channels.push_back(RouteDefectChannel{middle, std::move(contribution)});
        }
        return out;
    }

    [[nodiscard]] RouteDefectCertificate global_route_defect_certificate(core::Grade upper_grade) const {
        if (upper_grade.value < 2 || upper_grade.value >= maps_.size()) {
            throw std::out_of_range("global route defect certificate requires grade >= 2");
        }
        const auto middle_count = maps_[upper_grade.value - 1u].cols();
        std::vector<std::size_t> middle(middle_count);
        for (std::size_t i = 0; i < middle_count; ++i) middle[i] = i;
        return route_defect_certificate(upper_grade, middle, RouteCoverage::CompleteInterface);
    }

    [[nodiscard]] std::vector<numeric::ExactMatrix> route_contributions(core::Grade upper_grade) const {
        if (upper_grade.value<2 || upper_grade.value>=maps_.size()) throw std::out_of_range("route contributions require grade >= 2");
        const auto& lower=maps_[upper_grade.value-1u];
        const auto& upper=maps_[upper_grade.value];
        std::vector<numeric::ExactMatrix> routes;
        routes.reserve(lower.cols());
        for (std::size_t middle=0;middle<lower.cols();++middle) {
            routes.push_back(route_outer_product(lower,upper,middle));
        }
        return routes;
    }

    [[nodiscard]] numeric::Rational curvature_magnitude(core::Grade upper_grade,
                                                         const std::vector<ExactMetric>& metrics) const {
        if (metrics.size()!=complex_->grade_count()) throw std::invalid_argument("curvature metric count mismatch");
        if(upper_grade.value<2 || upper_grade.value>=maps_.size())throw std::out_of_range("curvature requires grade >= 2");
        const auto& upper=maps_[upper_grade.value];
        const auto& lower=maps_[upper_grade.value-1u];
        const auto& source_metric=metrics[upper_grade.value];
        const auto& target_metric=metrics[upper_grade.value-2u];
        const auto upper_transpose=upper.transpose(),lower_transpose=lower.transpose();
        numeric::Rational trace=0;
        // tr(Ms^-1 F^T Mt F), evaluated through the two original factors and
        // reusable metric solves. Neither F, an inverse, nor F^dagger F is built.
        std::set<std::size_t> active;
        for(const auto& [ij,q]:upper.entries()){(void)q;active.insert(ij.second);}
        for(const auto i:active){
            if(target_metric.supports_sparse_apply() && source_metric.supports_sparse_solve()){
                const auto basis=numeric::SparseRationalVector::basis(upper.cols(),i);
                const auto first=upper.apply_sparse(basis.view()),image=lower.apply_sparse(first.view());
                const auto measured=target_metric.apply(image.view()),middle=lower_transpose.apply_sparse(measured.view());
                const auto dual=upper_transpose.apply_sparse(middle.view()),solution=source_metric.solve(dual.view());
                for(const auto& entry:solution.entries())if(entry.index==i)trace+=entry.value;
            }else{
                // Compatibility with a caller's explicitly declared external
                // full-coordinate callback, which has no sparse action port.
                numeric::RationalVector basis(upper.cols());basis[i]=1;
                const auto measured=target_metric.apply(lower.apply(upper.apply(basis)));
                const auto dual=upper_transpose.apply(lower_transpose.apply(measured));
                trace+=source_metric.solve(dual)[i];
            }
        }
        trace.canonicalize();
        return trace;
    }

    static ExactConnectionTower coordinate_transport(const core::RelationalComplex& complex,
                                                      const std::vector<numeric::ExactMatrix>& coordinates) {
        if (coordinates.size()!=complex.grade_count()) throw std::invalid_argument("coordinate transport count mismatch");
        std::vector<numeric::ExactMatrix> maps;
        maps.reserve(complex.grade_count());
        maps.emplace_back(0,complex.grade(core::Grade{0}).cell_count());
        for (std::size_t k=1;k<complex.grade_count();++k) {
            const auto rhs=boundary_matrix(complex,core::Grade{static_cast<std::uint16_t>(k)}).multiply(coordinates[k]);
            maps.push_back(solve_columns(coordinates[k-1u],rhs));
        }
        return ExactConnectionTower(complex,std::move(maps));
    }

private:
    static numeric::ExactMatrix route_outer_product(const numeric::ExactMatrix& lower,
        const numeric::ExactMatrix& upper,std::size_t middle) {
        std::vector<std::pair<std::size_t,numeric::Rational>> left,right;
        for(const auto& [ij,q]:lower.entries())if(ij.second==middle)left.emplace_back(ij.first,q);
        for(const auto& [ij,q]:upper.entries())if(ij.first==middle)right.emplace_back(ij.second,q);
        numeric::ExactMatrix result(lower.rows(),upper.cols());
        for(const auto& [i,a]:left)for(const auto& [j,b]:right)result.set(i,j,a*b);
        return result;
    }
    const core::RelationalComplex* complex_{};
    std::vector<numeric::ExactMatrix> maps_;
};

} // namespace rfc::field
