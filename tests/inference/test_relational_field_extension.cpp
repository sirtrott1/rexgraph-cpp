#include "rfc/learning/relational_available_extension_codec.hpp"
#include "rfc/learning/relational_available_local_solve_codec.hpp"
#include "rfc/learning/relational_local_solve_work.hpp"
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
struct ExtensionFixture {Program target;RelationalFieldExtensionDeclaration declaration;};
M inclusion(std::size_t n){M j(n+1,n);for(std::size_t i=0;i<n;++i){j.set(i,i,1);j.set(n,i,Q{static_cast<long>(i)+1,2});}return j;}
M back(std::size_t n){M r(n,n+1);for(std::size_t i=0;i<n;++i)r.set(i,i,1);return r;}
storage::NativeSeriesEndpoint larger_source(){
    core::RelationSignature signature;const auto relation=signature.add_type("relation",core::BoundaryPolicy::DeclaredExact,0,8);core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100},relation,{{10,true,false,Q{1}},{20,true,true,Q{1}},{10,true,true,Q{1}},{20,false,false,Q{1}},{30,true,false,Q{0}}});
    builder.add_grade_one(core::RelationKey{101},relation,{{10,true,false,Q{1}},{20,true,false,Q{1,2}},{30,true,true,Q{1}}});
    builder.add_grade_one(core::RelationKey{102},relation,{{10,true,false,Q{1}},{20,true,true,Q{1}},{40,true,false,Q{1,2}},{40,false,true,Q{1}}});
    builder.add_higher(core::Grade{2},core::RelationKey{200},relation,{{0,true,false,Q{1}},{0,true,true,Q{1}},{1,false,false,Q{1}},{1,true,false,Q{0}}});
    return storage::NativeTemporalSeries(storage::TemporalEndpoint(builder.freeze(),30),1991).tail();
}
ExtensionFixture fixture(const Program& old,bool grow_source=true){
    numeric::ExactActionGuard guard;const auto publication=grow_source?larger_source():old.publication();const auto& source=publication.endpoint().source;
    auto declaration=old.declaration();++declaration.revision;declaration.stalks.clear();declaration.occurrences.clear();declaration.nodes.clear();
    RelationalFieldExtensionDeclaration d;d.key=800;d.revision=1;d.free_carrier=801;d.free_basis=802;d.isometric=true;
    const auto original=old.publication();const auto& before=original.endpoint().source;
    for(std::size_t k=0;k<before.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};std::vector<std::uint32_t> cells;std::vector<std::size_t> flags;
        for(std::size_t i=0;i<before.grade(g).cell_count();++i)cells.push_back(static_cast<std::uint32_t>(i));
        for(std::size_t i=0;i<before.grade(g).incidence_count();++i)flags.push_back(i);
        d.source_map.cells.push_back(std::move(cells));d.source_map.occurrences.push_back(std::move(flags));}
    std::map<core::CellId,M> jmap,rmap;std::size_t stalk_position{};
    for(std::size_t k=0;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<source.grade(g).cell_count();++i){const core::CellId cell{g,static_cast<std::uint32_t>(i)};
            const auto found=std::find_if(old.declaration().stalks.begin(),old.declaration().stalks.end(),[&](const auto& s){return s.cell==cell;});
            if(found!=old.declaration().stalks.end()){
                const auto n=found->directions.size();const auto j=inclusion(n),r=back(n);jmap.emplace(cell,j);rmap.emplace(cell,r);
                auto metric=M(n+1,n+1);for(const auto& [ij,q]:found->metric.entries())metric.set(ij.first,ij.second,q);metric.set(n,n,2);
                auto change=M::identity(n+1);for(std::size_t row=0;row<n;++row)change.set(n,row,-j.get(n,row));metric=change.transpose().multiply(metric).multiply(change);
                const auto basis=10000+stalk_position+100*declaration.revision;std::vector<std::uint64_t> directions;
                for(std::size_t row=0;row<=n;++row)directions.push_back(20000+100*stalk_position+row+10000*declaration.revision);
                declaration.stalks.push_back({cell,basis,directions,metric});d.stalks.push_back({cell,found->basis,basis,j,r});
            }else{const auto n=k==0?2u:3u;std::vector<std::uint64_t> directions;for(unsigned row=0;row<n;++row)directions.push_back(50000+100*stalk_position+row);
                declaration.stalks.push_back({cell,60000+stalk_position,directions,M::identity(n)});}
            ++stalk_position;
        }
    }
    const auto stalk=[&](core::CellId cell)->const StalkBasisDeclaration&{for(const auto& s:declaration.stalks)if(s.cell==cell)return s;throw std::logic_error("test missing target stalk");};
    for(std::size_t k=1;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto& arena=source.grade(g);
        for(std::size_t p=0;p<arena.incidence_count();++p){const core::CellId upper{g,arena.upper_index[p]},lower{core::Grade{static_cast<std::uint16_t>(k-1)},arena.lower_index[p]};
            const auto oldflag=std::find_if(old.declaration().occurrences.begin(),old.declaration().occurrences.end(),[&](const auto& f){return f.grade==g && f.occurrence==p;});
            if(oldflag!=old.declaration().occurrences.end()){
                auto map=jmap.at(lower).multiply(oldflag->map).multiply(rmap.at(upper));const auto complement=M::identity(jmap.at(upper).rows()).add(jmap.at(upper).multiply(rmap.at(upper)).scale(-1));
                M extra(map.rows(),map.cols());extra.set(0,map.cols()-1,Q{k==2?1:static_cast<long>(p%3)+1,3});map=map.add(extra.multiply(complement));
                declaration.occurrences.push_back({g,p,oldflag->role,map});
            }else if(k==2){M map(stalk(lower).directions.size(),stalk(upper).directions.size());map.set(0,0,1);map.set(1,1,2);declaration.occurrences.push_back({g,p,storage::TemporalTensorRole::CoefficientIndependentMap,map});}
            else{M map(stalk(lower).directions.size(),stalk(upper).directions.size());for(std::size_t row=0;row<map.rows();++row)for(std::size_t col=0;col<map.cols();++col)map.set(row,col,Q{static_cast<long>((row+1)*(col+2)),static_cast<long>(p-6)});
                declaration.occurrences.push_back({g,p,storage::TemporalTensorRole::CoefficientIndependentMap,map});}
        }
    }
    local::LinearStalkBundle bundle(source);for(const auto& s:declaration.stalks)bundle.set_dimension(s.cell,s.directions.size());
    std::vector<M> grade_j,grade_r;
    for(std::size_t k=0;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto n=bundle.total_dimension(g);const auto m=k<before.grade_count()?old.fibers().fibers().stalks().total_dimension(g):0;
        grade_j.emplace_back(n,m);grade_r.emplace_back(m,n);if(k>=before.grade_count())continue;
        const auto a=old.fibers().fibers().stalks().offsets(g),b=bundle.offsets(g);
        for(const auto& s:d.stalks)if(s.cell.grade==g){for(const auto& [ij,q]:s.inclusion.entries())grade_j[k].set(b[s.cell.index]+ij.first,a[s.cell.index]+ij.second,q);
            for(const auto& [ij,q]:s.reconstruction.entries())grade_r[k].set(a[s.cell.index]+ij.first,b[s.cell.index]+ij.second,q);}
    }
    for(const auto& node:old.declaration().nodes){auto next=node;
        if(node.kind==FieldProgramNodeKind::DeclaredMap){const auto input_grade=inference::field_interface_detail::node(old,*node.input).output_grade;
            next.map=grade_j[node.output_grade.value].multiply(node.map).multiply(grade_r[input_grade.value]);}
        declaration.nodes.push_back(next);
        if(node.kind==FieldProgramNodeKind::Input){const auto width=grade_j[node.output_grade.value].rows()-grade_j[node.output_grade.value].cols();for(std::size_t i=0;i<width;++i)d.free_directions.push_back(90000+d.free_directions.size());}
    }
    declaration.nodes.push_back({99+static_cast<std::uint64_t>(old.declaration().nodes.size()-3),FieldProgramNodeKind::DeclaredMap,{1},3,M::identity(bundle.total_dimension({1}))});
    return {Program(publication,declaration),d};
}
RelationalAvailableInputSubmission input_submission(const Program& p,const OwnedRelationalInputUseAsset& a,const V& values,std::uint64_t first,std::vector<Grant>& grants){
    const auto row=submission(p,a,values);const auto ab=encode_relational_input_use_asset(a);const auto pb=encode_relational_field_program(p);
    const auto value=relational_input_value_payload(ab,row.declaration,row.use,row.source_field);const auto access=relational_input_accession_payload(pb,ab,row.declaration,row.use,row.source_field);
    grant(grants,first,Role::InputAsset,ab,publication(a));grant(grants,first+1,Role::InputValue,value,publication(a,true));grant(grants,first+2,Role::InputAccession,access,publication(a));grant(grants,first+3,Role::Reconstruction,access,publication(a));
    return {row,{first,first+1,first+2,first+3}};
}
void raw(bool emit){
    const Program p(source(),base_declaration());const auto f=fixture(p);const OwnedRelationalFieldExtension model(p,f.target,f.declaration);const auto archive=encode_relational_field_extension(model);const storage::TemporalReadLimits policy;
    check(encode_relational_field_extension(decode_relational_field_extension(archive,policy))==archive,"complete source extension cold replay");
    check(model.kernel({1}).cols()==5 && model.kernel({2}).cols()==3,"new complete input and higher grade complements unresolved");
    const auto asset_owner=asset();const auto row=submission(p,asset_owner,{1,2,3,4});const OwnedRelationalInputAccession accession(asset_owner,p,row.declaration);const auto input=execute_relational_inputs(p,{accession.admit(row.use,row.source_field)});
    const auto transported=model.transport_accession(accession,909);const OwnedRelationalInputAccession next_accession(asset_owner,f.target,transported);
    const auto represented=execute_relational_inputs(f.target,{next_accession.admit(row.use,row.source_field)});
    const auto represented_asset=represented.inputs().at(0).accession().asset();
    check(represented_asset.declaration().bytes==asset_owner.declaration().bytes,"original byte/span source preserved under accession transport");
    const auto family=model.execute(input);const auto zero=family.evaluate(V(5));const auto target_result=zero.result();const auto represented_result=represented.result();
    for(const auto& node:p.declaration().nodes)check(target_result.field(node.key).values().dense()==represented_result.field(node.key).values().dense(),"explicit parent admitted image equals faithful source accession");
    auto bad=f.declaration;bad.free_directions.pop_back();rejects([&]{OwnedRelationalFieldExtension invalid(p,f.target,bad);},"incomplete named free carrier rejects");
    bad=f.declaration;bad.stalks[0].source_basis++;rejects([&]{OwnedRelationalFieldExtension invalid(p,f.target,bad);},"wrong source basis order rejects");
    bad=f.declaration;bad.stalks[0].reconstruction.set(0,0,2);rejects([&]{OwnedRelationalFieldExtension invalid(p,f.target,bad);},"false RJ rejects");
    bad=f.declaration;std::swap(bad.source_map.occurrences[1][0],bad.source_map.occurrences[1][2]);rejects([&]{OwnedRelationalFieldExtension invalid(p,f.target,bad);},"ordered repeated flags cannot be permuted");
    auto bad_program=f.target.declaration();bad_program.occurrences[3].map.set(0,0,99);const Program changed(f.target.publication(),bad_program);
    rejects([&]{OwnedRelationalFieldExtension invalid(p,changed,f.declaration);},"inactive intrinsic Phi compatibility retained");
    bad_program=f.target.declaration();bad_program.nodes[2].map.set(0,0,99);const Program action_changed(f.target.publication(),bad_program);
    rejects([&]{OwnedRelationalFieldExtension invalid(p,action_changed,f.declaration);},"original native node actions commute with inclusion");
    bad_program=f.target.declaration();bad_program.stalks[0].metric=M::identity(3);const Program metric_changed(f.target.publication(),bad_program);
    rejects([&]{OwnedRelationalFieldExtension invalid(p,metric_changed,f.declaration);},"metric substitution false pullback rejects");
    const Program foreign(p.publication(),p.declaration());const auto foreign_row=submission(foreign,asset_owner,{1,2,3,4});const OwnedRelationalInputAccession foreign_a(asset_owner,foreign,foreign_row.declaration);
    rejects([&]{(void)model.execute(execute_relational_inputs(foreign,{foreign_a.admit(row.use,row.source_field)}));},"equal bytes cannot replace actual extension parent");
    rejects([&]{(void)family.evaluate({0});},"unknown complement cannot silently pad missing coordinates");
    const auto branch=fixture(f.target,false);auto next_decl=branch.declaration;next_decl.revision=2;const auto next=model.extend(branch.target,next_decl);
    check(next.parent().has_value() && encode_relational_field_extension(decode_relational_field_extension(encode_relational_field_extension(next),policy))==encode_relational_field_extension(next),"whole actual extension lineage preserved");
    if(emit)std::cout<<'[';
    for(std::size_t trial=0;trial<12;++trial){const V values{Q{static_cast<long>(trial)-3,2},Q{static_cast<long>(trial%5)-1,3},Q{2},Q{static_cast<long>(trial%7)-2,5}};
        const auto spec=submission(p,asset_owner,values);const auto original=execute_relational_inputs(p,{accession.admit(spec.use,spec.source_field)});const auto fields=model.execute(original);
        V free(5);for(std::size_t i=0;i<free.size();++i)free[i]=Q{static_cast<long>(i+trial)-2,static_cast<long>(i)+2};const auto answer=fields.evaluate(free);const auto native=answer.result();
        check(encode_relational_extension_family(decode_relational_extension_family(encode_relational_extension_family(fields),policy))==encode_relational_extension_family(fields),"whole affine intermediary fields canonical");
        check(encode_relational_extension_evaluation(decode_relational_extension_evaluation(encode_relational_extension_evaluation(answer),policy))==encode_relational_extension_evaluation(answer),"explicit whole completion canonical");
        if(emit){if(trial)std::cout<<',';std::cout<<"{\"source\":";json(values);std::cout<<",\"J\":";json(model.inclusion({1}));std::cout<<",\"R\":";json(model.reconstruction({1}));std::cout<<",\"K\":";json(model.kernel({1}));std::cout<<",\"free\":";json(free);std::cout<<",\"flags\":[";
            const auto target_publication=f.target.publication();const auto& arena=target_publication.endpoint().source.grade({1});const auto offsets=f.target.fibers().fibers().stalks().offsets({1});const auto lower_offsets=f.target.fibers().fibers().stalks().offsets({0});
            for(std::size_t slot=0;slot<arena.incidence_count();++slot){if(slot)std::cout<<',';const auto& flag=f.target.declaration().occurrences.at(slot);
                std::cout<<"{\"slot\":"<<slot<<",\"coefficient\":\""<<(arena.aspects.get(target_publication.endpoint().source.signature().existence_aspect(),slot)?target_publication.endpoint().source.incidence_coefficient({1},slot):Q{0})<<"\",\"source\":"<<offsets[arena.upper_index[slot]]<<",\"target\":"<<lower_offsets[arena.lower_index[slot]]<<",\"Phi\":";json(flag.map);std::cout<<'}';}
            std::cout<<"],\"fields\":[";}
        for(std::size_t i=0;i<fields.fields().size();++i){const auto& field=fields.fields()[i];const auto target=f.target.node_action(field.node);const auto occurrence=f.target.occurrence_action(field.node);

            check(field.value.directions.cols()==5 && field.value.particular.support().count()==field.value.directions.rows(),"whole supported correlated field state");
            if(emit){if(i)std::cout<<',';std::cout<<"{\"node\":"<<field.node<<",\"input\":"<<(i?f.target.declaration().nodes[i].input.value():0)<<",\"map\":";json(target?target->matrix():M(0,0));
                std::cout<<",\"particular\":";json(field.value.particular.values().dense());std::cout<<",\"directions\":";json(field.value.directions);std::cout<<",\"value\":";json(native.field(field.node).values().dense());
                if(occurrence){std::cout<<",\"occurrence_map\":";json(occurrence->matrix());std::cout<<",\"occurrence_particular\":";json(field.occurrences->particular.values().dense());std::cout<<",\"occurrence_directions\":";json(field.occurrences->directions);std::cout<<",\"occurrence_value\":";json(native.occurrence_field(field.node).values().dense());}
                std::cout<<'}';}
        }
        if(emit)std::cout<<"]}";
    }
    if(emit)std::cout<<"]\n";
}
void filling_counterexample(){
    core::RelationSignature sig;const auto type=sig.add_type("relation",core::BoundaryPolicy::DeclaredExact,0,8);
    const auto make=[&](bool filled){core::RelationalComplexBuilder b(sig);for(unsigned i=0;i<3;++i)b.add_grade_one(core::RelationKey{100+i},type,
        {{10+i,true,true,Q{1}},{10+(i+1)%3,true,false,Q{1}}});
        if(filled)b.add_higher(core::Grade{2},core::RelationKey{200},type,{{0,true,false,Q{1}},{1,true,false,Q{1}},{2,true,false,Q{1}}});
        const auto source=b.freeze();storage::NativeTemporalSeries history(storage::TemporalEndpoint(source,10),filled?601:600);
        Declaration d;d.key=900;d.revision=filled?2:1;d.differential=true;
        for(std::size_t k=0;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};
            for(std::size_t i=0;i<source.grade(g).cell_count();++i)d.stalks.push_back({{g,static_cast<std::uint32_t>(i)},500,{501},M::identity(1)});
            if(k)for(std::size_t p=0;p<source.grade(g).incidence_count();++p)d.occurrences.push_back({g,p,storage::TemporalTensorRole::CoefficientIndependentMap,M::identity(1)});}
        d.nodes={{1,FieldProgramNodeKind::Input,{1},{},{}},{2,FieldProgramNodeKind::Boundary,{0},1,{}}};return Program(history.tail(),d);};
    const auto p=make(false),target=make(true);RelationalFieldExtensionDeclaration d;d.key=901;d.revision=1;d.free_carrier=902;d.free_basis=903;d.isometric=true;
    d.source_map.cells={{0,1,2},{0,1,2}};d.source_map.occurrences={{},{0,1,2,3,4,5}};
    for(const auto& s:p.declaration().stalks)d.stalks.push_back({s.cell,s.basis,s.basis,M::identity(1),M::identity(1)});
    const OwnedRelationalFieldExtension extension(p,target,d);
    const auto tiny_publication=p.publication();storage::NativeTemporalSeries history(tiny_publication.endpoint(),7777);RelationalInputUseDeclaration bytes;bytes.key=10;bytes.channel=11;bytes.encoding="UTF-8";bytes.bytes={std::byte{'x'}};
    bytes.uses={{111,{{{1},100}},{},{{0,1}}}};const OwnedRelationalInputUseAsset a(history,bytes);
    RelationalInputAccessionDeclaration input;input.key=12;input.node=1;input.source_grade={1};input.instances=d.source_map;input.accession=M::identity(3);input.reconstruction=M::identity(3);input.isometric=true;
    for(const auto& s:p.declaration().stalks)if(s.cell.grade.value==1)input.source_stalks.push_back(s);
    const OwnedRelationalInputAccession accession(a,p,input);const auto original=execute_relational_inputs(p,{accession.admit(111,tensor({1,1,1}))});
    const OwnedRelationalInputAccession transported(a,target,extension.transport_accession(accession,13));const auto represented=execute_relational_inputs(target,{transported.admit(111,tensor({1,1,1}))});
    RelationalFieldSolveDeclaration green;green.key=14;green.coordinate_carrier=15;green.coordinate_basis=16;green.grade={1};green.lambda=1;
    const auto before=PreparedRelationalFieldSolve(p,green).apply(original,1);const auto after=PreparedRelationalFieldSolve(target,green).apply(represented,1);
    check(before.field().values().dense()==V({1,1,1}) && after.field().values().dense()==V({Q{1,4},Q{1,4},Q{1,4}}),"faithful isometric boundary extension does not imply Green naturality");
}
Bytes authority_bytes(const std::vector<Grant>& grants){inference::field_program_codec_detail::Writer w;w.u64(grants.size());for(const auto& g:grants){w.u64(g.origin.key());w.u8(static_cast<std::uint8_t>(g.origin.role()));inference::field_program_codec_detail::blob(w,storage::encode_native_series(g.origin.publication().history()));inference::field_program_codec_detail::blob(w,g.origin.payload());w.u64(g.availability.source_namespace);w.u64(g.availability.max_sequence);}return std::move(w).take_bytes();}
Context authority(const Bytes& bytes){const storage::TemporalReadLimits policy;inference::field_program_codec_detail::Reader r(bytes,policy);std::vector<Grant> grants;const auto n=r.count(41);for(std::size_t i=0;i<n;++i){const auto key=r.u64();const auto role=static_cast<Role>(r.u8());auto history=storage::decode_native_series(r.blob());const auto raw=r.blob();const runtime::WorkAvailability cutoff{r.u64(),r.u64()};grants.push_back({Origin(key,storage::NativeSeriesPrefix::capture(history,history.last_sequence()),role,Bytes(raw.begin(),raw.end())),cutoff});}check(!r.remaining(),"independent trusted registry complete");return Context(grants);}
Bytes read_file(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing fixture file");const std::string s{std::istreambuf_iterator<char>(f),{}};Bytes bytes;for(const auto c:s)bytes.push_back(static_cast<std::byte>(c));return bytes;}
void write_file(const std::filesystem::path& path,const Bytes& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!f)throw std::runtime_error("fixture write");}
void available(const std::filesystem::path& export_dir={}){
    const Program p(source(),base_declaration());const auto bytes=asset();const auto prefix=publication(bytes);std::vector<Grant> grants;const auto pb=encode_relational_field_program(p);
    grant(grants,1,Role::Program,pb,prefix);grant(grants,2,Role::Maps,pb,prefix);grant(grants,3,Role::Metrics,pb,prefix);const PreparedAvailableRelationalProgram parent(p,{1,2,3},Context(grants));
    const auto f=fixture(p);const auto target_bytes=encode_relational_field_program(f.target);const auto target_publication=f.target.publication();storage::NativeTemporalSeries target_history(target_publication.endpoint(),1991);
    const auto target_prefix=storage::NativeSeriesPrefix::capture(target_history,target_history.last_sequence());
    grant(grants,5,Role::Program,target_bytes,target_prefix);grant(grants,6,Role::Maps,target_bytes,target_prefix);grant(grants,7,Role::Metrics,target_bytes,target_prefix);const PreparedAvailableRelationalProgram target(f.target,{5,6,7},Context(grants));
    const auto payload=relational_extension_payload(parent,target,f.declaration,RelationalExtensionStatus::Prospective);grant(grants,8,Role::Prospective,payload,prefix);
    const PreparedAvailableRelationalExtension model(parent,target,f.declaration,RelationalExtensionStatus::Prospective,8,Context(grants));const auto encoded=encode_available_relational_extension(model);const storage::TemporalReadLimits policy;
    check(encode_available_relational_extension(decode_available_relational_extension(encoded,Context(grants),policy))==encoded,"available complete extension parent/source cold replay");
    const auto row=input_submission(p,bytes,{1,2,3,4},10,grants);const auto family=model.execute_inputs({row},Context(grants));const auto family_bytes=encode_available_relational_extension_family(family);
    check(encode_available_relational_extension_family(decode_available_relational_extension_family(family_bytes,Context(grants),policy))==family_bytes,"actual old source input closure replayed before native full new family");
    const V free{2,-1,3,4,-2};grant(grants,20,Role::Prospective,relational_extension_completion_payload(family,free),prefix);const auto completed=family.evaluate(free,20,Context(grants));const auto completed_bytes=encode_available_relational_extension_evaluation(completed);
    check(encode_available_relational_extension_evaluation(decode_available_relational_extension_evaluation(completed_bytes,Context(grants),policy))==completed_bytes,"full prospective completion canonical replay");
    auto problem=RelationalFieldSolveDeclaration{};problem.key=1000;problem.coordinate_carrier=1001;problem.coordinate_basis=1002;problem.grade={1};problem.lambda=Q{1,2};
    RelationalSolveReadoutDeclaration readout;const auto target_pub=f.target.publication();const auto& target_source=target_pub.endpoint().source;
    for(std::size_t k=0;k<target_source.grade_count();++k){std::vector<std::uint32_t> cells;for(std::size_t i=0;i<target_source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count();++i)cells.push_back(static_cast<std::uint32_t>(i));readout.patch.push_back(std::move(cells));}
    readout.view.key=1003;readout.view.basis=1004;readout.view.accession=M::identity(9);readout.view.reconstruction=M::identity(9);for(unsigned i=0;i<9;++i)readout.view.directions.push_back(1010+i);
    const auto local_payload=relational_local_problem_payload(completed,1,problem);grant(grants,200,Role::Maps,local_payload,prefix);grant(grants,201,Role::Interface,local_payload,prefix);
    grant(grants,202,Role::Reconstruction,relational_local_readout_payload(completed,1,problem,readout),prefix);
    const PreparedAvailableRelationalLocalSolve local(completed,1,problem,readout,{200,201,202,0},Context(grants));const auto solved=local.execute(Context(grants));const auto local_bytes=encode_available_relational_local_result(solved);
    check(encode_available_relational_local_result(decode_available_relational_local_result(local_bytes,Context(grants),policy))==local_bytes,"available completed extension reaches actual owning local RHS and full preflight cold replay");
    const auto native_solved=solved.result();check(native_solved.extension_rhs().has_value(),"native local RHS retains complete extension family/projection/choice provenance");
    const auto actual_local=available_local_detail::rhs_program(local.rhs());check(actual_local.fibers().same_owner(f.target.fibers()),"local extension uses actual new whole program owner");
    for(const auto key:{8u,11u,20u,201u,202u})rejects_origin([&]{(void)decode_available_relational_local_result(local_bytes,withheld(grants,key),policy);},"local extension/held/completion/interface/readout closure before replay");
    runtime::HardwareInventory hardware;hardware.platform="test";hardware.affinity_observed=true;hardware.allowed_cpus.push_back({0,0,0,{}});runtime::CpuResourceCoordinator resources(hardware,{1,4096,100});
    auto job=relational_local_solve_job({300,1},{1,4096,1u<<20},completed,1,problem,readout,{200,201,202,0},Context(grants));auto run=runtime::WorkGraph({job}).submit(resources);run.wait();
    check(encode_available_relational_local_result(*run.completion({300,1}).result<AvailableRelationalLocalResult>())==local_bytes && resources.available_workers()==1,"actual joined extended whole-field local solve");
    for(const auto key:{1u,2u,3u,5u,6u,7u,8u,10u,11u,12u,13u,20u}){
        rejects_origin([&]{(void)completed.family().evaluate(free,20,withheld(grants,key));},"current extension/family/choice origin checked");
        rejects_origin([&]{(void)decode_available_relational_extension_evaluation(completed_bytes,withheld(grants,key),policy);},"whole cold extension closure before any replay");}
    auto invalid=row;invalid.input.declaration.accession=M(1,4);const auto ab=encode_relational_input_use_asset(invalid.input.asset);
    grant(grants,30,Role::InputAsset,ab,prefix);grant(grants,31,Role::InputValue,relational_input_value_payload(ab,invalid.input.declaration,invalid.input.use,invalid.input.source_field),prefix);
    const auto wrong=relational_input_accession_payload(pb,ab,invalid.input.declaration,invalid.input.use,invalid.input.source_field);grant(grants,32,Role::InputAccession,wrong,prefix);grant(grants,33,Role::Reconstruction,wrong,prefix);invalid.origins={30,31,32,33};
    rejects_origin([&]{(void)model.execute_inputs({invalid,row},withheld(grants,11));},"entire suffix admitted before invalid first A/R/source action");
    grant(grants,21,Role::InputValue,relational_extension_completion_payload(family,free),prefix);rejects_origin([&]{(void)family.evaluate(free,21,Context(grants));},"prospective completion cannot use InputValue role");
    const auto equal_row=input_submission(p,asset(false,9999),{1,2,3,4},100,grants);const auto equal_family=model.execute_inputs({equal_row},Context(grants));
    const auto native_family=family.family();const auto equal_native=equal_family.family();
    check(native_family.field(2).particular.values().dense()==equal_native.field(2).particular.values().dense() &&
        encode_available_relational_extension_family(equal_family)!=family_bytes,"equal numeric fields retain distinct original source-use/cut provenance");
    rejects_origin([&]{(void)equal_family.evaluate(free,20,Context(grants));},"input-specific completion cannot identify equal-valued source uses");
    const auto zero_row=input_submission(p,bytes,{0,0,0,0},110,grants);const auto zero_family=model.execute_inputs({zero_row},Context(grants));const auto zero_native=zero_family.family();
    check(zero_native.field(1).particular.values().nonzeros()==0 && zero_native.field(1).particular.support().count()==9 && zero_native.field(1).directions.cols()==5,
        "supported zero parent retains unresolved whole new complement");
    auto grafted=completed_bytes;grafted.back()^=std::byte{1};const auto fix_envelope=[&](Bytes& image){const auto hash=storage::rfcx_detail::fnv1a64(std::span<const std::byte>(image).subspan(32));
        for(unsigned i=0;i<8;++i)image[24+i]=static_cast<std::byte>((hash>>(i*8))&255u);};fix_envelope(grafted);
    rejects([&]{(void)decode_available_relational_extension_evaluation(grafted,Context(grants),policy);},"valid-envelope complete target field witness graft rejected");
    inference::field_program_codec_detail::Reader reader(encoded,policy);inference::field_program_codec_detail::header(reader,encoded,available_extension_codec_detail::model_magic);
    (void)reader.u8();(void)reader.blob();(void)reader.blob();const auto declaration_offset=reader.position();grafted=encoded;grafted[declaration_offset]^=std::byte{1};fix_envelope(grafted);
    rejects_origin([&]{(void)decode_available_relational_extension(grafted,Context(grants),policy);},"valid-envelope actual extension declaration graft preflights before any action");
    auto limited=numeric::ExactActionLimits{};limited.dimension=4;rejects([&]{(void)family.evaluate(free,20,Context(grants),limited);},"whole completion action bounded dimensions");
    limited={};limited.steps=1;rejects([&]{(void)model.execute_inputs({row},Context(grants),limited);},"whole native extension source/action steps bounded");
    limited={};limited.integer_bits=1;rejects([&]{(void)family.evaluate(free,20,Context(grants),limited);},"exact growth budget checked on complete completion");
    auto read_policy=policy;read_policy.max_file_bytes=32;rejects([&]{(void)decode_available_relational_extension_family(family_bytes,Context(grants),read_policy);},"available whole family byte cap before passive parse");
    const auto branch=fixture(f.target,false);auto next_d=branch.declaration;next_d.revision=2;const auto next_pb=encode_relational_field_program(branch.target);
    grant(grants,40,Role::Program,next_pb,target_prefix);grant(grants,41,Role::Maps,next_pb,target_prefix);grant(grants,42,Role::Metrics,next_pb,target_prefix);const PreparedAvailableRelationalProgram next_target(branch.target,{40,41,42},Context(grants));
    grant(grants,43,Role::Evidence,model.extension_payload(next_target,next_d,RelationalExtensionStatus::Observed),prefix);
    const auto next=model.extend(next_target,next_d,RelationalExtensionStatus::Observed,43,Context(grants));const auto next_bytes=encode_available_relational_extension(next);
    check(next.parent().has_value() && encode_available_relational_extension(decode_available_relational_extension(next_bytes,Context(grants),policy))==next_bytes,"complete observed/proposed extension lineage explicit actual parent");
    rejects_origin([&]{(void)decode_available_relational_extension(next_bytes,withheld(grants,8),policy);},"extended child retains prior prospective extension cutoff");
    std::optional<AvailableRelationalExtensionFamily> replace{family};bool released{};const auto escaped=replace->evaluate(free,20,Context(grants),{},[&]{if(!released){replace.reset();released=true;}});
    check(encode_available_relational_extension_evaluation(escaped)==completed_bytes,"callback release retains complete old and new field owners");
    std::vector<std::future<Bytes>> readers;for(unsigned i=0;i<6;++i)readers.push_back(std::async(std::launch::async,[family,free,grants]{return encode_available_relational_extension_evaluation(family.evaluate(free,20,Context(grants)));}));
    for(auto& r:readers)check(r.get()==completed_bytes,"six independent extension readers");
    const auto params=parameters();grant(grants,4,Role::Evidence,relational_evidence_root_payload(pb,params,{}),prefix);const Available root(parent,params,4,Context(grants));
    const auto training=published_input(parent,bytes,{1,0,0,0},50,grants);const auto observation=measurement(training,p,{1,0,0,0},71,60,grants);const auto learned=root.append({observation},Context(grants));
    const auto held=published_input(parent,asset(true,4444),{2,-1,3,1},70,grants);const auto old_family=learned.execute(held,Context(grants));const auto old_native=learned.evidence();V old_free(old_native.free_parameters().size(),Q{2});
    grant(grants,80,Role::Prospective,relational_parameter_setting_payload(learned,old_free),prefix);const auto selected=old_family.evaluate(old_free,80,Context(grants));const auto actual=selected.result().result().result().program();const auto selected_fixture=fixture(actual);
    const auto selected_pb=encode_relational_field_program(selected_fixture.target);grant(grants,81,Role::Program,selected_pb,target_prefix);grant(grants,82,Role::Maps,selected_pb,target_prefix);grant(grants,83,Role::Metrics,selected_pb,target_prefix);
    const PreparedAvailableRelationalProgram selected_target(selected_fixture.target,{81,82,83},Context(grants));grant(grants,84,Role::Prospective,relational_extension_payload(selected,selected_target,selected_fixture.declaration,RelationalExtensionStatus::Prospective),prefix);
    const PreparedAvailableRelationalExtension selected_model(selected,selected_target,selected_fixture.declaration,RelationalExtensionStatus::Prospective,84,Context(grants));
    const auto selected_input=input_submission(actual,asset(true,4445),{-2,4,3,1},90,grants);const auto selected_fields=selected_model.execute_inputs({selected_input},Context(grants));const auto selected_bytes=encode_available_relational_extension_family(selected_fields);
    check(encode_available_relational_extension_family(decode_available_relational_extension_family(selected_bytes,Context(grants),policy))==selected_bytes,"actual selected learned child and independent source uses preserved cold");
    const auto selected_native=selected_model.extension();const auto selected_source=selected_native.source();check(selected_source.fibers().same_owner(actual.fibers()),"selected extension extracts actual derived parent");
    rejects_origin([&]{(void)decode_available_relational_extension_family(selected_bytes,withheld(grants,61),policy);},"selected extension retains historical training target cutoff");
    rejects_origin([&]{(void)decode_available_relational_extension_family(selected_bytes,withheld(grants,80),policy);},"selected extension retains global parameter choice cutoff");
    rejects([&]{OwnedRelationalFieldExtension wrong_root(p,selected_fixture.target,selected_fixture.declaration);},"selected learned maps cannot be reassigned to old root owner");
    const auto independent=authority(authority_bytes(grants));
    check(encode_available_relational_extension_evaluation(decode_available_relational_extension_evaluation(completed_bytes,independent,policy))==completed_bytes,"cold independent publisher adoption retains complete source relationship");
    rejects([&]{(void)completed.family().evaluate(free,20,independent);},"live equal-receipt publishers do not replace actual owners");
    if(!export_dir.empty()){std::filesystem::create_directories(export_dir);write_file(export_dir/"extension.packet",completed_bytes);write_file(export_dir/"local.packet",local_bytes);write_file(export_dir/"authority.bin",authority_bytes(grants));}

}
} // namespace
int main(int argc,char** argv){try{if(argc==4 && std::string_view(argv[1])=="--serve"){
    const auto bytes=read_file(argv[2]);const auto context=authority(read_file(argv[3]));const auto result=decode_available_relational_extension_evaluation(bytes,context,storage::TemporalReadLimits{});
    check(encode_available_relational_extension_evaluation(result)==bytes,"standalone complete extension replay without model file");std::cout<<"served extension "<<bytes.size()<<" bytes\n";return 0;}
    if(argc==4 && std::string_view(argv[1])=="--serve-local"){
    const auto bytes=read_file(argv[2]);const auto context=authority(read_file(argv[3]));const auto result=decode_available_relational_local_result(bytes,context,storage::TemporalReadLimits{});
    check(encode_available_relational_local_result(result)==bytes,"standalone complete extension/local replay without model file");std::cout<<"served local "<<bytes.size()<<" bytes\n";return 0;}
    const bool emit=argc==2 && std::string_view(argv[1])=="--json";raw(emit);if(!emit){filling_counterexample();available(argc==3 && std::string_view(argv[1])=="--export"?std::filesystem::path(argv[2]):std::filesystem::path{});std::cout<<checks<<" checks\n";}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
