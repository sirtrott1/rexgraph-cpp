#include "rfc/inference/relational_value_admission.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <type_traits>
using namespace rfc;
using namespace rfc::inference;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool hit{};try{action();}catch(const std::exception&){hit=true;}check(hit,message);}
template<class T>concept RvaluePayloadView=requires(T x){std::move(x).payload_bytes();};
static_assert(!RvaluePayloadView<OwnedRelationalValueOrigin>);
void run(){core::RelationSignature signature;const auto t=signature.add_type("pair",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(signature);b.add_grade_one(t,{{1,true,true,1},{2,true,false,1}});
    storage::NativeTemporalSeries series(storage::TemporalEndpoint(b.freeze(),1),801);const auto pub=storage::NativeSeriesPrefix::capture(series,1);
    std::vector<std::byte> bytes(8u<<20,std::byte{7});const auto expected=bytes;
    const OwnedRelationalValueOrigin origin(1,pub,RelationalDependencyRole::InputValue,std::move(bytes));
    const auto count=origin.retained_byte_count();
    check(count==origin.receipt().capacity(),"origin stores one complete compact receipt before legacy payload access");
    const auto view=origin.payload_bytes();check(view.size()==expected.size()&&std::equal(view.begin(),view.end(),expected.begin()),"complete payload view preserved");
    check(view.data()>=origin.receipt().data()&&view.data()+view.size()<=origin.receipt().data()+origin.receipt().size(),"payload view belongs to its own immutable receipt");
    const RelationalAvailabilityContext context({{origin,{801,1}}});
    check(context.require_bytes(1,RelationalDependencyRole::InputValue,view).same_owner(origin),"full span authority comparison retains actual owner");
    context.recheck(origin);check(origin.retained_byte_count()==count,"authority admission and recheck do not materialize redundant payload cache");
    std::vector<std::future<const void*>> readers;
    for(unsigned i=0;i<8;++i)readers.push_back(std::async(std::launch::async,[origin,expected]{
        const auto& payload=origin.payload();if(payload!=expected)throw std::runtime_error("concurrent complete payload cache");
        if(origin.retained_byte_count()!=origin.receipt().capacity()+payload.capacity())throw std::runtime_error("serialized complete buffer accounting");
        return static_cast<const void*>(payload.data());}));
    const auto first=readers.front().get();for(std::size_t i=1;i<readers.size();++i)check(readers[i].get()==first,"one owner-local lazy cache across readers");
    check(origin.payload_bytes().data()==view.data(),"legacy lazy access does not replace original receipt storage");
    rejects([&]{context.require_bytes(1,RelationalDependencyRole::Metrics,view);},"complete cached bytes do not authorize another role");
    auto wrong=expected;wrong.back()=std::byte{8};rejects([&]{context.require_bytes(1,RelationalDependencyRole::InputValue,wrong);},"last payload byte compared exactly");
    const RelationalAvailabilityContext hidden({{origin,{801,0}}});rejects([&]{hidden.recheck(origin);},"cached payload never bypasses current cutoff");
    const OwnedRelationalValueOrigin equal(1,pub,RelationalDependencyRole::InputValue,expected);
    const RelationalAvailabilityContext foreign({{equal,{801,1}}});rejects([&]{foreign.recheck(origin);},"equal receipt and shared literal bytes do not replace live owner");
    check(origin.receipt()==equal.receipt(),"receipt wire stays canonical regardless of cache state");
    check(context.require(1,RelationalDependencyRole::InputValue,expected).same_owner(origin),"existing vector authority API delegates identical complete check");
}
}
int main(){try{run();std::cout<<checks<<" origin storage and cache checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
