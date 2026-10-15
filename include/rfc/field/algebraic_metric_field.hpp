#pragma once
#include "rfc/field/algebraic_metric.hpp"
#include "rfc/local/algebraic_fiber_system.hpp"

namespace rfc::field {
struct AlgebraicMetricFieldLimits { std::size_t max_retained_coordinates{1000000}; };

// Complete cell metrics on one retained fiber system. Boundary execution uses
// its original prepared native action; reverse occurrence actions use the
// declared metrics. Support, signed source weights and all chain laws survive.
class AlgebraicFiberMetricField {
    struct State {
        local::AlgebraicRelationalFiberSystem system;
        AlgebraicMetricDomain domain;
        std::vector<std::vector<std::optional<AlgebraicModuleMetric>>> metrics;
        std::vector<std::vector<local::AlgebraicModuleMap>> adjoints;
    };
public:
    using Values=std::vector<local::AlgebraicModuleVector>;
    static AlgebraicFiberMetricField declare(const local::AlgebraicRelationalFiberSystem& system,
        const AlgebraicMetricDomain& domain,const std::vector<AlgebraicModuleMetric>& metrics,AlgebraicMetricFieldLimits limits={}) {
        if (!system.bundle().field().same_owner(domain.field())) throw std::invalid_argument("metric field coefficient owners differ");
        const auto& source=system.bundle().source(); const auto d=domain.field().degree();
        std::size_t cells{},slots{};
        std::vector<std::vector<std::optional<AlgebraicModuleMetric>>> ordered(source.grade_count());
        for (std::size_t k=0;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            ordered[k].resize(source.grade(grade).cell_count()); cells=machine::checked_size_add(cells,ordered[k].size());
        }
        if (metrics.size()!=cells) throw std::invalid_argument("metric field must declare every retained cell");
        for (const auto& metric:metrics) {
            const auto cell=metric.carrier().cell();
            if (!system.bundle().stalk(cell).same_owner(metric.carrier())) throw std::invalid_argument("metric field carrier owner differs");
            if (!domain.same_owner(metric.domain())) throw std::invalid_argument("metric field domain owner differs");
            auto& slot=ordered.at(cell.grade.value).at(cell.index);
            if (slot) throw std::invalid_argument("duplicate metric field cell");
            slots=machine::checked_size_add(slots,machine::checked_size_mul(metric.measurement().coefficient_form().coefficients().size(),
                machine::checked_size_mul(d,d)));
            if (slots>limits.max_retained_coordinates) throw std::length_error("metric field retained coordinate budget");
            slot=metric;
        }
        std::vector<std::vector<local::AlgebraicModuleMap>> adjoints(source.grade_count());
        // Admission precedes adjoint allocation, including inactive and zero legs.
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)}; const auto& arena=source.grade(grade);
            for (std::size_t i=0;i<arena.incidence_count();++i) {
                const auto& map=system.occurrence_map(grade,i);
                slots=machine::checked_size_add(slots,machine::checked_size_mul(map.coefficients().size(),machine::checked_size_mul(d,d)));
                if (slots>limits.max_retained_coordinates) throw std::length_error("metric field retained adjoint coordinate budget");
            }
        }
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)}; const auto& arena=source.grade(grade);
            adjoints[k].reserve(arena.incidence_count());
            for (std::size_t i=0;i<arena.incidence_count();++i)
                adjoints[k].push_back(algebraic_metric_adjoint(system.occurrence_map(grade,i),
                    ordered[k][arena.upper_index[i]].value(),ordered[k-1][arena.lower_index[i]].value()));
        }
        return AlgebraicFiberMetricField(std::make_shared<const State>(State{system,domain,std::move(ordered),std::move(adjoints)}));
    }
    const local::AlgebraicRelationalFiberSystem& system() const & noexcept { return state_->system; }
    const AlgebraicMetricDomain& domain() const & noexcept { return state_->domain; }
    const AlgebraicModuleMetric& metric(core::CellId cell) const & { return state_->metrics.at(cell.grade.value).at(cell.index).value(); }
    const local::AlgebraicModuleMap& occurrence_adjoint(core::Grade grade,std::size_t slot) const & {
        if (!grade.value) throw std::out_of_range("metric occurrence adjoint requires a positive grade");
        return state_->adjoints.at(grade.value).at(slot);
    }
    const local::AlgebraicRelationalFiberSystem& system() const && = delete;
    const AlgebraicMetricDomain& domain() const && = delete;
    const AlgebraicModuleMetric& metric(core::CellId) const && = delete;
    const local::AlgebraicModuleMap& occurrence_adjoint(core::Grade,std::size_t) const && = delete;
    bool same_owner(const AlgebraicFiberMetricField& other) const noexcept { return state_==other.state_; }
    numeric::AlgebraicElement inner(core::Grade grade,const Values& x,const Values& y) const {
        validate(grade,x); validate(grade,y); auto result=domain().field().scalar(numeric::Rational{0});
        for (std::size_t i=0;i<x.size();++i)
            result=result+metric({grade,static_cast<std::uint32_t>(i)}).inner(x[i],y[i]);
        return result;
    }
    Values boundary(core::Grade grade,const Values& values) const { return system().apply(grade,values); }
    // The argument names B_k; the input is on grade k-1 and output on grade k.
    Values coboundary(core::Grade boundary_grade,const Values& values) const {
        if (!boundary_grade.value) throw std::out_of_range("coboundary requires a positive boundary grade");
        const auto lower=core::Grade{static_cast<std::uint16_t>(boundary_grade.value-1u)};
        const auto& arena=system().bundle().source().grade(boundary_grade); validate(lower,values);
        auto result=zeros(boundary_grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            if (!arena.aspects.get(system().bundle().source().signature().existence_aspect(),slot)) continue;
            const auto image=state_->adjoints[boundary_grade.value][slot].apply(values[arena.lower_index[slot]]);
            const auto weight=domain().field().scalar(system().bundle().source().incidence_coefficient(boundary_grade,slot));
            auto coefficients=result[arena.upper_index[slot]].coordinates();
            for (std::size_t j=0;j<coefficients.size();++j) coefficients[j]=coefficients[j]+weight*image.coordinates()[j];
            result[arena.upper_index[slot]]=image.carrier().vector(std::move(coefficients));
        }
        return result;
    }
    Values hodge(core::Grade grade,const Values& values) const {
        validate(grade,values); auto out=zeros(grade);
        if (grade.value) out=coboundary(grade,boundary(grade,values));
        if (std::size_t(grade.value)+1<system().bundle().source().grade_count()) {
            const auto upper=core::Grade{static_cast<std::uint16_t>(grade.value+1u)};
            const auto up=boundary(upper,coboundary(upper,values));
            for (std::size_t i=0;i<out.size();++i) {
                auto coefficients=out[i].coordinates();
                for (std::size_t j=0;j<coefficients.size();++j) coefficients[j]=coefficients[j]+up[i].coordinates()[j];
                out[i]=out[i].carrier().vector(std::move(coefficients));
            }
        }
        return out;
    }
    numeric::AlgebraicElement adjacent_quadrance(core::Grade grade,const Values& values) const {
        validate(grade,values); auto out=domain().field().scalar(numeric::Rational{0});
        if (grade.value) {
            const auto lower=core::Grade{static_cast<std::uint16_t>(grade.value-1u)}; const auto down=boundary(grade,values);
            out=out+inner(lower,down,down);
        }
        if (std::size_t(grade.value)+1<system().bundle().source().grade_count()) {
            const auto upper=core::Grade{static_cast<std::uint16_t>(grade.value+1u)}; const auto up=coboundary(upper,values);
            out=out+inner(upper,up,up);
        }
        return out;
    }
    Values zeros(core::Grade grade) const {
        const auto& arena=system().bundle().source().grade(grade); Values out; out.reserve(arena.cell_count());
        for (std::size_t i=0;i<arena.cell_count();++i) {
            const auto module=system().bundle().stalk({grade,static_cast<std::uint32_t>(i)});
            out.push_back(module.vector(std::vector<numeric::AlgebraicElement>(module.rank(),domain().field().scalar(numeric::Rational{0}))));
        }
        return out;
    }
private:
    explicit AlgebraicFiberMetricField(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    void validate(core::Grade grade,const Values& values) const {
        const auto& arena=system().bundle().source().grade(grade);
        if (values.size()!=arena.cell_count()) throw std::invalid_argument("metric grade field requires every retained stalk");
        for (std::size_t i=0;i<values.size();++i)
            if (!system().bundle().stalk({grade,static_cast<std::uint32_t>(i)}).same_owner(values[i].carrier()))
                throw std::invalid_argument("metric grade field carrier owner differs");
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::field
