#include "../../examples/native_product_chain_map/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_map_example;
std::size_t checks{}, cases{}, contributions{}, support_differences{};
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
void emit_plan(unsigned sample, const Fixture& f) {
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"source\":"; array(f.source,[](const auto& c){source(c);});
    std::cout << ",\"target\":"; array(f.target,[](const auto& c){source(c);});
    std::cout << ",\"maps\":";
    array(f.maps.factors,[](const auto& m){std::cout << '[';
        for (std::size_t k = 0; k < m.matrices.size(); ++k) {
            if (k) std::cout << ',';
            const auto& matrix = m.matrices[k];
            std::cout << "{\"shape\":[" << matrix.rows() << ',' << matrix.cols() << "],\"support\":";
            array(m.support[k],[&](const auto& site){std::cout << '[' << site.first << ',' << site.second << ",\"" << matrix.get(site.first,site.second) << "\"]";});
            std::cout << '}';
        }
        std::cout << ']';}); std::cout << "}\n";
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
void verify_comparison(const algebra::ProductMapComparison& c) {
    check(c.mapped_then_operation.plan().same_owner(c.operation_then_mapped.plan()),"comparison owns actual target");
    for (const auto& id : c.only_first_support) check(c.mapped_then_operation.values().at(id)==0 && !c.operation_then_mapped.values().contains(id),"first-only support is retained zero");
    for (const auto& id : c.only_second_support) check(c.operation_then_mapped.values().at(id)==0 && !c.mapped_then_operation.values().contains(id),"second-only support is retained zero");
    support_differences += c.only_first_support.size()+c.only_second_support.size();
}
void verify_lift(const algebra::ProductMapLift& w) {
    const auto p = w.plan(); const auto carrier = p.source().carrier(w.input());
    check(p.target().carrier(w.image())==carrier,"lift uses actual matching target carrier");
    for (std::size_t f = 0; f < 3; ++f) {
        const auto& s = w.stages()[f]; check(s.original_factor==f,"stage role is original");
        for (std::size_t j = 0; j < 3; ++j) check(s.input_target[j]==(j<f) && s.image_target[j]==(j<=f),"mixed stage names source and target roles");
        if (f) check(s.input==w.stages()[f-1].image,"stage order retains previous result");
        for (const auto& c : s.contributions) {
            check(c.input_value*c.coefficient==c.value,"uncontracted map contribution exact");
            check(c.input_value==s.input.at(c.input),"contribution binds actual source value");
            check(c.matrix_position==Matrix::Key{c.image[f].index,c.input[f].index},"matrix positions declared");
            for (std::size_t j = 0; j < 3; ++j) check(c.input[j].grade==c.image[j].grade && (j==f || c.input[j]==c.image[j]),"map is degree zero on original factor");
            ++contributions;
        }
    }
}
void exhaustive(bool emit) {
    for (unsigned sample = 0; sample < 6; ++sample) {
        const auto f = fixture(sample); const auto p = native_map_example::plan(sample); const auto sn = p.source().numeric_plan(), tn = p.target().numeric_plan();
        if (emit) emit_plan(sample,f);
        for (std::size_t ci = 0; ci < 12; ++ci) for (std::uint16_t g = 0; g <= sn.max_grade().value; ++g) {
            const auto c = carrier(ci); Numeric::Values values;
            for (const auto& ids : sn.cells()) if (ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==g) {
                const auto sum = ids[0].index+ids[1].index+ids[2].index;
                values.emplace(cell(c,ids),sum%3==0 ? Q{0} : sum%3==1 ? Q{-2,3} : Q{5,7});
            }
            // Top directions get a nonzero value as well as supported zeros.
            if (!values.empty() && g) values.begin()->second=Q{1};
            const auto input = p.source().field(c,{g},values,map_available.source); const auto nw = p.witness(input,map_available,map_lease()); const auto& w = nw.witness(); ++cases;
            check(nw.input()->plan().same_owner(p.source()) && nw.plan().same_owner(p),"native witness retains actual input and map");
            check(w.boundary().has_value()==(g!=0),"no boundary invented at grade zero");
            for (std::size_t j = 0; j < 2; ++j) for (std::size_t role = 0; role < 3; ++role) {
                const auto a = j ? map_available.target : map_available.source;
                check(nw.provenance().namespaces[j][role]==a[role].source_namespace && nw.provenance().sequences[j][role]==a[role].max_sequence,"six cutoffs stay in original order");
            }
            for (const auto& r : w.routes()) {
                check(r.operations.size()+1==r.prefix_maps.size() && r.source_path.size()==r.target_path.size() && r.target_path.size()==r.prefix_maps.size(),"every primitive prefix retained");
                for (const auto& m : r.prefix_maps) verify_lift(m);
                for (const auto& cmp : r.comparisons) verify_comparison(cmp);
            }
            check(w.routes()[3].operations.size()==(ci%2 ? 7u : 5u) && w.routes()[4].operations.size()==(ci%2 ? 5u : 7u),"canonical Yang-Baxter word lengths");
            verify_comparison(w.yang_baxter());
            if (w.boundary()) { verify_lift(w.boundary()->input_map);verify_lift(w.boundary()->boundary_map);verify_comparison(w.boundary()->comparison); }
            if (emit) {
                std::cout << "{\"phase\":\"witness\",\"sample\":" << sample << ",\"input\":";field(input.field(),sn);
                std::cout << ",\"routes\":";array(w.routes(),[&](const auto& r){std::cout << "{\"operations\":";array(r.operations,[](auto a){std::cout << static_cast<unsigned>(a);});
                    std::cout << ",\"source\":";array(r.source_path,[&](const auto& x){field(x,sn);});std::cout << ",\"target\":";array(r.target_path,[&](const auto& x){field(x,tn);});
                    std::cout << ",\"maps\":";array(r.prefix_maps,[&](const auto& x){lift(x,sn,tn);});
                    std::cout << ",\"comparisons\":";array(r.comparisons,[&](const auto& x){comparison(x,tn);});std::cout << '}';});
                std::cout << ",\"boundary\":";
                if (w.boundary()) { std::cout << "{\"input_map\":";lift(w.boundary()->input_map,sn,tn);std::cout << ",\"boundary_map\":";lift(w.boundary()->boundary_map,sn,tn);std::cout << ",\"comparison\":";comparison(w.boundary()->comparison,tn);std::cout << '}'; }
                else std::cout << "null";
                std::cout << ",\"yang_baxter\":";comparison(w.yang_baxter(),tn);std::cout << "}\n";
            }
        }
    }
    check(support_differences>0,"naturality preserves unequal zero supports");
}
void negative_and_ownership() {
    const auto p = native_map_example::plan(); const auto sn = p.source().numeric_plan(), tn = p.target().numeric_plan(); const auto f = fixture();
    const auto empty = p.source().field(carrier(0),{0},{},map_available.source);
    auto bad = f.maps; bad.factors[0].matrices.pop_back(); Guard guard(pure_limits());
    rejects<std::invalid_argument>([&]{algebra::PreparedProductChainMap::prepare(sn,tn,bad,guard);},"complete grade map required");
    bad=f.maps;bad.factors[0].support[0].erase({0,0});
    rejects<std::invalid_argument>([&]{algebra::PreparedProductChainMap::prepare(sn,tn,bad,guard);},"nonzero direction cannot be omitted");
    bad=f.maps;bad.factors[0].support[0].insert({9,0});
    rejects<std::invalid_argument>([&]{algebra::PreparedProductChainMap::prepare(sn,tn,bad,guard);},"foreign support position rejects");
    bad=f.maps;bad.factors[0].matrices[1].set(1,0,Q{2});
    rejects<std::invalid_argument>([&]{algebra::PreparedProductChainMap::prepare(sn,tn,bad,guard);},"bad rectangular chain map rejects");
    const auto micro = native_map_example::plan(2); bad=fixture(2).maps;bad.factors[0].matrices[2].set(1,0,Q{0});bad.factors[0].support[2].erase({1,0});
    rejects<std::invalid_argument>([&]{algebra::PreparedProductChainMap::prepare(micro.source().numeric_plan(),micro.target().numeric_plan(),bad,guard);},"one triangle does not realize square");
    auto missing=fixture(5);missing.target[0]=native_product_example::unit(true,1);
    const auto missing_source=endpoint_plan(missing.source,map_available.source),missing_target=endpoint_plan(missing.target,map_available.target);
    rejects<std::invalid_argument>([&]{Map::prepare(missing_source,missing_target,missing.maps,map_available,map_lease());},"target cannot silently drop an empty source grade");
    const auto foreign = native_map_example::plan(); const auto fi = foreign.source().field(carrier(0),{0},{},map_available.source);
    rejects<std::invalid_argument>([&]{p.apply(fi,map_available,map_lease());},"equal empty field from foreign owner rejects");
    rejects<std::invalid_argument>([&]{p.numeric_plan().apply(fi.field(),guard);},"numeric equal carrier is not identity");
    for (std::size_t side = 0; side < 2; ++side) for (std::size_t role = 0; role < 3; ++role) {
        auto a=map_available;auto& slot=side ? a.target[role] : a.source[role];--slot.max_sequence;
        rejects<std::invalid_argument>([&]{p.witness(empty,a,map_lease());},"each independent past availability rejects");
        a=map_available;(side ? a.target[role] : a.source[role]).source_namespace=99;
        rejects<std::invalid_argument>([&]{Map::prepare(p.source(),p.target(),f.maps,a,map_lease());},"each independent foreign namespace rejects");
    }
    auto later=map_available;for(auto& a:later.source)++a.max_sequence;for(auto& a:later.target)++a.max_sequence;
    const auto old=p.apply(empty,later,map_lease());check(old.provenance().sequences[0][0]==7 && old.provenance().sequences[1][0]==23,"later availability does not substitute later endpoints");
    const auto future=endpoint_plan(f.source,later.source); const auto future_input=future.field(carrier(0),{0},{},later.source);
    rejects<std::invalid_argument>([&]{p.apply(future_input,later,map_lease());},"future equal source field rejects");
    auto policy=map_policy();policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.apply(empty,map_available,runtime::ExecutionLease(policy));},"allocation disabled on empty lift");
    const auto expensive=native_map_example::plan(1);
    const auto expensive_empty=expensive.source().field(carrier(0),{0},{},map_available.source);
    rejects<runtime::ExecutionLimitExceeded>([&]{expensive.apply(expensive_empty,map_available);},"example does not silently enlarge default workspace");
    policy=map_policy();policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.apply(empty,map_available,runtime::ExecutionLease(policy));},"required unsupported backend rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.apply(empty,map_available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"explicit fallback retained");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.apply(empty,map_available,runtime::ExecutionLease({},stop));},"owned cancellation honored");
    const auto baseline=p.witness(empty,map_available,map_lease());policy=map_policy();policy.limits.scalar_work=baseline.provenance().execution_steps;
    const auto at_work=p.witness(empty,map_available,runtime::ExecutionLease(policy));check(at_work.provenance().execution_steps==policy.limits.scalar_work,"whole witness exact work boundary");
    --policy.limits.scalar_work;rejects<std::length_error>([&]{p.witness(empty,map_available,runtime::ExecutionLease(policy));},"prefixes cannot reset whole witness budget");
    policy=map_policy();policy.limits.rational_slots=baseline.provenance().requirements.rational_slots-1;
    rejects<runtime::ExecutionLimitExceeded>([&]{p.apply(empty,map_available,runtime::ExecutionLease(policy));},"full retained envelope admitted even on empty input");
    bad=f.maps;bad.factors[1].matrices[2].set(0,0,Q{256});Guard large(pure_limits());const auto compiled=algebra::PreparedProductChainMap::prepare(sn,tn,bad,large);
    numeric::ExactActionLimits small=pure_limits();small.integer_bits=8;Guard bits(small);
    rejects<std::length_error>([&]{compiled.apply(empty.field(),bits);},"empty action revalidates unused map coefficients");
    for(std::size_t side=0;side<2;++side) for(std::size_t role=0;role<3;++role) {
        // One bounded factor pin reduces authority for all six-role map work.
        storage::NativeAccessionLimits cap;cap.max_integer_bytes=1;cap.max_action_work=4000000;
        const auto sh=p.source(),th=p.target();auto src=sh.declaration(),tgt=th.declaration();auto& r=side ? tgt : src;const auto a=side ? map_available.target : map_available.source;
        storage::NativeAccessionDirectory directory(storage::TemporalEndpoint(r.factors[role].endpoint()->source,a[role].max_sequence),a[role].source_namespace,{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},cap);
        r.factors[role]=native_product_example::anchor(directory);
        const auto sp=Plan::prepare(src,map_available.source),tp=Plan::prepare(tgt,map_available.target);
        const auto bounded=Map::prepare(sp,tp,f.maps,map_available,map_lease());const auto in=sp.field(carrier(0),{0},{},map_available.source);
        const auto out=bounded.apply(in,map_available,map_lease());
        check(out.provenance().action_limits.integer_bits==8 && out.provenance().action_limits.steps==4000000,"all six minimum authorities admitted");
        const core::CellId v{{0},0},e{{1},0};const auto c=carrier(0);
        const auto growth=sp.field(c,{1},{{cell(c,{v,e,v}),Q{253,251}}},map_available.source);
        rejects<std::length_error>([&]{bounded.witness(growth,map_available,map_lease());},"each factor authority bounds exact growth on another role");
    }
    core::RelationSignature signature;core::RelationConstraints constraints;
    constraints.rules.push_back(core::RelationRule::role_magnitude(signature.existence_aspect(),Q{509,503}));
    (void)signature.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraints);
    core::RelationalComplexBuilder builder(signature);builder.declare_grade({2});const auto vacant=builder.freeze();
    const std::array vacant_factors{vacant,vacant,vacant};const auto vacant_source=endpoint_plan(vacant_factors,map_available.source),vacant_target=endpoint_plan(vacant_factors,map_available.target);
    Declaration vacant_maps{1,1,{}};for(auto& m:vacant_maps.factors)m=FactorMap::nonzero_support({Matrix(0,0),Matrix(0,0),Matrix(0,0)});
    const auto vacant_plan=Map::prepare(vacant_source,vacant_target,vacant_maps,map_available,map_lease());
    const auto vacant_input=vacant_source.field(carrier(0),{0},{},map_available.source);policy=map_policy();policy.limits.integer_bits=8;
    rejects<std::length_error>([&]{vacant_plan.witness(vacant_input,map_available,runtime::ExecutionLease(policy));},"unused rule scalars revalidate on empty carriers");
    auto moved=f.maps;auto* alias=&moved.factors[0].matrices[1];bool changed=false;
    Guard freeze(pure_limits(),[&]{if(!changed){changed=true;alias->set(1,0,Q{9});}});
    const auto frozen=algebra::PreparedProductChainMap::prepare(sn,tn,std::move(moved),freeze);
    check(frozen.declaration().factors[0].matrices[1].get(1,0)==1,"map elements freeze before checkpoints");
    auto handle=p.numeric_plan();bool replaced=false;Guard reentrant(pure_limits(),[&]{if(!replaced){replaced=true;handle=foreign.numeric_plan();}});
    const auto retained=handle.witness(empty.field(),reentrant);check(retained.plan().same_owner(p.numeric_plan()),"witness pins service before handle replacement");
    std::array<std::future<inference::NativeProductMapLift>,4> tasks;
    for(auto& task:tasks)task=std::async(std::launch::async,[&]{return p.apply(empty,map_available,map_lease());});
    for(auto& task:tasks){const auto out=task.get();check(out.plan().same_owner(p) && out.lift().image().values().empty(),"concurrent actions own independent scratch");}
}
} // namespace
int main(int argc, char** argv) {
    const bool emit=argc==2 && std::string_view(argv[1])=="--emit";
    if(argc>2 || (argc==2 && !emit))return 2;
    try { exhaustive(emit);if(!emit){negative_and_ownership();std::cout << "checks=" << checks << " witnesses=" << cases << " contributions=" << contributions << " support_differences=" << support_differences << '\n';} }
    catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}
}
