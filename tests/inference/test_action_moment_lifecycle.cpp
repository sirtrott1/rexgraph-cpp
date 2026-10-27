#include "../../examples/action_moment_data.hpp"
#include "../../examples/declared_local_field_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include "rfc/storage/relational_codes.hpp"
#include <iostream>
using namespace action_moment_example;
std::size_t checks{};
void check(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
int main(){try{
    for(const auto profile:{composite::BooleanLayoutProfile::Lsb0OctetsV1,composite::BooleanLayoutProfile::Lsb0OctetsV2}){
        auto endpoint=make();storage::TemporalDeclarationState dec;dec.namespace_key=5101;dec.profile=profile;
        const composite::DeclarationId type{5101,1,1}, qtype{5101,2,1};
        dec.registry=composite::DeclarationRegistry({composite::Declaration::rational_tensor(type,{{1,4,{10,11,12,13}}}),composite::Declaration::rational(qtype)});
        storage::TemporalDeclaredValue dv;dv.key=1000;dv.type=type;dv.value=composite::DeclaredValue::tensor(endpoint.tensors.back().tensor);dec.values.push_back(dv);
        endpoint.declarations=dec;
        const auto source=storage::encode_rfcx(endpoint.source); const auto wire=storage::encode_temporal_endpoint(endpoint);
        const auto recovered=storage::decode_temporal_endpoint(storage::MappedRfcx(source.bytes).materialize(),wire);
        check(storage::encode_temporal_endpoint(recovered)==wire,"canonical input endpoint");
        storage::NativeAccessionDirectory original(endpoint,900), reopened(recovered,900);
        const auto original_result=plans(original).new_third.execute(input(original),{{900,7}});
        const auto result=plans(reopened).new_third.execute(input(reopened),{{900,7}});
        check(!original_result.aggregate.carrier().same(result.aggregate.carrier()),"fresh source ownership");
        check(result.total==original_result.total && result.total==5,"reopened action-moment result");
        check(recovered.declarations->profile==profile,"profile retained");
        auto output=recovered;output.sequence=8;
        std::uint64_t key=3000;
        for(const auto& part:result.components){
            auto t=action_moment_example::field(key++,part.field.carrier().key,part.field.carrier().size(),part.field.sparse_values().entries());
            output.tensors.push_back(std::move(t));
        }
        output.tensors.push_back(action_moment_example::field(3010,result.aggregate.carrier().key,result.aggregate.carrier().size(),result.aggregate.sparse_values().entries()));
        // Exact numerical receipts use existing declared rational values. The action
        // plan and labels are caller admission data, not serialized proof authority.
        storage::TemporalDeclaredValue q;q.key=3000;q.type=qtype;q.value=composite::DeclaredValue::rational(result.total);output.declarations->values.push_back(q);
        const auto bytes=storage::encode_temporal_endpoint(output);
        const auto out=storage::decode_temporal_endpoint(storage::MappedRfcx(source.bytes).materialize(),bytes);
        check(storage::encode_temporal_endpoint(out)==bytes,"canonical actual output");
        check(out.tensors[out.tensors.size()-3].tensor.values().nonzeros()==0,"explicit zero component preserved");
        check(out.tensors.back().tensor.support().count()==3,"full result support preserved");
        const auto transition=storage::make_temporal_segment(recovered,output);
        const auto replay=storage::replay_temporal_segment(recovered,storage::decode_temporal_segment(storage::encode_temporal_segment(transition)));
        check(storage::encode_temporal_endpoint(replay)==bytes,"actual output segment replay");
        storage::NativeAccessionDirectory rd(replay,900);
        const auto rechecked=plans(rd).new_third.execute(input(rd),{{900,8}});
        check(rechecked.total==5 && rechecked.matches_reference==true,"replay recomputes result not authority");
        bool rejected=false;try{plans(rd).new_third.execute(input(rd),{{900,7}});}catch(const std::exception&){rejected=true;}
        check(rejected,"future result unavailable at old cutoff");
        // Existing heterogeneous typed-stalk binding supplies the actual input to
        // a same-stalk action decomposition after its real encoded round trip.
        auto f=declared_field_example::make();f.endpoint.declarations->profile=profile;
        const auto initial=storage::encode_temporal_endpoint(f.endpoint);
        const auto sb=storage::encode_rfcx(f.endpoint.source);
        auto fe=storage::decode_temporal_endpoint(storage::MappedRfcx(sb.bytes).materialize(),initial);
        const storage::DeclaredLocalFieldBinding bound(fe,f.contract);
        const auto applied=field::SparseStalkFieldActions(bound.fibers()).boundary(bound.bind_downward(core::Grade{1},f.down_inputs));
        const auto exported=bound.export_downward(applied.field,300,1300);
        fe.tensors.insert(fe.tensors.end(),exported.tensors.begin(),exported.tensors.end());
        fe.declarations->values.insert(fe.declarations->values.end(),exported.values.begin(),exported.values.end());
        const auto fw=storage::encode_temporal_endpoint(fe);
        const auto fresh=storage::decode_temporal_endpoint(storage::MappedRfcx(sb.bytes).materialize(),fw);
        storage::NativeAccessionDirectory fd(fresh,910);
        for(std::size_t i=0;i<exported.tensors.size();++i){
            const auto v=NativeFieldValue::stored(fd.pin(access(storage::NativeAccessionKind::Field,300+i)));
            const auto identity=NativeFactoredAction::identity(v.carrier());
            const auto result2=PreparedNativeActionMoments({{"direct",identity},{"cancel",identity.scaled(-1)},{"return",identity}},unit(v.carrier()),identity).execute(v,{{910,0}});
            check(result2.matches_reference==true,"typed stalk decomposition preserves actual field");
            check(result2.aggregate.carrier().declaration->kind==storage::TemporalCarrierKind::CellStalk,"stalk carrier identity retained");
            check(result2.cross_sum==-2*result2.total,"cancellation interactions retained");
        }
        std::cout<<"action_moment_lifecycle profile="<<unsigned(profile)<<" output_bytes="<<bytes.size()<<" typed_stalks="<<exported.tensors.size()<<"\n";
    }
    std::cout<<"action_moment_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
