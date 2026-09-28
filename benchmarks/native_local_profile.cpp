// End-to-end costs are published separately from hot lookup/packed word costs.
// This profile does not disable validation or claim cold OS page-cache timing.
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query.hpp"
#include "rfc/models/native_context.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>

namespace {
using namespace rfc;using namespace database;using Clock=std::chrono::steady_clock;
using V=composite::DeclaredValue;using D=composite::Declaration;
struct Fixture {InterpretationSchema schema;LogicalSnapshot data;};
Fixture fixture(std::size_t count){
    Fixture f;f.schema.source_namespace=6001;f.schema.binding_namespace=6002;
    const composite::DeclarationId q{6003,1,1},p{6003,2,1};f.schema.values=composite::DeclarationRegistry({D::rational(q),D::product(p,{{1,q,0}})});
    f.schema.subjects={{1,"subject"}};f.schema.assertions={{10,"datum",p,{{11,1,1,4,false,false}},{{1,"value"}}}};
    f.data.revision="0";f.data.subjects={{1,"a"},{1,"b"}};
    for(std::size_t i=0;i<count;++i){numeric::Rational value{1,3};if(i==0){numeric::Integer big{1};big<<=256;value=numeric::Rational(big,3);value.canonicalize();}
        f.data.assertions.push_back({{10,std::to_string(i)},V::product({V::rational(value)}),{{11,ReferenceState::Present,{{"left",{1,"a"},true},{"right",{1,"b"},true}}}}});}
    return f;
}
template<class Fn> void measure(std::size_t n,const char* phase,std::size_t repetitions,Fn&& fn){
    std::vector<long long> times;for(std::size_t i=0;i<repetitions;++i){const auto t=Clock::now();fn();times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count());}
    std::sort(times.begin(),times.end());std::cout<<n<<','<<phase<<','<<times[times.size()/2]<<','<<times.back()<<','<<repetitions<<'\n';
}
}
int main(int argc,char** argv){try{
    const std::size_t repetitions=argc>1?std::stoul(argv[1]):3;if(!repetitions || repetitions>21)throw std::invalid_argument("repetitions 1..21");
    std::cout<<"assertions,phase,median_ns,max_ns,repetitions\n";
    const auto profile_directory=argc>2?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("rfc-native-profile-"+std::to_string(Clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(profile_directory))throw std::invalid_argument("profile directory must be new");
    for(std::size_t n:{16u,128u,512u}){
        auto f=fixture(n);const auto base=InterpretedSnapshot::interpret(f.schema,f.data);const auto bytes=encode_interpreted_snapshot(base);const auto expected=sum_exact(base,10,{1});
        measure(n,"interpret_validate_index",repetitions,[&]{const auto x=InterpretedSnapshot::interpret(f.schema,f.data);if(x.assertions(10).size()!=n)throw std::logic_error("interpret count");});
        storage::NativeAccessionDirectory directory(*base.endpoint(),6001);
        storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::DeclaredValue;request.subject=1;request.factors={1,1};
        const auto handle=directory.intern(request);const auto before=directory.stats();
        measure(n,"known_handle_10000",repetitions,[&]{for(int i=0;i<10000;++i)if(directory.resolve(handle).sequence()!=0)throw std::logic_error("handle sequence");});
        const auto after=directory.stats();if(after.handle_resolutions-before.handle_resolutions!=10000*repetitions)throw std::logic_error("handle work count");if(after.descriptor_hashes!=before.descriptor_hashes || after.cold_bindings!=before.cold_bindings || after.bucket_comparisons!=before.bucket_comparisons)throw std::logic_error("hot path indexed source");
        measure(n,"cold_access_100",repetitions,[&]{for(int i=0;i<100;++i)if(base.payload(1,{1}).sequence()!=0)throw std::logic_error("cold access");});
        measure(n,"exact_sum",repetitions,[&]{if(sum_exact(base,10,{1})!=expected)throw std::logic_error("sum differs");});
        storage::NativeAtlasManifest manifest;manifest.key=1;manifest.revision=1;
        const storage::NativeAtlas atlas({base.source()},manifest);
        measure(n,"authorized_context_projection",repetitions,[&]{
            const auto context=models::NativeConsumerContext::build(atlas,{{1,request}},[&](const auto& origin){return origin.source==1 && origin.accession.kind==request.kind && origin.accession.subject==1 && origin.accession.factors==request.factors;});
            if(context.items().size()!=1 || std::get<numeric::Rational>(std::get<models::NativeDeclaredContext>(context.items().front().contents).value.data)!=std::get<numeric::Rational>(base.payload(1,{1}).declared_value()->data))throw std::logic_error("context projection");
        });
        measure(n,"encode_snapshot",repetitions,[&]{if(encode_interpreted_snapshot(base)!=bytes)throw std::logic_error("encode differs");});
        measure(n,"decode_validate_reconstruct",repetitions,[&]{const auto x=decode_interpreted_snapshot(bytes);if(x.assertions(10).size()!=n)throw std::logic_error("decode count");});
        f.data.revision="1";f.data.assertions.front().payload=V::product({V::rational(numeric::Rational{5,7})});
        const auto target=base.replace(f.schema,f.data);const auto update=base.difference(target);const auto target_bytes=encode_interpreted_snapshot(target);
        measure(n,"replace_whole_endpoint",repetitions,[&]{if(encode_interpreted_snapshot(base.replace(f.schema,f.data))!=target_bytes)throw std::logic_error("replacement differs");});
        measure(n,"make_delta",repetitions,[&]{(void)base.difference(target);});
        measure(n,"replay_validate_reconstruct",repetitions,[&]{if(encode_interpreted_snapshot(base.apply(update))!=target_bytes)throw std::logic_error("replay differs");});
        const auto source_path=profile_directory/(std::to_string(n)+".rfcx");
        storage::write_rfcx(base.endpoint()->source,source_path);
        measure(n,"fresh_mapped_open_validate",repetitions,[&]{storage::MappedRfcx image(source_path);if(image.grade(core::Grade{1}).incidence_count()!=2*n)throw std::logic_error("mapped count");});
        const storage::MappedRfcx image(source_path);
        const auto program=composite::compile_active_orientation_query(0,1,false);
        const composite::BinaryExecutionPlan plan(program,{2});composite::BinaryExecutionWorkspace scratch;
        if(image.binary_count(core::Grade{1},plan,scratch)!=2*n)throw std::logic_error("mapped initial query");
        measure(n,"warm_mapped_count_100",repetitions,[&]{for(int i=0;i<100;++i)if(image.binary_count(core::Grade{1},plan,scratch)!=2*n)throw std::logic_error("mapped query");});
        // Forced collision work stays explicit rather than presented as O(1).
        storage::NativeAccessionDirectory collisions(*base.endpoint(),6001,{},[](auto){return 0;});
        for(std::size_t i=1;i<=n;++i){request.subject=i;(void)collisions.intern(request);}
        const auto collision_before=collisions.stats();request.subject=n;(void)collisions.find(request);
        const auto comparisons=collisions.stats().bucket_comparisons-collision_before.bucket_comparisons;
        if(comparisons!=n)throw std::logic_error("collision counter");
        std::cerr<<"assertions="<<n<<" snapshot_bytes="<<bytes.size()<<" delta_bytes="<<encode_interpretation_update(update).size()<<" forced_collision_comparisons="<<comparisons<<" exact=true\n";
    }return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
