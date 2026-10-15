#pragma once
#include "rfc/field/algebraic_metric_field.hpp"
#include "../cellular_attachments/model.hpp"

namespace metric_example {
using Q=rfc::numeric::Rational;
using Field=rfc::numeric::CertifiedAlgebraicField;
using E=rfc::numeric::AlgebraicElement;
using Map=rfc::local::AlgebraicModuleMap;
using Module=rfc::local::AlgebraicModule;
using Metric=rfc::field::AlgebraicModuleMetric;
using Form=rfc::field::AlgebraicMeasurementForm;
using Domain=rfc::field::AlgebraicMetricDomain;
using Involution=rfc::numeric::AlgebraicInvolution;
using G=rfc::core::Grade;
struct Case { const char* name; Domain domain; };
inline std::vector<Case> cases() {
    const auto cubic=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto imaginary=Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto quartic=Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    const auto octic=Field::declare({1,0,0,0,0,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    return {
        {"real_cubic",Domain::real(rfc::geometry::SelectedRealAlgebraicEmbedding::isolate(cubic,1,Q{3,2}),Involution::identity(cubic))},
        {"hermitian_quadratic",Domain::hermitian(rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(imaginary,{0,1},Q{1,4}),
            Involution::declare(imaginary,-imaginary.generator()))},
        {"hermitian_quartic",Domain::hermitian(rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(quartic,{Q{11,16},Q{11,16}},Q{1,8}),
            Involution::declare(quartic,quartic.generator().inverse()))},
        {"hermitian_octic",Domain::hermitian(rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(octic,{Q{59,64},Q{49,128}},Q{1,32}),
            Involution::declare(octic,octic.generator().inverse()))}
    };
}
inline Metric metric(const Domain& domain,const Module& module,Q weight=Q{2}) {
    const auto& f=domain.field(); const auto t=f.generator()+f.scalar(Q{1});
    const auto conjugate=domain.involution().apply(t); const auto d=f.scalar(weight),e=f.scalar(Q{3});
    if (module.rank()!=2) throw std::invalid_argument("example metric uses a declared rank-two carrier");
    return Metric::certify(Form::declare(domain,Map::declare(module,module,{d,d*t,conjugate*d,conjugate*d*t+e})));
}
inline rfc::field::AlgebraicFiberMetricField metric_field(const Domain& domain,
    const rfc::local::AlgebraicRelationalFiberSystem& system) {
    std::vector<Metric> declared;
    for (std::size_t k=0;k<system.bundle().source().grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};
        for (std::size_t i=0;i<system.bundle().source().grade(grade).cell_count();++i) {
            const auto module=system.bundle().stalk({grade,static_cast<std::uint32_t>(i)});
            if (module.rank()==2) declared.push_back(metric(domain,module,Q(rfc::numeric::Integer(k+i+2))));
            else declared.push_back(Metric::identity(domain,module));
        }
    }
    return rfc::field::AlgebraicFiberMetricField::declare(system,domain,declared);
}
inline rfc::field::AlgebraicFiberMetricField::Values values(const rfc::field::AlgebraicFiberMetricField& field,G grade) {
    auto out=field.zeros(grade); const auto& f=field.domain().field();
    for (std::size_t i=0;i<out.size();++i) {
        std::vector<E> coordinates;
        for (std::size_t j=0;j<out[i].carrier().rank();++j)
            coordinates.push_back(f.generator().power(j+1)+f.scalar(Q(rfc::numeric::Integer(i+j+1))));
        out[i]=out[i].carrier().vector(std::move(coordinates));
    }
    return out;
}
} // namespace metric_example
