#include "../../examples/finite_cover_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace finite_cover_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,m);}
Fixture reopen(Fixture f,BooleanLayoutProfile profile){f.endpoint.declarations->profile=profile;const auto src=encode_rfcx(f.endpoint.source);const auto bits=encode_temporal_endpoint(f.endpoint);f.endpoint=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),bits);check(encode_temporal_endpoint(f.endpoint)==bits && encode_rfcx(f.endpoint.source).bytes==src.bytes,"source, chart, block declarations and fields canonical roundtrip");return f;}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        auto d=make_cover(true);const auto old=plan_cover(d);const auto before=cover_result(old,d);
        for(auto& f:d.locals)f=reopen(f,profile);
        for(auto& e:d.interfaces)e.common=reopen(e.common,profile);
        const auto prepared=prepare_cover(d);auto coords=coordinates_cover(prepared.view());TemporalDeclarationState ds;ds.namespace_key=21000;ds.profile=profile;ds.registry=DeclarationRegistry({Declaration::rational({21000,1,1})});coords.declarations=ds;
        const auto p=bind_cover(prepared,coords);const auto r=cover_result(p,d);
        check(!old.view().patches()[0].source_owner().same_owner(p.view().patches()[0].source_owner()),"fresh source owners not equated by equal bytes");
        check(r.total==3228 && r.total==before.total && r.aggregate.sparse_values().entries()==before.aggregate.sparse_values().entries(),"actual reopened values reach gluing and action moments");
        check(r.matches_reference==true && p.view().dimension(g1)==6,"redundant cyclic interface survives rebinding");
        for(std::size_t i=0;i<3;++i){TemporalTensor t;t.key=3000+i;t.role=TemporalTensorRole::Field;t.output=r.components[i].field.carrier().key;t.tensor={r.components[i].field.carrier().declaration->domain,numeric::StructuralSupport::full(12),r.components[i].field.sparse_values()};coords.tensors.push_back(t);}
        TemporalDeclaredValue value;value.key=4000;value.type={21000,1,1};value.value=DeclaredValue::rational(r.total);coords.declarations->values.push_back(value);
        const auto bytes=encode_temporal_endpoint(coords);const auto source=encode_rfcx(coords.source);const auto re=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),bytes);check(encode_temporal_endpoint(re)==bytes,"actual component outputs and moment reopen canonically");
        const auto q=bind_cover(prepare_cover(d),re);const auto rr=cover_result(q,d);
        check(!r.aggregate.carrier().same(rr.aggregate.carrier()),"fresh quotient carrier owner");
        for(std::size_t i=0;i<3;++i){check(re.tensors[i].tensor.values().entries()==rr.components[i].field.sparse_values().entries(),"stored component recomputed exactly");check(re.tensors[i].tensor.support().count()==12,"full supported zeros retained per component");}
        check(rr.total==r.total && rr.aggregate.sparse_values().entries()==r.aggregate.sparse_values().entries(),"complete replay same field and moment");
        check(rr.aggregate.publications().size()>=7,"three patches three interfaces and quotient all retained");
        Guard g;const auto local=q.view().restrict_field(g0,rr.aggregate.sparse_values(),2,g);const auto records=q.view().patches()[2].export_downward(local,6000,7000);
        auto endpoint=d.locals[2].endpoint;endpoint.tensors.insert(endpoint.tensors.end(),records.tensors.begin(),records.tensors.end());endpoint.declarations->values.insert(endpoint.declarations->values.end(),records.values.begin(),records.values.end());
        const auto wire=encode_temporal_endpoint(endpoint);const auto back=decode_temporal_endpoint(endpoint.source,wire);check(encode_temporal_endpoint(back)==wire,"restricted actual global result exported under local owner");
        const DeclaredLocalFieldBinding bound(back,d.locals[2].contract);std::vector<DeclaredFieldInput> ins;for(std::size_t i=0;i<records.tensors.size();++i)ins.push_back({records.tensors[i].key,records.values[i].key});const auto recovered=bound.bind_downward(g0,ins);
        for(const auto& [cell,v]:local.values())check(v.entries()==recovered.values().at(cell).entries(),"private coordinates retained on disk");
        for(auto availability_bad: {available(d)})for(std::size_t i=0;i<availability_bad.size();++i){auto aa=available(d);aa[i].max_sequence=6;rejects([&]{q.glue_field(g1,inputs(d.locals,1),aa);},"all source chart interface and quotient cutoffs enforced");}
        check(d.locals[0].endpoint.source.grade(g1).aspects.get(d.locals[0].endpoint.source.signature().aspects().id("gate"),2),"present zero role not removed");
        auto bad=d;for(auto& t:bad.locals[0].endpoint.tensors)if(t.key==bad.partitions[0].chart.reconstruction){auto m=temporal_tensor_matrix(t);m.set(0,0,3);t.tensor=temporal_matrix_tensor(m);}
        bad.locals[0]=reopen(bad.locals[0],profile);rejects([&]{plan_cover(bad);},"canonical encoding not an inverse certificate");
        bad=d;bad.partitions[0].blocks[0].declaration.id.version+=1;rejects([&]{plan_cover(bad);},"caller cannot substitute unretained block types");
        bad=d;for(auto& t:bad.locals[1].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto m=temporal_tensor_matrix(t);m.set(2,0,m.get(2,0)+1);t.tensor=temporal_matrix_tensor(m);break;}
        bad.locals[1]=reopen(bad.locals[1],profile);rejects([&]{plan_cover(bad);},"reopening checks full descent independently of observations");
        auto zero=d;const S coordinate(6,{{0,2},{1,3},{2,5}});
        for(std::size_t i=0;i<3;++i){const auto xf=p.view().restrict_field(g1,coordinate,i,g);set_field(zero.locals[i],g1,xf.values().at(0));zero.locals[i]=reopen(zero.locals[i],profile);}
        const auto zp=plan_cover(zero);const auto zr=cover_result(zp,zero);check(zr.total==166 && zr.components[1].self==0 && zr.components[2].self==0,"zero private components supplied and retained");
        auto zcoords=coordinates_cover(zp.view());TemporalTensor z;z.key=9000;z.role=TemporalTensorRole::Field;z.output=zr.components[2].field.carrier().key;z.tensor={numeric::IndexDomain::linear(12),numeric::StructuralSupport::full(12),zr.components[2].field.sparse_values()};zcoords.tensors.push_back(z);
        const auto zb=encode_temporal_endpoint(zcoords);const auto zre=decode_temporal_endpoint(zcoords.source,zb);check(zre.tensors[0].tensor.values().nonzeros()==0 && zre.tensors[0].tensor.support().count()==12,"all-zero full-support result persisted");
        std::cout<<"finite_cover_lifecycle profile="<<unsigned(profile)<<" output_bytes="<<bytes.size()<<'\n';
    }
    std::cout<<"finite_cover_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
