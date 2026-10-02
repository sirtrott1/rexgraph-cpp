#pragma once
#include "rfc/geometry/chain_map.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <memory>
#include <vector>

namespace rfc::geometry {

inline numeric::ExactMatrix materialize_chain_map_grade(const RelationalChainMap& map,core::Grade grade){
    numeric::ExactMatrix out(map.target().grade(grade).cell_count(),map.source().grade(grade).cell_count());
    for(std::size_t j=0;j<map.source().grade(grade).cell_count();++j){
        for(const auto& term:map.image(grade,j)){
            const auto old=out.get(term.target,j);
            out.set(term.target,j,old+term.coefficient);
        }
    }
    return out;
}

inline numeric::ExactMatrix materialize_relational_boundary(const core::RelationalComplex& complex,core::Grade grade){
    if(grade.value==0 || grade.value>=complex.grade_count()) throw std::out_of_range("chain homotopy boundary grade");
    const auto& arena=complex.grade(grade);
    numeric::ExactMatrix out(complex.grade(core::Grade{static_cast<std::uint16_t>(grade.value-1u)}).cell_count(),arena.cell_count());
    const auto exists=complex.signature().existence_aspect();
    for(std::size_t upper=0;upper<arena.cell_count();++upper){
        const auto [begin,end]=arena.incidence_range(upper);
        for(std::size_t p=begin;p<end;++p){
            if(!arena.aspects.get(exists,p)) continue;
            const auto value=complex.incidence_coefficient(grade,p);
            if(value!=0) out.set(arena.lower_index[p],upper,out.get(arena.lower_index[p],upper)+value);
        }
    }
    return out;
}

// H_k:C_k -> D_{k+1}. The final source grade may map to a zero-width target
// grade when D has no corresponding upper grade.
struct ChainHomotopyCertificate {
    bool endpoints_are_chain_maps{};
    bool exact_identity{};
    std::vector<numeric::ExactMatrix> residuals;
};

inline ChainHomotopyCertificate certify_chain_homotopy(
    const RelationalChainMap& F,
    const RelationalChainMap& G,
    const std::vector<numeric::ExactMatrix>& H) {
    if(&F.source()!=&G.source() || &F.target()!=&G.target()) throw std::invalid_argument("chain homotopy endpoint map mismatch");
    const auto& C=F.source();
    const auto& D=F.target();
    if(H.size()!=C.grade_count()) throw std::invalid_argument("chain homotopy grade count mismatch");
    const bool endpoints=F.is_chain_map() && G.is_chain_map();
    std::vector<numeric::ExactMatrix> residuals;
    residuals.reserve(C.grade_count());
    bool exact=endpoints;
    for(std::size_t k=0;k<C.grade_count();++k){
        const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
        const auto Fk=materialize_chain_map_grade(F,grade);
        const auto Gk=materialize_chain_map_grade(G,grade);
        auto rhs=numeric::ExactMatrix(D.grade(grade).cell_count(),C.grade(grade).cell_count());
        if(k+1u<D.grade_count()){
            if(H[k].rows()!=D.grade(core::Grade{static_cast<std::uint16_t>(k+1u)}).cell_count() || H[k].cols()!=C.grade(grade).cell_count()) {
                throw std::invalid_argument("chain homotopy H_k dimension mismatch");
            }
            const auto BD=materialize_relational_boundary(D,core::Grade{static_cast<std::uint16_t>(k+1u)});
            rhs=rhs.add(BD.multiply(H[k]));
        }else if(H[k].rows()!=0 || H[k].cols()!=C.grade(grade).cell_count()){
            throw std::invalid_argument("top chain homotopy map must have zero target dimension");
        }
        if(k>0){
            if(H[k-1u].rows()!=D.grade(grade).cell_count() || H[k-1u].cols()!=C.grade(core::Grade{static_cast<std::uint16_t>(k-1u)}).cell_count()) {
                throw std::invalid_argument("chain homotopy H_{k-1} dimension mismatch");
            }
            const auto BC=materialize_relational_boundary(C,grade);
            rhs=rhs.add(H[k-1u].multiply(BC));
        }
        auto residual=Gk.add(Fk.scale(numeric::Rational{-1})).add(rhs.scale(numeric::Rational{-1}));
        if(residual.nonzeros()!=0) exact=false;
        residuals.push_back(std::move(residual));
    }
    return {endpoints,exact,std::move(residuals)};
}

// Owns the declared coordinate maps and both actual carriers. This is a chain
// homotopy certificate; it does not manufacture a structural cylinder morphism.
class PreparedChainHomotopy {
public:
    PreparedChainHomotopy(std::shared_ptr<const core::RelationalComplex> source,
        std::shared_ptr<const core::RelationalComplex> target,
        std::vector<LinearGradeMap> first,std::vector<LinearGradeMap> second,
        std::vector<numeric::ExactMatrix> homotopy,std::size_t max_coordinate_slots=16000000) {
        if(!source || !target)throw std::invalid_argument("homotopy requires both immutable carrier owners");
        std::size_t slots{};
        if(source->grade_count()>target->grade_count())throw std::invalid_argument("homotopy target grade mismatch");
        for(std::size_t k=0;k<source->grade_count();++k){
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            slots=machine::checked_size_add(slots,machine::checked_size_mul(source->grade(grade).cell_count(),target->grade(grade).cell_count()));
        }
        for(const auto& matrix:homotopy)slots=machine::checked_size_add(slots,machine::checked_size_mul(matrix.rows(),matrix.cols()));
        for(const auto* maps:{&first,&second})for(const auto& grade:*maps)for(const auto& cell:grade)
            slots=machine::checked_size_add(slots,cell.size());
        if(slots>max_coordinate_slots)throw std::length_error("homotopy coordinate budget");
        state_=std::make_shared<const State>(std::move(source),std::move(target),std::move(first),std::move(second),std::move(homotopy));
    }
    ChainHomotopyCertificate certificate() const {return state_->certificate;}
    const RelationalChainMap& first() const & {return state_->first;}
    const RelationalChainMap& second() const & {return state_->second;}
    const RelationalChainMap& first() const &&=delete;
    const RelationalChainMap& second() const &&=delete;
private:
    struct State {
        std::shared_ptr<const core::RelationalComplex> source,target;
        RelationalChainMap first,second;
        std::vector<numeric::ExactMatrix> homotopy;
        ChainHomotopyCertificate certificate;
        State(std::shared_ptr<const core::RelationalComplex> c,std::shared_ptr<const core::RelationalComplex> d,
            std::vector<LinearGradeMap> f,std::vector<LinearGradeMap> g,std::vector<numeric::ExactMatrix> h)
            :source(std::move(c)),target(std::move(d)),first(*source,*target,std::move(f)),second(*source,*target,std::move(g)),
             homotopy(std::move(h)),certificate(certify_chain_homotopy(first,second,homotopy)){}
    };
    std::shared_ptr<const State> state_;
};

} // namespace rfc::geometry
