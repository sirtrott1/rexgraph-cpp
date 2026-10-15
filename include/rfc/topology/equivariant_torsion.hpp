#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/sparse_algebraic_matrix.hpp"
#include <memory>
#include <numeric>
#include <tuple>

namespace rfc::topology {

enum class EquivariantCoefficientBasis { NativeRational, CoordinatedIntegral };
namespace equivariant_detail {
using Q=numeric::Rational;
using Guard=numeric::ExactActionGuard;
using Key=std::tuple<std::size_t,std::size_t,std::uint64_t>;
using Boundary=std::map<Key,Q>;
struct State {
    core::RelationalComplex source;
    std::uint64_t order,key,revision;
    EquivariantCoefficientBasis basis;
    std::vector<std::vector<std::uint64_t>> voltages;
    std::vector<Boundary> boundaries;
};
inline std::uint64_t add_mod(std::uint64_t a,std::uint64_t b,std::uint64_t order){return a>=order-b?a-(order-b):a+b;}
inline core::Grade grade(std::size_t k){return core::Grade{static_cast<std::uint16_t>(k)};}
inline Q coefficient(const State& state,core::Grade g,std::size_t slot){
    return state.basis==EquivariantCoefficientBasis::NativeRational?state.source.incidence_coefficient(g,slot):
        Q{state.source.grade(g).lattice_coefficient[slot]};
}
inline void accumulate(Boundary& boundary,Key key,const Q& value,Guard& guard){
    const auto it=boundary.find(key);const Q old=it==boundary.end()?Q{0}:it->second;
    auto updated=guard.add(old,value);
    if(updated==0)boundary.erase(key);else boundary.insert_or_assign(std::move(key),std::move(updated));
    guard.entries(boundary.size());
}
inline void algebraic_value(const numeric::AlgebraicElement& value,Guard& guard){
    guard.entries(value.coordinates().size());
    for(const auto& coefficient:value.coordinates()){guard.step();guard.value(coefficient);}
}
inline void validate(const State& state,Guard& guard){
    guard.dimension(state.source.grade_count());guard.dimension(static_cast<std::size_t>(state.order));
    for(std::size_t k=0;k<state.source.grade_count();++k){
        const auto& arena=state.source.grade(grade(k));guard.dimension(arena.cell_count());guard.entries(arena.incidence_count());
        for(const auto& scale:arena.cell_scales){guard.step();guard.value(scale);}
        for(std::size_t slot=0;slot<arena.incidence_count();++slot){guard.step();guard.value(coefficient(state,grade(k),slot));}
        for(const auto& [key,value]:state.boundaries[k]){(void)key;guard.step();guard.value(value);}
    }
}
} // namespace equivariant_detail

class CyclicEquivariantSpecialization;
class CyclicTorsionCertificate;
class CyclicRegularChain;

// A full retained source plus one declared deck voltage on every occurrence,
// including inactive occurrences and every higher grade.  The resulting free
// Q[C_n]-chain is checked before any character specialization.  A geometric CW
// interpretation remains a declaration of the source, not an inference from
// equal aggregated rational boundaries.
class PreparedCyclicEquivariantChain {
public:
    static PreparedCyclicEquivariantChain prepare(core::RelationalComplex source,std::uint64_t order,
        std::vector<std::vector<std::uint64_t>> voltages,EquivariantCoefficientBasis basis,
        std::uint64_t key,std::uint64_t revision,numeric::ExactActionGuard& guard){
        using namespace equivariant_detail;
        if(!order||!key||!revision)throw std::invalid_argument("equivariant source needs a deck order, identity and revision");
        if(order>std::numeric_limits<std::size_t>::max())throw std::length_error("equivariant deck order exceeds addressable indices");
        if(basis!=EquivariantCoefficientBasis::NativeRational&&basis!=EquivariantCoefficientBasis::CoordinatedIntegral)
            throw std::invalid_argument("unknown equivariant coefficient basis");
        if(voltages.size()!=source.grade_count())throw std::invalid_argument("equivariant voltages omit source grades");
        State state{std::move(source),order,key,revision,basis,std::move(voltages),{}};
        state.boundaries.resize(state.source.grade_count());guard.dimension(static_cast<std::size_t>(order));
        const auto exists=state.source.signature().existence_aspect();
        for(std::size_t k=0;k<state.source.grade_count();++k){
            const auto g=grade(k);const auto& arena=state.source.grade(g);
            if(state.voltages[k].size()!=arena.incidence_count())throw std::invalid_argument("equivariant voltages must cover every retained occurrence");
            for(std::size_t slot=0;slot<arena.incidence_count();++slot){
                guard.step();const auto voltage=state.voltages[k][slot];
                if(voltage>=order)throw std::out_of_range("deck voltage is not a declared residue");
                if(!arena.aspects.get(exists,slot))continue;
                accumulate(state.boundaries[k],{arena.lower_index[slot],arena.upper_index[slot],voltage},coefficient(state,g,slot),guard);
            }
        }
        equivariant_detail::validate(state,guard);
        for(std::size_t k=2;k<state.boundaries.size();++k){
            Boundary defect;
            for(const auto& [left_key,left]:state.boundaries[k-1]){
                const auto [row,middle,first]=left_key;
                auto it=state.boundaries[k].lower_bound(Key{middle,0,0});
                for(;it!=state.boundaries[k].end()&&std::get<0>(it->first)==middle;++it){
                    const auto [unused,col,second]=it->first;(void)unused;
                    accumulate(defect,{row,col,add_mod(first,second,order)},guard.mul(left,it->second),guard);
                }
            }
            if(!defect.empty())throw std::invalid_argument("declared cyclic-equivariant boundaries do not form a chain complex");
        }
        return PreparedCyclicEquivariantChain(std::make_shared<const State>(std::move(state)));
    }
    const core::RelationalComplex& source() const & noexcept{return state_->source;}
    const core::RelationalComplex& source() const &&=delete;
    const std::vector<std::vector<std::uint64_t>>& voltages() const & noexcept{return state_->voltages;}
    const std::vector<std::vector<std::uint64_t>>& voltages() const &&=delete;
    const equivariant_detail::Boundary& formal_boundary(core::Grade grade) const &{return state_->boundaries.at(grade.value);}
    const equivariant_detail::Boundary& formal_boundary(core::Grade) const &&=delete;
    std::uint64_t order() const noexcept{return state_->order;}
    std::uint64_t key() const noexcept{return state_->key;}
    std::uint64_t revision() const noexcept{return state_->revision;}
    EquivariantCoefficientBasis coefficient_basis() const noexcept{return state_->basis;}
    bool same_owner(const PreparedCyclicEquivariantChain& other) const noexcept{return state_==other.state_;}
    void validate(numeric::ExactActionGuard& guard) const{equivariant_detail::validate(*state_,guard);}
    CyclicEquivariantSpecialization specialize(const numeric::AlgebraicElement& character,numeric::ExactActionGuard& guard) const;
    CyclicRegularChain regular_chain(core::Grade grade,numeric::SparseRationalVector values,numeric::ExactActionGuard& guard) const;
    CyclicRegularChain boundary(const CyclicRegularChain& input,numeric::ExactActionGuard& guard) const;
private:
    friend class CyclicRegularChain;
    explicit PreparedCyclicEquivariantChain(std::shared_ptr<const equivariant_detail::State> state):state_(std::move(state)){}
    std::shared_ptr<const equivariant_detail::State> state_;
};

class CyclicRegularChain {
public:
    const PreparedCyclicEquivariantChain& owner() const & noexcept{return owner_;}
    const PreparedCyclicEquivariantChain& owner() const &&=delete;
    core::Grade grade() const noexcept{return grade_;}
    const numeric::SparseRationalVector& values() const & noexcept{return values_;}
    const numeric::SparseRationalVector& values() const &&=delete;
private:
    friend class PreparedCyclicEquivariantChain;
    CyclicRegularChain(PreparedCyclicEquivariantChain owner,core::Grade grade,numeric::SparseRationalVector values)
        :owner_(std::move(owner)),grade_(grade),values_(std::move(values)){}
    PreparedCyclicEquivariantChain owner_;core::Grade grade_;numeric::SparseRationalVector values_;
};

inline CyclicRegularChain PreparedCyclicEquivariantChain::regular_chain(core::Grade grade,numeric::SparseRationalVector values,numeric::ExactActionGuard& guard) const{
    validate(guard);const auto size=machine::checked_size_mul(source().grade(grade).cell_count(),static_cast<std::size_t>(order()));
    if(values.size()!=size)throw std::invalid_argument("cyclic regular chain carrier differs");
    guard.entries(values.nonzeros());for(const auto& entry:values.entries()){guard.step();guard.value(entry.value);}
    return CyclicRegularChain(*this,grade,std::move(values));
}
inline CyclicRegularChain PreparedCyclicEquivariantChain::boundary(const CyclicRegularChain& input,numeric::ExactActionGuard& guard) const{
    if(!same_owner(input.owner()))throw std::invalid_argument("cyclic regular chain source owner differs");
    if(!input.grade().value)throw std::out_of_range("cyclic regular boundary needs a positive grade");
    validate(guard);const auto grade=input.grade();const auto lower=core::Grade{static_cast<std::uint16_t>(grade.value-1)};
    const auto& arena=source().grade(grade);const auto exists=source().signature().existence_aspect();
    std::map<std::size_t,numeric::Rational> result;const auto p=static_cast<std::size_t>(order());
    for(const auto& entry:input.values().entries()){
        const auto cell=entry.index/p,sheet=entry.index%p;const auto [begin,end]=arena.incidence_range(cell);
        for(std::size_t slot=begin;slot<end;++slot){
            guard.step();if(!arena.aspects.get(exists,slot))continue;
            const auto target_sheet=equivariant_detail::add_mod(sheet,state_->voltages[grade.value][slot],order());
            const auto index=machine::checked_size_add(machine::checked_size_mul(arena.lower_index[slot],p),static_cast<std::size_t>(target_sheet));
            const auto it=result.find(index);const auto old=it==result.end()?numeric::Rational{0}:it->second;
            auto value=guard.add(old,guard.mul(equivariant_detail::coefficient(*state_,grade,slot),entry.value));
            if(value==0)result.erase(index);else result.insert_or_assign(index,std::move(value));guard.entries(result.size());
        }
    }
    std::vector<numeric::SparseRationalEntry> values;values.reserve(result.size());
    for(auto& [index,value]:result)values.push_back({index,std::move(value)});
    return regular_chain(lower,numeric::SparseRationalVector(machine::checked_size_mul(source().grade(lower).cell_count(),p),std::move(values)),guard);
}

class CyclicEquivariantSpecialization {
    struct State {
        PreparedCyclicEquivariantChain source;
        numeric::AlgebraicElement character;
        std::vector<numeric::SparseAlgebraicMatrix> boundaries;
    };
public:
    const PreparedCyclicEquivariantChain& equivariant_source() const & noexcept{return state_->source;}
    const PreparedCyclicEquivariantChain& equivariant_source() const &&=delete;
    const numeric::AlgebraicElement& character() const & noexcept{return state_->character;}
    const numeric::AlgebraicElement& character() const &&=delete;
    const numeric::SparseAlgebraicMatrix& boundary(core::Grade grade) const &{return state_->boundaries.at(grade.value);}
    const numeric::SparseAlgebraicMatrix& boundary(core::Grade) const &&=delete;
    bool same_owner(const CyclicEquivariantSpecialization& other) const noexcept{return state_==other.state_;}
    std::vector<std::size_t> homology_dimensions(numeric::ExactActionGuard& guard) const{
        const auto& source=state_->source.source();std::vector<std::size_t> ranks(source.grade_count()+1,0),out;
        for(std::size_t k=1;k<source.grade_count();++k)ranks[k]=state_->boundaries[k].rank(guard);
        for(std::size_t k=0;k<source.grade_count();++k)out.push_back(source.grade(equivariant_detail::grade(k)).cell_count()-ranks[k]-ranks[k+1]);
        return out;
    }
    CyclicTorsionCertificate torsion(numeric::ExactActionGuard& guard) const;
private:
    friend class PreparedCyclicEquivariantChain;
    explicit CyclicEquivariantSpecialization(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};

inline CyclicEquivariantSpecialization PreparedCyclicEquivariantChain::specialize(const numeric::AlgebraicElement& character,numeric::ExactActionGuard& guard) const{
    validate(guard);const auto field=character.field();const auto one=field.scalar(numeric::Rational{1});
    equivariant_detail::algebraic_value(character,guard);
    if(character.power(order())!=one)throw std::invalid_argument("character does not respect the declared deck relation");
    std::vector<numeric::SparseAlgebraicMatrix> boundaries;boundaries.reserve(source().grade_count());
    for(std::size_t k=0;k<source().grade_count();++k){
        const auto cols=source().grade(equivariant_detail::grade(k)).cell_count();
        const auto rows=k?source().grade(equivariant_detail::grade(k-1)).cell_count():0;
        numeric::SparseAlgebraicMatrix boundary(rows,cols,field);
        for(const auto& [key,value]:state_->boundaries[k]){
            const auto [row,col,power]=key;guard.step();const auto term=field.scalar(value)*character.power(power);
            equivariant_detail::algebraic_value(term,guard);
            const auto sum=boundary.get(row,col)+term;equivariant_detail::algebraic_value(sum,guard);
            boundary.set(row,col,sum);
        }
        boundary.validate(guard);boundaries.push_back(std::move(boundary));
    }
    for(std::size_t k=2;k<boundaries.size();++k)
        if(boundaries[k-1].multiply(boundaries[k],guard).nonzeros())throw std::logic_error("character specialization lost the source chain certificate");
    return CyclicEquivariantSpecialization(std::make_shared<const CyclicEquivariantSpecialization::State>(
        CyclicEquivariantSpecialization::State{*this,character,std::move(boundaries)}));
}

// Based acyclic torsion with positive exponents in even grades.  Each
// determinant is [boundary-image basis, canonical lifts of the previous one]
// relative to the complete retained cell basis, so C1--a-->C0 contributes a.
class CyclicTorsionCertificate {
public:
    const CyclicEquivariantSpecialization& realization() const & noexcept{return realization_;}
    const CyclicEquivariantSpecialization& realization() const &&=delete;
    const numeric::AlgebraicElement& value() const & noexcept{return value_;}
    const numeric::AlgebraicElement& value() const &&=delete;
    const std::vector<std::vector<std::size_t>>& boundary_columns() const & noexcept{return columns_;}
    const std::vector<std::vector<std::size_t>>& boundary_columns() const &&=delete;
    const std::vector<numeric::AlgebraicElement>& basis_determinants() const & noexcept{return determinants_;}
    const std::vector<numeric::AlgebraicElement>& basis_determinants() const &&=delete;
    // Compare finite scalars modulo the declared cyclotomic units and Galois
    // action. This is not a classifier of arbitrary undeclared topological spaces.
    bool equivalent_under_cyclotomic_units(const CyclicTorsionCertificate& other,numeric::ExactActionGuard& guard) const{
        const auto p=realization_.equivariant_source().order();const auto field=value_.field();
        if(p!=other.realization_.equivariant_source().order()||!field.same_owner(other.value_.field()))
            throw std::invalid_argument("torsion comparison coefficient/deck owners differ");
        if(field.minimal_polynomial()!=numeric::cyclotomic_polynomial(p,field.limits()))
            throw std::invalid_argument("torsion orbit requires the declared full cyclotomic field");
        const auto z=field.generator();
        for(std::uint64_t u=0;u<p;++u){
            guard.step();if(std::gcd(u,p)!=1)continue;
            auto transformed=field.evaluate(value_.coordinates(),z.power(u));
            equivariant_detail::algebraic_value(transformed,guard);
            for(std::uint64_t k=0;k<p;++k){
                guard.step();if(transformed==other.value_||-transformed==other.value_)return true;
                transformed=transformed*z;equivariant_detail::algebraic_value(transformed,guard);
            }
        }
        return false;
    }
private:
    friend class CyclicEquivariantSpecialization;
    CyclicTorsionCertificate(CyclicEquivariantSpecialization realization,numeric::AlgebraicElement value,
        std::vector<std::vector<std::size_t>> columns,std::vector<numeric::AlgebraicElement> determinants)
        :realization_(std::move(realization)),value_(std::move(value)),columns_(std::move(columns)),determinants_(std::move(determinants)){}
    CyclicEquivariantSpecialization realization_;
    numeric::AlgebraicElement value_;
    std::vector<std::vector<std::size_t>> columns_;
    std::vector<numeric::AlgebraicElement> determinants_;
};

inline CyclicTorsionCertificate CyclicEquivariantSpecialization::torsion(numeric::ExactActionGuard& guard) const{
    const auto& source=state_->source.source();const auto field=state_->character.field();const auto grades=source.grade_count();
    std::vector<std::vector<std::size_t>> columns(grades+1);
    for(std::size_t k=1;k<grades;++k)columns[k]=state_->boundaries[k].column_basis(guard).columns;
    auto value=field.scalar(numeric::Rational{1});std::vector<numeric::AlgebraicElement> determinants;
    for(std::size_t k=0;k<grades;++k){
        const auto n=source.grade(equivariant_detail::grade(k)).cell_count();
        if(machine::checked_size_add(columns[k].size(),columns[k+1].size())!=n)
            throw std::domain_error("based torsion requires an acyclic specialized chain");
        numeric::SparseAlgebraicMatrix basis(n,n,field);
        if(k+1<grades){
            const auto boundary_basis=state_->boundaries[k+1].select_columns(columns[k+1],guard);
            for(const auto& [key,coefficient]:boundary_basis.entries())basis.set(key.first,key.second,coefficient);
        }
        for(std::size_t j=0;j<columns[k].size();++j)basis.set(columns[k][j],columns[k+1].size()+j,field.scalar(numeric::Rational{1}));
        const auto determinant=basis.determinant(guard);
        if(determinant.is_zero())throw std::logic_error("acyclic boundary/lift basis certificate failed");
        guard.step();value=k%2?value/determinant:value*determinant;equivariant_detail::algebraic_value(value,guard);
        determinants.push_back(determinant);
    }
    return CyclicTorsionCertificate(*this,std::move(value),std::move(columns),std::move(determinants));
}
} // namespace rfc::topology
