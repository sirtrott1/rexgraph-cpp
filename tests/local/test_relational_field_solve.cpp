#include "rfc/learning/relational_available_local_solve_codec.hpp"
#include "rfc/learning/relational_local_solve_work.hpp"
#include <latch>
#include <fstream>
#include <filesystem>
#include "rfc/core/builder.hpp"
#include <future>
#include <iostream>
#include <string_view>
using namespace rfc;
using namespace rfc::inference;
using namespace rfc::learning;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using T=numeric::SparseRationalTensor;
using Program=OwnedRelationalFieldProgram;
using Declaration=RelationalFieldProgramDeclaration;
using Evidence=PreparedRelationalFieldEvidence;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool caught{};try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
T tensor(const V& v){numeric::ExactActionGuard guard;return field_interface_detail::tensor(v,guard);}
storage::NativeSeriesEndpoint source(bool renamed = false) {
    core::RelationSignature signature;
    const auto relation = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{renamed ? 200u : 100u}, relation,
        {{renamed ? 40u : 10u, true, false, Q{1}}, {renamed ? 50u : 20u, true, true, Q{1}}, {renamed ? 40u : 10u, true, true, Q{1}},
         {renamed ? 50u : 20u, false, false, Q{1}}, {renamed ? 60u : 30u, true, false, Q{0}}});
    builder.add_grade_one(core::RelationKey{renamed ? 201u : 101u}, relation,
        {{renamed ? 40u : 10u, true, false, Q{1}}, {renamed ? 50u : 20u, true, false, Q{1, 2}}, {renamed ? 60u : 30u, true, true, Q{1}}});
    storage::TemporalEndpoint endpoint(builder.freeze(), 7);
    return storage::NativeTemporalSeries(endpoint, 991).tail();
}
Declaration declaration() {
    Declaration d; d.key = 51; d.revision = 2; d.differential = true;
    d.stalks = {{{core::Grade{0}, 0}, 1001, {101, 102}, M::from_rows({{2, 1}, {1, 3}})},
                {{core::Grade{0}, 1}, 1002, {201, 202, 203}, M::identity(3)},
                {{core::Grade{0}, 2}, 1001, {101, 102}, M::identity(2)},
                {{core::Grade{1}, 0}, 1003, {301, 302}, M::from_rows({{3, 1}, {1, 2}})},
                {{core::Grade{1}, 1}, 1003, {301, 302}, M::identity(2)}};
    const auto phi = storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences = {{core::Grade{1}, 0, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 1, phi, M::from_rows({{1, 0}, {0, 1}, {1, 1}})},
                     {core::Grade{1}, 2, phi, M::from_rows({{1, 2}, {0, 1}})},
                     {core::Grade{1}, 3, phi, M::from_rows({{7, 3}, {2, 5}, {1, 1}})},
                     {core::Grade{1}, 4, phi, M::identity(2)},
                     {core::Grade{1}, 5, phi, M::from_rows({{2, 1}, {1, 3}})},
                     {core::Grade{1}, 6, phi, M::from_rows({{0, 1}, {1, 0}, {2, -1}})},
                     {core::Grade{1}, 7, storage::TemporalTensorRole::RealizedIncidenceMap,
                         M::from_rows({{-1, 1}, {2, -2}})}};
    d.nodes = {{1, inference::FieldProgramNodeKind::Input, core::Grade{1}, {}, {}},
               {2, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 1, {}},
               {3, inference::FieldProgramNodeKind::DeclaredMap, core::Grade{1}, 2,
                   M::from_rows({{1, 0, 1, 0, 0, 0, 0}, {0, 1, 0, 1, 0, 0, 0},
                                 {0, 0, 0, 0, 1, 1, 0}, {1, 0, 0, 0, 0, 0, 1}})},
               {4, inference::FieldProgramNodeKind::Boundary, core::Grade{0}, 3, {}}};
    return d;
}
void json(const V& values) {
    std::cout << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << '"' << values[i] << '"'; } std::cout << ']';
}
void json(const M& matrix) {
    std::cout << '['; for (std::size_t i = 0; i < matrix.rows(); ++i) {
        if (i) std::cout << ',';
        V row(matrix.cols());
        for (std::size_t j = 0; j < row.size(); ++j) row[j] = matrix.get(i, j);
        json(row);
    } std::cout << ']';
}


