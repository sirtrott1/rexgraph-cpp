#pragma once
#include "rfc/approximation/dense.hpp"
#include "rfc/field/connection.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::approximation {

class ConnectionTower {
public:
    static ConnectionTower from_exact(const field::ExactConnectionTower& exact,
                                      const std::vector<field::ExactMetric>& metrics) {
        if(metrics.size()!=exact.grade_count())throw std::invalid_argument("approx connection metric count mismatch");
        std::vector<DenseMatrix> maps; maps.reserve(exact.grade_count());
        maps.emplace_back(0,exact.complex().grade(core::Grade{0}).cell_count());
        for(std::size_t k=1;k<exact.grade_count();++k)maps.push_back(DenseMatrix::from_exact(exact.map(core::Grade{static_cast<std::uint16_t>(k)})));
        std::vector<DenseMatrix> forms;forms.reserve(metrics.size());for(const auto& m:metrics)forms.push_back(DenseMatrix::from_exact(m.materialize()));
        return ConnectionTower(std::move(maps),std::move(forms));
    }
    [[nodiscard]] DenseMatrix curvature(core::Grade upper_grade) const{
        if(upper_grade.value<2 || upper_grade.value>=maps_.size())throw std::out_of_range("approx curvature grade");
        return maps_[upper_grade.value-1u].multiply(maps_[upper_grade.value]);
    }
    [[nodiscard]] double curvature_magnitude(core::Grade upper_grade) const{
        const auto F=curvature(upper_grade);
        const auto& source=metrics_.at(upper_grade.value);
        const auto& target=metrics_.at(upper_grade.value-2u);
        DenseMatrix source_inverse(source.rows(),source.cols());
        for(std::size_t j=0;j<source.cols();++j){Vector e(source.rows(),0.0);e[j]=1.0;const auto x=source.solve(e);for(std::size_t i=0;i<x.size();++i)source_inverse(i,j)=x[i];}
        const auto energy=source_inverse.multiply(F.transpose()).multiply(target).multiply(F);
        double trace=0.0;for(std::size_t i=0;i<energy.rows();++i)trace+=energy(i,i);return trace;
    }
private:
    ConnectionTower(std::vector<DenseMatrix> maps,std::vector<DenseMatrix> metrics):maps_(std::move(maps)),metrics_(std::move(metrics)){}
    std::vector<DenseMatrix> maps_; std::vector<DenseMatrix> metrics_;
};

} // namespace rfc::approximation
