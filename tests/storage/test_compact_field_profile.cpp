#include "../../examples/declared_local_field_data.hpp"
#include <iostream>
using namespace declared_field_example;
namespace {
std::size_t checks{};
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class A,class B>bool fields_equal(const A& a,const B& b){
    if(a.size()!=b.size())return false;
    auto ai=a.begin();auto bi=b.begin();
    for(;ai!=a.end();++ai,++bi)if(ai->first!=bi->first || ai->second.size()!=bi->second.size() || ai->second.entries()!=bi->second.entries())return false;
    return true;
}
void lifecycle(bool large){
    auto f=make(large,large?1000003:7);
    const auto original_defs=f.endpoint.declarations->registry.definitions();
    const DeclaredLocalFieldBinding before(f.endpoint,f.contract);
    const auto reference=field::SparseStalkFieldActions(before.fibers()).boundary(before.bind_downward(g1,f.down_inputs));
    f.endpoint.declarations->profile=BooleanLayoutProfile::Lsb0OctetsV2;
    f.endpoint.declarations->resource_limits={}; // No million-index V1 work allowance.
    const auto src=encode_rfcx(f.endpoint.source);const auto wire=encode_temporal_endpoint(f.endpoint);
    TemporalReadLimits read_limits; if(large)read_limits.max_dimension=1000003;
    const auto restored=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),wire,read_limits);
    check(restored.declarations->profile==BooleanLayoutProfile::Lsb0OctetsV2,"profile lost in endpoint");
    check(restored.declarations->registry.definitions()==original_defs,"declaration identities changed");
    check(encode_temporal_endpoint(restored)==wire,"noncanonical V2 endpoint");
    const DeclaredLocalFieldBinding after(restored,f.contract);
    check(!before.source_owner().same_owner(after.source_owner()),"restored runtime authority aliased");
    check(after.fibers().source_owner().same_owner(after.sheaf().source_owner()),"shared source lost");
    const auto down=field::SparseStalkFieldActions(after.fibers()).boundary(after.bind_downward(g1,f.down_inputs));
    check(fields_equal(reference.field.values(),down.field.values()),"V2 changed native downward result");
    const auto u0=local::SparseSheafActions(before.sheaf()).coboundary(before.bind_upward(g0,f.up_inputs));
    const auto u1=local::SparseSheafActions(after.sheaf()).coboundary(after.bind_upward(g0,f.up_inputs));
    check(fields_equal(u0.field.values(),u1.field.values()),"V2 changed native upward result");
    auto out=after.endpoint();const auto records=after.export_downward(down.field,300,1300);
    out.tensors.insert(out.tensors.end(),records.tensors.begin(),records.tensors.end());
    out.declarations->values.insert(out.declarations->values.end(),records.values.begin(),records.values.end());
    const auto output=encode_temporal_endpoint(out);const auto reopened=decode_temporal_endpoint(MappedRfcx(src.bytes).materialize(),output,read_limits);
    check(encode_temporal_endpoint(reopened)==output,"export/reopen changed output");
    const auto& stored=reopened.declarations->programs.front();
    check(stored.program.instructions()==f.endpoint.declarations->programs.front().program.instructions(),"producer wordcode changed");
    const auto& native_source=after.source_owner().source();
    const auto selected=stored.program.execute_domain_word_with_provider(6,0,stored.output,[&](composite::AspectId a,std::size_t word){
        return native_source.grade(g1).aspects.plane(a==0?native_source.signature().existence_aspect():native_source.signature().aspects().id("gate")).words().at(word);
    });
    check(selected==4,"stored Boolean operation lost the present zero-magnitude role");
    for(const auto& v:reopened.declarations->values)if(const auto* t=std::get_if<T>(&v.value.data))check(t->support().storage()==numeric::StructuralSupport::Storage::Full,"endpoint support expanded");
    if(large)check(wire.size()<10000 && output.size()<15000,"large endpoint unexpectedly expanded");
    std::cout<<"V2 field "<<(large?"1000003":"heterogeneous")<<" input_bytes="<<wire.size()<<" result_bytes="<<output.size()<<'\n';
}
}
int main(){try{lifecycle(false);lifecycle(true);std::cout<<"compact_field_profile checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
