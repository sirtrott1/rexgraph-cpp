#include "rfc/learning/relational_four_use_moment_basis.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Matrix=numeric::ExactMatrix;
namespace {
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected{};try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
OwnedRelationalFieldProgram parent_program(const OwnedRelationalInputUseAsset& asset,numeric::ExactActionLimits limits) {
    RelationalFieldProgramDeclaration d;d.key=20000;d.revision=1;d.differential=true;
    for(std::uint32_t word=0;word<4;++word) {
        std::vector<std::uint64_t> dirs;for(std::uint64_t i=1;i<=17;++i)dirs.push_back(i);
        d.stalks.push_back({{{0},word},701,std::move(dirs),Matrix::identity(17)});
        Matrix map(17,68);for(std::size_t i=0;i<17;++i)map.set(i,word*17+i,1);
        d.occurrences.push_back({{1},word,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    std::vector<std::uint64_t> dirs;for(std::uint64_t i=1000;i<1068;++i)dirs.push_back(i);
    d.stalks.push_back({{{1},0},8000,std::move(dirs),Matrix::identity(68)});
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {asset.publication(),std::move(d),limits};
}
OwnedRelationalFieldProgram model_program(const storage::NativeTemporalSeries& model,numeric::ExactActionLimits limits) {
    RelationalFieldProgramDeclaration d;d.key=20001;d.revision=1;d.differential=true;
    for(std::uint32_t word=0;word<4;++word) {
        std::vector<std::uint64_t> dirs;for(std::uint64_t i=1;i<=17;++i)dirs.push_back(i);
        for(std::uint64_t head=0;head<4;++head)if(head!=word)for(std::uint64_t role=0;role<2;++role)
            dirs.push_back(10000+100*head+10*word+role);
        d.stalks.push_back({{{0},word},90000+word,std::move(dirs),Matrix::identity(23)});
        Matrix map(23,4692);for(std::size_t i=0;i<17;++i)map.set(i,word*17+i,1);
        d.occurrences.push_back({{1},word,storage::TemporalTensorRole::CoefficientIndependentMap,std::move(map)});
    }
    std::vector<std::uint64_t> dirs;for(std::uint64_t i=1000;i<1068;++i)dirs.push_back(i);
    for(std::uint64_t i=2000;i<6624;++i)dirs.push_back(i);
    for(std::uint32_t context=0;context<13;++context)
        d.stalks.push_back({{{1},context},9000,dirs,Matrix::identity(4692)});
    // Declared reference zero maps are construction controls, not fitted role predictions.
    for(std::size_t occurrence=4;occurrence<28;++occurrence)
        d.occurrences.push_back({{1},occurrence,storage::TemporalTensorRole::CoefficientIndependentMap,Matrix(23,4692)});
    d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};
    return {model.at(model.last_sequence()),std::move(d),limits};
}
RelationalInputAccessionDeclaration declaration(const OwnedRelationalFieldProgram& program,bool lifted) {
    RelationalInputAccessionDeclaration d;d.key=lifted?40001:40000;d.node=1;d.source_grade={static_cast<std::uint16_t>(lifted?1:0)};
    d.isometric=!lifted;d.instances.cells={{0,1,2,3},{0}};d.instances.occurrences={{},{0,1,2,3}};
    if(lifted) d.source_stalks={program.declaration().stalks.at(4)};
    else for(std::size_t i=0;i<4;++i)d.source_stalks.push_back(program.declaration().stalks[i]);
    if(lifted) {
        d.accession=Matrix(60996,4692);d.reconstruction=Matrix(4692,60996);
        for(std::size_t i=0;i<4692;++i) {
            d.reconstruction.set(i,i,1);for(std::size_t context=0;context<13;++context)d.accession.set(4692*context+i,i,1);
        }
    } else {d.accession=Matrix::identity(68);d.reconstruction=d.accession;}
    return d;
}

storage::NativeTemporalSeries history(std::uint64_t ns,bool model,bool nonunit=false){
    core::RelationSignature signature;const auto group=signature.add_type("context",core::BoundaryPolicy::DeclaredExact,4,4);
    const auto pair=signature.add_type("ordered-pair",core::BoundaryPolicy::DeclaredExact,2,2);core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{100},group,{{1,true,false,1,{},core::OccurrenceKey{1}},{2,true,false,1,{},core::OccurrenceKey{2}},
        {3,true,false,1,{},core::OccurrenceKey{3}},{4,true,false,1,{},core::OccurrenceKey{4}}});
    if(model){std::uint64_t ordinal{};for(std::uint64_t h=0;h<4;++h)for(std::uint64_t d=0;d<4;++d)if(h!=d){
        b.add_grade_one(core::RelationKey{200+ordinal},pair,{{h+1,true,false,1,{},core::OccurrenceKey{5+2*ordinal}},
            {d+1,true,false,nonunit?2:1,{},core::OccurrenceKey{6+2*ordinal}}});++ordinal;}}
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(b.freeze(),1),ns);
}
OwnedRelationalInputUseAsset asset(const storage::NativeTemporalSeries& series){
    RelationalInputUseDeclaration d;d.key=70;d.channel=700;d.encoding="UTF-8";for(const char c:std::string("four"))d.bytes.push_back(static_cast<std::byte>(c));
    const auto p=series.at(1);RelationalSpanUse use{100,{{{1},100}},{},{{0,4}}};
    for(std::size_t i=0;i<4;++i)use.occurrences.push_back({{1},p.endpoint().source.occurrence_identity({1},i)});
    d.uses={use};return {series,d};
}
void run(){const numeric::ExactActionLimits limits{70000,16000000,64000000,262144};
    const auto source=history(601,false),model_source=history(602,true),publisher=history(603,false);
    const auto a=asset(source);const auto parent=parent_program(a,limits),model=model_program(model_source,limits);
    const auto original_d=declaration(parent,false),target_d=declaration(model,true);
    numeric::RationalVector grammar(68);for(std::size_t i=0;i<4;++i)grammar[17*i+i]=1;
    numeric::ExactActionGuard guard(limits);const auto field=field_interface_detail::tensor(grammar,guard);
    const auto original=OwnedRelationalInputAccession(a,parent,original_d,limits).admit(100,field,limits);
    RelationalKnownTensorSquareDeclaration sd{50000,1,9000,{{1},0},{}};for(std::uint64_t i=2000;i<6624;++i)sd.pair_directions.push_back(i);
    const OwnedRelationalKnownTensorSquare square(original,sd,limits);
    std::vector<RelationalMomentRolePair> pairs;std::size_t ordinal{};
    for(std::uint32_t h=0;h<4;++h)for(std::uint32_t d=0;d<4;++d)if(h!=d){const auto rank=h-(d<h?1u:0u);
        pairs.push_back({{{1},static_cast<std::uint32_t>(1+ordinal)},h,d,5+2*ordinal,6+2*ordinal,{17+2*rank,18+2*rank}});++ordinal;}
    std::vector<std::uint64_t> names;for(std::uint64_t i=0;i<7208;++i)names.push_back(700000000+i);
    const auto basis=make_relational_four_use_moment_basis(model,square,72080,1,2,names,pairs,limits);
    check(basis.parameters.variables.size()==64056&&basis.parameters.basis.nonzeros()==92616&&basis.parameters.parameters==names,"full actual flag variables and ordered named parameter axes");
    const PreparedRelationalFieldEvidence root(model,basis.parameters,limits);
    const auto known=square.admit(OwnedRelationalInputAccession(a,model,target_d,limits),limits);
    const auto response=root.execute(execute_relational_inputs(model,{known.input()},limits),limits);
    const auto lifted=square.lifted_field().values().dense();
    for(std::size_t pair=0;pair<12;++pair){numeric::ExactActionGuard g(limits);const auto reading=numeric::checked_apply(basis.readings[pair],lifted,g);
        const numeric::RationalVector base(reading.begin(),reading.begin()+68);
        check(numeric::checked_apply(basis.base_reconstructions[pair],base,g)==grammar,"complete source-conditioned inverse");
        check(numeric::dot(base,basis.base_metrics[pair].apply(base))==numeric::dot(grammar,grammar),"basis-aware base quadrance");
        const auto order=static_cast<std::size_t>(pairs[pair].head>pairs[pair].dependent);
        for(std::size_t role=0;role<2;++role)for(std::size_t row=0;row<1802;++row)
            check(response.field(2).directions.get(23*pairs[pair].dependent+pairs[pair].rows[role],(2*order+role)*1802+row)==reading[row],"actual attached parameter action agrees full declared reading");}
    auto omitted=pairs;omitted.pop_back();rejects([&]{make_relational_four_use_moment_basis(model,square,1,1,2,names,omitted,limits);},"incomplete pair carrier rejected");
    auto reordered=pairs;std::swap(reordered[0].head_flag,reordered[0].dependent_flag);rejects([&]{make_relational_four_use_moment_basis(model,square,1,1,2,names,reordered,limits);},"ordered occurrence identity cannot be swapped");
    auto rows=pairs;rows[0].rows[0]=0;rejects([&]{make_relational_four_use_moment_basis(model,square,1,1,2,names,rows,limits);},"equal-sized role row does not establish meaning");
    auto aliases=names;aliases[1]=aliases[0];rejects([&]{make_relational_four_use_moment_basis(model,square,1,1,2,aliases,pairs,limits);},"parameter identity aliases rejected");
    const auto wrong=model_program(history(604,true,true),limits);rejects([&]{make_relational_four_use_moment_basis(wrong,square,1,1,2,names,pairs,limits);},"profile requires declared physical unit pair participation");
    std::vector<RelationalOriginGrant> grants;const auto pub=storage::NativeSeriesPrefix::capture(publisher,1);const auto bytes=encode_relational_field_program(parent);
    const auto add=[&](std::uint64_t key,RelationalDependencyRole role,std::vector<std::byte> value){grants.push_back({OwnedRelationalValueOrigin(key,pub,role,std::move(value)),{603,1}});};
    add(1,RelationalDependencyRole::Program,bytes);add(2,RelationalDependencyRole::Maps,bytes);add(3,RelationalDependencyRole::Metrics,bytes);
    const RelationalAvailabilityContext pc(grants);const PreparedAvailableRelationalProgram prepared(parent,{1,2,3},pc);
    const auto image=[&](numeric::RationalVector value,RelationalInputAccessionDeclaration d,bool deny=false){
        numeric::ExactActionGuard g(limits);const auto tensor=field_interface_detail::tensor(value,g);auto current=grants;const auto ab=encode_relational_input_use_asset(a);
        const auto append=[&](std::uint64_t key,RelationalDependencyRole role,std::vector<std::byte> payload){current.push_back({OwnedRelationalValueOrigin(key,pub,role,std::move(payload)),{603,deny?0u:1u}});};
        append(10,RelationalDependencyRole::InputAsset,ab);append(11,RelationalDependencyRole::InputValue,relational_input_value_payload(ab,d,100,tensor));
        const auto access=relational_input_accession_payload(bytes,ab,d,100,tensor);append(12,RelationalDependencyRole::InputAccession,access);append(13,RelationalDependencyRole::Reconstruction,access);
        require_relational_four_use_grammar_image(prepared,{a,d,100,tensor},{10,11,12,13},RelationalAvailabilityContext(current));
    };
    image(grammar,original_d);check(true,"actual available four onehot fields admitted");
    rejects([&]{image(grammar,original_d,true);},"profile preflights current authority before reading field");
    auto invalid=grammar;invalid[0]=2;rejects([&]{image(invalid,original_d);},"arbitrary rational magnitude outside finite grammar image");
    invalid=grammar;invalid[1]=1;rejects([&]{image(invalid,original_d);},"multiple tags in one actual stalk rejected");
    invalid=grammar;invalid[0]=0;rejects([&]{image(invalid,original_d);},"supported zero does not invent a grammar tag");
    auto changed_basis=original_d;changed_basis.source_stalks[0].basis=702;rejects([&]{image(grammar,changed_basis);},"equal dimensions do not admit a different source basis");
    auto changed_metric=original_d;changed_metric.source_stalks[0].metric.set(0,0,2);rejects([&]{image(grammar,changed_metric);},"profile preserves its declared metric");
    rejects([&]{make_relational_four_use_moment_basis(model,square,1,1,2,names,pairs,limits,[]{throw std::runtime_error("cancel");});},"cancelled construction publishes no parameter basis");
}
}
int main(){try{run();std::cout<<checks<<" four-use moment basis checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
