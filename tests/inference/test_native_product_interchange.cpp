#include "../../examples/native_product_interchange/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_interchange_example;
std::size_t checks{}, witnesses{}, cells{};
void check(bool yes,const char* m) { ++checks; if (!yes) throw std::runtime_error(m); }
template<class E,class F> void rejects(F f,const char* m) {
    bool caught=false; try { f(); } catch (const E&) { caught=true; } check(caught,m);
}
void json(core::CellId c) { std::cout << '[' << c.grade.value << ',' << c.index << ']'; }
void json(algebra::ProductCell c) { std::cout << '['; json(c.left); std::cout << ','; json(c.right); std::cout << ']'; }
void json(const std::optional<core::RelationTypeId>& t) { if (t) std::cout << *t; else std::cout << "null"; }
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
template<class R,class F> void array(const R& xs,F fn) {
    std::cout << '['; bool comma=false; for (const auto& x : xs) { if (comma) std::cout << ','; comma=true; fn(x); } std::cout << ']';
}
void json(const Numeric::Values& v) {
    array(v,[](const auto& x){std::cout << '['; json(x.first); std::cout << ",\"" << x.second << "\"]";});
}
void json(const algebra::RetainedProductOccurrence& o) {
    std::cout << "{\"upper\":"; json(o.upper); std::cout << ",\"lower\":"; json(o.lower);
    std::cout << ",\"side\":" << (o.side == algebra::ProductBoundarySide::Right) << ",\"position\":" << o.attaching_position
        << ",\"occurrence\":" << o.factor_occurrence.value << ",\"flip\":" << o.koszul_flip << ",\"exists\":" << o.exists
        << ",\"factor_coefficient\":\"" << o.factor_coefficient << "\",\"coefficient\":\"" << o.coefficient << "\",\"aspects\":";
    array(o.factor_aspects,[](bool x){std::cout << x;}); std::cout << '}';
}
void emit_plan(unsigned sample,const Plan& p) {
    const auto n=p.numeric_plan(); const auto& d=n.declaration();
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"sources\":";
    array(std::array{p.factor(0),p.factor(1)},[](const auto& x){json(x.endpoint()->source);});
    std::cout << ",\"products\":";
    array(std::array{d.ab,d.ba},[](const auto& x){array(x.types,[](const auto& t){std::cout << '['; json(t.left); std::cout << ','; json(t.right); std::cout << ',' << t.key << ']';});});
    std::cout << ",\"types\":"; array(d.types,[](const auto& t){std::cout << '[' << t.ab << ',' << t.ba << ']';});
    std::cout << ",\"grade_counts\":[";
    const auto ab=n.product(Order::AB);
    for (std::uint16_t g=0;g<=n.max_grade().value;++g) { if (g) std::cout << ','; std::cout << ab.cell_count({g}); }
    std::cout << "]}\n";
}
void emit_cell(unsigned sample,const inference::NativeProductInterchangeCellWitness& native) {
    const auto& w=native.witness();
    std::cout << "{\"phase\":\"cell\",\"sample\":" << sample << ",\"order\":" << (w.order() == Order::BA) << ",\"source\":";
    json(w.source().cell); std::cout << ",\"target\":"; json(w.target().cell);
    const auto identity=[](const auto& x){std::cout << "[[" << x.left.grade.value << ',' << x.left.key << "],[" << x.right.grade.value << ',' << x.right.key << "]]";};
    std::cout << ",\"identities\":["; identity(w.source().source_identity); std::cout << ','; identity(w.target().source_identity);
    std::cout << "],\"types\":[" << w.source().type << ',' << w.target().type << "],\"flip\":" << w.interchange_flip();
    std::cout << ",\"source_flags\":"; array(w.source_flags(),[](const auto& o){json(o);});
    std::cout << ",\"target_flags\":"; array(w.target_flags(),[](const auto& o){json(o);});
    std::cout << ",\"correspondence\":"; array(w.flags(),[](const auto& f){std::cout << '[' << f.source_word_position << ',' << f.target_word_position << ','
        << f.upper_interchange_flip << ',' << f.lower_interchange_flip << ",\"" << f.mapped_boundary_coefficient << "\",\"" << f.image_boundary_coefficient << "\"]";});
    std::cout << "}\n";
}
void exercise(unsigned sample,const Plan& p,Order order,core::Grade g,Numeric::Values values,bool output) {
    const auto n=p.numeric_plan(); const auto input=p.field(order,g,std::move(values),available);
    const auto native=p.witness(input,available); const auto& w=native.witness(); ++witnesses;
    const auto target=algebra::product_interchange_detail::opposite(order);
    check(native.plan().same_owner(p) && native.input()->plan().same_owner(p) && w.plan().same_owner(n),"actual owning witness and input retained");
    check(n.order(w.image()) == target && n.order(w.round_trip()) == order && w.round_trip().values() == input.field().values(),"both directions round trip with full support");
    check(!input.input() && input.provenance().source_order == order && input.provenance().target_order == order,"declared field has its own product order");
    const auto image=p.interchange(input,available), back=p.interchange(image,available);
    check(image.field().values() == w.image().values() && back.field().values() == input.field().values(),"native actions match the owned exact witness");
    check(image.input()->provenance().action == inference::NativeProductInterchangeAction::DeclaredField &&
        back.input()->provenance().action == inference::NativeProductInterchangeAction::Interchange && back.input()->input(),"round trip retains both ordered operations even for equal or zero output");
    check(image.provenance().source_order == order && image.provenance().target_order == target &&
        image.provenance().namespaces == std::array<std::uint64_t,2>{910,911} && image.provenance().sequences == std::array<std::uint64_t,2>{7,11},"BA actions keep the original A/B temporal roles");
    std::optional<inference::DerivedNativeInterchangeField> dd;
    if (g.value) {
        const auto mapped=p.interchange(p.boundary(input,available),available), direct=p.boundary(image,available);
        check(mapped.field().values() == direct.field().values() && w.mapped_boundary()->values() == direct.field().values(),"signed interchange is an exact chain map including supported cancellation");
        if (g.value > 1) {
            dd=p.boundary(p.boundary(input,available),available);
            for (const auto& [c,q] : dd->field().values()) { (void)c; check(q == 0,"complete product chain family"); }
        }
    } else check(!w.mapped_boundary() && !w.image_boundary(),"grade zero has no invented boundary");
    if (output) {
        std::cout << "{\"phase\":\"action\",\"sample\":" << sample << ",\"order\":" << (order == Order::BA) << ",\"grade\":" << g.value << ",\"input\":";
        json(input.field().values()); std::cout << ",\"image\":"; json(w.image().values()); std::cout << ",\"round_trip\":"; json(w.round_trip().values());
        std::cout << ",\"mapped_boundary\":"; if (w.mapped_boundary()) json(w.mapped_boundary()->values()); else std::cout << "null";
        std::cout << ",\"image_boundary\":"; if (w.image_boundary()) json(w.image_boundary()->values()); else std::cout << "null";
        std::cout << ",\"dd\":"; if (dd) json(dd->field().values()); else std::cout << "null"; std::cout << "}\n";
    }
}
void samples(bool output) {
    for (unsigned sample=0;sample<16;++sample) {
        const auto p=plan(sample); const auto n=p.numeric_plan(); if (output) emit_plan(sample,p);
        check(!n.product(Order::AB).same_owner(n.product(Order::BA)),"AB and BA remain distinct actual owners even for repeated factors");
        for (const auto c : n.cells()) for (const auto order : {Order::AB,Order::BA}) {
            const auto id=order == Order::AB ? c : algebra::product_interchange_detail::image(c);
            const auto native=p.cell_witness(order,id,available); const auto& w=native.witness(); ++cells;
            check(w.source().product().same_owner(n.product(order)) && w.target().product().same_owner(n.product(algebra::product_interchange_detail::opposite(order))),"cell certificates own both canonical products");
            std::set<std::size_t> target_positions;
            for (const auto& flag : w.flags()) {
                check(flag.source.product().same_owner(w.source().product()) && flag.target.product().same_owner(w.target().product()),"flag witnesses retain actual product owners");
                check(flag.source.factor_occurrence == flag.target.factor_occurrence && flag.source.factor_aspects == flag.target.factor_aspects &&
                    flag.source.attaching_position == flag.target.attaching_position && flag.source.factor_coefficient == flag.target.factor_coefficient &&
                    flag.source.exists == flag.target.exists && flag.source.side != flag.target.side,"full original flags preserved separately from interchange and product parity");
                check(target_positions.insert(flag.target_word_position).second && flag.source_word_position < w.source_flags().size() &&
                    flag.target_word_position < w.target_flags().size() && flag.mapped_boundary_coefficient == flag.image_boundary_coefficient,"explicit boundary block permutation and exact signed coefficient identity");
            }
            check(target_positions.size() == w.target_flags().size(),"bijection covers every inactive and zero flag");
            if (output) emit_cell(sample,native);
            for (const Q& q : {Q{0},Q{-3,7}}) exercise(sample,p,order,id.grade(),{{id,q}},output);
        }
        for (const auto order : {Order::AB,Order::BA}) for (std::uint16_t g=0;g<=n.max_grade().value;++g) {
            exercise(sample,p,order,{g},{},output); Numeric::Values values;
            for (const auto c : n.cells()) if (c.grade().value == g) {
                const auto id=order == Order::AB ? c : algebra::product_interchange_detail::image(c);
                values.emplace(id,Q{static_cast<long>(c.left.index)-1,3});
            }
            exercise(sample,p,order,{g},std::move(values),output);
        }
    }
}
void contracts() {
    const auto p=plan(); const auto n=p.numeric_plan(); const Cell c{{{1},0},{{1},0}};
    const auto input=p.field(Order::AB,{2},{{c,Q{1}}},available), prior_input=input;
    const auto prior=p.witness(input,available); const auto f=sources(0); const auto d=declaration(f); Guard guard;
    const std::array bytes{storage::encode_rfcx(p.factor(0).endpoint()->source).bytes,storage::encode_rfcx(p.factor(1).endpoint()->source).bytes};
    const auto bad=[&](auto change){auto x=d; change(x); rejects<std::invalid_argument>([&]{Numeric::prepare(f,x,guard);},"complete malformed interchange declaration rejects");};
    bad([](auto& x){x.key=0;}); bad([](auto& x){x.revision=0;});
    bad([](auto& x){x.ab.key=0;}); bad([](auto& x){x.ba.key=0;}); bad([](auto& x){x.ab.revision=0;}); bad([](auto& x){x.ba.revision=0;});
    bad([](auto& x){x.ab.types.pop_back();}); bad([](auto& x){x.ba.types.pop_back();}); bad([](auto& x){x.types.pop_back();});
    bad([](auto& x){x.ab.types.back()=x.ab.types.front();}); bad([](auto& x){x.ba.types.back().left=999;});
    bad([](auto& x){x.types.back()=x.types.front();}); bad([](auto& x){x.types.back().ab=999;}); bad([](auto& x){x.types.back().ba=999;});
    bad([](auto& x){std::swap(x.types.front().ba,x.types.back().ba);});
    auto reordered=d; std::reverse(reordered.ab.types.begin(),reordered.ab.types.end()); std::reverse(reordered.ba.types.begin(),reordered.ba.types.end());
    std::reverse(reordered.types.begin(),reordered.types.end()); const auto re=Numeric::prepare(f,reordered,guard);
    check(re.declaration().types == reordered.types && re.declaration().ab.types == reordered.ab.types && re.cells() == n.cells(),"type declaration order remains independent from canonical correspondence");
    rejects<std::invalid_argument>([&]{re.witness(input.field(),guard);},"equal pure values cannot substitute an owner");
    const auto twin=plan(); rejects<std::invalid_argument>([&]{twin.witness(input,available);},"equal native values cannot substitute an owner");
    rejects<std::invalid_argument>([&]{twin.interchange(input,available);},"native interchange rejects another plan");
    rejects<std::invalid_argument>([&]{n.boundary(twin.numeric_plan().field(Order::AB,{2},{{c,0}},guard),guard);},"supported zero cannot bypass pure ownership");
    rejects<std::invalid_argument>([&]{p.field(Order::AB,{0},{{c,0}},available);},"supported zero checks grade");
    rejects<std::invalid_argument>([&]{p.field(static_cast<Order>(99),{0},{},available);},"empty input rejects unknown factor order");
    rejects<std::out_of_range>([&]{p.field(Order::AB,{9},{},available);},"empty input rejects an undeclared grade");
    auto invalid=c; invalid.right.index=999;
    rejects<std::out_of_range>([&]{p.cell_witness(Order::AB,invalid,available);},"full factor cell identities checked");
    const auto zero=p.field(Order::AB,{0},{},available);
    rejects<std::invalid_argument>([&]{p.boundary(zero,available);},"grade zero has no boundary");
    auto q=Q{1}; mpz_set_ui(mpq_denref(q.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{p.field(Order::AB,{2},{{c,q}},available);},"malformed rational rejects before arithmetic");
    Q unreduced{1}; mpz_set_ui(mpq_numref(unreduced.get_mpq_t()),2); mpz_set_ui(mpq_denref(unreduced.get_mpq_t()),6);
    const auto canon=p.field(Order::BA,{2},{{c,unreduced}},available);
    check(canon.field().values().at(c) == Q{1,3},"declared rational input is exact canonical bytes");
    // The unsigned coordinate swap is not a chain map on two odd grades.
    const auto signed_image=p.interchange(input,available);
    const auto unsigned_image=p.field(Order::BA,{2},{{c,Q{1}}},available);
    const auto mapped=p.interchange(p.boundary(input,available),available), unsigned_boundary=p.boundary(unsigned_image,available);
    check(mapped.field().values() != unsigned_boundary.field().values() && signed_image.field().values().at(c) == -1,"odd parity is necessary for chain naturality");
    const auto same_keys=p.field(Order::BA,{2},{{c,Q{1}}},available);
    check(input.field().values() == same_keys.field().values() && !input.field().product().same_owner(same_keys.field().product()),"equal positions and values do not identify AB and BA");
    for (std::size_t j=0;j<2;++j) {
        auto stale=available; --stale[j].max_sequence;
        rejects<std::invalid_argument>([&]{Plan::prepare(p.declaration(),stale);},"each preparation cutoff rejects before namespace changes");
        for (const auto order : {Order::AB,Order::BA}) {
            const auto in=p.field(order,{2},{{c,Q{1}}},available);
            rejects<std::invalid_argument>([&]{p.witness(in,stale);},"BA and AB independently admit original factor cutoffs");
        }
        stale=available; stale[j].source_namespace=999;
        rejects<std::invalid_argument>([&]{Plan::prepare(p.declaration(),stale);},"each preparation namespace checked");
        rejects<std::invalid_argument>([&]{p.field(Order::BA,{0},{},stale);},"empty BA action checks original namespaces");
    }
    auto future=available; for (auto& a : future) a.max_sequence+=100;
    const auto later=p.witness(input,future);
    check(later.witness().image().values() == prior.witness().image().values(),"later availability cannot replace captured source bytes");
    std::array<std::unique_ptr<storage::NativeAccessionDirectory>,2> dirs;
    for (std::size_t j=0;j<2;++j) dirs[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[j],available[j].max_sequence),available[j].source_namespace);
    const auto pins=[&]{return std::array{native_product_example::anchor(*dirs[0]),native_product_example::anchor(*dirs[1])};};
    const auto r=request(pins());
    for (std::size_t j=0;j<2;++j) {
        auto x=r; x.factor_identities[j]=0; rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"each original factor role must be named");
        x=r; x.factors[j]=dirs[j]->pin(storage::NativeAccessionRequest::sparse_chain({0},{10},{{10,Q{0}}}));
        rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"supported zero is not an empty source anchor");
        auto cochain=storage::NativeAccessionRequest::sparse_chain({0},{},{}); cochain.kind=storage::NativeAccessionKind::Cochain; cochain.pairing=storage::NativePairing::AlgebraicDual;
        x=r; x.factors[j]=dirs[j]->pin(cochain); rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"dual does not identify a source Chain");
    }
    auto alias=r; alias.factor_identities[1]=alias.factor_identities[0];
    rejects<std::invalid_argument>([&]{Plan::prepare(alias,available);},"same factor name cannot alias different publications");
    alias=r; alias.factors[1]=alias.factors[0]; alias.factor_identities[1]=alias.factor_identities[0]; alias.declaration=declaration({f[0],f[0]});
    auto alias_available=available; alias_available[1]=alias_available[0];
    const auto repeated=Plan::prepare(alias,alias_available);
    check(repeated.factor(1).endpoint() == alias.factors[0].endpoint() && !repeated.numeric_plan().product(Order::AB).same_owner(repeated.numeric_plan().product(Order::BA)),"same actual factor is explicit without collapsing product owners");
    storage::NativeAccessionDirectory foreign(storage::TemporalEndpoint(f[0],7),910);
    alias.factors[1]=native_product_example::anchor(foreign);
    rejects<std::invalid_argument>([&]{Plan::prepare(alias,alias_available);},"equal bytes and namespace cannot alias another authority");
    const auto old=Plan::prepare(r,available), old_input=old;
    const auto oi=old_input.field(Order::BA,{2},{{c,Q{1}}},available);
    for (std::size_t j=0;j<2;++j) { const auto endpoint=dirs[j]->pin_snapshot(); auto next=*endpoint; ++next.sequence;
        dirs[j]->advance(storage::make_temporal_segment(*endpoint,next)); check(old.factor(j).endpoint() == endpoint,"both old publications remain pinned"); }
    rejects<std::invalid_argument>([&]{Plan::prepare(request(pins()),available);},"future sources reject old cutoffs");
    auto next_available=available; for (auto& a : next_available) ++a.max_sequence;
    const auto next=Plan::prepare(request(pins()),next_available); const auto ni=next.field(Order::BA,{2},{{c,Q{1}}},next_available);
    rejects<std::invalid_argument>([&]{old.interchange(ni,next_available);},"future equal output cannot enter an old owner");
    check(old.witness(oi,available).input()->plan().same_owner(old),"old execution stays independent of later publication");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"owned allocation must be admitted");
    policy={}; policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"unsupported required GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(input,available,runtime::ExecutionLease(policy));
    check(fallback.provenance().fallback,"explicit CPU fallback retained");
    policy={}; policy.limits=prior.provenance().requirements; policy.limits.scalar_work=64000000; policy.limits.integer_bits=1u<<24;
    const auto at_action=p.witness(input,available,runtime::ExecutionLease(policy));
    check(at_action.witness().image().values() == prior.witness().image().values(),"at-limit action workspace admitted");
    --policy.limits.rational_slots; rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below complete action envelope rejects");
    policy={}; policy.limits.records=n.retained_words()-1;
    rejects<runtime::ExecutionLimitExceeded>([&]{p.field(Order::BA,{0},{},available,runtime::ExecutionLease(policy));},"empty action includes all retained words");
    policy={}; policy.limits.scalar_work=prior.provenance().execution_steps-1;
    rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below measured work rejects");
    policy={}; policy.limits.records=n.retained_words(); policy.limits.rational_slots=policy.limits.sparse_entries=2*n.entry_envelope();
    const auto at=Plan::prepare(r,available,runtime::ExecutionLease(policy)); const auto atn=at.numeric_plan();
    check(atn.cells() == n.cells(),"at-limit preparation includes certificate scratch before factor copying");
    --policy.limits.sparse_entries; rejects<runtime::ExecutionLimitExceeded>([&]{Plan::prepare(r,available,runtime::ExecutionLease(policy));},"below full preparation envelope rejects");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(input,available,runtime::ExecutionLease({},stop));},"owned cancellation rejects unpublished work");
    auto small=r; small.dimension_limit=1; rejects<std::length_error>([&]{Plan::prepare(small,available);},"all product grade populations are bounded");
    auto moved=d; auto& declaration_alias=moved.types.front().ba; Guard moved_guard({},[&]{declaration_alias=999;});
    const auto frozen=Numeric::prepare(f,std::move(moved),moved_guard);
    check(frozen.declaration().types == d.types,"moved declaration aliases cannot change retained words during checkpoints");
    Numeric::Values values{{c,Q{1}}}; auto& value_alias=values.begin()->second; Guard value_guard({},[&]{value_alias=2;});
    const auto frozen_field=n.field(Order::AB,{2},std::move(values),value_guard);
    check(frozen_field.values().at(c) == 1,"movable field aliases frozen before the first checkpoint");
    auto disposable=std::make_unique<Numeric>(n); Guard destroy({},[&]{disposable.reset();});
    const auto kept=disposable->cell_witness(Order::BA,c,destroy); check(kept.plan().same_owner(n),"checkpoint self destruction keeps actual state owned");
    std::vector<std::future<inference::NativeProductInterchangeChainWitness>> jobs;
    for (unsigned j=0;j<6;++j) jobs.push_back(std::async(std::launch::async,[p,input]{return p.witness(input,available);}));
    for (auto& job : jobs) { const auto w=job.get(); check(w.witness().image().values() == prior.witness().image().values(),"concurrent exact actions have independent scratch"); }
    const auto bf=sources(8); const Cell growth{{{0},0},{{1},0}};
    for (std::size_t limited=0;limited<2;++limited) {
        std::array<std::unique_ptr<storage::NativeAccessionDirectory>,2> bounded;
        for (std::size_t j=0;j<2;++j) { storage::NativeAccessionLimits cap; if (j == limited) { cap.max_integer_bytes=1; cap.max_action_work=3999999; }
            bounded[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(bf[j],available[j].max_sequence),available[j].source_namespace,
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto data){return storage::rfcx_detail::fnv1a64(data);},cap); }
        const auto bp=Plan::prepare(request({native_product_example::anchor(*bounded[0]),native_product_example::anchor(*bounded[1])}),available);
        const auto bi=bp.field(Order::AB,{1},{{growth,Q{253,251}}},available);
        check(bi.provenance().action_limits.integer_bits == 8 && bi.provenance().action_limits.steps == 3999999,"each source authority contributes arithmetic limits");
        rejects<std::length_error>([&]{bp.boundary(bi,available);},"each source bounds arithmetic on another factor");
    }
    core::RelationSignature sig; core::RelationConstraints constraint;
    constraint.rules.push_back(core::RelationRule::role_magnitude(sig.existence_aspect(),Q{509,503}));
    (void)sig.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraint); core::RelationalComplexBuilder builder(sig); builder.declare_grade({2});
    auto empty=bf; empty[0]=builder.freeze(); numeric::ExactActionLimits limit; limit.integer_bits=8; Guard unused(limit);
    rejects<std::length_error>([&]{Numeric::prepare(empty,declaration(empty),unused);},"unused exact rules checked on an empty factor");
    auto empty_d=declaration(empty); empty_d.types.pop_back();
    rejects<std::invalid_argument>([&]{Numeric::prepare(empty,empty_d,guard);},"empty carriers still require complete interchange directions");
    for (std::size_t j=0;j<2;++j) rejects<std::invalid_argument>([&]{
        core::RelationSignature s; const auto t=s.add_type("deformed"); core::RelationalComplexBuilder broken(s);
        broken.add_grade_one(core::RelationKey{100},t,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},{20,true,false,Q{1},{},core::OccurrenceKey{102}}});
        broken.add_higher({2},core::RelationKey{200},t,{{0,true,false,Q{1},{},core::OccurrenceKey{201}}});
        auto changed=f; changed[j]=broken.freeze(); Numeric::prepare(changed,declaration(changed),guard);
    },"every factor requires the full strict chain family");
    rejects<std::overflow_error>([&]{(void)algebra::product_interchange_detail::mul(std::numeric_limits<std::size_t>::max(),2);},"checked population product overflow");
    for (std::size_t j=0;j<2;++j) check(storage::encode_rfcx(p.factor(j).endpoint()->source).bytes == bytes[j],"failures preserve source bytes");
    const auto retained=p.witness(prior_input,available);
    check(retained.witness().image().values() == prior.witness().image().values(),"failures preserve retained exact results");
}
} // namespace
int main(int argc,char** argv) {
    const bool records=argc == 2 && std::string_view(argv[1]) == "--records";
    try { samples(records); if (!records) { contracts(); std::cout << "checks=" << checks << " cells=" << cells << " witnesses=" << witnesses << '\n'; } }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } return 0;
}
