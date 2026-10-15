#include "rfc/database/algebraic_graph_database.hpp"
#include "rfc/database/algebraic_grade_tensor_window.hpp"
#include "rfc/algebra/polygon.hpp"
#include "rfc/core/builder.hpp"
#include <fstream>
#include <iostream>

using namespace rfc::database;
using namespace rfc::storage;
using Q=rfc::numeric::Rational;
using E=rfc::numeric::AlgebraicElement;
using G=rfc::core::Grade;
using Module=rfc::local::AlgebraicModule;
using Vector=rfc::local::AlgebraicModuleVector;
using Bundle=rfc::local::AlgebraicStalkBundle;
using Map=rfc::local::AlgebraicModuleMap;
using Fibers=rfc::local::AlgebraicRelationalFiberSystem;
static std::size_t checks{},profiles{},nonflat{};
static std::optional<std::filesystem::path> export_directory;
static void tensor_json(std::ostream& out,const AlgebraicGradeTensor& tensor) {
    out<<'[';
    for (std::size_t i=0;i<tensor.domain().logical_size();++i) {
        if (i) out<<',';
        out<<'[';const auto coefficient=tensor.coordinate(i);
        for (std::size_t j=0;j<coefficient.coordinates().size();++j) {if (j) out<<',';out<<'"'<<coefficient.coordinates()[j].get_str()<<'"';}
        out<<']';
    }
    out<<']';
}
static void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
template<class F> static void rejects(F f,const char* message) {
    bool rejected=false;try {f();} catch (const std::exception&) {rejected=true;}check(rejected,message);
}
static std::uint64_t key(G grade,std::size_t cell) {return 1000+static_cast<std::uint64_t>(grade.value)*100+cell;}
static AlgebraicArchive populate(const AlgebraicArchive& archive,bool zero=false) {
    auto contents=archive.contents();const auto& bundle=archive.bundle();
    for (std::size_t k=0;k<bundle.source().grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};
        for (std::size_t i=0;i<bundle.source().grade(grade).cell_count();++i) {
            const auto carrier=bundle.stalk({grade,static_cast<std::uint32_t>(i)});
            std::vector<E> values(carrier.rank(),bundle.field().scalar(0));
            if (!zero) values[0]=bundle.field().generator()*bundle.field().scalar(Q{std::to_string(i+1)})+bundle.field().scalar(Q{std::to_string(k)});
            contents.records.push_back({key(grade,i),carrier.vector(std::move(values))});
        }
    }
    return AlgebraicArchive::declare(bundle,std::move(contents));
}
static AlgebraicSnapshot bind(const AlgebraicArchive& archive) {
    AlgebraicArchiveGraphContents contents;contents.fields={{1,archive.bundle().field()}};contents.bundles={{1,archive.bundle()}};
    contents.archives={{9,archive}};const auto& c=archive.contents();
    if (c.involution) contents.involutions={{1,*c.involution}};
    if (c.embedding) contents.embeddings={{1,*c.embedding}};
    if (c.metric_domain) contents.domains={{1,*c.metric_domain}};
    if (c.fibers) contents.fibers={{1,*c.fibers}};
    const auto graph=AlgebraicGraphSnapshot::anchor_only(AlgebraicArchiveGraph::declare(std::move(contents)),1);
    const AlgebraicGraphChannel channel{9631,1,2};
    const NativeTemporalSeries series(with_algebraic_graph_snapshot(TemporalEndpoint(graph.source(),31),channel,graph),9603);
    return AlgebraicSnapshot::select_graph_archive(AlgebraicGraphRevision::capture(series.at(31),channel),9,{9603,31});
}
// Direct K arithmetic over every original occurrence, separate from the native
// rational matrix and target-row execution schedule.
static std::vector<std::vector<E>> reference(const AlgebraicGradeTensor& input) {
    const auto& bundle=input.source().archive().bundle();const auto& fibers=*input.source().archive().contents().fibers;
    const auto lower=G{static_cast<std::uint16_t>(input.grade().value-1u)};const auto& arena=bundle.source().grade(input.grade());
    std::vector<std::vector<E>> result;
    for (std::size_t i=0;i<bundle.source().grade(lower).cell_count();++i)
        result.emplace_back(bundle.stalk({lower,static_cast<std::uint32_t>(i)}).rank(),bundle.field().scalar(0));
    for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
        const auto& map=fibers.occurrence_map(input.grade(),slot);const auto scalar=bundle.field().scalar(bundle.source().incidence_coefficient(input.grade(),slot));
        const auto value=input.stalk_pin({input.grade(),arena.upper_index[slot]});
        for (std::size_t row=0;row<map.target().rank();++row) for (std::size_t column=0;column<map.source().rank();++column)
            result[arena.lower_index[slot]][row]=result[arena.lower_index[slot]][row]+
                scalar*map.coefficients()[row*map.source().rank()+column]*value->coordinates()[column];
    }
    return result;
}
static void profile(const AlgebraicArchive& original,bool zero=false) {
    ++profiles;const auto source=bind(populate(original,zero));const auto& bundle=source.archive().bundle();
    const rfc::runtime::WorkAvailability cutoff{9603,31};
    std::ofstream observations;
    if (export_directory) {
        const auto prefix=*export_directory/("profile-"+std::to_string(profiles));
        write_algebraic_archive(source.archive(),prefix.string()+".rfcxa");
        observations.open(prefix.string()+".jsonl");
        if (!observations) throw std::runtime_error("cannot write independent grade observations");
        observations<<"{\"namespace\":9603,\"sequence\":31,\"archive\":9,\"bundle\":1,\"zero_input\":"<<(zero?"true":"false")<<"}\n";
    }
    for (std::size_t k=0;k<bundle.source().grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};std::vector<std::uint64_t> keys;
        for (std::size_t i=0;i<bundle.source().grade(grade).cell_count();++i) keys.push_back(key(grade,i));
        const auto tensor=AlgebraicGradeTensor::select(source,grade,keys,cutoff);
        if (observations.is_open()) {
            observations<<"{\"grade\":"<<k<<",\"keys\":[";
            for (std::size_t i=0;i<keys.size();++i) {if (i) observations<<',';observations<<keys[i];}
            observations<<"],\"coordinates\":";tensor_json(observations,tensor);
            observations<<",\"shape\":[1,"<<tensor.domain().logical_size()<<"],\"stalk_coordinates\":[";
            for (std::size_t i=0;i<tensor.domain().logical_size();++i) {
                if (i) observations<<',';
                const auto coordinate=tensor.source_coordinate(i);
                observations<<'['<<coordinate.cell.grade.value<<','<<coordinate.cell.index<<','<<coordinate.coordinate<<']';
            }
            observations<<']';
        }
        check(tensor.source().same_publication(source) && tensor.source().graph_origin()->revision.same_owner(source.graph_origin()->revision),"grade field lost selected graph endpoint");
        check(tensor.cell_count()==keys.size() && tensor.record_keys()==keys && !tensor.derived() && !tensor.input(),"full explicit field selection differs");
        const auto original_keys=keys;keys.clear();check(tensor.record_keys()==original_keys,"caller mutation changed retained field binding");
        check(tensor.stalk_offsets()==bundle.offsets(grade),"heterogeneous stalk offsets differ");
        for (std::size_t i=0;i<tensor.domain().logical_size();++i) {
            const auto coordinate=tensor.source_coordinate(i);const auto pin=tensor.stalk_pin(coordinate.cell);
            check(tensor.source_index(coordinate)==i,"stalk coordinate is not reversible");
            check(pin->carrier().same_owner(bundle.stalk(coordinate.cell)) && tensor.coordinate(i)==pin->coordinates()[coordinate.coordinate],"tensor changed actual carrier or full field tuple");
        }
        const auto shape=rfc::numeric::IndexDomain({1,tensor.domain().logical_size()});
        const auto window=AlgebraicGradeTensorWindow::prepare(tensor,shape,{0,0},shape.extents(),cutoff);
        check(window.supported_count()==tensor.domain().logical_size() && window.tensor().same_owner(tensor),"full shape window dropped support or reowned tensor");
        auto sum=bundle.field().scalar(0);
        for (std::size_t i=0;i<window.supported_count();++i) {
            check(window.source_index(i)==i && window.window_index(i)==i && window.source_coordinate(i)==tensor.source_coordinate(i),"shape failed exact coordinate realization");
            sum=sum+tensor.coordinate(i);
        }
        if (window.supported_count()) check(window.sum(cutoff)==sum,"complete coefficient contraction differs");
        else check(!window.sum(cutoff),"empty field became supported zero");
        const auto empty=AlgebraicGradeTensorWindow::prepare(tensor,shape,{0,0},{0,0},cutoff);
        check(empty.supported_count()==0 && !empty.sum(cutoff),"empty box became a supported zero sum");
        rejects([&]{(void)empty.coordinate(0);},"empty box exposed a coordinate");
        rejects([&]{(void)tensor.coordinate(tensor.domain().logical_size());},"full field extent ignored");
        rejects([&]{(void)AlgebraicGradeTensorWindow::prepare(tensor,rfc::numeric::IndexDomain({tensor.domain().logical_size()+1}),{0},{0},cutoff);},"shape omitted complete source coordinates");
        rejects([&]{(void)AlgebraicGradeTensorWindow::prepare(tensor,shape,{0},shape.extents(),cutoff);},"window rank mismatch ignored");
        rejects([&]{(void)window.sum({9603,30});},"window future cutoff accepted");
        if (tensor.domain().logical_size()) {
            const auto first=AlgebraicGradeTensorWindow::prepare(tensor,shape,{0,0},{1,1},cutoff);
            check(first.supported_count()==1 && first.window_index(0)==0 && first.coordinate(0)==tensor.coordinate(0),"partial box changed stalk coordinate");
            if (tensor.domain().logical_size()>1) check(!first.window_index(1),"outside box returned supported coordinate");
        }
        if (k && source.archive().contents().fibers) {
            const auto result=tensor.boundary(cutoff);const auto expected=reference(tensor);
            if (observations.is_open()) {observations<<",\"boundary\":";tensor_json(observations,result);}
            check(result.derived() && result.record_keys().empty() && result.input()->same_owner(tensor),"boundary lost input provenance or invented record keys");
            check(result.source().same_publication(source) && result.grade().value==k-1 && result.cell_count()==expected.size(),"boundary changed original complex or output grade");
            for (std::size_t i=0;i<expected.size();++i) {
                const rfc::core::CellId cell{result.grade(),static_cast<std::uint32_t>(i)};
                check(result.stalk_pin(cell)->carrier().same_owner(bundle.stalk(cell)) && result.stalk_pin(cell)->coordinates()==expected[i],"native boundary differs from full original K occurrence contraction");
            }
            if (k>=2) {
                const auto twice=result.boundary(cutoff);const auto expected_twice=reference(result);bool all_zero=true;
                if (observations.is_open()) {observations<<",\"twice\":";tensor_json(observations,twice);}
                for (std::size_t i=0;i<expected_twice.size();++i) {
                    const auto pin=twice.stalk_pin({twice.grade(),static_cast<std::uint32_t>(i)});
                    check(pin->coordinates()==expected_twice[i],"adjacent contraction lost a route or changed its order");
                    for (const auto& coefficient:pin->coordinates()) all_zero=all_zero && coefficient.is_zero();
                }
                if (source.archive().contents().fibers->flat()) check(all_zero,"flat full chain family failed");
                else if (!all_zero) ++nonflat;
                check(twice.input()->input()->same_owner(tensor),"two-step tensor history lost original input");
            }
            rejects([&]{(void)tensor.boundary({9603,30});},"boundary used a future source");
            rfc::runtime::ExecutionHostPolicy policy;policy.limits.rational_slots=0;
            rejects([&]{(void)tensor.boundary(cutoff,rfc::runtime::ExecutionLease(policy));},"boundary temporary admission ignored");
            policy={};policy.limits.integer_bits=0;
            rejects([&]{(void)tensor.boundary(cutoff,rfc::runtime::ExecutionLease(policy));},"boundary exact integer admission ignored");
        } else if (k) rejects([&]{(void)tensor.boundary(cutoff);},"archive without declared fibers gained a boundary");
        else rejects([&]{(void)tensor.boundary(cutoff);},"grade zero gained an undeclared lower grade");
        if (observations.is_open()) observations<<"}\n";
        if (tensor.cell_count()) {
            auto missing=original_keys;missing.pop_back();rejects([&]{(void)AlgebraicGradeTensor::select(source,grade,missing,cutoff);},"missing cell became implicit zero");
            auto absent=original_keys;absent[0]=0;rejects([&]{(void)AlgebraicGradeTensor::select(source,grade,absent,cutoff);},"absent record became supported zero");
            if (tensor.cell_count()>1) {auto duplicate=original_keys;duplicate[1]=duplicate[0];rejects([&]{(void)AlgebraicGradeTensor::select(source,grade,duplicate,cutoff);},"equal ranks identified different source cells");}
            rejects([&]{(void)tensor.stalk_pin({G{static_cast<std::uint16_t>(k+1)},0});},"tensor ignored original grade");
            rejects([&]{(void)tensor.source_index({{grade,0},bundle.stalk({grade,0}).rank()});},"tensor ignored local stalk rank");
        }
        auto cancelled=std::make_shared<std::atomic_bool>(true);const rfc::runtime::ExecutionLease stopped({},cancelled);
        rejects([&]{(void)AlgebraicGradeTensor::select(source,grade,original_keys,cutoff,stopped);},"tensor selection ignored cancellation");
        if (k) rejects([&]{(void)tensor.boundary(cutoff,stopped);},"boundary ignored cancellation");
        rejects([&]{(void)window.sum(cutoff,stopped);},"window contraction ignored cancellation");
        rfc::runtime::ExecutionHostPolicy policy;policy.limits.records=0;
        rejects([&]{(void)AlgebraicGradeTensor::select(source,grade,original_keys,cutoff,rfc::runtime::ExecutionLease(policy));},"tensor pin admission ignored");
        policy={};policy.backend=rfc::runtime::ExecutionBackend::ExactGpu;
        rejects([&]{(void)window.sum(cutoff,rfc::runtime::ExecutionLease(policy));},"unregistered tensor GPU accepted");
        auto moved=tensor;const auto owned=std::move(moved);rejects([&]{(void)moved.grade();},"moved tensor accessed missing state");
        check(owned.same_owner(tensor),"moved tensor owner changed");
    }
}
static void heterogeneous_and_empty() {
    const auto field=rfc::numeric::CertifiedAlgebraicField::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto complex=rfc::algebra::oriented_polygon({10,20,30},rfc::algebra::PolygonFilling::Disk);
    const auto bundle=Bundle::declare(complex,field,{{3,2,2},{2,2,2},{1}});std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
    for (std::size_t k=1;k<complex.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};const auto& arena=complex.grade(grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            const auto upper=bundle.stalk({grade,arena.upper_index[slot]}),lower=bundle.stalk({G{static_cast<std::uint16_t>(k-1)},arena.lower_index[slot]});
            std::vector<E> coefficients(upper.rank()*lower.rank(),field.scalar(0));
            for (std::size_t i=0;i<std::min(upper.rank(),lower.rank());++i) coefficients[i*upper.rank()+i]=field.scalar(1);
            maps.push_back({grade,slot,Map::declare(upper,lower,std::move(coefficients))});
        }
    }
    AlgebraicArchiveContents c;c.fibers=Fibers::declare(bundle,maps,rfc::local::AlgebraicChainPolicy::RequireFlat);
    profile(AlgebraicArchive::declare(bundle,c));profile(AlgebraicArchive::declare(bundle,c),true);
    const auto escaped=[&] {
        const auto source=bind(populate(AlgebraicArchive::declare(bundle,c)));
        const auto value=AlgebraicGradeTensor::select(source,G{2},{1200},{9603,31});
        return value.boundary({9603,31}).boundary({9603,31});
    }();
    check(escaped.source().source_publication()->sequence==31 && escaped.input()->input()->record_keys()==std::vector<std::uint64_t>{1200},"escaped grade field lost original endpoint/input");
    for (std::size_t i=0;i<escaped.domain().logical_size();++i) check(escaped.coordinate(i).is_zero(),"escaped flat contraction changed after owner release");
    const auto no_fibers=bind(populate(AlgebraicArchive::declare(bundle),true));
    const auto zero=AlgebraicGradeTensor::select(no_fibers,G{1},{1100,1101,1102},{9603,31});
    rejects([&]{(void)zero.boundary({9603,31});},"missing maps accepted by a boundary shortcut");
    rfc::core::RelationalComplexBuilder builder(rfc::core::RelationSignature{});builder.declare_grade(G{2});const auto empty=builder.freeze();
    const auto empty_bundle=Bundle::declare(empty,field,{{},{},{}});
    AlgebraicArchiveContents ec;ec.fibers=Fibers::declare(empty_bundle,{},rfc::local::AlgebraicChainPolicy::RequireFlat);
    profile(AlgebraicArchive::declare(empty_bundle,std::move(ec)));
}
int main(int argc,char** argv) {try {
    if (argc!=2 && argc!=3) throw std::invalid_argument("algebraic fixture directory and optional new observation directory required");
    if (argc==3) {
        export_directory=std::filesystem::path(argv[2]);
        if (!std::filesystem::create_directory(*export_directory)) throw std::invalid_argument("observation directory must be new");
    }
    for (const auto* name:{"real_cubic","hermitian_quadratic","hermitian_quartic","hermitian_octic","hermitian_quartic-deformed","redundant-reconstruction"}) {
        const auto archive=read_algebraic_archive(std::filesystem::path(argv[1])/(std::string(name)+".rfcxa"));profile(archive);profile(archive,true);
    }
    heterogeneous_and_empty();check(nonflat>0,"declared higher deformation was hidden by a flat-only observation");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"profiles\":"<<profiles<<",\"nonflat_observations\":"<<nonflat<<"}\n";
} catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
