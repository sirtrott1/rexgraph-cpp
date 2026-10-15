#include "rfc/inference/relational_known_tensor_square_codec.hpp"
#include "rfc/core/builder.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>

using namespace rfc;
using namespace rfc::inference;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
using T = numeric::SparseRationalTensor;
namespace {
std::size_t checks{};
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool caught{}; try { action(); } catch (const std::exception&) { caught = true; } check(caught, message);
}
T tensor(const V& values) { numeric::ExactActionGuard guard; return field_interface_detail::tensor(values, guard); }
storage::NativeSeriesEndpoint source() {
    core::RelationSignature signature;
    const auto type = signature.add_type("context", core::BoundaryPolicy::DeclaredExact, 1, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type,
        {{10, true, false, Q{1}}, {20, true, true, Q{1}}, {10, true, true, Q{1}},
         {20, false, false, Q{1}}, {30, true, false, Q{0}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 7);
    return storage::NativeTemporalSeries(endpoint, 991).tail();
}
OwnedRelationalInputUseAsset asset(const storage::NativeSeriesEndpoint& p) {
    storage::NativeTemporalSeries history(p.endpoint(), 992);
    auto next = p.endpoint(); next.sequence = 8; history.append_target(next);
    RelationalInputUseDeclaration d; d.key = 600; d.channel = 700; d.encoding = "UTF-8";
    const std::string raw = "caf\xc3\xa9 / caf\xc3\xa9";
    for (const auto c : raw) d.bytes.push_back(static_cast<std::byte>(c));
    RelationalSpanUse first{111, {{core::Grade{1}, 100}}, {}, {{0,5}}};
    for (std::size_t slot = 0; slot < 5; ++slot)
        first.occurrences.push_back({core::Grade{1}, p.endpoint().source.occurrence_identity(core::Grade{1}, slot)});
    d.uses = {first, {112, {{core::Grade{1}, 100}}, {}, {{8,13}, {0,0}, {0,5}}}};
    return {history, std::move(d)};
}
M original_metric() { return M::from_rows({{2,1},{1,3}}); }
M lifted_metric(const M& metric) {
    M out(6,6);
    for (std::size_t i=0;i<2;++i) for (std::size_t j=0;j<2;++j) {
        out.set(i,j,metric.get(i,j));
        for (std::size_t k=0;k<2;++k) for (std::size_t l=0;l<2;++l)
            out.set(2+2*i+k,2+2*j+l,metric.get(i,j)*metric.get(k,l));
    }
    return out;
}
RelationalKnownTensorSquareDeclaration square_declaration() { return {701,1,400,{core::Grade{1},0},{501,502,503,504}}; }
OwnedRelationalFieldProgram program(const storage::NativeSeriesEndpoint& publication, bool lifted,
    std::uint64_t key=41, M metric=original_metric()) {
    RelationalFieldProgramDeclaration d; d.key=key; d.revision=1; d.differential=true;
    for (std::uint32_t i=0;i<3;++i) d.stalks.push_back({{core::Grade{0},i},101,{102},M::identity(1)});
    const auto n=lifted?6u:2u;
    d.stalks.push_back({{core::Grade{1},0},lifted?400u:303u,
        lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},
        lifted?lifted_metric(metric):metric});
    for (std::size_t slot=0;slot<5;++slot) {
        M map(1,n); map.set(0,0,1); map.set(0,1,2); if(lifted) map.set(0,3,1);
        d.occurrences.push_back({core::Grade{1},slot,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    d.nodes={{1,FieldProgramNodeKind::Input,core::Grade{1},{},{}},
             {2,FieldProgramNodeKind::Boundary,core::Grade{0},1,{}}};
    return {publication,std::move(d)};
}
OwnedRelationalInputAccession accession(const OwnedRelationalInputUseAsset& bytes,
    const OwnedRelationalFieldProgram& p, bool lifted, M metric=original_metric()) {
    RelationalInputAccessionDeclaration d; d.key=lifted?902:901; d.node=1; d.source_grade={1}; d.isometric=true;
    d.source_stalks={{{core::Grade{1},0},lifted?400u:303u,
        lifted?std::vector<std::uint64_t>{31,32,501,502,503,504}:std::vector<std::uint64_t>{31,32},
        lifted?lifted_metric(metric):metric}};
    d.instances.cells={{0,1,2},{0}}; d.instances.occurrences={{},{0,1,2,3,4}};
    d.accession=M::identity(lifted?6:2); d.reconstruction=d.accession;
    return {bytes,p,std::move(d)};
}
std::vector<std::byte> false_square_witness(const OwnedRelationalKnownTensorSquare& square) {
    using namespace known_tensor_square_codec_detail;
    auto w=envelope(square_magic);
    const auto input=square.parent(); const auto owner=input.accession();
    blob(w,encode_relational_field_program(owner.program()));
    blob(w,encode_relational_input_use_asset(owner.asset()));
    field_interface_codec_detail::accession(w,owner.declaration());
    w.u64(input.use()); storage::temporal_codec_detail::tensor(w,input.source_field());
    declaration(w,square.declaration()); Writer bad;
    storage::temporal_codec_detail::tensor(bad,square.context_field());
    auto values=square.tensor_square().values().dense(); values[0]+=1;
    const auto flat=tensor(values);
    T changed(numeric::IndexDomain({2,2}),numeric::StructuralSupport::full(4),flat.values());
    storage::temporal_codec_detail::tensor(bad,changed);
    storage::temporal_codec_detail::tensor(bad,square.lifted_field());
    storage::temporal_codec_detail::matrix(bad,square.metric()); blob(w,bad.bytes()); return finish(std::move(w));
}
void corpus() {
    const auto publication=source(); const auto bytes=asset(publication);
    const auto before=program(publication,false), after=program(publication,true,42);
    const auto a=accession(bytes,before,false), b=accession(bytes,after,true);
    for (std::size_t trial=0;trial<40;++trial) {
        V x{Q{static_cast<long>(trial)-7,3},Q{static_cast<long>(trial%11)-4,5}};
        for(auto& q:x) q.canonicalize();
        const auto original=a.admit(trial%2?111:112,tensor(x));
        const OwnedRelationalKnownTensorSquare square(original,square_declaration());
        check(square.context_field().values().dense()==x,"actual owning context field");
        const V pairs{x[0]*x[0],x[0]*x[1],x[1]*x[0],x[1]*x[1]};
        const V lifted{x[0],x[1],pairs[0],pairs[1],pairs[2],pairs[3]};
        check(square.tensor_square().domain().extents()==std::vector<std::size_t>{2,2} &&
            square.tensor_square().values().dense()==pairs,"complete ordered stalk tensor square");
        check(square.lifted_field().values().dense()==lifted,"original context retained beside its square");
        check(square.tensor_square().support().count()==4 && square.lifted_field().support().count()==6,"supported zeros retained");
        square.require_image(square.lifted_field());
        const auto coordinate=square.pair_coordinate(1);
        check(coordinate.original_basis==303 && coordinate.left==31 && coordinate.right==32 &&
            coordinate.lifted_direction==502 && coordinate.context==core::CellId{core::Grade{1},0},"pair keeps actual context and operand identities");
        const auto joined=square.admit(b); const auto downstream=joined.input();
        const auto result=execute_relational_inputs(after,{downstream}); const auto fields=result.result();
        const Q q=x[0]+2*x[1]+x[0]*x[1];
        check(fields.field(1).values().dense()==lifted && fields.field(2).values().dense()==V({0,-q,0}),"joint field changes the complete native boundary response");
        check(fields.occurrence_field(2).values().dense()==V({q,-q,-q,0,0}),"repeated inactive and zero flags remain ordered");
        const Q old_norm=numeric::dot(x,original_metric().apply(x)), new_norm=numeric::dot(lifted,square.metric().apply(lifted));
        check(new_norm==old_norm+old_norm*old_norm,"exact direct sum and tensor product metric");
        const auto encoded=encode_relational_known_tensor_square(square);
        const auto cold=decode_relational_known_tensor_square(encoded,{});
        check(encode_relational_known_tensor_square(cold)==encoded && !cold.same_owner(square),"cold complete square replay with distinct owner");
        const auto packet=encode_relational_known_tensor_square_input(joined);
        const auto restored=decode_relational_known_tensor_square_input(packet,{}); const auto restored_input=restored.input();
        const auto restored_accession=restored_input.accession(); const auto restored_parent=restored.square().parent();
        check(restored_accession.asset().same_owner(restored_parent.accession().asset()),"cold upstream and downstream share actual original asset once");
        const auto restored_result=execute_relational_inputs(restored_accession.program(),{restored_input});
        check(encode_accession_field_result(restored_result)==encode_accession_field_result(result),"complete downstream native fields replay before decoding");
        check(encode_relational_known_tensor_square_input(restored)==packet,"known input canonical full ancestry");
        check(restored_input.use()==original.use() && restored_accession.asset().history().first_sequence()==7 &&
            restored_accession.asset().history().last_sequence()==8,"original use and cutoff history retained");
        check(restored_parent.accession().asset().recover(112,10).size()==3,"ordered repeated and empty byte spans retained");
        V old_image(6); old_image[0]=x[0]; old_image[1]=x[1];
        const auto old_result=execute_relational_inputs(before,{original});
        const auto included=after.execute({{1,999,tensor(old_image)}}); const auto old_fields=old_result.result();
        check(included.field(2).values().dense()==old_fields.field(2).values().dense(),"old action preserved on its declared inclusion");
    }
}
void refusals() {
    const auto publication=source(); const auto bytes=asset(publication);
    const auto before=program(publication,false), after=program(publication,true,42);
    const auto a=accession(bytes,before,false), b=accession(bytes,after,true);
    const auto input=a.admit(111,tensor({2,3})); const OwnedRelationalKnownTensorSquare square(input,square_declaration());
    auto d=square_declaration(); d.pair_directions.pop_back();
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,d);},"incomplete pair basis refused");
    d=square_declaration(); d.pair_directions[1]=d.pair_directions[0];
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,d);},"duplicate pair identity refused");
    d=square_declaration(); d.pair_directions[0]=31;
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,d);},"pair and original direction collision refused");
    d=square_declaration(); d.context={core::Grade{0},0};
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,d);},"unrelated local stalk cannot replace input context");
    auto changed=square.lifted_field().values().dense(); changed[3]+=1;
    rejects([&]{square.require_image(tensor(changed));},"off-image joint field refused");
    const T missing(numeric::IndexDomain({6}),numeric::StructuralSupport::sparse(6,{0}),numeric::SparseRationalVector(6,{{0,2}}));
    rejects([&]{square.require_image(missing);},"absence does not become supported zero");
    rejects([&]{square.pair_coordinate(4);},"pair bounds enforced");
    const auto foreign=asset(publication); const auto foreign_accession=accession(foreign,after,true);
    rejects([&]{square.admit(foreign_accession);},"equal raw asset bytes do not identify a foreign owner");
    const auto unchanged=program(publication,true,41); const auto unchanged_accession=accession(bytes,unchanged,true);
    rejects([&]{square.admit(unchanged_accession);},"enlarged carrier cannot silently rewrite old program version");
    auto wrong=b.declaration(); std::swap(wrong.source_stalks[0].directions[2],wrong.source_stalks[0].directions[3]);
    const OwnedRelationalInputAccession reordered(bytes,after,wrong);
    rejects([&]{square.admit(reordered);},"equal dimension with reordered product directions refused");
    wrong=b.declaration(); wrong.isometric=false; wrong.source_stalks[0].metric=M::identity(6);
    const OwnedRelationalInputAccession metric_changed(bytes,after,wrong);
    rejects([&]{square.admit(metric_changed);},"undeclared product metric refused");
    numeric::ExactActionLimits cap; cap.dimension=5;
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,square_declaration(),cap);},"complete joint dimension policy enforced");
    cap={}; cap.entries=5;
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,square_declaration(),cap);},"complete pair declaration resource cap enforced");
    cap={}; cap.integer_bits=3;
    rejects([&]{OwnedRelationalKnownTensorSquare bad(a.admit(111,tensor({7,7})),square_declaration(),cap);},"product intermediate bit growth refused exactly");
    bool stopped{};
    rejects([&]{OwnedRelationalKnownTensorSquare bad(input,square_declaration(),{},[&]{stopped=true;throw std::runtime_error("stop");});},"native checkpoint stops product preparation");
    check(stopped,"actual checkpoint invoked");
    rejects([&]{decode_relational_known_tensor_square(false_square_witness(square),{});},"canonical but false product witness rejected by recomputation");
    const auto joined=square.admit(b); const auto packet=encode_relational_known_tensor_square_input(joined);
    auto buffer=packet; bool cleared{};
    const auto restored=decode_relational_known_tensor_square_input(buffer,{}, {},[&]{if(!cleared){buffer.clear();cleared=true;}});
    check(cleared && encode_relational_known_tensor_square_input(restored)==packet,"decoder freezes original caller buffer before callbacks");
    storage::TemporalReadLimits bytes_cap; bytes_cap.max_file_bytes=packet.size()-1;
    rejects([&]{decode_relational_known_tensor_square_input(packet,bytes_cap);},"cold byte admission enforced before copy");
    const auto other=a.admit(112,tensor({2,3})); const OwnedRelationalKnownTensorSquare other_square(other,square_declaration());
    check(other_square.lifted_field().values().dense()==square.lifted_field().values().dense() &&
        encode_relational_known_tensor_square(other_square)!=encode_relational_known_tensor_square(square),"equal numeric fields retain distinct original-use provenance");
    std::vector<std::future<bool>> readers;
    for(unsigned i=0;i<6;++i) readers.push_back(std::async(std::launch::async,[packet]{
        const auto owned=decode_relational_known_tensor_square_input(packet,{});
        return encode_relational_known_tensor_square_input(owned)==packet;
    }));
    for(auto& reader:readers) check(reader.get(),"six concurrent immutable full ancestry readers");
}
void sparse_and_metric_contracts() {
    constexpr std::size_t n=4692,copies=13,m=n*copies;
    M a(m,n),r(n,m);
    for(std::size_t i=0;i<n;++i) {
        r.set(i,i,1);
        for(std::size_t copy=0;copy<copies;++copy) a.set(copy*n+i,i,1);
    }
    numeric::ExactActionLimits limits; limits.dimension=m; limits.steps=500000;
    numeric::ExactActionGuard guard(limits);
    field_interface_detail::faithful(a,r,guard);
    check(guard.steps()<500000,"4692-direction thirteen-copy accession uses represented sparse work");
    r.set(0,0,2);
    rejects([&]{numeric::ExactActionGuard bad(limits);field_interface_detail::faithful(a,r,bad);},"large sparse false reconstruction remains refused");
    numeric::ExactActionGuard positive_guard(limits);
    field::hodge_frame_detail::positive(M::identity(n),positive_guard);
    check(positive_guard.steps()<500000,"complete strict metric positivity uses sparse certificate");
    M singular(n,n);singular.set(n-1,n-1,1);
    rejects([&]{numeric::ExactActionGuard bad(limits);field::hodge_frame_detail::positive(singular,bad);},"strict metric still refuses sparse singular form");
    const M s=M::from_rows({{1,1},{0,1}}),back=M::from_rows({{1,-1},{0,1}}),metric=original_metric();
    const auto changed=back.transpose().multiply(metric).multiply(back);
    M lift(6,6);
    for(std::size_t i=0;i<2;++i)for(std::size_t j=0;j<2;++j) {
        lift.set(i,j,s.get(i,j));
        for(std::size_t k=0;k<2;++k)for(std::size_t l=0;l<2;++l)
            lift.set(2+2*i+k,2+2*j+l,s.get(i,j)*s.get(k,l));
    }
    check(lift.transpose().multiply(lifted_metric(changed)).multiply(lift)==lifted_metric(metric),
        "complete declared product metric transforms with both operand bases");
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc==2) {
            std::ifstream file(argv[1],std::ios::binary);
            const std::vector<char> chars{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
            std::vector<std::byte> bytes; for(const auto c:chars) bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
            const auto cold=decode_relational_known_tensor_square_input(bytes,{}); const auto input=cold.input();
            const auto result=execute_relational_inputs(input.accession().program(),{input});
            const auto fields=result.result();
            check(fields.field(2).values().dense()==V({0,-14,0}),"standalone owning packet acts on original joint input");
            check(encode_relational_known_tensor_square_input(cold)==bytes,"standalone canonical full ancestry");
        } else {
            corpus(); refusals(); sparse_and_metric_contracts(); const auto publication=source(); const auto bytes=asset(publication);
            const auto before=program(publication,false),after=program(publication,true,42);
            const OwnedRelationalKnownTensorSquare square(accession(bytes,before,false).admit(111,tensor({2,3})),square_declaration());
            const auto packet=encode_relational_known_tensor_square_input(square.admit(accession(bytes,after,true)));
            std::ofstream file("known-square-input.packet",std::ios::binary);
            file.write(reinterpret_cast<const char*>(packet.data()),static_cast<std::streamsize>(packet.size()));
        }
        std::cout<<checks<<" known tensor square checks passed\n"; return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
