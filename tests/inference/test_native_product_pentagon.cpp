#include "../../examples/native_product_pentagon/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_pentagon_example;
std::size_t checks{}, witnesses{}, cells{};
void check(bool yes, const char* m) { ++checks; if (!yes) throw std::runtime_error(m); }
template<class E, class F> void rejects(F f, const char* m) {
    bool caught = false; try { f(); } catch (const E&) { caught = true; } check(caught,m);
}
void json(core::CellId c) { std::cout << '[' << c.grade.value << ',' << c.index << ']'; }
void json(algebra::ProductCell c) { std::cout << '['; json(c.left); std::cout << ','; json(c.right); std::cout << ']'; }
void json(algebra::LeftAssociatedCell c) { std::cout << '['; json(c.inner); std::cout << ','; json(c.outer); std::cout << ']'; }
void json(algebra::RightAssociatedCell c) { std::cout << '['; json(c.outer); std::cout << ','; json(c.inner); std::cout << ']'; }
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
void json(const Cell& c) { std::visit([](const auto& x){ std::cout << '['; json(x.left); std::cout << ','; json(x.right); std::cout << ']'; },c); }
void json(const Numeric::Values& v) {
    std::cout << '['; bool comma = false;
    for (const auto& [c,q] : v) { if (comma) std::cout << ','; comma = true; std::cout << '['; json(c); std::cout << ",\"" << q << "\"]"; }
    std::cout << ']';
}
void json(const algebra::PentagonOccurrence& o) {
    std::cout << "{\"upper\":"; json(o.upper); std::cout << ",\"lower\":"; json(o.lower);
    std::cout << ",\"path\":[";
    for (std::size_t j = 0; j < o.path_length; ++j) { if (j) std::cout << ','; std::cout << (o.path[j] == algebra::ProductBoundarySide::Right); }
    std::cout << "],\"bits\":[";
    for (std::size_t j = 0; j < o.path_length; ++j) { if (j) std::cout << ','; std::cout << o.koszul_bits[j]; }
    std::cout << "],\"flip\":" << o.koszul_flip << ",\"factor\":" << o.factor << ",\"position\":" << o.attaching_position
        << ",\"word_position\":" << o.word_position << ",\"occurrence\":" << o.factor_occurrence.value << ",\"exists\":" << o.exists
        << ",\"factor_coefficient\":\"" << o.factor_coefficient << "\",\"coefficient\":\"" << o.coefficient << "\",\"aspects\":[";
    for (std::size_t j = 0; j < o.factor_aspects.size(); ++j) { if (j) std::cout << ','; std::cout << o.factor_aspects[j]; }
    std::cout << "]}";
}
template<class R, class F> void array(const R& range, F fn) {
    std::cout << '['; bool comma = false; for (const auto& x : range) { if (comma) std::cout << ','; comma = true; fn(x); } std::cout << ']';
}
void emit_plan(unsigned sample, const Plan& p) {
    const auto n = p.numeric_plan(); const auto& d = n.declaration();
    std::cout << "{\"phase\":\"plan\",\"sample\":" << sample << ",\"sources\":[";
    for (std::size_t f = 0; f < 4; ++f) { if (f) std::cout << ','; json(n.factor(f)); }
    const auto pair_types = [](const auto& ts){ array(ts,[](const auto& t){std::cout << '['; json(t.left); std::cout << ','; json(t.right); std::cout << ',' << t.key << ']';}); };
    const auto nested_types = [](const auto& ts){ array(ts,[](const auto& t){std::cout << '[' << t.inner_type << ','; json(t.factor_type); std::cout << ',' << t.key << ']';}); };
    const auto images = [](const auto& ts){ array(ts,[](const auto& t){std::cout << '[' << t.left << ',' << t.right << ']';}); };
    std::cout << "],\"pairs\":["; pair_types(d.abc.ab.types); std::cout << ','; pair_types(d.abc.bc.types); std::cout << ','; pair_types(d.bcd.bc.types);
    std::cout << "],\"triples\":["; nested_types(d.abc.left.types); std::cout << ','; nested_types(d.abc.right.types); std::cout << ',';
    nested_types(d.bcd.left.types); std::cout << ','; nested_types(d.bcd.right.types);
    std::cout << "],\"triple_images\":["; images(d.abc.types); std::cout << ','; images(d.bcd.types);
    std::cout << "],\"nested\":"; array(d.nested,[&](const auto& x){nested_types(x.types);});
    std::cout << ",\"middle\":"; array(d.middle.types,[](const auto& t){std::cout << '[' << t.left_type << ',' << t.right_type << ',' << t.key << ']';});
    std::cout << ",\"edges\":"; array(d.edges,[&](const auto& x){images(x);});
    std::cout << ",\"grade_counts\":[";
    for (std::uint16_t k = 0; k <= n.max_grade().value; ++k) { if (k) std::cout << ','; std::cout << n.cell_count(core::Grade{k}); }
    std::cout << "]}\n";
}
void emit_cell(unsigned sample, const inference::NativeProductPentagonCellWitness& w) {
    const auto& c = w.witness().cell();
    std::cout << "{\"phase\":\"cell\",\"sample\":" << sample << ",\"cells\":"; array(c.cells,[](const auto& x){json(x);});
    std::cout << ",\"identities\":"; array(c.factors,[](const auto& x){std::cout << '[' << x.grade.value << ',' << x.key << ']';});
    std::cout << ",\"types\":"; array(c.types,[](const auto x){std::cout << x;});
    std::cout << ",\"flags\":"; array(w.witness().flags(),[](const auto& word){array(word,[](const auto& x){json(x);});});
    std::cout << "}\n";
}
void exercise(unsigned sample, const Plan& p, core::Grade g, Numeric::Values data, bool output) {
    const auto input = p.field(Vertex::Left,g,std::move(data),available), boundary_input = input;
    const auto w = p.witness(input,available); ++witnesses;
    const auto& l = w.witness().long_path(); const auto& s = w.witness().short_path();
    check(w.plan().same_owner(p) && w.input()->plan().same_owner(p),"pentagon witness retains actual native owner and input");
    check(l[0].values() == input.field().values() && s[0].values() == l[0].values() && l[3].values() == s[2].values(),"both complete pentagon routes agree with support");
    for (const auto& e : w.witness().edges()) {
        check(e.input.values() == e.round_trip.values(),"every edge round trip retains support");
        if (g.value) check(e.mapped_boundary->values() == e.image_boundary->values(),"every edge has exact chain naturality");
    }
    auto current = input;
    for (std::size_t e = 0; e < 3; ++e) {
        const auto next = p.traverse(current,e,false,available);
        check(next.field().values() == l[e+1].values() && next.provenance().edge == e && !next.provenance().reverse &&
            next.input()->plan().same_owner(p),"native route retains edge and ordered owning input history"); current = next;
    }
    const auto middle = p.traverse(input,3,false,available), right = p.traverse(middle,4,false,available);
    check(right.field().values() == current.field().values() && right.input()->provenance().edge == 3 && current.input()->provenance().edge == 1,
        "equal outputs retain distinct path histories");
    const auto back = p.traverse(middle,3,true,available);
    check(back.provenance().reverse && back.provenance().edge == 3 && back.field().values() == input.field().values(),"reverse declared canonical edge is explicit");
    std::optional<inference::DerivedNativePentagonField> second;
    if (g.value > 1) {
        second = p.boundary(p.boundary(boundary_input,available),available);
        for (const auto& [c,q] : second->field().values()) { (void)c; check(q == 0,"complete four-factor strict chain family"); }
    }
    if (output) {
        std::cout << "{\"phase\":\"action\",\"sample\":" << sample << ",\"grade\":" << g.value << ",\"long\":";
        array(l,[](const auto& x){json(x.values());}); std::cout << ",\"short\":"; array(s,[](const auto& x){json(x.values());});
        std::cout << ",\"edges\":"; array(w.witness().edges(),[](const auto& e){
            std::cout << "{\"edge\":" << e.edge << ",\"round_trip\":"; json(e.round_trip.values()); std::cout << ",\"mapped_boundary\":";
            if (e.mapped_boundary) json(e.mapped_boundary->values()); else std::cout << "null";
            std::cout << ",\"image_boundary\":"; if (e.image_boundary) json(e.image_boundary->values()); else std::cout << "null"; std::cout << '}';
        }); std::cout << ",\"second_boundary\":"; if (second) json(second->field().values()); else std::cout << "null"; std::cout << "}\n";
    }
}
void samples(bool output) {
    for (unsigned sample = 0; sample < 12; ++sample) {
        const auto p = plan(sample); const auto n = p.numeric_plan(); if (output) emit_plan(sample,p);
        for (const auto& c : n.cells()) {
            const auto w = p.cell_witness(c.cells[0],available); ++cells;
            check(w.witness().cell() == c && w.plan().same_owner(p),"all five actual carriers retain the full original identity");
            for (std::size_t v = 1; v < 5; ++v) for (std::size_t i = 0; i < w.witness().flags()[0].size(); ++i) {
                const auto& a = w.witness().flags()[0][i]; const auto& b = w.witness().flags()[v][i];
                check(a.factor_occurrence == b.factor_occurrence && a.factor_aspects == b.factor_aspects && a.attaching_position == b.attaching_position &&
                    a.coefficient == b.coefficient && a.koszul_flip == b.koszul_flip,"full ordered fibers survive every parenthesization");
            }
            if (output) emit_cell(sample,w);
            const auto ids = algebra::product_pentagon_detail::factors(c.cells[0]); Guard inner_guard;
            const auto abc = n.inner_associator(false), bcd = n.inner_associator(true);
            const auto aw = abc.cell_witness({{ids[0],ids[1]},ids[2]},inner_guard), bw = bcd.cell_witness({{ids[1],ids[2]},ids[3]},inner_guard);
            for (const auto& flag : aw.flags()) for (const auto v : {std::size_t{0},std::size_t{1}}) {
                const auto& inner = v ? flag.right : flag.left; const auto& outer = w.witness().flags()[v][inner.word_position];
                check(outer.path_length == inner.path_length+1 && outer.factor_occurrence == inner.factor_occurrence &&
                    !outer.koszul_bits[0] && outer.coefficient == inner.coefficient,"ABC whiskering retains the actual canonical inner associator flags");
            }
            for (const auto& flag : bw.flags()) for (const auto v : {std::size_t{2},std::size_t{3}}) {
                const auto& inner = v == 2 ? flag.left : flag.right;
                const auto it = std::find_if(w.witness().flags()[v].begin(),w.witness().flags()[v].end(),[&](const auto& o){return o.factor == inner.factor+1 && o.attaching_position == inner.attaching_position;});
                check(it != w.witness().flags()[v].end() && it->path_length == inner.path_length+1 && it->factor_occurrence == inner.factor_occurrence &&
                    it->koszul_bits[0] == ((ids[0].grade.value%2) != 0) && it->coefficient == ((ids[0].grade.value%2) ? -inner.coefficient : inner.coefficient),
                    "BCD whiskering retains the actual canonical inner associator flags and outer parity");
            }
            for (const Q& q : {Q{0},Q{-3,7}}) exercise(sample,p,algebra::pentagon_grade(c.cells[0]),{{c.cells[0],q}},output);
            for (std::size_t v = 0; v < 5; ++v) {
                const auto input = p.field(static_cast<Vertex>(v),algebra::pentagon_grade(c.cells[v]),{{c.cells[v],Q{1}}},available);
                if (input.field().grade().value > 1) {
                    const auto dd = p.boundary(p.boundary(input,available),available);
                    for (const auto& [id,q] : dd.field().values()) { (void)id; check(q == 0,"each separate nested carrier has its complete chain family"); }
                }
            }
        }
        for (std::uint16_t k = 0; k <= n.max_grade().value; ++k) {
            exercise(sample,p,core::Grade{k},{},output); Numeric::Values data;
            for (const auto& c : n.cells()) if (algebra::pentagon_grade(c.cells[0]).value == k)
                data.emplace(c.cells[0],Q{static_cast<long>(algebra::product_pentagon_detail::factors(c.cells[0])[0].index)-1,3});
            exercise(sample,p,core::Grade{k},std::move(data),output);
        }
    }
}
void contracts() {
    const auto p = plan(); const auto n = p.numeric_plan(); const auto c = n.cells().back(); const auto g = algebra::pentagon_grade(c.cells[0]);
    const auto input = p.field(Vertex::Left,g,{{c.cells[0],Q{1}}},available); const auto prior = p.witness(input,available);
    const auto f = sources(0); const auto d = declaration(f); Guard guard;
    const auto bad = [&](auto change){auto x = d; change(x); rejects<std::invalid_argument>([&]{Numeric::prepare(f,x,guard);},"malformed complete pentagon declaration rejects");};
    bad([](auto& x){x.key=0;}); bad([](auto& x){x.revision=0;}); bad([](auto& x){++x.bcd.ab.revision;});
    bad([](auto& x){std::reverse(x.bcd.ab.types.begin(),x.bcd.ab.types.end());});
    bad([](auto& x){x.abc.types.pop_back();}); bad([](auto& x){x.bcd.types.pop_back();});
    for (std::size_t v = 0; v < 4; ++v) {
        bad([v](auto& x){x.nested[v].types.pop_back();}); bad([v](auto& x){x.nested[v].key=0;});
        bad([v](auto& x){x.nested[v].types.back().inner_type=0;}); bad([v](auto& x){x.nested[v].types.back().factor_type=999;});
        bad([v](auto& x){x.nested[v].types.back()=x.nested[v].types.front();});
    }
    bad([](auto& x){x.middle.types.pop_back();}); bad([](auto& x){x.middle.types.back().left_type=0;});
    bad([](auto& x){x.middle.types.back().right_type=0;}); bad([](auto& x){x.middle.types.back()=x.middle.types.front();}); bad([](auto& x){x.middle.revision=0;});
    for (std::size_t e = 0; e < 5; ++e) {
        bad([e](auto& x){x.edges[e].pop_back();}); bad([e](auto& x){std::swap(x.edges[e].front().right,x.edges[e].back().right);});
        bad([e](auto& x){x.edges[e].back()=x.edges[e].front();}); bad([e](auto& x){x.edges[e].back().left=999;});
    }
    auto reordered = d;
    for (auto* x : {&reordered.abc,&reordered.bcd}) {
        std::reverse(x->ab.types.begin(),x->ab.types.end()); std::reverse(x->bc.types.begin(),x->bc.types.end());
        std::reverse(x->left.types.begin(),x->left.types.end()); std::reverse(x->right.types.begin(),x->right.types.end()); std::reverse(x->types.begin(),x->types.end());
    }
    for (auto& x : reordered.nested) std::reverse(x.types.begin(),x.types.end());
    for (auto& x : reordered.edges) std::reverse(x.begin(),x.end());
    std::reverse(reordered.middle.types.begin(),reordered.middle.types.end());
    const auto re = Numeric::prepare(f,reordered,guard);
    check(re.declaration().edges == reordered.edges && re.cells() == n.cells(),"retained declaration order is independent from canonical correspondence");
    rejects<std::invalid_argument>([&]{re.witness(input.field(),guard);},"equal numerical bytes never bind an actual owner");
    const auto twin = plan(); rejects<std::invalid_argument>([&]{twin.witness(input,available);},"equal native bytes never bind actual owners");
    for (std::size_t v = 1; v < 5; ++v) rejects<std::invalid_argument>([&]{p.field(static_cast<Vertex>(v),g,{{c.cells[0],Q{0}}},available);},"zero input checks its full nested carrier");
    rejects<std::invalid_argument>([&]{p.field(Vertex::Left,{0},{{c.cells[0],0}},available);},"zero input checks grade");
    rejects<std::invalid_argument>([&]{p.field(static_cast<Vertex>(99),{0},{},available);},"empty input rejects an unknown tree");
    rejects<std::invalid_argument>([&]{p.field(Vertex::Left,{9},{},available);},"empty input rejects an undeclared grade");
    auto ids = algebra::product_pentagon_detail::factors(c.cells[0]); ids[3].index = 999;
    rejects<std::out_of_range>([&]{p.cell_witness(algebra::product_pentagon_detail::cell(0,ids),available);},"full factor identities checked");
    rejects<std::out_of_range>([&]{p.traverse(input,5,false,available);},"unknown edge rejects");
    rejects<std::invalid_argument>([&]{p.traverse(input,1,false,available);},"edge must start on its actual declared carrier");
    const auto middle = p.traverse(input,3,false,available);
    rejects<std::invalid_argument>([&]{p.witness(middle,available);},"whole pentagon starts on its declared left tree");
    auto q = Q{1}; mpz_set_ui(mpq_denref(q.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{p.field(Vertex::Left,g,{{c.cells[0],q}},available);},"malformed rational rejects");
    for (std::size_t j = 0; j < 4; ++j) {
        auto stale = available; --stale[j].max_sequence;
        rejects<std::invalid_argument>([&]{p.witness(input,stale);},"each factor independently checks its cutoff");
        stale = available; stale[j].source_namespace = 999;
        rejects<std::invalid_argument>([&]{p.field(Vertex::Left,{0},{},stale);},"each namespace checked on empty actions");
        auto captured = p.declaration();
        rejects<std::invalid_argument>([&]{Plan::prepare(captured,stale);},"each preparation availability role checked independently");
    }
    auto future = available; for (auto& a : future) a.max_sequence += 100;
    const auto later = p.witness(input,future);
    check(later.witness().long_path().back().values() == prior.witness().long_path().back().values(),"later availability cannot replace a captured publication");
    std::array<std::unique_ptr<storage::NativeAccessionDirectory>,4> dirs;
    for (std::size_t j = 0; j < 4; ++j) dirs[j] = std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[j],available[j].max_sequence),available[j].source_namespace);
    const auto pins = [&]{return std::array{native_product_example::anchor(*dirs[0]),native_product_example::anchor(*dirs[1]),native_product_example::anchor(*dirs[2]),native_product_example::anchor(*dirs[3])};};
    const auto r = request(pins());
    for (std::size_t j = 0; j < 4; ++j) { auto x = r; x.factor_identities[j] = 0; rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"all four factor identities required"); }
    for (std::size_t j = 0; j < 4; ++j) {
        auto x = r; x.factors[j] = dirs[j]->pin(storage::NativeAccessionRequest::sparse_chain({0},{10},{{10,Q{0}}}));
        rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"supported zero is not an empty source anchor");
        auto cochain = storage::NativeAccessionRequest::sparse_chain({0},{},{}); cochain.kind = storage::NativeAccessionKind::Cochain; cochain.pairing = storage::NativePairing::AlgebraicDual;
        x = r; x.factors[j] = dirs[j]->pin(cochain);
        rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"an algebraic dual does not identify a source Chain anchor");
    }
    auto alias = r; alias.factor_identities[1] = alias.factor_identities[0];
    rejects<std::invalid_argument>([&]{Plan::prepare(alias,available);},"same factor name cannot alias different publications");
    alias = r; alias.factors[1] = alias.factors[0]; alias.factor_identities[1] = alias.factor_identities[0];
    alias.declaration = declaration({f[0],f[0],f[2],f[3]}); auto alias_available = available; alias_available[1] = alias_available[0];
    check(Plan::prepare(alias,alias_available).factor(1).endpoint() == alias.factors[0].endpoint(),"explicit same actual factor admitted");
    const auto old = Plan::prepare(r,available); const auto old_input = old.field(Vertex::Left,g,{{c.cells[0],1}},available);
    for (std::size_t j = 0; j < 4; ++j) { const auto endpoint = dirs[j]->pin_snapshot(); auto next = *endpoint; ++next.sequence;
        dirs[j]->advance(storage::make_temporal_segment(*endpoint,next)); check(old.factor(j).endpoint() == endpoint,"every old factor remains pinned after advancement"); }
    rejects<std::invalid_argument>([&]{Plan::prepare(request(pins()),available);},"future publications reject older cutoffs");
    auto next_available = available; for (auto& a : next_available) ++a.max_sequence;
    const auto next = Plan::prepare(request(pins()),next_available), next_input_owner = next;
    const auto next_input = next_input_owner.field(Vertex::Left,g,{{c.cells[0],1}},next_available);
    rejects<std::invalid_argument>([&]{old.traverse(next_input,0,false,next_available);},"future equal output cannot enter an old owner");
    check(old.witness(old_input,available).input()->plan().same_owner(old),"old execution remains available without future data");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation = false;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"owning allocation admission required");
    policy = {}; policy.backend = runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"unsupported required GPU rejects");
    policy.fallback = runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback = p.witness(input,available,runtime::ExecutionLease(policy)); check(fallback.provenance().fallback,"explicit CPU fallback retained");
    policy = {}; policy.limits = prior.provenance().requirements; policy.limits.scalar_work = 64000000; policy.limits.integer_bits = 1u<<24;
    const auto at = p.witness(input,available,runtime::ExecutionLease(policy)); check(at.witness().long_path().back().values() == prior.witness().long_path().back().values(),"exact at-limit admitted reference workspace");
    --policy.limits.rational_slots; rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below full field witness workspace rejects");
    policy = {}; policy.limits.records = n.retained_words()-1;
    rejects<runtime::ExecutionLimitExceeded>([&]{p.field(Vertex::Left,{0},{},available,runtime::ExecutionLease(policy));},"empty action admits all retained words");
    policy = {}; policy.limits.scalar_work = prior.provenance().execution_steps-1;
    rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below observed work rejects");
    policy = {}; policy.limits.records = n.retained_words();
    policy.limits.rational_slots = policy.limits.sparse_entries = algebra::product_pentagon_detail::mul(n.entry_envelope(),2);
    const auto at_prepare = Plan::prepare(r,available,runtime::ExecutionLease(policy)); const auto at_numeric = at_prepare.numeric_plan();
    check(at_numeric.cells() == n.cells(),"exact at-limit preparation admits full certificate scratch before source copies");
    --policy.limits.sparse_entries;
    rejects<runtime::ExecutionLimitExceeded>([&]{Plan::prepare(r,available,runtime::ExecutionLease(policy));},"preparation below complete certificate envelope rejects");
    auto stop = std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(input,available,runtime::ExecutionLease({},stop));},"owned cancellation rejects unpublished work");
    auto small = r; small.dimension_limit = 1;
    rejects<std::length_error>([&]{Plan::prepare(small,available);},"whole factor dimension bound checked");
    auto moved = d; auto& declaration_alias = moved.edges[0][0].right; Guard moved_guard({},[&]{declaration_alias=999;});
    const auto frozen = Numeric::prepare(f,std::move(moved),moved_guard);
    check(frozen.declaration().edges == d.edges,"moved declaration element aliases cannot mutate retained type words");
    Numeric::Values moved_data{{c.cells[0],Q{1}}}; auto& value_alias = moved_data.begin()->second; Guard value_guard({},[&]{value_alias=2;});
    const auto frozen_field = n.field(Vertex::Left,g,std::move(moved_data),value_guard);
    check(frozen_field.values().at(c.cells[0]) == 1,"moved values captured before the first checkpoint");
    auto disposable = std::make_unique<Numeric>(n); Guard destroy({},[&]{disposable.reset();});
    const auto kept = disposable->cell_witness(c.cells[0],destroy); check(kept.plan().same_owner(n),"checkpoint self destruction keeps owner pinned");
    std::vector<std::future<inference::NativeProductPentagonChainWitness>> jobs;
    for (unsigned j = 0; j < 6; ++j) jobs.push_back(std::async(std::launch::async,[p,input]{return p.witness(input,available);}));
    for (auto& job : jobs) { const auto w = job.get(); check(w.witness().long_path().back().values() == prior.witness().long_path().back().values(),"concurrent actions own independent scratch"); }
    const core::CellId e{{1},0}, vertex{{0},0}; const Cell growth_cell = algebra::LeftPentagonCell{{{vertex,vertex},vertex},e};
    for (std::size_t limited = 0; limited < 4; ++limited) {
        std::array<std::unique_ptr<storage::NativeAccessionDirectory>,4> bounded;
        for (std::size_t j = 0; j < 4; ++j) { storage::NativeAccessionLimits cap; if (j == limited) { cap.max_integer_bytes=1; cap.max_action_work=3999999; }
            bounded[j] = std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[j],available[j].max_sequence),available[j].source_namespace,
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto data){return storage::rfcx_detail::fnv1a64(data);},cap); }
        const auto bp = Plan::prepare(request({native_product_example::anchor(*bounded[0]),native_product_example::anchor(*bounded[1]),native_product_example::anchor(*bounded[2]),native_product_example::anchor(*bounded[3])}),available);
        const auto bi = bp.field(Vertex::Left,{1},{{growth_cell,Q{253,251}}},available);
        check(bi.provenance().action_limits.integer_bits == 8 && bi.provenance().action_limits.steps == 3999999,"every actual factor contributes its work and integer bounds");
        rejects<std::length_error>([&]{bp.boundary(bi,available);},"every source authority bounds arithmetic on another factor");
    }
    core::RelationSignature sig; core::RelationConstraints constraint;
    constraint.rules.push_back(core::RelationRule::role_magnitude(sig.existence_aspect(),Q{509,503}));
    (void)sig.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraint); core::RelationalComplexBuilder builder(sig); builder.declare_grade({2});
    auto empty = sources(4); empty[0] = builder.freeze(); numeric::ExactActionLimits limit; limit.integer_bits = 8; Guard unused(limit);
    rejects<std::length_error>([&]{Numeric::prepare(empty,declaration(empty),unused);},"unused rules checked on a completely empty carrier");
    auto empty_d = declaration(empty); empty_d.edges[4].pop_back();
    rejects<std::invalid_argument>([&]{Numeric::prepare(empty,empty_d,guard);},"empty carriers require all unused pentagon directions");
    for (std::size_t j = 0; j < 4; ++j) rejects<std::invalid_argument>([&]{
        core::RelationSignature broken_sig; const auto type = broken_sig.add_type("deformed"); core::RelationalComplexBuilder broken(broken_sig);
        broken.add_grade_one(core::RelationKey{100},type,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},{20,true,false,Q{1},{},core::OccurrenceKey{102}}});
        broken.add_higher({2},core::RelationKey{200},type,{{0,true,false,Q{1},{},core::OccurrenceKey{201}}});
        auto changed = f; changed[j] = broken.freeze(); (void)Numeric::prepare(changed,declaration(changed),guard);
    },"each factor must satisfy its full strict chain family");
    rejects<std::overflow_error>([&]{(void)algebra::product_pentagon_detail::mul(std::numeric_limits<std::size_t>::max(),2);},"checked population product overflow");
}
} // namespace
int main(int argc,char** argv) {
    const bool records = argc == 2 && std::string_view(argv[1]) == "--records";
    try { samples(records); if (!records) { contracts(); std::cout << "checks=" << checks << " cells=" << cells << " witnesses=" << witnesses << '\n'; } }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } return 0;
}
