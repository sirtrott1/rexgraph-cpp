#include <numeric>
#include "rfc/inference/relational_comparison_forms_codec.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/relational_channels.hpp"
#include <future>
#include <iostream>
#include <string_view>
using namespace rfc;
using namespace rfc::inference;
using Q=numeric::Rational;using V=numeric::RationalVector;using M=numeric::ExactMatrix;using T=numeric::SparseRationalTensor;
using Program=OwnedRelationalFieldProgram;using Declaration=RelationalFieldProgramDeclaration;
using Channel=RelationalComparisonChannel;using Participation=RelationalComparisonParticipation;using Forms=PreparedRelationalComparisonForms;
namespace {
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool caught{};try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
T tensor(const V& v){numeric::ExactActionGuard guard;return field_interface_detail::tensor(v,guard);}
storage::NativeSeriesEndpoint source() {
    core::RelationSignature signature;
    const auto relation = signature.add_type("relation", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, relation,
        {{10, true, false, Q{1}}, {20, true, true, Q{1}}, {10, true, true, Q{1}},
         {20, false, false, Q{1}}, {30, true, false, Q{0}}});
    builder.add_grade_one(core::RelationKey{101}, relation,
        {{10, true, false, Q{1}}, {20, true, false, Q{1, 2}}, {30, true, true, Q{1}}});
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

Declaration phi_declaration(){auto d=declaration();d.occurrences[7].role=storage::TemporalTensorRole::CoefficientIndependentMap;d.occurrences[7].map=d.occurrences[7].map.scale(-1);return d;}
RelationalComparisonDeclaration comparison(bool count=false){return {1,1,core::Grade{1},{{{core::Grade{1},0},Q{2,3}},{{core::Grade{1},1},Q{-3,2}}},count?Participation::LegacyCount:Participation::Share};}
M metric(const Declaration& d,core::Grade grade){numeric::ExactActionGuard guard;std::vector<StalkBasisDeclaration> blocks;for(const auto& s:d.stalks)if(s.cell.grade==grade)blocks.push_back(s);return field_interface_detail::block_metric(blocks,guard);}
void corpus(bool emit){
    const auto p=source();const auto d=phi_declaration();const Program program(p,d);const auto N=metric(d,core::Grade{1});
    for(const bool count:{false,true}){const Forms forms(program,comparison(count));
        const auto archive=encode_relational_comparison_forms(forms);check(encode_relational_comparison_forms(decode_relational_comparison_forms(archive,{}))==archive,"complete form canonical bytes");
        for(std::size_t trial=0;trial<20;++trial){V x{Q{static_cast<long>(trial)-4,3},Q{static_cast<long>(trial%7)-2,5},Q{static_cast<long>(trial%11)-3,7},Q{static_cast<long>(trial%13)-6,2}};for(auto& q:x)q.canonicalize();
            for(const auto channel:{Channel::T,Channel::RawG,Channel::C,Channel::F}){
                const auto result=forms.apply(channel,tensor(x));const auto action=forms.operator_matrix(channel,16);const auto gram=N.multiply(action);
                check(action.apply(x)==result.output().values().dense(),"native factored action equals declared basis action");
                check(gram==gram.transpose(),"exact metric self adjoint complete action");
                check(result.quadrance()>=0,"squared norm form nonnegative");
                check(result.occurrence_fields().size()==8&&result.occurrence_fields()[3].values().nonzeros()>0&&result.occurrence_fields()[4].support().count()==2,"inactive/zero intrinsic flag fields retained");
                Q pair_energy{};
                if(channel==Channel::C||channel==Channel::F){const auto& arena=p.endpoint().source.grade(core::Grade{1});for(std::size_t a=0;a<8;++a)for(std::size_t b=a+1;b<8;++b)if(arena.lower_index[a]==arena.lower_index[b]){const auto pair=result.pair_residual(a,b,channel);pair_energy+=pair.quadrance();}}
                if(channel==Channel::C||channel==Channel::F)check(pair_energy==result.quadrance(),"native original pair and factored full quadrance");
                const auto image=encode_relational_comparison_result(result);const auto cold=decode_relational_comparison_result(image,{});check(encode_relational_comparison_result(cold)==image,"cold complete comparison intermediary replay");
                if(emit){std::cout<<"{\"input\":";json(x);std::cout<<",\"count\":"<<(count?"true":"false")<<",\"channel\":"<<static_cast<int>(channel)<<",\"weights\":[\"2/3\",\"-3/2\"],\"upper_metric\":";json(N);
                    std::cout<<",\"lower_metrics\":[";for(std::size_t i=0;i<3;++i){if(i)std::cout<<',';json(d.stalks[i].metric);}std::cout<<"],\"flags\":[";const auto& arena=p.endpoint().source.grade(core::Grade{1});
                    for(std::size_t i=0;i<8;++i){if(i)std::cout<<',';std::cout<<"{\"upper\":"<<arena.upper_index[i]<<",\"lower\":"<<arena.lower_index[i]<<",\"coefficient\":\""<<(arena.aspects.get(p.endpoint().source.signature().existence_aspect(),i)?p.endpoint().source.incidence_coefficient(core::Grade{1},i):Q{0})<<"\",\"phi\":";json(d.occurrences[i].map);std::cout<<'}';}
                    std::cout<<"],\"output\":";json(result.output().values().dense());std::cout<<",\"signed\":";json(result.signed_boundary_field().values().dense());std::cout<<",\"unsigned\":";json(result.unsigned_boundary_field().values().dense());std::cout<<",\"quadrance\":\""<<result.quadrance()<<"\"}\n";}
            }
        }
    }
}
void covariance(){const auto p=source();const auto d=phi_declaration();const Program original(p,d);auto framed=d;M upper(4,4),lower(7,7);std::size_t up{},lo{};
    for(auto& s:framed.stalks){auto S=M::identity(s.directions.size());S.set(0,1,1);const auto R=S.inverse();s.basis+=10000;s.metric=R.transpose().multiply(s.metric).multiply(R);auto& aggregate=s.cell.grade.value?upper:lower;auto& offset=s.cell.grade.value?up:lo;for(const auto& [ij,q]:S.entries())aggregate.set(offset+ij.first,offset+ij.second,q);offset+=S.rows();}
    const auto& arena=p.endpoint().source.grade(core::Grade{1});for(std::size_t i=0;i<8;++i){auto S=M::identity(d.occurrences[i].map.rows());S.set(0,1,1);auto R=M::identity(2);R.set(0,1,-1);framed.occurrences[i].map=S.multiply(d.occurrences[i].map).multiply(R);}framed.nodes[2].map=upper.multiply(framed.nodes[2].map).multiply(lower.inverse());const Program changed(p,framed);
    for(const bool count:{false,true})for(const auto channel:{Channel::T,Channel::RawG,Channel::C,Channel::F}){const Forms a(original,comparison(count)),b(changed,comparison(count));const V x{2,3,5,7};const auto old=a.apply(channel,tensor(x)),next=b.apply(channel,tensor(upper.apply(x)));
        check(next.output().values().dense()==upper.apply(old.output().values().dense())&&old.quadrance()==next.quadrance(),"whole field metric/basis covariance");
        check(next.signed_boundary_field().values().dense()==lower.apply(old.signed_boundary_field().values().dense()),"common boundary source basis covariance");
        for(std::size_t i=0;i<8;++i){auto S=M::identity(old.occurrence_fields()[i].domain().logical_size());S.set(0,1,1);check(next.occurrence_fields()[i].values().dense()==S.apply(old.occurrence_fields()[i].values().dense()),"every original inactive/zero flag basis covariance");}
        (void)arena;
    }
}
void admissions(){const auto p=source();const auto d=phi_declaration();const Program program(p,d);const Forms forms(program,comparison());auto missing=d;missing.occurrences[3].role=storage::TemporalTensorRole::RealizedIncidenceMap;missing.occurrences[3].map=M(3,2);const Program omitted(p,missing);
    rejects([&]{Forms invalid(omitted,comparison());},"inactive zero T cannot infer Phi");auto bad=comparison();bad.weights[0].cell.index=1;rejects([&]{Forms invalid(program,bad);},"actual relation weight identity/order");
    const T partial(numeric::IndexDomain({4}),numeric::StructuralSupport::sparse(4,{0}),numeric::SparseRationalVector(4,{{0,1}}));rejects([&]{forms.apply(Channel::C,partial);},"absent source directions not zero filled");
    const auto result=forms.apply(Channel::C,tensor({2,3,5,7}));rejects([&]{result.pair_residual(0,1,Channel::C);},"different actual lower interfaces cannot compare");
    const auto repeat=result.pair_residual(0,2,Channel::C);check(repeat.residual().values().nonzeros()==0,"equal repeated flag fields remain original pair");const auto inactive=result.pair_residual(1,3,Channel::C);check(inactive.weight()==0&&inactive.residual().values().nonzeros()>0,"inactive intrinsic data distinct from pair participation");
    rejects([&]{forms.operator_matrix(Channel::C,1);},"bounded action matrix inspection");auto limits=numeric::ExactActionLimits{};limits.entries=1;rejects([&]{forms.apply(Channel::C,tensor({2,3,5,7}),limits);},"resource admission applies current owner");rejects([&]{forms.apply(Channel::F,tensor({2,3,5,7}),{},[]{throw std::runtime_error("cancel");});},"comparison cancellation");
    const auto archive=encode_relational_comparison_result(result);auto broken=archive;broken.back()^=std::byte{1};rejects([&]{decode_relational_comparison_result(broken,{});},"comparison archive integrity");
    using namespace relational_comparison_codec_detail;auto w=envelope(comparison_magic);blob(w,encode_relational_comparison_forms(forms));w.u8(static_cast<std::uint8_t>(Channel::C));storage::temporal_codec_detail::tensor(w,result.input());storage::temporal_codec_detail::tensor(w,tensor(V(4)));storage::temporal_codec_detail::tensor(w,result.signed_boundary_field());storage::temporal_codec_detail::tensor(w,result.unsigned_boundary_field());w.u64(8);for(const auto& f:result.occurrence_fields())storage::temporal_codec_detail::tensor(w,f);storage::temporal_codec_detail::rational(w,result.quadrance());const auto false_witness=finish(std::move(w));rejects([&]{decode_relational_comparison_result(false_witness,{});},"valid envelope cannot certify false complete action field");
    std::vector<std::future<V>> jobs;for(std::size_t i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[forms]{const auto value=forms.apply(Channel::F,tensor({2,3,5,7}));return value.output().values().dense();}));const auto expected=forms.apply(Channel::F,tensor({2,3,5,7}));for(auto& job:jobs)check(job.get()==expected.output().values().dense(),"independent concurrent exact factored actions");
}
void scalar_reduction(){
    for(std::size_t seed=0;seed<32;++seed){core::RelationSignature signature;const auto type=signature.add_type("channel",core::BoundaryPolicy::DeclaredExact,0,8);core::RelationalComplexBuilder builder(signature);builder.declare_grade(core::Grade{2});
        for(std::size_t e=0;e<5;++e){std::vector<core::BoundaryOccurrenceInput> flags;for(std::size_t v=0;v<4;++v)flags.emplace_back(v,(seed+v+e)%6!=0,(seed+v+2*e)%3==0,Q{static_cast<unsigned long>((seed+3*v+5*e)%7),static_cast<unsigned long>(1+(seed+v+e)%4)});builder.add_grade_one(core::RelationKey{100+e},type,flags);}
        const auto source=builder.freeze();storage::TemporalEndpoint endpoint(source,1);const storage::NativeTemporalSeries history(endpoint,123);Declaration d;d.key=1;d.revision=1;d.differential=true;
        for(std::size_t k=0;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};for(std::size_t i=0;i<source.grade(g).cell_count();++i)d.stalks.push_back({{g,static_cast<std::uint32_t>(i)},1,{1},M::identity(1)});if(k)for(std::size_t i=0;i<source.grade(g).incidence_count();++i)d.occurrences.push_back({g,i,storage::TemporalTensorRole::CoefficientIndependentMap,M::identity(1)});}
        d.nodes={{1,FieldProgramNodeKind::Input,core::Grade{1},{},{}}};const Program program(history.tail(),d);V weights(5),x(5);RelationalComparisonDeclaration declaration;declaration.key=1;declaration.revision=1;declaration.grade={1};
        for(std::size_t e=0;e<5;++e){weights[e]=Q{static_cast<long>((seed+e)%5)-2,3};weights[e].canonicalize();x[e]=Q{static_cast<long>((seed+2*e)%7)-3,5};x[e].canonicalize();declaration.weights.push_back({{core::Grade{1},static_cast<std::uint32_t>(e)},weights[e]});}
        for(const bool count:{false,true}){declaration.participation=count?Participation::LegacyCount:Participation::Share;const Forms forms(program,declaration);const field::PreparedRelationalChannels legacy(source,weights,count?field::CoparticipationReading::Count:field::CoparticipationReading::Share);const auto reference=legacy.apply_all(x);
            for(const auto channel:{Channel::T,Channel::RawG,Channel::C,Channel::F}){const auto result=forms.apply(channel,tensor(x));const auto index=channel==Channel::C?3u:channel==Channel::F?2u:static_cast<unsigned>(channel);check(result.output().values().dense()==reference.fields[index],"native heterogeneous forms reduce to unchanged scalar channels");}
        }
    }
}

}
int main(int argc,char** argv){try{const bool emit=argc>1&&std::string_view(argv[1])=="--emit";corpus(emit);covariance();admissions();scalar_reduction();if(!emit)std::cout<<"relational_comparison_checks="<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<"comparison failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
