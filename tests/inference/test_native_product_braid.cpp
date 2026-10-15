#include "../../examples/native_product_braid/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>
namespace {
using namespace rfc;
using namespace native_braid_example;
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
void emit_cell(unsigned sample,const std::array<core::CellId,3>& ids,const inference::NativeProductBraidCellWitness& native) {
    const auto& w=native.witness().hexagon(); std::cout << "{\"phase\":\"cell\",\"sample\":" << sample << ",\"ids\":";
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
        std::cout << '}';});
    std::cout << ",\"routes\":"; array(native.witness().routes(),[](const auto& both){array(both,[](const auto& route){
        std::cout << "{\"carriers\":"; array(route.carriers,[](auto c){std::cout << index(c);});
        std::cout << ",\"edges\":"; array(route.edges,[](auto i){std::cout << i;});
        std::cout << ",\"flags\":"; array(route.flags,[](const auto& f){std::cout << '[' << f.original_factor << ',';
            array(f.word_positions,[](auto i){std::cout << i;}); std::cout << ',' << f.upper_interchange_flip << ',' << f.lower_interchange_flip
                << ",\"" << f.mapped_boundary_coefficient << "\",\"" << f.image_boundary_coefficient << "\"]";}); std::cout << '}';});});
    std::cout << "}\n";
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
std::vector<Action> adjacent_actions(Carrier c,Generator g) {
    if ((g==Generator::FirstPair)==(c.association==Association::Left)) return {Action::InnerInterchange};
    return {Action::Reassociate,Action::InnerInterchange,Action::Reassociate};
}
void emit_types(unsigned sample,const Braid& p) {
    std::cout << "{\"phase\":\"types\",\"sample\":" << sample << ",\"witnesses\":";
    const auto n=p.numeric_plan(); array(n.type_witnesses(),[](const auto& w){std::cout << "{\"original\":";
        array(w.original_types,[](const auto& t){json(t);}); std::cout << ",\"source\":" << index(w.source) << ",\"routes\":";
        array(w.routes,[](const auto& path){array(path,[](const auto& node){std::cout << '[' << index(node.carrier) << ',' << node.type << ']';});}); std::cout << '}';});
    std::cout << "}\n";
}
void exercise(unsigned sample,const Braid& p,Carrier c,core::Grade g,Numeric::Values values,bool output) {
    const auto h=p.hexagon(); const auto n=h.numeric_plan(); const auto input=h.field(c,g,std::move(values),available);
    const auto native=p.witness(input,available,sample_lease(sample)); const auto& w=native.witness(); ++witnesses;
    check(native.plan().same_owner(p) && w.plan().same_owner(p.numeric_plan()),"witness retains its actual proof owner");
    check(!input.input() && input.plan().same_owner(h),"canonical input has its actual hexagon owner");
    std::size_t native_steps{};
    for (std::size_t r=0;r<2;++r) {
        const auto& route=w.routes()[r]; const auto& actual=native.native_paths()[r];
        check(route.path.size()==4 && route.steps.size()==3,"both adjacent words retain three normalized steps");
        check(actual.size()==(r==index(c)%2 ? 6u : 8u),"five and seven primitive operations remain distinct histories");
        std::size_t position{};
        for (std::size_t j=0;j<3;++j) {
            const auto& step=route.steps[j]; const auto& d=step.data();
            check(step.plan().same_owner(p.numeric_plan()) && d.generator==route.generators[j],"normalized step owns its declared operation");
            check(n.carrier(d.input)==n.carrier(route.path[j]) && d.input.values()==route.path[j].values(),"normalized step input remains canonical");
            check(n.carrier(d.image)==n.carrier(route.path[j+1]) && d.image.values()==route.path[j+1].values(),"normalized step image remains canonical");
            const auto actions=adjacent_actions(n.carrier(d.input),d.generator);
            check(d.edges.size()==actions.size() && d.inverse_path.size()==actions.size()+1,"reassociations and complete inverse route retained");
            check(n.carrier(d.round_trip)==n.carrier(d.input) && d.round_trip.values()==d.input.values(),"each normalized generator is an exact involution");
            for (std::size_t k=0;k<actions.size();++k) {
                const auto& e=d.edges[k]; check(e.action==actions[k],"ordered primitive operation witness");
                const auto& before=actual[position++]; const auto& after=actual[position];
                check(before.field().values()==e.input.values() && n.carrier(before.field())==n.carrier(e.input) && after.field().values()==e.image.values() &&
                    n.carrier(after.field())==n.carrier(e.image),"every actual native intermediate matches its numeric witness");
                check(after.plan().same_owner(h) && after.input() && after.input()->field().values()==before.field().values() &&
                    after.input()->input()==before.input() && after.provenance().step==e.action,"canonical native primitive history is linked without rebinding");
                const auto& report=after.provenance();
                check(report.namespaces==std::array<std::uint64_t,3>{910,911,912} && report.sequences==std::array<std::uint64_t,3>{7,11,19} &&
                    report.factors==std::array<std::uint64_t,3>{41,42,43},"permutations never reorder original temporal roles");
                native_steps+=report.execution_steps;
                check(e.round_trip.values()==e.input.values() && n.carrier(e.round_trip)==n.carrier(e.input),"primitive inverse retains actual source carrier");
                if (g.value) check(e.mapped_boundary && e.image_boundary && e.mapped_boundary->values()==e.image_boundary->values() &&
                    n.carrier(*e.mapped_boundary)==n.carrier(*e.image_boundary),"every primitive operation retains exact chain naturality");
                else check(!e.mapped_boundary && !e.image_boundary,"grade zero has no invented boundary");
            }
            if (g.value) check(d.boundary_path.size()==actions.size()+1 && d.image_boundary &&
                d.boundary_path.back().values()==d.image_boundary->values(),"composed generator retains the boundary route");
            else check(d.boundary_path.empty() && !d.image_boundary,"grade zero has no composed boundary");
        }
        check(position+1==actual.size(),"native route has neither missing nor extra operations");
    }
    const auto& report=native.provenance();
    check(report.native_steps==native_steps && report.execution_steps==report.numeric_steps+native_steps && native_steps>0,"total work includes numeric proof and both native histories");
    check(report.declaration==1900 && report.hexagon_declaration==1600 && report.preparation_availability[2].max_sequence==19 &&
        report.adoption_availability[1].max_sequence==11 && report.execution_availability[0].max_sequence==7 &&
        report.preparation_steps==input.provenance().preparation_steps && report.preparation_policy==input.provenance().preparation_policy,
        "proof identity, original native preparation and three stages of availability retained");
    check(native.native_paths()[0].back().field().values()==native.native_paths()[1].back().field().values() &&
        n.carrier(native.native_paths()[0].back().field())==n.carrier(native.native_paths()[1].back().field()),"exact Yang-Baxter result agrees on actual carrier and full support");
    std::optional<inference::DerivedNativeHexagonField> dd;
    if (g.value>1) { dd=h.boundary(h.boundary(input,available),available);
        for (const auto& [id,q] : dd->field().values()) { (void)id; check(q==0,"complete strict nested chain family"); } }
    if (output) {
        std::cout << "{\"phase\":\"action\",\"sample\":" << sample << ",\"input\":"; json(n,input.field()); std::cout << ",\"routes\":";
        array(w.routes(),[&](const auto& route){std::cout << "{\"generators\":"; array(route.generators,[](auto gen){std::cout << static_cast<unsigned>(gen);});
            std::cout << ",\"path\":"; array(route.path,[&](const auto& f){json(n,f);}); std::cout << ",\"steps\":";
            array(route.steps,[&](const auto& step){const auto& d=step.data(); std::cout << "{\"generator\":" << static_cast<unsigned>(d.generator)
                << ",\"input\":"; json(n,d.input); std::cout << ",\"image\":"; json(n,d.image); std::cout << ",\"back\":"; json(n,d.round_trip);
                std::cout << ",\"edges\":"; array(d.edges,[&](const auto& e){json(n,e);}); std::cout << ",\"inverse\":";
                array(d.inverse_path,[&](const auto& f){json(n,f);}); std::cout << ",\"boundary\":"; array(d.boundary_path,[&](const auto& f){json(n,f);});
                std::cout << ",\"image_boundary\":"; if (d.image_boundary) json(n,*d.image_boundary); else std::cout << "null"; std::cout << '}';}); std::cout << '}';});
        std::cout << ",\"native_paths\":"; array(native.native_paths(),[&](const auto& path){array(path,[&](const auto& f){json(n,f.field());});});
        std::cout << ",\"dd\":"; if (dd) json(n,dd->field()); else std::cout << "null"; std::cout << "}\n";
    }
}
void samples(bool output) {
    for (unsigned sample=0;sample<16;++sample) {
        const auto p=braid(sample); const auto n=p.numeric_plan(); const auto h=p.hexagon(); const auto numeric=h.numeric_plan();
        if (output) { emit_plan(sample,h); emit_types(sample,p); }
        std::size_t type_count=1; for (std::size_t f=0;f<3;++f) type_count*=numeric.factor(f).signature().type_count()+1;
        check(n.type_witnesses().size()==12*type_count,"complete original constructor triples include unused directions");
        for (const auto& w : n.type_witnesses()) {
            check(w.routes[0].front()==w.routes[1].front() && w.routes[0].back()==w.routes[1].back(),"both complete type routes commute");
            check(w.routes[0].size()==(index(w.source)%2 ? 8u : 6u) && w.routes[1].size()==(index(w.source)%2 ? 6u : 8u),"complete type histories retain all reassociations");
        }
        for (const auto& ids : numeric.cells()) {
            const auto native=p.cell_witness(ids,available,sample_lease(sample)); const auto& w=native.witness(); const auto& hw=w.hexagon(); ++cells;
            check(native.plan().same_owner(p) && w.plan().same_owner(n) && hw.plan().same_owner(n.hexagon()),"cell proof owns the actual adopted service");
            for (std::size_t i=0;i<12;++i) {
                const auto& both=w.routes()[i];
                check(both[0].carriers.back()==both[1].carriers.back() && both[0].flags.size()==hw.carriers()[i].flags.size(),"both cell routes preserve the actual final carrier and every flag");
                for (std::size_t r=0;r<2;++r) {
                    const auto& route=both[r]; check(route.carriers.size()==route.edges.size()+1,"cell route retains every actual intermediate carrier");
                    for (std::size_t j=0;j<route.edges.size();++j) {
                        const auto& e=hw.edges()[route.edges[j]]; check(e.source==route.carriers[j] && e.target==route.carriers[j+1],"cell route names actual hexagon edge owners");
                        if (e.action==Action::InnerInterchange) check(e.inner_receipt.has_value(),"adjacent swap retains the canonical pair receipt");
                    }
                    for (const auto& f : route.flags) check(f.word_positions.size()==route.carriers.size() &&
                        f.mapped_boundary_coefficient==f.image_boundary_coefficient,"every original flag has an exact signed boundary trace");
                }
                for (std::size_t j=0;j<both[0].flags.size();++j) {
                    const auto& a=both[0].flags[j]; const auto& b=both[1].flags[j];
                    check(a.original_factor==b.original_factor && a.word_positions.back()==b.word_positions.back() && a.upper_interchange_flip==b.upper_interchange_flip &&
                        a.lower_interchange_flip==b.lower_interchange_flip && a.mapped_boundary_coefficient==b.mapped_boundary_coefficient,"both routes preserve original role, parity and full flag");
                }
            }
            if (output) emit_cell(sample,ids,native);
        }
        if (!output && 4*n.entry_envelope()>runtime::ExecutionHostPolicy{}.limits.rational_slots) {
            rejects<runtime::ExecutionLimitExceeded>([&]{Braid::adopt(h,{1,1},available);},"large stress profile rejects the default logical workspace");
            const auto empty=h.field(carrier(0),{0},{},available);
            rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(empty,available);},"an explicit stress workspace does not weaken later default policy admission");
        }
        for (std::size_t i=0;i<12;++i) for (std::uint16_t g=0;g<=numeric.max_grade().value;++g) {
            exercise(sample,p,carrier(i),{g},{},output); Numeric::Values values;
            for (const auto& ids : numeric.cells()) if (ids[0].grade.value+ids[1].grade.value+ids[2].grade.value==g)
                values.emplace(cell(carrier(i),ids),Q{static_cast<long>(ids[0].index)-1,3});
            exercise(sample,p,carrier(i),{g},std::move(values),output);
        }
    }
}
void contracts() {
    const auto p=braid(); const auto n=p.numeric_plan(); const auto h=p.hexagon(); const auto hn=h.numeric_plan(); Guard guard;
    const core::CellId e{{1},0},v{{0},0}; const std::array ids{e,e,v}; const auto c=cell(carrier(0),ids);
    const auto input=h.field(carrier(0),{2},{{c,Q{1}}},available); const auto prior=p.witness(input,available);
    const std::array bytes{storage::encode_rfcx(h.factor(0).endpoint()->source).bytes,storage::encode_rfcx(h.factor(1).endpoint()->source).bytes,
        storage::encode_rfcx(h.factor(2).endpoint()->source).bytes};
    for (const auto d : {algebra::ProductBraidDeclaration{0,1},algebra::ProductBraidDeclaration{1,0}}) {
        rejects<std::invalid_argument>([&]{BraidNumeric::adopt(hn,d,guard);},"pure braid needs a name and revision");
        rejects<std::invalid_argument>([&]{Braid::adopt(h,d,available);},"native braid needs a name and revision");
    }
    const auto zero=h.field(carrier(0),{0},{},available);
    rejects<std::invalid_argument>([&]{n.adjacent(zero.field(),static_cast<Generator>(99),guard);},"unknown generator rejects on empty pure field");
    rejects<std::invalid_argument>([&]{p.adjacent(zero,static_cast<Generator>(99),available);},"unknown generator rejects on empty native field");
    auto invalid=ids; invalid[2].index=999;
    rejects<std::out_of_range>([&]{p.cell_witness(invalid,available);},"all original cell identities are checked");
    const auto other=braid(); const auto foreign=other.hexagon().field(carrier(0),{2},{{c,Q{0}}},available);
    rejects<std::invalid_argument>([&]{p.witness(foreign,available);},"equal zero output cannot enter another actual hexagon owner");
    rejects<std::invalid_argument>([&]{n.witness(foreign.field(),guard);},"pure foreign owner rejects");
    const auto named=Braid::adopt(h,{1950,2},available); const auto named_witness=named.witness(input,available);
    check(named.hexagon().same_owner(h) && !named.same_owner(p) && named_witness.witness().routes()[0].path.back().values()==prior.witness().routes()[0].path.back().values(),
        "explicit named proof adopters share the same canonical field representation");
    for (std::size_t i=0;i<12;++i) for (const auto gen : {Generator::FirstPair,Generator::SecondPair}) {
        const auto in=h.field(carrier(i),{2},{{cell(carrier(i),ids),Q{1}}},available);
        const auto step=p.step_witness(in,gen,available); const auto back=p.adjacent(step.image(),gen,available);
        check(back.field().values()==in.field().values() && hn.carrier(back.field())==carrier(i),"every normalized native generator has its exact inverse");
        const auto pure=n.adjacent(in.field(),gen,guard);
        check(pure.values()==step.image().field().values() && hn.carrier(pure)==hn.carrier(step.image().field()),"pure and native adjacent actions agree");
        for (std::size_t j=0;j<3;++j) {
            auto stale=available; --stale[j].max_sequence;
            rejects<std::invalid_argument>([&]{p.witness(in,stale);},"every order checks original independent cutoffs");
            rejects<std::invalid_argument>([&]{Braid::adopt(h,{1,1},stale);},"adoption checks original independent cutoffs");
            stale=available; stale[j].source_namespace=999;
            rejects<std::invalid_argument>([&]{p.step_witness(in,gen,stale);},"every generator checks original independent namespaces");
        }
    }
    auto later=available; for (auto& a : later) a.max_sequence+=100;
    const auto later_witness=p.witness(input,later);
    check(later_witness.native_paths()[0].back().field().values()==prior.native_paths()[0].back().field().values(),"later availability cannot substitute source bytes");
    const auto f=sources(0); std::array<std::unique_ptr<storage::NativeAccessionDirectory>,3> dirs;
    for (std::size_t j=0;j<3;++j) dirs[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(f[j],available[j].max_sequence),available[j].source_namespace);
    const auto pins=[&]{return std::array{native_product_example::anchor(*dirs[0]),native_product_example::anchor(*dirs[1]),native_product_example::anchor(*dirs[2])};};
    const auto old_hex=Plan::prepare(request(pins()),available); const auto old=Braid::adopt(old_hex,{1900,1},available);
    const auto oi=old_hex.field(carrier(9),{2},{{cell(carrier(9),ids),Q{1}}},available);
    for (std::size_t j=0;j<3;++j) { const auto endpoint=dirs[j]->pin_snapshot(); auto next=*endpoint; ++next.sequence;
        dirs[j]->advance(storage::make_temporal_segment(*endpoint,next)); check(old.hexagon().factor(j).endpoint()==endpoint,"old proof pins survive every source advance"); }
    rejects<std::invalid_argument>([&]{Plan::prepare(request(pins()),available);},"future sources reject old cutoffs");
    auto next_available=available; for (auto& a : next_available) ++a.max_sequence;
    const auto next_hex=Plan::prepare(request(pins()),next_available); const auto ni=next_hex.field(carrier(9),{2},{{cell(carrier(9),ids),Q{1}}},next_available);
    rejects<std::invalid_argument>([&]{old.witness(ni,next_available);},"future equal values cannot enter an old proof owner");
    const auto old_witness=old.witness(oi,available);
    check(old_witness.native_paths()[0].front().plan().same_owner(old_hex),"old cutoff execution keeps its original source owner");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"allocation is admitted before owned proof work");
    policy={}; policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"unsupported required backend rejects");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(input,available,runtime::ExecutionLease(policy));
    check(fallback.provenance().fallback,"explicit CPU fallback is retained");
    const auto fallback_h=Plan::prepare(h.declaration(),available,runtime::ExecutionLease(policy)); const auto fallback_p=Braid::adopt(fallback_h,{1,1},available);
    const auto fallback_in=fallback_h.field(carrier(0),{0},{},available);
    const auto fallback_witness=fallback_p.witness(fallback_in,available);
    check(fallback_witness.provenance().fallback,"original preparation fallback stays visible through canonical input history");
    const auto fallback_cell=fallback_p.cell_witness(ids,available);
    check(fallback_cell.provenance().fallback && fallback_cell.provenance().preparation_steps==fallback_in.provenance().preparation_steps &&
        fallback_cell.provenance().preparation_policy==fallback_in.provenance().preparation_policy,"original preparation and fallback remain visible without a field input");
    policy={}; policy.limits=prior.provenance().requirements; policy.limits.scalar_work=64000000; policy.limits.integer_bits=1u<<24;
    const auto at_limit=p.witness(input,available,runtime::ExecutionLease(policy));
    check(at_limit.native_paths()[0].back().field().values()==prior.native_paths()[0].back().field().values(),"at-limit full action envelope admitted");
    --policy.limits.rational_slots; rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below full action envelope rejects");
    policy={}; policy.limits.records=n.retained_words()-1;
    rejects<runtime::ExecutionLimitExceeded>([&]{p.witness(zero,available,runtime::ExecutionLease(policy));},"empty proof requires all retained words");
    policy={}; policy.limits.scalar_work=prior.provenance().execution_steps;
    const auto at_work=p.witness(input,available,runtime::ExecutionLease(policy));
    check(at_work.provenance().execution_steps==policy.limits.scalar_work,"exact total work boundary includes both native traces");
    --policy.limits.scalar_work; rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"below total work rejects even when individual components fit");
    policy={}; policy.limits.scalar_work=prior.provenance().numeric_steps+prior.provenance().native_steps/2;
    rejects<std::length_error>([&]{p.witness(input,available,runtime::ExecutionLease(policy));},"per-step guards cannot bypass aggregate work admission");
    policy={}; policy.limits.records=n.retained_words(); policy.limits.rational_slots=policy.limits.sparse_entries=4*n.entry_envelope();
    check(Braid::adopt(h,{1,1},available,runtime::ExecutionLease(policy)).numeric_plan().hexagon().same_owner(hn),"at-limit adoption reuses actual carrier without raw copies");
    --policy.limits.sparse_entries; rejects<runtime::ExecutionLimitExceeded>([&]{Braid::adopt(h,{1,1},available,runtime::ExecutionLease(policy));},"below adoption envelope rejects");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.witness(input,available,runtime::ExecutionLease({},stop));},"owned cancellation rejects unpublished proof work");
    rejects<runtime::ExecutionCancelled>([&]{Braid::adopt(h,{1,1},available,runtime::ExecutionLease({},stop));},"cancelled adoption rejects");
    algebra::ProductBraidDeclaration moved{1,2}; auto& alias=moved.key; Guard freeze({},[&]{alias=0;});
    const auto frozen=BraidNumeric::adopt(hn,std::move(moved),freeze); check(frozen.declaration().key==1,"declaration aliases frozen before callbacks");
    auto disposable=std::make_unique<BraidNumeric>(n); Guard destroy({},[&]{disposable.reset();});
    check(disposable->witness(input.field(),destroy).plan().same_owner(n),"callback destruction preserves owning state");
    std::vector<std::future<inference::NativeProductBraidWitness>> jobs;
    for (unsigned j=0;j<4;++j) jobs.push_back(std::async(std::launch::async,[p,input]{return p.witness(input,available);}));
    for (auto& job : jobs) { const auto w=job.get(); check(w.native_paths()[0].back().field().values()==prior.native_paths()[0].back().field().values(),"concurrent witnesses use independent owned scratch"); }
    const auto bf=sources(8); const auto growth=cell(carrier(0),{v,e,v});
    for (std::size_t limited=0;limited<3;++limited) {
        std::array<std::unique_ptr<storage::NativeAccessionDirectory>,3> bounded;
        for (std::size_t j=0;j<3;++j) { storage::NativeAccessionLimits cap; if (j==limited) { cap.max_integer_bytes=1; cap.max_action_work=3999999; }
            bounded[j]=std::make_unique<storage::NativeAccessionDirectory>(storage::TemporalEndpoint(bf[j],available[j].max_sequence),available[j].source_namespace,
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto data){return storage::rfcx_detail::fnv1a64(data);},cap); }
        const auto bh=Plan::prepare(request({native_product_example::anchor(*bounded[0]),native_product_example::anchor(*bounded[1]),native_product_example::anchor(*bounded[2])}),available);
        const auto bp=Braid::adopt(bh,{1,1},available); const auto bi=bh.field(carrier(0),{1},{{growth,Q{253,251}}},available);
        const auto empty=bh.field(carrier(0),{0},{},available); const auto bounded_w=bp.witness(empty,available);
        check(bounded_w.provenance().action_limits.integer_bits==8 && bounded_w.provenance().action_limits.steps==3999999,"every original source bounds braid work and arithmetic");
        rejects<std::length_error>([&]{bp.witness(bi,available);},"every source authority bounds boundary growth on another factor");
    }
    core::RelationSignature sig; core::RelationConstraints constraint;
    constraint.rules.push_back(core::RelationRule::role_magnitude(sig.existence_aspect(),Q{509,503}));
    (void)sig.add_type("unused-rule",core::BoundaryPolicy::DeclaredExact,0,0,constraint); core::RelationalComplexBuilder builder(sig); builder.declare_grade({2});
    auto empty=bf; empty[0]=builder.freeze();
    storage::NativeAccessionDirectory ea(storage::TemporalEndpoint(empty[0],7),910),eb(storage::TemporalEndpoint(empty[1],11),911),ec(storage::TemporalEndpoint(empty[2],19),912);
    const auto eh=Plan::prepare(request({native_product_example::anchor(ea),native_product_example::anchor(eb),native_product_example::anchor(ec)}),available);
    const auto ep=Braid::adopt(eh,{1,1},available); const auto ei=eh.field(carrier(11),{0},{},available); policy={}; policy.limits.integer_bits=8;
    rejects<std::length_error>([&]{Braid::adopt(eh,{1,1},available,runtime::ExecutionLease(policy));},"adoption checks unused rule scalars even on empty carriers");
    rejects<std::length_error>([&]{ep.witness(ei,available,runtime::ExecutionLease(policy));},"current policy shrink rechecks unused scalars on empty proof input");
    numeric::ExactActionLimits small; small.entries=4*n.entry_envelope()-1; Guard too_small(small);
    rejects<std::length_error>([&]{n.witness(input.field(),too_small);},"pure proof admits the complete retained envelope");
    rejects<std::overflow_error>([&]{(void)algebra::product_braid_detail::mul(std::numeric_limits<std::size_t>::max(),2);},"checked proof population overflow");
    for (std::size_t j=0;j<3;++j) check(storage::encode_rfcx(h.factor(j).endpoint()->source).bytes==bytes[j],"all failures preserve original source bytes");
    const auto retained=p.witness(input,available);
    check(retained.native_paths()[0].back().field().values()==prior.native_paths()[0].back().field().values(),"failed proof work preserves prior exact result");
}
} // namespace
int main(int argc,char** argv) {
    const bool records=argc==2 && std::string_view(argv[1])=="--records";
    try { samples(records); if (!records) { contracts(); std::cout << "checks=" << checks << " cells=" << cells << " witnesses=" << witnesses << '\n'; } }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } return 0;
}
