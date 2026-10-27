#include "../../examples/declared_local_field_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace declared_field_example;
namespace {
std::size_t checks{};
void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F&& f,const char* s){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,s);}
template<class A,class B>bool fields_equal(const A& a,const B& b){
    if(a.size()!=b.size())return false;
    auto ai=a.begin();auto bi=b.begin();
    for(;ai!=a.end();++ai,++bi)if(ai->first!=bi->first || ai->second.size()!=bi->second.size() || ai->second.entries()!=bi->second.entries())return false;
    return true;
}
void lifecycle(bool large){
    auto f=make(large,large?1000003:7);
    f.endpoint.declarations->profile=BooleanLayoutProfile::Lsb0OctetsV2;
    f.endpoint.declarations->resource_limits={};
    const DeclaredLocalFieldBinding before(f.endpoint,f.contract);
    const auto expected=field::SparseStalkFieldActions(before.fibers()).boundary(before.bind_downward(g1,f.down_inputs));
    const auto expected_up=local::SparseSheafActions(before.sheaf()).coboundary(before.bind_upward(g0,f.up_inputs));
    auto next=f.endpoint;next.sequence=1;
    auto& target=*next.declarations;const auto& base=*f.endpoint.declarations;
    auto definitions=target.registry.definitions();std::vector<DeclarationMap> maps;
    for(auto& d:definitions){const auto old=d.id;++d.id.version;maps.push_back({old,d.id,DeclarationMapKind::Recode,{}});
        for(auto& factor:d.factors)++factor.type.version;
        if(d.element.valid())++d.element.version;
        if(d.id.key==901)std::reverse(d.factors.begin(),d.factors.end());
    }
    target.registry=DeclarationRegistry(std::move(definitions));
    const DeclarationRecoding recoding(base.registry,target.registry,maps,{}, {base.profile,target.profile});
    for(auto& value:target.values){value.value=recoding.apply(value.type,value.value);value.type=recoding.target_id(value.type);}
    for(auto& layout:target.source_layouts)layout.type=recoding.target_id(layout.type);
    target.programs={recode_declared_program(base,target,base.programs.front(),901,maps)};
    for(auto* family:{&f.contract.downward,&f.contract.upward})for(auto& b:family->stalks)++b.declaration.id.version;
    const auto update=make_temporal_declaration_update(f.endpoint.declarations,next.declarations,maps);
    check(update.replaced_values.empty(),"pure recoding called data replacement");
    const auto segment=make_temporal_segment(f.endpoint,next,{}, {},RelationalCodeEncoding::Automatic,{},update);
    check(segment.source.grades.empty() && segment.tensors.empty(),"recoding altered source or numeric fields");
    const auto source_wire=encode_rfcx(f.endpoint.source);
    const auto base_wire=encode_temporal_endpoint(f.endpoint);
    TemporalReadLimits policy;if(large)policy.max_dimension=1000003;
    const auto restored=decode_temporal_endpoint(MappedRfcx(source_wire.bytes).materialize(),base_wire,policy);
    const auto wire=encode_temporal_segment(segment);
    const auto decoded=decode_temporal_segment(wire,policy);
    check(encode_temporal_segment(decoded)==wire,"transition bytes noncanonical");
    const auto replay=replay_temporal_segment(restored,decoded);
    check(encode_temporal_endpoint(replay)==encode_temporal_endpoint(next),"replay lost typed field");
    check(replay.declarations->profile==base.profile,"replay selected another profile");
    const DeclaredLocalFieldBinding after(replay,f.contract);
    check(!after.source_owner().same_owner(before.source_owner()),"replay reused runtime authority");
    check(after.fibers().source_owner().same_owner(after.sheaf().source_owner()),"action families lost shared source");
    const auto down=field::SparseStalkFieldActions(after.fibers()).boundary(after.bind_downward(g1,f.down_inputs));
    const auto up=local::SparseSheafActions(after.sheaf()).coboundary(after.bind_upward(g0,f.up_inputs));
    check(fields_equal(expected.field.values(),down.field.values()),"downward exact result changed");
    check(fields_equal(expected_up.field.values(),up.field.values()),"upward exact result changed");
    const auto old_bits=execute_declared_source_program(restored.source,*restored.declarations,902,g1);
    const auto new_bits=execute_declared_source_program(replay.source,*replay.declarations,902,g1);
    check(old_bits.words()==new_bits.words() && new_bits.count()==1 && new_bits.get(2),"retargeted bits lost zero-magnitude role");
    check(target.programs.front().program.instructions().front().aspect==1,"factor-order operand not retargeted");
    check(target.programs.front().program.instruction_count()==base.programs.front().program.instruction_count(),"instruction count changed");
    auto out=after.endpoint();const auto outputs=after.export_downward(down.field,300,1300);
    out.tensors.insert(out.tensors.end(),outputs.tensors.begin(),outputs.tensors.end());
    out.declarations->values.insert(out.declarations->values.end(),outputs.values.begin(),outputs.values.end());
    const auto out_wire=encode_temporal_endpoint(out);
    const auto again=decode_temporal_endpoint(MappedRfcx(source_wire.bytes).materialize(),out_wire,policy);
    check(encode_temporal_endpoint(again)==out_wire,"export canonical round trip");
    for(const auto& v:again.declarations->values)if(const auto* t=std::get_if<T>(&v.value.data))
        check(t->support().storage()==numeric::StructuralSupport::Storage::Full,"support expanded during replay/export");
    auto wrong=next.declarations;wrong->values.front().cell=StableCellKey{g0,10};
    const auto changed=make_temporal_declaration_update(f.endpoint.declarations,wrong,maps);
    check(changed.replaced_values.size()==1,"owner change hidden as recoding");
    auto unrecorded=changed;unrecorded.replaced_values.clear();
    rejects([&]{validate_temporal_declaration_transition(f.endpoint.declarations,unrecorded);},"foreign owner accepted as recoding");
    if(large)check(wire.size()<10000 && out_wire.size()<15000,"large replay expanded implicit support");
    std::cout<<"recode_field "<<(large?"1000003":"heterogeneous")<<" segment_bytes="<<wire.size()<<" output_bytes="<<out_wire.size()<<'\n';
}
}
int main(){try{lifecycle(false);lifecycle(true);std::cout<<"symbolic_recode_field checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
