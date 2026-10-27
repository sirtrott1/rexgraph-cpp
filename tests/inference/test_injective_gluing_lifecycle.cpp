#include "../../examples/injective_gluing_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace injective_gluing_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,m);}
Fixture reopen(Fixture f,BooleanLayoutProfile profile){
    f.endpoint.declarations->profile=profile;const auto src=encode_rfcx(f.endpoint.source);const auto bytes=encode_temporal_endpoint(f.endpoint);
    f.endpoint=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),bytes);
    check(encode_temporal_endpoint(f.endpoint)==bytes,"canonical independent endpoint including split witnesses");check(encode_rfcx(f.endpoint.source).bytes==src.bytes,"original source bytes retained");return f;
}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        auto data=make_dataset(true);const auto before=plan(data);const auto old=result(before,data);
        data.common=reopen(data.common,profile);for(auto& f:data.locals)f=reopen(f,profile);
        const auto prepared=prepare(data);auto coords=coordinates(prepared.view());TemporalDeclarationState ds;ds.namespace_key=14000;ds.profile=profile;ds.registry=DeclarationRegistry({Declaration::rational({14000,1,1})});coords.declarations=ds;
        const auto p=bind_plan(prepared,coords);const auto r=result(p,data);
        check(r.total==454 && r.total==old.total && r.aggregate.sparse_values().entries()==old.aggregate.sparse_values().entries(),"real V1/V2 values drive identical pushout and moment");
        check(!p.view().patches()[0].source_owner().same_owner(before.view().patches()[0].source_owner()),"fresh local source owner");
        check(!p.view().common().source_owner().same_owner(before.view().common().source_owner()),"fresh common source owner");
        check(r.matches_reference==true,"actual component actions match direct action");
        std::uint64_t id=3000;for(const auto& c:r.components){TemporalTensor t;t.key=id++;t.role=TemporalTensorRole::Field;t.output=c.field.carrier().key;t.tensor={c.field.carrier().declaration->domain,numeric::StructuralSupport::full(c.field.carrier().size()),c.field.sparse_values()};coords.tensors.push_back(t);}
        TemporalDeclaredValue v;v.key=4000;v.type={14000,1,1};v.value=DeclaredValue::rational(r.total);coords.declarations->values.push_back(v);
        const auto bytes=encode_temporal_endpoint(coords);const auto src=encode_rfcx(coords.source);const auto re=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),bytes);check(encode_temporal_endpoint(re)==bytes,"canonical output component fields and moment");
        const auto fresh=bind_plan(prepare(data),re);const auto again=result(fresh,data);
        check(!again.aggregate.carrier().same(r.aggregate.carrier()),"fresh quotient publication");
        for(std::size_t i=0;i<2;++i){check(re.tensors[i].tensor.values().entries()==again.components[i].field.sparse_values().entries(),"stored actual component replay");check(re.tensors[i].tensor.support().count()==8,"private and shared supported zeros retained");}
        check(again.total==r.total && again.aggregate.sparse_values().entries()==r.aggregate.sparse_values().entries(),"recomputed aggregate");
        check(again.aggregate.publications().size()>=4,"common, both locals, coordinate publications retained");
        Guard g;const auto local=fresh.view().restrict_field(g0,again.aggregate.sparse_values(),1,g);const auto records=fresh.view().patches()[1].export_downward(local,6000,7000);
        auto e=data.locals[1].endpoint;e.tensors.insert(e.tensors.end(),records.tensors.begin(),records.tensors.end());e.declarations->values.insert(e.declarations->values.end(),records.values.begin(),records.values.end());
        const auto wire=encode_temporal_endpoint(e);const auto back=decode_temporal_endpoint(e.source,wire);check(encode_temporal_endpoint(back)==wire,"actual global output restricted to original local owner and exported");
        const DeclaredLocalFieldBinding bound(back,data.locals[1].contract);std::vector<DeclaredFieldInput> rs;for(std::size_t i=0;i<records.tensors.size();++i)rs.push_back({records.tensors[i].key,records.values[i].key});const auto field=bound.bind_downward(g0,rs);
        check(field.nonzeros()==local.nonzeros(),"typed restricted field numerics recovered");
        for(const auto& [cell,value]:local.values())check(field.values().at(cell).entries()==value.entries(),"all local complementary values retained on disk");
        auto limits=availability(3);limits.back().max_sequence=6;rejects([&]{fresh.boundary(g1).execute(again.input,limits);},"old cutoff cannot use later common declaration");
        check(data.locals[0].endpoint.source.grade(g1).aspects.get(data.locals[0].endpoint.source.signature().aspects().id("gate"),2),"present-zero composite role bit survived");
        auto bad=data;for(auto& t:bad.locals[0].endpoint.tensors)if(t.key==bad.requests[0].split[0].reconstruction){auto m=temporal_tensor_matrix(t);m.set(0,0,3);t.tensor=temporal_matrix_tensor(m);}
        bad.locals[0]=reopen(bad.locals[0],profile);rejects([&]{plan(bad);},"canonical bytes do not certify a false split inverse");
        bad=data;for(auto& t:bad.locals[1].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto m=temporal_tensor_matrix(t);m.set(1,0,m.get(1,0)+1);t.tensor=temporal_matrix_tensor(m);break;}
        bad.locals[1]=reopen(bad.locals[1],profile);rejects([&]{plan(bad);},"canonical bytes do not certify noncommuting common injection");
        // Supported-zero private component is supplied, not missing coverage.
        auto zero=data;const auto coordinate=S(4,{{0,2},{1,3}});
        for(std::size_t side=0;side<2;++side){const auto field_local=p.view().restrict_field(g1,coordinate,side,g);set_field(zero.locals[side],g1,field_local.values().at(0));zero.locals[side]=reopen(zero.locals[side],profile);}
        const auto zp=plan(zero);const auto zr=result(zp,zero);check(zr.components[1].field.sparse_values().nonzeros()==0 && zr.components[1].self==0 && zr.total==68,"supplied zero complement retained");
        auto out=coordinates(zp.view());TemporalTensor zt;zt.key=9000;zt.role=TemporalTensorRole::Field;zt.output=zr.components[1].field.carrier().key;zt.tensor={numeric::IndexDomain::linear(8),numeric::StructuralSupport::full(8),zr.components[1].field.sparse_values()};out.tensors.push_back(zt);
        const auto zbytes=encode_temporal_endpoint(out);const auto zread=decode_temporal_endpoint(out.source,zbytes);check(zread.tensors[0].tensor.support().count()==8 && zread.tensors[0].tensor.values().nonzeros()==0,"zero full-support result roundtrip");
        std::cout<<"injective_lifecycle profile="<<unsigned(profile)<<" output_bytes="<<bytes.size()<<'\n';
    }
    std::cout<<"injective_gluing_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
