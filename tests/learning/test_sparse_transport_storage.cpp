#include "../../examples/sparse_transport_data.hpp"
#include "rfc/storage/declaration_codec.hpp"
#include <iostream>
using namespace sparse_transport_example;
namespace {
std::size_t checks{};void check(bool yes,const char* m){++checks;if(!yes)throw std::runtime_error(m);}
using D=composite::Declaration;using Value=composite::DeclaredValue;
composite::DeclarationId id(std::uint64_t k){return {0xB701,k,1};}
std::vector<Sample> roundtrip(std::size_t n,composite::BooleanLayoutProfile profile) {
 const auto source=samples(n);std::vector<Value> values;
 composite::DeclarationRegistry registry({D::rational_tensor(id(1),{{10,n,{}}}),
     D::rational_tensor(id(2),{{20,2,{10,20}},{10,n,{}}}),D::product(id(3),{{1,id(1),1},{2,id(2),0}})});
 for(const auto& sample:source){std::vector<std::size_t> support;std::vector<numeric::SparseRationalEntry> entries;
    for(const auto& t:sample.targets){auto index=t.cell*n+t.axis;support.push_back(index);if(t.value!=0)entries.push_back({index,t.value});}
    values.push_back(Value::product({Value::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({n}),
        numeric::StructuralSupport::full(n),sample.input)),Value::tensor(numeric::SparseRationalTensor(
        numeric::IndexDomain({2,n}),numeric::StructuralSupport::sparse(2*n,support),S(2*n,entries)))}));
 }
 const composite::CompiledLayout layout(registry,id(3),profile);
 const auto bytes=storage::encode_declared_archive(layout,values);const auto read=storage::decode_declared_archive(bytes);
 check(read.layout.profile()==profile && read.layout.declarations().definitions()==registry.definitions(),"type/profile identities changed");
 check(storage::encode_declared_archive(read.layout,read.values)==bytes,"canonical typed archive differs");
 std::vector<Sample> restored;
 for(const auto& val:read.values){const auto& product=std::get<Value::Product>(val.data).elements;
    const auto& input=std::get<numeric::SparseRationalTensor>(product[0].data);
    const auto& target=std::get<numeric::SparseRationalTensor>(product[1].data);
    check(input.support().count()==n && target.support().count()==2,"input/observation coverage changed");
    Sample sample{input.values(),{}};
    // The finite target support is the observation selection, not every ambient zero.
    for(std::size_t j=0;j<2;++j){const auto index=n+j;check(target.support().contains(index),"target support missing");
       Q value=0;for(const auto& e:target.values().entries())if(e.index==index)value=e.value;
       sample.targets.push_back({1,j,value});}
    restored.push_back(std::move(sample));
 }
 Guard g(limits(n));Fit fit(model(n,g),{},g);Guard h(limits(n));auto learned=fit.append_sparse(restored,h);
 check(learned.particular()==V({Q{3,5},Q{4,5},Q{-4,5},Q{3,5},0,0}),"decoded observations changed fit");
 check(learned.kernel().cols()==2,"decoded evidence discarded original freedom");
 Guard m(limits(n));const auto owner=learned.materialize(V{7,9},m);
 const auto& fibers=owner.fibers();check(fibers.complex().grade(core::Grade{1}).incidence_count()==4,"occurrence identity lost");
 check(fibers.transport_override(core::Grade{1},2)->nonzeros()==0 && fibers.transport_override(core::Grade{1},3)->nonzeros()==0,"inactive parameters became realized contributions");
 check(fibers.transport_override(core::Grade{1},1)->get(0,0)==Q{3,5} && fibers.transport_override(core::Grade{1},1)->get(1,0)==Q{4,5},"actual materialized map differs");
 if(n==1000003)check(bytes.size()<1024,"compact observations unexpectedly expanded");
 return restored;
}
}
int main(){try{
 for(auto p:{composite::BooleanLayoutProfile::Lsb0OctetsV1,composite::BooleanLayoutProfile::Lsb0OctetsV2})(void)roundtrip(7,p);
 (void)roundtrip(1000003,composite::BooleanLayoutProfile::Lsb0OctetsV2);
 std::cout<<checks<<" sparse observation codec/replay checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
