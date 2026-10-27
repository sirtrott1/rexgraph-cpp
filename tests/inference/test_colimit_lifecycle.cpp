#include "../../examples/colimit_gluing_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace colimit_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,std::string part){bool no=false;try{f();}catch(const std::exception& e){no=std::string(e.what()).find(part)!=std::string::npos;if(!no)throw;}check(no,"expected refusal absent");}
TemporalEndpoint reopened(TemporalEndpoint e,BooleanLayoutProfile profile){if(e.declarations)e.declarations->profile=profile;const auto src=encode_rfcx(e.source);const auto wire=encode_temporal_endpoint(e);auto out=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),wire);check(encode_temporal_endpoint(out)==wire && encode_rfcx(out.source).bytes==src.bytes,"canonical existing source endpoint codecs");return out;}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        auto d=dataset(true);const auto old=plan_unsplit(d);const auto expected=result_unsplit(old,d);
        for(auto& f:d.locals)f.endpoint=reopened(f.endpoint,profile);
        for(auto& f:d.commons)f.endpoint=reopened(f.endpoint,profile);
        d.certificates=reopened(d.certificates,profile);
        const auto prepared=prepare_unsplit(d);auto coordinates=coordinates_unsplit(prepared.view());
        TemporalDeclarationState ds;ds.namespace_key=30000;ds.profile=profile;ds.registry=DeclarationRegistry({Declaration::rational({30000,1,1})});coordinates.declarations=std::move(ds);
        const auto p=bind_unsplit(prepared,coordinates);const auto result=result_unsplit(p,d);
        check(!old.view().patches()[0].source_owner().same_owner(p.view().patches()[0].source_owner()),"fresh local owners not inferred from bytes");
        check(result.total==730 && result.total==expected.total && result.aggregate.sparse_values().entries()==expected.aggregate.sparse_values().entries(),"persisted maps feed actual unsplit gluing execution");
        check(result.aggregate.publications().size()>=9,"all local/common/certificate/coordinate dependencies");
        for(std::size_t i=0;i<4;++i){TemporalTensor t;t.key=3000+i;t.role=TemporalTensorRole::Field;t.output=result.components[i].field.carrier().key;t.tensor={result.components[i].field.carrier().declaration->domain,numeric::StructuralSupport::full(10),result.components[i].field.sparse_values()};coordinates.tensors.push_back(t);}
        TemporalDeclaredValue value;value.key=4000;value.type={30000,1,1};value.value=DeclaredValue::rational(result.total);coordinates.declarations->values.push_back(value);
        const auto out=reopened(coordinates,profile);const auto q=bind_unsplit(prepare_unsplit(d),out);const auto again=result_unsplit(q,d);
        check(!result.aggregate.carrier().same(again.aggregate.carrier()),"fresh result coordinate owner");
        for(std::size_t i=0;i<4;++i){check(out.tensors[i].tensor.values().entries()==again.components[i].field.sparse_values().entries(),"actual component output replayed");check(out.tensors[i].tensor.support().count()==10,"supported zero output coordinates survive");}
        check(again.total==expected.total,"exact stored moment recomputed");
        for(std::size_t k=0;k<available().size();++k){auto a=available();a[k].max_sequence=6;rejects([&]{p.glue_field(g1,inputs(d.locals,1),a);},"avail");}
        // Valid bytes are not a valid certificate. Rebinding must reject lies.
        auto bad=d;auto key=bad.records[0].relation_lift.key;
        for(auto& t:bad.certificates.tensors)if(t.key==key){auto m=temporal_tensor_matrix(t);m.set(0,2,0);t.tensor=temporal_matrix_tensor(m);}
        bad.certificates=reopened(bad.certificates,profile);rejects([&]{prepare_unsplit(bad);},"kernel differs");
        bad=d;bad.requests[0].stalks[0].sides[0][0].role=TemporalTensorRole::SheafRestriction;rejects([&]{prepare_unsplit(bad);},"roles");
        bad=d;set_field(bad.locals[3],g1,S(2,{{0,4},{1,11}}));bad.locals[3].endpoint=reopened(bad.locals[3].endpoint,profile);const auto inconsistent=plan_unsplit(bad);rejects([&]{result_unsplit(inconsistent,bad);},"observation image");
        Guard g;const auto dx=p.view().boundary(g1).apply(result.input.sparse_values(),g);const auto local=p.view().restrict_field(g0,dx,1,g);const auto& binding=p.view().patches()[1];
        const auto records=binding.export_downward(local,6000,7000);auto local_endpoint=d.locals[1].endpoint;
        local_endpoint.tensors.insert(local_endpoint.tensors.end(),records.tensors.begin(),records.tensors.end());
        local_endpoint.declarations->values.insert(local_endpoint.declarations->values.end(),records.values.begin(),records.values.end());
        const auto re=reopened(local_endpoint,profile);check(re.tensors.size()==local_endpoint.tensors.size(),"computed local readout exports under original local owner");
        const DeclaredLocalFieldBinding rebound(re,d.locals[1].contract);std::vector<DeclaredFieldInput> ins;
        for(std::size_t i=0;i<records.tensors.size();++i)ins.push_back({records.tensors[i].key,records.values[i].key});
        const auto recovered=rebound.bind_downward(g0,ins);
        check(recovered.values().size()==local.values().size(),"local recovered stalk inventory");
        for(const auto& [cell,v]:local.values())check(v.entries()==recovered.values().at(cell).entries(),"exact recovered local values");
        auto zero=d;set_field(zero.locals[1],g1,S(2,{{0,2}}));set_field(zero.locals[2],g1,S(2,{{0,3}}));set_field(zero.locals[3],g1,S(2,{{0,Q{5,2}}}));
        const auto zp=plan_unsplit(zero);const auto zr=result_unsplit(zp,zero);check(zr.total==26 && zr.components[3].self==0,"supported zero private component");
        auto zep=coordinates_unsplit(zp.view());TemporalTensor zt;zt.key=9090;zt.role=TemporalTensorRole::Field;zt.output=zr.components[3].field.carrier().key;zt.tensor={zt.output?numeric::IndexDomain::linear(10):numeric::IndexDomain{},numeric::StructuralSupport::full(10),zr.components[3].field.sparse_values()};zep.tensors.push_back(zt);
        const auto zre=reopened(zep,profile);check(zre.tensors[0].tensor.values().nonzeros()==0 && zre.tensors[0].tensor.support().count()==10,"zero result not missing support");
    }
    std::cout<<"colimit_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
