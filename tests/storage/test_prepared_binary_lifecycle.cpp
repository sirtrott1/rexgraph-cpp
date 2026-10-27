#include "../../examples/prepared_binary_source_data.hpp"
#include "rfc/storage/prepared_relational_binary.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include "rfc/storage/relational_codes.hpp"
#include <iostream>
using namespace prepared_binary_source_example;
namespace {
std::size_t checks=0;
void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F&& f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,"expected binding rejection");}
void run(BooleanLayoutProfile profile){
    auto f=prepared_binary_source_example::make();f.endpoint.declarations->profile=profile;
    const auto original=encode_temporal_endpoint(f.endpoint);const auto source=encode_rfcx(f.endpoint.source);
    const auto reopened=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),original);
    const PreparedRelationalBinaryOperation before(*f.endpoint.codes,166),after(*reopened.codes,166);
    check(!before.same_owner(after),"reopen reused prepared owner");
    check(encode_temporal_endpoint(reopened)==original,"reopen changed records");
    const auto& d=*reopened.declarations;const auto& layout=d.source_layouts.front();
    const core::SourceAspectLayoutBinding binding(reopened.source.signature(),CompiledLayout(d.registry,layout.type,d.profile),layout.factors);
    const auto& planes=reopened.source.grade(g1).aspects;const auto& aspects=binding.source_aspects();
    BinaryExecutionWorkspace scratch;
    const auto result=after.materialize(901,aspects.size(),planes.slots(),[&](auto a,auto w){return planes.plane(aspects.at(a)).word(w);},scratch);
    check(result.outputs.size()==3 && result.outputs[0].count()==1 && result.outputs[0].get(2),"present zero gate lost");
    check(result.outputs[1].count()==4 && !result.outputs[1].get(3),"inactive occurrence counted");
    check(result.outputs[2].words()==result.outputs[0].words(),"duplicate selected output lost");
    check(result.receipt.words==1 && result.receipt.instructions==6 && result.receipt.aspect_reads==3,"single-pass receipt");
    const RelationalCodeAlgebra reference(*reopened.codes);
    for(std::size_t slot=0;slot<planes.slots();++slot){
        RelationalCodeValue::Product inputs;for(auto aspect:aspects)inputs.push_back(RelationalCodeValue{std::uint64_t(planes.get(aspect,slot))});
        const auto old=reference.apply(166,RelationalCodeValue{inputs});const auto fresh=after.apply(RelationalCodeValue{inputs},scratch);
        check(relational_code_detail::value_equal(old,fresh.value),"reopened reference differs");
        for(std::size_t i=0;i<3;++i)check(result.outputs[i].get(slot)==bool(std::get<std::uint64_t>(std::get<2>(fresh.value.data)[i].data)),"packed/value discrepancy");
    }
    // Export actual selected gate values, not a second hard-coded expected record.
    auto output=reopened;output.sequence=1;RelationalCodeValue::Product actual;
    for(const auto& plane:result.outputs)actual.push_back(RelationalCodeValue{std::uint64_t(plane.get(2))});
    output.codes->sections.front().value=RelationalCodeValue{actual};
    TemporalDeclaredValue v;v.key=950;v.type=id(904);v.occurrence=StableOccurrenceKey{g1,core::OccurrenceKey{1003}};
    std::vector<DeclaredValue> typed;for(const auto& plane:result.outputs)typed.push_back(DeclaredValue::boolean(plane.get(2)));
    v.value=DeclaredValue::product(std::move(typed));output.declarations->values.push_back(v);
    // The same source still supplies native exact fields independently of its role bits.
    const DeclaredLocalFieldBinding fields(output,f.contract);
    const auto down=field::SparseStalkFieldActions(fields.fibers()).boundary(fields.bind_downward(g1,f.down_inputs));
    const auto up=local::SparseSheafActions(fields.sheaf()).coboundary(fields.bind_upward(g0,f.up_inputs));
    check(down.field.values().size()==2,"downward field count");
    check(up.field.values().size()==1,"upward field count");
    const auto exported=fields.export_downward(down.field,300,1300);
    output.tensors.insert(output.tensors.end(),exported.tensors.begin(),exported.tensors.end());
    output.declarations->values.insert(output.declarations->values.end(),exported.values.begin(),exported.values.end());
    const auto wire=encode_temporal_endpoint(output);const auto recovered=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),wire);
    check(encode_temporal_endpoint(recovered)==wire,"output roundtrip");
    check(relational_code_operation_bytes(*output.codes,166)==relational_code_operation_bytes(*reopened.codes,166),"producer wordcode replaced");
    const auto segment=make_temporal_segment(reopened,output);const auto replay=replay_temporal_segment(reopened,decode_temporal_segment(encode_temporal_segment(segment)));
    check(encode_temporal_endpoint(replay)==wire,"result replay");
    const PreparedRelationalBinaryOperation replayed(*replay.codes,166);
    check(replayed.operation().registers==std::vector<std::uint16_t>{2,4,2},"register order/duplicates changed");
    auto wrong=reopened;auto definitions=wrong.declarations->registry.definitions();
    for(auto& type:definitions)if(type.id==id(904))std::swap(type.factors[0],type.factors[1]);
    wrong.declarations->registry=DeclarationRegistry(std::move(definitions));rejects([&]{validate_temporal_endpoint(wrong);});
    wrong=reopened;wrong.codes->types[1].factors[1].source_aspect="unknown";
    // Generic code-to-declaration checks factor IDs/order; source view binding
    // separately checks aspect names. Never claim this plan discovers ownership.
    rejects([&]{core::SourceAspectLayoutBinding bad(reopened.source.signature(),CompiledLayout(d.registry,id(901),d.profile),{{1,"exists"},{2,"unknown"}});});
    std::cout<<"lifecycle profile="<<unsigned(profile)<<" endpoint_bytes="<<wire.size()<<" words="<<result.receipt.words<<" reads="<<result.receipt.aspect_reads<<'\n';
}
}
int main(){try{run(BooleanLayoutProfile::Lsb0OctetsV1);run(BooleanLayoutProfile::Lsb0OctetsV2);std::cout<<"prepared_binary_lifecycle checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
