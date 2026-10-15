#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_family_gluing.hpp"
#include <iostream>
#include <numeric>
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Fit=learning::PreparedGradedTransportFit;
using Glue=learning::PreparedTransportFamilyGluing;
using Status=learning::TransportParameterRelationStatus;
namespace {
std::size_t checks{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,why);}
core::RelationalComplex source() {
    core::RelationSignature signature;auto type=signature.add_type("boundary",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(type,{{10,true,true,1},{20,true,false,1}});
    builder.add_grade_one(type,{{20,true,true,1},{30,true,false,1}});
    builder.add_grade_one(type,{{10,true,true,1},{30,true,false,1}});
    builder.add_higher(core::Grade{2},type,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    return builder.freeze();
}
Fit fit(std::size_t selected,std::size_t dimension,Guard& guard,bool fixed_selected=false) {
    const auto c=source();local::LinearStalkBundle stalks(c,dimension);local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> fixed,variables;
    for(std::size_t k=1;k<c.grade_count();++k) {
        const core::Grade g{static_cast<std::uint16_t>(k)};
        for(std::size_t p=0;p<c.grade(g).incidence_count();++p) {
            M map(dimension,dimension);
            if(k!=selected || fixed_selected)for(std::size_t i=0;i<dimension;++i)map.set(i,i,c.incidence_coefficient(g,p));
            if(k==selected)for(std::size_t i=0;i<dimension;++i)for(std::size_t j=0;j<dimension;++j)
                (fixed_selected?fixed:variables).push_back({p,i,j});
            fibers.set_transport(g,p,std::move(map));
        }
    }
    auto observation=learning::PreparedTransportObservations::at_grade(local::OwnedLinearFiberSystem(fibers),
        core::Grade{static_cast<std::uint16_t>(selected)},fixed,variables,guard);
    return Fit(observation,{},guard);
}
learning::TransportFamilyInterface interface(const std::vector<Fit>& members,std::size_t left,std::size_t right,std::uint64_t key=1,bool flipped=false) {
    learning::TransportFamilyInterface out{key,1,left,right,source(),{},{},{},{}};
    const auto count=out.common.grade_count();out.common_left.cells.resize(count);out.common_left.occurrences.resize(count);
    out.common_right.cells.resize(count);out.common_right.occurrences.resize(count);out.left_views.resize(count);out.right_views.resize(count);
    for(std::size_t k=0;k<count;++k) {
        const core::Grade g{static_cast<std::uint16_t>(k)};const auto& a=out.common.grade(g);
        out.common_left.cells[k].resize(a.cell_count());std::iota(out.common_left.cells[k].begin(),out.common_left.cells[k].end(),0);
        out.common_left.occurrences[k].resize(a.incidence_count());std::iota(out.common_left.occurrences[k].begin(),out.common_left.occurrences[k].end(),0);
        out.common_right.cells[k]=out.common_left.cells[k];out.common_right.occurrences[k]=out.common_left.occurrences[k];
        for(std::size_t i=0;i<a.cell_count();++i) {
            const auto l=members[left].model().owner().fibers().stalks().dimension({g,static_cast<std::uint32_t>(i)});
            const auto r=members[right].model().owner().fibers().stalks().dimension({g,static_cast<std::uint32_t>(i)});
            const auto common=std::max(l,r);
            const auto view=[&](std::size_t n,bool flip){M accession(common,n),reconstruction(n,common);
                for(std::size_t j=0;j<n;++j){accession.set(j,j,flip?-1:1);reconstruction.set(j,j,flip?-1:1);}
                return learning::TransportContextView{1+100*k+i,std::move(accession),std::move(reconstruction)};};
            out.left_views[k].push_back(view(l,flipped && k==2));out.right_views[k].push_back(view(r,false));
        }
    }
    return out;
}
void same_grade() {
    Guard g;std::vector<Fit> members{fit(2,1,g),fit(2,1,g)};auto declaration=interface(members,0,1);
    Glue glue(members,{declaration},g);
    check(glue.consistent() && glue.kernel().rows()==6 && glue.kernel().cols()==1,"full higher-grade glued family differs");
    check(glue.members()[0].model().owner().same_owner(members[0].model().owner()),"source owner lost");
    check(glue.interfaces()[0].common_left.occurrences[2].size()==3,"retained higher occurrences lost");
    check(glue.classify({{0,0,1},{1,0,-1}},0,g).status==Status::Forced,"glued equality not forced");
    check(glue.classify({{0,0,1}},0,g).status==Status::Open,"unresolved common parameter fixed silently");
    check(glue.classify({{0,0,1},{1,0,-1}},1,g).status==Status::Excluded,"excluded claim not detected");
    for(int t=-2;t<=2;++t) {
        const auto realized=glue.materialize({Q{t,3}},g);
        check(local::GradeTensorField(realized[0].fibers(),core::Grade{2}).matrix()==
              local::GradeTensorField(realized[1].fibers(),core::Grade{2}).matrix(),"materialized interface differs");
        const auto parameters=glue.parameters({Q{t,3}},g);check(parameters[0]==parameters[3],"original parameter agreement differs");
    }
    declaration.common_left.cells[0][0]=99;
    check(glue.interfaces()[0].common_left.cells[0][0]==0,"caller mutation changed retained declaration");
    Glue independent(members,{},g);check(independent.kernel().cols()==2,"empty interface removed independent directions");
}
void mixed_grade_and_rectangular() {
    Guard g;std::vector<Fit> mixed{fit(1,1,g),fit(2,1,g)};Glue together(mixed,{interface(mixed,0,1)},g);
    check(together.consistent() && together.kernel().cols()==0,"different selected grades failed to glue");
    const auto realized=together.materialize({},g);
    for(std::size_t k=1;k<=2;++k)check(local::GradeTensorField(realized[0].fibers(),core::Grade{static_cast<std::uint16_t>(k)}).matrix()==
        local::GradeTensorField(realized[1].fibers(),core::Grade{static_cast<std::uint16_t>(k)}).matrix(),"mixed-grade realized response differs");
    std::vector<Fit> rectangular{fit(2,1,g),fit(2,2,g)};Glue partial(rectangular,{interface(rectangular,0,1)},g);
    check(partial.consistent() && partial.kernel().cols()==3,"unshared stalk input directions were removed");
    for(int t=0;t<=2;++t) {
        const auto fields=partial.materialize(V(3,Q{t,5}),g);
        const auto& large=fields[1].fibers();const auto& small=fields[0].fibers();
        for(std::size_t p=0;p<3;++p) {
            const auto& a=*small.transport_override(core::Grade{2},p);const auto& b=*large.transport_override(core::Grade{2},p);
            check(b.get(0,0)==a.get(0,0) && b.get(1,0)==0,"common input response not fully compared");
        }
    }
}
void joint_contradiction() {
    Guard g;std::vector<Fit> members{fit(2,1,g,true),fit(2,1,g),fit(2,1,g)};
    Glue glue(members,{interface(members,0,1,1),interface(members,1,2,2),interface(members,2,0,3,true)},g);
    check(!glue.consistent(),"joint signed interface contradiction accepted");
    check(std::none_of(glue.statuses().begin(),glue.statuses().end(),[](auto s){return s==Status::Excluded;}),
        "individual equations were not feasible before joint conditioning");
    check(!glue.contradiction().empty() && numeric::is_zero(glue.equations().transpose().apply(glue.contradiction())),"left-null contradiction certificate invalid");
    check(numeric::dot(glue.contradiction(),glue.residual_origin())!=0,"contradiction witness pairs to zero");
    rejects([&]{(void)glue.materialize({},g);},"inconsistent family materialized");
}
void sparse_redundant_carrier() {
    Guard g;std::vector<Fit> members{fit(2,2,g,true),fit(2,2,g,true)};
    auto common=interface(members,0,1);
    for(auto* side:{&common.left_views,&common.right_views})for(auto& grade:*side)for(auto& view:grade) {
        M accession(256,2),reconstruction(2,256);
        accession.set(0,0,1);accession.set(255,1,1);reconstruction.set(0,0,1);reconstruction.set(1,255,1);
        view.accession=std::move(accession);view.reconstruction=std::move(reconstruction);
    }
    Glue glue(members,{common},g);
    check(glue.consistent() && glue.equations().rows()==0,"tautological zero equation rectangle was retained");
    check(glue.witnesses().size()==9 && std::all_of(glue.witnesses().begin(),glue.witnesses().end(),
        [](const auto& witness){return witness.output_dimension==256 && witness.input.common_input.cols()==2;}),
        "omitting zero equations lost original interface dimensions or occurrences");
    check(glue.materialize({},g).size()==2,"sparse padded-carrier agreement failed to materialize");
}
void refusals() {
    Guard g;std::vector<Fit> members{fit(2,1,g),fit(2,1,g)};
    auto bad=interface(members,0,1);bad.common_left.occurrences[2][0]=1;
    rejects([&]{Guard h;Glue x(members,{bad},h);},"occurrence type/sign/ownership change accepted");
    bad=interface(members,0,1);bad.left_views.pop_back();
    rejects([&]{Guard h;Glue x(members,{bad},h);},"omitted upper grade accepted");
    bad=interface(members,0,1);bad.left_views[2][0].reconstruction.set(0,0,0);
    rejects([&]{Guard h;Glue x(members,{bad},h);},"nonreconstructive interface accepted");
    const auto valid=interface(members,0,1);
    rejects([&]{Guard h;Glue x(members,{valid,valid},h);},"duplicate interface identity accepted");
    rejects([&]{Guard h({},[]{throw std::runtime_error("cancel");});Glue x(members,{valid},h);},"cancellation ignored");
    rejects([&]{auto limits=numeric::ExactActionLimits{};limits.entries=1;Guard h(limits);Glue x(members,{valid},h);},"entry budget ignored");
}
}
int main(){try{same_grade();mixed_grade_and_rectangular();joint_contradiction();sparse_redundant_carrier();refusals();std::cout<<checks<<" transport family gluing checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
