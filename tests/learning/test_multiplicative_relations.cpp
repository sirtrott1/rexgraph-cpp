#include "rfc/learning/multiplicative_relations.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using System=learning::PreparedMultiplicativeRelations;
using Ratio=learning::RetainedRatio;
std::size_t checks{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F fn,const char* why){bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit,why);}
std::shared_ptr<const core::RelationalComplex> fixture(){
    core::RelationSignature signature;const auto pair=signature.add_type("equation",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto joint=signature.add_type("syzygy",core::BoundaryPolicy::DeclaredExact,3,3);
    core::RelationalComplexBuilder builder(signature);
    for(std::uint64_t i=0;i<8;++i)builder.declare_witness_identity(100+i);
    const std::pair<std::uint64_t,std::uint64_t> endpoints[]{{0,1},{1,2},{0,2},{3,4},{6,6},{7,7}};
    for(std::size_t i=0;i<6;++i)builder.add_grade_one(core::RelationKey{200+i},pair,
        {{100+endpoints[i].first,true,true,Q{1},{},core::OccurrenceKey{300+2*i}},
         {100+endpoints[i].second,true,false,Q{1},{},core::OccurrenceKey{301+2*i}}});
    builder.add_higher(core::Grade{2},core::RelationKey{400},joint,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
    return std::make_shared<const core::RelationalComplex>(builder.freeze());
}
std::vector<Ratio> ratios(Q closure){return {{{200},2},{{201},3},{{202},std::move(closure)},{{203},-2},{{204},1},{{205},-1}};}
}
int main(){try{
    numeric::ExactActionGuard guard;const auto source=fixture();const System valid(source,ratios(6),guard);
    check(valid.source()==source,"multiplicative learner retains the exact source owner");
    check(valid.components().size()==5 && valid.free_amplitudes()==4,"componentwise nonzero amplitudes include isolated cell");
    check(valid.cycles().size()==3,"tree certificate retains all independent closing relations");
    check(valid.cycles()[0].holonomy==1 && valid.cycles()[1].holonomy==1 && valid.cycles()[2].holonomy==-1,
          "exact rational holonomy includes self-relations");
    check(!valid.components()[4].nonzero_solution,"inconsistent self-relation forces its component to zero");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){
        const auto field=valid.solve({Q{a},Q{b},Q{7,3},-5},guard);
        check(field==V{Q{a},Q{2*a},Q{6*a},Q{b},Q{-2*b},Q{7,3},-5,0},"componentwise full exact solution family");
        check(numeric::is_zero(valid.residual(field,guard)),"all original multiplicative equations hold");
        const auto twisted=local::GradeTensorField(valid.fibers(),core::Grade{1}).matrix();
        check(numeric::is_zero(twisted.transpose().apply(field)),"same solutions are native twisted grade-zero kernel fields");
    }
    const System obstructed(source,ratios(5),guard);
    check(obstructed.free_amplitudes()==3 && !obstructed.components()[0].nonzero_solution,"nontrivial cycle forces only its own component to zero");
    const auto partial=obstructed.solve({2,3,4},guard);
    check(partial==V{0,0,0,2,-4,3,4,0} && numeric::is_zero(obstructed.residual(partial,guard)),
          "global nonzero solution may vanish on inconsistent components");
    const auto& cycle=obstructed.cycles()[0];
    check(cycle.tail_occurrence==core::OccurrenceKey{302} && cycle.head_occurrence==core::OccurrenceKey{303},
          "closing witness retains exact occurrence identities");
    const auto declared=ratios(5);std::map<core::RelationKey,Q> values;for(const auto& r:declared)values.emplace(r.relation,r.value);
    Q product{1};std::size_t current=obstructed.components()[cycle.component].root.index;
    for(const auto step:obstructed.cycle_walk(0,guard)){
        const auto& arena=source->grade(core::Grade{1});const auto [begin,end]=arena.incidence_range(step.relation.index);
        check(end-begin==2,"walk steps retain original pair flags");
        const auto tail=arena.lower_index[begin],head=arena.lower_index[begin+1];
        check(current==(step.forward?tail:head),"holonomy witness is a continuous original-source walk");
        current=step.forward?head:tail;const auto q=values.at(arena.relation_identities[step.relation.index]);
        if(step.forward)product*=q;else product/=q;
    }
    check(product==cycle.holonomy && product!=1 && current==obstructed.components()[cycle.component].root.index,
          "closing walk independently reproduces the nontrivial holonomy");
    check(source->grade(core::Grade{2}).cell_count()==1 && obstructed.fibers().complex().grade_count()==3,
          "higher declared syzygy retained even when twisted connection is curved");
    auto omitted=ratios(6);omitted.pop_back();rejects([&]{System bad(source,omitted,guard);},"incomplete ratio declaration rejects");
    auto zero=ratios(6);zero[0].value=0;rejects([&]{System bad(source,zero,guard);},"zero ratio outside multiplicative profile rejects");
    auto foreign=ratios(6);foreign[0].relation=core::RelationKey{99};rejects([&]{System bad(source,foreign,guard);},"foreign relation identity rejects");
    auto duplicate=ratios(6);duplicate[0].relation=duplicate[1].relation;rejects([&]{System bad(source,duplicate,guard);},"duplicate ratio identity rejects");
    rejects([&]{(void)valid.solve({1},guard);},"missing free amplitudes reject");
    rejects([&]{(void)valid.residual({1},guard);},"foreign field carrier rejects");
    std::cout<<"multiplicative relations: "<<checks<<" checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
