#include "../../examples/native_product_hexagon/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_hexagon_example;
std::size_t checks{}, witnesses{}, cells{};
void check(bool yes,const char* m) { ++checks; if (!yes) throw std::runtime_error(m); }
template<class E,class F> void rejects(F f,const char* m) {
    bool caught=false; try { f(); } catch (const E&) { caught=true; } check(caught,m);
}
void json(core::CellId c) { std::cout << '[' << c.grade.value << ',' << c.index << ']'; }
void json(algebra::ProductCell c) { std::cout << '['; json(c.left); std::cout << ','; json(c.right); std::cout << ']'; }
void json(const Cell& c) {
    std::visit([](const auto& x) { using T=std::decay_t<decltype(x)>;
        std::cout << '[';
        if constexpr (std::is_same_v<T,algebra::LeftAssociatedCell>) { json(x.inner); std::cout << ','; json(x.outer); }
        else { json(x.outer); std::cout << ','; json(x.inner); }
        std::cout << ']';
    },c);
}
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
std::size_t index(Carrier c) { return 2*static_cast<std::size_t>(c.order)+(c.association == Association::Right ? 1u : 0u); }
void json(const Numeric::Values& v) {
    array(v,[](const auto& x){std::cout << '['; json(x.first); std::cout << ",\"" << x.second << "\"]";});
}
void json(const algebra::NestedProductOccurrence& o) {
    std::cout << "{\"upper\":"; json(o.upper); std::cout << ",\"lower\":"; json(o.lower);
    std::cout << ",\"path\":[";
    for (std::size_t i=0;i<o.path_length;++i) { if (i) std::cout << ','; std::cout << (o.path[i] == algebra::ProductBoundarySide::Right); }
    std::cout << "],\"factor\":" << o.factor << ",\"position\":" << o.attaching_position << ",\"word_position\":" << o.word_position
        << ",\"occurrence\":" << o.factor_occurrence.value << ",\"flips\":[" << o.inner_koszul_flip << ',' << o.outer_koszul_flip << ',' << o.koszul_flip
        << "],\"exists\":" << o.exists << ",\"factor_coefficient\":\"" << o.factor_coefficient << "\",\"coefficient\":\"" << o.coefficient << "\",\"aspects\":";
    array(o.factor_aspects,[](bool x){std::cout << x;}); std::cout << '}';
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
    array(std::array{p.factor(0),p.factor(1),p.factor(2)},[](const auto& x){json(x.endpoint()->source);});
    const auto product_types=[](const auto& xs){array(xs,[](const auto& t){std::cout << '['; json(t.left); std::cout << ','; json(t.right); std::cout << ',' << t.key << ']';});};
    const auto nested_types=[](const auto& xs){array(xs,[](const auto& t){std::cout << '[' << t.inner_type << ','; json(t.factor_type); std::cout << ',' << t.key << ']';});};
    std::cout << ",\"pairs\":"; array(d.pairs,[&](const auto& pair){std::cout << '['; product_types(pair.ab.types); std::cout << ','; product_types(pair.ba.types); std::cout << ',';
        array(pair.types,[](const auto& t){std::cout << '[' << t.ab << ',' << t.ba << ']';}); std::cout << ']';});
    std::cout << ",\"associators\":"; array(d.associators,[&](const auto& a){std::cout << '['; product_types(a.ab.types); std::cout << ','; product_types(a.bc.types);
        std::cout << ','; nested_types(a.left.types); std::cout << ','; nested_types(a.right.types); std::cout << ',';
        array(a.types,[](const auto& t){std::cout << '[' << t.left << ',' << t.right << ']';}); std::cout << ']';});
    std::cout << ",\"swaps\":"; array(d.swaps,[](const auto& x){std::cout << '[';
        const auto images=[](const auto& xs){array(xs,[](const auto& t){std::cout << '[' << t.source << ',' << t.target << ']';});};
        images(x.inner); std::cout << ','; images(x.block); std::cout << ']';});
    std::cout << ",\"grade_counts\":[";
    for (std::uint16_t g=0;g<=n.max_grade().value;++g) { if (g) std::cout << ',';
        std::cout << std::count_if(n.cells().begin(),n.cells().end(),[&](const auto& ids){return ids[0].grade.value+ids[1].grade.value+ids[2].grade.value == g;}); }
    std::cout << "]}\n";
}
void emit_cell(unsigned sample,const std::array<core::CellId,3>& ids,const inference::NativeProductHexagonCellWitness& native) {
    const auto& w=native.witness(); std::cout << "{\"phase\":\"cell\",\"sample\":" << sample << ",\"ids\":";
    array(ids,[](auto c){json(c);}); std::cout << ",\"carriers\":";
    array(w.carriers(),[](const auto& c){std::cout << "{\"carrier\":" << index(c.carrier) << ",\"cell\":"; json(c.cell);
        std::cout << ",\"identities\":"; array(c.identities,[](const auto& id){std::cout << '[' << id.grade.value << ',' << id.key << ']';});
        std::cout << ",\"type\":" << c.type << ",\"flags\":"; array(c.flags,[](const auto& o){json(o);}); std::cout << '}';});
    std::cout << ",\"edges\":"; array(w.edges(),[](const auto& e){std::cout << "{\"source\":" << index(e.source) << ",\"target\":" << index(e.target)
        << ",\"action\":" << static_cast<unsigned>(e.action) << ",\"flags\":";
        array(e.flags,[](const auto& f){std::cout << '[' << f.original_factor << ',' << f.source_word_position << ',' << f.target_word_position << ','
            << f.upper_interchange_flip << ',' << f.lower_interchange_flip << ",\"" << f.mapped_boundary_coefficient << "\",\"" << f.image_boundary_coefficient << "\"]";});
        std::cout << ",\"inner\":";
        if (e.inner_receipt) {
            const auto& r=*e.inner_receipt; const auto& pair=r.interchange;
            std::cout << "{\"source\":"; json(r.source_inner.cell); std::cout << ",\"target\":"; json(r.target_inner.cell);
            std::cout << ",\"types\":[" << r.source_inner.type << ',' << r.target_inner.type << "],\"flip\":" << pair.interchange_flip() << ",\"source_flags\":";
            array(pair.source_flags(),[](const auto& o){json(o);}); std::cout << ",\"target_flags\":"; array(pair.target_flags(),[](const auto& o){json(o);});
            std::cout << ",\"correspondence\":"; array(pair.flags(),[](const auto& f){std::cout << '[' << f.source_word_position << ',' << f.target_word_position << ','
                << f.upper_interchange_flip << ',' << f.lower_interchange_flip << ",\"" << f.mapped_boundary_coefficient << "\",\"" << f.image_boundary_coefficient << "\"]";}); std::cout << '}';
        } else std::cout << "null";
        std::cout << '}';}); std::cout << "}\n";
}
void json(const Numeric& n,const algebra::OwnedAssociatedProductField& f) {
    std::cout << "{\"carrier\":" << index(n.carrier(f)) << ",\"grade\":" << f.grade().value << ",\"values\":"; json(f.values()); std::cout << '}';
}
void json(const Numeric& n,const algebra::ProductHexagonEdgeWitness& e) {
    std::cout << "{\"action\":" << static_cast<unsigned>(e.action) << ",\"input\":"; json(n,e.input);
    std::cout << ",\"image\":"; json(n,e.image); std::cout << ",\"back\":"; json(n,e.round_trip);
    std::cout << ",\"mapped_boundary\":"; if (e.mapped_boundary) json(n,*e.mapped_boundary); else std::cout << "null";
    std::cout << ",\"image_boundary\":"; if (e.image_boundary) json(n,*e.image_boundary); else std::cout << "null"; std::cout << '}';
}
void exercise(unsigned sample,const Plan& p,Carrier c,core::Grade g,Numeric::Values values,bool output) {
    const auto n=p.numeric_plan(); const auto input=p.field(c,g,std::move(values),available);
    const auto native=p.witness(input,available); const auto& w=native.witness(); ++witnesses;
    check(native.plan().same_owner(p) && native.input()->plan().same_owner(p) && w.plan().same_owner(n),"actual witness and input owners retained");
    check(!input.input() && input.provenance().source_carrier == c && input.provenance().target_carrier == c,"declared field retains its actual carrier");
    for (const auto& h : w.hexagons()) {
        check(h.long_path.size() == 6 && h.edges.size() == 5,"both operation paths retain all intermediate carriers");
        check(n.carrier(h.long_path.back()) == n.carrier(h.direct.image) && h.long_path.back().values() == h.direct.image.values(),"hexagon agrees in owner and full supported field");
        const auto check_edge=[&](const auto& e) {
            check(n.carrier(e.round_trip) == n.carrier(e.input) && e.round_trip.values() == e.input.values(),"every directed edge has an exact owned inverse");
            const auto image=p.step(p.field(n.carrier(e.input),g,e.input.values(),available),e.action,available);
            check(n.carrier(image.field()) == n.carrier(e.image) && image.field().values() == e.image.values(),"native actions reuse the canonical owned fields");
            check(image.input() && image.provenance().source_carrier == n.carrier(e.input) && image.provenance().target_carrier == n.carrier(e.image) &&
                image.provenance().step == e.action && image.provenance().namespaces == std::array<std::uint64_t,3>{910,911,912} &&
                image.provenance().sequences == std::array<std::uint64_t,3>{7,11,19},"permutation never changes original A/B/C temporal roles");
            if (g.value) check(e.mapped_boundary && e.image_boundary && n.carrier(*e.mapped_boundary) == n.carrier(*e.image_boundary) &&
                e.mapped_boundary->values() == e.image_boundary->values(),"each edge is an exact chain map with complete supported zeros");
            else check(!e.mapped_boundary && !e.image_boundary,"grade zero has no invented boundary");
        };
        check_edge(h.direct); for (const auto& e : h.edges) check_edge(e);
    }
    std::optional<inference::DerivedNativeHexagonField> dd;
    if (g.value > 1) {
        dd=p.boundary(p.boundary(input,available),available);
        for (const auto& [id,q] : dd->field().values()) { (void)id; check(q == 0,"full strict nested chain family"); }
    }
    if (output) {
        std::cout << "{\"phase\":\"action\",\"sample\":" << sample << ",\"input\":"; json(n,input.field()); std::cout << ",\"hexagons\":";
        array(w.hexagons(),[&](const auto& h){std::cout << "{\"long_path\":"; array(h.long_path,[&](const auto& f){json(n,f);});
            std::cout << ",\"direct\":"; json(n,h.direct); std::cout << ",\"edges\":"; array(h.edges,[&](const auto& e){json(n,e);}); std::cout << '}';});
        std::cout << ",\"dd\":"; if (dd) json(n,dd->field()); else std::cout << "null"; std::cout << "}\n";
    }
}
void samples(bool output) {
    for (unsigned sample=0;sample<16;++sample) {
        const auto p=plan(sample); const auto n=p.numeric_plan(); if (output) emit_plan(sample,p);
        for (std::size_t i=0;i<6;++i) for (std::size_t j=i+1;j<6;++j)
            check(!n.associator(static_cast<Permutation>(i)).same_owner(n.associator(static_cast<Permutation>(j))),"equal signatures and positions do not collapse actual owners");
        for (const auto& ids : n.cells()) {
            const auto native=p.cell_witness(ids,available); const auto& w=native.witness(); ++cells;
            check(w.plan().same_owner(n) && w.carriers().size() == 12 && w.edges().size() == 36,"full carrier and directed edge inventory retained");
            for (const auto& c : w.carriers()) check(c.owner.same_owner(n.associator(c.carrier.order)) && c.cell == cell(c.carrier,ids),"cell witness retains actual nesting and carrier");
            for (const auto& e : w.edges()) {
                check(e.source_owner.same_owner(n.associator(e.source.order)) && e.target_owner.same_owner(n.associator(e.target.order)),"each edge retains actual source and target owners");
                std::set<std::size_t> positions;
                for (const auto& f : e.flags) {
                    check(f.source.factor_occurrence == f.target.factor_occurrence && f.source.factor_aspects == f.target.factor_aspects &&
                        f.source.factor_coefficient == f.target.factor_coefficient && f.source.attaching_position == f.target.attaching_position &&
                        f.source.exists == f.target.exists && f.mapped_boundary_coefficient == f.image_boundary_coefficient,"original full flag and signed coefficient identity");
                    check(positions.insert(f.target_word_position).second && f.original_factor == orders[index(e.source)/2][f.source.factor],"original flag role and explicit permutation");
                }
                check(positions.size() == w.carriers()[index(e.target)].flags.size(),"bijection covers inactive and supported zero flags");
                check(e.inner_receipt.has_value() == (e.action == Action::InnerInterchange),"only inner lift carries a canonical pair receipt");
                if (e.inner_receipt) {
                    const auto& r=*e.inner_receipt; const auto o=orders[index(e.source)/2]; const auto s=index(e.source)%2;
                    check(r.source_inner.product().same_owner(e.source_owner.inner_product(s==1)) && r.target_inner.product().same_owner(e.target_owner.inner_product(s==1)),"receipt retains actual nested subproduct owners");
                    check(r.interchange.plan().same_owner(n.pair(o[s],o[s+1])) && !r.source_inner.product().same_owner(r.interchange.source().product()) &&
                        !r.target_inner.product().same_owner(r.interchange.target().product()),"canonical interchange service is an explicit lift between distinct owned products");
                }
            }
            if (output) emit_cell(sample,ids,native);
            const auto g=core::Grade{static_cast<std::uint16_t>(ids[0].grade.value+ids[1].grade.value+ids[2].grade.value)};
            for (const Q& q : {Q{0},Q{-3,7}}) exercise(sample,p,carrier(0),g,{{cell(carrier(0),ids),q}},output);
        }
        for (std::size_t i=0;i<12;++i) for (std::uint16_t g=0;g<=n.max_grade().value;++g) {
            exercise(sample,p,carrier(i),{g},{},output); Numeric::Values values;
            for (const auto& ids : n.cells()) if (ids[0].grade.value+ids[1].grade.value+ids[2].grade.value == g)
                values.emplace(cell(carrier(i),ids),Q{static_cast<long>(ids[0].index)-1,3});
            exercise(sample,p,carrier(i),{g},std::move(values),output);
        }
    }
}
void contracts() {
    const auto p=plan(); const auto n=p.numeric_plan(); const auto f=sources(0); const auto d=declaration(f); Guard guard;
    const core::CellId e{{1},0},v{{0},0}; const std::array ids{e,e,v}; const auto c=cell(carrier(0),ids);
    const auto input=p.field(carrier(0),{2},{{c,Q{1}}},available), prior_input=input;
    const auto prior=p.witness(input,available);
    const std::array bytes{storage::encode_rfcx(p.factor(0).endpoint()->source).bytes,storage::encode_rfcx(p.factor(1).endpoint()->source).bytes,
        storage::encode_rfcx(p.factor(2).endpoint()->source).bytes};
    const auto bad=[&](auto change){auto x=d; change(x); rejects<std::invalid_argument>([&]{Numeric::prepare(f,x,guard);},"malformed complete hexagon declaration rejects");};
    bad([](auto& x){x.key=0;}); bad([](auto& x){x.revision=0;});
    for (std::size_t i=0;i<3;++i) {
        bad([&](auto& x){x.pairs[i].types.pop_back();}); bad([&](auto& x){x.pairs[i].revision=0;});
        bad([&](auto& x){std::swap(x.pairs[i].types.front().ba,x.pairs[i].types.back().ba);});
    }
    for (std::size_t i=0;i<6;++i) {
        bad([&](auto& x){x.associators[i].revision=0;}); bad([&](auto& x){x.associators[i].ab.key+=1;});
        bad([&](auto& x){x.associators[i].bc.revision+=1;}); bad([&](auto& x){std::reverse(x.associators[i].ab.types.begin(),x.associators[i].ab.types.end());});
        bad([&](auto& x){x.associators[i].left.types.pop_back();}); bad([&](auto& x){x.associators[i].right.types.back().factor_type=999;});
        bad([&](auto& x){x.associators[i].types.pop_back();});
    }
    for (std::size_t i=0;i<12;++i) for (bool block : {false,true}) {
        const auto select=[&](auto& x)->auto&{return block ? x.swaps[i].block : x.swaps[i].inner;};
        bad([&](auto& x){select(x).pop_back();}); bad([&](auto& x){select(x).back()=select(x).front();});
        bad([&](auto& x){select(x).back().source=999;}); bad([&](auto& x){select(x).back().target=999;});
        bad([&](auto& x){std::swap(select(x).front().target,select(x).back().target);});
    }
    auto reordered=d;
    for (auto& a : reordered.associators) { std::reverse(a.left.types.begin(),a.left.types.end()); std::reverse(a.right.types.begin(),a.right.types.end()); std::reverse(a.types.begin(),a.types.end()); }
    for (auto& s : reordered.swaps) { std::reverse(s.inner.begin(),s.inner.end()); std::reverse(s.block.begin(),s.block.end()); }
    const auto re=Numeric::prepare(f,reordered,guard);
    check(re.declaration().swaps[0].inner == reordered.swaps[0].inner && re.cells() == n.cells(),"declared word order remains retained independently of correspondence");
    rejects<std::invalid_argument>([&]{re.witness(input.field(),guard);},"equal pure values cannot substitute actual owners");
    const auto twin=plan(); rejects<std::invalid_argument>([&]{twin.witness(input,available);},"equal native values cannot substitute actual owners");
    rejects<std::invalid_argument>([&]{twin.step(input,Action::BlockInterchange,available);},"native edge rejects another plan");
    const auto twin_zero=twin.numeric_plan().field(carrier(0),{2},{{c,Q{0}}},guard);
    rejects<std::invalid_argument>([&]{n.boundary(twin_zero,guard);},"supported zero cannot bypass pure ownership");
    rejects<std::invalid_argument>([&]{p.field(carrier(0),{0},{{c,0}},available);},"supported zero checks grade");
    rejects<std::out_of_range>([&]{p.field({static_cast<Permutation>(99),Association::Left},{0},{},available);},"empty input rejects unknown permutation");
    rejects<std::invalid_argument>([&]{p.field({Permutation::ABC,static_cast<Association>(99)},{0},{},available);},"empty input rejects unknown association");
    rejects<std::out_of_range>([&]{p.field(carrier(0),{9},{},available);},"empty input rejects undeclared grade");
    auto invalid=ids; invalid[2].index=999;
    rejects<std::out_of_range>([&]{p.cell_witness(invalid,available);},"all original cell identities checked");
    const auto zero=p.field(carrier(0),{0},{},available);
    rejects<std::invalid_argument>([&]{p.boundary(zero,available);},"grade zero has no invented boundary");
    rejects<std::invalid_argument>([&]{p.step(zero,static_cast<Action>(99),available);},"empty input rejects unknown operation");
    auto q=Q{1}; mpz_set_ui(mpq_denref(q.get_mpq_t()),0);
    rejects<std::invalid_argument>([&]{p.field(carrier(0),{2},{{c,q}},available);},"malformed rational rejects before arithmetic");
    Q unreduced{1}; mpz_set_ui(mpq_numref(unreduced.get_mpq_t()),2); mpz_set_ui(mpq_denref(unreduced.get_mpq_t()),6);
    const auto canon=p.field(carrier(0),{2},{{c,unreduced}},available);
    check(canon.field().values().at(c) == Q{1,3},"declared rational input has exact canonical values");
    const auto signed_image=p.step(input,Action::InnerInterchange,available); const auto ci=n.carrier(signed_image.field());
    const auto unsigned_image=p.field(ci,{2},{{cell(ci,ids),Q{1}}},available);
    const auto mapped=p.step(p.boundary(input,available),Action::InnerInterchange,available), unsigned_boundary=p.boundary(unsigned_image,available);
    check(mapped.field().values() != unsigned_boundary.field().values() && signed_image.field().values().at(cell(ci,ids)) == -1,"odd interchange sign is necessary for chain naturality");
    const auto same_cell=cell(carrier(0),{e,e,e});
    const auto ab=p.field(carrier(0),{3},{{same_cell,Q{0}}},available), ba=p.field(carrier(4),{3},{{same_cell,Q{0}}},available);
    check(ab.field().values() == ba.field().values() && !ab.field().plan().same_owner(ba.field().plan()),"equal positions and supported zero values do not identify carriers");
    const auto block=p.step(input,Action::BlockInterchange,available), back=p.step(block,Action::BlockInterchange,available);
    check(back.input()->input() && back.input()->provenance().step == Action::BlockInterchange && back.field().values() == input.field().values(),"equal output does not erase either native operation");
    for (std::size_t j=0;j<3;++j) {
        auto stale=available; --stale[j].max_sequence;
        rejects<std::invalid_argument>([&]{Plan::prepare(p.declaration(),stale);},"each preparation cutoff is independent");
        for (std::size_t i=0;i<12;++i) {
            const auto in=p.field(carrier(i),{2},{{cell(carrier(i),ids),Q{1}}},available);
            rejects<std::invalid_argument>([&]{p.witness(in,stale);},"every permutation checks the original publication cutoffs");
        }
        stale=available; stale[j].source_namespace=999;
        rejects<std::invalid_argument>([&]{Plan::prepare(p.declaration(),stale);},"each preparation namespace checked");
        rejects<std::invalid_argument>([&]{p.field(carrier(11),{0},{},stale);},"empty permuted input checks original namespaces");
    }
    auto future=available; for (auto& a : future) a.max_sequence+=100;
    const auto later=p.witness(input,future);
    check(later.witness().hexagons()[0].direct.image.values() == prior.witness().hexagons()[0].direct.image.values(),"later availability never replaces pinned source bytes");
    std::array<std::unique_ptr<storage::NativeAccessionDirectory>,3> dirs;
    for (std::size_t j=0;j<3;++j) dirs[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[j],available[j].max_sequence),available[j].source_namespace);
    const auto pins=[&]{return std::array{native_product_example::anchor(*dirs[0]),native_product_example::anchor(*dirs[1]),native_product_example::anchor(*dirs[2])};};
    const auto r=request(pins());
    for (std::size_t j=0;j<3;++j) {
        auto x=r; x.factor_identities[j]=0; rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"all original factor roles must be named");
        x=r; x.factors[j]=dirs[j]->pin(storage::NativeAccessionRequest::sparse_chain({0},{10},{{10,Q{0}}}));
        rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"supported zero is not an empty owning anchor");
        auto cochain=storage::NativeAccessionRequest::sparse_chain({0},{},{}); cochain.kind=storage::NativeAccessionKind::Cochain; cochain.pairing=storage::NativePairing::AlgebraicDual;
        x=r; x.factors[j]=dirs[j]->pin(cochain); rejects<std::invalid_argument>([&]{Plan::prepare(x,available);},"dual does not identify a source Chain");
    }
    auto alias=r; alias.factor_identities[2]=alias.factor_identities[0];
    rejects<std::invalid_argument>([&]{Plan::prepare(alias,available);},"same role name cannot alias different publications");
    alias=r; alias.factors[2]=alias.factors[0]; alias.factor_identities[2]=alias.factor_identities[0]; alias.declaration=declaration({f[0],f[1],f[0]});
    auto alias_available=available; alias_available[2]=alias_available[0];
    const auto repeated=Plan::prepare(alias,alias_available);
    check(repeated.factor(2).endpoint() == alias.factors[0].endpoint() && !repeated.numeric_plan().associator(Permutation::ABC).same_owner(repeated.numeric_plan().associator(Permutation::CBA)),"explicit repeated factor retains distinct actual nested carriers");
    storage::NativeAccessionDirectory foreign(storage::TemporalEndpoint(f[0],7),910); alias.factors[2]=native_product_example::anchor(foreign);
    rejects<std::invalid_argument>([&]{Plan::prepare(alias,alias_available);},"equal bytes and namespace cannot alias another authority");
    const auto old=Plan::prepare(r,available); const auto oi=old.field(carrier(9),{2},{{cell(carrier(9),ids),Q{1}}},available);
    for (std::size_t j=0;j<3;++j) { const auto endpoint=dirs[j]->pin_snapshot(); auto next=*endpoint; ++next.sequence;
        dirs[j]->advance(storage::make_temporal_segment(*endpoint,next)); check(old.factor(j).endpoint() == endpoint,"all original publications remain pinned"); }
    rejects<std::invalid_argument>([&]{Plan::prepare(request(pins()),available);},"future sources reject old cutoffs");
    auto next_available=available; for (auto& a : next_available) ++a.max_sequence;
    const auto next=Plan::prepare(request(pins()),next_available); const auto ni=next.field(carrier(9),{2},{{cell(carrier(9),ids),Q{1}}},next_available);
    rejects<std::invalid_argument>([&]{old.step(ni,Action::BlockInterchange,next_available);},"future equal output cannot enter an old owner");
    check(old.witness(oi,available).input()->plan().same_owner(old),"old execution remains independent of later publication");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"owned allocation must be admitted");
    policy={}; policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"unsupported required GPU rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(input,available,runtime::ExecutionLease(policy));
    check(fallback.provenance().fallback,"explicit CPU fallback retained");
    policy={}; policy.limits=prior.provenance().requirements; policy.limits.scalar_work=64000000; policy.limits.integer_bits=1u<<24;
    const auto at_action=p.witness(input,available,runtime::ExecutionLease(policy));
    check(at_action.witness().hexagons()[0].direct.image.values() == prior.witness().hexagons()[0].direct.image.values(),"at-limit action workspace admitted");
    --policy.limits.rational_slots; rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below full action envelope rejects");
    policy={}; policy.limits.records=n.retained_words()-1;
    rejects<runtime::ExecutionLimitExceeded>([&]{p.field(carrier(11),{0},{},available,runtime::ExecutionLease(policy));},"empty action includes all retained words");
    policy={}; policy.limits.scalar_work=prior.provenance().execution_steps-1;
    rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below measured scalar work rejects");
    policy={}; policy.limits.records=n.retained_words(); policy.limits.rational_slots=policy.limits.sparse_entries=2*n.entry_envelope();
    const auto at=Plan::prepare(r,available,runtime::ExecutionLease(policy)); const auto atn=at.numeric_plan();
    check(atn.cells() == n.cells(),"at-limit preparation admits all certificate scratch before copying");
    --policy.limits.sparse_entries; rejects<runtime::ExecutionLimitExceeded>([&]{Plan::prepare(r,available,runtime::ExecutionLease(policy));},"below preparation envelope rejects");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(input,available,runtime::ExecutionLease({},stop));},"owned cancellation rejects unpublished work");
    auto small=r; small.dimension_limit=1; rejects<std::length_error>([&]{Plan::prepare(small,available);},"all nested grade populations bounded");
    auto moved=d; auto& declaration_alias=moved.swaps[0].inner.front().target; Guard moved_guard({},[&]{declaration_alias=999;});
    const auto frozen=Numeric::prepare(f,std::move(moved),moved_guard);
    check(frozen.declaration().swaps[0].inner == d.swaps[0].inner,"moved declaration aliases cannot change retained maps during checkpoints");
    Numeric::Values values{{c,Q{1}}}; auto& value_alias=values.begin()->second; Guard value_guard({},[&]{value_alias=2;});
    const auto frozen_field=n.field(carrier(0),{2},std::move(values),value_guard);
    check(frozen_field.values().at(c) == 1,"movable field aliases frozen before the first checkpoint");
    auto disposable=std::make_unique<Numeric>(n); Guard destroy({},[&]{disposable.reset();});
    const auto kept=disposable->cell_witness(ids,destroy); check(kept.plan().same_owner(n),"checkpoint self destruction keeps state owned");
    std::vector<std::future<inference::NativeProductHexagonChainWitness>> jobs;
    for (unsigned j=0;j<6;++j) jobs.push_back(std::async(std::launch::async,[p,input]{return p.witness(input,available);}));
    for (auto& job : jobs) { const auto w=job.get(); check(w.witness().hexagons()[0].direct.image.values() == prior.witness().hexagons()[0].direct.image.values(),"concurrent exact actions use independent scratch"); }
    const auto bf=sources(8); const auto growth=cell(carrier(0),{v,e,v});
    for (std::size_t limited=0;limited<3;++limited) {
        std::array<std::unique_ptr<storage::NativeAccessionDirectory>,3> bounded;
        for (std::size_t j=0;j<3;++j) { storage::NativeAccessionLimits cap; if (j == limited) { cap.max_integer_bytes=1; cap.max_action_work=3999999; }
            bounded[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(bf[j],available[j].max_sequence),available[j].source_namespace,
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto data){return storage::rfcx_detail::fnv1a64(data);},cap); }
        const auto bp=Plan::prepare(request({native_product_example::anchor(*bounded[0]),native_product_example::anchor(*bounded[1]),native_product_example::anchor(*bounded[2])}),available);
        const auto bi=bp.field(carrier(0),{1},{{growth,Q{253,251}}},available);
        check(bi.provenance().action_limits.integer_bits == 8 && bi.provenance().action_limits.steps == 3999999,"every original authority contributes arithmetic limits");
        rejects<std::length_error>([&]{bp.boundary(bi,available);},"each authority bounds arithmetic on another factor");
    }
    core::RelationSignature sig; core::RelationConstraints constraint;
    constraint.rules.push_back(core::RelationRule::role_magnitude(sig.existence_aspect(),Q{509,503}));
    (void)sig.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraint); core::RelationalComplexBuilder builder(sig); builder.declare_grade({2});
    auto empty=bf; empty[0]=builder.freeze(); numeric::ExactActionLimits limit; limit.integer_bits=8; Guard unused(limit);
    rejects<std::length_error>([&]{Numeric::prepare(empty,declaration(empty),unused);},"unused exact rules checked even without cells");
    auto empty_d=declaration(empty); empty_d.swaps[11].block.pop_back();
    rejects<std::invalid_argument>([&]{Numeric::prepare(empty,empty_d,guard);},"empty carriers still need complete block directions");
    for (std::size_t j=0;j<3;++j) rejects<std::invalid_argument>([&]{
        core::RelationSignature s; const auto t=s.add_type("deformed"); core::RelationalComplexBuilder broken(s);
        broken.add_grade_one(core::RelationKey{100},t,{{10,true,true,Q{1},{},core::OccurrenceKey{101}},{20,true,false,Q{1},{},core::OccurrenceKey{102}}});
        broken.add_higher({2},core::RelationKey{200},t,{{0,true,false,Q{1},{},core::OccurrenceKey{201}}});
        auto changed=f; changed[j]=broken.freeze(); Numeric::prepare(changed,declaration(changed),guard);
    },"each factor requires the full strict chain family");
    rejects<std::overflow_error>([&]{(void)algebra::product_hexagon_detail::mul(std::numeric_limits<std::size_t>::max(),2);},"checked population product overflow");
    for (std::size_t j=0;j<3;++j) check(storage::encode_rfcx(p.factor(j).endpoint()->source).bytes == bytes[j],"failures preserve all original source bytes");
    const auto retained=p.witness(prior_input,available);
    check(retained.witness().hexagons()[0].direct.image.values() == prior.witness().hexagons()[0].direct.image.values(),"failures preserve retained exact results");
}
} // namespace
int main(int argc,char** argv) {
    const bool records=argc == 2 && std::string_view(argv[1]) == "--records";
    try { samples(records); if (!records) { contracts(); std::cout << "checks=" << checks << " cells=" << cells << " witnesses=" << witnesses << '\n'; } }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } return 0;
}
