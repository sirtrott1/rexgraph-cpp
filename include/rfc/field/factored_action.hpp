#pragma once
#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <initializer_list>
#include <span>

namespace rfc::field {
namespace factored_action_detail {
using Sparse=numeric::SparseRationalVector;
using Guard=numeric::ExactActionGuard;
inline void validate(const Sparse& value,Guard& guard) {
    guard.dimension(value.size());guard.entries(value.nonzeros());
    for(const auto& entry:value.entries()){guard.step();guard.value(entry.value);}
}
inline Sparse scale(const Sparse& value,const numeric::Rational& q,Guard& guard) {
    validate(value,guard);guard.value(q);std::vector<numeric::SparseRationalEntry> entries;
    for(const auto& entry:value.entries()){
        auto result=guard.mul(entry.value,q);if(result!=0)entries.push_back({entry.index,std::move(result)});
        guard.entries(machine::checked_size_add(value.nonzeros(),entries.size()));
    }
    return Sparse(value.size(),std::move(entries));
}
inline numeric::RationalVector scale(numeric::RationalVector value,const numeric::Rational& q,Guard& guard) {
    guard.vector(value);guard.value(q);for(auto& x:value)x=guard.mul(x,q);return value;
}
inline Sparse add(const Sparse& a,const Sparse& b,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("sparse field sum dimension");
    validate(a,guard);validate(b,guard);std::vector<numeric::SparseRationalEntry> result;
    std::size_t i=0,j=0;const auto& x=a.entries();const auto& y=b.entries();
    const auto retained=machine::checked_size_add(x.size(),y.size());
    while(i<x.size() || j<y.size()){
        guard.step();numeric::SparseRationalEntry entry;
        if(j==y.size() || (i<x.size() && x[i].index<y[j].index))entry=x[i++];
        else if(i==x.size() || y[j].index<x[i].index)entry=y[j++];
        else {entry={x[i].index,guard.add(x[i].value,y[j].value)};++i;++j;}
        if(entry.value!=0)result.push_back(std::move(entry));
        guard.entries(machine::checked_size_add(retained,result.size()));
    }
    return Sparse(a.size(),std::move(result));
}
inline numeric::RationalVector add(numeric::RationalVector a,const numeric::RationalVector& b,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("field sum dimension");
    guard.vector(a);guard.vector(b);
    for(std::size_t i=0;i<a.size();++i)a[i]=guard.add(a[i],b[i]);
    return a;
}
inline numeric::Rational dot(const Sparse& a,const Sparse& b,Guard& guard) {
    if(a.size()!=b.size())throw std::invalid_argument("sparse pairing dimension");
    validate(a,guard);validate(b,guard);numeric::Rational result=0;std::size_t i=0,j=0;
    while(i<a.nonzeros() && j<b.nonzeros()){
        guard.step();const auto& x=a.entries()[i];const auto& y=b.entries()[j];
        if(x.index<y.index)++i;else if(y.index<x.index)++j;
        else {result=guard.add(result,guard.mul(x.value,y.value));++i;++j;}
    }
    return result;
}
inline bool equal(const Sparse& a,const Sparse& b) {return a.size()==b.size() && a.entries()==b.entries();}
} // namespace factored_action_detail

// Immutable exact actions have dense and sparse ports. Dense ports are an
// explicit requested coordinate realization; sparse ports never expand zeros.
// The same retained factors and their algebraic transposes serve both ports.
class FactoredAction {
public:
    using Vector=numeric::RationalVector;
    using Sparse=numeric::SparseRationalVector;
    using Guard=numeric::ExactActionGuard;
private:
    struct State {
        std::size_t rows,cols,entries;
        std::function<Vector(const Vector&,Guard&)> forward,transpose;
        std::function<Sparse(const Sparse&,Guard&)> sparse_forward,sparse_transpose;
    };
    std::shared_ptr<const State> state_;
    explicit FactoredAction(State value):state_(std::make_shared<const State>(std::move(value))){}
public:
    template<class Owner,class Forward,class Transpose>
    static FactoredAction owned(Owner owner,std::size_t rows,std::size_t cols,std::size_t entries,
                                Forward forward,Transpose transpose) {
        auto pin=std::make_shared<const Owner>(std::move(owner));
        return FactoredAction({rows,cols,entries,
            [pin,forward](const Vector& x,Guard& g){return forward(*pin,x,g);},
            [pin,transpose](const Vector& x,Guard& g){return transpose(*pin,x,g);},
            [pin,forward](const Sparse& x,Guard& g){return forward(*pin,x,g);},
            [pin,transpose](const Sparse& x,Guard& g){return transpose(*pin,x,g);}});
    }
    static FactoredAction matrix(numeric::ExactMatrix value) {
        const auto rows=value.rows(),cols=value.cols(),entries=value.nonzeros();
        return owned(std::move(value),rows,cols,entries,
            [](const auto& a,const auto& x,auto& g){return numeric::checked_apply(a,x,g);},
            [](const auto& a,const auto& x,auto& g){return numeric::checked_transpose_apply(a,x,g);});
    }
    static FactoredAction solve(numeric::PreparedExactSolve value) {
        const auto n=value.matrix().rows(),entries=value.stored_entries();
        return owned(std::move(value),n,n,entries,
            [](const auto& a,const auto& x,auto& g){return a.apply(x,g);},
            [](const auto& a,const auto& x,auto& g){return a.apply_transpose(x,g);});
    }
    static FactoredAction identity(std::size_t n) {
        return owned(n,n,n,0,[](auto,const auto& x,auto&){return x;},[](auto,const auto& x,auto&){return x;});
    }
    std::size_t rows() const noexcept {return state_->rows;}
    std::size_t cols() const noexcept {return state_->cols;}
    std::size_t retained_entries() const noexcept {return state_->entries;}
    Vector apply(Vector input,Guard& guard) const {
        const auto pin=state_;if(input.size()!=pin->cols)throw std::invalid_argument("factored action input dimension");
        guard.dimension(pin->rows);guard.entries(machine::checked_size_add(pin->entries,machine::checked_size_add(pin->rows,pin->cols)));
        input=guard.canonical_input(input);auto out=pin->forward(input,guard);
        if(out.size()!=pin->rows)throw std::logic_error("factored action output dimension");
        guard.vector(out);return out;
    }
    Sparse apply(const Sparse& input,Guard& guard) const {
        const auto pin=state_;if(input.size()!=pin->cols)throw std::invalid_argument("factored sparse input dimension");
        guard.dimension(pin->rows);factored_action_detail::validate(input,guard);
        guard.entries(machine::checked_size_add(pin->entries,input.nonzeros()));auto out=pin->sparse_forward(input,guard);
        if(out.size()!=pin->rows)throw std::logic_error("factored sparse output dimension");
        factored_action_detail::validate(out,guard);
        guard.entries(machine::checked_size_add(pin->entries,machine::checked_size_add(input.nonzeros(),out.nonzeros())));return out;
    }
    Vector transpose_apply(Vector input,Guard& guard) const {
        const auto pin=state_;if(input.size()!=pin->rows)throw std::invalid_argument("factored transpose input dimension");
        guard.dimension(pin->cols);guard.entries(machine::checked_size_add(pin->entries,machine::checked_size_add(pin->rows,pin->cols)));
        input=guard.canonical_input(input);auto out=pin->transpose(input,guard);
        if(out.size()!=pin->cols)throw std::logic_error("factored transpose output dimension");
        guard.vector(out);return out;
    }
    Sparse transpose_apply(const Sparse& input,Guard& guard) const {
        const auto pin=state_;if(input.size()!=pin->rows)throw std::invalid_argument("factored sparse transpose input dimension");
        guard.dimension(pin->cols);factored_action_detail::validate(input,guard);
        guard.entries(machine::checked_size_add(pin->entries,input.nonzeros()));auto out=pin->sparse_transpose(input,guard);
        if(out.size()!=pin->cols)throw std::logic_error("factored sparse transpose output dimension");
        factored_action_detail::validate(out,guard);
        guard.entries(machine::checked_size_add(pin->entries,machine::checked_size_add(input.nonzeros(),out.nonzeros())));return out;
    }
    Vector apply(Vector input,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(std::move(input),guard);
    }
    Sparse apply(const Sparse& input,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(input,guard);
    }
    // A braced coordinate list keeps the established full-vector meaning.
    // Sparse input always names its logical carrier and supported entries.
    Vector apply(std::initializer_list<numeric::Rational> input,Guard& guard) const {
        return apply(Vector(input),guard);
    }
    Vector apply(std::initializer_list<numeric::Rational> input,
                 numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return apply(Vector(input),guard);
    }
    Vector transpose_apply(Vector input,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return transpose_apply(std::move(input),guard);
    }
    Sparse transpose_apply(const Sparse& input,numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return transpose_apply(input,guard);
    }
    Vector transpose_apply(std::initializer_list<numeric::Rational> input,Guard& guard) const {
        return transpose_apply(Vector(input),guard);
    }
    Vector transpose_apply(std::initializer_list<numeric::Rational> input,
                 numeric::ExactActionLimits limits={},std::function<void()> check={}) const {
        Guard guard(limits,std::move(check));return transpose_apply(Vector(input),guard);
    }
    FactoredAction transpose() const {
        return owned(*this,cols(),rows(),retained_entries(),
            [](const auto& a,const auto& x,auto& g){return a.transpose_apply(x,g);},
            [](const auto& a,const auto& x,auto& g){return a.apply(x,g);});
    }
    FactoredAction then(FactoredAction next) const {
        if(rows()!=next.cols())throw std::invalid_argument("factored composition dimension");
        const auto rows_out=next.rows(),entries=machine::checked_size_add(retained_entries(),next.retained_entries());
        return owned(std::pair{*this,std::move(next)},rows_out,cols(),entries,
            [](const auto& a,const auto& x,auto& g){return a.second.apply(a.first.apply(x,g),g);},
            [](const auto& a,const auto& x,auto& g){return a.first.transpose_apply(a.second.transpose_apply(x,g),g);});
    }
    FactoredAction scaled(numeric::Rational scale) const {
        scale=numeric::canonical_rational(scale);
        return owned(std::pair{*this,std::move(scale)},rows(),cols(),machine::checked_size_add(retained_entries(),1),
            [](const auto& a,const auto& x,auto& g){return factored_action_detail::scale(a.first.apply(x,g),a.second,g);},
            [](const auto& a,const auto& x,auto& g){return factored_action_detail::scale(a.first.transpose_apply(x,g),a.second,g);});
    }
    FactoredAction plus(FactoredAction other) const {
        if(rows()!=other.rows() || cols()!=other.cols())throw std::invalid_argument("factored sum dimension");
        const auto entries=machine::checked_size_add(retained_entries(),other.retained_entries());
        return owned(std::pair{*this,std::move(other)},rows(),cols(),entries,
            [](const auto& a,const auto& x,auto& g){return factored_action_detail::add(a.first.apply(x,g),a.second.apply(x,g),g);},
            [](const auto& a,const auto& x,auto& g){return factored_action_detail::add(a.first.transpose_apply(x,g),a.second.transpose_apply(x,g),g);});
    }
    Vector selected(const Sparse& input,std::span<const std::size_t> outputs,Guard& guard) const {
        const auto pin=*this;factored_action_detail::validate(input,guard);
        if(input.size()!=cols())throw std::invalid_argument("selected sparse action input dimension");
        guard.dimension(outputs.size());Vector out;out.reserve(outputs.size());
        for(const auto row:outputs){
            if(row>=rows())throw std::out_of_range("selected action output");
            const auto dual=pin.transpose_apply(Sparse::basis(rows(),row),guard);
            out.push_back(factored_action_detail::dot(dual,input,guard));guard.entries(out.size());
        }
        return out;
    }
    Vector selected(Vector input,std::span<const std::size_t> outputs,Guard& guard) const {
        input=guard.canonical_input(input);std::vector<numeric::SparseRationalEntry> entries;
        for(std::size_t i=0;i<input.size();++i){guard.step();if(input[i]!=0)entries.push_back({i,input[i]});}
        return selected(Sparse(input.size(),std::move(entries)),outputs,guard);
    }
    Vector selected(std::initializer_list<numeric::Rational> input,
                    std::span<const std::size_t> outputs,Guard& guard) const {
        return selected(Vector(input),outputs,guard);
    }
};
} // namespace rfc::field
