#pragma once

#include "rfc/learning/relational_field_evidence.hpp"
#include "rfc/inference/relational_available_tensor_square.hpp"
#include <array>
#include <tuple>

namespace rfc::learning {
struct RelationalMomentRolePair {
    core::CellId relation;
    std::uint32_t head{}, dependent{};
    std::uint64_t head_flag{}, dependent_flag{};
    std::array<std::size_t,2> rows;
};
struct RelationalFourUseMomentBasis {
    inference::OwnedRelationalFieldProgram program;
    RelationalFieldParameterDeclaration parameters;
    std::vector<numeric::ExactMatrix> readings, base_reconstructions, base_metrics;
};
// This is a declared observation/model restriction on the full owned input.
// The base inverse reconstructs Gamma68, not an unrestricted4692 carrier.
inline RelationalFourUseMomentBasis make_relational_four_use_moment_basis(
    inference::OwnedRelationalFieldProgram program, inference::OwnedRelationalKnownTensorSquare reference,
    std::uint64_t key,std::uint64_t revision,std::uint64_t node,std::vector<std::uint64_t> parameters,
    std::vector<RelationalMomentRolePair> pairs,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
    using namespace relational_evidence_detail;
    const auto p=program;const auto square=reference;const auto frozen=pairs;const auto names=parameters;Guard guard(limits,std::move(check));
    const auto publication=p.publication();const auto& source=publication.endpoint().source;
    require(source.grade_count()==2&&source.grade({0}).cell_count()==4&&source.grade({1}).cell_count()==13&&
        key&&revision&&frozen.size()==12&&names.size()==7208,"four-use moment basis requires complete declared source/pair/parameter carriers");
    std::set<std::uint64_t> unique_names;for(const auto name:names){guard.step();require(name&&unique_names.insert(name).second,"moment parameter identities are absent or repeated");}
    const auto& selected=inference::field_interface_detail::node(p,node);
    require(selected.kind==inference::FieldProgramNodeKind::Boundary&&selected.output_grade==core::Grade{0}&&selected.input,
        "moment parameters require the actual lower Boundary node");
    const auto& predecessor=inference::field_interface_detail::node(p,*selected.input);
    require(predecessor.kind==inference::FieldProgramNodeKind::Input&&predecessor.output_grade==core::Grade{1},
        "moment parameters require the complete declared grade-one input");
    require(square.original_stalk().directions.size()==68&&square.lifted_field().domain().logical_size()==4692,
        "moment basis requires complete known original68 and ordered square");
    auto directions=square.original_stalk().directions;directions.insert(directions.end(),square.declaration().pair_directions.begin(),square.declaration().pair_directions.end());
    for(std::uint32_t i=0;i<13;++i){const auto& s=inference::field_interface_detail::stalk(p,{{1},i});
        require(s.basis==square.declaration().lifted_basis&&s.directions==directions&&s.metric==square.metric(),"moment role source changes actual lifted basis/order/metric");}
    std::set<std::pair<std::uint32_t,std::uint32_t>> seen;std::set<std::uint32_t> relations;
    using Triple=std::tuple<std::size_t,std::size_t,std::size_t>;
    std::map<Triple,std::size_t> variables;
    RelationalFieldParameterDeclaration d{key,revision,node,{},numeric::ExactMatrix(0,7208),names};
    std::vector<std::tuple<std::size_t,std::size_t,numeric::Rational>> coefficients;
    RelationalFourUseMomentBasis out{p,d,{},{},{}};
    const auto& arena=source.grade({1});
    for(const auto& pair:frozen){guard.step();require(pair.head<4&&pair.dependent<4&&pair.head!=pair.dependent&&pair.relation.grade==core::Grade{1}&&
        pair.relation.index>0&&pair.relation.index<13&&seen.emplace(pair.head,pair.dependent).second&&relations.insert(pair.relation.index).second,
        "moment basis changes complete actual directed pair identities");
        const auto [begin,end]=arena.incidence_range(pair.relation.index);
        require(end-begin==2&&arena.lower_index[begin]==pair.head&&arena.lower_index[begin+1]==pair.dependent&&
            arena.occurrence_identities[begin].value==pair.head_flag&&arena.occurrence_identities[begin+1].value==pair.dependent_flag,
            "moment basis changes actual head/dependent ordered flag interface");
        require(arena.lattice_coefficient[begin]==1&&arena.lattice_coefficient[begin+1]==1,
            "moment role profile requires actual positive unit pair participation");
        const auto& lower=inference::field_interface_detail::stalk(p,{{0},pair.dependent});
        const auto rank=pair.head-(pair.dependent<pair.head?1u:0u);
        require(lower.directions.size()==23&&pair.rows[0]==17+2*rank&&pair.rows[1]==18+2*rank&&
            lower.directions[pair.rows[0]]==10000+100*pair.head+10*pair.dependent&&
            lower.directions[pair.rows[1]]==10001+100*pair.head+10*pair.dependent,"moment role rows change actual declared incoming-flag directions");
        std::vector<std::uint32_t> others;for(std::uint32_t i=0;i<4;++i)if(i!=pair.head&&i!=pair.dependent)others.push_back(i);
        const long u=static_cast<long>(others[0])-static_cast<long>(pair.head),v=static_cast<long>(others[1])-static_cast<long>(pair.head);
        numeric::ExactMatrix t(4,4),r(4,4);t.set(0,pair.head,1);t.set(1,pair.dependent,1);
        t.set(2,others[0],1);t.set(2,others[1],1);t.set(3,others[0],u);t.set(3,others[1],v);
        r.set(pair.head,0,1);r.set(pair.dependent,1,1);r.set(others[0],2,numeric::Rational(v,v-u));r.set(others[0],3,numeric::Rational(-1,v-u));
        r.set(others[1],2,numeric::Rational(-u,v-u));r.set(others[1],3,numeric::Rational(1,v-u));
        require(numeric::checked_multiply(r,t,guard)==numeric::ExactMatrix::identity(4)&&numeric::checked_multiply(t,r,guard)==numeric::ExactMatrix::identity(4),"moment source-conditioned inverse failed");
        numeric::ExactMatrix q(1802,4692),back(68,68);
        for(const auto& [ij,value]:t.entries())for(std::size_t tag=0;tag<17;++tag)q.set(17*ij.first+tag,17*ij.second+tag,value);
        for(const auto& [ij,value]:r.entries())for(std::size_t tag=0;tag<17;++tag)back.set(17*ij.first+tag,17*ij.second+tag,value);
        std::size_t block{};
        for(std::size_t left=0;left<4;++left)for(std::size_t right=left+1;right<4;++right){
            for(const auto& [ij,a]:t.entries())if(ij.first==left)for(const auto& [kl,b]:t.entries())if(kl.first==right)
                for(std::size_t x=0;x<17;++x)for(std::size_t y=0;y<17;++y)q.set(68+289*block+17*x+y,68+(17*ij.second+x)*68+17*kl.second+y,guard.mul(a,b));
            ++block;
        }
        const auto metric=numeric::checked_multiply(back.transpose(),numeric::checked_multiply(square.original_stalk().metric,back,guard),guard);
        const auto order=static_cast<std::size_t>(pair.head>pair.dependent);
        for(std::size_t role=0;role<2;++role)for(const auto& [ij,value]:q.entries()){
            guard.step();const Triple triple{begin+1,pair.rows[role],ij.second};
            auto [where,inserted]=variables.emplace(triple,d.variables.size());
            if(inserted)d.variables.push_back({begin+1,pair.rows[role],ij.second});
            coefficients.emplace_back(where->second,(2*order+role)*1802+ij.first,value);
            guard.entries(machine::checked_size_add(d.variables.size(),coefficients.size()));
        }
        out.readings.push_back(std::move(q));out.base_reconstructions.push_back(std::move(back));out.base_metrics.push_back(metric);
    }
    d.basis=numeric::ExactMatrix(d.variables.size(),7208);
    for(const auto& [row,col,value]:coefficients)d.basis.set(row,col,guard.add(d.basis.get(row,col),value));
    guard.matrix(d.basis);out.parameters=std::move(d);return out;
}
namespace four_use_grammar_detail {
// One validator for warm submissions and passively parsed original fields.
// No source, accession, metric solve, product or fitted object is constructed.
inline void require_image(const inference::RelationalInputAccessionDeclaration& declaration,
    const numeric::SparseRationalTensor& field,const std::function<void()>& check={}) {
    using inference::field_program_detail::require;
    require(declaration.source_grade==core::Grade{0}&&declaration.source_stalks.size()==4&&
        field.domain().extents()==std::vector<std::size_t>{68}&&field.support().count()==68,
        "teacher grammar needs complete original four17-coordinate source stalks");
    std::vector<std::uint64_t> directions;for(std::uint64_t i=1;i<=17;++i)directions.push_back(i);
    for(std::size_t i=0;i<4;++i)require(declaration.source_stalks[i].cell==core::CellId{{0},static_cast<std::uint32_t>(i)}&&
        declaration.source_stalks[i].basis==701&&declaration.source_stalks[i].directions==directions&&
        declaration.source_stalks[i].metric==numeric::ExactMatrix::identity(17),"teacher grammar source stalk basis/order/metric differs");
    std::array<std::size_t,4> counts{};
    field.values().for_each_nonzero([&](std::size_t index,const auto& value){if(check)check();require(value==1&&index<68,"teacher grammar field outside declared onehot image");++counts[index/17];});
    for(const auto n:counts)require(n==1,"teacher grammar needs one actual available observation per original stalk");
}
} // namespace four_use_grammar_detail
// Authority first, finite image second, before A/R or known multiplication.
inline void require_relational_four_use_grammar_image(inference::PreparedAvailableRelationalProgram program,
    inference::RelationalInputSubmission submission,inference::RelationalInputOrigins origins,
    const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) {
    const auto p=program;const auto frozen=std::move(submission);const auto authority=context;const auto callback=std::move(check);
    p.check_inputs({{frozen,origins}},authority,callback);
    four_use_grammar_detail::require_image(frozen.declaration,frozen.source_field,callback);
}
} // namespace rfc::learning
