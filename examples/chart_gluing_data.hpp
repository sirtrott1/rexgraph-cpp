#pragma once
#include "local_gluing_data.hpp"

namespace chart_gluing_example {
using namespace local_gluing_example;
struct ChartFixture {Fixture field;std::vector<LocalStalkChartRecords> charts;};
inline M coordinate_change(std::size_t n){
    if(n==1)return M::from_rows({{2}});
    if(n==2)return M::from_rows({{1,1},{0,1}});
    if(n==3)return M::from_rows({{1,0,1},{0,2,0},{0,0,1}});
    throw std::invalid_argument("small example chart extent");
}
inline M inverse_change(std::size_t n){
    if(n==1)return M::from_rows({{Q{1,2}}});
    if(n==2)return M::from_rows({{1,-1},{0,1}});
    if(n==3)return M::from_rows({{1,0,-1},{0,Q{1,2},0},{0,0,1}});
    throw std::invalid_argument("small example inverse extent");
}
// Build an independent coordinate description of the same local physical maps.
// The upward family remains a separately declared family, not inferred from d.
inline ChartFixture changed(Fixture old,std::size_t patch,std::uint64_t tag){
    const DeclaredLocalFieldBinding original(old.endpoint,old.contract);
    ChartFixture result{old,{}};auto& f=result.field;
    auto defs=f.endpoint.declarations->registry.definitions();
    std::set<std::uint64_t> remove;for(const auto& b:f.contract.downward.maps)remove.insert(b.tensor);
    std::erase_if(f.endpoint.tensors,[&](const auto& t){return remove.contains(t.key);});f.contract.downward.maps.clear();
    for(std::size_t i=0;i<f.contract.downward.stalks.size();++i){
        const auto common=old.contract.downward.stalks[i];auto& local=f.contract.downward.stalks[i];
        const auto n=local.carrier.domain.logical_size();const auto Smap=coordinate_change(n),back=inverse_change(n);
        local.carrier.coordinate_system=tag*100+n;
        local.declaration=Declaration::rational_tensor({tag,n,1},{{tag,n,[&]{std::vector<std::uint64_t> ids;for(std::size_t j=0;j<n;++j)ids.push_back(tag*1000+j+1);return ids;}()}});
        defs.push_back(local.declaration);
        for(auto& c:f.endpoint.carriers)if(c.key==local.carrier.key)c=local.carrier;
        // Preserve the independent upward signature but keep its carrier record current.
        for(auto& b:f.contract.upward.stalks)if(b.carrier.key==local.carrier.key){b.carrier=local.carrier;b.declaration=local.declaration;}
        for(auto& t:f.endpoint.tensors)if(t.role==TemporalTensorRole::Field && t.output==local.carrier.key){
            Guard g;const auto values=numeric::checked_apply(Smap,t.tensor.values(),g);
            t.tensor={t.tensor.domain(),numeric::StructuralSupport::full(n),values};
            for(auto& v:f.endpoint.declarations->values)if(v.cell==local.carrier.cell){v.type=local.declaration.id;v.value=DeclaredValue::tensor(t.tensor);}
        }
        TemporalCarrier common_carrier{8000+i,TemporalCarrierKind::Explicit,{},{},common.carrier.coordinate_system,common.carrier.domain};
        f.endpoint.carriers.push_back(common_carrier);
        const auto forward=9000+2*i,reverse=forward+1;
        for(const bool to_common:{true,false}){
            TemporalTensor t;t.key=to_common?forward:reverse;t.role=to_common?TemporalTensorRole::Accession:TemporalTensorRole::Reconstruction;
            t.input=to_common?local.carrier.key:common_carrier.key;t.output=to_common?common_carrier.key:local.carrier.key;
            t.owner.kind=TemporalOwnerKind::Cell;t.owner.cell=*local.carrier.cell;t.tensor=temporal_matrix_tensor(to_common?back:Smap);f.endpoint.tensors.push_back(t);
        }
        result.charts.push_back({patch,local,common_carrier,common.declaration,forward,reverse});
    }
    const auto find=[&](core::CellId c)->const DeclaredStalkBinding&{
        const auto key=stable_cell_key(f.endpoint.source,c);
        for(const auto& s:old.contract.downward.stalks)if(s.carrier.cell==key)return s;
        throw std::logic_error("example cell");
    };
    for(std::size_t k=1;k<f.endpoint.source.grade_count();++k){
        const core::Grade grade{static_cast<std::uint16_t>(k)},lower{static_cast<std::uint16_t>(k-1)};const auto& a=f.endpoint.source.grade(grade);
        for(std::size_t slot=0;slot<a.incidence_count();++slot){
            const auto& u=find({grade,a.upper_index[slot]});const auto& l=find({lower,a.lower_index[slot]});
            const auto nu=u.carrier.domain.logical_size(),nl=l.carrier.domain.logical_size();
            M actual(nl,nu);const auto& o=original.fibers().fibers().transport_override(grade,slot);
            if(o)actual=*o;else for(std::size_t j=0;j<nu;++j)actual.set(j,j,f.endpoint.source.incidence_coefficient(grade,slot));
            Guard g;actual=numeric::checked_multiply(coordinate_change(nl),numeric::checked_multiply(actual,inverse_change(nu),g),g);
            TemporalTensor t;t.key=4000+100*k+slot;t.role=TemporalTensorRole::RealizedIncidenceMap;t.input=u.carrier.key;t.output=l.carrier.key;
            t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence={grade,a.occurrence_identities[slot]};t.tensor=temporal_matrix_tensor(actual);
            f.endpoint.tensors.push_back(t);f.contract.downward.maps.push_back({t.key,t.role});
        }
    }
    f.contract.downward.allow_constant_identity=false;
    f.endpoint.declarations->registry=DeclarationRegistry(std::move(defs));validate_temporal_endpoint(f.endpoint);
    return result;
}
inline PreparedNativeLocalGluing plan(const std::vector<Fixture>& fs,std::vector<LocalFieldInterface> interfaces,
                                    std::vector<LocalStalkChartRecords> charts){
    auto p=PreparedNativeLocalGluing::prepare_with_charts(publications(fs),std::move(interfaces),std::move(charts));
    return bind(p,coordinate_endpoint(p.view()));
}
} // namespace chart_gluing_example
