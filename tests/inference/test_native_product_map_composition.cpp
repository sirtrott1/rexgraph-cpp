#include "../../examples/native_product_map_composition/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_map_example;
namespace ex = native_composition_example;
std::size_t checks{}, cases{}, contributions{}, factor_contributions{}, support_differences{};
void check(bool yes, const char* message) { ++checks; if (!yes) throw std::runtime_error(message); }
template<class E, class F> void rejects(F fn, const char* message) {
    bool caught = false; try { fn(); } catch (const E&) { caught = true; } check(caught,message);
}
template<class R, class F> void array(const R& xs, F fn) {
    std::cout << '['; bool comma = false;
    for (const auto& x : xs) { if (comma) std::cout << ',';
            comma = true; fn(x); } std::cout << ']';
}
void json(core::CellId c) { std::cout << '[' << c.grade.value << ',' << c.index << ']'; }
void json(const std::array<core::CellId,3>& ids) { array(ids,[](auto c){json(c);}); }
void json(const Cell& c) { json(algebra::product_associator_detail::factors(c)); }
void json(const Numeric::Values& values) {
    array(values,[](const auto& x){std::cout << '[';json(x.first);std::cout << ",\"" << x.second << "\"]";});
}
void field(const algebra::OwnedAssociatedProductField& f, const Numeric& h) {
    std::cout << "{\"carrier\":" << algebra::product_hexagon_detail::index(h.carrier(f)) << ",\"grade\":" << f.grade().value << ",\"values\":";
    json(f.values()); std::cout << '}';
}
void source(const core::RelationalComplex& c) {
    std::cout << "{\"sizes\":[";
    for (std::size_t k = 0; k < c.grade_count(); ++k) { if (k) std::cout << ','; std::cout << algebra::owned_product_detail::count(c,k); }
    std::cout << "],\"scales\":[";
    for (std::size_t k = 0; k < c.grade_count(); ++k) { if (k) std::cout << ','; array(c.grade(algebra::owned_product_detail::grade(k)).cell_scales,[](const auto& q){std::cout << '"' << q << '"';}); }
    std::cout << "],\"flags\":["; bool comma = false;
    for (std::size_t k = 1; k < c.grade_count(); ++k) {
        const auto& a = c.grade(algebra::owned_product_detail::grade(k));
        for (std::size_t j = 0; j < a.incidence_count(); ++j) {
            if (comma) std::cout << ',';
            comma = true;
            std::cout << '[' << k << ',' << a.upper_index[j] << ',' << a.lower_index[j] << ',' << a.aspects.get(c.signature().existence_aspect(),j)
                << ",\"" << a.lattice_coefficient[j] << "\"]";
        }
    }
    std::cout << "]}";
}
void lift(const algebra::ProductMapLift& w, const Numeric& sn, const Numeric& tn) {
    std::cout << "{\"input\":"; field(w.input(),sn); std::cout << ",\"image\":"; field(w.image(),tn);
    std::cout << ",\"stages\":";
    array(w.stages(),[](const auto& s){std::cout << "{\"factor\":" << s.original_factor << ",\"input_target\":";
        array(s.input_target,[](bool b){std::cout << b;}); std::cout << ",\"image_target\":"; array(s.image_target,[](bool b){std::cout << b;});
        const auto values=[](const auto& v){array(v,[](const auto& x){std::cout << '[';json(x.first);std::cout << ",\"" << x.second << "\"]";});};
        std::cout << ",\"input\":"; values(s.input); std::cout << ",\"image\":"; values(s.image);
        std::cout << ",\"contributions\":";
        array(s.contributions,[](const auto& c){std::cout << '[';json(c.input);std::cout << ',';json(c.image);
            std::cout << ",[" << c.matrix_position.first << ',' << c.matrix_position.second << "],\"" << c.input_value << "\",\"" << c.coefficient << "\",\"" << c.value << "\"]";});
        std::cout << '}';}); std::cout << '}';
}
void comparison(const algebra::ProductMapComparison& c, const Numeric& h) {
    std::cout << "{\"first\":";field(c.mapped_then_operation,h);std::cout << ",\"second\":";field(c.operation_then_mapped,h);
    std::cout << ",\"only_first\":";array(c.only_first_support,[](const auto& x){json(x);});
    std::cout << ",\"only_second\":";array(c.only_second_support,[](const auto& x){json(x);});std::cout << '}';
}
void map_plan(const std::array<core::RelationalComplex,3>& src, const std::array<core::RelationalComplex,3>& dst, const Declaration& d) {
    std::cout << "{\"source\":"; array(src,[](const auto& c){source(c);}); std::cout << ",\"target\":"; array(dst,[](const auto& c){source(c);});
    std::cout << ",\"maps\":"; array(d.factors,[](const auto& f){std::cout << '[';
        for (std::size_t k = 0; k < f.matrices.size(); ++k) {
            if(k)std::cout << ',';
            const auto& m = f.matrices[k];
            std::cout << "{\"shape\":[" << m.rows() << ',' << m.cols() << "],\"support\":";
            array(f.support[k],[&](const auto& site){std::cout << '[' << site.first << ',' << site.second << ",\"" << m.get(site.first,site.second) << "\"]";});std::cout << '}';
        }
        std::cout << ']';});std::cout << '}';
}
void composition_comparison(const algebra::ProductMapCompositionComparison& c, const Numeric& h) {
    std::cout << "{\"first\":";field(c.direct,h);std::cout << ",\"second\":";field(c.staged,h);
    std::cout << ",\"only_first\":";array(c.only_direct_support,[](const auto& x){json(x);});
    std::cout << ",\"only_second\":";array(c.only_staged_support,[](const auto& x){json(x);});std::cout << '}';
}
void composed_lift(const algebra::ProductMapCompositionLift& w) {
    const auto p = w.plan(); const auto a = p.first().source(), b = p.first().target(), c = p.second().target();
    std::cout << "{\"first\":"; lift(w.first(),a,b); std::cout << ",\"second\":";lift(w.second(),b,c);
    std::cout << ",\"direct\":";lift(w.direct(),a,c);std::cout << ",\"comparison\":";composition_comparison(w.comparison(),c);std::cout << '}';
}
void emit_plan(unsigned sample, const ex::Fixture& f, const ex::PureComposition& p) {
    const auto direct = p.direct();
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"first\":";map_plan(f.source,f.intermediate,f.first);
    std::cout << ",\"second\":";map_plan(f.intermediate,f.target,f.second);std::cout << ",\"direct\":";map_plan(f.source,f.target,direct.declaration());
    std::cout << ",\"factors\":";array(p.factors(),[](const auto& factor){array(factor,[](const auto& g){
        std::cout << "{\"factor\":" << g.original_factor << ",\"grade\":" << g.grade << ",\"sizes\":[" << g.source_size << ',' << g.intermediate_size << ',' << g.target_size << "],\"support\":";
        array(g.support,[&](const auto& site){std::cout << '[' << site.first << ',' << site.second << ",\"" << g.matrix.get(site.first,site.second) << "\"]";});
        std::cout << ",\"contributions\":";array(g.contributions,[](const auto& t){
            const auto pos=[](const auto& x){std::cout << '[' << x.first << ',' << x.second << ']';};
            std::cout << '[';pos(t.first_position);std::cout << ',';pos(t.second_position);std::cout << ',';pos(t.position);
            std::cout << ",\"" << t.first_coefficient << "\",\"" << t.second_coefficient << "\",\"" << t.value << "\"]";
        });std::cout << '}';});});std::cout << "}\n";
}
void direct_witness(const algebra::ProductMapCoherenceWitness& w, const Numeric& a, const Numeric& c) {
    std::cout << "{\"input\":";field(w.routes()[0].source_path[0],a);std::cout << ",\"routes\":";
    array(w.routes(),[&](const auto& r){std::cout << "{\"operations\":";array(r.operations,[](auto op){std::cout << static_cast<unsigned>(op);});
        std::cout << ",\"source\":";array(r.source_path,[&](const auto& x){field(x,a);});std::cout << ",\"target\":";array(r.target_path,[&](const auto& x){field(x,c);});
        std::cout << ",\"maps\":";array(r.prefix_maps,[&](const auto& x){lift(x,a,c);});std::cout << ",\"comparisons\":";array(r.comparisons,[&](const auto& x){comparison(x,c);});std::cout << '}';});
    std::cout << ",\"boundary\":";
    if(w.boundary()){std::cout << "{\"input_map\":";lift(w.boundary()->input_map,a,c);std::cout << ",\"boundary_map\":";lift(w.boundary()->boundary_map,a,c);std::cout << ",\"comparison\":";comparison(w.boundary()->comparison,c);std::cout << '}';}
    else std::cout << "null";
    std::cout << ",\"yang_baxter\":";comparison(w.yang_baxter(),c);std::cout << '}';
}
void emit_witness(unsigned sample, const inference::NativeProductCompositionWitness& native) {
    const auto p = native.plan().numeric_plan(); const auto a = p.first().source(), c = p.second().target();const auto& w = native.witness();
    std::cout << "{\"phase\":\"witness\",\"sample\":" << sample << ",\"direct\":";direct_witness(w.direct(),a,c);
    std::cout << ",\"routes\":";array(w.routes(),[&](const auto& r){std::cout << "{\"prefixes\":";array(r.prefixes,[](const auto& x){composed_lift(x);});
        std::cout << ",\"path\":";array(r.staged_path,[&](const auto& x){field(x,c);});std::cout << ",\"comparisons\":";array(r.staged_naturality,[&](const auto& x){comparison(x,c);});std::cout << '}';});
    std::cout << ",\"boundary\":";
    if(w.boundary()){const auto& b = *w.boundary();std::cout << "{\"input\":";composed_lift(b.input);std::cout << ",\"boundary\":";composed_lift(b.boundary);
        std::cout << ",\"staged_boundary\":";field(b.staged_boundary,c);std::cout << ",\"comparison\":";comparison(b.staged_naturality,c);std::cout << ",\"images\":";composition_comparison(b.image_boundaries,c);std::cout << '}';}
    else std::cout << "null";
    std::cout << ",\"staged_yang_baxter\":";comparison(w.staged_yang_baxter(),c);std::cout << "}\n";
}
void verify_comparison(const algebra::ProductMapComparison& c) {
    check(c.mapped_then_operation.plan().same_owner(c.operation_then_mapped.plan()),"comparison retains actual owner");
    for(const auto& id:c.only_first_support)check(c.mapped_then_operation.values().at(id)==0 && !c.operation_then_mapped.values().contains(id),"first-only supported zero retained");
    for(const auto& id:c.only_second_support)check(c.operation_then_mapped.values().at(id)==0 && !c.mapped_then_operation.values().contains(id),"second-only supported zero retained");
    support_differences+=c.only_first_support.size()+c.only_second_support.size();
}
void verify_map(const algebra::ProductMapLift& w) {
    const auto p=w.plan();check(p.source().carrier(w.input())==p.target().carrier(w.image()),"lift preserves declared associated carrier");
    for(std::size_t f=0;f<3;++f){const auto& s=w.stages()[f];check(s.original_factor==f,"original A/B/C order retained");
        if(f)check(s.input==w.stages()[f-1].image,"tensor stage binds prior stage");
        for(const auto& t:s.contributions){check(t.input_value*t.coefficient==t.value && t.input_value==s.input.at(t.input),"individual tensor contribution exact");
            check(t.matrix_position==Matrix::Key{t.image[f].index,t.input[f].index},"tensor contribution uses declared map position");
            for(std::size_t j=0;j<3;++j)check(t.input[j].grade==t.image[j].grade && (j==f || t.input[j]==t.image[j]),"other tensor roles unchanged");
            ++contributions;
        }
    }
}
void verify_lift(const algebra::ProductMapCompositionLift& w) {
    const auto p=w.plan();check(w.first().plan().same_owner(p.first()) && w.second().plan().same_owner(p.second()) && w.direct().plan().same_owner(p.direct()),"actual three map owners retained");
    check(w.first().image().plan().same_owner(w.second().input().plan()) && w.first().image().values()==w.second().input().values(),"second lift consumes actual first image");
    check(w.first().input().values()==w.direct().input().values(),"direct and staged routes begin at actual input");
    check(w.comparison().direct.values()==w.direct().image().values() && w.comparison().staged.values()==w.second().image().values(),"comparison binds both histories");
    check(w.comparison().only_direct_support.empty() && w.comparison().only_staged_support.empty(),"path support composition preserves exact current support");
    check(w.staged_work()[0].end==w.staged_work()[1].begin && w.staged_work()[0].steps()>0 && w.staged_work()[1].steps()>0,"staged phases share cumulative guard");
    verify_map(w.first());verify_map(w.second());verify_map(w.direct());
}
void exhaustive(bool emit) {
    for(unsigned sample=0;sample<9;++sample){const auto f=ex::fixture(sample);const auto plans=ex::plans(sample);const auto p=plans.composition;
        const auto numeric=p.numeric_plan();const auto source=p.first().source().numeric_plan();
        if(emit)emit_plan(sample,f,numeric);
        for(std::size_t role=0;role<3;++role)for(const auto& grade:numeric.factors()[role]){
            check(grade.original_factor==role && grade.matrix.rows()==grade.target_size && grade.matrix.cols()==grade.source_size,"complete factor composition carrier");
            Matrix accumulated(grade.target_size,grade.source_size);std::set<Matrix::Key> supported;
            for(const auto& term:grade.contributions){check(term.first_position.first==term.second_position.second,"actual intermediate coordinate joins");
                check(term.position==Matrix::Key{term.second_position.first,term.first_position.second},"composition position follows G after F");
                check(term.value==term.second_coefficient*term.first_coefficient,"factor contribution is exact ordered product");
                accumulated.set(term.position.first,term.position.second,accumulated.get(term.position.first,term.position.second)+term.value);supported.insert(term.position);++factor_contributions;
            }
            check(accumulated==grade.matrix && supported==grade.support,"support survives zero normalization and cancellation");
        }
        for(std::size_t ci=0;ci<12;++ci)for(std::uint16_t g=0;g<=source.max_grade().value;++g){const auto carrier_value=carrier(ci);Numeric::Values values;
            for(const auto& ids:source.cells())if(ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==g){const auto sum=ids[0].index+ids[1].index+ids[2].index;
                values.emplace(cell(carrier_value,ids),sum%3==0 ? Q{0} : sum%3==1 ? Q{-2,3} : Q{5,7});}
            if(!values.empty())values.begin()->second=Q{1};
            const auto input=p.first().source().field(carrier_value,{g},values,ex::available_composition.source);
            const auto nw=p.witness(input,ex::available_composition,ex::composition_lease());const auto& w=nw.witness();++cases;
            check(nw.plan().same_owner(p) && nw.input()->plan().same_owner(p.first().source()),"native composition owns its actual origin");
            check(!nw.stages()[0]->prior_input() && nw.stages()[1]->prior_input()==nw.stages()[0],"native staged input is the actual prior receipt");
            check(nw.stages()[1]->input()==nw.stages()[0]->input(),"native origin is shared without a midpoint rebind");
            check(nw.stages()[1]->lift().input().values()==nw.stages()[0]->lift().image().values(),"native second stage consumes first output");
            const std::array av{ex::available_composition.source,ex::available_composition.intermediate,ex::available_composition.target};
            for(std::size_t j=0;j<3;++j)for(std::size_t r=0;r<3;++r)check(nw.provenance().namespaces[j][r]==av[j][r].source_namespace && nw.provenance().sequences[j][r]==av[j][r].max_sequence,"nine original cutoff roles stay ordered");
            check(w.boundary().has_value()==(g!=0),"grade zero has no invented boundary");
            for(std::size_t r=0;r<5;++r){const auto& route=w.routes()[r];const auto& direct=w.direct().routes()[r];
                check(route.prefixes.size()==direct.prefix_maps.size() && route.staged_path.size()==route.prefixes.size() && route.staged_naturality.size()==direct.operations.size(),"every direct and staged prefix retained");
                for(const auto& lift_value:route.prefixes)verify_lift(lift_value);
                for(const auto& comparison_value:route.staged_naturality)verify_comparison(comparison_value);
                for(const auto& comparison_value:direct.comparisons)verify_comparison(comparison_value);
            }
            verify_comparison(w.direct().yang_baxter());verify_comparison(w.staged_yang_baxter());
            if(w.boundary()){verify_lift(w.boundary()->input);verify_lift(w.boundary()->boundary);verify_comparison(w.boundary()->staged_naturality);verify_comparison(w.direct().boundary()->comparison);}
            check(nw.stages()[0]->provenance().execution_steps==w.routes()[0].prefixes[0].staged_work()[0].steps(),"first native phase cost matches actual guard interval");
            check(nw.stages()[1]->provenance().execution_steps==w.routes()[0].prefixes[0].staged_work()[1].steps(),"second native phase cost matches actual guard interval");
            if(emit)emit_witness(sample,nw);
        }
    }
    check(support_differences>0,"boundary support differences remain explicit");
}
void identity_controls(bool emit) {
    for(unsigned sample=0;sample<9;++sample){const auto p=ex::plans(sample);const auto source=p.first.source(),middle=p.first.target();
        const auto left_id=ex::Composition::identity(source,3400,1,ex::available_composition.source,ex::composition_lease());
        const auto right_id=ex::Composition::identity(middle,3500,1,ex::available_composition.intermediate,ex::composition_lease());
        const inference::NativeProductCompositionAvailability la{ex::available_composition.source,ex::available_composition.source,ex::available_composition.intermediate},
            ra{ex::available_composition.source,ex::available_composition.intermediate,ex::available_composition.intermediate};
        const auto left=ex::Composition::prepare(left_id,p.first,3600,1,la,ex::composition_lease()),
            right=ex::Composition::prepare(p.first,right_id,3700,1,ra,ex::composition_lease());
        const auto a=source.numeric_plan(),b=middle.numeric_plan();
        check(left_id.source().same_owner(left_id.target()) && right_id.source().same_owner(right_id.target()),"identity binds actual product owner");
        if(emit){const auto f=ex::fixture(sample);const auto ld=left_id.numeric_plan(),rd=right_id.numeric_plan();
            std::cout << "{\"phase\":\"identity_plan\",\"sample\":" << sample << ",\"left\":";map_plan(f.source,f.source,ld.declaration());
            std::cout << ",\"right\":";map_plan(f.intermediate,f.intermediate,rd.declaration());std::cout << "}\n";
        }
        for(std::size_t ci=0;ci<12;++ci)for(std::uint16_t g=0;g<=a.max_grade().value;++g){const auto c=carrier(ci);Numeric::Values values;
            for(const auto& ids:a.cells())if(ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==g)values.emplace(cell(c,ids),ids[0].index%2 ? Q{0} : Q{3,7});
            const auto input=source.field(c,{g},values,ex::available_composition.source);
            const auto l=left.apply(input,la,ex::composition_lease()),r=right.apply(input,ra,ex::composition_lease());
            const auto base=p.first.apply(input,{ex::available_composition.source,ex::available_composition.intermediate},ex::composition_lease());
            check(l.lift().first().image().values()==values,"left identity retains supported zeros");
            check(l.lift().second().image().values()==base.lift().image().values() && r.lift().second().image().values()==base.lift().image().values(),"identity compositions have exact original output and support");
            check(!l.plan().same_owner(r.plan()),"equal identity outputs retain separate named operations");
            verify_lift(l.lift());verify_lift(r.lift());
            if(emit){std::cout << "{\"phase\":\"identity\",\"sample\":" << sample << ",\"left\":";composed_lift(l.lift());std::cout << ",\"right\":";composed_lift(r.lift());std::cout << "}\n";}
        }
    }
}
void negative_and_ownership() {
    const auto p=ex::plans().composition,foreign=ex::plans().composition;const auto empty=p.first().source().field(carrier(0),{0},{},ex::available_composition.source);
    rejects<std::invalid_argument>([&]{ex::Composition::prepare(p.first(),foreign.second(),1,1,ex::available_composition,ex::composition_lease());},"equal midpoint keys and values cannot supply identity");
    Guard guard(ex::composition_limits());
    rejects<std::invalid_argument>([&]{ex::PureComposition::prepare(p.first().numeric_plan(),foreign.second().numeric_plan(),1,1,guard);},"numeric intermediate owner is also required");
    rejects<std::invalid_argument>([&]{ex::Composition::prepare(p.first(),p.second(),0,1,ex::available_composition,ex::composition_lease());},"composition identity must be declared");
    const auto other=foreign.first().source().field(carrier(0),{0},{},ex::available_composition.source);
    rejects<std::invalid_argument>([&]{p.witness(other,ex::available_composition,ex::composition_lease());},"foreign equal empty input rejects");
    for(std::size_t side=0;side<3;++side)for(std::size_t role=0;role<3;++role){auto a=ex::available_composition;
        const auto slot=[&]()->runtime::WorkAvailability& {return side==0 ? a.source[role] : side==1 ? a.intermediate[role] : a.target[role];};
        --slot().max_sequence;rejects<std::invalid_argument>([&]{p.witness(empty,a,ex::composition_lease());},"each of nine past availabilities rejects");
        a=ex::available_composition;slot().source_namespace=99;
        rejects<std::invalid_argument>([&]{ex::Composition::prepare(p.first(),p.second(),1,1,a,ex::composition_lease());},"each of nine foreign namespaces rejects");
    }
    auto later=ex::available_composition;for(auto& a:later.source)++a.max_sequence;for(auto& a:later.intermediate)++a.max_sequence;for(auto& a:later.target)++a.max_sequence;
    const auto old=p.apply(empty,later,ex::composition_lease());check(old.provenance().sequences[0][0]==7 && old.provenance().sequences[1][0]==23 && old.provenance().sequences[2][0]==37,"later availability keeps original endpoint bytes");
    const auto future=endpoint_plan(ex::fixture().source,later.source);const auto future_field=future.field(carrier(0),{0},{},later.source);
    rejects<std::invalid_argument>([&]{p.apply(future_field,later,ex::composition_lease());},"future source field cannot replace original origin");
    auto policy=ex::composition_policy();policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.apply(empty,ex::available_composition,runtime::ExecutionLease(policy));},"empty composition still admits allocation");
    policy=ex::composition_policy();policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.apply(empty,ex::available_composition,runtime::ExecutionLease(policy));},"required unsupported GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.apply(empty,ex::available_composition,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"explicit exact CPU fallback retained");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.apply(empty,ex::available_composition,runtime::ExecutionLease(ex::composition_policy(),stop));},"owned cancellation honored");
    const auto baseline=p.witness(empty,ex::available_composition,ex::composition_lease());policy=ex::composition_policy();policy.limits.scalar_work=baseline.provenance().execution_steps;
    const auto at_work=p.witness(empty,ex::available_composition,runtime::ExecutionLease(policy));
    check(at_work.provenance().execution_steps==policy.limits.scalar_work,"all proof routes share exact work boundary");
    --policy.limits.scalar_work;rejects<std::length_error>([&]{p.witness(empty,ex::available_composition,runtime::ExecutionLease(policy));},"staged and direct routes cannot reset work budget");
    for(unsigned resource=0;resource<3;++resource){policy=ex::composition_policy();const auto r=baseline.provenance().requirements;
        if(resource==0)policy.limits.records=r.records-1;else if(resource==1)policy.limits.rational_slots=r.rational_slots-1;else policy.limits.sparse_entries=r.sparse_entries-1;
        rejects<runtime::ExecutionLimitExceeded>([&]{p.apply(empty,ex::available_composition,runtime::ExecutionLease(policy));},"complete retained envelope admitted before empty fast path");}
    const auto expensive=ex::plans(1).composition;const auto expensive_empty=expensive.first().source().field(carrier(0),{0},{},ex::available_composition.source);
    rejects<runtime::ExecutionLimitExceeded>([&]{expensive.apply(expensive_empty,ex::available_composition);},"example does not silently enlarge default budgets");
    const auto order=ex::plans(8).composition.numeric_plan();const auto fm=order.first(),gm=order.second(),dm=order.direct();
    check(dm.declaration().factors[0].matrices[0]==gm.declaration().factors[0].matrices[0].multiply(fm.declaration().factors[0].matrices[0]),"factor order is G after F");
    check(dm.declaration().factors[0].matrices[0]!=fm.declaration().factors[0].matrices[0].multiply(gm.declaration().factors[0].matrices[0]),"noncommuting control detects wrong order");
    const auto canceled=ex::plans(6).composition.numeric_plan();check(canceled.factors()[0][0].support.size()==1 && canceled.factors()[0][0].matrix.nonzeros()==0 && canceled.factors()[0][0].contributions.size()==2,"canceled direction survives sparse normalization");
    const auto zero=ex::plans(7).composition.numeric_plan();check(zero.factors()[0][0].contributions.size()==2 && zero.factors()[0][0].contributions[0].value==0 && zero.factors()[0][0].contributions[1].value==0,"declared zero paths remain individual contributions");
    auto handle=p.numeric_plan();const auto replacement=foreign.numeric_plan();bool changed=false;
    Guard reentrant(ex::composition_limits(),[&]{if(!changed){changed=true;handle=replacement;}});
    const auto pinned=handle.witness(empty.field(),reentrant);check(pinned.plan().same_owner(p.numeric_plan()),"service is pinned before callback replacement");
    std::size_t checkpoints=0;Guard cancelled(ex::composition_limits(),[&]{if(++checkpoints==64)throw runtime::ExecutionCancelled();});
    rejects<runtime::ExecutionCancelled>([&]{p.numeric_plan().witness(empty.field(),cancelled);},"mid-action cancellation discards unpublished work");
    std::array<std::future<inference::NativeProductCompositionLift>,4> tasks;
    for(auto& task:tasks)task=std::async(std::launch::async,[&]{return p.apply(empty,ex::available_composition,ex::composition_lease());});
    for(auto& task:tasks){const auto out=task.get();check(out.plan().same_owner(p) && out.stages()[1]->prior_input()==out.stages()[0],"concurrent compositions own independent staged receipts");}
    std::optional<inference::NativeProductCompositionLift> retained;
    {const auto local=ex::plans(6).composition;const core::CellId vertex{{0},0};const auto in=local.first().source().field(carrier(0),{0},{{cell(carrier(0),{vertex,vertex,vertex}),Q{1}}},ex::available_composition.source);retained=local.apply(in,ex::available_composition,ex::composition_lease());}
    check(retained->stages()[1]->prior_input()->lift().image().values().size()==2 && retained->stages()[1]->lift().image().values().begin()->second==0,"receipts outlive original plans and inputs");
}
void authority_and_growth() {
    const auto p=ex::plans().composition;const auto f=ex::fixture();const std::array av{ex::available_composition.source,ex::available_composition.intermediate,ex::available_composition.target};
    const auto src=p.first().source(),mid=p.first().target(),dst=p.second().target();
    for(std::size_t side=0;side<3;++side)for(std::size_t role=0;role<3;++role){
        std::array requests{src.declaration(),mid.declaration(),dst.declaration()};auto& request=requests[side];const auto a=av[side][role];
        storage::NativeAccessionLimits cap;cap.max_integer_bytes=1;cap.max_action_work=4000000;
        storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(request.factors[role].endpoint()->source,a.max_sequence),a.source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},cap);
        request.factors[role]=native_product_example::anchor(directory);
        const auto sh=Plan::prepare(requests[0],av[0]),mh=Plan::prepare(requests[1],av[1]),th=Plan::prepare(requests[2],av[2]);
        const auto first=Map::prepare(sh,mh,f.first,{av[0],av[1]},ex::composition_lease()),second=Map::prepare(mh,th,f.second,{av[1],av[2]},ex::composition_lease());
        const auto bounded=ex::Composition::prepare(first,second,1,1,ex::available_composition,ex::composition_lease());
        const auto empty=sh.field(carrier(0),{0},{},av[0]);const auto result=bounded.apply(empty,ex::available_composition,ex::composition_lease());
        check(result.provenance().action_limits.integer_bits==8 && result.provenance().action_limits.steps==4000000,"all nine minimum integer and work authorities admitted");
        const core::CellId v{{0},0},e{{1},0};const auto growth=sh.field(carrier(0),{1},{{cell(carrier(0),{v,e,v}),Q{253,251}}},av[0]);
        rejects<std::length_error>([&]{bounded.witness(growth,ex::available_composition,ex::composition_lease());},"every original authority bounds cross-role exact growth");
    }
    auto c=ex::fixture(6);c.first.factors[0].matrices[0]=Matrix::from_rows({{Q{17}},{Q{17}}});c.second.factors[0].matrices[0]=Matrix::from_rows({{Q{17},Q{-17}}});
    const auto sh=endpoint_plan(c.source,av[0]),mh=endpoint_plan(c.intermediate,av[1]),th=endpoint_plan(c.target,av[2]);
    const auto first=Map::prepare(sh,mh,c.first,{av[0],av[1]},ex::composition_lease()),second=Map::prepare(mh,th,c.second,{av[1],av[2]},ex::composition_lease());
    const auto canceled=ex::Composition::prepare(first,second,1,1,ex::available_composition,ex::composition_lease());const auto empty=sh.field(carrier(0),{0},{},av[0]);
    auto policy=ex::composition_policy();policy.limits.integer_bits=8;
    rejects<std::length_error>([&]{canceled.apply(empty,ex::available_composition,runtime::ExecutionLease(policy));},"canceled unused factor products still revalidate integer growth");
    const auto actual=canceled.numeric_plan();check(actual.factors()[0][0].matrix.nonzeros()==0 && actual.factors()[0][0].contributions[0].value==289,"large canceled terms remain retained separately");
    const auto input=src.field(carrier(0),{0},{},av[0]);const auto baseline=p.apply(input,ex::available_composition,ex::composition_lease());
    policy=ex::composition_policy();policy.limits.scalar_work=baseline.provenance().preparation_steps;
    const auto prepared=ex::Composition::prepare(p.first(),p.second(),2,2,ex::available_composition,runtime::ExecutionLease(policy));
    const auto prepared_result=prepared.apply(input,ex::available_composition,ex::composition_lease());
    check(prepared_result.provenance().preparation_steps==policy.limits.scalar_work,"composition preparation shares one complete exact guard");
    --policy.limits.scalar_work;rejects<std::length_error>([&]{ex::Composition::prepare(p.first(),p.second(),2,2,ex::available_composition,runtime::ExecutionLease(policy));},"preparation work cannot reset between matrix and chain checks");
    auto limits=ex::composition_limits();limits.entries=p.numeric_plan().entry_envelope();Guard exact(limits);
    const auto exact_witness=p.numeric_plan().witness(input.field(),exact);check(exact_witness.plan().same_owner(p.numeric_plan()),"whole logical envelope is sufficient");
    --limits.entries;bool checkpoint=false;Guard small(limits,[&]{checkpoint=true;});
    rejects<std::length_error>([&]{p.numeric_plan().witness(input.field(),small);},"below-envelope proof rejects before allocations");check(!checkpoint,"preflight does not invoke callbacks");
}
void remaining_materialization_budget() {
    storage::TemporalDeclarationState state;state.namespace_key=188;
    const composite::DeclarationId type{188,1,1};state.registry=composite::DeclarationRegistry({composite::Declaration::integer(type)});
    state.values={{1,type,0,0,{},{},composite::DeclaredValue::integer(numeric::Integer{1})},
        {2,type,0,0,{},{},composite::DeclaredValue::integer(numeric::Integer{1})}};
    storage::temporal_codec_detail::Writer writer;storage::temporal_codec_detail::declaration_state(writer,state);
    const auto& bytes=writer.bytes();storage::TemporalReadLimits limits;
    limits.declarations.max_materialized_bytes=2*sizeof(storage::TemporalDeclaredValue)+1;
    bool protected_before_materialization=false;
    try{storage::temporal_codec_detail::Reader reader(bytes,limits);(void)storage::temporal_codec_detail::declaration_state(reader);}
    catch(const std::length_error& e){protected_before_materialization=std::string_view(e.what())=="decode materialization budget";}
    check(protected_before_materialization,"zero aggregate byte allowance must reject before materialization");
    // Exhausting byte allowance still permits an explicitly zero-byte value.
    const composite::DeclarationId boolean_type{188,2,1};state.registry=composite::DeclarationRegistry({composite::Declaration::integer(type),composite::Declaration::boolean(boolean_type)});
    state.values[1].type=boolean_type;state.values[1].value=composite::DeclaredValue::boolean(true);
    storage::temporal_codec_detail::Writer boolean_writer;storage::temporal_codec_detail::declaration_state(boolean_writer,state);
    const auto& boolean_bytes=boolean_writer.bytes();storage::temporal_codec_detail::Reader reader(boolean_bytes,limits);
    const auto decoded=storage::temporal_codec_detail::declaration_state(reader);
    check(composite::declared_value_equal(decoded.values[1].value,state.values[1].value),"zero-byte boolean may use an exhausted byte allowance");
}
} // namespace
int main(int argc,char** argv) {
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";if(argc>2 || (argc==2 && !emit))return 2;
    try{exhaustive(emit);identity_controls(emit);if(!emit){negative_and_ownership();authority_and_growth();remaining_materialization_budget();
        std::cout << "checks=" << checks << " witnesses=" << cases << " contributions=" << contributions << " factor_contributions=" << factor_contributions << " support_differences=" << support_differences << '\n';}}
    catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}
}
