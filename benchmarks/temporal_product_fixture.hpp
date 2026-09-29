#pragma once

// Phase 06 deterministic retained-source workload. This is a benchmark fixture,
// not a new mathematical representation or a production persistence format.
#include "rfc/storage/native_series_codec.hpp"
#include "rfc/storage/native_defects.hpp"
#include "rfc/storage/native_accession.hpp"
#include "rfc/field/connection.hpp"
#include <array>
#include <fstream>
#include <sstream>

namespace rfc_temporal_product {
using namespace rfc;
using namespace rfc::storage;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
using Vec = numeric::RationalVector;
using Bytes = std::vector<std::byte>;
inline constexpr std::uint64_t source_namespace = 6006;
inline constexpr std::size_t last_step = 16;
inline constexpr std::array<const char*, last_step + 1> events{
    "base", "marker_flip", "orientation_repair", "unequal_share_repair", "balanced_local_maps",
    "partial_chart_change", "deformed_curvature", "metric_only", "sparse_innovations", "zero_field_birth",
    "activate_zero_occurrence", "tensor_support_only", "declaration_recode", "unused_direction_transport",
    "repair_deformation", "remove_zero_birth", "remove_cancelling_occurrences"};
struct Checks {
    std::size_t count{};
    void operator()(bool ok, const char* message) {
        ++count;
        if (!ok) throw std::runtime_error(std::string("product verification: ") + message);
    }
    template<class F> void rejects(F&& f, const char* message) {
        bool rejected = false;
        try { f(); } catch (const std::exception&) { rejected = true; }
        (*this)(rejected, message);
    }
};
inline StableCellKey cell(std::uint16_t grade, std::uint64_t key) { return {core::Grade{grade}, key}; }
inline StableOccurrenceKey occurrence(std::uint16_t grade, std::uint64_t key) {
    return {core::Grade{grade}, core::OccurrenceKey{key}};
}
inline std::uint64_t edge_key(std::size_t triangle, std::size_t edge) { return 100 + 4 * triangle + edge; }
inline std::uint64_t face_key(std::size_t triangle) { return 100 + triangle; }
inline std::uint64_t lower_key(std::size_t triangle, std::size_t edge, std::size_t leg) {
    return 1000 + 32 * triangle + 8 * edge + leg;
}
inline std::uint64_t upper_key(std::size_t triangle, std::size_t edge) { return 1000000 + 4 * triangle + edge; }
inline std::uint64_t stalk_key(StableCellKey c) { return 10000 + 1000000 * std::uint64_t(c.grade.value) + c.key; }
inline bool born_present(std::size_t step) { return step >= 9 && step < 15; }
inline bool deformed(std::size_t step) { return step >= 6 && step < 14; }
inline std::size_t triangle_count(std::size_t base, std::size_t step) { return base + (born_present(step) ? 1u : 0u); }
inline Q scale(std::size_t step, std::size_t triangle, std::size_t edge) {
    if (triangle || edge || step < 2) return Q{1};
    return step < 3 ? Q{-1} : Q{-7, 3};
}
inline M imbalance() { return M::from_rows({{Q{1,5}, Q{2,9}}, {Q{-3,11}, Q{0}}}); }
inline M upper_map() { return M::from_rows({{Q{1}, Q{2,5}}, {Q{3,7}, Q{1}}}); }
inline M paired_map() { return M::from_rows({{Q{2}, Q{1,3}}, {Q{0}, Q{1}}}); }
inline M latent_J(std::size_t step) {
    return step == 13 ? M::from_rows({{Q{1}, Q{17,19}}, {Q{0}, Q{1}}}) : M::identity(2);
}
inline Vec latent_x(std::size_t step) { return step < 8 ? Vec{Q{1}, Q{2}} : Vec{Q{5,7}, Q{0}}; }
inline Q large_innovation() { return Q{"115792089237316195423570985008687907853269984665640564039457584007913129639939/37"}; }
inline core::RelationSignature signature(bool recoded) {
    using composite::AspectRole;
    composite::AspectSchema aspects;
    if (recoded) {
        aspects.add("marker", AspectRole::Custom);
        aspects.add("orientation", AspectRole::Orientation);
        aspects.add("introduced", AspectRole::Custom, true);
        aspects.add("exists", AspectRole::Existence, true);
    } else {
        aspects.add("exists", AspectRole::Existence, true);
        aspects.add("orientation", AspectRole::Orientation);
        aspects.add("marker", AspectRole::Custom);
    }
    core::RelationSignature sig(aspects);
    sig.add_type("edge", core::BoundaryPolicy::DeclaredExact, 3, 5);
    sig.add_type("closed-triangle", core::BoundaryPolicy::DeclaredExact, 3, 3);
    sig.add_type("empty-assertion", core::BoundaryPolicy::DeclaredExact, 0, 0);
    return sig;
}
inline SourceDeclarationMigration migration() {
    return {signature(false), signature(true),
        {{"exists", "exists"}, {"orientation", "orientation"}, {"marker", "marker"}},
        {{"edge", "edge"}, {"closed-triangle", "closed-triangle"}, {"empty-assertion", "empty-assertion"}},
        {{"introduced", false, {}}}};
}
struct LegSpec {
    StableOccurrenceKey key;
    StableCellKey upper, lower;
    bool active{};
    Q coefficient{0};
    bool marker{};
    M realized{2,2};
    bool explicit_map{};
};
inline std::vector<LegSpec> legs(std::size_t base, std::size_t step) {
    std::vector<LegSpec> out;
    for (std::size_t t = 0; t < triangle_count(base, step); ++t) {
        const std::array<std::uint64_t,3> vs{1, 10 + 4*t, 11 + 4*t};
        for (std::size_t j = 0; j < 3; ++j) {
            const Q s = scale(step,t,j);
            for (std::size_t k = 0; k < 5; ++k) {
                if (step == 16 && t == 0 && j == 0 && (k == 2 || k == 3)) continue;
                const auto target = k == 1 ? vs[(j+1)%3] : k == 4 ? 12+4*t : vs[j];
                const Q c = k == 0 ? Q(-s) : k == 1 ? s : k == 2 ? Q{2,7} : k == 3 ? Q{-2,7} : Q{0};
                LegSpec leg{occurrence(1,lower_key(t,j,k)),cell(1,edge_key(t,j)),cell(0,target),
                    k != 4 || (step >= 10 && t == 0 && j == 0), c,
                    ((t+j+k)%3 == 0) != (step >= 1 && k == 0 && j == 0 && t%5 == 0), M::identity(2).scale(c), false};
                if (step >= 4 && t == 0 && j == 0 && (k == 2 || k == 3)) {
                    leg.realized = paired_map().scale(c); leg.explicit_map = true;
                }
                if (deformed(step) && t == 0 && j == 0 && k == 0) {
                    leg.realized = leg.realized.add(imbalance()); leg.explicit_map = true;
                }
                out.push_back(std::move(leg));
            }
            // Common right factor preserves complete flat assembly. It does
            // not commute with the lower paired map or the later deformation.
            const bool explicit_upper = step >= 4 && t == 0;
            out.push_back({occurrence(2,upper_key(t,j)),cell(2,face_key(t)),cell(1,edge_key(t,j)),true,
                Q{1}/s, false, (explicit_upper?upper_map():M::identity(2)).scale(Q{1}/s), explicit_upper});
        }
    }
    return out;
}
inline core::RelationalComplex source(std::size_t n, std::size_t step) {
    auto sig = signature(step >= 12);
    core::RelationalComplexBuilder b(sig); b.declare_grade(core::Grade{3});
    b.declare_witness_identity(1);
    const auto spec = legs(n,step);
    std::map<StableCellKey,std::vector<const LegSpec*>> fibers;
    for(const auto& leg:spec)fibers[leg.upper].push_back(&leg);
    std::map<std::uint64_t,std::uint32_t> edges;
    for (std::size_t t = 0; t < triangle_count(n,step); ++t) {
        for (std::size_t j = 0; j < 3; ++j) {
            std::vector<core::BoundaryOccurrenceInput> inputs;
            for (const auto* entry : fibers.at(cell(1,edge_key(t,j)))) {
                const auto& leg=*entry;
                std::vector<core::AspectValue> tags{{sig.aspects().id("marker"),leg.marker}};
                if (step >= 12) tags.push_back({sig.aspects().id("introduced"),false});
                inputs.push_back({leg.lower.key,leg.active,leg.coefficient < 0,
                    leg.coefficient < 0 ? Q(-leg.coefficient) : leg.coefficient,std::move(tags),leg.key.key});
            }
            edges[edge_key(t,j)] = b.add_grade_one(core::RelationKey{edge_key(t,j)},sig.type_id("edge"),inputs).index;
        }
        std::vector<core::HigherIncidenceInput> inputs;
        for (std::size_t j = 0; j < 3; ++j) {
            const Q c = Q{1}/scale(step,t,j);
            std::vector<core::AspectValue> tags{{sig.aspects().id("marker"),false}};
            if (step >= 12) tags.push_back({sig.aspects().id("introduced"),false});
            inputs.push_back({edges.at(edge_key(t,j)),true,c<0,c<0?Q(-c):c,std::move(tags),core::OccurrenceKey{upper_key(t,j)}});
        }
        b.add_higher(core::Grade{2},core::RelationKey{face_key(t)},sig.type_id("closed-triangle"),inputs);
    }
    b.add_grade_one(core::RelationKey{50},sig.type_id("empty-assertion"),{});
    return b.freeze();
}
inline numeric::SparseRationalTensor vector_tensor(const Vec& values) {
    std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i=0;i<values.size();++i) if(values[i]!=0) entries.push_back({i,values[i]});
    return {numeric::IndexDomain::linear(values.size()),numeric::StructuralSupport::full(values.size()),
        numeric::SparseRationalVector(values.size(),entries)};
}
inline TemporalTensor field(std::uint64_t key,std::uint64_t carrier,const Vec& xs) {
    TemporalTensor t;t.key=key;t.role=TemporalTensorRole::Field;t.output=carrier;t.tensor=vector_tensor(xs);return t;
}
inline TemporalTensor matrix_tensor(std::uint64_t key,TemporalTensorRole role,std::uint64_t in,std::uint64_t out,const M& m) {
    TemporalTensor t;t.key=key;t.role=role;t.input=in;t.output=out;t.tensor=temporal_matrix_tensor(m);return t;
}
inline composite::DeclarationId declaration_id(std::uint64_t key,std::size_t step) {return {606,key,step>=12?2u:1u};}
inline TemporalDeclarationState declarations(std::size_t step) {
    using D = composite::Declaration;using V = composite::DeclaredValue;
    const auto b=declaration_id(1,step),p=declaration_id(2,step),q=declaration_id(3,step),v=declaration_id(4,step);
    TemporalDeclarationState s;s.namespace_key=607;
    s.registry=composite::DeclarationRegistry({D::boolean(b),D::product(p,{{1,b,0},{2,b,0},{3,b,0}}),
        D::rational(q),D::product(v,{{1,q,0},{2,b,0}})});
    s.source_layouts={{1,p,{{1,"exists"},{2,"orientation"},{3,"marker"}}}};
    s.programs={{1,1,composite::compile_active_orientation_query(composite::AspectId{0},composite::AspectId{1},false),2}};
    s.values={{1,v,0,0,cell(0,1),{},V::product({V::rational(Q{3,5}),V::boolean(true)})}};
    s.code_types={{2,b},{3,p}};
    return s;
}
inline std::vector<composite::DeclarationMap> declaration_maps() {
    std::vector<composite::DeclarationMap> out;
    for(std::uint64_t k=1;k<=4;++k)out.push_back({declaration_id(k,0),declaration_id(k,12),composite::DeclarationMapKind::Recode,{}});
    return out;
}
inline std::vector<LocalBlockDefinition> blocks(std::size_t n,std::size_t step) {
    LocalBlockDefinition a,b,p;a.block_key=1;b.block_key=2;p.block_key=3;p.kind=LocalBlockKind::PartialChart;
    a.cells={cell(0,1),cell(1,50)};b.cells={cell(0,1)};
    for(std::size_t t=0;t<triangle_count(n,step);++t) {
        auto& selected=t%2?a:b;
        for(auto id:{10+4*t,11+4*t,12+4*t})selected.cells.push_back(cell(0,id));
        for(std::size_t j=0;j<3;++j)selected.cells.push_back(cell(1,edge_key(t,j)));
        selected.cells.push_back(cell(2,face_key(t)));
    }
    p.cells={cell(0,1),cell(1,100),cell(2,100)};
    p.occurrences={occurrence(2,upper_key(0,0)),occurrence(1,lower_key(0,0,0))};
    if(step<16)p.occurrences.push_back(occurrence(1,lower_key(0,0,step>=5?3:2)));
    for(auto* x:{&a,&b,&p})canonicalize_local_block(*x);
    return {a,b,p};
}
inline RelationalCodeState codes(const TemporalEndpoint& e,std::size_t step) {
    RelationalCodeState c;c.namespace_key=608;
    c.types={{1,RelationalCodeTypeKind::Rational,{0,1},{}},{2,RelationalCodeTypeKind::Finite,{0,1},{}},
        relational_aspect_code_type(e.source,3,2,{{1,e.source.signature().existence_aspect()},
        {2,e.source.signature().orientation_aspect()},{3,e.source.signature().aspects().id("marker")}})};
    c.fields={1,2};
    RelationalCodeOperation id;id.key=1;id.input=id.output=1;c.operations={id};
    c.sections={{1,1,1,1,cell(0,1),{},false,RelationalCodeValue{vector_tensor({1,2})}},
        {2,1,1,2,cell(0,1),{},false,RelationalCodeValue{vector_tensor({deformed(step)?Q{3}:Q{1},Q{2}})}}};
    c.sections.push_back(relational_aspect_code_section(3,2,3,occurrence(1,lower_key(0,0,0))));
    c.interfaces={{1,1,2,1,1}};return c;
}
inline TemporalEndpoint endpoint(std::size_t n,std::size_t step) {
    if(n<2 || n>1024 || step>last_step)throw std::invalid_argument("product fixture requires 2..1024 triangles, steps 0..16");
    TemporalEndpoint e(source(n,step),step);e.local_blocks=blocks(n,step);
    for(auto& block:e.local_blocks)if(block.kind==LocalBlockKind::StrictSubcomplex)
        block=make_strict_local_block(e.source,block.block_key,strict_block_mask(e.source,block));
    const auto count=e.source.grade(core::Grade{1}).cell_count();
    e.carriers={{1,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(count)},
        {2,TemporalCarrierKind::Explicit,{}, {},700,numeric::IndexDomain::linear(2)},
        {3,TemporalCarrierKind::Explicit,{}, {},701,numeric::IndexDomain::linear(1)}};
    Vec xs(count,Q{0});
    const auto& ids=e.source.grade(core::Grade{1}).relation_identities;
    for(std::size_t i=0;i<count;++i) {
        const auto id=ids[i].value;
        if(id>=100 && id<edge_key(n,0)) {xs[i]=Q{static_cast<long>(id%7),static_cast<unsigned long>(id%3+1)};xs[i].canonicalize();}
        if(step>=8 && id==100)xs[i]+=large_innovation();
    }
    e.tensors={field(10,1,xs),field(20,2,latent_x(step)),
        matrix_tensor(21,TemporalTensorRole::Metric,2,2,step>=7?M::diagonal({Q{2},Q{3}}):M::identity(2)),
        matrix_tensor(22,TemporalTensorRole::Accession,2,3,M::from_rows({{Q{1},Q{2}}})),
        matrix_tensor(23,TemporalTensorRole::Metric,3,3,M::identity(1))};
    for(std::size_t k=0;k<e.source.grade_count();++k) {
        const core::Grade g{static_cast<std::uint16_t>(k)};
        for(std::size_t i=0;i<e.source.grade(g).cell_count();++i) {
            const auto c=stable_cell_key(e.source,{g,static_cast<std::uint32_t>(i)});
            e.carriers.push_back({stalk_key(c),TemporalCarrierKind::CellStalk,g,c,stalk_key(c)+10000000,numeric::IndexDomain::linear(2)});
        }
    }
    for(const auto& spec:legs(n,step))if(spec.explicit_map) {
        auto t=matrix_tensor(1000000000+spec.key.key.value,TemporalTensorRole::RealizedIncidenceMap,
            stalk_key(spec.upper),stalk_key(spec.lower),spec.realized);
        t.owner.kind=TemporalOwnerKind::Occurrence;t.owner.occurrence=spec.key;
        if(step>=11 && spec.key==occurrence(1,lower_key(0,0,2))) {
            // This changes support, not coefficients: the lower-left zero is now declared absent.
            t.tensor=numeric::SparseRationalTensor(numeric::IndexDomain({2,2}),numeric::StructuralSupport::sparse(4,{0,1,3}),t.tensor.values());
        }
        e.tensors.push_back(std::move(t));
    }
    e.codes=codes(e,step);e.declarations=declarations(step);
    validate_temporal_endpoint(e);return e;
}
inline void verify_declared_program(Checks& check,std::size_t n,std::size_t step,const TemporalEndpoint& e) {
    const auto bits=execute_declared_source_program(e.source,*e.declarations,1,core::Grade{1});
    const auto& ids=e.source.grade(core::Grade{1}).occurrence_identities;
    check(bits.size()==ids.size(),"declared program retains full finite occurrence domain");
    std::map<std::uint64_t,bool> expected;
    for(const auto& leg:legs(n,step))if(leg.key.upper_grade.value==1)
        expected.emplace(leg.key.key.value,leg.active && leg.coefficient>=0);
    for(std::size_t i=0;i<ids.size();++i)check(bits.get(i)==expected.at(ids[i].value),"declared program survives signature/factor binding and exact replay");
}
inline TemporalSegment segment(const TemporalEndpoint& a,const TemporalEndpoint& b) {
    std::optional<SourceDeclarationMigration> sm;
    std::optional<TemporalDeclarationUpdate> dm;
    if(b.sequence==12) {sm=migration();dm=make_temporal_declaration_update(a.declarations,b.declarations,declaration_maps());}
    return make_temporal_segment(a,b,b.sequence==13?std::vector<TemporalCorrespondence>{{2,latent_J(13)}}:std::vector<TemporalCorrespondence>{},
        {},RelationalCodeEncoding::Automatic,std::move(sm),std::move(dm));
}
inline NativeDefectRealization realization(const TemporalEndpoint& e) {
    NativeDefectRealization r;r.kind=NativeDefectRealizationKind::LinearStalks;
    for(const auto& c:e.carriers) { if(c.kind==TemporalCarrierKind::CellStalk) r.stalk_carriers.push_back(c.key); }
    return r;
}
inline std::vector<NativeDefectQuery> queries() {
    NativeDefectQuery all;all.key=1;all.upper_grade=core::Grade{2};
    auto partial=all;partial.key=2;partial.selection=NativeDefectSelection::LocalBlock;partial.block=3;
    auto left=all;left.key=3;left.selection=NativeDefectSelection::LocalBlock;left.block=1;
    auto right=left;right.key=4;right.block=2;
    return {all,partial,left,right};
}
inline std::vector<RelationalCodeObservationQuery> observations() {
    return {{1,RelationalCodeObservationKind::Interface,0,{},{}},
        {2,RelationalCodeObservationKind::Section,0,2,{}},
        {3,RelationalCodeObservationKind::Application,1,1,{}}};
}
inline void same_endpoint(Checks& check,const TemporalEndpoint& a,const TemporalEndpoint& b) {
    check(encode_rfcx(a.source).bytes==encode_rfcx(b.source).bytes,"complete source bytes");
    check(encode_temporal_endpoint(a)==encode_temporal_endpoint(b),"complete endpoint bytes");
}
inline M multiply_reference(const M& a,const M& b) {
    M out(a.rows(),b.cols());
    for(std::size_t i=0;i<a.rows();++i)for(std::size_t j=0;j<b.cols();++j) {
        Q x=0;for(std::size_t k=0;k<a.cols();++k)x+=a.get(i,k)*b.get(k,j);out.set(i,j,x);
    }
    return out;
}
inline void verify_routes(Checks& check,std::size_t n,std::size_t step,const NativeDefectView& actual) {
    const auto spec=legs(n,step);
    std::map<StableCellKey,std::vector<const LegSpec*>> outgoing;
    for(const auto& leg:spec)outgoing[leg.upper].push_back(&leg);
    std::map<NativeDefectBlockKey,M> sums;
    std::set<NativeRouteKey> expected;
    for(const auto& upper:spec)if(upper.key.upper_grade.value==2)for(const auto* lower:outgoing.at(upper.lower)) {
        const NativeRouteKey key{upper.key,lower->key};expected.insert(key);
        const M value=upper.active&&lower->active?multiply_reference(lower->realized,upper.realized):M(2,2);
        check(actual.contains(key),"retained route absent");const auto& r=actual.route(key);
        check(r.upper==upper.upper && r.middle==upper.lower && r.lower==lower->lower,"route identity/attachment");
        check(r.active==(upper.active&&lower->active),"route activity");check(r.contribution==value,"independent route product");
        const NativeDefectBlockKey bk{lower->lower,upper.upper};
        if(!sums.contains(bk)) sums.emplace(bk,M(2,2));
        sums.at(bk)=sums.at(bk).add(value);
    }
    const auto keys=actual.route_keys();check(std::set<NativeRouteKey>(keys.begin(),keys.end())==expected,"complete retained route support");
    const auto& all=actual.certificate(1);check(all.sums==sums,"independent complete sums including retained zeros");
    check(all.complete(),"full coverage");check(all.curved_in_scope()==deformed(step),"deformation event status");
    check(!actual.certificate(2).complete(),"partial chart completeness");check(!actual.certificate(2).curved_in_scope(),"partial is not curvature");
    check(!actual.certificate(3).complete() && !actual.certificate(4).complete(),"strict blocks not automatically ambient-complete");
    std::set<NativeRouteKey> glued=actual.certificate(3).selected_routes;
    const auto& rhs=actual.certificate(4).selected_routes;glued.insert(rhs.begin(),rhs.end());
    check(glued==expected,"glued strict route coverage");
    auto assembled=actual.certificate(3).sums;
    for(const auto& [bk,m]:actual.certificate(4).sums) {
        if(!assembled.contains(bk)) assembled.emplace(bk,M(2,2));
        assembled.at(bk)=assembled.at(bk).add(m);
    }
    check(assembled==sums,"local sums assemble exactly");
}
inline void verify_word(Checks& check,std::size_t step,const NativeDefectView& old,const NativeDefectView& next) {
    if(step==16)return; // The selected lower occurrence is deliberately removed.
    const NativeRouteKey key{occurrence(2,upper_key(0,0)),occurrence(1,lower_key(0,0,2))};
    const auto word=old.word_defect_to(next,key);
    check(word.direct==word.expanded,"complete finite temporal word on the retained local route");
    if(step==4) {
        check(upper_map().multiply(paired_map())!=paired_map().multiply(upper_map()),"fixture maps genuinely noncommute");
        const auto old_A=M::identity(2).scale(Q{-3,7});
        const auto old_B=M::identity(2).scale(Q{2,7});
        const auto linear=old_B.multiply(word.position_defects[0]).add(word.position_defects[1].multiply(old_A));
        check(word.direct!=linear,"finite interaction cannot be dropped as a first-order approximation");
    }
}
inline void verify_fresh(Checks& check,const NativeDefectStore& actual,const TemporalEndpoint& e) {
    NativeDefectStore fresh(e,source_namespace,realization(e),queries(),{},observations());
    const auto a=actual.pin(),b=fresh.pin();check(a.route_keys()==b.route_keys(),"fresh index route support");
    for(const auto k:b.route_keys())check(a.route(k).contribution==b.route(k).contribution,"fresh index route matrix");
    for(const auto& q:queries()) {
        check(a.certificate(q.key).selected_routes==b.certificate(q.key).selected_routes,"fresh chart membership");
        check(a.certificate(q.key).sums==b.certificate(q.key).sums,"fresh chart sums");
    }
    for(const auto id:std::vector<RelationalCodeIdentity>{{1,1},{2,3},{3,1},{3,2}}) {
        check(actual.observation(id)->bytes==fresh.observation(id)->bytes,"fresh observation bytes");
        check(actual.observation(id)->compatible==fresh.observation(id)->compatible,"fresh interface compatibility");
    }
    check(actual.observation({1,1})->compatible==std::optional<bool>{!deformed(e.sequence)},"interface obstruction retained");
}
inline std::size_t occurrence_count(const TemporalEndpoint& e) {
    std::size_t count=0;for(std::size_t k=1;k<e.source.grade_count();++k)count+=e.source.grade(core::Grade{static_cast<std::uint16_t>(k)}).incidence_count();return count;
}
inline void save(const std::filesystem::path& p,std::span<const std::byte> bytes) {
    std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.close();
    if(!out)throw std::runtime_error("product output write failed");
}
inline void emit_routes(std::ostream& out,const NativeDefectView& v) {
    for(const auto key:v.route_keys()) {
        const auto& r=v.route(key);out<<v.sequence()<<','<<key.upper.key.value<<','<<key.lower.key.value<<','
            <<r.upper.key<<','<<r.middle.key<<','<<r.lower.key<<','<<r.active;
        for(std::size_t i=0;i<2;++i) { for(std::size_t j=0;j<2;++j) out<<','<<r.contribution.get(i,j).get_str(); }
        out<<'\n';
    }
}
} // namespace rfc_temporal_product
