#include "../../examples/algebraic_storage/model.hpp"
#include <iostream>

using namespace archive_example; using namespace rfc::storage;
static std::size_t checks{};
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {
    bool failed=false;try {action();} catch (const std::exception&) {failed=true;} check(failed,message);
}
template<class T> concept TemporaryRecords=requires(T value) {std::move(value).contents();};
template<class T> concept TemporaryAction=requires(T value) {std::move(value).rational_action();};
static_assert(!TemporaryRecords<Archive> && !TemporaryAction<Comparison>);
static void rehash(std::vector<std::byte>& bytes) {
    const auto hash=rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));
    for (std::size_t i=0;i<8;++i) bytes[24+i]=std::byte((hash>>(8*i))&255u);
}
static void roundtrip(const Case& sample,bool deformed) {
    const auto original=make(sample,deformed);const auto bytes=encode_algebraic_archive(original);
    const auto fixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/algebraic_v1"/
        (std::string(sample.name)+(deformed?"-deformed":"")+".rfcxa");
    check(read_file_exact(fixture,16u<<20)==bytes,"RFCXA v1 canonical fixture bytes changed");
    const auto restored=decode_algebraic_archive(bytes);check(encode_algebraic_archive(restored)==bytes,"canonical typed archive bytes differ");
    check(encode_rfcx(restored.bundle().source()).bytes==encode_rfcx(original.bundle().source()).bytes,"complete source bytes lost");
    check(!restored.bundle().same_owner(original.bundle()),"archive silently reused bundle owner");
    check(!restored.bundle().field().same_owner(original.bundle().field()),"archive silently reused field owner");
    const auto& oldx=std::get<rfc::local::AlgebraicModuleVector>(original.at(10));
    const auto& x=std::get<rfc::local::AlgebraicModuleVector>(restored.at(10));
    const auto scalar=rfc::numeric::AlgebraicFieldEmbedding::declare(oldx.carrier().field(),x.carrier().field(),x.carrier().field().generator());
    const auto a=identity_comparison(scalar,oldx.carrier(),x.carrier()),r=identity_comparison(scalar.inverse(),x.carrier(),oldx.carrier());
    const auto typed=rfc::local::AlgebraicReconstruction::declare(a,r);
    const auto compared=typed.accession().apply(oldx);check(compared.coordinates()==x.coordinates(),"explicit accession changed coefficients");
    const auto back=typed.reconstruction().apply(compared);check(back.coordinates()==oldx.coordinates(),"explicit reconstruction lost coefficients");
    check(a.rational_action().apply(oldx.rational_coordinates())==compared.rational_coordinates(),"complete scalar restriction disagrees");
    const auto& metric=std::get<Metric>(restored.at(50)); const auto solved=metric.solve(x),residual=metric.apply(solved);
    check(residual.coordinates()==x.coordinates(),"restored exact factor solve residual differs");
    rejects([&]{(void)metric.apply(oldx);},"same values bypassed restored carrier ownership");
    for (const auto& pivot:metric.pivot_certificates()) check(pivot.sign==1 && pivot.interval.lower>0,"restored positivity not certified");
    const auto& covector=std::get<rfc::local::AlgebraicCovector>(restored.at(20));
    check(covector.evaluate(x)==metric.quadrance(x),"restored covector metric relation differs");
    rejects([&]{(void)covector.evaluate(oldx);},"restored covector accepts old carrier");
    const auto& indefinite=std::get<Form>(restored.at(80));rejects([&]{(void)Metric::certify(indefinite);},"indefinite form became positive metric");
    const auto& map=std::get<Map>(restored.at(30));const auto& adjoint=std::get<Map>(restored.at(70));
    const auto recomputed=rfc::field::algebraic_metric_adjoint(map,metric,std::get<Metric>(restored.at(60)));
    check(recomputed.coefficients()==adjoint.coefficients(),"complete restored adjoint differs");
    const auto& source=original.bundle().source();
    check(restored.contents().fibers->chain_policy()==original.contents().fibers->chain_policy(),"declared chain policy lost");
    check(restored.contents().fibers->flat()==!deformed,"full chain defect family was not recomputed");
    for (std::size_t k=1;k<source.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};
        check(original.contents().fibers->rational_boundary(grade)==restored.contents().fibers->rational_boundary(grade),"boundary changed");
        if (k>=2) check(original.contents().fibers->rational_chain_defect(grade)==restored.contents().fibers->rational_chain_defect(grade),"higher chain defect changed");
        for (std::size_t i=0;i<source.grade(grade).incidence_count();++i) {
            const auto& before=original.contents().fibers->occurrence_map(grade,i);const auto& after=restored.contents().fibers->occurrence_map(grade,i);
            check(before.rational_action()==after.rational_action(),"retained occurrence map lost");
        }
    }
    if (deformed) rejects([&]{(void)restored.contents().fibers->homology_dimensions_over_field();},"deformation entered flat homology");
    AlgebraicArchiveContents invalid=original.contents();invalid.records.push_back(invalid.records.front());
    rejects([&]{(void)Archive::declare(original.bundle(),invalid);},"duplicate archive name accepted");
    invalid=original.contents();invalid.records.front().value=x;
    rejects([&]{(void)Archive::declare(original.bundle(),invalid);},"foreign record carrier accepted");
    invalid=original.contents();invalid.metric_domain=restored.contents().metric_domain;
    rejects([&]{(void)Archive::declare(original.bundle(),invalid);},"foreign metric domain accepted");
    for (std::size_t end=0;end<bytes.size();++end)
        rejects([&]{(void)decode_algebraic_archive(std::span<const std::byte>(bytes).first(end));},"truncated image accepted");
    auto corrupt=bytes;corrupt.back()^=std::byte{1};rejects([&]{(void)decode_algebraic_archive(corrupt);},"bad checksum accepted");
    corrupt=bytes;corrupt[8]=std::byte{2};rejects([&]{(void)decode_algebraic_archive(corrupt);},"unknown wire version accepted");
    corrupt=bytes;corrupt[12]=std::byte{1};rejects([&]{(void)decode_algebraic_archive(corrupt);},"unknown wire flags accepted");
    const auto source_size=rfcx_detail::Reader(bytes,32).u64();const auto first_coefficient=40+static_cast<std::size_t>(source_size)+8;
    corrupt=bytes;corrupt[first_coefficient+5]=std::byte{0};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive(corrupt);},"leading zero integer magnitude accepted after rehash");
    corrupt=bytes;for (std::size_t i=0;i<8;++i) corrupt[first_coefficient-8+i]=std::byte{255};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive(corrupt);},"oversized polynomial count allocated");
    corrupt=bytes;const auto denominator=first_coefficient+6;corrupt[denominator+5]=std::byte{0};rehash(corrupt);
    rejects([&]{(void)decode_algebraic_archive(corrupt);},"zero denominator accepted after rehash");
    // Last record is a declared indefinite form. A type label cannot certify it.
    const AlgebraicArchiveLimits policy;
    algebraic_archive_detail::Admission admission(policy);
    algebraic_archive_detail::Writer writer(bytes.size());algebraic_archive_detail::values(writer,indefinite.coefficient_form().coefficients(),admission);
    corrupt=bytes;const auto tag=bytes.size()-8-writer.take().size()-6-1;check(corrupt[tag]==std::byte{3},"form tag fixture position");
    corrupt[tag]=std::byte{4};rehash(corrupt);rejects([&]{(void)decode_algebraic_archive(corrupt);},"forged metric claim accepted");
    AlgebraicArchiveLimits limits;limits.max_file_bytes=bytes.size()-1;
    rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"file byte admission ignored");
    rejects([&]{(void)encode_algebraic_archive(original,limits);},"writer byte admission ignored");
    limits={};limits.max_records=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"record budget ignored");
    limits={};limits.modules.max_cells=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"source cell budget ignored");
    limits={};limits.modules.max_rank=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"rank budget ignored");
    limits={};limits.fibers.max_occurrences=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"occurrence budget ignored");
    limits={};limits.max_rational_coordinates=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"aggregate coordinate budget ignored");
    limits={};limits.field.max_degree=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"field degree budget ignored");
    limits={};limits.field.max_scalar_steps=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"field proof work budget ignored");
    limits={};limits.metric.max_factor_steps=1;rejects([&]{(void)decode_algebraic_archive(bytes,limits);},"factor work budget ignored");
}
static void redundant_reconstruction() {
    const auto f=Field::declare({1,0,1},rfc::numeric::IrreducibleReduction{3});
    const auto source=cellular_example::source(false,Q{1});std::vector<std::vector<std::size_t>> ranks;
    for (std::size_t k=0;k<source.grade_count();++k) ranks.emplace_back(source.grade(G{static_cast<std::uint16_t>(k)}).cell_count(),2);
    ranks[0][1]=3;const auto bundle=rfc::local::AlgebraicStalkBundle::declare(source,f,ranks);
    const auto u=bundle.stalk({G{0},0}),v=bundle.stalk({G{0},1});const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0});
    const auto a=Comparison::from_map(Map::declare(u,v,{one,zero,zero,one,one,one}));
    const auto r=Comparison::from_map(Map::declare(v,u,{one,zero,zero,zero,one,zero}));
    const auto typed=rfc::local::AlgebraicReconstruction::declare(a,r);const auto projector=typed.physical_projector();
    check(projector.rational_action().multiply(projector.rational_action())==projector.rational_action(),"redundant physical projector not idempotent");
    check(projector.rational_action()!=rfc::numeric::ExactMatrix::identity(6),"redundant carrier implicitly inverted");
    const auto operation=Map::declare(u,u,{one,f.generator(),zero,one});const auto physical=typed.physical_operation(operation);
    const auto x=u.vector({one,f.generator()}),ax=a.apply(x),result=physical.apply(ax),ox=operation.apply(x),expected=a.apply(ox);
    check(result.coordinates()==expected.coordinates(),"A O R physical action not faithful");
    AlgebraicArchiveContents contents;
    contents.records={{1,Map::declare(u,v,a.coefficients())},{2,Map::declare(v,u,r.coefficients())},{3,operation}};
    contents.reconstructions={{9,1,2}};
    const auto archive=Archive::declare(bundle,contents),restored=decode_algebraic_archive(encode_algebraic_archive(archive));
    const auto restored_pair=restored.reconstruction(9);const auto restored_projector=restored_pair.physical_projector();
    check(restored_projector.rational_action()==projector.rational_action(),"durable redundant reconstruction changed");
    contents.reconstructions={{9,1,3}};rejects([&]{(void)Archive::declare(bundle,contents);},"ill-typed reconstruction map reference accepted");
    contents.reconstructions={{9,1,99}};rejects([&]{(void)Archive::declare(bundle,contents);},"missing reconstruction map reference accepted");
    const auto incompatible=Comparison::from_map(Map::declare(v,u,{one,zero,one,zero,one,zero}));
    rejects([&]{(void)rfc::local::AlgebraicReconstruction::declare(a,incompatible);},"false left inverse accepted");
    const auto conjugate=rfc::numeric::AlgebraicFieldEmbedding::declare(f,f,-f.generator());
    const auto sigma=Comparison::declare(conjugate,u,u,{one,zero,zero,one});const auto sigma2=sigma.then(sigma);
    const auto conjugated=sigma.apply(x);check(conjugated.rational_coordinates()==sigma.rational_action().apply(x.rational_coordinates()),"semilinear full action differs");
    check(sigma2.rational_action()==rfc::numeric::ExactMatrix::identity(4),"nontrivial field comparison composition differs");
    const auto larger=Field::declare({1,0,0,0,1},rfc::numeric::EisensteinShift{2,1});
    const auto larger_bundle=rfc::local::AlgebraicStalkBundle::declare(source,larger,ranks);const auto w=larger_bundle.stalk(u.cell());
    const auto inclusion=rfc::numeric::AlgebraicFieldEmbedding::declare(f,larger,larger.generator().power(2));
    const auto included=identity_comparison(inclusion,u,w);const auto image=included.apply(x);
    check(image.rational_coordinates()==included.rational_action().apply(x.rational_coordinates()),"proper scalar extension loses target directions");
    check(included.rational_action().rows()==8 && included.rational_action().cols()==4,"proper field inclusion collapsed to equal dimensions");
    rejects([&]{(void)inclusion.inverse();},"proper field inclusion inferred an inverse");
    auto limits=bundle.limits();limits.max_matrix_coordinates=1;
    const auto limited=rfc::local::AlgebraicStalkBundle::declare(source,f,ranks,limits);
    rejects([&]{(void)Comparison::declare(conjugate,limited.stalk(u.cell()),limited.stalk(u.cell()),{one,zero,zero,one});},"comparison matrix budget ignored");
}
static void optional_domains() {
    const auto f=Field::declare({-1,1},rfc::numeric::IrreducibleReduction{3});
    const auto model=cellular_example::make(f,false);AlgebraicArchiveContents contents;
    const auto u=model.bundle.stalk({G{0},0});contents.records={{1,u.vector({f.generator(),f.scalar(Q{0})})}};
    const auto archive=Archive::declare(model.bundle,contents),restored=decode_algebraic_archive(encode_algebraic_archive(archive));
    check(!restored.contents().metric_domain && !restored.contents().embedding && !restored.contents().involution,"missing structures implicitly inferred");
    contents.involution=rfc::numeric::AlgebraicInvolution::identity(f);
    const auto involution_only=Archive::declare(model.bundle,contents);
    const auto restored_involution=decode_algebraic_archive(encode_algebraic_archive(involution_only));
    check(restored_involution.contents().involution.has_value(),"standalone involution lost");
}
int main() {try {const auto samples=cases();for (const auto& sample:samples) roundtrip(sample,false);roundtrip(samples[2],true);
    redundant_reconstruction();optional_domains();std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
