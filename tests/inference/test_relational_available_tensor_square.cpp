#include "rfc/inference/relational_available_tensor_square_codec.hpp"
#include "rfc/core/builder.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string_view>

using namespace rfc;
using namespace rfc::inference;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using T = numeric::SparseRationalTensor;
using Bytes = std::vector<std::byte>;
namespace {
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
template<class F> void before_math(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::invalid_argument&) { caught = true; } check(caught, message);
}
T tensor(V values) {
    for (auto& q : values) q.canonicalize();
    numeric::ExactActionGuard guard; return field_interface_detail::tensor(values, guard);
}
storage::NativeTemporalSeries history(std::uint64_t ns) {
    core::RelationSignature signature;
    const auto type = signature.add_type("context", core::BoundaryPolicy::DeclaredExact, 1, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10,true,false,Q{1}}, {20,true,true,Q{1}}, {10,true,true,Q{1}},
         {20,false,false,Q{1}}, {30,true,false,Q{0}}});
    storage::TemporalEndpoint endpoint(builder.freeze(),7); storage::NativeTemporalSeries series(endpoint, ns);
    for (std::uint64_t i=8;i<=13;++i) { endpoint.sequence=i; series.append_target(endpoint); }
    return series;
}
M metric(bool lifted) {
    const M original=M::from_rows({{2,1},{1,3}}); if (!lifted) return original;
    M out(6,6);
    for (const auto& [ij,q] : original.entries()) {
        out.set(ij.first,ij.second,q);
        for (const auto& [kl,r] : original.entries()) out.set(2+2*ij.first+kl.first,2+2*ij.second+kl.second,q*r);
    }
    return out;
}
OwnedRelationalFieldProgram program(const storage::NativeTemporalSeries& series, bool lifted) {
    RelationalFieldProgramDeclaration d; d.key=lifted?42:41; d.revision=1; d.differential=true;
    for (std::uint32_t i=0;i<3;++i) d.stalks.push_back({{{0},i},101,{102},M::identity(1)});
    d.stalks.push_back({{{1},0},lifted?400u:303u,
        lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},metric(lifted)});
    for (std::size_t i=0;i<5;++i) {
        M map(1,lifted?6:2); map.set(0,0,1); map.set(0,1,2); if (lifted) map.set(0,3,1);
        d.occurrences.push_back({{1},i,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {series.at(7),std::move(d)};
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series) {
    RelationalInputUseDeclaration d; d.key=600; d.channel=700; d.encoding="UTF-8";
    for (const auto c:std::string("caf\xc3\xa9 / caf\xc3\xa9")) d.bytes.push_back(static_cast<std::byte>(c));
    RelationalSpanUse use{111,{{{1},100}},{},{{0,5},{8,13},{0,0},{0,5}}};
    const auto publication=series.at(7); const auto source=publication.endpoint().source;
    for (std::size_t i=0;i<5;++i) use.occurrences.push_back({{1},source.occurrence_identity({1},i)});
    d.uses={use}; return {storage::NativeSeriesPrefix::capture(series,8).history(),std::move(d)};
}
RelationalInputAccessionDeclaration declaration(bool lifted) {
    RelationalInputAccessionDeclaration d; d.key=lifted?902:901; d.node=1; d.source_grade={1}; d.isometric=true;
    d.source_stalks={{{{1},0},lifted?400u:303u,
        lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},metric(lifted)}};
    d.instances.cells={{0,1,2},{0}}; d.instances.occurrences={{},{0,1,2,3,4}};
    d.accession=M::identity(lifted?6:2); d.reconstruction=d.accession; return d;
}
RelationalKnownTensorSquareDeclaration square_declaration() { return {701,1,400,{{1},0},{501,502,503,504}}; }
constexpr RelationalTensorSquareOrigins keys{{1,2,3},{4,5,6,7},8,{9,10,11},{12,13,14,15}};
struct Fixture {
    storage::NativeTemporalSeries series=history(991), publications=history(992);
    OwnedRelationalFieldProgram parent=program(series,false), target=program(series,true);
    OwnedRelationalInputUseAsset bytes=asset(series);
    RelationalAccessionInput original;
    RelationalKnownTensorSquareInput reference;
    std::vector<RelationalOriginGrant> grants;
    explicit Fixture(V value) :
        original(OwnedRelationalInputAccession(bytes,parent,declaration(false)).admit(111,tensor(std::move(value)))),
        reference(OwnedRelationalKnownTensorSquare(original,square_declaration()).admit(
            OwnedRelationalInputAccession(bytes,target,declaration(true)))) {
        using Role=RelationalDependencyRole;
        const auto early=storage::NativeSeriesPrefix::capture(publications,8),late=storage::NativeSeriesPrefix::capture(publications,12);
        const runtime::WorkAvailability current{992,12};
        const auto pp=encode_relational_field_program(parent),tp=encode_relational_field_program(target);
        const auto bp=encode_relational_input_use_asset(bytes),pv=relational_input_value_payload(original),pa=relational_input_accession_payload(original);
        const auto derived=reference.input(); const auto tv=relational_input_value_payload(derived),ta=relational_input_accession_payload(derived);
        const auto add=[&](std::uint64_t key,Role role,Bytes payload,bool newer=false) {
            grants.push_back({OwnedRelationalValueOrigin(key,newer?late:early,role,std::move(payload)),current});
        };
        add(1,Role::Program,pp); add(2,Role::Maps,pp); add(3,Role::Metrics,pp);
        add(4,Role::InputAsset,bp); add(5,Role::InputValue,pv,true); add(6,Role::InputAccession,pa); add(7,Role::Reconstruction,pa);
        add(8,Role::Interface,relational_tensor_square_dependency_payload(parent,square_declaration()),true);
        add(9,Role::Program,tp,true); add(10,Role::Maps,tp,true); add(11,Role::Metrics,tp,true);
        add(12,Role::InputAsset,bp); add(13,Role::InputValue,tv,true); add(14,Role::InputAccession,ta,true); add(15,Role::Reconstruction,ta,true);
        add(16,Role::TargetValue,tv,true);
    }
    RelationalInputSubmission submission() const { return {bytes,declaration(false),111,original.source_field()}; }
};
Bytes graft(const Bytes& native, const AvailableRelationalTensorSquareInput& input) {
    using namespace available_tensor_square_codec_detail;
    auto w=envelope(input_magic); blob(w,native); origins(w,input.origins());
    for (const auto& origin:input.obligations()) blob(w,origin.receipt());
    return finish(std::move(w));
}
void json(const T& field) {
    std::cout<<'[';
    for (std::size_t i=0;i<field.domain().logical_size();++i) { if(i)std::cout<<',';std::cout<<'"'<<field.state(i).value()<<'"'; }
    std::cout<<']';
}
void run(bool emit) {
    for (std::size_t trial=0;trial<20;++trial) {
        V x{Q{static_cast<long>(trial)-7,3},Q{static_cast<long>(trial%11)-4,5}};
        for (auto& q:x) q.canonicalize();
        const Fixture f(x); const RelationalAvailabilityContext context(f.grants);
        const PreparedAvailableRelationalProgram parent(f.parent,keys.parent_program,context),target(f.target,keys.program,context);
        const PreparedAvailableRelationalTensorSquare producer(parent,square_declaration(),keys.declaration,context);
        const auto square=producer.lift(f.submission(),keys.parent_input,context);
        const auto input=square.admit_input(target,declaration(true),keys.input,context);
        const auto result=input.execute(context); result.admit(context);
        check(square.obligations().size()==8 && input.obligations().size()==15,"complete original plus derived obligations retained");
        check(encode_relational_known_tensor_square_input(input.native_input())==encode_relational_known_tensor_square_input(f.reference),"available owner preserves whole native ancestry");
        check(encode_accession_field_result(result.result())==encode_accession_field_result(execute_relational_inputs(f.target,{f.reference.input()})),"admitted complete action equals native original action");
        const auto native=result.result(); const auto fields=native.result(); const auto actual=input.native_input(); const auto product=actual.square();
        check(fields.field(2).values().dense()==V({0,-(x[0]+2*x[1]+x[0]*x[1]),0}),"complete rational downstream response");
        check(product.lifted_field().support().count()==6 && fields.occurrence_field(2).support().count()==5,"supported zero and all ordered flags retained");
        const auto packet=encode_available_relational_tensor_square_input(input),output=encode_available_relational_tensor_square_result(result);
        const auto cold=decode_available_relational_tensor_square_input(packet,context,{});
        const auto restored=decode_available_relational_tensor_square_result(output,context,{});
        check(encode_available_relational_tensor_square_input(cold)==packet && encode_available_relational_tensor_square_result(restored)==output,"canonical current-authority cold replay");
        const auto restored_native=cold.native_input(); const auto restored_square=restored_native.square();
        check(restored_native.input().accession().asset().same_owner(restored_square.parent().accession().asset()),"one original asset owner restored for both parents");
        check(restored_square.parent().accession().asset().recover(111,20).size()==4,"original repeated empty and UTF8 spans retained");
        auto no_math=numeric::ExactActionLimits{};no_math.dimension=0;no_math.steps=0;
        for (std::size_t dependency=0;dependency<15;++dependency) {
            auto hidden=f.grants;hidden[dependency].availability.max_sequence=hidden[dependency].origin.publication().cutoff()-1;
            const RelationalAvailabilityContext unavailable(hidden);
            rejects([&]{input.admit(unavailable);},"warm input checks each original or derived cutoff");
            rejects([&]{result.admit(unavailable);},"warm result checks original and derived cutoffs");
            before_math([&]{input.execute(unavailable,no_math);},"execution refuses unavailable dependency before arithmetic");
            before_math([&]{decode_available_relational_tensor_square_input(packet,unavailable,{},no_math);},"cold input preflights every dependency before product");
            before_math([&]{decode_available_relational_tensor_square_result(output,unavailable,{},no_math);},"cold result preflights every dependency before product and action");
            auto copied=f.grants;const auto old=copied[dependency].origin;
            copied[dependency].origin=OwnedRelationalValueOrigin(old.key(),old.publication(),old.role(),old.payload());
            rejects([&]{input.admit(RelationalAvailabilityContext(copied));},"equal receipt cannot replace actual warm origin owner");
            if (dependency<8) before_math([&]{producer.lift(f.submission(),keys.parent_input,unavailable,no_math);},"original producer preflights before A/R and multiplication");
            before_math([&]{square.admit_input(target,declaration(true),keys.input,unavailable,no_math);},"derived input preflights both parents before A/R");
        }
        if (emit) {
            std::cout<<"{\"original\":";json(product.context_field());std::cout<<",\"square\":";json(product.tensor_square());
            std::cout<<",\"lifted\":";json(fields.field(1));std::cout<<",\"boundary\":";json(fields.field(2));
            std::cout<<",\"occurrences\":";json(fields.occurrence_field(2));std::cout<<"}\n";
        }
    }
}
void adversarial() {
    const Fixture f({2,3}); const RelationalAvailabilityContext context(f.grants);
    const PreparedAvailableRelationalProgram parent(f.parent,keys.parent_program,context),target(f.target,keys.program,context);
    const PreparedAvailableRelationalTensorSquare producer(parent,square_declaration(),keys.declaration,context);
    const auto square=producer.lift(f.submission(),keys.parent_input,context);
    const auto input=square.admit_input(target,declaration(true),keys.input,context); const auto result=input.execute(context);
    const auto packet=encode_available_relational_tensor_square_input(input),output=encode_available_relational_tensor_square_result(result);
    numeric::ExactActionLimits no_math;no_math.dimension=0;no_math.steps=0;
    auto absent=f.grants;absent.erase(absent.begin()+4);
    before_math([&]{decode_available_relational_tensor_square_input(packet,RelationalAvailabilityContext(absent),{},no_math);},"missing parent value cannot be laundered through available derived input");
    auto wrong_namespace=f.grants;wrong_namespace[4].availability.source_namespace=993;
    before_math([&]{producer.lift(f.submission(),keys.parent_input,RelationalAvailabilityContext(wrong_namespace),no_math);},"namespace-local original availability");
    auto target_role=keys.input;target_role.value=16;
    before_math([&]{square.admit_input(target,declaration(true),target_role,context,no_math);},"target publication cannot be relabeled as known input");
    auto changed=f.submission();changed.source_field=tensor({3,3});
    before_math([&]{producer.lift(changed,keys.parent_input,context,no_math);},"original complete coefficient content checked before product");
    auto changed_declaration=declaration(true);changed_declaration.key=903;
    const auto altered_native=f.reference.square().admit(OwnedRelationalInputAccession(f.bytes,f.target,changed_declaration));
    before_math([&]{decode_available_relational_tensor_square_input(graft(encode_relational_known_tensor_square_input(altered_native),input),context,{},no_math);},"valid receipt cannot be grafted onto altered derived accession");
    auto new_square=square_declaration();new_square.key=702;
    const auto altered_square=OwnedRelationalKnownTensorSquare(f.original,new_square).admit(OwnedRelationalInputAccession(f.bytes,f.target,declaration(true)));
    before_math([&]{decode_available_relational_tensor_square_input(graft(encode_relational_known_tensor_square_input(altered_square),input),context,{},no_math);},"interface origin binds exact original program and ordered product declaration");
    const Fixture different({3,3});
    before_math([&]{decode_available_relational_tensor_square_input(graft(encode_relational_known_tensor_square_input(different.reference),input),context,{},no_math);},"grafted parent field rejected before product");
    using namespace available_tensor_square_codec_detail;
    auto w=envelope(available_tensor_square_codec_detail::result_magic);blob(w,packet);
    blob(w,encode_accession_field_result(execute_relational_inputs(different.target,{different.reference.input()})));
    const auto false_response=finish(std::move(w));
    rejects([&]{decode_available_relational_tensor_square_result(false_response,context,{});},"cold replay verifies complete response rather than saved witness");
    storage::TemporalReadLimits byte_cap;byte_cap.max_file_bytes=packet.size()-1;
    rejects([&]{decode_available_relational_tensor_square_input(packet,context,byte_cap);},"cold byte cap before copy");
    numeric::ExactActionLimits cap;cap.dimension=5;
    rejects([&]{producer.lift(f.submission(),keys.parent_input,context,cap);},"exact product dimension cap");
    rejects([&]{decode_available_relational_tensor_square_input(packet,context,{},cap);},"cold exact action cap");
    bool stopped{};
    rejects([&]{producer.lift(f.submission(),keys.parent_input,context,{},[&]{stopped=true;throw std::runtime_error("cancel");});},"cancelled preparation exposes no child");
    check(stopped,"owned callback reached");
    rejects([&]{input.execute(context,{},[]{throw std::runtime_error("cancel");});},"cancelled execution exposes no result");
    auto buffer=output;auto mutable_context=context;auto mutable_input=input;bool changed_owner{};
    const auto cold=decode_available_relational_tensor_square_result(buffer,mutable_context,{}, {},[&]{
        if(!changed_owner){changed_owner=true;buffer.clear();mutable_context=RelationalAvailabilityContext(absent);}
    });
    check(changed_owner && encode_available_relational_tensor_square_result(cold)==output,"cold bytes and actual context pinned before hostile callbacks");
    bool replaced{};
    const auto pinned=mutable_input.execute(context,{},[&]{if(!replaced){replaced=true;mutable_input=cold.input();}});
    check(replaced && encode_available_relational_tensor_square_result(pinned)==output,"executing compound owner pinned before replacement");
    std::vector<std::future<Bytes>> readers;
    for(unsigned i=0;i<6;++i) readers.push_back(std::async(std::launch::async,[output,context]{
        return encode_available_relational_tensor_square_result(decode_available_relational_tensor_square_result(output,context,{}));
    }));
    for(auto& reader:readers) check(reader.get()==output,"six concurrent current-authority cold readers");
    const auto escaped=[&]{const auto local=decode_available_relational_tensor_square_input(packet,context,{});return local.execute(context);}();
    check(encode_available_relational_tensor_square_result(escaped)==output,"complete parent and derived owners survive producer destruction");
    std::ofstream file("available-known-square-result.packet",std::ios::binary);
    file.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()));
}
} // namespace
int main(int argc,char** argv) {
    try {
        const bool emit=argc==2 && std::string_view(argv[1])=="--json";
        if(argc==2 && !emit) {
            const Fixture publisher({2,3});const RelationalAvailabilityContext context(publisher.grants);
            std::ifstream file(argv[1],std::ios::binary);const std::vector<char> chars{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
            Bytes bytes;for(const auto c:chars)bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
            const auto result=decode_available_relational_tensor_square_result(bytes,context,{});
            const auto native=result.result();const auto fields=native.result();
            check(fields.field(2).values().dense()==V({0,-14,0}),"fresh current fixture authority complete field replay");
            check(encode_available_relational_tensor_square_result(result)==bytes,"fresh canonical replay");
        } else { run(emit);adversarial(); }
        if(!emit)std::cout<<checks<<" available tensor square checks passed\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
