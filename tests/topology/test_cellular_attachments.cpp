#include "../../examples/cellular_attachments/model.hpp"
#include "rfc/storage/rfcx.hpp"
#include <iostream>

using namespace cellular_example;
using Policy=rfc::local::AlgebraicChainPolicy;
using M=rfc::numeric::ExactMatrix;
static std::size_t checks{};
static void require(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); ++checks; }
template<class Error,class F> static void rejects(F action,const char* message) {
    bool caught=false; try {action();} catch (const Error&) {caught=true;} require(caught,message);
}
template<class T> concept TemporarySteps=requires(T value) {std::move(value).steps();};
static_assert(!std::default_initializable<Prepared::Path> && !TemporarySteps<Prepared::Path>);

static void routes(const Prepared& prepared) {
    const auto& source=prepared.bundle().source(); const auto d=prepared.bundle().field().degree();
    for (std::size_t k=2;k<source.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)},lower=G{static_cast<std::uint16_t>(k-2)};
        const auto offsets=prepared.bundle().offsets(lower),upper=prepared.bundle().offsets(grade);
        const auto& expected=prepared.realization().rational_chain_defect(grade);
        for (std::size_t cell=0;cell<source.grade(grade).cell_count();++cell) {
            M result(expected.rows(),(upper[cell+1]-upper[cell])*d); const auto trace=prepared.chain_routes(grade,cell);
            for (const auto& route:trace) {
                const auto w=route.upper_active && route.lower_active?Q(route.upper_coefficient*route.lower_coefficient):Q{0};
                require(route.realized.rational_action()==route.underlying.rational_action().scale(w),"retained two-step route coefficient used once");
                require(route.underlying.source().cell()==rfc::core::CellId{grade,static_cast<std::uint32_t>(cell)},"route source is actual top stalk");
                require(route.underlying.target().cell()==route.target,"route target is actual lower stalk");
                for (const auto& [key,q]:route.realized.rational_action().entries()) {
                    const auto row=offsets[route.target.index]*d+key.first;
                    result.set(row,key.second,Q(result.get(row,key.second)+q));
                }
            }
            for (std::size_t i=0;i<result.rows();++i) for (std::size_t j=0;j<result.cols();++j)
                require(result.get(i,j)==expected.get(i,upper[cell]*d+j),"all local routes reconstruct the complete global defect");
        }
    }
}
static void model_checks(const Field& field) {
    const auto model=make(field); const auto image=rfc::storage::encode_rfcx(model.bundle.source()).bytes;
    const auto prepared=model.prepare(Policy::RequireFlat);
    require(rfc::storage::encode_rfcx(prepared.bundle().source()).bytes==image,"all original source bytes and vertices preserved");
    require(prepared.bundle().source().grade(G{0}).cell_count()==4,"inactive foreign vertex retained without collapse");
    require(prepared.realization().flat(),"all grade-two through grade-four laws hold");
    require(prepared.realization().homology_dimensions_over_field()==std::vector<std::size_t>({4,0,0,0,0}),"complete higher algebraic homology");
    for (const auto& face:prepared.faces()) {
        require(face.holonomy_defect.rational_action().nonzeros()==0,"flat full loop action");
        require(face.boundary_defect.rational_action().nonzeros()==0,"flat chart realization");
        for (const auto& term:face.occurrences) {
            require(term.prefix_before.source().cell().index==face.base_vertex,"prefix starts in declared actual base stalk");
            require(term.realized.rational_action()==term.underlying.rational_action().scale(term.active?term.coefficient:Q{0}),"face weight applied once");
            if (!term.active) {
                require(term.underlying.rational_action().nonzeros()!=0 && term.realized.rational_action().nonzeros()==0,"inactive underlying map retained separately");
                require(term.prefix_before.rational_action()==term.prefix_after.rational_action(),"inactive step does not advance prefix");
            }
        }
    }
    routes(prepared);
    const auto first=prepared.path(0,{{0,true},{1,true}}),last=prepared.path(2,{{2,true}});
    const auto loop=first.then(last);
    require(loop.source()==0 && loop.target()==0 && loop.steps().size()==3,"owned multi-vertex path endpoints");
    const auto expected=model.edges[0].forward.then(model.edges[1].forward).then(model.edges[2].forward);
    require(loop.transport().rational_action()==expected.rational_action(),"complete chronological path action");
    const auto undone=first.then(first.inverse());
    require(undone.steps().size()==4,"inverse cancellation does not erase operation history");
    require(undone.transport().rational_action()==M::identity(2*field.degree()),"full inverse path action");
    const auto prefix=loop.prefix(1); require(prefix.target()==1 && prefix.steps().size()==1,"prefix retains no future path suffix");
    const auto copied=prepared; const auto empty=copied.path(0,{});
    const auto receipt=prepared.replay_face_moves(empty,{{0,0,false,true,0}});
    require(receipt.full_transport_preserved() && receipt.every_used_loop_trivial(),"trivial full holonomy preserves certified face insertion");
    require(receipt.before().steps().empty() && receipt.after().steps().size()==3,"source move receipt keeps both actual paths");
    const auto rotated=prepared.replay_face_moves(prepared.path(1,{}),{{0,1,false,true,0}});
    require(rotated.after().source()==1 && rotated.after().target()==1 && rotated.full_transport_preserved(),"cyclic word chart changes actual basepoint");
    const auto reverse=prepared.replay_face_moves(empty,{{0,0,true,true,0}});
    require(reverse.full_transport_preserved(),"inverse attaching representative");
    const auto removed=prepared.replay_face_moves(receipt.after(),{{0,0,false,false,0}});
    require(removed.after().steps().empty(),"certified face removal");
    const auto detached=[&]{const auto ephemeral=model.prepare(Policy::RequireFlat); return ephemeral.path(0,{{0,true},{1,true}});}();
    require(detached.target()==2 && detached.transport().rational_action()==first.transport().rational_action(),"path pins destroyed wrapper's source and maps");
    const auto foreign=model.prepare(Policy::RequireFlat);
    rejects<std::invalid_argument>([&]{(void)first.then(foreign.path(2,{}));},"equal carrier bytes do not identify connection owners");
    rejects<std::invalid_argument>([&]{(void)foreign.replay_face_moves(first,{});},"source move must bind actual connection owner");
    rejects<std::invalid_argument>([&]{(void)prepared.path(0,{{1,true}});},"nonincident path rejection");
    rejects<std::out_of_range>([&]{(void)prepared.path(99,{});},"unknown path carrier rejection");
    rejects<std::out_of_range>([&]{(void)loop.prefix(4);},"prefix bound");
    rejects<std::invalid_argument>([&]{(void)prepared.replay_face_moves(empty,{{0,1,false,true,0}});},"face move actual basepoint must match");
    rejects<std::invalid_argument>([&]{(void)prepared.replay_face_moves(empty,{{0,0,false,false,0}});},"source certificate requires actual removed word");
    const auto Z=model.faces[1].chart;
    rejects<std::invalid_argument>([&]{(void)Z.add(model.faces[0].chart);},"map addition must bind actual equal carriers");
    const auto copy_field=Field::declare(field.minimal_polynomial(),field.certificate());
    rejects<std::invalid_argument>([&]{(void)Z.scale(copy_field.scalar(Q{0}));},"foreign scalar zero does not bypass field owner");
    const auto cancelled=Z.add(Z.scale(field.scalar(Q{-1})));
    require(cancelled.rational_action().nonzeros()==0,"complete module map additive inverse");
    const auto weighted=make(field,false,false,false,Q{2}).prepare(Policy::RequireFlat);
    routes(weighted);
    require(weighted.realization().flat(),"weighted edge normalization retains source weights and exact telescoping");
    const auto repeated=make(field,false,false,false,Q{2},false,2).prepare(Policy::RequireFlat);
    require(repeated.faces()[0].occurrences.size()==7,"repeated attaching occurrences remain individually retained"); routes(repeated);
    const auto reverse_loop=make(field,false,true,false,Q{2},false,2,true).prepare(Policy::DeclaredDeformation);
    const auto reverse_action=reverse_loop.faces()[0].holonomy.rational_action();
    require(reverse_action.get(field.degree(),field.degree())==Q{1,4},"reverse repeated loop uses complete inverse transport"); routes(reverse_loop);
}
static void branching(const Field& field) {
    rfc::core::RelationSignature signature;
    const auto branch=signature.add_branch_type("retained_branch",2,0);
    const auto edge=signature.add_type("pairwise_edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto higher=signature.add_type("declared_attachment",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(branch,{{100,true,true,true,1},{101,true,false,false,Q{1,3}},
                                 {102,true,false,false,Q{1,3}},{103,true,false,false,Q{1,3}}});
    for (const auto target:{101u,102u,103u})
        builder.add_grade_one(edge,{{100,true,true,false,1},{target,true,false,false,1}});
    builder.add_canonical_higher(G{2},branch,{0,1,2,3},0);
    builder.add_higher(G{2},higher,{{0,true,true,false,1},{1,true,false,false,Q{1,3}},
        {2,true,false,false,Q{1,3}},{3,true,false,false,Q{1,3}},
        {1,true,false,false,0},{0,false,false,false,7}});
    const auto source=builder.freeze(); const auto image=rfc::storage::encode_rfcx(source).bytes;
    std::vector<std::vector<std::size_t>> ranks{{2,2,2,2},{2,2,2,2},{2,2}};
    const auto bundle=Bundle::declare(source,field,std::move(ranks));
    const auto one=field.scalar(Q{1}),zero=field.scalar(Q{0});
    std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
    for (std::uint16_t k=1;k<3;++k) {
        const auto& arena=source.grade(G{k});
        for (std::size_t slot=0;slot<arena.incidence_count();++slot)
            maps.push_back({G{k},slot,Map::declare(bundle.stalk({G{k},arena.upper_index[slot]}),
                bundle.stalk({G{static_cast<std::uint16_t>(k-1)},arena.lower_index[slot]}),{one,zero,zero,one})});
    }
    const auto prepared=Prepared::declare(bundle,{}, {},maps,Policy::RequireFlat);
    require(prepared.faces().empty() && prepared.realization().flat(),"nonpairwise higher attachments use complete declared maps");
    const auto relation=rfc::topology::higher_attaching_relation(source,G{2},0);
    require(relation.has_branching_participant(),"branching attachment remains a branching relation");
    require(rfc::storage::encode_rfcx(prepared.bundle().source()).bytes==image,"branching source canonical bytes preserved"); routes(prepared);
    const auto& zero_map=prepared.realization().occurrence_map(G{2},8);
    require(zero_map.rational_action().nonzeros()!=0 && source.incidence_coefficient(G{2},8)==0,
        "supported zero retains its complete nonzero underlying map");
    rejects<std::invalid_argument>([&]{(void)prepared.path(0,{{0,true}});},"branching relation cannot become an implicit pairwise path");
    rejects<std::invalid_argument>([&]{(void)prepared.path(0,{{1,true}});},"pairwise path needs a declared edge connection");
}
static void curved_and_partial(const Field& field) {
    const auto model=make(field,false,true); const auto deformed=model.prepare(Policy::DeclaredDeformation);
    require(!deformed.realization().flat(),"curved full chart exposes boundary defect"); routes(deformed);
    rejects<std::invalid_argument>([&]{(void)model.prepare(Policy::RequireFlat);},"strict full chart refuses nontrivial loop action");
    rejects<std::logic_error>([&]{(void)deformed.realization().homology_dimensions_over_field();},"curved homology refusal");
    const auto partial=make(field,false,true,true).prepare(Policy::RequireFlat);
    require(partial.realization().flat(),"restricted chart can land in invariant directions");
    require(partial.faces()[0].holonomy_defect.rational_action().nonzeros()!=0 &&
        partial.faces()[0].boundary_defect.rational_action().nonzeros()==0,"full transport curvature is separate from chart defect");
    const auto receipt=partial.replay_face_moves(partial.path(0,{}),{{0,0,false,true,0}});
    require(!receipt.full_transport_preserved() && !receipt.every_used_loop_trivial(),"structural face move is separate from represented transport equality");
    const auto cancelled=partial.replay_face_moves(partial.path(0,{}),{{0,0,false,true,0},{0,0,false,false,0}});
    require(cancelled.full_transport_preserved() && !cancelled.every_used_loop_trivial(),"equal final actions do not certify trivial individual relators");
    const auto broken=make(field,true,false,false,Q{1},true);
    rejects<std::invalid_argument>([&]{(void)broken.prepare(Policy::RequireFlat);},"grade-four defect cannot hide behind lower flat interfaces");
    const auto retained=broken.prepare(Policy::DeclaredDeformation);
    require(retained.realization().rational_chain_defect(G{2}).nonzeros()==0 &&
        retained.realization().rational_chain_defect(G{3}).nonzeros()==0 &&
        retained.realization().rational_chain_defect(G{4}).nonzeros()!=0,"full higher chain family retained separately");
    routes(retained);
}
static void admissions(const Field& field) {
    const auto model=make(field); auto missing=model.explicit_maps; missing.erase(missing.begin());
    rejects<std::invalid_argument>([&]{(void)Prepared::declare(model.bundle,model.edges,model.faces,missing,Policy::RequireFlat);},"missing inactive map rejection");
    auto edges=model.edges; edges[1]=edges[0];
    rejects<std::invalid_argument>([&]{(void)Prepared::declare(model.bundle,edges,model.faces,model.explicit_maps,Policy::RequireFlat);},"duplicate connection rejection");
    auto faces=model.faces; faces.pop_back();
    rejects<std::invalid_argument>([&]{(void)Prepared::declare(model.bundle,model.edges,faces,model.explicit_maps,Policy::RequireFlat);},"all retained loop charts required");
    const auto duplicate=make(field);
    edges=model.edges; edges[0]=duplicate.edges[0];
    rejects<std::invalid_argument>([&]{(void)Prepared::declare(model.bundle,edges,model.faces,model.explicit_maps,Policy::RequireFlat);},"equal edge matrices do not identify stalk owners");
    auto limits=rfc::topology::CellularAttachmentLimits{}; limits.max_trace_coordinates=1;
    rejects<std::length_error>([&]{(void)model.prepare(Policy::RequireFlat,limits);},"complete trace coordinate admission");
    limits={}; limits.max_path_steps=2; const auto bounded=model.prepare(Policy::RequireFlat,limits);
    rejects<std::length_error>([&]{(void)bounded.path(0,{{0,true},{1,true},{2,true}});},"path word admission");
    rejects<std::length_error>([&]{(void)bounded.replay_face_moves(bounded.path(0,{}),{{0,0,false,true,0}});},"prospective face insertion checked before mutation");
    limits={}; limits.max_chain_routes=0; const auto no_routes=model.prepare(Policy::RequireFlat,limits);
    rejects<std::length_error>([&]{(void)no_routes.chain_routes(G{4},0);},"complete route count admission");
    limits={}; limits.max_chain_route_coordinates=1; const auto tiny=model.prepare(Policy::RequireFlat,limits);
    rejects<std::length_error>([&]{(void)tiny.chain_routes(G{2},0);},"complete route coordinate admission");
}
int main() {
    try {
        const auto rational=Field::declare({0,1},rfc::numeric::IrreducibleReduction{2});
        const auto algebraic=Field::declare({-2,0,0,1},rfc::numeric::EisensteinShift{2,0});
        for (const auto& field:{rational,algebraic}) {model_checks(field); curved_and_partial(field); branching(field); admissions(field);}
        std::cout<<"{\"checks\":"<<checks<<",\"passed\":true}\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
