#include "../../examples/algebraic_graph/model.hpp"
#include <iostream>

using namespace algebraic_graph_example;
using namespace rfc::storage;
static std::size_t checks{};
static void check(bool condition,const char* message) {++checks;if (!condition) throw std::runtime_error(message);}
template<class Function> static void rejects(Function action,const char* message) {
    bool failed=false;try {action();} catch (const std::exception&) {failed=true;}check(failed,message);
}
template<class T> concept TemporaryContents=requires(T x){std::move(x).contents();};
template<class T> concept TemporaryCarrier=requires(T x){std::move(x).carrier(1);};
template<class T> concept TemporarySource=requires(T x){std::move(x).source();};
static_assert(!TemporaryContents<Graph> && !TemporaryCarrier<Graph> && !TemporarySource<DC>);
static void rehash(std::vector<std::byte>& bytes) {
    const auto hash=rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for (std::size_t i=0;i<8;++i) bytes[24+i]=std::byte((hash>>(8*i))&255u);
}
static std::size_t find_block(const std::vector<std::byte>& bytes,const std::vector<std::byte>& block) {
    const auto it=std::search(bytes.begin(),bytes.end(),block.begin(),block.end());
    check(it!=bytes.end(),"wire test block absent");
    check(std::search(it+1,bytes.end(),block.begin(),block.end())==bytes.end(),"wire test block ambiguous");
    return static_cast<std::size_t>(it-bytes.begin());
}
static void owners_and_complete_maps(const Graph& before,const Graph& graph) {
    check(!before.same_owner(graph),"decoded graph reused original owner");check(graph.same_owner(graph),"copy lost graph owner");
    check(!graph.field(1).same_owner(graph.field(4)),"independent equal fields were interned");
    check(graph.field(1).minimal_polynomial()==graph.field(4).minimal_polynomial(),"equal field presentations changed");
    check(!graph.bundle(2).same_owner(graph.bundle(3)),"independent equal bundles were interned");
    check(graph.bundle(2).field().same_owner(graph.bundle(3).field()),"declared field sharing lost");
    check(graph.archive(1).bundle().same_owner(graph.archive(4).bundle()),"declared bundle sharing lost");
    check(!graph.archive(1).same_owner(graph.archive(4)),"independent equal archives were interned");
    check(graph.archive(1).contents().fibers->same_owner(*graph.archive(4).contents().fibers),"declared fiber sharing lost");
    check(graph.archive(2).contents().metric_domain->same_owner(*graph.archive(3).contents().metric_domain),"declared metric sharing lost");
    const auto& c=graph.contents();
    for (const auto& row:c.archives) {
        check(encode_algebraic_archive(row.value)==encode_algebraic_archive(before.archive(row.key)),"archive declaration bytes changed in graph");
        check(row.value.bundle().same_owner(graph.bundle(algebraic_graph_detail::owner_key(c.bundles,row.value.bundle()))),"archive bundle reference lost");
        check(!row.value.bundle().same_owner(before.archive(row.key).bundle()),"decoded carrier reused original owner");
    }
    for (const auto& row:c.domains) {
        check(row.value.involution().same_owner(algebraic_graph_detail::at(c.involutions,row.key)),"domain involution reference lost");
        check(algebraic_graph_detail::same_owner(row.value.embedding(),algebraic_graph_detail::at(c.embeddings,row.key)),"domain selected-root reference lost");
    }
    for (const auto& row:c.carriers) {
        check(row.value.rational_action()==before.carrier(row.key).rational_action(),"complete carrier action changed");
        check(row.value.coefficients().size()==row.value.source().rank()*row.value.target().rank(),"target directions were omitted");
        const auto& domain=row.value.source();const auto d=domain.field().degree();
        for (std::size_t j=0;j<domain.rank();++j) for (std::size_t k=0;k<d;++k) {
            std::vector<E> values(domain.rank(),domain.field().scalar(0));
            rfc::numeric::RationalVector coefficient(d);coefficient[k]=1;values[j]=domain.field().element(coefficient);
            const auto x=domain.vector(std::move(values)),y=row.value.apply(x);
            check(row.value.rational_action().apply(x.rational_coordinates())==y.rational_coordinates(),"semilinear restriction lost a complete basis direction");
        }
    }
    const auto& x=std::get<rfc::local::AlgebraicModuleVector>(graph.archive(1).at(10));
    const auto complete=graph.carrier(1).then(graph.carrier(2));
    check(complete.rational_action()==graph.carrier(2).rational_action().multiply(graph.carrier(1).rational_action()),"semilinear composition order differs");
    const auto a=graph.carrier(1).apply(x),b=graph.carrier(2).apply(a),ab=complete.apply(x);
    check(b.coordinates()==ab.coordinates(),"complete comparison composition differs");
    rejects([&]{(void)graph.scalars(1).inverse();},"proper field inclusion inferred an inverse");
    rejects([&]{(void)graph.carrier(1).apply(std::get<rfc::local::AlgebraicModuleVector>(before.archive(1).at(10)));},"equal source bytes bypassed actual carriers");
    const auto projector=graph.reconstruction(1).physical_projector();
    check(projector.rational_action().multiply(projector.rational_action())==projector.rational_action(),"explicit reconstruction projector not idempotent");
    check(projector.rational_action()!=rfc::numeric::ExactMatrix::identity(projector.rational_action().rows()),"redundant carrier claimed a two-sided inverse");
    for (const auto& row:c.reconstructions) {
        const auto& r=graph.reconstruction(row.key);const auto n=r.accession().source().rank()*r.accession().source().field().degree();
        const auto identity=r.accession().then(r.reconstruction());
        check(identity.rational_action()==rfc::numeric::ExactMatrix::identity(n),"R A full identity lost");
    }
    for (const auto& row:c.measurements) {
        const auto& proof=graph.measurement(row.key);const auto& u=std::get<rfc::local::AlgebraicModuleVector>(graph.archive(row.source_archive).at(10));
        const auto v=proof.carrier().apply(u);
        check(proof.target().quadrance(v)==proof.domains().scalar_comparison().apply(proof.source().quadrance(u)),"certified measurement transport differs");
    }
    // The sample has zero second coordinate. Changing its complete future action
    // leaves that sample unchanged but must not identify the operations.
    auto coefficients=graph.carrier(1).coefficients();coefficients.back()=graph.field(2).scalar(0);
    const auto changed=C::declare(graph.scalars(1),graph.carrier(1).source(),graph.carrier(1).target(),coefficients);
    const auto same_value=changed.apply(x);
    check(same_value.coordinates()==a.coordinates(),"zero-coordinate control sample differs");
    check(changed.rational_action()!=graph.carrier(1).rational_action(),"new complete target directions were suppressed");
    auto add=c;add.carriers.push_back({8,changed});const auto retained=Graph::declare(add);
    const auto reread=decode_algebraic_archive_graph(encode_algebraic_archive_graph(retained));
    check(!reread.carrier(1).same_owner(reread.carrier(8)),"equal sample output interned distinct comparisons");
    check(reread.carrier(8).rational_action()==changed.rational_action(),"non-sample target direction lost in wire");
    rejects([&]{(void)rfc::field::AlgebraicMeasurementComparison::declare(graph.measurement(1).domains(),changed,
        graph.measurement(1).source(),graph.measurement(1).target());},"sample equality stood in for a full form comparison");
    for (const auto& row:c.fibers) {
        const auto& arena=row.value.bundle().source().grade(G{1});check(arena.incidence_count()==4,"repeated or inactive source occurrences lost");
        for (std::size_t i=0;i<arena.incidence_count();++i)
            check(row.value.occurrence_map(G{1},i).coefficients().size()==row.value.bundle().stalk({G{1},0}).rank()*row.value.bundle().stalk({G{0},0}).rank(),"complete retained occurrence map lost");
    }
}
static void rejection_and_roots(const Graph& graph) {
    auto c=graph.contents();c.fields.push_back({1,c.fields.front().value});rejects([&]{(void)Graph::declare(c);},"duplicate graph name accepted");
    c=graph.contents();c.fields.push_back({9,c.fields.front().value});rejects([&]{(void)Graph::declare(c);},"one owner registered under two implicit alias names");
    c=graph.contents();c.fields.pop_back();rejects([&]{(void)Graph::declare(c);},"unregistered field owner accepted");
    c=graph.contents();c.bundles.erase(c.bundles.begin());rejects([&]{(void)Graph::declare(c);},"unregistered bundle owner accepted");
    c=graph.contents();c.scalars.pop_back();rejects([&]{(void)Graph::declare(c);},"unregistered scalar comparison accepted");
    c=graph.contents();c.measurements.front().source_record=10;rejects([&]{(void)Graph::declare(c);},"vector used as a measurement form");
    c=graph.contents();c.measurements.front().target_archive=3;rejects([&]{(void)Graph::declare(c);},"equal target presentation bypassed carrier owner");
    c=graph.contents();c.reconstructions.front().reconstruction=4;rejects([&]{(void)Graph::declare(c);},"non-reconstructing pair accepted");
    AlgebraicArchiveGraphLimits limits;limits.max_owner_records=1;rejects([&]{(void)Graph::declare(graph.contents(),limits);},"graph owner admission ignored");
    limits={};limits.measurement.max_coefficient_products=0;rejects([&]{(void)Graph::declare(graph.contents(),limits);},"measurement product admission ignored");
    const auto& f=graph.field(1);const auto inv=rfc::numeric::AlgebraicInvolution::declare(f,-f.generator());
    const auto negative=D::hermitian(rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(f,{0,-1},Q{1,4}),inv);
    rejects([&]{(void)DC::declare(graph.scalars(1),negative,graph.measurement(1).domains().target());},"opposite selected complex root transported measurements");
    const auto neg_image=S::declare(f,graph.field(2),-graph.scalars(1).generator_image());
    rejects([&]{(void)DC::declare(neg_image,graph.measurement(1).domains().source(),graph.measurement(1).domains().target());},"algebraic automorphism bypassed selected root compatibility");
    const auto wrong_real=D::real(rfc::geometry::SelectedRealAlgebraicEmbedding::isolate(graph.field(3),Q{-3,2},-1),
        rfc::numeric::AlgebraicInvolution::identity(graph.field(3)));
    rejects([&]{(void)DC::declare(graph.scalars(5),wrong_real,graph.measurement(3).domains().target());},"opposite real root transported to the complex domain");
    // Q(2^(1/4)) admits a real selection with identity involution and an
    // imaginary selection with alpha -> -alpha. Identity scalars do not intertwine.
    const auto field=F::declare({-2,0,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto real=D::real(rfc::geometry::SelectedRealAlgebraicEmbedding::isolate(field,1,Q{5,4}),rfc::numeric::AlgebraicInvolution::identity(field));
    const auto complex=D::hermitian(rfc::geometry::SelectedComplexAlgebraicEmbedding::isolate(field,{0,Q{19,16}},Q{1,16}),
        rfc::numeric::AlgebraicInvolution::declare(field,-field.generator()));
    rejects([&]{(void)DC::declare(S::identity(field),real,complex);},"different metric involutions transported along identity");
    const auto real_identity=S::identity(graph.field(3));
    const auto& real_domain=graph.measurement(3).domains().source();const auto compatible=DC::declare(real_identity,real_domain,real_domain);
    check(compatible.source().same_owner(real_domain),"real selected comparison rejected its own owner");
    c=graph.contents();c.fields.push_back({9,F::declare({1,0,1},rfc::numeric::IrreducibleReduction{3})});
    const auto independent=decode_algebraic_archive_graph(encode_algebraic_archive_graph(Graph::declare(c)));
    check(!independent.field(1).same_owner(independent.field(9)),"unused independently declared field was collapsed");
    c=graph.contents();const auto& d=graph.measurement(1).domains().source();
    c.domains.push_back({9,D::hermitian(std::get<rfc::geometry::SelectedComplexAlgebraicEmbedding>(d.embedding()),d.involution())});
    const auto repeated=decode_algebraic_archive_graph(encode_algebraic_archive_graph(Graph::declare(c)));
    check(!algebraic_graph_detail::at(repeated.contents().domains,1).same_owner(algebraic_graph_detail::at(repeated.contents().domains,9)),"independent equal metric domains interned");
}
static void wire_rejections(const Graph& graph,const std::vector<std::byte>& bytes) {
    for (const std::size_t offset:{0u,8u,10u,12u,16u,24u}) {
        auto corrupt=bytes;corrupt[offset]^=std::byte{1};rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"invalid graph envelope accepted");
    }
    for (std::size_t i=0;i<32;++i) {
        rejects([&]{(void)decode_algebraic_archive_graph(std::span<const std::byte>(bytes).first(i));},"truncated envelope accepted");
        auto corrupt=bytes;corrupt.resize(bytes.size()-i-1);rehash(corrupt);
        rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"truncated graph accepted");
    }
    // Repair length/checksum so truncation reaches actual typed section readers.
    for (std::size_t i=32;i<bytes.size();i+=113) {
        auto corrupt=std::vector<std::byte>(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(i));
        for (std::size_t k=0;k<8;++k) corrupt[16+k]=std::byte((i>>(8*k))&255u);
        rehash(corrupt);rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"typed truncated graph accepted");
    }
    auto corrupt=bytes;corrupt.push_back(std::byte{0});const auto size=corrupt.size();
    for (std::size_t k=0;k<8;++k) corrupt[16+k]=std::byte((size>>(8*k))&255u);
    rehash(corrupt);rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"trailing graph data accepted");
    corrupt=bytes;for (std::size_t k=0;k<8;++k) corrupt[32+k]=std::byte{255};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"oversized owner count allocated");
    corrupt=bytes;corrupt[56+5]=std::byte{0};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"noncanonical integer magnitude accepted");
    corrupt=bytes;corrupt[68]=std::byte{1};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"negative zero rational accepted");
    const AlgebraicArchiveLimits policy;algebraic_archive_detail::Admission admission(policy);
    algebraic_archive_detail::Writer writer(bytes.size());
    algebraic_graph_codec_detail::write_table(writer,graph.contents().scalars,[&](const auto& scalar) {
        writer.u64(algebraic_graph_detail::owner_key(graph.contents().fields,scalar.source()));
        writer.u64(algebraic_graph_detail::owner_key(graph.contents().fields,scalar.target()));
        algebraic_archive_detail::element(writer,scalar.generator_image(),admission);
    });
    const auto position=find_block(bytes,writer.take());
    corrupt=bytes;corrupt[position+16]=std::byte{99};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"missing field reference accepted");
    corrupt=bytes;corrupt[position+32+22]=std::byte{1};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"valid algebraic image with incompatible root accepted as metric transport");
    const auto& carrier=graph.carrier(1);algebraic_archive_detail::Writer coefficients(bytes.size());
    algebraic_archive_detail::values(coefficients,carrier.coefficients(),admission);
    const auto coefficient_position=find_block(bytes,coefficients.take());
    algebraic_archive_detail::Writer prefix(bytes.size());
    for (std::size_t i=0;i+1<carrier.coefficients().size();++i) algebraic_archive_detail::element(prefix,carrier.coefficients()[i],admission);
    corrupt=bytes;corrupt[coefficient_position+prefix.take().size()+5]=std::byte{3};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive_graph(corrupt);},"forged full measurement preservation accepted");
    AlgebraicArchiveGraphLimits limits;limits.max_rational_coordinates=1;
    rejects([&]{(void)decode_algebraic_archive_graph(bytes,limits);},"aggregate rational decode admission ignored");
    rejects([&]{(void)encode_algebraic_archive_graph(graph,limits);},"aggregate rational encode admission ignored");
    limits={};limits.max_edges=1;rejects([&]{(void)decode_algebraic_archive_graph(bytes,limits);},"aggregate edge admission ignored");
    limits={};limits.max_file_bytes=bytes.size()-1;rejects([&]{(void)encode_algebraic_archive_graph(graph,limits);},"graph byte encode admission ignored");
    rejects([&]{(void)decode_algebraic_archive_graph(bytes,limits);},"graph byte decode admission ignored");
}
int main() {
    try {
        const auto original=make();const auto bytes=encode_algebraic_archive_graph(original);const auto graph=decode_algebraic_archive_graph(bytes);
        const auto fixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/algebraic_graph_v1/comparisons.rfcxg";
        check(read_file_exact(fixture,64u<<20)==bytes,"RFCXG v1 canonical fixture bytes changed");
        check(encode_algebraic_archive_graph(graph)==bytes,"canonical owner graph bytes differ");
        owners_and_complete_maps(original,graph);rejection_and_roots(graph);wire_rejections(graph,bytes);
        const auto second=decode_algebraic_archive_graph(bytes);check(!second.field(1).same_owner(graph.field(1)),"separate graph decodes interned fields");
        const auto held=[&] {const auto temporary=decode_algebraic_archive_graph(bytes);
            return std::pair{temporary.carrier(1),std::get<rfc::local::AlgebraicModuleVector>(temporary.archive(1).at(10))};}();
        const auto held_value=held.first.apply(held.second);
        check(held_value.carrier().same_owner(held.first.target()),"comparison did not retain graph carriers after release");
        const auto empty=Graph::declare({});const auto empty_bytes=encode_algebraic_archive_graph(empty);
        check(encode_algebraic_archive_graph(decode_algebraic_archive_graph(empty_bytes))==empty_bytes,"empty graph canonical roundtrip failed");
        AlgebraicArchiveGraphContents linear;linear.fields.push_back({1,F::declare({-2,1},rfc::numeric::IrreducibleReduction{3})});
        const auto linear_graph=Graph::declare(linear);const auto linear_bytes=encode_algebraic_archive_graph(linear_graph);
        check(encode_algebraic_archive_graph(decode_algebraic_archive_graph(linear_bytes))==linear_bytes,"degree-one field admission failed");
        std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"graph_bytes\":"<<bytes.size()<<"}\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
