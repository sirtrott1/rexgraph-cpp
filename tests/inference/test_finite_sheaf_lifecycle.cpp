#include "../../examples/finite_sections_data.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
using namespace finite_sections_example;
std::size_t checks{};void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes,"expected refusal");}
TemporalEndpoint reopen(TemporalEndpoint e,BooleanLayoutProfile profile){
    if(e.declarations)e.declarations->profile=profile;
    const auto source=encode_rfcx(e.source);const auto wire=encode_temporal_endpoint(e);
    auto x=decode_temporal_endpoint(MappedRfcx(source.bytes).materialize(),wire);check(encode_temporal_endpoint(x)==wire && encode_rfcx(x.source).bytes==source.bytes,"canonical source and endpoint bytes");return x;
}
void reopen_data(Data& d,BooleanLayoutProfile profile){for(auto& p:d.maps.locals)p.endpoint=reopen(p.endpoint,profile);for(auto& p:d.maps.commons)p.endpoint=reopen(p.endpoint,profile);d.maps.certificates=reopen(d.maps.certificates,profile);d.selectors=reopen(d.selectors,profile);}
int main(){try{
    for(const auto profile:{BooleanLayoutProfile::Lsb0OctetsV1,BooleanLayoutProfile::Lsb0OctetsV2}){
        auto d=data();const auto records=store_conditions(d,policies());const auto old=recorded_family(d,records);const auto oldcore=old.conflict(availability());reopen_data(d,profile);const auto p=recorded_family(d,records);const auto c=p.conflict(availability());
        check(c.subset_minimal && c.clauses==oldcore.clauses && c.deletion_witnesses==oldcore.deletion_witnesses,"stored finite conditions recompile and retain conflict");
        check(!p.gluing().view().patches()[0].source_owner().same_owner(old.gluing().view().patches()[0].source_owner()),"fresh local owners");
        auto equality_records=records;equality_records.pop_back();const auto eq=recorded_family(d,equality_records);const auto q=eq.query(availability(),{{0,1}});const auto field=eq.materialize(q,availability());
        const auto action=eq.gluing().coboundary(g0);const auto output=action.execute(field,availability());check(output.sparse_values().empty(),"actual selected global section has zero coboundary");
        check(field.publications().size()>=9,"selection and complete catalogue dependencies retained");
        std::vector<NativeMomentChannel> channels;for(std::size_t i=0;i<3;++i)channels.push_back({"local-"+std::to_string(i),eq.gluing().coboundary(g0,i)});
        const auto moment=PreparedNativeActionMoments(channels,NativeBilinearForm::named_unit(action.output()),action).execute(field,availability());check(moment.total==0 && moment.matches_reference.value_or(false),"selected finite section feeds actual moment actions");
        const auto qep=*field.carrier().publication.endpoint();auto saved=qep;
        TemporalTensor t;t.key=10000;t.role=TemporalTensorRole::Field;t.output=field.carrier().key;t.tensor={field.carrier().declaration->domain,numeric::StructuralSupport::full(field.carrier().size()),field.sparse_values()};saved.tensors.push_back(t);
        TemporalTensor z;z.key=10001;z.role=TemporalTensorRole::Field;z.output=output.carrier().key;z.tensor={output.carrier().declaration->domain,numeric::StructuralSupport::full(output.carrier().size()),output.sparse_values()};saved.tensors.push_back(z);
        const auto restored=reopen(saved,profile);check(restored.tensors[0].tensor.values().entries()==field.sparse_values().entries() && restored.tensors[1].tensor.support().count()==output.carrier().size() && restored.tensors[1].tensor.values().empty(),"selected and supported-zero output records survive");
        Guard g;const auto local=eq.gluing().view().restrict_field(g0,field.sparse_values(),0,g);const auto exported=eq.gluing().view().patches()[0].export_upward(local,80000,81000);auto lep=d.maps.locals[0].endpoint;lep.tensors.insert(lep.tensors.end(),exported.tensors.begin(),exported.tensors.end());lep.declarations->values.insert(lep.declarations->values.end(),exported.values.begin(),exported.values.end());const auto again=reopen(lep,profile);DeclaredLocalFieldBinding bind(again,d.maps.locals[0].contract);std::vector<DeclaredFieldInput> fields;for(std::size_t i=0;i<exported.values.size();++i)fields.push_back({exported.tensors[i].key,exported.values[i].key});const auto lf=bind.bind_upward(g0,fields);check(lf.nonzeros()==local.nonzeros(),"selected local section exports/rebinds");
        // Nonsection alternative still feeds actual action/moment, but is not
        // mislabeled as a global section when that check is disabled.
        auto nc=data(false);const auto nr=store_conditions(nc,policies(false));reopen_data(nc,profile);const auto co=recorded_family(nc,nr,false);const auto cq=co.query(availability(),{{0,1}});const auto cx=co.materialize(cq,availability());const auto cop=co.gluing().coboundary(g0);const auto cy=cop.execute(cx,availability());check(!cy.sparse_values().empty(),"compatible finite cochain can fail section");
        channels.clear();for(std::size_t i=0;i<3;++i)channels.push_back({"part-"+std::to_string(i),co.gluing().coboundary(g0,i)});const auto cm=PreparedNativeActionMoments(channels,NativeBilinearForm::named_unit(cop.output()),cop).execute(cx,availability());check(cm.total==29 && cm.matches_reference.value_or(false),"finite cochain output moment exact");
        auto obs=data();const auto rec=observation(obs,0,1,0);const auto prs=store_conditions(obs,policies(false));reopen_data(obs,profile);const auto observed=recorded_family(obs,prs,true,{rec});check(observed.query(availability(),{{0,1}}).status==Status::Unsatisfiable && observed.query(availability(),{{0,0}}).status==Status::Satisfiable,"zero observation remains a condition after decoding");
        check(observed.family().coverage()[0].observed==1 && observed.family().coverage()[0].dimension==2,"unobserved coordinate remains unobserved");
        auto partial=obs;partial.catalogues[0].alternatives[0].fields[0]=rec.record;rejects([&]{recorded_family(partial,prs);});
        auto bad=d;auto& rows=std::get<DeclaredValue::Repeated>(bad.selectors.declarations->values[0].value.data);rows.elements.clear();bad.selectors=reopen(bad.selectors,profile);const auto impossible=recorded_family(bad,records);check(impossible.query(availability()).status==Status::Unsatisfiable && impossible.conflict(availability()).clauses.size()==1,"empty decoded relation is genuine no-solution family");
        auto wrong=records;std::reverse(wrong[0].patches.begin(),wrong[0].patches.end());rejects([&]{recorded_family(d,wrong);});wrong=records;wrong[0].expected_table.max_count=3;rejects([&]{recorded_family(d,wrong);});
        auto limits=local::FiniteSearchLimits{};limits.max_steps=0;const auto incomplete=p.conflict(availability(),limits);check(incomplete.status==Status::BudgetExceeded && !incomplete.subset_minimal,"decode does not evade search budgets");
        auto late=availability();late.back().max_sequence=6;rejects([&]{p.query(late);});rejects([&]{p.conflict(late);});rejects([&]{action.execute(field,late);});
        rejects([&]{eq.materialize(old.query(availability()),availability());});
    }
    std::cout<<"finite_sheaf_lifecycle checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
