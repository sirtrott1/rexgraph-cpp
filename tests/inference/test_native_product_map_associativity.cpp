#include "../../examples/native_product_map_associativity/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_map_example;
namespace ex = native_associativity_example;
std::size_t checks{}, cases{}, paths{}, prefixes{}, zero_paths{}, boundary_differences{};
void check(bool yes, const char* message) { ++checks; if (!yes) throw std::runtime_error(message); }
template<class E, class F> void rejects(F fn, const char* message) {
    bool caught=false; try { fn(); } catch(const E&) { caught=true; } check(caught,message);
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
void describe(const algebra::PreparedProductChainMap& p) {
    const auto a=p.source(), b=p.target();
    map_plan({a.factor(0),a.factor(1),a.factor(2)},{b.factor(0),b.factor(1),b.factor(2)},p.declaration());
}
void describe(const ex::PureComposition& p) {
    std::cout << "{\"first\":";describe(p.first());std::cout << ",\"second\":";describe(p.second());
    std::cout << ",\"direct\":";describe(p.direct());std::cout << ",\"factors\":";
    array(p.factors(),[](const auto& factor){array(factor,[](const auto& g){
        std::cout << "{\"factor\":" << g.original_factor << ",\"grade\":" << g.grade << ",\"sizes\":[" << g.source_size << ',' << g.intermediate_size << ',' << g.target_size << "],\"support\":";
        array(g.support,[&](const auto& site){std::cout << '[' << site.first << ',' << site.second << ",\"" << g.matrix.get(site.first,site.second) << "\"]";});
        std::cout << ",\"contributions\":";array(g.contributions,[](const auto& t){
            const auto pos=[](const auto& x){std::cout << '[' << x.first << ',' << x.second << ']';};
            std::cout << '[';pos(t.first_position);std::cout << ',';pos(t.second_position);std::cout << ',';pos(t.position);
            std::cout << ",\"" << t.first_coefficient << "\",\"" << t.second_coefficient << "\",\"" << t.value << "\"]";
        });std::cout << '}';});});std::cout << '}';
}
void emit_plan(unsigned sample,const ex::PureAssociativity& p) {
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"prefix\":";describe(p.prefix());
    std::cout << ",\"last\":";describe(p.last());std::cout << ",\"tail\":";describe(p.tail());
    std::cout << ",\"left\":";describe(p.left());std::cout << ",\"right\":";describe(p.right());
    std::cout << ",\"triples\":";array(p.factors(),[](const auto& f){array(f,[](const auto& g){
        std::cout << "{\"factor\":" << g.original_factor << ",\"grade\":" << g.grade << ",\"sizes\":";
        array(g.sizes,[](auto n){std::cout << n;});std::cout << ",\"support\":";
        array(g.support,[&](auto site){std::cout << '[' << site.first << ',' << site.second << ",\"" << g.matrix.get(site.first,site.second) << "\"]";});
        std::cout << ",\"contributions\":";array(g.contributions,[](const auto& t){
            std::cout << "{\"positions\":";array(t.positions,[](auto x){std::cout << '[' << x.first << ',' << x.second << ']';});
            std::cout << ",\"position\":[" << t.position.first << ',' << t.position.second << "],\"coefficients\":";
            array(t.coefficients,[](const auto& q){std::cout << '"' << q << '"';});
            std::cout << ",\"pairs\":[\"" << t.first_pair << "\",\"" << t.last_pair << "\"],\"values\":[\"" << t.left_value << "\",\"" << t.right_value << "\"]}";
        });std::cout << '}';});});std::cout << "}\n";
}
void assoc_lift(const algebra::ProductMapAssociativityLift& w) {
    const auto p=w.plan();const auto last=p.last();const auto z=last.source(), target_owner=last.target();
    std::cout << "{\"prefix\":";composed_lift(w.prefix());std::cout << ",\"last\":";lift(w.last(),z,target_owner);
    std::cout << ",\"left\":";composed_lift(w.left());std::cout << ",\"right\":";composed_lift(w.right());
    std::cout << ",\"parents\":";composition_comparison(w.parenthesizations(),target_owner);
    std::cout << ",\"staged\":";composition_comparison(w.staged_comparison(),target_owner);std::cout << '}';
}
void emit_witness(unsigned sample,const inference::NativeProductAssociativityWitness& native) {
    const auto& w=native.witness();const auto p=native.plan().numeric_plan();const auto h=p.last().target();
    std::cout << "{\"phase\":\"witness\",\"sample\":" << sample << ",\"routes\":";
    array(w.routes,[&](const auto& r){std::cout << "{\"operations\":";
        const auto i=static_cast<std::size_t>(&r-w.routes.data());
        array(w.left.direct().routes()[i].operations,[](auto op){std::cout << static_cast<unsigned>(op);});
        std::cout << ",\"prefixes\":";array(r.prefixes,[](const auto& x){assoc_lift(x);});
        std::cout << ",\"path\":";array(r.staged_path,[&](const auto& x){field(x,h);});
        std::cout << ",\"comparisons\":";array(r.staged_naturality,[&](const auto& x){comparison(x,h);});std::cout << '}';});
    std::cout << ",\"boundary\":";
    if(w.boundary){const auto& b=*w.boundary;std::cout << "{\"input\":";assoc_lift(b.input);std::cout << ",\"boundary\":";assoc_lift(b.boundary);
        std::cout << ",\"staged_boundary\":";field(b.staged_boundary,h);std::cout << ",\"comparison\":";comparison(b.staged_naturality,h);std::cout << '}';}
    else std::cout << "null";
    std::cout << ",\"yang_baxter\":";comparison(w.staged_yang_baxter,h);std::cout << "}\n";
}
void same(const algebra::ProductMapCompositionComparison& c) {
    check(c.direct.values()==c.staged.values() && c.only_direct_support.empty() && c.only_staged_support.empty(),"association preserves exact path support");
}
void verify(const algebra::ProductMapAssociativityLift& w) {
    same(w.parenthesizations());same(w.staged_comparison());
    check(w.last().input().values()==w.prefix().second().image().values(),"third stage consumes actual second output");
    check(w.last().plan().same_owner(w.plan().last()),"third map identity retained");
    check(w.left().plan().same_owner(w.plan().left()) && w.right().plan().same_owner(w.plan().right()),"distinct owning parenthesization plans retained");
    for(const auto& t:w.last().stages())for(const auto& q:t.contributions)check(q.value==q.coefficient*q.input_value,"third tensor path exact");
    ++prefixes;
}
void exhaustive(bool emit) {
    for(unsigned sample=0;sample<9;++sample){const auto p=ex::plans(sample).associativity;const auto numeric=p.numeric_plan();
        const auto source=p.prefix().first().source().numeric_plan();if(emit)emit_plan(sample,numeric);
        for(const auto& factor:numeric.factors())for(const auto& g:factor){Matrix sum(g.sizes[3],g.sizes[0]);std::set<Matrix::Key> support;
            for(const auto& t:g.contributions){check(t.positions[0].first==t.positions[1].second && t.positions[1].first==t.positions[2].second,"two actual intermediate coordinates");
                check(t.left_value==t.coefficients[2]*(t.coefficients[1]*t.coefficients[0]) && t.right_value==(t.coefficients[2]*t.coefficients[1])*t.coefficients[0],"each parenthesization retains exact triple path");
                sum.set(t.position.first,t.position.second,sum.get(t.position.first,t.position.second)+t.left_value);support.insert(t.position);++paths;zero_paths+=t.left_value==0;}
            check(sum==g.matrix && support==g.support,"triple support and coefficients match");
        }
        for(std::size_t ci=0;ci<12;++ci)for(std::uint16_t grade=0;grade<=source.max_grade().value;++grade){const auto c=carrier(ci);Numeric::Values values;
            for(const auto& ids:source.cells())if(ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==grade){const auto n=ids[0].index+ids[1].index+ids[2].index;
                values.emplace(cell(c,ids),n%3==0 ? Q{0} : n%3==1 ? Q{-2,3} : Q{5,7});}
            if(!values.empty())values.begin()->second=Q{1};
            const auto input=p.prefix().first().source().field(c,{grade},values,ex::available_associativity[0]);
            const auto native=p.witness(input,ex::available_associativity,ex::associativity_lease());const auto& w=native.witness();++cases;
            check(native.prefix_input()->input()->plan().same_owner(input.plan()),"origin retains actual X owner");
            check(native.last_stage()->prior_input()==native.prefix_input()->stages()[1],"actual native prior receipt retained");
            check(native.last_stage()->input()==native.prefix_input()->input(),"no fictitious Z field declaration");
            for(std::size_t j=0;j<4;++j)for(std::size_t f=0;f<3;++f)check(native.provenance().namespaces[j][f]==ex::available_associativity[j][f].source_namespace && native.provenance().sequences[j][f]==ex::available_associativity[j][f].max_sequence,"twelve original cutoff roles retained");
            for(const auto& r:w.routes){check(r.prefixes.size()==r.staged_path.size(),"all prefixes and path views retained");for(const auto& x:r.prefixes)verify(x);
                for(const auto& x:r.staged_naturality)boundary_differences+=x.only_first_support.size()+x.only_second_support.size();}
            check(w.boundary.has_value()==(grade!=0),"boundary grade is explicit");
            if(w.boundary){verify(w.boundary->input);verify(w.boundary->boundary);boundary_differences+=w.boundary->staged_naturality.only_first_support.size()+w.boundary->staged_naturality.only_second_support.size();}
            const auto prior=std::make_shared<const inference::NativeProductCompositionLift>(p.prefix().apply(input,
                {ex::available_associativity[0],ex::available_associativity[1],ex::available_associativity[2]},ex::associativity_lease()));
            const auto continued=p.continue_from(prior,ex::available_associativity,ex::associativity_lease());
            verify(continued.lift());check(continued.prefix_input()==prior && continued.last_stage()->prior_input()==prior->stages()[1],"continuation adopts caller's exact receipt pointer");
            check(continued.last_stage()->lift().image().values()==native.last_stage()->lift().image().values(),"continued and complete witnesses agree");
            check(continued.provenance().aggregate_steps==continued.provenance().prior_execution_steps+continued.provenance().execution_steps && continued.provenance().prior_execution_steps==prior->provenance().execution_steps,"ancestral work charged once");
            if(emit){emit_witness(sample,native);std::cout << "{\"phase\":\"continuation\",\"sample\":" << sample << ",\"lift\":";assoc_lift(continued.lift());std::cout << "}\n";}
        }
    }
    check(zero_paths>0 && boundary_differences>0,"zero paths and boundary support differences exercised");
}
void contracts() {
    const auto p=ex::plans(6).associativity, other=ex::plans(6).associativity;
    const auto a=ex::available_associativity;const auto x=p.prefix().first().source().field(carrier(0),{0},{},a[0]);
    const auto prefix_a=inference::NativeProductCompositionAvailability{a[0],a[1],a[2]};
    const auto prior=std::make_shared<const inference::NativeProductCompositionLift>(p.prefix().apply(x,prefix_a,ex::associativity_lease()));
    rejects<std::invalid_argument>([&]{p.continue_from({},a,ex::associativity_lease());},"null receipt rejected");
    const auto foreign=std::make_shared<const inference::NativeProductCompositionLift>(other.prefix().apply(other.prefix().first().source().field(carrier(0),{0},{},a[0]),prefix_a,ex::associativity_lease()));
    rejects<std::invalid_argument>([&]{p.continue_from(foreign,a,ex::associativity_lease());},"equal-key foreign receipt rejected");
    rejects<std::invalid_argument>([&]{ex::Associativity::prepare(p.prefix(),other.last(),ex::associativity_declaration,a,ex::associativity_lease());},"equal-shaped foreign midpoint rejected");
    auto bad=ex::associativity_declaration;bad.right_key=bad.left_key;
    rejects<std::invalid_argument>([&]{ex::Associativity::prepare(p.prefix(),p.last(),bad,a,ex::associativity_lease());},"parenthesization identities must be declared distinctly");
    bad=ex::associativity_declaration;bad.revision=0;
    rejects<std::invalid_argument>([&]{ex::Associativity::prepare(p.prefix(),p.last(),bad,a,ex::associativity_lease());},"zero revision rejected");
    rejects<std::invalid_argument>([&]{p.apply(foreign->input()->plan().field(carrier(0),{0},{},a[0]),a,ex::associativity_lease());},"foreign native field rejected");
    for(std::size_t j=0;j<4;++j)for(std::size_t f=0;f<3;++f){auto cutoff=a;--cutoff[j][f].max_sequence;
        rejects<std::invalid_argument>([&]{p.continue_from(prior,cutoff,ex::associativity_lease());},"all twelve prior cutoffs enforced");
        cutoff=a;++cutoff[j][f].source_namespace;rejects<std::invalid_argument>([&]{p.apply(x,cutoff,ex::associativity_lease());},"all twelve namespace roles enforced");}
    auto later=a;for(auto& h:later)for(auto& f:h)++f.max_sequence;
    const auto later_result=p.continue_from(prior,later,ex::associativity_lease());check(later_result.prefix_input()->provenance().execution_availability.source[0].max_sequence==a[0][0].max_sequence,"later availability does not rewrite old receipt");
    const auto future=endpoint_plan(ex::fixture(6).prefix.source,later[0]);
    rejects<std::invalid_argument>([&]{p.apply(future.field(carrier(0),{0},{},later[0]),a,ex::associativity_lease());},"future native field rejected");
    auto policy=ex::associativity_policy();policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.apply(x,a,runtime::ExecutionLease(policy));},"owning allocation permission required");
    policy=ex::associativity_policy();policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.apply(x,a,runtime::ExecutionLease(policy));},"required unavailable GPU rejected");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;const auto fallback=p.apply(x,a,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"explicit CPU fallback recorded");
    const auto result=p.continue_from(prior,a,ex::associativity_lease());policy=ex::associativity_policy();policy.limits.scalar_work=result.provenance().aggregate_steps;
    const auto exact_work=p.continue_from(prior,a,runtime::ExecutionLease(policy));check(exact_work.provenance().aggregate_steps==policy.limits.scalar_work,"continuation accepts exact aggregate work cap");
    --policy.limits.scalar_work;rejects<std::length_error>([&]{p.continue_from(prior,a,runtime::ExecutionLease(policy));},"aggregate work may not reset after prior receipt");
    policy=ex::associativity_policy();policy.limits.scalar_work=prior->provenance().execution_steps;
    rejects<std::length_error>([&]{p.continue_from(prior,a,runtime::ExecutionLease(policy));},"new work rejects when prior work exhausts budget");
    auto small=ex::associativity_policy();small.limits.scalar_work=prior->provenance().execution_steps;
    const auto tight=std::make_shared<const inference::NativeProductCompositionLift>(p.prefix().apply(x,prefix_a,runtime::ExecutionLease(small)));
    rejects<std::length_error>([&]{p.continue_from(tight,a,ex::associativity_lease());},"larger current budget cannot discard captured prior work authority");
    small=ex::associativity_policy();small.limits.integer_bits=8;
    const auto narrow=std::make_shared<const inference::NativeProductCompositionLift>(p.prefix().apply(x,prefix_a,runtime::ExecutionLease(small)));
    const auto inherited_bits=p.continue_from(narrow,a,ex::associativity_lease());check(inherited_bits.provenance().action_limits.integer_bits==8,"ancestral integer authority retained");
    policy=ex::associativity_policy();policy.limits.rational_slots=result.provenance().requirements.rational_slots;
    const auto exact_space=p.apply(x,a,runtime::ExecutionLease(policy));check(exact_space.provenance().requirements.rational_slots==policy.limits.rational_slots,"exact workspace cap accepted");
    --policy.limits.rational_slots;rejects<runtime::ExecutionLimitExceeded>([&]{p.apply(x,a,runtime::ExecutionLease(policy));},"workspace preflight rejects below bound");
    auto limits=ex::associativity_limits();limits.entries=p.numeric_plan().entry_envelope();Guard guard(limits);
    const auto proof=p.numeric_plan().witness(x.field(),guard);check(proof.routes.size()==5,"complete bound admits witness");
    --limits.entries;bool callback=false;Guard below(limits,[&]{callback=true;});
    rejects<std::length_error>([&]{p.numeric_plan().witness(x.field(),below);},"entry preflight before callbacks");check(!callback,"no callback before preflight");
    rejects<std::length_error>([&]{p.numeric_plan().continue_from(prior->lift(),below);},"continuation preflight precedes receipt copies");check(!callback,"no callback before continuation preflight");
    auto canceled=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.continue_from(prior,a,runtime::ExecutionLease(ex::associativity_policy(),canceled));},"canceled continuation publishes no receipt");
    canceled->store(false);Guard mid(ex::associativity_limits(),[&]{if(!canceled->exchange(true))throw runtime::ExecutionCancelled();});
    rejects<runtime::ExecutionCancelled>([&]{p.numeric_plan().continue_from(prior->lift(),mid);},"mid-action cancellation preserves original receipt");
    check(prior->plan().same_owner(p.prefix()),"failed continuation preserves original plan");
    std::array<std::future<inference::NativeProductAssociativityLift>,4> jobs;
    for(auto& job:jobs)job=std::async(std::launch::async,[p,prior,a]{return p.continue_from(prior,a,ex::associativity_lease());});
    for(auto& job:jobs)check(job.get().last_stage()->lift().image().values()==result.last_stage()->lift().image().values(),"concurrent continuation owns independent work");
    std::optional<inference::NativeProductAssociativityLift> retained;
    {const auto local=ex::plans(6).associativity;const auto input=local.prefix().first().source().field(carrier(0),{0},{},a[0]);retained=local.apply(input,a,ex::associativity_lease());}
    check(retained->input()->plan().same_owner(retained->plan().prefix().first().source()),"result pins destroyed caller's origin");
    bool entered=false;Guard reentry(ex::associativity_limits(),[&]{if(!entered){entered=true;(void)p.continue_from(prior,a,ex::associativity_lease());}});
    (void)p.numeric_plan().apply(x.field(),reentry);check(entered,"reentrant pure action uses independent owned work");
}
void endpoint_authorities() {
    const auto base=ex::plans(6).associativity;const auto a=ex::available_associativity;const auto f=ex::fixture(6);
    const std::array hs{base.prefix().first().source(),base.prefix().first().target(),base.prefix().second().target(),base.last().target()};
    for(std::size_t j=0;j<4;++j)for(std::size_t role=0;role<3;++role){std::array requests{hs[0].declaration(),hs[1].declaration(),hs[2].declaration(),hs[3].declaration()};
        storage::NativeAccessionLimits cap;cap.max_integer_bytes=1;cap.max_action_work=4000000;
        storage::NativeAccessionDirectory d(storage::TemporalEndpoint(requests[j].factors[role].endpoint()->source,a[j][role].max_sequence),a[j][role].source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},cap);
        requests[j].factors[role]=native_product_example::anchor(d);
        const std::array products{Plan::prepare(requests[0],a[0]),Plan::prepare(requests[1],a[1]),Plan::prepare(requests[2],a[2]),Plan::prepare(requests[3],a[3])};
        const auto first=Map::prepare(products[0],products[1],f.prefix.first,{a[0],a[1]},ex::associativity_lease()),second=Map::prepare(products[1],products[2],f.prefix.second,{a[1],a[2]},ex::associativity_lease());
        const auto prefix=ex::Composition::prepare(first,second,3300,4,{a[0],a[1],a[2]},ex::associativity_lease());
        const auto last=Map::prepare(products[2],products[3],f.last,{a[2],a[3]},ex::associativity_lease());
        const auto p=ex::Associativity::prepare(prefix,last,ex::associativity_declaration,a,ex::associativity_lease());
        const auto input=products[0].field(carrier(0),{0},{},a[0]);const auto result=p.apply(input,a,ex::associativity_lease());
        check(result.provenance().action_limits.integer_bits==8 && result.provenance().action_limits.steps==4000000,"all twelve original authorities participate in minimum");
    }
    auto f2=ex::fixture(6);f2.prefix.first.factors[0].matrices[0]=Matrix::from_rows({{Q{7}},{Q{7}}});f2.prefix.second.factors[0].matrices[0]=Matrix::from_rows({{Q{3},Q{-3}}});
    f2.last.factors[0].matrices[0]=Matrix::from_rows({{Q{17}},{Q{-17}}});
    const auto x=endpoint_plan(f2.prefix.source,a[0]),y=endpoint_plan(f2.prefix.intermediate,a[1]),z=endpoint_plan(f2.prefix.target,a[2]),w=endpoint_plan(f2.target,a[3]);
    const auto first=Map::prepare(x,y,f2.prefix.first,{a[0],a[1]},ex::associativity_lease()),second=Map::prepare(y,z,f2.prefix.second,{a[1],a[2]},ex::associativity_lease()),last=Map::prepare(z,w,f2.last,{a[2],a[3]},ex::associativity_lease());
    const auto prefix=ex::Composition::prepare(first,second,3300,4,{a[0],a[1],a[2]},ex::associativity_lease());
    const auto p=ex::Associativity::prepare(prefix,last,ex::associativity_declaration,a,ex::associativity_lease());
    auto policy=ex::associativity_policy();policy.limits.integer_bits=8;
    const auto empty=x.field(carrier(0),{0},{},a[0]);
    const auto prior=std::make_shared<const inference::NativeProductCompositionLift>(prefix.apply(empty,{a[0],a[1],a[2]},runtime::ExecutionLease(policy)));
    rejects<std::length_error>([&]{p.continue_from(prior,a,ex::associativity_lease());},"unused canceled triple terms revalidate ancestral integer cap");
    const auto n=p.numeric_plan();check(n.factors()[0][0].matrix.nonzeros()==0 && n.factors()[0][0].contributions[0].left_value==357,"uncontracted canceled triple values retained");
}
void identities() {
    for(unsigned sample=0;sample<9;++sample){const auto p=ex::plans(sample).associativity;const auto a=ex::available_associativity;
        const auto first=p.prefix().first(),second=p.prefix().second();
        const auto x=first.source(),y=first.target(),z=second.target();
        const auto ix=ex::Composition::identity(x,5000,1,a[0],ex::associativity_lease()),iy=ex::Composition::identity(y,5100,1,a[1],ex::associativity_lease()),iz=ex::Composition::identity(z,5200,1,a[2],ex::associativity_lease());
        for(const auto& id:{ix,iy,iz}){const auto n=id.numeric_plan();for(const auto& f:n.declaration().factors)for(const auto& m:f.matrices)check(m==Matrix::identity(m.rows()),"identity preserves every grade including empty grades");}
        const auto before=ex::Composition::prepare(ix,first,5300,1,{a[0],a[0],a[1]},ex::associativity_lease());
        const auto middle=ex::Composition::prepare(first,iy,5400,1,{a[0],a[1],a[1]},ex::associativity_lease());
        const std::array<inference::NativeProductAssociativityAvailability,3> av{{{a[0],a[0],a[1],a[2]}, {a[0],a[1],a[1],a[2]}, {a[0],a[1],a[2],a[2]}}};
        const std::array variants{ex::Associativity::prepare(before,second,ex::associativity_declaration,av[0],ex::associativity_lease()),
            ex::Associativity::prepare(middle,second,ex::associativity_declaration,av[1],ex::associativity_lease()),
            ex::Associativity::prepare(p.prefix(),iz,ex::associativity_declaration,av[2],ex::associativity_lease())};
        const auto source=x.numeric_plan();
        for(std::size_t ci=0;ci<12;++ci)for(std::uint16_t g=0;g<=source.max_grade().value;++g){Numeric::Values values;const auto c=carrier(ci);
            for(const auto& ids:source.cells())if(ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==g)values.emplace(cell(c,ids),ids[0].index%2 ? Q{0} : Q{2,3});
            const auto input=x.field(c,{g},values,a[0]);const auto expected=p.prefix().apply(input,{a[0],a[1],a[2]},ex::associativity_lease());
            for(std::size_t j=0;j<variants.size();++j){const auto actual=variants[j].apply(input,av[j],ex::associativity_lease());
                check(actual.last_stage()->lift().image().values()==expected.stages()[1]->lift().image().values(),"identity in each position preserves complete supported output");}
        }
    }
}

} // namespace
int main(int argc,char** argv) {
    try { const bool emit=argc==2 && std::string_view(argv[1])=="--emit";exhaustive(emit);if(!emit){contracts();endpoint_authorities();identities();}
        std::cout << "{\"phase\":\"summary\",\"checks\":" << checks << ",\"witnesses\":" << cases << ",\"prefixes\":" << prefixes << ",\"triple_paths\":" << paths << ",\"zero_paths\":" << zero_paths << ",\"boundary_support_differences\":" << boundary_differences << "}\n";
    } catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}
}
