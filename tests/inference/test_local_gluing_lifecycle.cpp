#include "../../examples/local_gluing_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace local_gluing_example;
std::size_t checks=0;void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
Fixture roundtrip(Fixture f,BooleanLayoutProfile profile){
    f.endpoint.declarations->profile=profile;const auto source=encode_rfcx(f.endpoint.source);const auto bytes=encode_temporal_endpoint(f.endpoint);
    f.endpoint=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),bytes);
    check(encode_temporal_endpoint(f.endpoint)==bytes,"local canonical endpoint bytes");check(encode_rfcx(f.endpoint.source).bytes==source.bytes,"actual local source round trip");return f;
}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        std::vector<Fixture> fs{roundtrip(make(100,10,20,1,false,true),profile),roundtrip(make(200,30,40),profile),roundtrip(make(300,20,30),profile)};
        auto ia=vertex_interface(fs,0,2,20),ib=vertex_interface(fs,1,2,30);
        for(auto* i:{&ia,&ib}){const auto wire=encode_rfcx(i->common.source());i->common=core::OwnedRelationalSource(MappedRfcx(wire.bytes).materialize());}
        const auto prepared=PreparedNativeLocalGluing::prepare(publications(fs),{ia,ib});
        auto coords=coordinate_endpoint(prepared.view());TemporalDeclarationState dec;dec.namespace_key=9801;dec.profile=profile;dec.registry=DeclarationRegistry({Declaration::rational({9801,1,1})});coords.declarations=dec;
        const auto plan=bind(prepared,coords);const auto before=third(plan,fs);check(before.total==5 && before.matches_reference==true,"reopened locals feed actual action moments");
        std::uint64_t id=3000;
        for(const auto& component:before.components){TemporalTensor t;t.key=id++;t.role=TemporalTensorRole::Field;t.output=component.field.carrier().key;
            t.tensor={component.field.carrier().declaration->domain,numeric::StructuralSupport::full(component.field.carrier().size()),component.field.sparse_values()};coords.tensors.push_back(t);}
        TemporalDeclaredValue value;value.key=4000;value.type={9801,1,1};value.value=DeclaredValue::rational(before.total);coords.declarations->values.push_back(value);
        const auto src=encode_rfcx(coords.source);const auto wire=encode_temporal_endpoint(coords);auto recovered=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),wire);
        check(encode_temporal_endpoint(recovered)==wire,"canonical actual component fields");check(recovered.tensors[1].tensor.values().nonzeros()==0 && recovered.tensors[1].tensor.support().count()==3,"supported zero component survives");
        const auto new_prepared=PreparedNativeLocalGluing::prepare(publications(fs),{ia,ib});const auto fresh=bind(new_prepared,recovered);const auto result=third(fresh,fs);
        check(!result.aggregate.carrier().same(before.aggregate.carrier()),"reopen gives fresh owners");check(result.total==before.total && result.aggregate.sparse_values().entries()==before.aggregate.sparse_values().entries(),"recomputed actual fields agree");
        for(std::size_t p=0;p<3;++p)check(result.components[p].field.sparse_values().entries()==recovered.tensors[p].tensor.values().entries(),"stored component origin is rechecked");
        check(result.input.publications().size()>=4 && result.aggregate.publications().size()>=4,"local source and carrier dependencies retained");
        auto available=availability(3);available[1].max_sequence=6;bool refused=false;try{fresh.boundary(g1).algebraic_transpose().execute(result.input,available);}catch(const std::exception&){refused=true;}check(refused,"later local source unavailable at old cutoff");
        std::vector<Fixture> hf{roundtrip(make(100,10,20,1,true),profile),roundtrip(make(200,20,30,1,true),profile)};
        const auto hp=make_plan(hf,{vertex_interface(hf,0,1,20)});const auto input=hp.glue_field(g1,inputs(hf,1),availability(2));const auto applied=hp.boundary(g1).execute(input,availability(2));
        check(applied.sparse_values().size()==6 && applied.sparse_values().entries()==S(6,{{0,-2},{3,2}}).entries(),"heterogeneous actual downward action");
        check(hp.view().layout(g0)[0].dimension==1 && hp.view().layout(g0)[1].dimension==2 && hp.view().layout(g0)[2].dimension==3,"heterogeneous quotient stalk types");
        Guard guard;const auto local=hp.view().restrict_field(g0,applied.sparse_values(),1,guard);const auto records=hp.view().patches()[1].export_downward(local,6000,7000);auto endpoint=hf[1].endpoint;
        endpoint.tensors.insert(endpoint.tensors.end(),records.tensors.begin(),records.tensors.end());endpoint.declarations->values.insert(endpoint.declarations->values.end(),records.values.begin(),records.values.end());
        const auto export_wire=encode_temporal_endpoint(endpoint);const auto export_source=encode_rfcx(endpoint.source);const auto export_back=decode_temporal_endpoint(MappedRfcx(export_source.bytes).materialize(),export_wire);
        check(encode_temporal_endpoint(export_back)==export_wire,"actual typed boundary restriction exported and reopened");
        const auto& arena=fs[0].endpoint.source.grade(g1);check(arena.incidence_count()==6 && arena.aspects.get(fs[0].endpoint.source.signature().aspects().id("gate"),2),"retained zero-role occurrence survives local round trip");
        std::cout<<"local_gluing_lifecycle profile="<<unsigned(profile)<<" output_bytes="<<wire.size()<<'\n';
    }
    std::cout<<"local_gluing_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
