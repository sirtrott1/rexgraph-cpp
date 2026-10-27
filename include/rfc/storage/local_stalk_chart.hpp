#pragma once
#include "rfc/storage/declared_local_field.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"

namespace rfc::storage {
// A complete linear map represented as a scalar identity plus stored corrections.
// A nonzero identity term is allowed only on square carriers. This is NOT a
// metric or a partial observation. It keeps large identities/shears symbolic.
class StalkChartMap {
    numeric::Rational scalar_;
    numeric::ExactMatrix correction_;
    using Q=numeric::Rational;
    using S=numeric::SparseRationalVector;
    using G=numeric::ExactActionGuard;
public:
    StalkChartMap(Q scalar,numeric::ExactMatrix correction)
        :scalar_(numeric::canonical_rational(scalar)),correction_(std::move(correction)) {
        if(scalar_!=0 && rows()!=cols())throw std::invalid_argument("chart identity term on rectangular carrier");
    }
    static StalkChartMap identity(std::size_t n,Q scale=1){return {scale,numeric::ExactMatrix(n,n)};}
    static StalkChartMap explicit_map(numeric::ExactMatrix a){return {0,std::move(a)};}
    std::size_t rows()const{return correction_.rows();}
    std::size_t cols()const{return correction_.cols();}
    std::size_t stored_entries()const{return machine::checked_size_add(1,correction_.nonzeros());}
    // Read-only factors for exact subspace-descent checks. Never expand s I.
    const numeric::Rational& identity_coefficient()const & {return scalar_;}
    const numeric::Rational& identity_coefficient()const &&=delete;
    const numeric::ExactMatrix& correction()const & {return correction_;}
    const numeric::ExactMatrix& correction()const &&=delete;
    void validate(G& g)const {g.step();g.value(scalar_);g.matrix(correction_);}
    bool is_zero(G& g)const {
        validate(g);
        if(scalar_==0 || rows()==0)return correction_.nonzeros()==0;
        if(correction_.nonzeros()!=rows())return false;
        for(const auto& [ij,v]:correction_.entries()){g.step();if(ij.first!=ij.second || v!=-scalar_)return false;}
        return true;
    }
    StalkChartMap minus(const StalkChartMap& b,G& g)const {
        if(rows()!=b.rows() || cols()!=b.cols())throw std::invalid_argument("chart difference carriers");
        validate(g);b.validate(g);
        const auto held=machine::checked_size_add(stored_entries(),b.stored_entries());
        g.entries(machine::checked_size_add(held,correction_.nonzeros()));auto m=correction_;
        for(const auto& [ij,v]:b.correction_.entries()){
            g.entries(machine::checked_size_add(held,machine::checked_size_add(m.nonzeros(),1)));
            m.set(ij.first,ij.second,g.sub(m.get(ij.first,ij.second),v));
        }
        return {g.sub(scalar_,b.scalar_),std::move(m)};
    }
    bool equals(const StalkChartMap& b,G& g)const {
        return rows()==b.rows() && cols()==b.cols() && minus(b,g).is_zero(g);
    }
    // this * b; visit stored factors, never the implicit diagonal.
    StalkChartMap multiply(const StalkChartMap& b,G& g)const {
        if(cols()!=b.rows())throw std::invalid_argument("chart product carriers");
        validate(g);b.validate(g);numeric::ExactMatrix out(rows(),b.cols());
        const auto held=machine::checked_size_mul(machine::checked_size_add(stored_entries(),b.stored_entries()),4);
        g.entries(held);
        const auto add=[&](std::size_t i,std::size_t j,const Q& q){
            g.entries(machine::checked_size_add(held,machine::checked_size_add(out.nonzeros(),1)));
            out.set(i,j,g.add(out.get(i,j),q));
        };
        if(scalar_!=0)for(const auto& [ij,q]:b.correction_.entries())add(ij.first,ij.second,g.mul(scalar_,q));
        if(b.scalar_!=0)for(const auto& [ij,q]:correction_.entries())add(ij.first,ij.second,g.mul(b.scalar_,q));
        std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>> rows;
        for(const auto& [ij,q]:b.correction_.entries()){g.step();rows[ij.first].emplace_back(ij.second,q);}
        for(const auto& [ij,q]:correction_.entries()){
            g.step();const auto r=rows.find(ij.second);if(r==rows.end())continue;
            for(const auto& [j,v]:r->second)add(ij.first,j,g.mul(q,v));
        }
        return {g.mul(scalar_,b.scalar_),std::move(out)};
    }
    S apply(const S& x,G& g,bool transpose=false)const {
        validate(g);g.sparse_vector(x);
        if(x.size()!=(transpose?rows():cols()))throw std::invalid_argument("chart field carrier");
        std::map<std::size_t,Q> out;
        const auto held=machine::checked_size_add(stored_entries(),x.nonzeros());g.entries(held);
        const auto add=[&](std::size_t i,const Q& q){
            g.entries(machine::checked_size_add(held,machine::checked_size_add(out.size(),1)));
            const auto at=out.find(i);auto v=g.add(at==out.end()?Q{0}:at->second,q);
            if(v==0)out.erase(i);else out[i]=std::move(v);
        };
        if(scalar_!=0)for(const auto& e:x.entries())add(e.index,g.mul(scalar_,e.value));
        for(const auto& [ij,q]:correction_.entries()){
            g.step();const auto in=transpose?ij.first:ij.second;
            const auto at=std::lower_bound(x.entries().begin(),x.entries().end(),in,[](const auto& a,auto b){return a.index<b;});
            if(at!=x.entries().end() && at->index==in)add(transpose?ij.second:ij.first,g.mul(q,at->value));
        }
        g.entries(machine::checked_size_add(held,machine::checked_size_mul(out.size(),2)));
        return numeric::checked_sparse_detail::vector(transpose?cols():rows(),std::move(out),g);
    }
};
// Caller admission of a full coordinate ISOMORPHISM, local -> common -> local.
// The common declaration is a stalk coordinate type, not another source cell.
// Structural identification still requires an independent LocalFieldInterface.
struct LocalStalkChart {
    std::size_t patch{};
    DeclaredStalkBinding local;
    std::uint64_t common_coordinates{};
    composite::Declaration common_declaration;
    StalkChartMap to_common,from_common;
};
// Select genuine persisted maps in one local endpoint. The default role names
// are fixed by this profile; reversing a direction or substituting a sheaf map
// is rejected. No role string is treated as an inverse certificate.
struct LocalStalkChartRecords {
    std::size_t patch{};
    DeclaredStalkBinding local;
    TemporalCarrier common;
    composite::Declaration common_declaration;
    std::uint64_t accession{},reconstruction{};
};
inline LocalStalkChart local_stalk_chart_from_records(const DeclaredLocalFieldBinding& binding,
    const LocalStalkChartRecords& r,numeric::ExactActionGuard& g) {
    const auto require=[](bool b,const char* m){if(!b)throw std::invalid_argument(m);};
    const auto& e=binding.endpoint();
    const auto cs=temporal_endpoint_detail::carriers(e);const auto ts=temporal_endpoint_detail::tensors(e);
    require(cs.contains(r.common.key) && *cs.at(r.common.key)==r.common,"chart common carrier differs");
    require(r.common.kind==TemporalCarrierKind::Explicit && r.common.coordinate_system!=0,"chart common carrier must be explicit");
    require(e.declarations && e.declarations->registry.at(r.common_declaration.id)==r.common_declaration,"chart common definition differs");
    require(r.common.domain==r.local.carrier.domain,"chart profile requires equal complete extents");
    const auto load=[&](std::uint64_t key,bool forward){
        require(ts.contains(key),"chart map record absent");const auto& t=*ts.at(key);
        require(t.role==(forward?TemporalTensorRole::Accession:TemporalTensorRole::Reconstruction),"chart map role differs");
        require(t.input==(forward?r.local.carrier.key:r.common.key) && t.output==(forward?r.common.key:r.local.carrier.key),"chart map direction differs");
        require(t.owner.kind==TemporalOwnerKind::Cell && r.local.carrier.cell==t.owner.cell,"chart map owner differs");
        require(t.tensor.support().count()==t.tensor.domain().logical_size(),"chart map must have total support");
        auto m=temporal_tensor_matrix(t);g.matrix(m);return StalkChartMap::explicit_map(std::move(m));
    };
    return {r.patch,r.local,r.common.coordinate_system,r.common_declaration,load(r.accession,true),load(r.reconstruction,false)};
}
} // namespace rfc::storage
