#pragma once
#include "rfc/core/axioms.hpp"
#include "rfc/numeric/algebraic_field.hpp"

namespace rfc::local {
struct AlgebraicModuleLimits {
    std::size_t max_cells{4096},max_rank{64},max_retained_coordinates{1000000},max_matrix_coordinates{1000000};
};
namespace algebraic_module_detail {
struct BundleState {
    core::RelationalComplex source;
    numeric::CertifiedAlgebraicField field;
    std::vector<std::vector<std::size_t>> ranks;
    AlgebraicModuleLimits limits;
};
} // namespace algebraic_module_detail
class AlgebraicModuleVector;
class AlgebraicStalkBundle;

// A free K-module belongs to one retained cell and one explicit bundle basis.
// Repeating equal declarations creates a different carrier owner.
class AlgebraicModule {
public:
    AlgebraicModule(const AlgebraicModule&)=default;
    AlgebraicModule& operator=(const AlgebraicModule&)=default;
    core::CellId cell() const noexcept { return cell_; }
    std::size_t rank() const noexcept { return owner_->ranks[cell_.grade.value][cell_.index]; }
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return owner_->field; }
    const AlgebraicModuleLimits& limits() const & noexcept { return owner_->limits; }
    const core::RelationalComplex& source() const & noexcept { return owner_->source; }
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const AlgebraicModuleLimits& limits() const && = delete;
    const core::RelationalComplex& source() const && = delete;
    bool same_owner(const AlgebraicModule& other) const noexcept { return owner_==other.owner_ && cell_==other.cell_; }
    AlgebraicModuleVector vector(std::vector<numeric::AlgebraicElement> coordinates) const;
private:
    friend class AlgebraicStalkBundle;
    AlgebraicModule(std::shared_ptr<const algebraic_module_detail::BundleState> owner,core::CellId cell)
        : owner_(std::move(owner)),cell_(cell) {}
    std::shared_ptr<const algebraic_module_detail::BundleState> owner_;
    core::CellId cell_;
};
class AlgebraicModuleVector {
public:
    AlgebraicModuleVector(const AlgebraicModuleVector&)=default;
    AlgebraicModuleVector& operator=(const AlgebraicModuleVector&)=default;
    const AlgebraicModule& carrier() const & noexcept { return carrier_; }
    const std::vector<numeric::AlgebraicElement>& coordinates() const & noexcept { return coordinates_; }
    const AlgebraicModule& carrier() const && = delete;
    const std::vector<numeric::AlgebraicElement>& coordinates() const && = delete;
    numeric::RationalVector rational_coordinates() const {
        numeric::RationalVector result; result.reserve(machine::checked_size_mul(carrier_.rank(),carrier_.field().degree()));
        for (const auto& value:coordinates_) result.insert(result.end(),value.coordinates().begin(),value.coordinates().end());
        return result;
    }
private:
    friend class AlgebraicModule;
    AlgebraicModuleVector(AlgebraicModule carrier,std::vector<numeric::AlgebraicElement> values)
        : carrier_(std::move(carrier)),coordinates_(std::move(values)) {}
    AlgebraicModule carrier_;
    std::vector<numeric::AlgebraicElement> coordinates_;
};
inline AlgebraicModuleVector AlgebraicModule::vector(std::vector<numeric::AlgebraicElement> coordinates) const {
    if (coordinates.size()!=rank()) throw std::invalid_argument("algebraic module vector rank differs");
    for (const auto& value:coordinates)
        if (!field().owns(value)) throw std::invalid_argument("algebraic module vector field owner differs");
    return {*this,std::move(coordinates)};
}
class AlgebraicStalkBundle {
public:
    AlgebraicStalkBundle(const AlgebraicStalkBundle&)=default;
    AlgebraicStalkBundle& operator=(const AlgebraicStalkBundle&)=default;
    static AlgebraicStalkBundle declare(const core::RelationalComplex& source,const numeric::CertifiedAlgebraicField& field,
        std::vector<std::vector<std::size_t>> ranks,AlgebraicModuleLimits limits={}) {
        if (!source.grade_count() || ranks.size()!=source.grade_count())
            throw std::invalid_argument("algebraic bundle requires a complete retained source and rank declaration");
        std::size_t cells{},coordinates{};
        for (std::size_t k=0;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            if (ranks[k].size()!=source.grade(grade).cell_count())
                throw std::invalid_argument("algebraic bundle must declare every retained cell stalk");
            cells=machine::checked_size_add(cells,ranks[k].size());
            for (const auto rank:ranks[k]) {
                if (!rank) throw std::invalid_argument("algebraic cell stalk rank must be positive");
                if (rank>limits.max_rank) throw std::length_error("algebraic module rank budget");
                coordinates=machine::checked_size_add(coordinates,machine::checked_size_mul(rank,field.degree()));
            }
        }
        if (cells>limits.max_cells || coordinates>limits.max_retained_coordinates)
            throw std::length_error("algebraic bundle retained coordinate budget");
        if (!core::bianchi_through(source,source.max_grade()).satisfied())
            throw std::invalid_argument("algebraic bundle source violates the complete chain family");
        return AlgebraicStalkBundle(std::make_shared<const algebraic_module_detail::BundleState>(
            algebraic_module_detail::BundleState{source,field,std::move(ranks),limits}));
    }
    const core::RelationalComplex& source() const & noexcept { return state_->source; }
    const numeric::CertifiedAlgebraicField& field() const & noexcept { return state_->field; }
    const AlgebraicModuleLimits& limits() const & noexcept { return state_->limits; }
    const core::RelationalComplex& source() const && = delete;
    const numeric::CertifiedAlgebraicField& field() const && = delete;
    const AlgebraicModuleLimits& limits() const && = delete;
    bool same_owner(const AlgebraicStalkBundle& other) const noexcept { return state_==other.state_; }
    AlgebraicModule stalk(core::CellId cell) const {
        (void)state_->ranks.at(cell.grade.value).at(cell.index); return AlgebraicModule(state_,cell);
    }
    std::vector<std::size_t> offsets(core::Grade grade) const {
        const auto& ranks=state_->ranks.at(grade.value); std::vector<std::size_t> result(ranks.size()+1,0);
        for (std::size_t i=0;i<ranks.size();++i) result[i+1]=machine::checked_size_add(result[i],ranks[i]);
        return result;
    }
private:
    explicit AlgebraicStalkBundle(std::shared_ptr<const algebraic_module_detail::BundleState> state) : state_(std::move(state)) {}
    std::shared_ptr<const algebraic_module_detail::BundleState> state_;
};

