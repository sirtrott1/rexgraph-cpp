#include "../../examples/declared_local_field_data.hpp"
#include <iostream>
#include <type_traits>
using namespace declared_field_example;
namespace {
std::size_t checks{};
void check(bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);}
template<class F>void reject(F fn,const char* msg){bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}catch(const std::length_error&){caught=true;}check(caught,msg);}
TemporalEndpoint reopen(const TemporalEndpoint& e, TemporalReadLimits limits={}) {
    const auto source=encode_rfcx(e.source);
    const auto bytes=encode_temporal_endpoint(e);
    MappedRfcx mapped(source.bytes);
    return decode_temporal_endpoint(mapped.materialize(),bytes,limits);
}
Q at(const S& x,std::size_t coordinate) { for(const auto& v:x.entries())if(v.index==coordinate)return v.value;return 0; }
void append(TemporalEndpoint& e, const DeclaredFieldRecords& r) {
    e.tensors.insert(e.tensors.end(),r.tensors.begin(),r.tensors.end());
    e.declarations->values.insert(e.declarations->values.end(),r.values.begin(),r.values.end());
}
bool equal(const field::SparseStalkValues& a,const field::SparseStalkValues& b){
    if(a.size()!=b.size())return false;
    for(const auto& [k,v]:a)if(!b.contains(k)||!temporal_endpoint_detail::values_equal(v,b.at(k)))return false;
    return true;
}
void lifecycle(){
    auto f=make();
    const auto bytes=encode_temporal_endpoint(f.endpoint);
    DeclaredLocalFieldBinding original(f.endpoint,f.contract);
    auto e=reopen(f.endpoint);
    check(encode_temporal_endpoint(e)==bytes,"V1 endpoint round trip differs");
    DeclaredLocalFieldBinding bound(e,f.contract);
    check(!bound.source_owner().same_owner(original.source_owner()),"decode reuses old runtime source owner");
    check(bound.fibers().source_owner().same_owner(bound.sheaf().source_owner()),"directional sources differ");
    check(&bound.fibers().fibers().complex()==&bound.sheaf().sheaf().complex(),"native source addresses differ");
    check(bound.same_owner(DeclaredLocalFieldBinding(bound)),"handle copy loses identity");
    check(bound.fibers().fibers().stalks().dimension({g1,0})==2 && bound.sheaf().sheaf().stalks().dimension({g1,0})==3,"families conflated");
    const auto& src=bound.source_owner().source();
    check(src.grade(g1).incidence_count()==6,"occurrences lost");
    for(std::size_t i=0;i<6;++i){
        check(src.occurrence_identity(g1,i)==f.endpoint.source.occurrence_identity(g1,i),"occurrence key changed");
        check(src.incidence_coefficient(g1,i)==f.endpoint.source.incidence_coefficient(g1,i),"coefficient changed");
    }
    check(src.grade(g1).aspects.get(src.signature().aspects().id("gate"),2),"present zero gate lost");
    check(!src.grade(g1).aspects.get(src.signature().existence_aspect(),3),"inactive occurrence lost");
    check(bound.fibers().fibers().transport_override(g1,2).has_value(),"explicit zero map lost");
    check(bound.fibers().fibers().transport_override(g1,0)->get(0,0)==-6,"coefficient was not applied once");
    auto input=bound.bind_downward(g1,f.down_inputs);
    auto obs=bound.bind_upward(g0,f.up_inputs);
    auto d=field::SparseStalkFieldActions(bound.fibers()).boundary(input);
    auto u=local::SparseSheafActions(bound.sheaf()).coboundary(obs);
    check(d.field.values().at(0).entries()==S(1,{{0,-18}}).entries(),"wrong rectangular down action");
    check(d.field.values().at(1).entries()==S(2,{{0,3},{1,4}}).entries(),"wrong second down action");
    check(d.field.nonzeros()==3,"zero/cancelling entries changed result");
    check(u.field.values().at(0).entries()==S(3,{{0,-18},{2,-60}}).entries(),"restriction sign/coordinates wrong");
    const auto d0=field::SparseStalkFieldActions(original.fibers()).boundary(original.bind_downward(g1,f.down_inputs));
    const auto u0=local::SparseSheafActions(original.sheaf()).coboundary(original.bind_upward(g0,f.up_inputs));
    check(equal(d0.field.values(),d.field.values()),"down round-trip action changed");
    check(equal(u0.field.values(),u.field.values()),"up round-trip action changed");
    auto dr=bound.export_downward(d.field,300,1300);auto ur=bound.export_upward(u.field,400,1400);
    check(dr.tensors.size()==3 && dr.values.size()==3,"missing total output records");
    check(dr.tensors[2].tensor.support().count()==3 && dr.tensors[2].tensor.values().empty(),"supported zero output omitted");
    auto result=bound.endpoint();append(result,dr);append(result,ur);
    auto result2=reopen(result);DeclaredLocalFieldBinding again(result2,f.contract);
    const auto reread=again.bind_upward(g1,{{400,1400}});
    check(equal(reread.values(),u.field.values()),"encoded result changed");
    check(encode_temporal_endpoint(result2)==encode_temporal_endpoint(result),"result canonical bytes differ");
    const auto& stored=again.endpoint().declarations->programs.at(0);
    check(stored.program.instructions()==f.endpoint.declarations->programs[0].program.instructions(),"producer instruction record changed");
    // The stored loads use layout factor ordinals, not the raw source AspectIds.
    const auto& reopened_src=again.source_owner().source();
    const auto mask=stored.program.execute_domain_word_with_provider(6,0,stored.output,[&](composite::AspectId a,std::size_t word){
        return reopened_src.grade(g1).aspects.plane(a==0?reopened_src.signature().existence_aspect():reopened_src.signature().aspects().id("gate")).words().at(word);
    });
    check(mask==4,"retained zero gate cannot be selected by stored Boolean operation");
    reject([&]{(void)field::SparseStalkFieldActions(bound.fibers()).boundary(d0.field);},"stale field accepted");
    reject([&]{(void)bound.export_downward(d0.field,500,1500);},"foreign output exported");
    reject([&]{(void)bound.export_upward(u0.field,500,1500);},"foreign sheaf exported");
    reject([&]{(void)bound.export_downward(d.field,0,1500);},"zero output ID accepted");
    // Snapshot lifetime: subsequent edits of the caller's data do not change a binding.
    f.endpoint.tensors.clear(); e.tensors.clear();
    const auto after=field::SparseStalkFieldActions(bound.fibers()).boundary(bound.bind_downward(g1,f.down_inputs));
    check(at(after.field.values().at(0),0)==-18,"caller mutation changed binding");
}
void invalid(){
    const auto f=make();
    const auto fail=[&](auto edit,const char* msg){auto x=f;edit(x);reject([&]{DeclaredLocalFieldBinding bad(x.endpoint,x.contract);},msg);};
    fail([](auto& x){x.endpoint.declarations.reset();},"missing declarations accepted");
    fail([](auto& x){x.contract.downward_profile=static_cast<DeclaredDownwardProfile>(99);},"unknown profile accepted");
    fail([](auto& x){x.contract.downward.stalks[0].carrier.coordinate_system++;},"wrong coordinate system accepted");
    fail([](auto& x){x.contract.downward.stalks[0].carrier.cell=StableCellKey{g0,999};},"foreign cell accepted");
    fail([](auto& x){x.contract.downward.stalks[1].declaration.axes[0].coordinates={10202,10201};},"reordered coordinates accepted");
    fail([](auto& x){x.contract.downward.stalks[0].declaration=Declaration::boolean(id(900));},"Boolean stalk accepted");
    fail([](auto& x){x.contract.downward.stalks.pop_back();},"missing cell declaration accepted");
    fail([](auto& x){x.contract.upward.stalks.push_back(x.contract.upward.stalks[0]);},"duplicate cell accepted");
    fail([](auto& x){x.contract.downward.maps[0].role=TemporalTensorRole::SheafRestriction;},"wrong map direction accepted");
    fail([](auto& x){x.contract.downward.maps.pop_back();x.contract.downward.allow_constant_identity=true;},"omitted stored map defaulted");
    fail([](auto& x){x.contract.downward.maps.push_back(x.contract.downward.maps[0]);},"duplicate map accepted");
    fail([](auto& x){x.contract.downward.maps[0].tensor=999;},"missing selected map accepted");
    fail([](auto& x){x.endpoint.tensors[4].input=113;},"foreign map coordinates accepted");
    fail([](auto& x){x.endpoint.tensors[4].tensor=temporal_matrix_tensor(M(2,1));},"wrong map shape accepted");
    fail([](auto& x){auto& t=x.endpoint.tensors[4];t.tensor=T(t.tensor.domain(),numeric::StructuralSupport::sparse(2,{}),S(2));},"partial map accepted");
    fail([](auto& x){auto& c=x.contract.downward.stalks[0];c.declaration.axes[0].extent=0;},"zero dimension accepted");
    DeclaredLocalFieldBinding b(f.endpoint,f.contract);
    reject([&]{(void)b.bind_downward(g1,{});},"missing total input accepted");
    reject([&]{(void)b.bind_upward(g0,{{110,1110}});},"missing zero input accepted");
    reject([&]{(void)b.bind_downward(g1,{{103,1103},{103,1103}});},"duplicate input accepted");
    reject([&]{(void)b.bind_upward(g1,{{103,1103}});},"physical field used as observation coordinates");
    reject([&]{(void)b.bind_downward({99},{});},"foreign grade accepted");
    reject([&]{(void)b.bind_downward(g1,{{999,1103}});},"foreign tensor accepted");
    auto partial=f;auto& t=partial.endpoint.tensors[0];
    t.tensor=T(t.tensor.domain(),numeric::StructuralSupport::sparse(2,{0}),S(2,{{0,3}}));
    partial.endpoint.declarations->values[0].value=DeclaredValue::tensor(t.tensor);
    DeclaredLocalFieldBinding pb(partial.endpoint,partial.contract);
    reject([&]{(void)pb.bind_downward(g1,partial.down_inputs);},"partial observed support authorized total input");
    auto inconsistent=f;inconsistent.endpoint.declarations->values[0].value=DeclaredValue::tensor(T::zero(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2)));
    DeclaredLocalFieldBinding ib(inconsistent.endpoint,inconsistent.contract);
    reject([&]{(void)ib.bind_downward(g1,inconsistent.down_inputs);},"declared/field values disagree but accepted");
    auto limits=DeclaredLocalFieldBinding::default_limits();limits.steps=0;
    reject([&]{DeclaredLocalFieldBinding bad(f.endpoint,f.contract,limits);},"prepare work limit ignored");
    limits=DeclaredLocalFieldBinding::default_limits();limits.integer_bits=1;
    reject([&]{DeclaredLocalFieldBinding bad(f.endpoint,f.contract,limits);},"prepare integer limit ignored");
    limits=DeclaredLocalFieldBinding::default_limits();limits.entries=0;
    reject([&]{DeclaredLocalFieldBinding bad(f.endpoint,f.contract,limits);},"prepare entry limit ignored");
    auto bytes=encode_temporal_endpoint(f.endpoint);bytes.back()^=std::byte{1};
    reject([&]{(void)decode_temporal_endpoint(f.endpoint.source,bytes);},"damaged bytes accepted");
    TemporalReadLimits rl;rl.max_records=2;
    reject([&]{(void)reopen(f.endpoint,rl);},"decode record limit ignored");
    rl={};rl.max_file_bytes=16;
    reject([&]{(void)reopen(f.endpoint,rl);},"decode byte limit ignored");
    rl={};rl.max_dimension=1;
    reject([&]{(void)reopen(f.endpoint,rl);},"decode dimension limit ignored");
}
void profiles(){
    auto f=make();
    // First down map is already realized: native must NOT multiply it by -3.
    auto& t=f.endpoint.tensors[4];t.role=TemporalTensorRole::RealizedIncidenceMap;
    f.contract.downward.maps[0].role=t.role;
    DeclaredLocalFieldBinding b(f.endpoint,f.contract);
    auto d=field::SparseStalkFieldActions(b.fibers()).boundary(b.bind_downward(g1,f.down_inputs));
    check(at(d.field.values().at(0),0)==6,"already-realized map signed twice");
    auto& gate=f.endpoint.tensors[6];gate.role=TemporalTensorRole::RealizedIncidenceMap;
    gate.tensor=temporal_matrix_tensor(M::from_rows({{2,0},{0,0},{0,0}}));
    f.contract.downward.maps[2].role=gate.role;
    reject([&]{DeclaredLocalFieldBinding bad(f.endpoint,f.contract);},"physical profile allows nonzero map on zero coefficient");
    f.contract.downward_profile=DeclaredDownwardProfile::CompleteRealizedOverrides;
    DeclaredLocalFieldBinding generalized(f.endpoint,f.contract);
    d=field::SparseStalkFieldActions(generalized.fibers()).boundary(generalized.bind_downward(g1,f.down_inputs));
    check(at(d.field.values().at(2),0)==6,"generalized realized map behavior changed");
}
std::size_t sparse(std::size_t n){
    auto f=make(true,n);DeclaredLocalFieldBinding b(f.endpoint,f.contract);
    check(!b.fibers().fibers().transport_override(g1,0),"identity down default materialized");
    check(!b.sheaf().sheaf().restriction_override(g1,0),"identity up default materialized");
    auto d=field::SparseStalkFieldActions(b.fibers()).boundary(b.bind_downward(g1,f.down_inputs));
    check(at(d.field.values().at(0),0)==2 && at(d.field.values().at(0),n-1)==Q{-12,7},"sparse exact output wrong");
    check(d.field.nonzeros()==4,"unused extent produced numeric entries");
    auto bad=f;bad.contract.downward.allow_constant_identity=false;
    reject([&]{DeclaredLocalFieldBinding x(bad.endpoint,bad.contract);},"undeclared default accepted");
    bad=f;bad.endpoint.carriers[0].coordinate_system=2;bad.contract.downward.stalks[0].carrier.coordinate_system=2;
    reject([&]{DeclaredLocalFieldBinding x(bad.endpoint,bad.contract);},"equal dimensions identified different coordinate systems");
    return d.work.scalar_products;
}
}
int main(){try{lifecycle();invalid();profiles();const auto a=sparse(7),b=sparse(1000003);check(a==b,"action work follows unused extent");
std::cout<<"declared_local_field checks="<<checks<<" passed; sparse products="<<a<<","<<b<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