Declaration base_declaration() {
    auto d=declaration();d.nodes.resize(3);d.occurrences[0].map=M(2,2);d.occurrences[6].map=M(3,2);
    d.occurrences[7].role=storage::TemporalTensorRole::CoefficientIndependentMap;
    d.occurrences[7].map=d.occurrences[7].map.scale(-1);return d;
}
OwnedRelationalInputUseAsset asset(bool held=false,std::uint64_t ns=992) {
    const auto p=source(held);auto endpoint=p.endpoint();endpoint.sequence=held?20u:7u;
    storage::NativeTemporalSeries history(endpoint,ns);++endpoint.sequence;history.append_target(endpoint);++endpoint.sequence;history.append_target(endpoint);
    RelationalInputUseDeclaration d;d.key=600;d.channel=700;d.encoding="UTF-8";
    for(const auto c:std::string("caf\xc3\xa9 / caf\xc3\xa9"))d.bytes.push_back(static_cast<std::byte>(c));
    d.uses={{111,{storage::stable_cell_key(endpoint.source,{core::Grade{1},0})},{},{{0,5}}},
            {112,{storage::stable_cell_key(endpoint.source,{core::Grade{1},1})},{},{{8,13}}}};
    for(std::size_t i=0;i<8;++i)d.uses[0].occurrences.push_back({core::Grade{1},endpoint.source.occurrence_identity(core::Grade{1},i)});
    return {history,d};
}
RelationalFieldParameterDeclaration parameters() {
    RelationalFieldParameterDeclaration d;d.key=100;d.revision=1;d.node=2;d.parameters={101,102,103,104,105};
    d.variables={{0,0,0},{0,0,1},{0,1,0},{0,1,1},{6,0,0},{6,0,1},{6,1,0},{6,1,1},{6,2,0},{6,2,1},{3,0,0},{4,1,1}};
    d.basis=M(12,5);const std::vector<std::size_t> column{0,1,1,2,2,0,0,1,1,2,3,4};
    for(std::size_t i=0;i<column.size();++i)d.basis.set(i,column[i],1);
    return d;
}
// Independent declared target: two whole heterogeneous maps, not a terminal scalar fit.
V response(const V& x,const V& theta) {
    const auto a=x[0],b=x[1],c=x[2],d=x[3]; const auto u=theta[0],v=theta[1],w=theta[2];
    return {-a-2*b+2*c+d+u*a+v*b,-b+c+3*d+v*a+w*b,
        -a+(w*c+u*d)/2,-b+(u*c+v*d)/2,-a-b+(v*c+w*d)/2,-c+d,2*c-2*d};
}
using Origin=inference::OwnedRelationalValueOrigin;
using Grant=inference::RelationalOriginGrant;
using Context=inference::RelationalAvailabilityContext;
using Role=inference::RelationalDependencyRole;
using Available=PreparedAvailableRelationalEvidence;
using Bytes=std::vector<std::byte>;
template<class F>void rejects_origin(F f,const char* message){
    bool caught{};try{f();}catch(const std::invalid_argument& e){const std::string reason=e.what();caught=reason.find("origin")!=std::string::npos || reason.find("availability")!=std::string::npos;}
    check(caught,message);
}
storage::NativeSeriesPrefix publication(const OwnedRelationalInputUseAsset& bytes,bool late=false){
    auto history=bytes.history();if(late){const auto tail=history.tail();auto endpoint=tail.endpoint();++endpoint.sequence;history.append_target(endpoint);++endpoint.sequence;history.append_target(endpoint);}
    return storage::NativeSeriesPrefix::capture(history,history.last_sequence());
}
void grant(std::vector<Grant>& grants,std::uint64_t key,Role role,const Bytes& payload,storage::NativeSeriesPrefix prefix){
    const runtime::WorkAvailability cutoff{prefix.source_namespace(),prefix.cutoff()};
    grants.push_back({Origin(key,prefix,role,payload),cutoff});
}
RelationalInputSubmission submission(const Program& p,const OwnedRelationalInputUseAsset& bytes,const V& values){
    RelationalInputAccessionDeclaration a;a.key=901;a.node=1;a.source_grade={1};a.isometric=true;
    for(const auto& s:p.declaration().stalks)if(s.cell.grade.value==1)a.source_stalks.push_back(s);
    a.instances.cells={{0,1,2},{0,1}};a.instances.occurrences={{},{0,1,2,3,4,5,6,7}};
    a.accession=M::identity(4);a.reconstruction=M::identity(4);return {bytes,a,111,tensor(values)};
}
AvailableRelationalFieldResult published_input(const PreparedAvailableRelationalProgram& prepared,
    const OwnedRelationalInputUseAsset& bytes,const V& values,std::uint64_t first,std::vector<Grant>& grants){
    const auto spec=submission(prepared.program(),bytes,values);const auto ab=encode_relational_input_use_asset(bytes);
    const auto pb=encode_relational_field_program(prepared.program());
    const auto vp=relational_input_value_payload(ab,spec.declaration,spec.use,spec.source_field);
    const auto ap=relational_input_accession_payload(pb,ab,spec.declaration,spec.use,spec.source_field);
    grant(grants,first,Role::InputAsset,ab,publication(bytes));grant(grants,first+1,Role::InputValue,vp,publication(bytes,true));
    grant(grants,first+2,Role::InputAccession,ap,publication(bytes));grant(grants,first+3,Role::Reconstruction,ap,publication(bytes));
    const Context context(grants);const auto admitted=prepared.admit_input(spec,{first,first+1,first+2,first+3},context);
    return prepared.execute({admitted},context);
}
RelationalAvailableTrainingObservation measurement(const AvailableRelationalFieldResult& input,const Program& p,
    const V& values,std::uint64_t key,std::uint64_t first,std::vector<Grant>& grants){
    const auto target=asset(false,3000+key);const auto field=tensor(response(values,{2,-1,3}));
    const auto ab=encode_relational_input_use_asset(target);
    grant(grants,first,Role::TargetAsset,ab,publication(target));
    grant(grants,first+1,Role::TargetValue,relational_target_value_payload(encode_relational_field_program(p),2,ab,111,field),publication(target,true));
    return {key,input,target,111,field,{{core::Grade{0},0}},{first,first+1}};
}
Context withheld(const std::vector<Grant>& grants,std::uint64_t key){
    auto changed=grants;bool found{};for(auto& g:changed)if(g.origin.key()==key){g.availability.max_sequence=g.origin.publication().cutoff()-1;found=true;}
    if(!found)throw std::runtime_error("test missing origin");
    return Context(changed);
}
Program chain_program() {
    core::RelationSignature sig; const auto type=sig.add_type("relation",core::BoundaryPolicy::DeclaredExact,0,8);core::RelationalComplexBuilder b(sig);
    for(unsigned i=0;i<2;++i)b.add_grade_one(core::RelationKey{100+i},type,
        {{10,true,false,Q{1}},{20,true,true,Q{1}},{10,true,true,Q{1}},{20,false,false,Q{1}},{30,true,false,Q{0}}});
    b.add_higher(core::Grade{2},core::RelationKey{200},type,{{0,true,false,Q{1}},{1,true,true,Q{1}}});
    auto d=declaration();d.nodes.resize(3);
    d.stalks[0].directions={101,102,103};d.stalks[0].basis=9001;d.stalks[0].metric=M::from_rows({{3,1,0},{1,2,1},{0,1,3}});
    d.stalks[1].directions={201,202};d.stalks[1].basis=9002;d.stalks[1].metric=M::from_rows({{2,1},{1,2}});
    d.stalks.push_back({{core::Grade{2},0},9003,{501,502,503},M::from_rows({{3,1,0},{1,3,1},{0,1,2}})});
    d.occurrences.clear();const auto phi=storage::TemporalTensorRole::CoefficientIndependentMap;
    for(unsigned i=0;i<2;++i){
        const auto off=static_cast<std::size_t>(i)*5;
        d.occurrences.push_back({core::Grade{1},off,phi,M::from_rows({{2,2},{1,1},{-1,-1}})});
        d.occurrences.push_back({core::Grade{1},off+1,phi,M::from_rows({{-1,-1},{0,0}})});
        d.occurrences.push_back({core::Grade{1},off+2,phi,M::from_rows({{1,1},{-1,-1},{0,0}})});
        d.occurrences.push_back({core::Grade{1},off+3,phi,M::from_rows({{7,3},{2,5}})});
        d.occurrences.push_back({core::Grade{1},off+4,phi,M::identity(2)});
    }
    const auto c=M::from_rows({{1,2,0},{-1,-2,0}});
    d.occurrences.push_back({core::Grade{2},0,phi,c});d.occurrences.push_back({core::Grade{2},1,phi,c.scale(-1)});
    d.nodes[2].map=M::from_rows({{1,0,0,0,0,0,0},{0,1,0,0,0,0,0},{0,0,1,0,0,0,0},{0,0,0,1,0,0,0}});
    storage::TemporalEndpoint endpoint(b.freeze(),7);return Program(storage::NativeTemporalSeries(endpoint,777).tail(),d);
}
OwnedAccessionFieldResult raw_input(const Program& p,const V& values) {
    const auto source_owner=p.publication();const auto& c=source_owner.endpoint().source;
    storage::NativeTemporalSeries history(source_owner.endpoint(),888);
    RelationalInputUseDeclaration bytes;bytes.key=800;bytes.channel=801;bytes.encoding="UTF-8";bytes.bytes={std::byte{'x'}};
    RelationalSpanUse use;use.key=111;use.spans={{0,1}};use.cells.push_back(storage::stable_cell_key(c,{core::Grade{1},0}));
    RelationalInputAccessionDeclaration a;a.key=802;a.node=1;a.source_grade={1};a.isometric=true;
    for(const auto& s:p.declaration().stalks)if(s.cell.grade.value==1)a.source_stalks.push_back(s);
    for(std::size_t k=0;k<c.grade_count();++k){
        const auto g=core::Grade{static_cast<std::uint16_t>(k)};std::vector<std::uint32_t> cells;std::vector<std::size_t> flags;
        for(std::size_t i=0;i<c.grade(g).cell_count();++i)cells.push_back(static_cast<std::uint32_t>(i));
        for(std::size_t i=0;i<c.grade(g).incidence_count();++i){flags.push_back(i);use.occurrences.push_back({g,c.occurrence_identity(g,i)});}
        a.instances.cells.push_back(std::move(cells));a.instances.occurrences.push_back(std::move(flags));
    }
    bytes.uses={use};const OwnedRelationalInputUseAsset asset_owner(history,bytes);a.accession=M::identity(values.size());a.reconstruction=M::identity(values.size());
    const OwnedRelationalInputAccession accession(asset_owner,p,a);return execute_relational_inputs(p,{accession.admit(111,tensor(values))});
}
RelationalFieldSolveDeclaration problem_decl(bool inverse=false) {
    RelationalFieldSolveDeclaration d;d.key=700;d.revision=1;d.coordinate_carrier=701;d.coordinate_basis=702;d.grade={1};
    d.kind=inverse?RelationalFieldSolveKind::HarmonicInverse:RelationalFieldSolveKind::Green;d.lambda=inverse?Q{0}:Q{2,3};return d;
}
RelationalSolveReadoutDeclaration readout(const Program& p,core::Grade grade={1},bool subset=false) {
    RelationalSolveReadoutDeclaration d;const auto publication=p.publication();const auto& c=publication.endpoint().source;
    std::size_t width{};
    for(std::size_t k=0;k<c.grade_count();++k){std::vector<std::uint32_t> cells;
        for(std::size_t i=0;i<c.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();++i)
            if(!subset || k!=grade.value || i==0)cells.push_back(static_cast<std::uint32_t>(i));
        d.patch.push_back(std::move(cells));}
    for(const auto cell:d.patch.at(grade.value))width+=inference::field_interface_detail::stalk(p,{grade,cell}).directions.size();
    d.view.key=710;d.view.basis=711;for(std::size_t i=0;i<width+1;++i)d.view.directions.push_back(720+i);
    d.view.accession=M(width+1,width);d.view.reconstruction=M(width,width+1);
    for(std::size_t i=0;i<width;++i){d.view.accession.set(i,i,1);d.view.accession.set(width,i,1);d.view.reconstruction.set(i,i,1);}
    return d;
}
RelationalSolveOrigins publish_local(const AvailableRelationalSolveRhs& rhs,std::uint64_t node,const RelationalFieldSolveDeclaration& d,
    const RelationalSolveReadoutDeclaration& r,std::uint64_t first,std::vector<Grant>& grants,const storage::NativeSeriesPrefix& prefix){
    const auto payload=relational_local_problem_payload(rhs,node,d);grant(grants,first,Role::Maps,payload,prefix);
    grant(grants,first+1,Role::Interface,payload,prefix);grant(grants,first+2,Role::Reconstruction,relational_local_readout_payload(rhs,node,d,r),prefix);
    if(d.harmonic_frame)grant(grants,first+3,Role::Maps,payload,prefix);
    return {first,first+1,first+2,d.harmonic_frame?first+3:0};
}
void mathematical(bool emit) {
    const auto p=chain_program();const auto source_owner=p.publication();const auto& c=source_owner.endpoint().source;
    check(c.grade_count()==3 && c.grade({1}).incidence_count()==10 && c.grade({2}).incidence_count()==2,"complete original chain/occurrences");
    const auto base=problem_decl();const PreparedRelationalFieldSolve green(p,base);const PreparedRelationalFieldSolve inverse(p,problem_decl(true));
    check(inverse.operators().frame.cols()==2,"complete two-direction harmonic gauge");
    auto incomplete=problem_decl(true);incomplete.harmonic_frame=M(4,1);incomplete.harmonic_frame->set(0,0,1);
    rejects([&]{PreparedRelationalFieldSolve invalid(p,incomplete);},"incomplete gauge rejects");
    auto dependent=problem_decl(true);dependent.harmonic_frame=M(4,2);rejects([&]{PreparedRelationalFieldSolve invalid(p,dependent);},"dependent gauge rejects");
    auto negative=base;negative.lambda=-1;rejects([&]{PreparedRelationalFieldSolve invalid(p,negative);},"negative Green parameter rejects");
    auto partition=base;partition.interior={green.operators().layout.x};rejects([&]{PreparedRelationalFieldSolve invalid(p,partition);},"singular physical interior rejects");
    partition.interior={0,0};rejects([&]{PreparedRelationalFieldSolve invalid(p,partition);},"duplicate interior rejects");
    if(emit)std::cout<<'[';
    for(std::size_t trial=0;trial<12;++trial){const V j{Q{static_cast<long>(trial)-3,2},Q{static_cast<long>(trial%5)-1,3},Q{2},Q{static_cast<long>(trial%7)-2,5}};
        const auto input=raw_input(p,j);
        for(unsigned kind=0;kind<2;++kind){auto d=problem_decl(kind==1);if(!kind)d.lambda=Q{static_cast<long>(trial%4),3};
            const PreparedRelationalFieldSolve full(p,d);const auto answer=full.apply(input,1);
            const auto& op=full.operators();numeric::ExactActionGuard guard;
            const numeric::PreparedExactSolve global(op.augmented,guard);check(global.apply(answer.augmented_rhs(),guard)==answer.reconstructed(),"same global augmented solve equality");
            check(answer.field().support().count()==4 && answer.residual()==V(op.layout.size),"full supported field and residual");
            for(unsigned physical=0;physical<2;++physical){auto local=d;
                const auto stop=physical?op.layout.x:op.layout.c;for(std::size_t i=0;i<stop;++i)if(i<op.layout.t || i>=op.layout.x)local.interior.push_back(i);
                // Gauge rows have a zero diagonal and remain in the interface.
                const PreparedRelationalFieldSolve split(p,local);const auto reconstructed=split.apply(input,1);
                check(reconstructed.reconstructed()==answer.reconstructed(),"physical partition preserves complete response");
                check(split.partition().schur().apply(reconstructed.interface_response())==reconstructed.interface_rhs(),"retained Schur interface response");
            }
            if(emit){if(trial || kind)std::cout<<',';std::cout<<"{\"kind\":"<<kind<<",\"lambda\":\""<<d.lambda<<"\",\"B\":";json(op.lower);
                std::cout<<",\"C\":";json(op.upper);std::cout<<",\"N\":";json(op.metric);std::cout<<",\"Nb\":";json(op.below_metric);
                std::cout<<",\"Nu\":";json(op.above_metric);std::cout<<",\"H\":";json(op.frame);std::cout<<",\"j\":";json(j);
                std::cout<<",\"x\":";json(answer.field().values().dense());std::cout<<",\"aug\":";json(op.augmented);
                std::cout<<",\"rhs\":";json(answer.augmented_rhs());std::cout<<",\"full\":";json(answer.reconstructed());std::cout<<'}';}
        }
    }
    if(emit)std::cout<<"]\n";
    auto rebased=p.declaration();std::vector<M> change;numeric::ExactActionGuard rebase_guard;
    for(std::size_t i=0;i<rebased.stalks.size();++i){auto& stalk=rebased.stalks[i];auto basis=M::identity(stalk.directions.size());
        if(basis.rows()>1)basis.set(i%2?1:0,i%2?0:1,1);
        change.push_back(basis);
        stalk.metric=numeric::checked_multiply(basis.transpose(),numeric::checked_multiply(stalk.metric,basis,rebase_guard),rebase_guard);
        stalk.basis=10000+i;for(std::size_t j=0;j<stalk.directions.size();++j)stalk.directions[j]=20000+10*i+j;
    }
    const auto find_stalk=[&](core::CellId cell){for(std::size_t i=0;i<rebased.stalks.size();++i)if(rebased.stalks[i].cell==cell)return i;throw std::logic_error("basis fixture stalk");};
    for(auto& flag:rebased.occurrences){const auto& arena=c.grade(flag.grade);const auto upper=find_stalk({flag.grade,arena.upper_index.at(flag.occurrence)});
        const auto lower=find_stalk({core::Grade{static_cast<std::uint16_t>(flag.grade.value-1u)},arena.lower_index.at(flag.occurrence)});
        flag.map=numeric::checked_multiply(change[lower].inverse(),numeric::checked_multiply(flag.map,change[upper],rebase_guard),rebase_guard);
    }
    rebased.nodes.resize(2);const Program transformed(p.publication(),rebased);auto field_change=M(4,4);
    for(std::size_t i=0;i<2;++i)for(const auto& [ij,q]:change[3+i].entries())field_change.set(2*i+ij.first,2*i+ij.second,q);
    const V j{1,2,-3,4};const auto original_rhs=raw_input(p,j);const auto new_rhs=raw_input(transformed,field_change.inverse().apply(j));
    for(unsigned kind=0;kind<2;++kind){const auto d=problem_decl(kind==1);const auto a=PreparedRelationalFieldSolve(p,d).apply(original_rhs,1);
        const auto b=PreparedRelationalFieldSolve(transformed,d).apply(new_rhs,1);
        check(field_change.apply(b.field().values().dense())==a.field().values().dense(),"oblique named basis covariance of complete Green/gauged response");
    }
    const Program foreign(p.publication(),p.declaration());rejects([&]{(void)green.apply(raw_input(foreign,{1,2,3,4}),1);},"equal source/dimension program cannot replace actual RHS owner");
    const auto original=raw_input(p,{1,2,3,4});rejects([&]{(void)green.apply(original,2);},"wrong RHS grade rejects");
    auto limits=numeric::ExactActionLimits{};limits.dimension=3;rejects([&]{PreparedRelationalFieldSolve invalid(p,base,limits);},"preparation dimensions bounded");
    limits={};limits.steps=1;rejects([&]{(void)green.apply(original,1,limits);},"action steps bounded");
    limits={};limits.integer_bits=1;rejects([&]{(void)green.apply(original,1,limits);},"action integer growth bounded");
}
Bytes read_file(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing fixture file");const std::string s{std::istreambuf_iterator<char>(f),{}};Bytes b;for(const auto c:s)b.push_back(static_cast<std::byte>(c));return b;}
void write_file(const std::filesystem::path& path,const Bytes& b){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!f)throw std::runtime_error("fixture file write");}
Bytes authority_bytes(const std::vector<Grant>& grants){inference::field_program_codec_detail::Writer w;w.u64(grants.size());for(const auto& g:grants){w.u64(g.origin.key());w.u8(static_cast<std::uint8_t>(g.origin.role()));inference::field_program_codec_detail::blob(w,storage::encode_native_series(g.origin.publication().history()));inference::field_program_codec_detail::blob(w,g.origin.payload());w.u64(g.availability.source_namespace);w.u64(g.availability.max_sequence);}return std::move(w).take_bytes();}
Context authority(const Bytes& b){const storage::TemporalReadLimits policy;inference::field_program_codec_detail::Reader r(b,policy);std::vector<Grant> grants;const auto n=r.count(41);for(std::size_t i=0;i<n;++i){const auto key=r.u64();const auto role=static_cast<Role>(r.u8());auto history=storage::decode_native_series(r.blob());const auto raw=r.blob();Bytes payload(raw.begin(),raw.end());const runtime::WorkAvailability cutoff{r.u64(),r.u64()};grants.push_back({Origin(key,storage::NativeSeriesPrefix::capture(history,history.last_sequence()),role,payload),cutoff});}check(!r.remaining(),"independent trusted registry complete");return Context(grants);}
void available(const std::filesystem::path& export_dir={}){
    const Program p(source(),base_declaration());const auto bytes=asset();const auto prefix=publication(bytes);std::vector<Grant> grants;const auto pb=encode_relational_field_program(p);
    grant(grants,1,Role::Program,pb,prefix);grant(grants,2,Role::Maps,pb,prefix);grant(grants,3,Role::Metrics,pb,prefix);
    const PreparedAvailableRelationalProgram program(p,{1,2,3},Context(grants));const auto input=published_input(program,bytes,{1,2,-1,3},10,grants);
    auto d=problem_decl();auto read=readout(p,{1},true);const auto keys=publish_local(input,1,d,read,100,grants,prefix);
    const PreparedAvailableRelationalLocalSolve prepared(input,1,d,read,keys,Context(grants));const auto result=prepared.execute(Context(grants));const auto packet=encode_available_relational_local_result(result);
    const storage::TemporalReadLimits policy;
    check(result.patch_field().support().count()==2 && result.visible().support().count()==3,"faithful selected closed patch complete fields");
    check(result.reconstructed().values().dense()==result.patch_field().values().dense(),"full A/R admitted image reconstruction");
    check(encode_available_relational_local_result(decode_available_relational_local_result(packet,Context(grants),policy))==packet,"fixed local packet exact cold replay");
    auto changed_packet=packet;changed_packet.back()^=std::byte{1};
    const auto repair=[&](Bytes& image){const auto hash=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(image).subspan(32));
        for(unsigned i=0;i<8;++i)image[24+i]=static_cast<std::byte>((hash>>(8*i))&255u);};repair(changed_packet);
    rejects([&]{(void)decode_available_relational_local_result(changed_packet,Context(grants),policy);},"valid-envelope complete reconstruction witness graft rejects");
    using PacketReader=inference::field_program_codec_detail::Reader;PacketReader packet_reader(packet,policy);
    inference::field_program_codec_detail::header(packet_reader,packet,available_local_codec_detail::magic);(void)packet_reader.u8();(void)packet_reader.blob();(void)packet_reader.u64();
    const auto key_offset=packet_reader.position();changed_packet=packet;changed_packet.at(key_offset)^=std::byte{1};repair(changed_packet);
    rejects_origin([&]{(void)decode_available_relational_local_result(changed_packet,Context(grants),policy);},"valid-envelope local problem graft rejected before replay");
    const auto independent=authority(authority_bytes(grants));check(encode_available_relational_local_result(decode_available_relational_local_result(packet,independent,policy))==packet,"cold actual independent origin adoption");
    rejects([&]{(void)prepared.execute(independent);},"live equivalent receipt authority does not identify actual publishers");
    for(const auto key:{1u,2u,3u,10u,11u,12u,13u,100u,101u,102u}){
        rejects_origin([&]{(void)prepared.execute(withheld(grants,key));},"current local closure cutoff checked");
        rejects_origin([&]{(void)decode_available_relational_local_result(packet,withheld(grants,key),policy);},"cold local closure cutoff before replay");
    }
    auto invalid=d;invalid.interior={999};const auto invalid_keys=publish_local(input,1,invalid,read,110,grants,prefix);
    rejects_origin([&]{PreparedAvailableRelationalLocalSolve bad(input,1,invalid,read,invalid_keys,withheld(grants,112));},"late readout checked before invalid partition preparation");
    auto patch=read;patch.patch[0].clear();const auto patch_keys=publish_local(input,1,d,patch,120,grants,prefix);
    rejects([&]{PreparedAvailableRelationalLocalSolve bad(input,1,d,patch,patch_keys,Context(grants));},"semantic patch retains inactive and zero lower boundaries");
    auto gauge=d;gauge.kind=RelationalFieldSolveKind::HarmonicInverse;gauge.lambda=0;gauge.grade={0};auto gauge_read=readout(p,{0});
    const auto gauge_keys=publish_local(input,2,gauge,gauge_read,130,grants,prefix);
    const PreparedAvailableRelationalLocalSolve derived(input,2,gauge,gauge_read,gauge_keys,Context(grants));const auto derived_problem=derived.problem();gauge.harmonic_frame=derived_problem.operators().frame;
    const auto explicit_keys=publish_local(input,2,gauge,gauge_read,140,grants,prefix);
    const PreparedAvailableRelationalLocalSolve explicit_gauge(input,2,gauge,gauge_read,explicit_keys,Context(grants));
    const auto explicit_answer=explicit_gauge.execute(Context(grants)).result();const auto derived_answer=derived.execute(Context(grants)).result();
    check(explicit_answer.field().values().dense()==derived_answer.field().values().dense(),"declared complete gauge equals exact derived response");
    rejects_origin([&]{(void)explicit_gauge.execute(withheld(grants,143));},"supplied gauge immutable map origin checked");
    std::optional<PreparedAvailableRelationalLocalSolve> replace{prepared};bool replaced{};
    const auto escaped=replace->execute(Context(grants),{},[&]{if(!replaced){replace.reset();replaced=true;}});
    check(encode_available_relational_local_result(escaped)==packet,"local callback release pins parent through complete result");
    std::vector<std::future<Bytes>> readers;for(unsigned i=0;i<6;++i)readers.push_back(std::async(std::launch::async,[prepared,grants]{return encode_available_relational_local_result(prepared.execute(Context(grants)));}));
    for(auto& f:readers)check(f.get()==packet,"six independent exact readers");
    runtime::HardwareInventory hardware;hardware.platform="test";hardware.affinity_observed=true;for(std::size_t i=0;i<4;++i)hardware.allowed_cpus.push_back({i,0,i,{}});
    runtime::CpuResourceCoordinator service(hardware,{2,1u<<20,100});const runtime::WorkDemand demand{1,4096,1u<<20};
    auto job=relational_local_solve_job({200,1},demand,input,1,d,read,keys,Context(grants));auto run=runtime::WorkGraph({job}).submit(service);run.wait();
    check(encode_available_relational_local_result(*run.completion({200,1}).result<AvailableRelationalLocalResult>())==packet,"actual joined work result");
    auto mutable_d=d;mutable_d.interior={0};const auto mutable_keys=publish_local(input,1,mutable_d,read,160,grants,prefix);auto& retained_index=mutable_d.interior.at(0);
    auto immutable_job=relational_local_solve_job({230,1},demand,input,1,mutable_d,read,mutable_keys,Context(grants));retained_index=999;
    auto immutable_run=runtime::WorkGraph({immutable_job}).submit(service);immutable_run.wait();
    check(immutable_run.completion({230,1}).receipt().status==runtime::WorkStatus::Succeeded,"queued declaration remains pinned after caller input changes");
    auto mutable_packet=packet;auto& retained_byte=mutable_packet.at(0);
    auto immutable_replay=relational_local_replay_job({231,1},demand,mutable_packet,Context(grants),policy);retained_byte=std::byte{0};
    auto immutable_replay_run=runtime::WorkGraph({immutable_replay}).submit(service);immutable_replay_run.wait();
    check(encode_available_relational_local_result(*immutable_replay_run.completion({231,1}).result<AvailableRelationalLocalResult>())==packet,"queued packet remains pinned after caller input changes");
    const auto baseline=result;
    for(const auto cancel_step:{std::size_t{1},std::size_t{25},std::size_t{100},std::size_t{250},std::size_t{500}}){
        std::latch entered(1),release(1);std::atomic<std::size_t> count{};std::atomic<bool> finished{};
        const auto callback=[&]{if(++count==cancel_step){entered.count_down();release.wait();}};
        auto work=relational_local_solve_job({201,cancel_step},demand,input,1,d,read,keys,Context(grants),{},callback);
        auto active=runtime::WorkGraph({work}).submit(service);auto waiter=std::async(std::launch::async,[&]{active.wait();finished=true;});entered.wait();active.cancel();
        check(!finished,"stop waits for actual owned action to exit");release.count_down();waiter.get();
        check(active.completion({201,cancel_step}).receipt().status==runtime::WorkStatus::Cancelled,"actual owned stop status");
        rejects([&]{(void)active.completion({201,cancel_step}).result<AvailableRelationalLocalResult>();},"cancelled work has no published result");
        check(service.available_workers()==2 && encode_available_relational_local_result(baseline)==packet,"join returns resources and preserves old immutable result");
    }
    for(const auto phase:{RelationalLocalWorkPhase::Preparation,RelationalLocalWorkPhase::Action,RelationalLocalWorkPhase::Replay}){
        std::latch entered(1),release(1);const auto callback=[&](RelationalLocalWorkPhase current){if(current==phase){entered.count_down();release.wait();}};
        const runtime::WorkKey work_key{210,static_cast<std::uint64_t>(phase)+1};
        auto work=phase==RelationalLocalWorkPhase::Replay?relational_local_replay_job(work_key,demand,packet,Context(grants),policy,{}, {},callback):
            relational_local_solve_job(work_key,demand,input,1,d,read,keys,Context(grants),{}, {},callback);
        auto active=runtime::WorkGraph({work}).submit(service);entered.wait();active.cancel();release.count_down();active.wait();
        check(active.completion(work_key).receipt().status==runtime::WorkStatus::Cancelled,"owned preparation/action/replay phase stop joined");
        rejects([&]{(void)active.completion(work_key).result<AvailableRelationalLocalResult>();},"phase stop publishes no partial packet or field");
        check(service.available_workers()==2,"all phase resource leases returned after join");
    }
    auto replay_run=runtime::WorkGraph({relational_local_replay_job({220,1},demand,packet,Context(grants),policy)}).submit(service);replay_run.wait();
    check(encode_available_relational_local_result(*replay_run.completion({220,1}).result<AvailableRelationalLocalResult>())==packet,"actual joined packet replay exact");
    const auto params=parameters();grant(grants,4,Role::Evidence,relational_evidence_root_payload(pb,params,{}),prefix);
    const Available root(program,params,4,Context(grants));const auto training=published_input(program,bytes,{1,0,0,0},20,grants);
    const auto observation=measurement(training,p,{1,0,0,0},71,30,grants);const auto evidence=root.append({observation},Context(grants));
    const auto held=published_input(program,asset(true,4444),{2,-1,3,1},40,grants);const auto family=evidence.execute(held,Context(grants));
    const auto native_evidence=evidence.evidence();V free(native_evidence.kernel().cols());
    // Use the native common free carrier, retaining all unresolved directions.
    free=V(native_evidence.free_parameters().size(),Q{0});
    Bytes first_packet;V first_response;
    for(unsigned setting_index=0;setting_index<2;++setting_index){if(!free.empty())free[0]=Q{static_cast<long>(setting_index)+3};
        const auto setting_key=50u+setting_index;grant(grants,setting_key,Role::Prospective,relational_parameter_setting_payload(evidence,free),prefix);
        const auto choice=family.evaluate(free,setting_key,Context(grants));const auto selected=choice.result().result().result().program();auto choice_read=readout(selected);auto choice_d=problem_decl();
        const auto choice_keys=publish_local(choice,1,choice_d,choice_read,200+setting_index*10,grants,prefix);
        const PreparedAvailableRelationalLocalSolve child(choice,1,choice_d,choice_read,choice_keys,Context(grants));const auto answer=child.execute(Context(grants));const auto encoded=encode_available_relational_local_result(answer);
        check(encode_available_relational_local_result(decode_available_relational_local_result(encoded,Context(grants),policy))==encoded,"selected actual learned child cold local solve");
        const auto child_problem=child.problem();const auto child_owner=child_problem.program();check(child_owner.fibers().same_owner(selected.fibers()),"actual selected child program owner retained");
        check(!selected.fibers().same_owner(p.fibers()),"prospective child not coerced to root");
        rejects_origin([&]{(void)child.execute(withheld(grants,31));},"local selected child retains historical target cutoff");
        rejects_origin([&]{(void)decode_available_relational_local_result(encoded,withheld(grants,setting_key),policy);},"local cold selected child global setting before replay");
        const auto answer_native=answer.result();
        if(!setting_index){first_packet=encoded;first_response=answer_native.field().values().dense();}
        else check(first_response!=answer_native.field().values().dense(),"learned whole map alternative changes complete held local response");
    }
    if(!export_dir.empty()){std::filesystem::create_directories(export_dir);write_file(export_dir/"fixed.packet",packet);write_file(export_dir/"child.packet",first_packet);write_file(export_dir/"authority.bin",authority_bytes(grants));}
}
} // namespace
int main(int argc,char** argv){try{if(argc==4 && std::string_view(argv[1])=="--serve"){
    const auto packet=read_file(argv[2]);const auto ctx=authority(read_file(argv[3]));const auto result=decode_available_relational_local_result(packet,ctx,storage::TemporalReadLimits{});
    check(encode_available_relational_local_result(result)==packet,"standalone disclosed global context cold serving");std::cout<<"served "<<packet.size()<<" bytes\n";return 0;}
    const bool emit=argc==2 && std::string_view(argv[1])=="--json";mathematical(emit);if(!emit){available(argc==3 && std::string_view(argv[1])=="--export"?argv[2]:std::filesystem::path{});std::cout<<checks<<" checks\n";}return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
