#include "../../examples/chart_gluing_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace chart_gluing_example;
std::size_t checks=0;void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
Fixture reopen(Fixture f,BooleanLayoutProfile profile){
    f.endpoint.declarations->profile=profile;const auto source=encode_rfcx(f.endpoint.source);const auto bytes=encode_temporal_endpoint(f.endpoint);
    f.endpoint=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),bytes);
    check(encode_temporal_endpoint(f.endpoint)==bytes,"canonical local chart endpoint bytes");
    check(encode_rfcx(f.endpoint.source).bytes==source.bytes,"retained source unchanged");return f;
}
int main(){try{
    for(auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        const auto first=changed(make(100,10,20,1,false,true),0,10900),last=changed(make(300,20,30),2,10902);
        auto charts=first.charts;charts.insert(charts.end(),last.charts.begin(),last.charts.end());
        std::vector<Fixture> before{first.field,make(200,30,40),last.field};std::vector<Fixture> fs;
        for(const auto& f:before)fs.push_back(reopen(f,profile));
        auto left=vertex_interface(fs,0,2,20),right=vertex_interface(fs,1,2,30);
        for(auto* x:{&left,&right}){const auto wire=encode_rfcx(x->common.source());x->common=core::OwnedRelationalSource(MappedRfcx(wire.bytes).materialize());}
        const auto prepared=PreparedNativeLocalGluing::prepare_with_charts(publications(fs),{left,right},charts);
        auto coords=coordinate_endpoint(prepared.view());TemporalDeclarationState d;d.namespace_key=12000;d.profile=profile;d.registry=DeclarationRegistry({Declaration::rational({12000,1,1})});coords.declarations=d;
        const auto p=bind(prepared,coords);const auto r=third(p,fs);check(r.total==5 && r.matches_reference==true,"reopened chart sources feed native action moment result");
        check(moments(p,fs)==std::array<Q,3>{1,2,5},"complete selected moment sequence");
        std::uint64_t id=3000;for(const auto& c:r.components){TemporalTensor t;t.key=id++;t.role=TemporalTensorRole::Field;t.output=c.field.carrier().key;
            t.tensor={c.field.carrier().declaration->domain,numeric::StructuralSupport::full(c.field.carrier().size()),c.field.sparse_values()};coords.tensors.push_back(t);}
        TemporalDeclaredValue v;v.key=4000;v.type={12000,1,1};v.value=DeclaredValue::rational(r.total);coords.declarations->values.push_back(v);
        const auto bytes=encode_temporal_endpoint(coords);const auto source=encode_rfcx(coords.source);const auto restored=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),bytes);
        check(encode_temporal_endpoint(restored)==bytes,"actual moment component output bytes stable");
        const auto fresh=bind(PreparedNativeLocalGluing::prepare_with_charts(publications(fs),{left,right},charts),restored);const auto again=third(fresh,fs);
        check(!again.aggregate.carrier().same(r.aggregate.carrier()),"reconstructed fresh global carrier owner");
        check(again.total==r.total && again.aggregate.sparse_values().entries()==r.aggregate.sparse_values().entries(),"actual output replay");
        for(std::size_t i=0;i<3;++i)check(restored.tensors[i].tensor.values().entries()==again.components[i].field.sparse_values().entries(),"stored component rechecked against reconstructed operation");
        check(restored.tensors[1].tensor.support().count()==3 && restored.tensors[1].tensor.values().nonzeros()==0,"supported zero component preserved");
        check(again.aggregate.publications().size()>=4,"chart endpoints and quotient publication retained");
        auto a=availability(3);a[1].max_sequence=6;bool refused=false;try{fresh.boundary(g1).algebraic_transpose().execute(again.input,a);}catch(const std::exception&){refused=true;}check(refused,"later chart source rejected under old cutoff");
        const auto& ar=fs[0].endpoint.source.grade(g1);check(ar.aspects.get(fs[0].endpoint.source.signature().aspects().id("gate"),2),"zero-magnitude role bit survives chart data path");
        // Actual common boundary returns through the inverse chart, then the
        // existing typed export stores it in the original local coordinates.
        const auto input=fresh.glue_field(g1,inputs(fs,1),availability(3));const auto output=fresh.boundary(g1).execute(input,availability(3));Guard guard;
        const auto local=fresh.view().restrict_field(g0,output.sparse_values(),0,guard);const auto records=fresh.view().patches()[0].export_downward(local,6000,7000);
        auto e=fs[0].endpoint;e.tensors.insert(e.tensors.end(),records.tensors.begin(),records.tensors.end());e.declarations->values.insert(e.declarations->values.end(),records.values.begin(),records.values.end());
        const auto wire=encode_temporal_endpoint(e);const auto re=decode_temporal_endpoint(e.source,wire);check(encode_temporal_endpoint(re)==wire,"inverse-chart actual field exported and reopened");
        // Persisted chart map mismatch cannot be laundered by canonical decoding.
        auto bad=fs;for(auto& t:bad[0].endpoint.tensors)if(t.key==charts[0].reconstruction){t.tensor=temporal_matrix_tensor(M::from_rows({{3}}));}
        bad[0]=reopen(bad[0],profile);refused=false;try{PreparedNativeLocalGluing::prepare_with_charts(publications(bad),{left,right},charts);}catch(const std::exception&){refused=true;}check(refused,"canonical invalid inverse still rejected on rebinding");
        std::cout<<"chart_lifecycle profile="<<unsigned(profile)<<" output_bytes="<<bytes.size()<<'\n';
    }
    std::cout<<"chart_gluing_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