// Complete row-major K coefficients, with separate domain/codomain owners.
class AlgebraicModuleMap {
    struct State { AlgebraicModule source,target; std::vector<numeric::AlgebraicElement> coefficients; numeric::ExactMatrix action; };
public:
    AlgebraicModuleMap(const AlgebraicModuleMap&)=default;
    AlgebraicModuleMap& operator=(const AlgebraicModuleMap&)=default;
    static AlgebraicModuleMap declare(const AlgebraicModule& source,const AlgebraicModule& target,
                                     std::vector<numeric::AlgebraicElement> coefficients) {
        if (!source.field().same_owner(target.field())) throw std::invalid_argument("module map coefficient field owners differ");
        const auto count=machine::checked_size_mul(source.rank(),target.rank());
        if (coefficients.size()!=count) throw std::invalid_argument("module map requires a complete coefficient block");
        const auto d=source.field().degree();
        const auto slots=machine::checked_size_mul(count,machine::checked_size_mul(d,d));
        if (slots>source.limits().max_matrix_coordinates || slots>target.limits().max_matrix_coordinates)
            throw std::length_error("module rational action coordinate budget");
        numeric::ExactMatrix action(machine::checked_size_mul(target.rank(),d),machine::checked_size_mul(source.rank(),d));
        for (std::size_t i=0;i<target.rank();++i) for (std::size_t j=0;j<source.rank();++j) {
            const auto& value=coefficients[i*source.rank()+j];
            if (!source.field().owns(value)) throw std::invalid_argument("module coefficient field owner differs");
            const auto block=value.regular_action();
            for (const auto& [key,q]:block.entries()) action.set(i*d+key.first,j*d+key.second,q);
        }
        return AlgebraicModuleMap(std::make_shared<const State>(State{source,target,std::move(coefficients),std::move(action)}));
    }
    static AlgebraicModuleMap identity(const AlgebraicModule& carrier) {
        const auto count=machine::checked_size_mul(carrier.rank(),carrier.rank());
        if (count>carrier.limits().max_matrix_coordinates) throw std::length_error("module identity coordinate budget");
        std::vector<numeric::AlgebraicElement> coefficients(count,carrier.field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<carrier.rank();++i) coefficients[i*carrier.rank()+i]=carrier.field().scalar(numeric::Rational{1});
        return declare(carrier,carrier,std::move(coefficients));
    }
    static AlgebraicModuleMap zero(const AlgebraicModule& source,const AlgebraicModule& target) {
        if (!source.field().same_owner(target.field())) throw std::invalid_argument("zero module map field owners differ");
        const auto count=machine::checked_size_mul(source.rank(),target.rank());
        if (count>std::min(source.limits().max_matrix_coordinates,target.limits().max_matrix_coordinates))
            throw std::length_error("zero module map coordinate budget");
        return declare(source,target,std::vector<numeric::AlgebraicElement>(count,source.field().scalar(numeric::Rational{0})));
    }
    const AlgebraicModule& source() const & noexcept { return state_->source; }
    const AlgebraicModule& target() const & noexcept { return state_->target; }
    const std::vector<numeric::AlgebraicElement>& coefficients() const & noexcept { return state_->coefficients; }
    const numeric::ExactMatrix& rational_action() const & noexcept { return state_->action; }
    const AlgebraicModule& source() const && = delete;
    const AlgebraicModule& target() const && = delete;
    const std::vector<numeric::AlgebraicElement>& coefficients() const && = delete;
    const numeric::ExactMatrix& rational_action() const && = delete;
    bool same_owner(const AlgebraicModuleMap& other) const noexcept { return state_==other.state_; }
    AlgebraicModuleMap scale(const numeric::AlgebraicElement& scalar) const {
        if (!state_->source.field().owns(scalar)) throw std::invalid_argument("module map scalar field owner differs");
        auto out=state_->coefficients; for (auto& value:out) value=value*scalar;
        return declare(state_->source,state_->target,std::move(out));
    }
    AlgebraicModuleMap add(const AlgebraicModuleMap& other) const {
        if (!state_->source.same_owner(other.state_->source) || !state_->target.same_owner(other.state_->target))
            throw std::invalid_argument("module map addition carrier owners differ");
        auto out=state_->coefficients;
        for (std::size_t i=0;i<out.size();++i) out[i]=out[i]+other.state_->coefficients[i];
        return declare(state_->source,state_->target,std::move(out));
    }
    AlgebraicModuleVector apply(const AlgebraicModuleVector& value) const {
        if (!state_->source.same_owner(value.carrier())) throw std::invalid_argument("module input carrier owner differs");
        auto out=std::vector<numeric::AlgebraicElement>(state_->target.rank(),state_->source.field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<out.size();++i) for (std::size_t j=0;j<state_->source.rank();++j)
            out[i]=out[i]+state_->coefficients[i*state_->source.rank()+j]*value.coordinates()[j];
        return state_->target.vector(std::move(out));
    }
    AlgebraicModuleMap then(const AlgebraicModuleMap& next) const {
        if (!state_->target.same_owner(next.state_->source)) throw std::invalid_argument("module intermediate carrier owners differ");
        const auto count=machine::checked_size_mul(state_->source.rank(),next.state_->target.rank());
        if (count>std::min(state_->source.limits().max_matrix_coordinates,next.state_->target.limits().max_matrix_coordinates))
            throw std::length_error("module composition coordinate budget");
        std::vector<numeric::AlgebraicElement> out(count,state_->source.field().scalar(numeric::Rational{0}));
        for (std::size_t i=0;i<next.state_->target.rank();++i) for (std::size_t j=0;j<state_->source.rank();++j)
            for (std::size_t k=0;k<state_->target.rank();++k)
                out[i*state_->source.rank()+j]=out[i*state_->source.rank()+j]+
                    next.state_->coefficients[i*state_->target.rank()+k]*state_->coefficients[k*state_->source.rank()+j];
        return declare(state_->source,next.state_->target,std::move(out));
    }
    std::size_t rank_over_field() const {
        const auto rank=state_->action.rank(),d=state_->source.field().degree();
        if (rank%d) throw std::logic_error("module scalar restriction rank invariant");
        return rank/d;
    }
    std::size_t nullity_over_field() const { return state_->source.rank()-rank_over_field(); }
    AlgebraicModuleMap inverse() const {
        if (state_->source.rank()!=state_->target.rank()) throw std::invalid_argument("rectangular module map has no inverse");
        const auto inverse_action=state_->action.inverse(); const auto d=state_->source.field().degree();
        std::vector<numeric::AlgebraicElement> coefficients; coefficients.reserve(state_->coefficients.size());
        for (std::size_t i=0;i<state_->source.rank();++i) for (std::size_t j=0;j<state_->target.rank();++j) {
            numeric::RationalVector column(d);
            for (std::size_t k=0;k<d;++k) column[k]=inverse_action.get(i*d+k,j*d);
            coefficients.push_back(state_->source.field().element(column));
        }
        auto result=declare(state_->target,state_->source,std::move(coefficients));
        if (result.rational_action()!=inverse_action) throw std::logic_error("inverse module complete block invariant");
        return result;
    }
private:
    explicit AlgebraicModuleMap(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::local
