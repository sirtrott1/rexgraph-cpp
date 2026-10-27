#include "../../examples/sheaf_gluing_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace sheaf_gluing_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const std::string& reason){bool rejected=false;try{f();}catch(const std::exception& e){if(std::string(e.what()).find(reason)==std::string::npos)throw;rejected=true;}check(rejected,"expected refusal absent");}
TemporalEndpoint reopened(TemporalEndpoint e,BooleanLayoutProfile profile){
    if(e.declarations)e.declarations->profile=profile;
    const auto src=encode_rfcx(e.source);const auto wire=encode_temporal_endpoint(e);auto out=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),wire);
    check(encode_temporal_endpoint(out)==wire && encode_rfcx(out.source).bytes==src.bytes,"canonical existing source/endpoint bytes");return out;
}
void mutate_matrix(TemporalEndpoint& e,std::uint64_t key,std::size_t i,std::size_t j,Q delta){for(auto& t:e.tensors)if(t.key==key){auto m=temporal_tensor_matrix(t);m.set(i,j,m.get(i,j)+delta);t.tensor=temporal_matrix_tensor(m);return;}throw std::logic_error("missing example record");}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        auto d=dataset(true);const auto old=plan(d);const auto expected=result(old,d);
        for(auto& f:d.locals)f.endpoint=reopened(f.endpoint,profile);
        for(auto& f:d.commons)f.endpoint=reopened(f.endpoint,profile);
        d.certificates=reopened(d.certificates,profile);
        const auto prepared=prepare(d);auto ep=coordinates(prepared.view());
        TemporalDeclarationState ds;ds.namespace_key=49000;ds.profile=profile;ds.registry=DeclarationRegistry({Declaration::rational({49000,1,1})});ep.declarations=std::move(ds);
        const auto p=bind_plan(prepared,ep);const auto r=result(p,d);
        check(!old.view().patches()[0].source_owner().same_owner(p.view().patches()[0].source_owner()),"reopening creates new source owners");
        check(r.total==2238 && r.total==expected.total && r.aggregate.sparse_values().entries()==expected.aggregate.sparse_values().entries(),"actual persisted restrictions and fields drive moments");
        check(r.aggregate.publications().size()>=8,"all local common certificate and coordinate dependencies");
        for(std::size_t i=0;i<3;++i){TemporalTensor t;t.key=3000+i;t.role=TemporalTensorRole::Field;t.output=r.components[i].field.carrier().key;t.tensor={r.components[i].field.carrier().declaration->domain,numeric::StructuralSupport::full(5),r.components[i].field.sparse_values()};ep.tensors.push_back(t);}
        TemporalDeclaredValue scalar;scalar.key=4000;scalar.type={49000,1,1};scalar.value=DeclaredValue::rational(r.total);ep.declarations->values.push_back(scalar);
        const auto re=reopened(ep,profile);const auto next=bind_plan(prepare(d),re);const auto again=result(next,d);
        for(std::size_t i=0;i<3;++i){check(re.tensors[i].tensor.values().entries()==again.components[i].field.sparse_values().entries(),"stored actual output reexecuted");check(re.tensors[i].tensor.support().count()==5,"supported zero coordinates not omitted");}
        check(again.total==2238 && !r.aggregate.carrier().same(again.aggregate.carrier()),"fresh result coordinate owner and exact moment");
        for(std::size_t i=0;i<available().size();++i){auto a=available();a[i].max_sequence=6;rejects([&]{p.glue_field(g0,inputs(d.locals,0),a);},"avail");rejects([&]{p.coboundary(g0).execute(r.input,a);},"avail");}
        Guard g;const auto local=p.view().restrict_field(g1,r.aggregate.sparse_values(),1,g);const auto& binding=p.view().patches()[1];const auto exported=binding.export_upward(local,6000,7000);auto local_ep=d.locals[1].endpoint;
        local_ep.tensors.insert(local_ep.tensors.end(),exported.tensors.begin(),exported.tensors.end());local_ep.declarations->values.insert(local_ep.declarations->values.end(),exported.values.begin(),exported.values.end());
        const auto local_re=reopened(local_ep,profile);const DeclaredLocalFieldBinding rebound(local_re,d.locals[1].contract);std::vector<DeclaredFieldInput> fields;
        for(std::size_t i=0;i<exported.tensors.size();++i)fields.push_back({exported.tensors[i].key,exported.values[i].key});
        const auto recovered=rebound.bind_upward(g1,fields);check(recovered.values().size()==local.values().size(),"local upward output inventory retained");
        for(const auto& [cell,v]:local.values())check(v.entries()==recovered.values().at(cell).entries(),"local sheaf output readout exports under its owner");
        rejects([&]{rebound.export_upward(local,8000,9000);},"foreign");
        // False certificates can be canonical bytes. Admission still rejects them.
        auto bad=d;mutate_matrix(bad.certificates,bad.records[0].compatibility_lift.key,0,0,1);bad.certificates=reopened(bad.certificates,profile);rejects([&]{prepare(bad);},"omits compatible directions");
        bad=d;mutate_matrix(bad.locals[0].endpoint,bad.locals[0].contract.upward.maps[0].tensor,0,1,1);bad.locals[0].endpoint=reopened(bad.locals[0].endpoint,profile);rejects([&]{prepare(bad);},"naturality");
        bad=d;set_upward(bad.locals[1],g0,0,S(3,{{0,4},{1,5},{2,7}}));bad.locals[1].endpoint=reopened(bad.locals[1].endpoint,profile);const auto badplan=plan(bad);rejects([&]{result(badplan,bad);},"disagree");
        // Partial support is not permission to assume missing coordinates are zero.
        bad=d;const auto field=bad.locals[0].inputs[0][0];
        for(auto& t:bad.locals[0].endpoint.tensors)if(t.key==field.tensor){t.tensor={t.tensor.domain(),numeric::StructuralSupport::sparse(2,{0}),S(2,{{0,5}})};for(auto& v:bad.locals[0].endpoint.declarations->values)if(v.key==field.value)v.value=DeclaredValue::tensor(t.tensor);}
        bad.locals[0].endpoint=reopened(bad.locals[0].endpoint,profile);const auto partial=plan(bad);rejects([&]{result(partial,bad);},"partial input");
        // A fully supplied zero is accepted and remains a supported output record.
        auto zeros=d;for(auto& f:zeros.locals)for(std::size_t cell=0;cell<2;++cell)set_upward(f,g0,static_cast<std::uint32_t>(cell),S(f.contract.upward.stalks[cell].carrier.domain.logical_size()));
        const auto zp=plan(zeros);const auto zr=result(zp,zeros);check(zr.total==0 && zr.aggregate.sparse_values().empty(),"total zero is valid");
        auto zep=coordinates(zp.view());TemporalTensor z;z.key=9000;z.role=TemporalTensorRole::Field;z.output=zr.aggregate.carrier().key;z.tensor={zr.aggregate.carrier().declaration->domain,numeric::StructuralSupport::full(5),zr.aggregate.sparse_values()};zep.tensors.push_back(z);
        const auto zz=reopened(zep,profile);check(zz.tensors[0].tensor.support().count()==5 && zz.tensors[0].tensor.values().empty(),"zero result support survives codecs");
    }
    std::cout<<"sheaf_cover_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
