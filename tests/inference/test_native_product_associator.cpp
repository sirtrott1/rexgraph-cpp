#include "../../examples/native_product_associator/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace rfc;
using namespace native_associator_example;
std::size_t checks{}, witnesses{}, cells{};
void check(bool yes, const char* m) { ++checks; if (!yes) throw std::runtime_error(m); }
template<class E, class F> void rejects(F f, const char* m) {
    bool caught = false; try { f(); } catch (const E&) { caught = true; } check(caught,m);
}
void json(core::CellId c) { std::cout << '[' << c.grade.value << ',' << c.index << ']'; }
void json(algebra::ProductCell c) { std::cout << '['; json(c.left); std::cout << ','; json(c.right); std::cout << ']'; }
void json(Left c) { std::cout << '['; json(c.inner); std::cout << ','; json(c.outer); std::cout << ']'; }
void json(Right c) { std::cout << '['; json(c.outer); std::cout << ','; json(c.inner); std::cout << ']'; }
void json(const Cell& c) { std::visit([](const auto& x){ json(x); }, c); }
void json(const std::optional<core::RelationTypeId>& t) { if (t) std::cout << *t; else std::cout << "null"; }
void json(const Numeric::Values& v) {
    std::cout << '['; bool comma = false; for (const auto& [c,q] : v) {
        if (comma) std::cout << ',';
        comma = true; std::cout << '['; json(c); std::cout << ",\"" << q << "\"]";
    } std::cout << ']';
}
void json(const core::RelationalComplex& c) {
    std::cout << "{\"sizes\":[";
    for (std::size_t k = 0; k < c.grade_count(); ++k) { if (k) std::cout << ','; std::cout << c.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(); }
    std::cout << "],\"type_count\":" << c.signature().type_count() << ",\"cells\":["; bool comma = false;
    for (std::size_t k = 0; k < c.grade_count(); ++k) {
        const auto g = core::Grade{static_cast<std::uint16_t>(k)}; const auto& a = c.grade(g);
        for (std::size_t i = 0; i < a.cell_count(); ++i) {
            if (comma) std::cout << ',';
        comma = true; const core::CellId id{g, static_cast<std::uint32_t>(i)};
            std::cout << '['; json(id); std::cout << ',' << algebra::owned_product_detail::identity(c,id).key << ',';
            json(algebra::owned_product_detail::type(c,id)); std::cout << ",\"" << a.cell_scales[i] << "\"]";
        }
    }
    std::cout << "],\"flags\":["; comma = false;
    for (std::size_t k = 1; k < c.grade_count(); ++k) {
        const auto g = core::Grade{static_cast<std::uint16_t>(k)}; const auto& a = c.grade(g);
        for (std::size_t i = 0; i < a.cell_count(); ++i) {
            const auto [begin,end] = a.incidence_range(i);
            for (auto p = begin; p < end; ++p) {
                if (comma) std::cout << ',';
        comma = true;
                std::cout << '[' << k << ',' << i << ',' << a.lower_index[p] << ',' << a.occurrence_identities[p].value << ','
                    << p-begin << ',' << a.aspects.get(c.signature().existence_aspect(),p) << ",\"" << a.lattice_coefficient[p] << "\",[";
                for (std::size_t j = 0; j < c.signature().aspects().size(); ++j) { if (j) std::cout << ','; std::cout << a.aspects.get(static_cast<composite::AspectId>(j),p); }
                std::cout << "]]";
            }
        }
    }
    std::cout << "]}";
}
void json(const algebra::NestedProductOccurrence& o) {
    std::cout << "{\"upper\":"; json(o.upper); std::cout << ",\"lower\":"; json(o.lower);
    std::cout << ",\"path\":["; for (std::size_t i = 0; i < o.path_length; ++i) { if (i) std::cout << ','; std::cout << (o.path[i] == algebra::ProductBoundarySide::Right); }
    std::cout << "],\"factor\":" << o.factor << ",\"position\":" << o.attaching_position << ",\"word_position\":" << o.word_position
        << ",\"occurrence\":" << o.factor_occurrence.value << ",\"flips\":[" << o.inner_koszul_flip << ',' << o.outer_koszul_flip << ',' << o.koszul_flip
        << "],\"exists\":" << o.exists << ",\"coefficient\":\"" << o.coefficient << "\",\"factor_coefficient\":\"" << o.factor_coefficient << "\",\"aspects\":[";
    for (std::size_t i = 0; i < o.factor_aspects.size(); ++i) { if (i) std::cout << ','; std::cout << o.factor_aspects[i]; } std::cout << "]}";
}
void emit_plan(unsigned sample, const Plan& p) {
    const auto numeric = p.numeric_plan(); const auto& d = numeric.declaration();
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"sources\":[";
    for (std::size_t f = 0; f < 3; ++f) { if (f) std::cout << ','; json(numeric.factor(f)); }
    std::cout << "],\"inner_types\":[";
    for (const bool bc : {false,true}) {
        if (bc) std::cout << ',';
        std::cout << '['; bool comma = false;
        for (const auto& t : (bc ? d.bc : d.ab).types) { if (comma) std::cout << ',';
        comma = true; std::cout << '['; json(t.left); std::cout << ','; json(t.right); std::cout << ',' << t.key << ']'; } std::cout << ']';
    }
    std::cout << "],\"outer_types\":[";
    for (const bool right : {false,true}) {
        if (right) std::cout << ',';
        std::cout << '['; bool comma = false;
        for (const auto& t : (right ? d.right : d.left).types) { if (comma) std::cout << ',';
        comma = true; std::cout << '[' << t.inner_type << ','; json(t.factor_type); std::cout << ',' << t.key << ']'; } std::cout << ']';
    }
    std::cout << "],\"types\":["; bool comma = false;
    for (const auto& t : d.types) { if (comma) std::cout << ',';
        comma = true; std::cout << '[' << t.left << ',' << t.right << ']'; }
    std::cout << "],\"grade_counts\":[";
    for (std::size_t k = 0; k <= numeric.max_grade().value; ++k) { if (k) std::cout << ','; std::cout << numeric.cell_count(core::Grade{static_cast<std::uint16_t>(k)}); }
    std::cout << "]}\n";
}
void emit_cell(unsigned sample, const inference::NativeProductAssociatorCellWitness& w) {
    const auto& c = w.witness().cell(); std::cout << "{\"phase\":\"cell\",\"sample\":" << sample << ",\"left\":"; json(c.left);
    std::cout << ",\"right\":"; json(c.right); std::cout << ",\"types\":[" << c.left_type << ',' << c.right_type << "],\"identities\":[";
    const auto identity = [](algebra::ProductFactorIdentity id){ std::cout << '[' << id.grade.value << ',' << id.key << ']'; };
    std::cout << '['; identity(c.left_inner_identity.left); std::cout << ','; identity(c.left_inner_identity.right); std::cout << "],"; identity(c.left_outer_identity);
    std::cout << ','; identity(c.right_outer_identity); std::cout << ",["; identity(c.right_inner_identity.left); std::cout << ','; identity(c.right_inner_identity.right); std::cout << "]],\"flags\":[";
    bool comma = false; for (const auto& f : w.witness().flags()) { if (comma) std::cout << ',';
        comma = true; std::cout << '['; json(f.left); std::cout << ','; json(f.right); std::cout << ']'; }
    std::cout << "]}\n";
}
void exercise(unsigned sample, const Plan& p, Association a, core::Grade g, Numeric::Values v, bool output) {
    const auto input = p.field(a,g,std::move(v),available); const auto w = p.witness(input,available); ++witnesses;
    check(w.plan().same_owner(p) && w.input()->plan().same_owner(p), "chain witness retains actual native owner and input");
    check(w.witness().round_trip().values() == input.field().values(), "round trip preserves all structural support");
    check(w.witness().image().association() != a && w.witness().image().grade() == g, "distinct associated carriers preserve grade");
    const auto mapped = p.reassociate(input,available);
    check(mapped.field().values() == w.witness().image().values() && mapped.input()->plan().same_owner(p), "action and witness share actual owner");
    if (g.value) {
        check(w.witness().mapped_boundary()->values() == w.witness().image_boundary()->values(), "exact chain naturality including zero support");
        if (g.value > 1) {
            const auto d = p.boundary(p.boundary(input,available),available);
            for (const auto& [c,q] : d.field().values()) { (void)c; check(q == 0, "complete nested chain family"); }
        }
    }
    if (output) {
        std::cout << "{\"phase\":\"action\",\"sample\":" << sample << ",\"side\":" << (a == Association::Right) << ",\"grade\":" << g.value << ",\"input\":";
        json(input.field().values()); std::cout << ",\"image\":"; json(mapped.field().values()); std::cout << ",\"round_trip\":"; json(w.witness().round_trip().values());
        std::cout << ",\"boundary\":"; if (g.value) json(w.witness().image_boundary()->values()); else std::cout << "null";
        std::cout << ",\"mapped_boundary\":"; if (g.value) json(w.witness().mapped_boundary()->values()); else std::cout << "null";
        std::cout << ",\"second_boundary\":"; if (g.value > 1) { const auto d = p.boundary(p.boundary(input,available),available); json(d.field().values()); } else std::cout << "null";
        std::cout << "}\n";
    }
}
void samples(bool output) {
    for (unsigned sample = 0; sample < 12; ++sample) {
        const auto p = plan(sample); const auto numeric = p.numeric_plan();
        if (output) emit_plan(sample,p);
        for (const auto& c : numeric.cells()) {
            const auto w = p.cell_witness(c.left,available); ++cells;
            check(w.witness().cell() == c && w.plan().same_owner(p), "complete owning cell certificate");
            for (const auto& f : w.witness().flags()) {
                check(f.left.factor_occurrence == f.right.factor_occurrence && f.left.factor_aspects == f.right.factor_aspects && f.left.attaching_position == f.right.attaching_position,
                    "complete original flags with attaching order and aspect words");
                check(f.left.coefficient == f.right.coefficient && f.left.koszul_flip == f.right.koszul_flip, "two structural Koszul paths give the same exact sign");
            }
            if (output) emit_cell(sample,w);
            for (const Q& value : {Q{0},Q{-3,7}}) for (const auto a : {Association::Left,Association::Right})
                exercise(sample,p,a,c.left.grade(),{{a == Association::Left ? Cell{c.left} : Cell{c.right},value}},output);
        }
        for (std::uint16_t k = 0; k <= numeric.max_grade().value; ++k) for (const auto a : {Association::Left,Association::Right}) {
            exercise(sample,p,a,core::Grade{k},{},output);
            Numeric::Values v; for (const auto& c : numeric.cells()) if (c.left.grade().value == k)
                v.emplace(a == Association::Left ? Cell{c.left} : Cell{c.right}, Q{static_cast<long>(c.left.inner.left.index)-1,3});
            exercise(sample,p,a,core::Grade{k},std::move(v),output);
        }
    }
}
void contracts() {
    const auto p = plan(); const auto n = p.numeric_plan(); const auto c = n.cells().back();
    const auto input = p.field(Association::Left,c.left.grade(),{{c.left,Q{1}}},available);
    const auto prior = p.witness(input,available); const auto bytes = storage::encode_rfcx(p.factor(0).endpoint()->source).bytes;
    const auto f = sources(0); auto d = declaration(f); Guard guard;
    const auto prepare_bad = [&](auto change) { auto bad = d; change(bad); rejects<std::invalid_argument>([&]{ Numeric::prepare(f,bad,guard); }, "malformed complete declaration rejects"); };
    prepare_bad([](auto& x){x.key=0;}); prepare_bad([](auto& x){x.revision=0;});
    prepare_bad([](auto& x){x.ab.types.pop_back();}); prepare_bad([](auto& x){x.bc.types.pop_back();});
    prepare_bad([](auto& x){x.left.types.pop_back();}); prepare_bad([](auto& x){x.right.types.pop_back();}); prepare_bad([](auto& x){x.types.pop_back();});
    prepare_bad([](auto& x){x.left.types.back().inner_type=0;}); prepare_bad([](auto& x){x.right.types.back().factor_type=999;});
    prepare_bad([](auto& x){x.left.types.back()=x.left.types.front();}); prepare_bad([](auto& x){x.right.types.back().key=x.right.types.front().key;});
    prepare_bad([](auto& x){std::swap(x.types.back().right,x.types.front().right);}); prepare_bad([](auto& x){x.types.back()=x.types.front();});
    prepare_bad([](auto& x){x.right.key=0;}); prepare_bad([](auto& x){x.left.revision=0;});
    auto reordered = d; std::reverse(reordered.ab.types.begin(),reordered.ab.types.end()); std::reverse(reordered.bc.types.begin(),reordered.bc.types.end());
    std::reverse(reordered.left.types.begin(),reordered.left.types.end()); std::reverse(reordered.right.types.begin(),reordered.right.types.end()); std::reverse(reordered.types.begin(),reordered.types.end());
    const auto re = Numeric::prepare(f,reordered,guard);
    check(re.declaration().types == reordered.types && re.cells() == n.cells(), "declaration ordering retained without changing declared correspondence");
    rejects<std::invalid_argument>([&]{re.reassociate(input.field(),guard);}, "equal declarations never bind actual field owners");
    rejects<std::invalid_argument>([&]{p.field(Association::Right,c.left.grade(),{{c.left,0}},available);}, "wrong associated zero carrier");
    rejects<std::invalid_argument>([&]{p.field(Association::Left,core::Grade{0},{{c.left,0}},available);}, "zero field still verifies grade");
    auto bad_cell = c.left; bad_cell.outer.index = 999;
    rejects<std::out_of_range>([&]{p.cell_witness(bad_cell,available);}, "cell witness verifies actual full carrier");
    rejects<std::out_of_range>([&]{p.field(Association::Left,core::Grade{7},{},available);}, "empty grade outside declared family");
    rejects<std::invalid_argument>([&]{p.field(static_cast<Association>(99),core::Grade{0},{},available);}, "unknown association rejected on empty input");
    auto malformed=Q{1};mpz_set_ui(mpq_denref(malformed.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{p.field(Association::Left,c.left.grade(),{{c.left,malformed}},available);}, "zero rational denominator rejected");
    for (std::size_t i=0;i<3;++i) {
        auto stale=available; --stale[i].max_sequence;
        rejects<std::invalid_argument>([&]{p.witness(input,stale);}, "every execution cutoff remains independent");
        stale=available;stale[i].source_namespace=999;
        rejects<std::invalid_argument>([&]{p.field(Association::Left,core::Grade{0},{},stale);}, "every namespace checked on empty input");
    }
    auto future=available;for(auto& a:future)a.max_sequence+=100;
    const auto future_witness=p.witness(input,future);check(future_witness.witness().image().values()==prior.witness().image().values(), "later cutoff cannot advance a selected source");
    const auto twin=plan();rejects<std::invalid_argument>([&]{twin.witness(input,available);}, "equal native bytes never bind owners");
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(f[0],7),910), b(storage::TemporalEndpoint(f[1],11),911),
        e(storage::TemporalEndpoint(f[2],19),912);
    auto r=request(a,b,e); const auto old=Plan::prepare(r,available); const auto old_input=old.field(Association::Left,c.left.grade(),{{c.left,1}},available);
    auto alias=r;alias.factor_identities[1]=alias.factor_identities[0]; rejects<std::invalid_argument>([&]{Plan::prepare(alias,available);}, "factor identity cannot alias another actual publication");
    alias=r;alias.factors[1]=alias.factors[0];alias.factor_identities[1]=alias.factor_identities[0];auto alias_available=available;alias_available[1]=alias_available[0];
    check(Plan::prepare(alias,alias_available).factor(0).endpoint()==alias.factors[1].endpoint(), "explicit repeated actual factor is admissible");
    const auto endpoint=a.pin_snapshot();auto next=*endpoint;next.sequence=8;a.advance(storage::make_temporal_segment(*endpoint,next));
    check(old.factor(0).endpoint()==endpoint && old.factor(0).sequence()==7 && old.witness(old_input,available).input()->plan().same_owner(old), "old generation and owner survive advancement");
    rejects<std::invalid_argument>([&]{Plan::prepare(request(a,b,e),available);}, "new publication cannot enter an earlier cutoff");
    auto newer_available=available;newer_available[0].max_sequence=8;const auto newer=Plan::prepare(request(a,b,e),newer_available);
    const auto newer_input=newer.field(Association::Left,c.left.grade(),{{c.left,1}},newer_available);
    rejects<std::invalid_argument>([&]{old.reassociate(newer_input,newer_available);}, "future equal field cannot leak into old owner");
    for (std::size_t i=0;i<3;++i) {
        auto changed=request(a,b,e);changed.factor_identities[i]=0;rejects<std::invalid_argument>([&]{Plan::prepare(changed,newer_available);}, "each factor needs its declared identity");
    }
    runtime::ExecutionHostPolicy policy;policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));}, "owning allocation admission");
    policy={};policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));}, "required unsupported GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(input,available,runtime::ExecutionLease(policy));check(fallback.provenance().fallback, "explicit exact CPU fallback reported");
    const auto required=prior.provenance().requirements;policy={};policy.limits=required;policy.limits.scalar_work=64000000;policy.limits.integer_bits=1u<<24;
    const auto admitted=p.witness(input,available,runtime::ExecutionLease(policy));check(admitted.witness().image().values()==prior.witness().image().values(), "exact at-limit reference admission");
    --policy.limits.rational_slots;rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(input,available,runtime::ExecutionLease(policy));}, "just below rational envelope");
    policy={};policy.limits.records=n.retained_words()-1;rejects<runtime::ExecutionLimitExceeded>([&]{p.field(Association::Left,core::Grade{0},{},available,runtime::ExecutionLease(policy));}, "whole retained source count on empty action");
    policy={};policy.limits.sparse_entries=n.entry_envelope()-1;rejects<runtime::ExecutionLimitExceeded>([&]{p.cell_witness(c.left,available,runtime::ExecutionLease(policy));}, "complete retained certificate count");
    policy={};policy.limits.scalar_work=prior.provenance().execution_steps-1;rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));}, "just below actual action work");
    policy={};policy.limits.integer_bits=8;const Left growth_cell{{{core::Grade{1},0},{core::Grade{0},0}},{core::Grade{1},0}};const auto growth=p.field(Association::Left,growth_cell.grade(),{{growth_cell,Q{253,251}}},available,runtime::ExecutionLease(policy));
    rejects<std::length_error>([&]{p.boundary(growth,available,runtime::ExecutionLease(policy));}, "exact intermediate integer growth");
    auto stop=std::make_shared<std::atomic_bool>(true);rejects<runtime::ExecutionCancelled>([&]{p.witness(input,available,runtime::ExecutionLease({},stop));}, "owned native cancellation");
    auto rsmall=r;rsmall.dimension_limit=1;rejects<std::length_error>([&]{Plan::prepare(rsmall,available);}, "complete dimension bounds before zero action");
    const auto source_copy=f;auto copied=d;bool called=false;Guard checkpoint({},[&]{if(!called){called=true;copied.types.clear();}});
    const auto frozen=Numeric::prepare(source_copy,copied,checkpoint);check(frozen.declaration().types==d.types, "constructor copied before caller mutation callback");
    auto moved=d;auto& moved_alias=moved.types.front().right;Guard moved_guard({},[&]{moved_alias=999;});
    const auto move_frozen=Numeric::prepare(f,std::move(moved),moved_guard);
    check(move_frozen.declaration().types==d.types, "moved constructor element aliases cannot mutate owned declarations");
    Numeric::Values moved_values{{c.left,Q{1}}};auto& value_alias=moved_values.begin()->second;Guard field_checkpoint({},[&]{value_alias=2;});
    const auto field_frozen=n.field(Association::Left,c.left.grade(),std::move(moved_values),field_checkpoint);
    check(field_frozen.values().at(c.left)==1, "moved field element aliases cannot mutate the captured input");
    auto disposable=std::make_unique<Numeric>(n);Guard destroy({},[&]{disposable.reset();});
    const auto kept=disposable->cell_witness(c.left,destroy);check(kept.plan().same_owner(n), "self destruction at checkpoint retains owner");
    std::vector<std::future<inference::NativeProductAssociatorChainWitness>> jobs;
    for(unsigned i=0;i<6;++i)jobs.push_back(std::async(std::launch::async,[p,input]{return p.witness(input,available);}));
    for(auto& job:jobs){const auto result=job.get();check(result.witness().image().values()==prior.witness().image().values(), "owned concurrent calls have independent scratch");}
    const auto retained=p.witness(input,available);check(storage::encode_rfcx(p.factor(0).endpoint()->source).bytes==bytes && retained.witness().image().values()==prior.witness().image().values(), "failures preserve original bytes and results");
    for (std::size_t limited=0;limited<3;++limited) {
        std::array<std::unique_ptr<storage::NativeAccessionDirectory>,3> dirs;
        for(std::size_t i=0;i<3;++i) {
            storage::NativeAccessionLimits cap;
            if(i==limited){cap.max_integer_bytes=1;cap.max_action_work=3999999;}
            dirs[i]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[i],available[i].max_sequence),
                available[i].source_namespace,std::vector<storage::RelationalCodeObservationQuery>{},
                [](auto data){return storage::rfcx_detail::fnv1a64(data);},cap);
        }
        const auto bounded=Plan::prepare(request(*dirs[0],*dirs[1],*dirs[2]),available);
        const auto bounded_input=bounded.field(Association::Left,growth_cell.grade(),{{growth_cell,Q{253,251}}},available);
        check(bounded_input.provenance().action_limits.integer_bits==8 && bounded_input.provenance().action_limits.steps==3999999,
            "all three actual source authorities contribute integer and work caps");
        rejects<std::length_error>([&]{bounded.boundary(bounded_input,available);}, "each source authority bounds arithmetic on another factor");
    }
    core::RelationSignature unused_signature;
    core::RelationConstraints constraint;constraint.rules.push_back(core::RelationRule::role_magnitude(unused_signature.existence_aspect(),Q{509,503}));
    (void)unused_signature.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraint);
    core::RelationalComplexBuilder unused_builder(unused_signature);unused_builder.declare_grade(core::Grade{2});
    auto empty_sources=sources(10);empty_sources[1]=unused_builder.freeze();auto empty_declaration=declaration(empty_sources);
    numeric::ExactActionLimits empty_limit;empty_limit.integer_bits=8;Guard unused_guard(empty_limit);
    rejects<std::length_error>([&]{Numeric::prepare(empty_sources,empty_declaration,unused_guard);}, "unused exact rules are admitted even without any product cells");
    empty_declaration=declaration(empty_sources);empty_declaration.types.pop_back();
    rejects<std::invalid_argument>([&]{Numeric::prepare(empty_sources,empty_declaration,guard);}, "empty products still require all unused constructor triples");
    rejects<std::overflow_error>([&]{(void)algebra::product_associator_detail::mul(std::numeric_limits<std::size_t>::max(),2);}, "checked population products");
}
} // namespace
int main(int argc,char** argv) {
    const bool records=argc==2 && std::string_view(argv[1])=="--records";
    try { samples(records); if(!records){contracts();std::cout<<"checks="<<checks<<" cells="<<cells<<" witnesses="<<witnesses<<'\n';} }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} return 0;
}
