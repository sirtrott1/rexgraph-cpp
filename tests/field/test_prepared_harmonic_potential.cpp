#include "rfc/core/builder.hpp"
#include "rfc/field/execution_tower.hpp"
#include "rfc/approximation/tower.hpp"
#include <future>
#include <iostream>
#include <string_view>

using namespace rfc;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using A = numeric::ExactMatrix;
using K = field::OperatorKind;
using M = field::ExactMetric;
namespace {
std::size_t checks{};
void check(bool value, const char* message) {++checks; if (!value) throw std::runtime_error(message);}
template<class F> void rejects(F f, const char* message) {
    bool caught = false; try {f();} catch (const std::exception&) {caught = true;} check(caught, message);
}
std::shared_ptr<const core::RelationalComplex> square(bool filled) {
    core::RelationSignature signature;
    const auto type = signature.add_type("retained-square", core::BoundaryPolicy::DeclaredExact, 0, 12);
    core::RelationalComplexBuilder b(signature); b.declare_grade(core::Grade{2});
    b.add_grade_one(core::RelationKey{100}, type,
        {{0,true,true,Q{1}}, {1,true,false,Q{1}}, {0,true,false,Q{2}},
         {0,true,true,Q{2}}, {2,false,false,Q{7}}});
    b.add_grade_one(core::RelationKey{101}, type, {{1,true,true,Q{1}}, {2,true,false,Q{1}}});
    b.add_grade_one(core::RelationKey{102}, type, {{2,true,true,Q{1}}, {3,true,false,Q{1}}});
    b.add_grade_one(core::RelationKey{103}, type, {{3,true,true,Q{1}}, {0,true,false,Q{1}}});
    if (filled) b.add_higher(core::Grade{2},core::RelationKey{200},type,
        {{0,true,false,Q{1}}, {1,true,false,Q{1}}, {2,true,false,Q{1}}, {3,true,false,Q{1}}});
    return std::make_shared<const core::RelationalComplex>(b.freeze());
}
std::shared_ptr<const core::RelationalComplex> branch() {
    core::RelationSignature signature;
    const auto type = signature.add_type("original-branch", core::BoundaryPolicy::DeclaredExact, 0, 12);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{500},type,
        {{0,true,true,Q{1}}, {1,true,false,Q{1,2}}, {2,true,false,Q{1,2}},
         {1,true,false,Q{3}}, {1,true,true,Q{3}}, {2,false,false,Q{11}}});
    return std::make_shared<const core::RelationalComplex>(b.freeze());
}
std::vector<M> metrics(const core::RelationalComplex& source, unsigned mode) {
    std::vector<M> result;
    for (std::uint16_t k = 0; k < source.grade_count(); ++k) {
        const auto n = source.grade(core::Grade{k}).cell_count();
        if (!mode) {result.push_back(M::identity(n)); continue;}
        A form(n,n);
        for (std::size_t i = 0; i < n; ++i) {
            form.set(i,i,Q{static_cast<unsigned long>(i + 5)});
            if (mode == 2 && i) {form.set(i-1,i,Q{1,3}); form.set(i,i-1,Q{1,3});}
        }
        if (mode == 1) {
            V diagonal(n); for (std::size_t i = 0; i < n; ++i) diagonal[i] = form.get(i,i);
            result.push_back(M::diagonal(std::move(diagonal)));
        } else result.emplace_back(std::move(form));
    }
    return result;
}
void vector_json(const V& values) {
    std::cout << '['; for (std::size_t i=0;i<values.size();++i) {if(i)std::cout<<',';std::cout<<'"'<<values[i]<<'"';} std::cout<<']';
}
void matrix_json(const A& matrix) {
    std::cout << "{\"rows\":"<<matrix.rows()<<",\"cols\":"<<matrix.cols()<<",\"values\":[";
    for(std::size_t i=0;i<matrix.rows();++i) {if(i)std::cout<<',';V row(matrix.cols());
        for(std::size_t j=0;j<matrix.cols();++j)row[j]=matrix.get(i,j);
        vector_json(row);}
    std::cout<<"]}";
}
void exact_cases(bool emit) {
    const std::vector sources{square(false),square(true),branch()};
    for (const auto& source : sources) for (unsigned mode = 0; mode < 3; ++mode) {
        const auto forms = metrics(*source,mode);
        const field::PreparedExactExecutionTower tower(source,forms,field::PreparedExactProfile::GlobalActions);
        const auto retained_count = source->grade(core::Grade{1}).incidence_count();
        for (std::uint16_t k=0;k<source->grade_count();++k) {
            const core::Grade grade{k}; const field::ExactHodgeOperator hodge(*source,grade,forms);
            const auto inverse = tower.prepare_action({K::HarmonicInverse,grade,0});
            const auto projectors = field::exact_hodge_projectors(hodge);
            check(inverse.output_grade() == grade && inverse.output_size() == hodge.size(),"inverse output carrier");
            for(unsigned sample=0;sample<5;++sample) {
                V input(hodge.size());
                for(std::size_t i=0;i<input.size();++i) {input[i]=Q{static_cast<long>(i+sample)-2,static_cast<unsigned long>(i+1)};input[i].canonicalize();}
                const auto value = inverse.apply(input);
                const auto harmonic = projectors.project_harmonic(input);
                check(hodge.apply(value)==numeric::sub(input,harmonic),"L L# = I - Pi_h");
                check(inverse.apply(hodge.apply(input))==numeric::sub(input,harmonic),"L# L = I - Pi_h");
                check(numeric::is_zero(projectors.project_harmonic(value)),"complete harmonic gauge");
                check(value==field::generalized_hodge_inverse_apply(hodge,input),"independent restricted stiffness comparison");
                check(inverse.apply(harmonic)==V(input.size()),"inverse of harmonic field");
                const auto other=V(input.size(),Q{2});
                check(forms[k].inner(value,other)==forms[k].inner(input,inverse.apply(other)),"metric self-adjoint inverse");
                V potential;
                if(k) {
                    const auto action=tower.prepare_action({K::GradientPotential,grade,0}); potential=action.apply(input);
                    const auto lower=core::Grade{static_cast<std::uint16_t>(k-1u)};
                    const field::BoundaryOperator boundary(*source,grade);
                    check(action.output_grade()==lower && action.output_size()==source->grade(lower).cell_count(),"potential output carrier");
                    check(potential==boundary.apply(value),"complete original-boundary potential witness");
                    check(boundary.adjoint(potential,forms[lower.value],forms[k])==projectors.project_gradient(input),"potential reconstructs gradient");
                    const auto complete_kernel=field::materialize_boundary_matrix(*source,grade).transpose().multiply(forms[lower.value].materialize()).kernel_frame();
                    check(numeric::is_zero(complete_kernel.transpose().apply(forms[lower.value].apply(potential))),"full adjoint-kernel gauge");
                }
                if(emit) {
                    std::cout<<"{\"grade\":"<<k<<",\"metrics\":[";
                    for(std::size_t j=0;j<forms.size();++j){if(j)std::cout<<',';matrix_json(forms[j].materialize());}
                    std::cout<<"],\"boundaries\":[";
                    for(std::uint16_t j=1;j<source->grade_count();++j){if(j>1)std::cout<<',';matrix_json(field::materialize_boundary_matrix(*source,core::Grade{j}));}
                    std::cout<<"],\"input\":";vector_json(input);std::cout<<",\"inverse\":";vector_json(value);
                    std::cout<<",\"potential\":";vector_json(potential);std::cout<<"}\n";
                }
            }
        }
        check(retained_count==source->grade(core::Grade{1}).incidence_count(),"retained occurrences changed");
    }
    const auto source=branch();const auto forms=metrics(*source,0);
    const field::PreparedExactExecutionTower tower(source,forms,field::PreparedExactProfile::GlobalActions);
    check(tower.prepare_action({K::GradientPotential,core::Grade{1},0}).apply({1})==V({Q{-2,3},Q{1,3},Q{1,3}}),"branch potential exact value");
    const field::ExactHodgeOperator hodge(*source,core::Grade{0},forms);
    check(hodge.harmonic_frame().cols()==2,"connected branch full kernel dimension");
    const V anchored_kernel{0,1,-1};
    check(numeric::is_zero(field::BoundaryOperator(*source,core::Grade{1}).transpose_apply(anchored_kernel)),"single head anchor misses kernel direction");
}
void contracts() {
    const auto source=square(false);const auto forms=metrics(*source,2);
    const field::PreparedExactExecutionTower tower(source,forms,field::PreparedExactProfile::GlobalActions);
    for(const auto kind:{K::HarmonicInverse,K::GradientPotential}) {
        const auto action=tower.prepare_action({kind,core::Grade{1},0});const V input{1,2,3,4};const auto expected=action.apply(input);
        const auto steps=action.preparation_steps(); std::vector<std::future<V>> jobs;
        for(unsigned i=0;i<8;++i)jobs.push_back(std::async(std::launch::async,[action,input]{return action.apply(input);}));
        for(auto& job:jobs)check(job.get()==expected,"concurrent independent scratch");
        check(steps==action.preparation_steps() && steps>0,"factors prepared again during use");
        auto limits=numeric::ExactActionLimits{};limits.steps=1;rejects([&]{action.apply(input,limits);},"current work limit");
        limits={};limits.dimension=action.solve_dimension()-1;rejects([&]{action.apply(input,limits);},"current dimension limit");
        limits={};limits.entries=action.retained_entries()-1;rejects([&]{action.apply(input,limits);},"current entry limit");
        limits={};limits.integer_bits=0;rejects([&]{action.apply(V(4),limits);},"current factor bit limit");
        rejects([&]{action.apply({1});},"carrier dimension check");
        V malformed=input;mpz_set_ui(mpq_denref(malformed[0].get_mpq_t()),0);
        rejects([&]{action.apply(malformed);},"zero denominator");
        std::size_t calls{};
        rejects([&]{action.apply(input,{},[&]{if(++calls==10)throw std::runtime_error("stop");});},"mid-action cancellation");
        check(action.apply(input)==expected,"cancelled application poisoned factors");
        rejects([&]{tower.prepare_action({kind,core::Grade{1},1});},"unrequested lambda accepted");
        calls=0;rejects([&]{tower.prepare_action({kind,core::Grade{1},0},{},[&]{if(++calls==10)throw std::runtime_error("stop");});},"mid-preparation cancellation");
        limits={};limits.entries=1;rejects([&]{tower.prepare_action({kind,core::Grade{1},0},limits);},"preparation bounds");
        const field::PreparedExactExecutionTower finite(source,metrics(*source,0));
        rejects([&]{finite.prepare_action({kind,core::Grade{1},0});},"finite profile accepted global inverse");
        const auto approximation=approximation::ApproximationTower::unweighted(*source);
        rejects([&]{static_cast<void>(approximation.apply({kind,core::Grade{1},0},{1,2,3,4}));},"implicit approximate inverse");
    }
    rejects([&]{tower.prepare_action({K::GradientPotential,core::Grade{0},0});},"grade-zero potential accepted");
    const auto held=[] {const auto p=square(false);
        return field::PreparedExactExecutionTower(p,metrics(*p,2),field::PreparedExactProfile::GlobalActions)
            .prepare_action({K::GradientPotential,core::Grade{1},0});}();
    check(held.apply({1,2,3,4}).size()==4,"action outlives tower");
    core::RelationalComplexBuilder empty(core::RelationSignature{});empty.declare_witness_identity(1);empty.declare_grade(core::Grade{2});
    const auto owner=std::make_shared<const core::RelationalComplex>(empty.freeze());
    const field::PreparedExactExecutionTower empty_tower(owner,metrics(*owner,0),field::PreparedExactProfile::GlobalActions);
    for(std::uint16_t k=0;k<3;++k) {
        const V input(k?0:1,Q{3});
        check(empty_tower.prepare_action({K::HarmonicInverse,core::Grade{k},0}).apply(input)==V(input.size()),"all-harmonic/empty inverse");
        if(k) {
            const auto action=empty_tower.prepare_action({K::GradientPotential,core::Grade{k},0});
            check(action.apply(input)==V(k==1?1:0),"empty upper and lower potential carriers");
            rejects([&]{action.apply({1});},"zero result skipped actual input dimension");
        }
    }
}
}
int main(int argc,char** argv) {try {
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    exact_cases(emit);contracts();if(!emit)std::cout<<"prepared_harmonic_potential_checks="<<checks<<'\n';return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
