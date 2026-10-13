#include "rfc/learning/relational_shared_finite_operator_codec.hpp"
#pragma push_macro("main")
#undef main
#define main rfc_shared_operator_fixture_main
#include "test_relational_available_finite_operator.cpp"
#undef main
#pragma pop_macro("main")
namespace {
using Packing=storage::SharedPacketLimits;
Bytes image(std::size_t count,bool repeated=false){Bytes out;std::uint64_t state=0x123456789abcdef0ULL;for(std::size_t i=0;i<count;++i){state=state*6364136223846793005ULL+1;out.push_back(repeated?std::byte{0}:static_cast<std::byte>(state>>56u));}return out;}
Bytes packet(std::uint64_t expanded,const std::vector<Bytes>& chunks,const std::vector<std::uint64_t>& refs){
    storage::rfcx_detail::Writer w;w.raw(storage::shared_packet_detail::magic);w.u32(1);w.u32(0);w.u64(0);w.u64(0);w.u64(expanded);w.u64(chunks.size());
    for(const auto& chunk:chunks){w.u64(chunk.size());w.raw(chunk);}w.u64(refs.size());for(const auto id:refs){w.u64(id);}
    w.patch_u64(16,w.size());w.patch_u64(24,storage::rfcx_detail::fnv1a64(std::span<const std::byte>(w.bytes()).subspan(32)));return std::move(w).take_bytes();
}
void primitive_checks(){
    for(const auto count:{0u,1u,255u,256u,257u,4095u,4096u,4097u,65537u}){const auto source=image(count);const auto shared=storage::encode_shared_packet(source);const auto stats=storage::inspect_shared_packet(shared);
        check(storage::decode_shared_packet(shared)==source&&stats.expanded_bytes==count&&stats.stored_bytes==shared.size(),"exact byte inversion at every profile boundary");
        check(storage::encode_shared_packet(storage::decode_shared_packet(shared))==shared,"deterministic canonical bytes");
    }
    const auto source=image(65536,true);const auto shared=storage::encode_shared_packet(source);const auto stats=storage::inspect_shared_packet(shared);
    check(stats.unique_chunks==1&&stats.chunk_references==16&&shared.size()<source.size()/10,"repeated literal sharing keeps complete ordered positions");
    const std::vector<Bytes> bad{
        packet(1,{{std::byte{'a'}}},{1}),packet(0,{{}},{}),packet(1,{{std::byte{'a'}}},{0,0}),packet(2,{{std::byte{'a'}},{std::byte{'a'}}},{0,1}),
        packet(1,{{std::byte{'a'}},{std::byte{'b'}}},{0}),packet(2,{{std::byte{'b'}},{std::byte{'a'}}},{1,0}),packet(2,{{std::byte{'a'}},{std::byte{'b'}}},{0,1}),
        packet(std::numeric_limits<std::uint64_t>::max(),{{std::byte{'a'}}},{0})};
    for(const auto& bytes:bad){rejects([&]{(void)storage::decode_shared_packet(bytes);},"malformed reference/zero/length/duplicate/dead/reordered/partition/expansion rejects");}
    auto damaged=shared;damaged.back()^=std::byte{1};rejects([&]{(void)storage::decode_shared_packet(damaged);},"integrity corruption rejects");
    damaged=shared;damaged.push_back(std::byte{0});rejects([&]{(void)storage::decode_shared_packet(damaged);},"trailing packet rejects");
    auto limits=Packing{};limits.max_expanded_bytes=source.size()-1;rejects([&]{(void)storage::decode_shared_packet(shared,limits);},"expanded bound precedes output allocation");
    limits=Packing{};limits.max_stored_bytes=shared.size()-1;rejects([&]{(void)storage::decode_shared_packet(shared,limits);},"stored bound precedes packet freeze");
    limits=Packing{};limits.max_records=16;rejects([&]{(void)storage::decode_shared_packet(shared,limits);},"dictionary plus roots aggregate record bound");
    limits=Packing{};limits.max_work_bytes=shared.size();rejects([&]{(void)storage::decode_shared_packet(shared,limits);},"counted byte work bound");
    limits=Packing{};limits.max_stored_bytes=56;rejects([&]{(void)storage::encode_shared_packet(source,limits);},"encoded stored admission before emission");
    limits=Packing{};limits.max_records=1;rejects([&]{(void)storage::encode_shared_packet(source,limits);},"encoder cumulative record refusal");
    rejects([&]{(void)storage::encode_shared_packet(source,{},[]{throw std::runtime_error("cancel");});},"cancelled encode returns no prefix");
    rejects([&]{(void)storage::decode_shared_packet(shared,{},[]{throw std::runtime_error("cancel");});},"cancelled decode returns no prefix");
    auto buffer=source;auto policy=Packing{};bool changed{};const auto frozen=storage::encode_shared_packet(buffer,policy,[&]{if(!changed){changed=true;buffer.clear();policy.max_expanded_bytes=0;}});
    check(changed&&storage::decode_shared_packet(frozen)==source,"encoder freezes source and policy before callbacks");
    buffer=shared;policy=Packing{};changed=false;const auto result=storage::decode_shared_packet(buffer,policy,[&]{if(!changed){changed=true;buffer.clear();policy.max_expanded_bytes=0;}});
    check(changed&&result==source,"decoder freezes packet and policy before callbacks");
}
void run_shared(bool emit,const std::filesystem::path& exports={}){
    primitive_checks();const auto f=operator_fixture();auto grants=f.grants;const auto data=prepare(f,{2,3,5,7},grants);const Context context(grants);const storage::TemporalReadLimits read;
    const auto instance=instantiate(f,data,context);const auto family=instance.execute(context);const auto evaluation=operator_choice(f,family,grants,1,{3,5,7});const Context choices(grants);
    const auto op=encode_shared_available_relational_finite_operator(f.op);const auto in=encode_shared_available_relational_finite_operator_instance(instance);
    const auto all=encode_shared_available_relational_finite_operator_result(family);const auto value=encode_shared_available_relational_finite_operator_evaluation(evaluation);
    const auto original_op=encode_available_relational_finite_operator(f.op);const auto original_in=encode_available_relational_finite_operator_instance(instance);
    const auto original_all=encode_available_relational_finite_operator_result(family);const auto original_value=encode_available_relational_finite_operator_evaluation(evaluation);
    check(storage::decode_shared_packet(op)==original_op&&storage::decode_shared_packet(in)==original_in&&storage::decode_shared_packet(all)==original_all&&storage::decode_shared_packet(value)==original_value,"every full-parent original v1 byte is exactly reconstructed");
    check(encode_available_relational_finite_operator(decode_shared_available_relational_finite_operator(op,choices,read))==original_op,"operator old-codec canonical interoperability");
    check(encode_available_relational_finite_operator_instance(decode_shared_available_relational_finite_operator_instance(in,choices,read))==original_in,"instance old-codec canonical interoperability");
    check(encode_available_relational_finite_operator_result(decode_shared_available_relational_finite_operator_result(all,choices,read))==original_all,"family old-codec canonical interoperability");
    const auto restored=decode_shared_available_relational_finite_operator_evaluation(value,choices,read);check(encode_available_relational_finite_operator_evaluation(restored)==original_value&&encode_shared_available_relational_finite_operator_evaluation(restored)==value,"completion exact old bytes and canonical shared bytes");
    const auto held=restored.family().instance();const auto finite=held.source().finite();const auto asset=finite.asset();for(const auto& row:held.inputs()){check(row.input.input.asset.same_owner(asset),"sharing byte literals does not duplicate or merge actual independent owners");}
    const auto raw=restored.result().result();const auto output=raw.result();check(output.inputs()[0].accession().asset().same_owner(asset),"selected whole field preserves ONE actual independent source owner");
    const auto fields=output.result();check(restored.family().size()==4&&fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"complete correlated field and unknown alternatives preserved");
    std::set<std::uint64_t> keys;for(const auto& origin:instance.obligations()){keys.insert(origin.key());}keys.insert(1500);keys.insert(1501);
    for(const auto key:keys){rejects_origin([&]{(void)decode_shared_available_relational_finite_operator_evaluation(value,withheld(grants,key),read);},"ALL old parent/input/unused/unmatched/instance/globalsetting/choice cutoffs still required");}
    const auto malformed=storage::encode_shared_packet(false_operator_witness(original_all));
    rejects_origin([&]{(void)decode_shared_available_relational_finite_operator_result(malformed,withheld(grants,data.origins.instance),read);},"whole authority precedes malformed legacy math witness after exact reconstruction");
    rejects([&]{(void)decode_shared_available_relational_finite_operator_result(malformed,choices,read);},"byte sharing never admits a false mathematical witness");
    auto small=read;small.max_file_bytes=value.size()+1;rejects([&]{(void)decode_shared_available_relational_finite_operator_evaluation(value,choices,small);},"compact stored bytes cannot bypass old expanded read policy");
    auto buffer=value;auto mutable_context=choices;auto mutable_policy=read;auto mutable_sharing=Packing{};bool changed{};
    const auto frozen=decode_shared_available_relational_finite_operator_evaluation(buffer,mutable_context,mutable_policy,{},mutable_sharing,[&]{if(!changed){changed=true;buffer.clear();mutable_context=Context({});mutable_policy.max_file_bytes=1;mutable_sharing.max_expanded_bytes=1;}});
    check(changed&&encode_shared_available_relational_finite_operator_evaluation(frozen)==value,"shared wrapper freezes complete external context and both policies before callbacks");
    std::atomic<bool> okay{true};std::vector<std::thread> readers;for(unsigned i=0;i<6;++i){readers.emplace_back([&]{try{const auto cold=decode_shared_available_relational_finite_operator_evaluation(value,choices,read);if(encode_shared_available_relational_finite_operator_evaluation(cold)!=value){okay=false;}}catch(...){okay=false;}});}
    for(auto& reader:readers){reader.join();}check(okay,"six immutable shared full-parent cold readers");
    if(!exports.empty()){std::filesystem::create_directories(exports);write_file(exports/"family.packet",all);write_file(exports/"choice.packet",value);write_file(exports/"authority.bin",storage::encode_shared_packet(authority_bytes(grants)));write_file(exports/"family-original.packet",original_all);write_file(exports/"choice-original.packet",original_value);}
    for(std::size_t trial=0;trial<12;++trial){V x{Q{static_cast<long>(trial)+1,3},Q{static_cast<long>(trial)-2,5},Q{2*static_cast<long>(trial)+1,7},Q{3-static_cast<long>(trial),2}};for(auto& q:x){q.canonicalize();}
        const V xi{Q{static_cast<long>(trial)-2},Q{static_cast<long>(trial)+3},Q{2*static_cast<long>(trial)}};auto current=f.grants;
        const auto d=prepare(f,x,current,3000+100*trial,8300+trial);const auto complete=instantiate(f,d,Context(current)).execute(Context(current));const auto choice=operator_choice(f,complete,current,1,xi,5000+2*trial,5001+2*trial);
        const auto encoded=encode_shared_available_relational_finite_operator_evaluation(choice);const auto cold=decode_shared_available_relational_finite_operator_evaluation(encoded,Context(current),read);
        const auto chosen=cold.result().result();const auto owned=chosen.result();const auto whole=owned.result();const auto all_raw=cold.family().result().result();const auto affine=std::get<RelationalAffineProgramResult>(*all_raw.branch(1).value);
        check(whole.field(2).values().dense()==response(x,{-10+4*xi[0],5-2*xi[0],xi[0]})&&whole.occurrence_field(2).domain().logical_size()==19,"shared cold actual independently supplied complete learned fields unchanged before decoding");
        if(emit){std::cout<<"{\"xi\":";json(xi);std::cout<<",\"input\":";json(x);std::cout<<",\"particular\":";json(affine.field(2).particular.values().dense());std::cout<<",\"directions\":";json(affine.field(2).directions);std::cout<<",\"response\":";json(whole.field(2).values().dense());std::cout<<",\"next\":";json(whole.field(3).values().dense());std::cout<<",\"occurrences\":";json(whole.occurrence_field(2).values().dense());std::cout<<"}\n";}
    }
}
} // namespace
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--primitive"){primitive_checks();std::cout<<checks<<" shared byte checks passed\n";return 0;}
    if(argc==3&&(std::string(argv[1])=="--read-family"||std::string(argv[1])=="--read-choice")){
        const auto path=std::filesystem::path(argv[2]);const auto context=authority(storage::decode_shared_packet(read_file(path.parent_path()/"authority.bin")));const auto bytes=read_file(path);const storage::TemporalReadLimits read;
        if(std::string(argv[1])=="--read-family"){const auto family=decode_shared_available_relational_finite_operator_result(bytes,context,read);check(family.size()==4&&encode_shared_available_relational_finite_operator_result(family)==bytes,"standalone current-registry shared full family");}
        else{const auto choice=decode_shared_available_relational_finite_operator_evaluation(bytes,context,read);const auto raw=choice.result().result();const auto output=raw.result();const auto fields=output.result();check(choice.family().size()==4&&output.inputs()[0].use()==312&&fields.field(2).values().dense()==response({2,3,5,7},{2,-1,3}),"standalone full field/source/use/current origin ancestry");check(encode_shared_available_relational_finite_operator_evaluation(choice)==bytes,"standalone shared canonical completion");}
        std::cout<<checks<<" standalone shared packet checks passed\n";return 0;
    }
    if(argc==3&&std::string(argv[1])=="--write"){run_shared(false,argv[2]);}else{run_shared(argc==2&&std::string(argv[1])=="--emit");}
    if(!(argc==2&&std::string(argv[1])=="--emit")){std::cout<<checks<<" shared packet checks passed\n";}return 0;
}catch(const std::exception& e){std::cerr<<"shared packet failure: "<<e.what()<<'\n';return 1;}}
