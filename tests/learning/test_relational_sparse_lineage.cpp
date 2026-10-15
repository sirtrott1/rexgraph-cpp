#include "rfc/learning/relational_affine_program_codec.hpp"
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using T = numeric::SparseRationalTensor;
namespace {
constexpr std::size_t n = 7208;
std::size_t checks{};
void check(bool value, const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
T tensor(const V& values) { numeric::ExactActionGuard guard({n,16000000,1000000,262144});return field_interface_detail::tensor(values,guard); }
numeric::ExactActionLimits limits() { return {n+1,16000000,1000000,262144}; }
storage::NativeSeriesEndpoint source() {
    core::RelationSignature signature;
    const auto type=signature.add_type("full-map-axis",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100},type,{{10,true,false,1},{20,true,false,1}});
    storage::TemporalEndpoint endpoint(builder.freeze(),7);return storage::NativeTemporalSeries(endpoint,991).tail();
}
OwnedRelationalFieldProgram program(const storage::NativeSeriesEndpoint& p) {
    RelationalFieldProgramDeclaration d;d.key=1;d.revision=1;d.differential=true;
    d.stalks={{{{0},0},11,{12},M::identity(1)},{{{0},1},21,{22},M::identity(1)}};
    std::vector<std::uint64_t> axes;for(std::size_t i=0;i<n;++i)axes.push_back(1000+i);
    d.stalks.push_back({{{1},0},31,std::move(axes),M::identity(n)});
    d.occurrences={{{1},0,storage::TemporalTensorRole::CoefficientIndependentMap,M(1,n)},
                   {{1},1,storage::TemporalTensorRole::CoefficientIndependentMap,M(1,n)}};
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {p,std::move(d),limits()};
}
OwnedRelationalInputUseAsset asset(const storage::NativeSeriesEndpoint& p) {
    RelationalInputUseDeclaration d;d.key=1;d.channel=2;d.encoding="UTF-8";d.bytes={std::byte{'x'}};
    d.uses={{111,{{{1},100}},{{{1},p.endpoint().source.occurrence_identity({1},0)},
                            {{1},p.endpoint().source.occurrence_identity({1},1)}},{{0,1},{0,0}}}};
    return {storage::NativeTemporalSeries(p.endpoint(),992),std::move(d)};
}
OwnedAccessionFieldResult input(const OwnedRelationalFieldProgram& p,const OwnedRelationalInputUseAsset& a,
    std::size_t axis) {
    RelationalInputAccessionDeclaration d;d.key=1;d.node=1;d.source_grade={1};d.isometric=true;
    d.source_stalks={p.declaration().stalks.back()};d.instances.cells={{0,1},{0}};d.instances.occurrences={{},{0,1}};
    d.accession=M::identity(n);d.reconstruction=d.accession;
    const OwnedRelationalInputAccession owner(a,p,d,limits());V values(n);values[axis]=1;
    return execute_relational_inputs(p,{owner.admit(111,tensor(values),limits())},limits());
}
RelationalFieldParameterDeclaration parameters() {
    RelationalFieldParameterDeclaration d;d.key=1;d.revision=1;d.node=2;
    for(std::size_t i=0;i<n;++i){d.variables.push_back({1,0,i});d.parameters.push_back(10000+i);}
    d.basis=M::identity(n);return d;
}
void run() {
    const auto publication=source();const auto p=program(publication);const auto a=asset(publication);
    const PreparedRelationalFieldEvidence root(p,parameters(),limits());
    check(root.kernel()==M::identity(n),"all7208 actual occurrence-map directions retained before evidence");
    const auto first_input=input(p,a,0),last_input=input(p,a,n-1);
    const RelationalFieldTrainingObservation first{1,first_input,a,111,tensor({0,2}),{{{0},0},{{0},1}}};
    std::size_t checkpoints{};
    const auto child=root.append({first},limits(),[&]{++checkpoints;});
    check(checkpoints<1000000,"child lineage fits represented-action step policy");
    check(child.kernel().cols()==n-1 && child.free_parameters().front()==10001,"complete unobserved free names retained");
    const auto relation=child.parent_relation();check(relation.has_value(),"actual child-to-parent relation retained");
    check(relation->map.rows()==n && relation->map.cols()==n-1 && relation->map.nonzeros()==n-1,"full sparse rectangular lineage map preserves every free axis");
    for(std::size_t i=0;i<n;++i)check(relation->shift[i]==(i==0?2:0),"complete original parameter shift");
    for(std::size_t i=0;i<n-1;++i)check(relation->map.get(i+1,i)==1,"ordered free-axis descent remains exact");
    check(root.kernel().multiply(relation->map)==child.kernel(),"parent family kernel descent certificate");
    const auto held=child.execute(last_input,limits());
    check(!held.identified(2,1) && held.field(2).directions.cols()==n-1 && held.field(2).directions.get(1,n-2)==1,"off-evidence whole held response retains original last free direction");
    const RelationalFieldTrainingObservation second{2,last_input,a,111,tensor({0,3}),{{{0},0},{{0},1}}};
    const auto grandchild=child.append({second},limits());
    check(grandchild.kernel().cols()==n-2 && grandchild.parent_relation()->map.rows()==n-1,"second sparse update preserves actual lineage extent");
    check(grandchild.execute(last_input,limits()).identified(2,1),"new evidence identifies actual held map action");
    storage::TemporalReadLimits policy;policy.max_dimension=n+1;
    const auto packet=encode_relational_field_evidence(grandchild);
    const auto cold=decode_relational_field_evidence(packet,policy,limits());
    check(encode_relational_field_evidence(cold)==packet && cold.parent_relation()->map==grandchild.parent_relation()->map,"canonical complete cold multi-update lineage");
    check(cold.parent()->parent()->kernel().cols()==n,"cold original full source and all initial axes preserved");
    std::vector<std::future<bool>> readers;
    for(unsigned i=0;i<3;++i)readers.push_back(std::async(std::launch::async,[packet,policy]{
        return encode_relational_field_evidence(decode_relational_field_evidence(packet,policy,limits()))==packet;
    }));
    for(auto& reader:readers)check(reader.get(),"immutable concurrent complete sparse lineage readers");
    std::cout<<"append_checkpoints="<<checkpoints<<'\n';
}
} // namespace
int main(){try{run();std::cout<<checks<<" sparse full-map lineage checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"sparse lineage failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
