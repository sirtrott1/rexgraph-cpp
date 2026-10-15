#include "../../examples/native_gluing/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_gluing_example;
using Source=algebra::GluingSource;
std::size_t checks{},cases{},sections{};
void check(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
template<class Error,class F> void rejects(F f,const char* message) { bool caught=false; try { f(); } catch (const Error&) { caught=true; } check(caught,message); }
bool zero(const Values& values) { for(const auto& [i,q]:values) { (void)i; if(q!=0) return false; } return true; }
Values values(const inference::DerivedNativeGluedChain& chain) { return chain.chain().values(); }
Values values(const inference::MappedNativeGluingChain& chain) { return chain.values(); }
Values boundary(const core::RelationalComplex& source,core::Grade g,const Values& input) {
    Guard guard; Values output; const auto& a=source.grade(g);
    for(const auto& [i,q]:input) { const auto [begin,end]=a.incidence_range(i); for(auto p=begin;p<end;++p) {
        auto& v=output.try_emplace(a.lower_index[p],Q{0}).first->second;
        if(a.aspects.get(source.signature().existence_aspect(),p)) v=guard.add(v,guard.mul(q,source.incidence_coefficient(g,p)));
    }} return output;
}
void json(const Values& a) { std::cout<<'['; bool comma=false; for(const auto& [i,q]:a) { if(comma) std::cout<<','; comma=true; std::cout<<'['<<i<<",\""<<q<<"\"]"; } std::cout<<']'; }
template<class T> void rows(const std::vector<std::vector<T>>& a) { std::cout<<'['; for(std::size_t k=0;k<a.size();++k) { if(k) std::cout<<','; std::cout<<'['; for(std::size_t i=0;i<a[k].size();++i) { if(i) std::cout<<','; std::cout<<a[k][i]; } std::cout<<']'; } std::cout<<']'; }
void json(const algebra::RetainedSourceMap& m) { std::cout<<"{\"cells\":"; rows(m.cells); std::cout<<",\"occurrences\":"; rows(m.occurrences); std::cout<<'}'; }
void json(const core::RelationalComplex& c) {
    std::cout<<"{\"sizes\":["; for(std::size_t k=0;k<c.grade_count();++k) { if(k) std::cout<<','; std::cout<<c.grade(grade(static_cast<std::uint16_t>(k))).cell_count(); }
    std::cout<<"],\"occurrences\":[";
    for(std::size_t k=0;k<c.grade_count();++k) {
        if(k) std::cout<<',';
        const auto g=grade(static_cast<std::uint16_t>(k)); const auto& a=c.grade(g); std::cout<<'[';
        for(std::size_t p=0;p<a.incidence_count();++p) {
            if(p) std::cout<<',';
            std::cout<<'['<<a.upper_index[p]<<','<<a.lower_index[p]<<','<<a.occurrence_identities[p].value<<','<<a.aspects.get(c.signature().existence_aspect(),p)<<",\""<<c.incidence_coefficient(g,p)<<"\",[";
            for(std::size_t j=0;j<c.signature().aspects().size();++j) { if(j) std::cout<<','; std::cout<<a.aspects.get(static_cast<composite::AspectId>(j),p); }
            std::cout<<"]]";
        } std::cout<<']';
    } std::cout<<"]}";
}
void emit(const Plan& plan,const Map& map,core::Grade g,const Values& input,const Values& output,const std::optional<Values>& before,const std::optional<Values>& after) {
    const auto numeric=plan.numeric_plan(); const auto& d=numeric.declaration();
    std::cout<<"{\"kind\":\"chain\",\"grade\":"<<g.value<<",\"sources\":[";
    for(const auto s:{Source::Common,Source::Left,Source::Right,Source::Glued}) { if(s!=Source::Common) std::cout<<','; json(numeric.source(s)); }
    std::cout<<"],\"square\":["; json(d.common_left); std::cout<<','; json(d.common_right); std::cout<<','; json(d.left_glued); std::cout<<','; json(d.right_glued);
    std::cout<<"],\"target\":"; json(map.numeric_plan().target());
    std::cout<<",\"maps\":["; json(map.numeric_plan().participant_map(false)); std::cout<<','; json(map.numeric_plan().participant_map(true)); std::cout<<','; json(map.numeric_plan().map());
    std::cout<<"],\"input\":"; json(input); std::cout<<",\"output\":"; json(output);
    std::cout<<",\"source_boundary\":"; if(before) json(*before); else std::cout<<"null";
    std::cout<<",\"target_boundary\":"; if(after) json(*after); else std::cout<<"null";
    std::cout<<"}\n";
}
inference::NativeGluingMorphismRequest identity_request(const Plan& plan,const storage::NativeAccessionDirectory& target) {
    auto request=map_request(plan,target); const auto constructed=plan.constructed_source();
    for(auto* m:{&request.left,&request.right}) {
        for(auto& c:m->cells) c.to=constructed->cells.at({m->from,c.from});
        for(auto& o:m->occurrences) o.to=constructed->occurrences.at({m->from,o.from});
    } return request;
}
void samples(bool records) {
    for(const auto& e:std::array<Q,3>{Q{1},Q{2,3},Q{5,2}}) for(const auto& f:std::array<Q,2>{Q{1},Q{3,4}}) {
        const auto plan=prepare(e,f); const auto constructed=plan.constructed_source();
        for(unsigned family=0;family<2;++family) {
            storage::NativeAccessionDirectory target(storage::TemporalEndpoint(family?constructed->source:triangle(e,f),10),903);
            const auto map=Map::prepare(plan,family?identity_request(plan,target):map_request(plan,target),map_available);
            for(std::uint16_t k=0;k<4;++k) for(std::uint32_t i=0;i<constructed->source.grade(grade(k)).cell_count();++i)
                for(const auto& q:std::array<Q,2>{Q{0},Q{-3,7}}) {
                    const auto input=plan.chain(grade(k),{{i,q}},available); const auto output=map.execute(input,map_available);
                    check(output.input()->plan().same_owner(plan),"mediator loses original input owner");
                    check(output.values().size()==1,"basis target support disappeared");
                    check(output.values().begin()->second==q,"basis mediator value");
                    check(output.plan().target().endpoint()==map.target().endpoint(),"mediator output loses target owner");
                    std::optional<Values> before,after;
                    if(k) {
                        const auto d=plan.boundary(input,available); before=values(d); after=boundary(map.numeric_plan().target(),grade(k),output.values());
                        check(values(map.execute(d,map_available))==*after,"full strict mediator fails boundary naturality or support");
                        if(k>1) check(zero(values(plan.boundary(d,available))),"full glued chain family fails");
                    }
                    if(records) emit(plan,map,grade(k),values(input),output.values(),before,after);
                    ++cases;
                }
        }
        for(std::uint16_t k=0;k<4;++k) for(const auto& q:std::array<Q,2>{Q{0},Q{5,3}}) {
            Values left,right;
            const auto numeric=plan.numeric_plan();
            for(std::uint32_t i=0;i<numeric.source(Source::Left).grade(grade(k)).cell_count();++i) left.emplace(i,q);
            for(std::uint32_t i=0;i<numeric.source(Source::Right).grade(grade(k)).cell_count();++i) right.emplace(i,q);
            const auto out=plan.glue_sections(42,grade(k),local_chain(plan,false,grade(k),left),local_chain(plan,true,grade(k),right),available);
            check(out.chain().values().size()==numeric.source().grade(grade(k)).cell_count(),"complete section loses supported zero cells");
            for(const auto& [i,v]:out.chain().values()) { (void)i; check(v==q,"overlap was summed rather than glued"); }
            check(out.local_inputs().size()==2 && out.provenance().field_identity==42,"glued section loses complete local inputs/identity");
            if(records) {
                std::cout<<"{\"kind\":\"section\",\"grade\":"<<k<<",\"maps\":["; json(numeric.declaration().left_glued); std::cout<<','; json(numeric.declaration().right_glued);
                std::cout<<"],\"left\":"; json(left); std::cout<<",\"right\":"; json(right); std::cout<<",\"output\":"; json(out.chain().values()); std::cout<<"}\n";
            } ++sections;
        }
    }
}
void contracts() {
    const auto plan=prepare(); const auto constructed=plan.constructed_source(); auto numeric=plan.numeric_plan();
    check(constructed->source.grade(grade(0)).cell_count()==4 && constructed->source.grade(grade(1)).cell_count()==5,"wrong retained pushout counts");
    check(constructed->cells.at({2,cell(0,30)})!=constructed->cells.at({3,cell(0,30)}),"equal raw keys imply identity outside interface");
    check(constructed->cells.at({2,cell(1,100)})==constructed->cells.at({3,cell(1,100)}),"declared common relation duplicated");
    check(constructed->source.grade(grade(1)).incidence_count()==30,"common occurrence fiber duplicated or discarded");
    std::size_t inactive{},active_zero{};
    for(std::uint16_t k=1;k<4;++k) { const auto& a=constructed->source.grade(grade(k)); for(std::size_t p=0;p<a.incidence_count();++p) {
        if(!a.aspects.get(constructed->source.signature().existence_aspect(),p)) ++inactive;
        else if(constructed->source.incidence_coefficient(grade(k),p)==0) ++active_zero;
    }} check(inactive==9 && active_zero==9,"inactive or active-zero retention");
    const auto canonical=storage::encode_rfcx(plan.atlas().glue(500).source).bytes;
    check(storage::encode_rfcx(constructed->source).bytes==canonical,"prepared source differs from existing canonical atlas construction");
    auto broken=numeric.declaration(); broken.common_left.occurrences[1].pop_back(); Guard guard;
    rejects<std::invalid_argument>([&]{algebra::OwnedRelationalGluing::prepare(numeric.source(Source::Common),numeric.source(Source::Left),numeric.source(Source::Right),numeric.source(),broken,guard);},"omitted inactive interface occurrence accepted");
    broken=numeric.declaration(); broken.right_glued.cells[0][2]=broken.left_glued.cells[0][2]; Guard collision;
    rejects<std::invalid_argument>([&]{algebra::OwnedRelationalGluing::prepare(numeric.source(Source::Common),numeric.source(Source::Left),numeric.source(Source::Right),numeric.source(),broken,collision);},"undeclared equal-key identification accepted");
    broken=numeric.declaration(); std::swap(broken.common_right.occurrences[1][2],broken.common_right.occurrences[1][3]); Guard polarity;
    rejects<std::invalid_argument>([&]{algebra::OwnedRelationalGluing::prepare(numeric.source(Source::Common),numeric.source(Source::Left),numeric.source(Source::Right),numeric.source(),broken,polarity);},"changed occurrence polarity accepted");
    Guard field_guard; Values draft{{0,Q{1}}}; auto* alias=&draft.begin()->second; bool changed=false;
    Guard owned_guard({},[&]{if(!changed){changed=true; *alias=Q{100};}});
    const auto frozen=numeric.chain(grade(0),std::move(draft),owned_guard); check(frozen.values().at(0)==1,"moved field aliases checkpoint mutation");
    auto declaration=numeric.declaration(); auto* map_alias=&declaration.common_left.cells[0][0]; changed=false;
    Guard map_guard({},[&]{if(!changed){changed=true; *map_alias=99;}});
    const auto captured=algebra::OwnedRelationalGluing::prepare(numeric.source(Source::Common),numeric.source(Source::Left),numeric.source(Source::Right),numeric.source(),std::move(declaration),map_guard);
    check(captured.declaration().common_left.cells[0][0]==0,"moved declaration aliases checkpoint mutation");
    auto handle=numeric; changed=false; Guard replace({},[&]{if(!changed){changed=true; handle=captured;}});
    const auto old=handle.chain(grade(0),{{0,Q{0}}},replace); check(old.gluing().same_owner(numeric),"callback replaces actual source owner");
    auto current=available; current.left.source_namespace=999;
    rejects<std::invalid_argument>([&]{plan.chain(grade(0),{},current);},"foreign current source namespace accepted for empty action");
    current=available; current.right.max_sequence=8;
    rejects<std::invalid_argument>([&]{plan.chain(grade(0),{},current);},"future right source accepted for empty action");
    current=available; current.common.max_sequence=6;
    rejects<std::invalid_argument>([&]{plan.chain(grade(0),{},current);},"future common interface accepted");
    const auto twin=prepare(); const auto from_twin=twin.chain(grade(1),{{0,Q{0}}},available);
    rejects<std::invalid_argument>([&]{plan.boundary(from_twin,available);},"equal-key foreign glued field accepted");
    const auto partial=local_chain(plan,false,grade(0),{{0,Q{0}}});
    rejects<std::invalid_argument>([&]{plan.glue_sections(42,grade(0),partial,local_chain(plan,true,grade(0),{},true),available);},"partial section implicitly extended");
    rejects<std::invalid_argument>([&]{plan.glue_sections(0,grade(0),partial,partial,available);},"same-field identity omitted");
    const Values a{{0,0},{1,1},{2,2}},b{{0,0},{1,2},{2,2}};
    rejects<std::invalid_argument>([&]{plan.glue_sections(42,grade(0),local_chain(plan,false,grade(0),a),local_chain(plan,true,grade(0),b),available);},"same-field obstruction silently repaired");
    rejects<std::invalid_argument>([&]{plan.include(false,grade(0),local_chain(twin,false,grade(0),a),available);},"foreign equal-valued participant field accepted");
    const auto inc=plan.include(false,grade(1),local_chain(plan,false,grade(1),{{0,Q{0}}}),available);
    check(inc.local_inputs().size()==1 && values(plan.boundary(inc,available)).size()==2,"inclusion loses supported zero history");
    const auto empty=prepare(1,1,true); check(empty.constructed_source()->source.grade_count()==4,"empty grade inventory lost");
    check(values(empty.chain(grade(3),{},available)).empty(),"empty declared field unsupported");
    const auto source_pin=plan.constructed_source(); check(source_pin->cells.size()==16,"standalone source pin loses original provenance");
    const auto survived=[] { auto p=prepare(); return p.chain(grade(2),{{0,Q{1}}},available); }();
    check(survived.plan().boundary(survived,available).input()->plan().same_owner(survived.plan()),"result loses source lifetime or input ancestry");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation=false;
    rejects<std::invalid_argument>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));},"allocation policy bypass on empty action");
    policy={}; policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));},"unsupported GPU used");
    policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));
    check(fallback.provenance().fallback,"fallback omitted from receipt");
    policy={}; policy.limits.scalar_work=1;
    rejects<std::length_error>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));},"current full source work unchecked");
    policy={}; policy.limits.integer_bits=0;
    rejects<std::length_error>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));},"current zero integer cap bypass");
    policy={}; policy.limits.rational_slots=1;
    rejects<std::length_error>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease(policy));},"current containers unchecked");
    const auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{plan.chain(grade(0),{},available,runtime::ExecutionLease({},stop));},"owned cancellation ignored");
    rejects<std::length_error>([&]{Plan::prepare({plan.atlas(),500,4},available);},"glued output dimension bypass");
    storage::NativeAccessionDirectory target(storage::TemporalEndpoint(triangle(),10),903);
    auto request=map_request(plan,target); const auto map=Map::prepare(plan,request,map_available);
    auto bad=request; bad.left.cells.pop_back();
    rejects<std::invalid_argument>([&]{Map::prepare(plan,bad,map_available);},"unused map source direction omitted");
    bad=request; bad.right.occurrences[0].to=occurrence(1,1002);
    rejects<std::invalid_argument>([&]{Map::prepare(plan,bad,map_available);},"complete occurrence map changed");
    const auto input=plan.chain(grade(2),{{0,Q{1}},{1,Q{-1}}},available); const auto result=map.execute(input,map_available);
    check(result.values().size()==1 && result.values().at(0)==0,"noninjective mediator loses cancellation support");
    check(result.input()->plan().same_owner(plan),"mediator loses ordered input history");
    auto map_cut=map_available; map_cut.target.max_sequence=9;
    rejects<std::invalid_argument>([&]{map.execute(input,map_cut);},"future target accepted");
    map_cut=map_available; map_cut.source.common.source_namespace=999;
    rejects<std::invalid_argument>([&]{map.execute(input,map_cut);},"foreign common map cutoff accepted");
    rejects<std::invalid_argument>([&]{map.execute(twin.chain(grade(0),{},available),map_available);},"foreign source field accepted by mediator");
    std::vector<std::future<Values>> futures;
    for(unsigned i=0;i<6;++i) futures.push_back(std::async(std::launch::async,[plan,map,input]{return values(map.execute(plan.boundary(input,available),map_available));}));
    for(auto& future:futures) check(future.get()==boundary(map.numeric_plan().target(),grade(2),result.values()),"concurrent scratch changed exact output");
    // Same-field restriction and the chain boundary are separate contracts.
    const auto sections_input=plan.glue_sections(42,grade(1),local_chain(plan,false,grade(1),{{0,0},{1,1},{2,0}}),local_chain(plan,true,grade(1),{{0,0},{1,0},{2,0}}),available);
    check(!zero(values(plan.boundary(sections_input,available))),"global section boundary discarded exterior contributions");
    rejects<std::invalid_argument>([&]{plan.glue_sections(42,grade(0),local_chain(plan,false,grade(0),{{0,0},{1,-1},{2,1}}),local_chain(plan,true,grade(0),{{0,0},{1,0},{2,0}}),available);},"local boundary mismatch falsely descends as same field");
    const auto later_atlas=plan.atlas().advanced({{3,storage::make_temporal_segment(*plan.source_anchor(Source::Right).endpoint(),storage::TemporalEndpoint(triangle(),10))}});
    const auto later_input=later_atlas.access(3,storage::NativeAccessionRequest::sparse_chain(grade(0),{10},{}));
    rejects<std::invalid_argument>([&]{plan.include(true,grade(0),later_input,available);},"later participant substituted into historical gluing");
    check(plan.source_anchor(Source::Right).sequence()==9,"participant advancement rewrites old source");
    std::vector<storage::NativeAtlasSource> disjoint_sources{{1,900,storage::TemporalEndpoint(triangle(1,1,true,true),7)},
        {2,901,storage::TemporalEndpoint(triangle(),8)},{3,902,storage::TemporalEndpoint(triangle(),9)}};
    const auto disjoint=Plan::prepare({storage::NativeAtlas(disjoint_sources,manifest(true)),500},available);
    check(disjoint.constructed_source()->source.grade(grade(0)).cell_count()==6 && disjoint.constructed_source()->source.grade(grade(1)).cell_count()==6,"empty interface incorrectly matches equal keys");
    // Equal numerical zero responses cannot identify two retained occurrences.
    for(const bool ordered:{false,true}) {
        core::RelationSignature sig;
        if(ordered) sig.add_attaching_loop_type("ordered"); else sig.add_type("unordered");
        core::RelationalComplexBuilder builder(sig);
        builder.add_grade_one(core::RelationKey{100},0,{{10,true,false,Q{0},{},core::OccurrenceKey{101}},
            {10,true,false,Q{0},{},core::OccurrenceKey{102}}});
        const auto source=builder.freeze();
        const auto definition=target_map(1,source);
        storage::NativeAtlasManifest small; small.key=701; small.revision=1;
        small.embeddings={{1,1,2,{},definition.cells,definition.occurrences},{2,1,3,{},definition.cells,definition.occurrences}};
        small.pushouts={{500,1,2,{},{}}};
        const auto square=Plan::prepare({storage::NativeAtlas({{1,900,storage::TemporalEndpoint(source,7)},
            {2,901,storage::TemporalEndpoint(source,8)},{3,902,storage::TemporalEndpoint(source,9)}},small),500},available);
        storage::NativeAccessionDirectory to(storage::TemporalEndpoint(source,10),903);
        auto maps=map_request(square,to); std::swap(maps.right.occurrences[0].to,maps.right.occurrences[1].to);
        rejects<std::invalid_argument>([&]{Map::prepare(square,maps,map_available);},ordered?"ordered occurrence map silently permuted":"zero-valued common occurrences silently identified");
    }
    const auto original_map=map.numeric_plan(); auto left_map=original_map.participant_map(false),right_map=original_map.participant_map(true);
    auto* mutable_map=&right_map.cells[0][0]; changed=false;
    Guard freeze_map({},[&]{if(!changed){changed=true; *mutable_map=99;}});
    const auto safe_map=algebra::OwnedGluingMorphism::prepare(numeric,original_map.target(),std::move(left_map),std::move(right_map),freeze_map);
    check(safe_map.participant_map(true).cells[0][0]==0,"moved mediator map aliases checkpoint mutation");
    auto replaced_map=safe_map; changed=false;
    Guard checkpoint({},[&]{if(!changed){changed=true; replaced_map=original_map;}});
    check(replaced_map.apply(frozen,checkpoint).at(0)==1,"mediator execution changes owner under replacement");
    auto moved_request=map_request(plan,target); auto* native_alias=&moved_request.right.cells[0].to;
    const auto frozen_native_map=Map::prepare(plan,moved_request,map_available);
    *native_alias=cell(0,999);
    check(frozen_native_map.declaration().right.cells[0].to==cell(0,10),"native mediator retains caller's mutable declaration alias");
    const auto original=target.pin_snapshot(); auto next=*original; next.sequence=11; target.advance(storage::make_temporal_segment(*original,next));
    check(map.target().sequence()==10 && values(map.execute(input,map_available)).at(0)==0,"target advancement rewrites old map");
}
} // namespace
int main(int argc,char** argv) {
    try { const bool records=argc>1 && std::string_view(argv[1])=="--json"; samples(records); contracts();
        if(!records) std::cout<<"Checks: "<<checks<<"; native gluing cases: "<<cases<<"; sections: "<<sections<<'\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
