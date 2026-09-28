#pragma once

#include "rfc/composite/layout.hpp"

namespace rfc::composite {

enum class DeclarationMapKind : std::uint8_t { Recode = 0, Embedding = 1 };
struct DeclarationFactorFill {
    std::uint64_t factor{};
    DeclaredValue value;
};
struct DeclarationMap {
    DeclarationId from, to;
    DeclarationMapKind kind{DeclarationMapKind::Recode};
    // Only added Product factors. Explicit values, never inferred null/defaults.
    std::vector<DeclarationFactorFill> fills;
};

// An owning, checked map of the complete declaration DAG. A Recode is a
// bijective change of representation; Embedding preserves all old values and
// permits explicitly supported carrier growth. Neither is a lossy quotient.
class DeclarationRecoding {
public:
    DeclarationRecoding(DeclarationRegistry base, DeclarationRegistry target,
                        std::vector<DeclarationMap> maps, DeclarationLimits limits = {})
        : base_(std::move(base)), target_(std::move(target)), maps_(std::move(maps)), limits_(limits) {
        using namespace declaration_detail;
        limits_.validate();
        // Revalidate under this operation's policy, including user-supplied registries.
        (void)DeclarationRegistry(base_.definitions(), limits_);
        (void)DeclarationRegistry(target_.definitions(), limits_);
        require(maps_.size() == base_.definitions().size(), "declaration recoding must cover the entire base DAG");
        std::set<DeclarationId> targets;
        for (std::size_t i = 0; i < maps_.size(); ++i) {
            const auto& m = maps_[i];
            require(m.kind <= DeclarationMapKind::Embedding && by_id_.emplace(m.from, i).second &&
                    targets.insert(m.to).second, "invalid/noninjective declaration recoding");
            (void)base_.at(m.from); (void)target_.at(m.to);
        }
        for (const auto& d : base_.definitions()) {
            for (const auto& t : target_.definitions())
                if (t.id == d.id) require(t == d, "immutable declaration identity redefined across endpoints");
            validate_map(by_id_.at(d.id));
        }
    }
    [[nodiscard]] DeclarationId target_id(DeclarationId id) const { return maps_.at(by_id_.at(id)).to; }
    [[nodiscard]] DeclaredValue apply(DeclarationId id, const DeclaredValue& value) const {
        CompiledLayout(base_, id, BooleanLayoutProfile::Lsb0OctetsV1, limits_).validate(value);
        std::size_t nodes=0, materialized=0;
        (void)preflight(id,value,nodes,materialized);
        auto result = recode(id, value);
        CompiledLayout(target_, target_id(id), BooleanLayoutProfile::Lsb0OctetsV1, limits_).validate(result);
        return result;
    }
private:
    void validate_map(std::size_t slot) const {
        using namespace declaration_detail;
        const auto& m = maps_[slot]; const auto& a = base_.at(m.from); const auto& b = target_.at(m.to);
        const bool grow = m.kind == DeclarationMapKind::Embedding;
        if (!grow) {
            for (const auto& f : a.factors)
                require(maps_.at(by_id_.at(f.type)).kind == DeclarationMapKind::Recode, "recode contains an embedding child");
            if (a.element.valid()) require(maps_.at(by_id_.at(a.element)).kind == DeclarationMapKind::Recode,
                                          "recode contains an embedding element");
        }
        require(a.kind == b.kind, "declaration recoding changes value constructor");
        require(a.kind == DeclarationKind::Product || m.fills.empty(), "unexpected factor initialization lane");
        switch (a.kind) {
        case DeclarationKind::Boolean: case DeclarationKind::Integer: case DeclarationKind::Rational: break;
        case DeclarationKind::Finite:
            require(grow || a.symbols.size() == b.symbols.size(), "recode changes symbol population");
            for (auto x : a.symbols) require(std::find(b.symbols.begin(), b.symbols.end(), x) != b.symbols.end(), "removed finite symbol");
            break;
        case DeclarationKind::Product: case DeclarationKind::Alternative: {
            require(grow || a.factors.size() == b.factors.size(), "recode changes factor/case population");
            std::set<std::uint64_t> old;
            for (const auto& f : a.factors) {
                old.insert(f.key);
                const auto it = std::find_if(b.factors.begin(), b.factors.end(), [&](const auto& g){return f.key == g.key;});
                require(it != b.factors.end() && it->grade == f.grade && it->type == target_id(f.type),
                        "factor identity/type/grade not preserved");
            }
            std::set<std::uint64_t> initialized;
            for (const auto& fill : m.fills) {
                require(!old.contains(fill.factor) && initialized.insert(fill.factor).second, "invalid added-factor initialization");
                const auto it = std::find_if(b.factors.begin(), b.factors.end(), [&](const auto& f){return f.key == fill.factor;});
                require(it != b.factors.end(), "initialization of nonexistent factor");
                CompiledLayout(target_, it->type, BooleanLayoutProfile::Lsb0OctetsV1, limits_).validate(fill.value);
            }
            if (a.kind == DeclarationKind::Product)
                require(old.size() + initialized.size() == b.factors.size(), "added product factor lacks explicit value");
            break;
        }
        case DeclarationKind::Optional:
            require(target_id(a.element) == b.element, "optional element correspondence mismatch"); break;
        case DeclarationKind::Repeated:
            require(target_id(a.element) == b.element &&
                    (grow ? (b.min_count <= a.min_count && b.max_count >= a.max_count)
                          : (b.min_count == a.min_count && b.max_count == a.max_count)), "repeated domain not preserved"); break;
        case DeclarationKind::RationalTensor:
            require(a.axes.size() == b.axes.size(), "tensor recoding cannot add/remove an axis");
            for (const auto& x : a.axes) {
                const auto it = std::find_if(b.axes.begin(), b.axes.end(), [&](const auto& y){return x.key == y.key;});
                require(it != b.axes.end() && x.coordinates.empty() == it->coordinates.empty() &&
                        (grow ? it->extent >= x.extent : it->extent == x.extent), "tensor axis correspondence mismatch");
                for (auto c : x.coordinates)
                    require(std::find(it->coordinates.begin(), it->coordinates.end(), c) != it->coordinates.end(), "tensor coordinate removed");
            }
            break;
        default: throw std::invalid_argument("unknown declaration recoding constructor");
        }
    }
    std::size_t preflight(DeclarationId id,const DeclaredValue& value,
                          std::size_t& nodes,std::size_t& materialized) const {
        using namespace declaration_detail;
        const auto& m=maps_.at(by_id_.at(id));const auto& a=base_.at(id);const auto& b=target_.at(m.to);
        const auto work=[&](std::size_t n){nodes=machine::checked_size_add(nodes,n);budget(nodes,limits_.max_value_nodes,"recoding aggregate work budget");};
        const auto allocate=[&](std::size_t n,std::size_t size){materialized=machine::checked_size_add(materialized,machine::checked_size_mul(n,size));budget(materialized,limits_.max_materialized_bytes,"recoding aggregate buffer budget");};
        const auto charge=[&](const LayoutAllocation& x){work(x.value_nodes);allocate(x.materialized_bytes,1);};
        std::size_t bits=0;
        const auto add=[&](std::size_t n){bits=machine::checked_size_add(bits,n);budget(machine::ceil_div(bits,8),limits_.max_encoded_bytes,"recoding encoded size budget");};
        switch(a.kind) {
        case DeclarationKind::Boolean:case DeclarationKind::Finite:case DeclarationKind::Integer:case DeclarationKind::Rational: {
            const auto x=CompiledLayout(target_,m.to,BooleanLayoutProfile::Lsb0OctetsV1,limits_).allocation(value);charge(x);add(x.bits);break;
        }
        case DeclarationKind::Product: {
            work(1);allocate(b.factors.size(),sizeof(DeclaredValue));
            for(const auto& f:b.factors){
                const auto it=std::find_if(a.factors.begin(),a.factors.end(),[&](const auto& x){return x.key==f.key;});
                if(it!=a.factors.end())add(preflight(it->type,std::get<DeclaredValue::Product>(value.data).elements.at(static_cast<std::size_t>(it-a.factors.begin())),nodes,materialized));
                else {
                    const auto fill=std::find_if(m.fills.begin(),m.fills.end(),[&](const auto& x){return x.factor==f.key;});
                    const auto x=CompiledLayout(target_,f.type,BooleanLayoutProfile::Lsb0OctetsV1,limits_).allocation(fill->value);charge(x);add(x.bits);
                }
            }break;
        }
        case DeclarationKind::Alternative: {
            work(1);allocate(1,sizeof(DeclaredValue));add(tag_bits(b.factors.size()));
            const auto& x=std::get<DeclaredValue::Alternative>(value.data);
            const auto it=std::find_if(a.factors.begin(),a.factors.end(),[&](const auto& f){return f.key==x.key;});
            add(preflight(it->type,x.element.front(),nodes,materialized));break;
        }
        case DeclarationKind::Optional: {
            work(1);add(1);const auto& xs=std::get<DeclaredValue::Optional>(value.data).element;
            allocate(xs.size(),sizeof(DeclaredValue));if(!xs.empty())add(preflight(a.element,xs.front(),nodes,materialized));break;
        }
        case DeclarationKind::Repeated: {
            work(1);const auto& xs=std::get<DeclaredValue::Repeated>(value.data).elements;
            allocate(xs.size(),sizeof(DeclaredValue));add(machine::checked_size_mul(varuint_bytes(xs.size()),8));
            for(const auto& x:xs)add(preflight(a.element,x,nodes,materialized));
            break;
        }
        case DeclarationKind::RationalTensor: {
            const auto& x=std::get<numeric::SparseRationalTensor>(value.data);
            const auto domain=DeclarationRegistry::tensor_domain(b);
            work(machine::checked_size_add(1,machine::checked_size_add(x.support().count(),x.stored_nonzeros())));
            allocate(x.support().count(),sizeof(std::size_t));allocate(x.stored_nonzeros(),sizeof(numeric::SparseRationalEntry));
            add(machine::checked_size_mul(varuint_bytes(x.support().count()),8));
            x.support().for_each_active([&](auto i){add(machine::checked_size_mul(varuint_bytes(tensor_position(a,b,x.domain(),domain,i)),8));});
            add(machine::checked_size_mul(varuint_bytes(x.stored_nonzeros()),8));
            for(const auto& e:x.values().entries()) {
                add(machine::checked_size_mul(varuint_bytes(tensor_position(a,b,x.domain(),domain,e.index)),8));add(rational_bits(e.value,limits_));
                allocate(machine::ceil_div(integer_bits(e.value.get_num()),8),1);allocate(machine::ceil_div(integer_bits(e.value.get_den()),8),1);
            }break;
        }
        default:throw std::invalid_argument("unknown recoding preflight constructor");
        }
        return bits;
    }
    static std::size_t tensor_position(const Declaration& a,const Declaration& b,const numeric::IndexDomain& old,
                                       const numeric::IndexDomain& target,std::size_t i) {
        const auto coord=old.unflatten(i);std::vector<std::size_t> mapped(b.axes.size());
        for(std::size_t j=0;j<b.axes.size();++j) {
            const auto& axis=b.axes[j];
            const auto it=std::find_if(a.axes.begin(),a.axes.end(),[&](const auto& x){return x.key==axis.key;});
            const auto x=coord.at(static_cast<std::size_t>(it-a.axes.begin()));
            if(axis.coordinates.empty())mapped[j]=x;
            else mapped[j]=static_cast<std::size_t>(std::find(axis.coordinates.begin(),axis.coordinates.end(),it->coordinates.at(x))-axis.coordinates.begin());
        }
        return target.flatten(mapped);
    }
    DeclaredValue recode(DeclarationId id, const DeclaredValue& v) const {
        const auto& m = maps_.at(by_id_.at(id)); const auto& a = base_.at(id); const auto& b = target_.at(m.to);
        switch (a.kind) {
        case DeclarationKind::Boolean: case DeclarationKind::Finite:
        case DeclarationKind::Integer: case DeclarationKind::Rational: return v;
        case DeclarationKind::Product: {
            std::vector<DeclaredValue> result; result.reserve(b.factors.size());
            for (const auto& f : b.factors) {
                const auto it = std::find_if(a.factors.begin(), a.factors.end(), [&](const auto& x){return x.key == f.key;});
                if (it != a.factors.end()) result.push_back(recode(it->type, std::get<DeclaredValue::Product>(v.data).elements.at(static_cast<std::size_t>(it-a.factors.begin()))));
                else {
                    const auto fill = std::find_if(m.fills.begin(), m.fills.end(), [&](const auto& x){return x.factor == f.key;});
                    result.push_back(fill->value);
                }
            }
            return DeclaredValue::product(std::move(result));
        }
        case DeclarationKind::Alternative: {
            const auto& x = std::get<DeclaredValue::Alternative>(v.data);
            const auto it = std::find_if(a.factors.begin(), a.factors.end(), [&](const auto& f){return f.key == x.key;});
            return DeclaredValue::alternative(x.key, recode(it->type, x.element.at(0)));
        }
        case DeclarationKind::Optional: {
            const auto& x = std::get<DeclaredValue::Optional>(v.data).element;
            return x.empty() ? DeclaredValue::absent() : DeclaredValue::present(recode(a.element, x.front()));
        }
        case DeclarationKind::Repeated: {
            std::vector<DeclaredValue> result;
            for (const auto& x : std::get<DeclaredValue::Repeated>(v.data).elements) result.push_back(recode(a.element, x));
            return DeclaredValue::repeated(std::move(result));
        }
        case DeclarationKind::RationalTensor: {
            const auto& old = std::get<numeric::SparseRationalTensor>(v.data);
            std::vector<std::size_t> shape; for (const auto& axis : b.axes) shape.push_back(axis.extent);
            numeric::IndexDomain domain(std::move(shape));
            declaration_detail::budget(old.support().count(), limits_.max_value_nodes, "tensor recoding support work budget");
            declaration_detail::budget(machine::checked_size_mul(old.support().count(), sizeof(std::size_t)),
                                       limits_.max_materialized_bytes, "tensor recoding support buffer budget");
            const auto position = [&](std::size_t i) { return tensor_position(a,b,old.domain(),domain,i); };
            std::vector<std::size_t> support; support.reserve(old.support().count()); old.support().for_each_active([&](auto i){support.push_back(position(i));});
            std::vector<numeric::SparseRationalEntry> values;
            for (const auto& x : old.values().entries()) values.push_back({position(x.index), x.value});
            const auto size = domain.logical_size();
            return DeclaredValue::tensor(numeric::SparseRationalTensor(std::move(domain),
                numeric::StructuralSupport::sparse(size, std::move(support)), numeric::SparseRationalVector(size, std::move(values))));
        }
        default: throw std::invalid_argument("unknown declaration recoding constructor");
        }
    }
    DeclarationRegistry base_, target_;
    std::vector<DeclarationMap> maps_;
    DeclarationLimits limits_;
    std::map<DeclarationId, std::size_t> by_id_;
};

inline std::vector<DeclarationMap> identity_declaration_maps(const DeclarationRegistry& registry) {
    std::vector<DeclarationMap> out;
    for (const auto& d : registry.definitions()) out.push_back({d.id, d.id, DeclarationMapKind::Recode, {}});
    return out;
}
} // namespace rfc::composite
