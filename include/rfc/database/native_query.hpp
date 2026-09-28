#pragma once
#include "rfc/database/native_interpretation.hpp"

namespace rfc::database {
enum class NativeTruth : std::uint8_t {False,True,Unknown};
enum class ExactPredicateKind : std::uint8_t {Equal,NotEqual,Less,LessEqual,Greater,GreaterEqual,IsNull,IsNotNull};
struct ExactScalarPredicate {
    std::vector<std::uint64_t> factors;
    ExactPredicateKind operation{ExactPredicateKind::Equal};
    composite::DeclaredValue literal;
};
namespace native_query_detail {
using interpretation_detail::require;
struct Scalar {
    composite::DeclarationId selected,element;
    composite::DeclarationKind kind;
    bool nullable{};
};
inline Scalar scalar(const InterpretationSchema& schema,std::uint64_t kind,const std::vector<std::uint64_t>& path){
    const auto it=std::find_if(schema.assertions.begin(),schema.assertions.end(),[&](const auto& a){return a.key==kind;});require(it!=schema.assertions.end(),"query assertion kind absent");
    auto type=it->payload;for(const auto factor:path){const auto& d=schema.values.at(type);require(d.kind==composite::DeclarationKind::Product,"query factor requires a product");
        const auto f=std::find_if(d.factors.begin(),d.factors.end(),[&](const auto& x){return x.key==factor;});require(f!=d.factors.end(),"query factor absent");type=f->type;}
    const auto& selected=schema.values.at(type);auto element=type;const bool nullable=selected.kind==composite::DeclarationKind::Optional;if(nullable)element=selected.element;
    const auto k=schema.values.at(element).kind;
    require(k==composite::DeclarationKind::Integer || k==composite::DeclarationKind::Rational || k==composite::DeclarationKind::Boolean || k==composite::DeclarationKind::Finite,
            "bounded scalar query does not coerce nested or byte-string values");return {type,element,k,nullable};
}
inline void validate(const InterpretationSchema& schema,const Scalar& s,const ExactScalarPredicate& p){
    require(p.operation<=ExactPredicateKind::IsNotNull,"unknown exact predicate");
    if(p.operation==ExactPredicateKind::IsNull || p.operation==ExactPredicateKind::IsNotNull){require(s.nullable,"null predicate requires an optional scalar declaration");return;}
    const composite::CompiledLayout layout(schema.values,s.element);(void)layout.allocation(p.literal);
    if(s.kind==composite::DeclarationKind::Boolean || s.kind==composite::DeclarationKind::Finite)
        require(p.operation==ExactPredicateKind::Equal || p.operation==ExactPredicateKind::NotEqual,"no implicit order on Boolean/finite values");
}
inline NativeTruth evaluate(const Scalar& s,const ExactScalarPredicate& predicate,const composite::DeclaredValue& selected){
    const auto* value=&selected;bool null=false;
    if(s.nullable){const auto& option=std::get<composite::DeclaredValue::Optional>(selected.data);null=option.element.empty();if(!null)value=&option.element.at(0);}
    if(predicate.operation==ExactPredicateKind::IsNull)return null?NativeTruth::True:NativeTruth::False;
    if(predicate.operation==ExactPredicateKind::IsNotNull)return null?NativeTruth::False:NativeTruth::True;
    if(null)return NativeTruth::Unknown;
    int compare=0;
    if(s.kind==composite::DeclarationKind::Integer)compare=mpz_cmp(std::get<numeric::Integer>(value->data).get_mpz_t(),std::get<numeric::Integer>(predicate.literal.data).get_mpz_t());
    else if(s.kind==composite::DeclarationKind::Rational)compare=mpq_cmp(std::get<numeric::Rational>(value->data).get_mpq_t(),std::get<numeric::Rational>(predicate.literal.data).get_mpq_t());
    else compare=composite::declared_value_equal(*value,predicate.literal)?0:1;
    bool result=false;switch(predicate.operation){
        case ExactPredicateKind::Equal:result=compare==0;break;case ExactPredicateKind::NotEqual:result=compare!=0;break;
        case ExactPredicateKind::Less:result=compare<0;break;case ExactPredicateKind::LessEqual:result=compare<=0;break;
        case ExactPredicateKind::Greater:result=compare>0;break;case ExactPredicateKind::GreaterEqual:result=compare>=0;break;
        default:throw std::invalid_argument("unsupported scalar operation");}
    return result?NativeTruth::True:NativeTruth::False;
}
} // namespace native_query_detail
// A bounded native predicate, not a parser or a compiler for arbitrary SQL.
// Input/output are retained assertion keys; duplicates remain distinct.
inline std::vector<std::uint64_t> select_exact(const InterpretedSnapshot& source,std::uint64_t kind,
                                              const ExactScalarPredicate& predicate,std::size_t max_work=100000){
    const auto scalar=native_query_detail::scalar(source.schema(),kind,predicate.factors);native_query_detail::validate(source.schema(),scalar,predicate);
    const auto& keys=source.assertions(kind);interpretation_detail::bound(keys.size(),max_work,"native selection work budget");std::vector<std::uint64_t> out;
    for(const auto key:keys) {
        if(native_query_detail::evaluate(scalar,predicate,*source.payload(key,predicate.factors).declared_value())==NativeTruth::True) out.push_back(key);
    }
    return out;
}
inline std::optional<numeric::Rational> sum_exact(const InterpretedSnapshot& source,std::uint64_t kind,
                                                 const std::vector<std::uint64_t>& factors,std::size_t max_work=100000){
    const auto s=native_query_detail::scalar(source.schema(),kind,factors);
    native_query_detail::require(s.kind==composite::DeclarationKind::Integer || s.kind==composite::DeclarationKind::Rational,"sum requires a declared numeric scalar");
    const auto& keys=source.assertions(kind);interpretation_detail::bound(keys.size(),max_work,"native sum work budget");std::optional<numeric::Rational> result;
    for(const auto key:keys){const auto pin=source.payload(key,factors);const auto* value=pin.declared_value().get();
        if(s.nullable){const auto& option=std::get<composite::DeclaredValue::Optional>(value->data);if(option.element.empty())continue;value=&option.element.at(0);}
        if(!result)result=numeric::Rational{0};
        if(s.kind==composite::DeclarationKind::Integer)*result+=std::get<numeric::Integer>(value->data);else *result+=std::get<numeric::Rational>(value->data);
    }
    if(result) result->canonicalize();
    return result;
}
} // namespace rfc::database
