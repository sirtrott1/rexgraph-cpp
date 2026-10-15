#include "rfc/learning/relational_available_tensor_square_evidence_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using T=numeric::SparseRationalTensor;
using Bytes=std::vector<std::byte>;
using Model=PreparedAvailableRelationalTensorSquareEvidence;
namespace {
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F action,const char* reason){bool hit{};try{action();}catch(const std::exception&){hit=true;}check(hit,reason);}
template<class F>void before_math(F action,const char* reason){bool hit{};try{action();}catch(const std::invalid_argument&){hit=true;}check(hit,reason);}
T tensor(const V& values){numeric::ExactActionGuard guard;return field_interface_detail::tensor(values,guard);}
storage::NativeTemporalSeries history(std::uint64_t ns) {
    core::RelationSignature signature;const auto type=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,1,8);
    core::RelationalComplexBuilder b(signature);b.add_grade_one(core::RelationKey{100},type,
        {{10,true,false,1},{20,true,true,1},{10,true,true,1},{20,false,false,1},{30,true,false,0}});
    storage::TemporalEndpoint endpoint(b.freeze(),1);storage::NativeTemporalSeries series(endpoint,ns);
    for(std::uint64_t i=2;i<=5;++i){endpoint.sequence=i;series.append_target(endpoint);}return series;
}
M metric(bool lifted){const M m=M::from_rows({{2,1},{1,3}});if(!lifted)return m;M out(6,6);
    for(const auto& [ij,q]:m.entries()){out.set(ij.first,ij.second,q);for(const auto& [kl,r]:m.entries())out.set(2+2*ij.first+kl.first,2+2*ij.second+kl.second,q*r);}return out;}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series,bool lifted) {
    RelationalFieldProgramDeclaration d;d.key=lifted?42:41;d.revision=1;d.differential=true;
    d.stalks={{{{0},0},101,{11,12},metric(false)},{{{0},1},102,{21,22,23},M::identity(3)},{{{0},2},103,{31},M::identity(1)},
        {{{1},0},lifted?400u:303u,lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},metric(lifted)}};
    const std::array<std::size_t,5> rows{2,3,2,3,1};
    for(std::size_t i=0;i<5;++i)d.occurrences.push_back({{1},i,storage::TemporalTensorRole::CoefficientIndependentMap,M(rows[i],lifted?6:2)});
    M map=M::identity(6);map.set(5,2,1);map.set(5,3,-1);
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}},{3,FieldProgramNodeKind::DeclaredMap,{0},2,map}};
    return {series.at(1),std::move(d)};
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series,std::uint64_t key) {
    RelationalInputUseDeclaration d;d.key=key;d.channel=700;d.encoding="UTF-8";
    for(const auto c:std::string("bank / bank"))d.bytes.push_back(static_cast<std::byte>(c));
    const auto endpoint=series.at(1);RelationalSpanUse use{111,{{{1},100}},{},{{0,4},{7,11},{0,0},{0,4}}};
    for(std::size_t i=0;i<5;++i)use.occurrences.push_back({{1},endpoint.endpoint().source.occurrence_identity({1},i)});
    d.uses={use};return {storage::NativeSeriesPrefix::capture(series,1).history(),std::move(d)};
}
RelationalInputAccessionDeclaration declaration(bool lifted) {
    RelationalInputAccessionDeclaration d;d.key=lifted?902:901;d.node=1;d.source_grade={1};d.isometric=true;
    d.source_stalks={{{{1},0},lifted?400u:303u,lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},metric(lifted)}};
    d.instances.cells={{0,1,2},{0}};d.instances.occurrences={{},{0,1,2,3,4}};d.accession=M::identity(lifted?6:2);d.reconstruction=d.accession;return d;
}
RelationalKnownTensorSquareDeclaration square_declaration(){return {701,1,400,{{1},0},{501,502,503,504}};}
RelationalFieldParameterDeclaration parameters(){RelationalFieldParameterDeclaration d;d.key=50;d.revision=1;d.node=2;
    for(std::size_t slot=0;slot<3;++slot)for(std::size_t row=0;row<(slot==1?3u:2u);++row)for(std::size_t col=0;col<6;++col)d.variables.push_back({slot,row,col});
    for(std::uint64_t i=0;i<30;++i)d.parameters.push_back(10000+i);
    d.basis=M(42,30);for(std::size_t i=0;i<42;++i)d.basis.set(i,i<30?i:i-30,1);return d;
}
struct Case {
    RelationalAccessionInput original;
    RelationalKnownTensorSquareInput compound;
    OwnedRelationalInputUseAsset target_asset;
    T target;
    RelationalTensorSquareOrigins keys;
    RelationalTargetOrigins targets;
    std::uint64_t interface{},evidence{};
    AvailableRelationalInitialInputLineage lineage;
};
struct Fixture {
    storage::NativeTemporalSeries source=history(991),publisher=history(992);
    OwnedRelationalFieldProgram parent=program(source,false),target=program(source,true);
    std::vector<RelationalOriginGrant> grants;
    std::vector<Case> cases;
    void add(std::uint64_t key,RelationalDependencyRole role,Bytes payload,std::uint64_t cutoff){
        grants.push_back({OwnedRelationalValueOrigin(key,storage::NativeSeriesPrefix::capture(publisher,cutoff),role,std::move(payload)),{992,5}});
    }
    Fixture(){using R=RelationalDependencyRole;
        const auto p=encode_relational_field_program(parent),t=encode_relational_field_program(target);
        add(1,R::Program,p,1);add(2,R::Maps,p,1);add(3,R::Metrics,p,1);
        add(8,R::Interface,relational_tensor_square_dependency_payload(parent,square_declaration()),1);
        add(9,R::Program,t,1);add(10,R::Maps,t,1);add(11,R::Metrics,t,1);
        add(17,R::Evidence,relational_evidence_root_payload(t,parameters(),{}),1);
        const auto root_origin=grants.back().origin;
        for(std::size_t i=0;i<3;++i){const std::array<V,3> x{V{1,0},V{0,1},V{1,1}};
            const auto a=asset(source,600+i),b=asset(source,900+i);
            const auto original=OwnedRelationalInputAccession(a,parent,declaration(false)).admit(111,tensor(x[i]));
            const auto compound=OwnedRelationalKnownTensorSquare(original,square_declaration()).admit(OwnedRelationalInputAccession(a,target,declaration(true)));
            const auto value=compound.input();const auto base=1000+100*i;
            RelationalTensorSquareOrigins keys{{1,2,3},{base,base+1,base+2,base+3},8,{9,10,11},{base+4,base+5,base+6,base+7}};
            const auto bytes=encode_relational_input_use_asset(a);add(base,R::InputAsset,bytes,1);
            add(base+1,R::InputValue,relational_input_value_payload(original),2);
            add(base+2,R::InputAccession,relational_input_accession_payload(original),2);add(base+3,R::Reconstruction,relational_input_accession_payload(original),2);
            add(base+4,R::InputAsset,bytes,1);add(base+5,R::InputValue,relational_input_value_payload(value),3);
            add(base+6,R::InputAccession,relational_input_accession_payload(value),3);add(base+7,R::Reconstruction,relational_input_accession_payload(value),3);
            const T y=tensor({0,0,-x[i][0],-2*x[i][1],-x[i][0]*x[i][1],0});
            add(base+8,R::TargetAsset,encode_relational_input_use_asset(b),1);
            add(base+9,R::TargetValue,relational_target_value_payload(t,2,encode_relational_input_use_asset(b),111,y),4);
            const auto prior_source=history(1400+i);const auto prior=asset(prior_source,400+i);
            const OwnedRelationalInitialInputLineage lineage_owner(prior,a,{500+i,1,{{111,111}}});
            add(base+12,R::InputAsset,encode_relational_input_use_asset(prior),1);
            add(base+13,R::Interface,relational_initial_input_lineage_payload(lineage_owner),1);
            const RelationalAvailabilityContext current(grants);const auto checked=admit_available_relational_tensor_square_input(compound,keys,current);
            const AvailableRelationalInitialInputLineage lineage(lineage_owner,{base+12,base,base+13},current);
            const auto ta=grants[grants.size()-4].origin,tv=grants[grants.size()-3].origin;
            auto dependencies=checked.obligations();dependencies.insert(dependencies.end(),lineage.obligations().begin(),lineage.obligations().end());
            const auto descriptor=relational_tensor_square_observation_payload(i+1,root_origin,dependencies,ta,tv,111,{{{0},0},{{0},1},{{0},2}});
            add(base+10,R::Interface,descriptor,4);add(base+11,R::Evidence,descriptor,5);
            cases.push_back({original,compound,b,y,keys,{base+8,base+9},base+10,base+11,lineage});
        }
    }
    AvailableRelationalTensorSquareResult supplied(std::size_t i,const RelationalAvailabilityContext& c)const{
        return admit_available_relational_tensor_square_input(cases[i].compound,cases[i].keys,c).execute(c);
    }
    RelationalAvailableTensorSquareObservation observation(std::size_t i,const RelationalAvailabilityContext& c)const{
        return {i+1,supplied(i,c),cases[i].target_asset,111,cases[i].target,{{{0},0},{{0},1},{{0},2}},cases[i].targets,cases[i].interface,cases[i].evidence,cases[i].lineage};
    }
};
void run(){const Fixture f;const RelationalAvailabilityContext context(f.grants);
    const PreparedAvailableRelationalProgram program(f.target,{9,10,11},context);
    const Model root(program,parameters(),17,context);
    const auto one=f.observation(0,context),two=f.observation(1,context);
    const auto query=f.supplied(2,context);
    const auto child=root.append({one},context),learned=child.append({two},context);
    const auto family=learned.execute(query,context,{}, {},f.cases[2].lineage);const auto fields=family.result();
    const auto initial_evidence=root.evidence(),child_evidence=child.evidence(),learned_evidence=learned.evidence();
    check(initial_evidence.kernel().cols()==30&&child_evidence.kernel().cols()==27&&learned_evidence.kernel().cols()==24,"whole correlated maps remain owned through two training updates");
    check(learned.obligations().size()==48,"both original-source lineages fifteen-origin products and all target/observer authorities retained");
    check(fields.field(1).particular.support().count()==6&&fields.field(2).particular.support().count()==6&&fields.occurrence_field(2).particular.support().count()==11,"whole lifted input heterogeneous stalks and all original flags retained");
    check(fields.identified(2,0)&&fields.field(2).particular.state(0)==Q{0}&&!fields.identified(2,2),"exact cancellation and off-evidence unknowns remain distinct");
    for(std::size_t col=0;col<24;++col)check(fields.field(3).directions.get(5,col)==fields.field(2).directions.get(5,col)+fields.field(2).directions.get(2,col)-fields.field(2).directions.get(3,col),"complete cross-stalk common-free correlation through fixed next map");
    check(fields.occurrence_field(2).directions.nonzeros()>0,"cancelled boundary retains actual flag alternatives");
    const auto packet=encode_available_relational_tensor_square_evidence(learned),response=encode_available_relational_tensor_square_affine_result(family);
    Bytes streamed_model,streamed_family;std::size_t largest_chunk{};
    stream_available_relational_tensor_square_evidence(learned,[&](auto chunk){largest_chunk=std::max(largest_chunk,chunk.size());streamed_model.insert(streamed_model.end(),chunk.begin(),chunk.end());});
    stream_available_relational_tensor_square_affine_result(family,[&](auto chunk){largest_chunk=std::max(largest_chunk,chunk.size());streamed_family.insert(streamed_family.end(),chunk.begin(),chunk.end());});
    check(streamed_model==packet&&streamed_family==response&&largest_chunk<=65536,"complete model/history/field/receipt stream equals canonical raw bytes");
    check(encode_shared_available_relational_tensor_square_evidence(learned)==storage::encode_shared_packet(packet)&&encode_shared_available_relational_tensor_square_affine_result(family)==storage::encode_shared_packet(response),"integrated shared serializers preserve full original packet bytes");
    inference::field_interface_codec_detail::PacketContext cache;
    const auto encoded_program=cache.program(f.target),same_program=cache.program(f.target);
    const auto foreign_program=::program(f.source,true);const auto encoded_foreign=cache.program(foreign_program);
    check(encoded_program.bytes()==same_program.bytes()&&encoded_foreign.bytes()==encoded_program.bytes()&&!foreign_program.same_owner(f.target)&&cache.programs.size()==2,"equal program bytes keep distinct actual serialization owners");
    auto rebind=learned;Bytes held_stream;bool rebound{};
    stream_available_relational_tensor_square_evidence(rebind,[&](auto chunk){held_stream.insert(held_stream.end(),chunk.begin(),chunk.end());},[&]{if(!rebound){rebound=true;rebind=root;}});
    check(rebound&&held_stream==packet,"native model is pinned before preparation callbacks");
    std::size_t emitted{};
    rejects([&]{stream_available_relational_tensor_square_affine_result(family,[&](auto){++emitted;},[]{throw std::runtime_error("cancel");});},"native stream cancellation propagates");
    check(!emitted,"cancelled preparation emits no model prefix");
    auto stream_limits=storage::SharedPacketLimits{};stream_limits.max_expanded_bytes=packet.size()-1;
    rejects([&]{encode_shared_available_relational_tensor_square_evidence(learned,stream_limits);},"integrated serializer admits complete logical model before sharing");
    const auto cold=decode_available_relational_tensor_square_evidence(packet,context,{});
    const auto restored=decode_available_relational_tensor_square_affine_result(response,context,{});
    check(encode_available_relational_tensor_square_evidence(cold)==packet&&encode_available_relational_tensor_square_affine_result(restored)==response,"canonical complete compound training/held/cold lineage");
    const auto cold_root=cold.parent()->parent()->evidence();
    check(cold_root.kernel().cols()==30,"cold root and every actual source/parameter ancestor retained");
    const auto third=f.observation(2,context);const auto after=learned.append({third},context);const auto held=after.execute(query,context,{}, {},f.cases[2].lineage);const auto result=held.result();
    check(result.identified(2,2)&&result.field(2).particular.values().dense()==V({0,0,-1,-2,-1,0}),"new input-conditioned evidence changes full independently held response");
    check(result.field(3).particular.state(5)==Q{1},"complete later field agrees before terminal decoding");
    numeric::ExactActionLimits no_math;no_math.dimension=0;no_math.steps=0;
    for(std::size_t dependency=0;dependency<f.grants.size();++dependency){
        auto hidden=f.grants;hidden[dependency].availability.max_sequence=hidden[dependency].origin.publication().cutoff()-1;
        const RelationalAvailabilityContext early(hidden);
        const auto key=hidden[dependency].origin.key();const bool held_target=key>=1208&&key<=1211;
        if(!held_target){
            rejects([&]{family.admit(early);},"model and held result recheck all original derived target interface and evidence authorities");
            before_math([&]{decode_available_relational_tensor_square_affine_result(response,early,{},no_math);},"all original and learned dependencies preflight before cold frame/product/action");
            before_math([&]{learned.execute(query,early,no_math,{},f.cases[2].lineage);},"warm held execution refuses hidden original or learned dependency before action");
        }
        const bool irrelevant=key>=1200;
        if(!irrelevant){
            before_math([&]{tensor_square_evidence_codec_detail::preflight_evidence(packet,early,{}, {},{});},"passive preflight rejects every training cutoff without any mathematical reconstruction");
            before_math([&]{decode_available_relational_tensor_square_evidence(packet,early,{},no_math);},"cold model closes all training dependency cutoffs before root fit");
        }
        if(key>=1000&&key<1100)before_math([&]{root.append({one,two},early);},"whole suffix is checked before fitting");
    }
    auto earlier=f.grants;earlier[8].availability.max_sequence=0;const RelationalAvailabilityContext hidden(earlier);
    before_math([&]{root.append({one,two},hidden);},"input asset unavailable despite later derived and target authorities");
    auto changed=one;changed.observed={{{0},1}};
    before_math([&]{root.append({changed},context);},"observer interface binds whole source mask");
    changed=one;changed.evidence_origin=one.interface_origin;
    before_math([&]{root.append({changed},context);},"interface cannot be renamed to sample evidence authority");
    auto copied=f.grants;const auto original=copied[8].origin;copied[8].origin=OwnedRelationalValueOrigin(original.key(),original.publication(),original.role(),original.payload());
    rejects([&]{family.admit(RelationalAvailabilityContext(copied));},"equal serialized origin does not replace warm original owner");
    rejects([&]{learned.append({third},context,[]{throw std::runtime_error("cancel");});},"cancelled product-aware append publishes no child");
    auto buffer=response;auto mutable_context=context;bool replaced{};
    const auto pinned=decode_available_relational_tensor_square_affine_result(buffer,mutable_context,{}, {},[&]{if(!replaced){replaced=true;buffer.clear();mutable_context=hidden;}});
    check(replaced&&encode_available_relational_tensor_square_affine_result(pinned)==response,"cold source bytes/current authority pinned before callbacks");
    const auto packed=encode_shared_available_relational_tensor_square_affine_result(family);
    const auto model_packed=encode_shared_available_relational_tensor_square_evidence(learned);
    auto model_image=storage::view_shared_packet(model_packed);
    check(model_image.materialized_bytes()==0,"shared model storage validates with no expanded allocation");
    const auto indexed_model=tensor_square_evidence_codec_detail::decode_image_evidence(model_image,context,{});
    check(encode_available_relational_tensor_square_evidence(indexed_model)==packet,"indexed model reader preserves complete raw native bytes");
    check(model_image.materialized_bytes()<model_image.size(),"large model witnesses remain logical during native reconstruction");
    numeric::ExactActionLimits no_indexed_math;no_indexed_math.steps=0;
    before_math([&]{(void)tensor_square_evidence_codec_detail::decode_image_evidence(model_image,hidden,{},no_indexed_math);},"indexed reader closes authority before root fit");
    const auto unpacked=decode_shared_available_relational_tensor_square_affine_result(packed,context,{});
    check(encode_available_relational_tensor_square_affine_result(unpacked)==response,"shared passive storage preserves every source model and field byte");
    const auto restored_lineage=unpacked.lineage();const auto restored_input=unpacked.input().input();const auto restored_native=restored_input.native_input();const auto restored_square=restored_native.square();
    check(restored_lineage&&restored_lineage->lineage().initial().same_owner(restored_square.parent().accession().asset()),"cold older and initial source lineage adopts the actual product asset once");
    auto mutable_shared=packed;auto shared_context=context;storage::TemporalReadLimits shared_policy;bool mutated_shared{};
    const auto shared_pin=decode_shared_available_relational_tensor_square_affine_result(mutable_shared,shared_context,shared_policy,{}, {},[&]{if(!mutated_shared){mutated_shared=true;mutable_shared.clear();shared_context=hidden;shared_policy.max_file_bytes=0;}});
    check(mutated_shared&&encode_available_relational_tensor_square_affine_result(shared_pin)==response,"indexed shared bytes/current context/read policy pinned before callbacks");
    auto family_image=storage::view_shared_packet(packed);const auto indexed_family=tensor_square_evidence_codec_detail::decode_image_family(family_image,context,{});
    check(encode_available_relational_tensor_square_affine_result(indexed_family)==response&&family_image.materialized_bytes()<family_image.size(),"whole affine family and witnesses reconstructed without complete expansion");
    std::vector<std::future<bool>> readers;for(unsigned i=0;i<4;++i)readers.push_back(std::async(std::launch::async,[response,context]{return encode_available_relational_tensor_square_affine_result(decode_available_relational_tensor_square_affine_result(response,context,{}))==response;}));
    for(auto& reader:readers)check(reader.get(),"concurrent complete source/model/held lineage replay");
    const auto escaped=[&]{return decode_available_relational_tensor_square_affine_result(response,context,{});}();
    const auto escaped_fields=escaped.result();
    check(escaped_fields.field(2).directions==fields.field(2).directions,"complete intermediary family survives producer lifetime");
}
} // namespace
int main(){try{run();std::cout<<checks<<" available product-aware evidence checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"product-aware evidence failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
