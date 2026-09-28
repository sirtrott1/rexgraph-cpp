#pragma once
#include "rfc/approximation/dense.hpp"
#include "rfc/field/execution_tower.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::approximation {

struct TowerField { std::vector<Vector> grades; };

class ApproximationTower {
public:
    static ApproximationTower from_exact(const core::RelationalComplex& complex,
                                         const std::vector<field::ExactMetric>& metrics) {
        if(metrics.size()!=complex.grade_count())throw std::invalid_argument("approx tower metric count mismatch");
        std::vector<DenseMatrix> forms; forms.reserve(metrics.size());
        for(const auto& metric:metrics)forms.push_back(DenseMatrix::from_exact(metric.materialize()));
        return ApproximationTower(complex,std::move(forms));
    }
    static ApproximationTower unweighted(const core::RelationalComplex& complex){
        std::vector<DenseMatrix> forms; forms.reserve(complex.grade_count());
        for(std::size_t k=0;k<complex.grade_count();++k)forms.push_back(DenseMatrix::identity(complex.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count()));
        return ApproximationTower(complex,std::move(forms));
    }

    [[nodiscard]] Vector apply(const field::OperatorRequest& request,const Vector& input) const{
        const auto grade=request.grade.value;
        if(grade>=complex_->grade_count())throw std::out_of_range("approx operator grade");
        switch(request.kind){
            case field::OperatorKind::Boundary:
                if(grade==0)throw std::invalid_argument("grade zero has no boundary");
                return boundary(grade).apply(input);
            case field::OperatorKind::Adjoint:
                if(grade==0)throw std::invalid_argument("grade zero has no adjoint");
                return adjoint(grade,input);
            case field::OperatorKind::HodgeDown:{
                if(input.size()!=metric_[grade].rows())throw std::invalid_argument("approx Hodge input size mismatch");
                if(grade==0)return Vector(input.size(),0.0);
                const auto B=boundary(grade); return adjoint(grade,B.apply(input));
            }
            case field::OperatorKind::HodgeUp:{
                if(input.size()!=metric_[grade].rows())throw std::invalid_argument("approx Hodge input size mismatch");
                if(grade+1u>=complex_->grade_count())return Vector(input.size(),0.0);
                const auto B=boundary(grade+1u); return B.apply(adjoint(grade+1u,input));
            }
            case field::OperatorKind::Hodge:{
                auto down=apply({field::OperatorKind::HodgeDown,request.grade,{}},input);
                auto up=apply({field::OperatorKind::HodgeUp,request.grade,{}},input);return approximation::add(down,up);
            }
            case field::OperatorKind::Green:{
                const auto L=action_matrix(request.grade,field::OperatorKind::Hodge);
                const auto A=DenseMatrix::identity(L.rows()).add(L.scale(request.lambda.get_d()));
                return A.solve(input);
            }
            case field::OperatorKind::HarmonicProjector:
                throw std::invalid_argument("approx harmonic projector is intentionally not implicit; request exact projector then convert if desired");
            case field::OperatorKind::Dirac:
                throw std::invalid_argument("Dirac acts on tower field; use apply_dirac");
        }
        throw std::logic_error("unknown approximate operator request");
    }

    [[nodiscard]] TowerField apply_dirac(const TowerField& input) const{
        if(input.grades.size()!=complex_->grade_count())throw std::invalid_argument("approx Dirac grade count mismatch");
        TowerField out; out.grades.reserve(input.grades.size());
        for(std::size_t k=0;k<input.grades.size();++k)out.grades.emplace_back(input.grades[k].size(),0.0);
        for(std::size_t k=1;k<input.grades.size();++k){
            const auto B=boundary(k);
            out.grades[k-1u]=approximation::add(out.grades[k-1u],B.apply(input.grades[k]));
            out.grades[k]=approximation::add(out.grades[k],adjoint(k,input.grades[k-1u]));
        }
        return out;
    }

    [[nodiscard]] TowerField apply_hodge(const TowerField& input) const{
        if(input.grades.size()!=complex_->grade_count())throw std::invalid_argument("approx Hodge tower count mismatch");
        TowerField out; out.grades.reserve(input.grades.size());
        for(std::size_t k=0;k<input.grades.size();++k)out.grades.push_back(apply({field::OperatorKind::Hodge,core::Grade{static_cast<std::uint16_t>(k)},{}},input.grades[k]));
        return out;
    }

private:
    ApproximationTower(const core::RelationalComplex& complex,std::vector<DenseMatrix> metrics):complex_(&complex),metric_(std::move(metrics)){}
    [[nodiscard]] DenseMatrix boundary(std::size_t grade) const{return DenseMatrix::from_exact(field::boundary_matrix(*complex_,core::Grade{static_cast<std::uint16_t>(grade)}));}
    [[nodiscard]] Vector adjoint(std::size_t upper_grade,const Vector& lower) const{
        const auto B=boundary(upper_grade);
        const auto weighted=metric_[upper_grade-1u].apply(lower);
        return metric_[upper_grade].solve(B.transpose().apply(weighted));
    }
    [[nodiscard]] DenseMatrix action_matrix(core::Grade grade,field::OperatorKind kind) const{
        const auto n=complex_->grade(grade).cell_count(); DenseMatrix A(n,n);
        for(std::size_t j=0;j<n;++j){Vector e(n,0.0);e[j]=1.0;const auto y=apply({kind,grade,{}},e);for(std::size_t i=0;i<n;++i)A(i,j)=y[i];}
        return A;
    }
    const core::RelationalComplex* complex_{};
    std::vector<DenseMatrix> metric_;
};

inline Vector to_approximate(const numeric::RationalVector& exact){Vector out;out.reserve(exact.size());for(const auto& v:exact)out.push_back(v.get_d());return out;}
inline TowerField to_approximate(const field::ExactTowerField& exact){TowerField out;out.grades.reserve(exact.grades.size());for(const auto& g:exact.grades)out.grades.push_back(to_approximate(g));return out;}

} // namespace rfc::approximation
