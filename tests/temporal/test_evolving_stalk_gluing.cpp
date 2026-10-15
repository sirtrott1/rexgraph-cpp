#include "rfc/core/builder.hpp"
#include "rfc/learning/transport_family_gluing.hpp"
#include "rfc/local/section.hpp"
#include "rfc/temporal/operation_word.hpp"

#include <array>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// A finite chronological experiment in the native transport learning and
// gluing APIs. ExactMatrix stores supported terms. The only whole vectors in
// these APIs enumerate the actually represented 2D/3D stalk and parameter
// coordinates; the million-direction common carrier never becomes an
// amplitude array or a per-coordinate row index.
namespace {
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using S=numeric::SparseRationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Fit=learning::PreparedGradedTransportFit;
using Glue=learning::PreparedTransportFamilyGluing;
using Status=learning::TransportParameterRelationStatus;
constexpr core::Grade nodes{0},edges{1},faces{2};
constexpr std::size_t large_carrier=1000003,entry_budget=50000;
std::size_t checks{};

void check(bool yes,const char* why) {
    ++checks;if(!yes)throw std::runtime_error(why);
}
template<class F>void rejects(F&& fn,const char* why) {
    bool caught=false;try {fn();}catch(const std::exception&){caught=true;}
    check(caught,why);
}
bool same(const S& a,const S& b) {return a.size()==b.size() && a.entries()==b.entries();}
numeric::ExactActionLimits limits() {
    auto out=numeric::ExactActionLimits{};out.dimension=large_carrier;out.entries=entry_budget;return out;
}
core::Grade grade(std::size_t k){return {static_cast<std::uint16_t>(k)};}

core::RelationalComplex source() {
    core::RelationSignature signature;
    const auto role=signature.aspects().add("sensor boundary",composite::AspectRole::RelationRole,false);
    const auto provenance=signature.aspects().add("original observation",composite::AspectRole::Provenance,false);
    const auto type=signature.add_type("retained temporal boundary",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder b(signature);
    b.add_grade_one(core::RelationKey{1010},type,{
        {101,true,true,Q{1},{{role,true},{provenance,true}},core::OccurrenceKey{100}},
        {307,true,false,Q{1},{{provenance,true}},core::OccurrenceKey{101}},
        {101,true,false,Q{0},{{role,true}},core::OccurrenceKey{102}},
        {911,false,true,Q{1},{{provenance,true}},core::OccurrenceKey{103}}});
    b.add_grade_one(core::RelationKey{2020},type,{
        {307,true,true,Q{1},{{provenance,true}},core::OccurrenceKey{200}},
        {911,true,false,Q{1},{},core::OccurrenceKey{201}}});
    b.add_grade_one(core::RelationKey{3030},type,{
        {101,true,true,Q{1},{},core::OccurrenceKey{300}},
        {911,true,false,Q{1},{{role,true}},core::OccurrenceKey{301}}});
    b.add_higher(faces,core::RelationKey{4040},type,{
        {0,true,false,Q{1},{{role,true},{provenance,true}},core::OccurrenceKey{400}},
        {1,true,false,Q{1},{{provenance,true}},core::OccurrenceKey{401}},
        {2,true,true,Q{1},{},core::OccurrenceKey{402}}});
    return b.freeze();
}

void retained(const core::RelationalComplex& a,const core::RelationalComplex& b) {
    check(a.grade_count()==b.grade_count(),"temporal state lost retained grades");
    check(a.signature().aspects().size()==b.signature().aspects().size(),"temporal state lost aspect schema");
    for(std::size_t k=0;k<a.grade_count();++k) {
        const auto& x=a.grade(grade(k));const auto& y=b.grade(grade(k));
        check(x.identities==y.identities && x.relation_identities==y.relation_identities,"cell identities changed during learning");
        check(x.occurrence_identities==y.occurrence_identities,"occurrence identity changed during learning");
        check(x.lower_index==y.lower_index && x.upper_index==y.upper_index,"retained incidence changed during learning");
        check(x.relation_types==y.relation_types && x.cell_scales==y.cell_scales,"retained type/scale changed during learning");
        check(x.lattice_coefficient==y.lattice_coefficient,"retained composite coefficient changed during learning");
        for(std::size_t p=0;p<x.incidence_count();++p)
            for(std::size_t aspect=0;aspect<a.signature().aspects().size();++aspect)
                check(x.aspects.get(static_cast<composite::AspectId>(aspect),p)==
                    y.aspects.get(static_cast<composite::AspectId>(aspect),p),"retained Boolean occurrence aspect changed");
    }
}

Fit unknown(std::size_t dimension,Guard& guard) {
    const auto c=source();local::LinearStalkBundle stalks(c,dimension);
    local::LinearRelationalFiberSystem fibers(stalks);
    std::vector<learning::TransportCoordinate> variables;
    for(std::size_t k=1;k<c.grade_count();++k) {
        const auto g=grade(k);
        for(std::size_t p=0;p<c.grade(g).incidence_count();++p) {
            M map(dimension,dimension);
            if(g==faces) {
                for(std::size_t i=0;i<dimension;++i)for(std::size_t j=0;j<dimension;++j)
                    variables.push_back({p,i,j});
            } else for(std::size_t i=0;i<dimension;++i)map.set(i,i,c.incidence_coefficient(g,p));
            fibers.set_transport(g,p,std::move(map));
        }
    }
    auto observations=learning::PreparedTransportObservations::at_grade(
        local::OwnedLinearFiberSystem(fibers),faces,{},std::move(variables),guard);
    return Fit(std::move(observations),{},guard);
}

learning::TransportObservation reading(std::size_t dimension,std::size_t input_axis,const V& response) {
    if(response.size()!=dimension)throw std::logic_error("test observation dimension");
    V input(dimension),target(3*dimension);input.at(input_axis)=1;
    for(std::size_t i=0;i<dimension;++i)target[i]=response[i];
    // Observe only the first retained boundary relation. The native chain
    // constraints infer the responses on the remaining two relations.
    return {std::move(input),std::move(target),{1,0,0}};
}

learning::TransportFamilyInterface declaration(const std::vector<Fit>& members,
        std::size_t common_dimension=0,std::uint64_t revision=1) {
    learning::TransportFamilyInterface out{7001,revision,0,1,source(),{},{},{},{}};
    const auto count=out.common.grade_count();
    out.common_left.cells.resize(count);out.common_left.occurrences.resize(count);
    out.common_right.cells.resize(count);out.common_right.occurrences.resize(count);
    out.left_views.resize(count);out.right_views.resize(count);
    for(std::size_t k=0;k<count;++k) {
        const auto& arena=out.common.grade(grade(k));
        out.common_left.cells[k].resize(arena.cell_count());
        out.common_left.occurrences[k].resize(arena.incidence_count());
        std::iota(out.common_left.cells[k].begin(),out.common_left.cells[k].end(),0);
        std::iota(out.common_left.occurrences[k].begin(),out.common_left.occurrences[k].end(),0);
        out.common_right.cells[k]=out.common_left.cells[k];
        out.common_right.occurrences[k]=out.common_left.occurrences[k];
        for(std::size_t i=0;i<arena.cell_count();++i) {
            const core::CellId cell{grade(k),static_cast<std::uint32_t>(i)};
            const auto l=members[0].model().owner().fibers().stalks().dimension(cell);
            const auto r=members[1].model().owner().fibers().stalks().dimension(cell);
            const auto dimension=common_dimension?common_dimension:std::max(l,r);
            const auto view=[&](std::size_t n) {
                M a(dimension,n),reconstruction(n,dimension);
                for(std::size_t j=0;j<n;++j) {
                    const auto row=dimension>16 && j+1==n?dimension-1:j;
                    a.set(row,j,1);reconstruction.set(j,row,1);
                }
                return learning::TransportContextView{8000+100*k+i,std::move(a),std::move(reconstruction)};
            };
            out.left_views[k].push_back(view(l));out.right_views[k].push_back(view(r));
        }
    }
    return out;
}
Glue join(std::vector<Fit> members,Guard& guard,std::size_t common_dimension=0,std::uint64_t revision=1) {
    auto interface=declaration(members,common_dimension,revision);
    return Glue(std::move(members),{std::move(interface)},guard);
}

void valid_realization(const local::OwnedLinearFiberSystem& state,const M& expected) {
    const auto original=source();const auto& fibers=state.fibers();retained(original,fibers.complex());
    const auto b1=local::GradeTensorField(fibers,edges).matrix();
    const auto b2=local::GradeTensorField(fibers,faces).matrix();
    check(b1.multiply(b2).nonzeros()==0,"learned directional field breaks chain condition");
    for(std::size_t p=0;p<3;++p) {
        const auto& map=*fibers.transport_override(faces,p);
        check(map==expected.scale(original.incidence_coefficient(faces,p)),"learned occurrence response differs from exact global transport");
    }
    const auto existence=original.signature().existence_aspect();
    check(original.grade(edges).aspects.get(existence,2),"existing zero occurrence disappeared");
    check(!original.grade(edges).aspects.get(existence,3),"absent occurrence became existing");
    check(fibers.transport_override(edges,2)->nonzeros()==0 &&
          fibers.transport_override(edges,3)->nonzeros()==0,"zero/absent occurrence acquired amplitudes");
    const auto first_relation=fibers.relation_tensor(edges,0);
    check(first_relation.blocks().size()==3,"existing zero block was dropped or absent block included");
    check(std::any_of(first_relation.blocks().begin(),first_relation.blocks().end(),
        [](const auto& block){return block.incidence_slot==2 && block.map.nonzeros()==0;}),
        "present zero retained block missing");
}

struct Stage {
    const char* id;const char* time;const char* event;
    std::size_t left_free{},right_free{},joint_free{},observations{},equations{};
    bool consistent{};
};
Stage stage(const char* id,const char* time,const char* event,const Glue& glue) {
    return {id,time,event,glue.members()[0].kernel().cols(),glue.members()[1].kernel().cols(),
        glue.kernel().cols(),glue.members()[0].observations().size()+glue.members()[1].observations().size(),
        glue.equations().rows(),glue.consistent()};
}
void quoted_vector(std::ostream& out,const V& values) {
    out<<'[';for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<'"'<<values[i].get_str()<<'"';}out<<']';
}
void quoted_matrix(std::ostream& out,const M& map) {
    out<<'[';for(std::size_t i=0;i<map.rows();++i) {
        if(i)out<<',';
        out<<'[';for(std::size_t j=0;j<map.cols();++j) {
            if(j)out<<',';
            out<<'"'<<map.get(i,j).get_str()<<'"';
        }out<<']';
    }out<<']';
}

struct SectionCertificate {std::size_t cells{},zero_stalks{},supported_values{};};
SectionCertificate section_gluing(const M& learned) {
    const auto c=source();const local::RelationalLocalCarrier carrier(c);
    const auto a=carrier.closure({{edges,0},{edges,1}});
    const auto b=carrier.closure({{edges,1},{edges,2}});
    local::BasicSection<S> all;
    for(std::size_t k=0;k<2;++k)for(std::size_t i=0;i<c.grade(grade(k)).cell_count();++i)
        all.emplace(core::CellId{grade(k),static_cast<std::uint32_t>(i)},S(large_carrier));
    for(std::size_t i=0;i<3;++i) {
        const Q sign=i==2?Q{-1}:Q{1};
        all.at({edges,static_cast<std::uint32_t>(i)})=S(large_carrier,
            {{0,Q(sign*learned.get(0,0))},{large_carrier-1,Q(sign*learned.get(1,0))}});
    }
    const auto left=local::restrict_section(carrier,all,a);
    const auto right=local::restrict_section(carrier,all,b);
    const auto glued=local::glue_sections(carrier,left,a,right,b,local::OverlapCoverage::RequireComplete,same);
    check(glued.size()==6,"local tensor sections failed to recover all retained edge/node cells");
    for(const auto& [cell,value]:all)check(same(glued.at(cell),value),"exact tensor section gluing changed values");
    auto missing=right;missing.erase({nodes,1});
    rejects([&]{(void)local::glue_sections(carrier,left,a,missing,b,local::OverlapCoverage::RequireComplete,same);},
        "absent overlap stalk was confused with a present zero stalk");
    auto wrong=right;wrong.at({edges,1})=S(large_carrier,{{0,Q{4,5}},{large_carrier-1,Q{3,5}}});
    rejects([&]{(void)local::glue_sections(carrier,left,a,wrong,b,local::OverlapCoverage::RequireComplete,same);},
        "directionally incompatible local tensor section glued");
    std::size_t zeros{},support{};for(const auto& [cell,value]:glued){(void)cell;zeros+=value.nonzeros()==0;support+=value.nonzeros();}
    check(zeros==3 && support==6,"section support/zero accounting changed");
    return {glued.size(),zeros,support};
}

void run() {
    Guard guard(limits());const auto left0=unknown(2,guard),right0=unknown(2,guard);
    check(left0.kernel().cols()==4 && right0.kernel().cols()==4,"native chain closure did not retain four directional freedoms");
    check(left0.model().parameter_count()==12,"native occurrence parameter identities collapsed");
    const auto initial=join({left0,right0},guard);
    check(initial.consistent() && initial.kernel().cols()==4,"native gluing did not identify common directional family");
    const auto first=reading(2,0,{Q{3,5},Q{4,5}});
    const auto second=reading(2,1,{Q{-4,5},Q{3,5}});
    const auto left1=left0.append({first},guard);
    const auto one_reading=join({left1,right0},guard);
    check(one_reading.kernel().cols()==2,"one directional observation should leave two free outputs");
    const auto right1=right0.append({second},guard);
    const auto two_readings=join({left1,right1},guard);
    check(two_readings.consistent() && two_readings.kernel().cols()==0,"complementary local observations did not determine shared transport");
    check(left1.update().chain_reused && right1.update().chain_reused,"append rebuilt fixed chain constraints");
    check(&left1.chain()==&left0.chain() && &right1.chain()==&right0.chain(),"temporal append lost retained chain ownership");
    check(left0.observations().empty() && right0.observations().empty(),"new observations mutated earlier snapshots");
    check(left1.update().assembled_observations==1 && right1.update().assembled_observations==1,"native update did not isolate new evidence");
    const auto first_realizations=two_readings.materialize({},guard);
    const M learned=*first_realizations[0].fibers().transport_override(faces,0);
    M rotation(2,2);rotation.set(0,0,Q{3,5});rotation.set(0,1,Q{-4,5});
    rotation.set(1,0,Q{4,5});rotation.set(1,1,Q{3,5});
    check(learned==rotation,"actual learned transport differs from observed rational rotation");
    check(learned.transpose().multiply(learned)==M::identity(2),"learned map is not exact quadrance-preserving rotation");
    for(const auto& realized:first_realizations)valid_realization(realized,learned);

    // Export the exactly inferred second direction to the first patch as an
    // identified observation. This is an immutable append, not a weight merge.
    const auto left2=left1.append({second},guard);
    check(left2.kernel().cols()==0 && left1.kernel().cols()==2,"observation exchange changed its parent family");
    const auto bad_second=reading(2,1,{Q{4,5},Q{3,5}});
    const auto candidate_right=right0.append({bad_second},guard);
    check(candidate_right.kernel().cols()==2,"candidate local reading should be individually consistent");
    const auto conflict=join({left2,candidate_right},guard,0,2);
    check(!conflict.consistent(),"same-magnitude incompatible temporal observation was accepted globally");
    const auto contradiction=conflict.contradiction();
    check(!contradiction.empty(),"native gluing omitted contradiction certificate");
    check(numeric::is_zero(numeric::checked_transpose_apply(conflict.equations(),contradiction,guard)),
        "native contradiction witness does not annihilate all allowed parameter corrections");
    const Q witness_pairing=numeric::dot(contradiction,conflict.residual_origin());
    check(witness_pairing!=0,"native contradiction witness does not separate the incompatible residual");
    const V directional_difference{Q{-8,5},Q{0}};
    const Q residual_quadrance=numeric::dot(directional_difference,directional_difference);
    check(numeric::dot(V{Q{-4,5},Q{3,5}},V{Q{-4,5},Q{3,5}})==1 &&
          numeric::dot(V{Q{4,5},Q{3,5}},V{Q{4,5},Q{3,5}})==1,
          "conflict fixture changed magnitude as well as direction");
    check(residual_quadrance==Q{64,25},"directional observation residual quadrance differs");
    rejects([&]{(void)conflict.materialize({},guard);},"contradictory global family materialized");
    rejects([&]{(void)right1.append({bad_second},guard);},"contradictory cumulative local evidence was accepted");

    // Explicit policy: reject the candidate reading and restore the last
    // committed observation for that same source/time identity. No averaging,
    // metric change, transport adjustment or new relation is silently chosen.
    const auto repaired=join({left2,right1},guard,0,3);
    check(repaired.consistent() && repaired.kernel().cols()==0,"declared observation repair did not restore global consistency");
    const auto restored=repaired.materialize({},guard);
    for(const auto& state:restored)valid_realization(state,learned);
    check(restored[1].fibers().transport_override(faces,0)->entries()==learned.entries(),"repair changed committed exact learning parameters");

    // Rectangular refinement is a separate, declared carrier experiment. All
    // old directions embed by J; the new third input is initially unobserved.
    const auto large0=unknown(3,guard);
    const auto expanded=join({left2,large0},guard,3,4);
    check(expanded.consistent() && expanded.kernel().cols()==3,"stalk growth lost old directions or fabricated new observations");
    const auto new_reading=reading(3,2,{Q{1,7},Q{-2,7},Q{3,7}});
    const auto large1=large0.append({new_reading},guard);
    const auto expanded_observed=join({left2,large1},guard,3,5);
    check(expanded_observed.consistent() && expanded_observed.kernel().cols()==0,"new-stalk reading did not determine its three new responses");
    const auto grown=expanded_observed.materialize({},guard);
    const M grown_map=*grown[1].fibers().transport_override(faces,0);
    M expected_growth(3,3);
    for(const auto& [key,q]:learned.entries())expected_growth.set(key.first,key.second,q);
    expected_growth.set(0,2,Q{1,7});expected_growth.set(1,2,Q{-2,7});expected_growth.set(2,2,Q{3,7});
    check(grown_map==expected_growth,"grown stalk did not retain exact old and declared new directions");
    valid_realization(grown[1],expected_growth);
    M j2(3,2),j1(9,6),j0(9,6);
    j2.set(0,0,1);j2.set(1,1,1);
    for(std::size_t cell=0;cell<3;++cell)for(std::size_t axis=0;axis<2;++axis) {
        j1.set(3*cell+axis,2*cell+axis,1);j0.set(3*cell+axis,2*cell+axis,1);
    }
    const auto old_b2=local::GradeTensorField(restored[0].fibers(),faces).matrix();
    const auto old_b1=local::GradeTensorField(restored[0].fibers(),edges).matrix();
    const auto new_b2=local::GradeTensorField(grown[1].fibers(),faces).matrix();
    const auto new_b1=local::GradeTensorField(grown[1].fibers(),edges).matrix();
    const auto temporal_word=temporal::finite_word_defect(temporal::OperationWord({old_b2,old_b1}),
        temporal::OperationWord({new_b2,new_b1}),{j2,j1,j0});
    check(temporal_word.exact_identity() && temporal_word.direct.nonzeros()==0,
        "native temporal word identity failed across changing stalk dimensions");
    check(std::all_of(temporal_word.position_defects.begin(),temporal_word.position_defects.end(),
        [](const M& e){return e.nonzeros()==0;}),"stalk growth altered an old directional boundary action");
    const auto new_direction=numeric::checked_apply(grown_map,S::basis(3,2),guard);
    check(same(new_direction,S(3,{{0,Q{1,7}},{1,Q{-2,7}},{2,Q{3,7}}})),"sparse new-direction field realization differs");

    // The same complete gluing is computed in a padded common fiber. The
    // entry budget is much smaller than its declared dimension, so the old
    // context product's b.rows() bucket allocation would be rejected here.
    Guard compact_guard(limits()),padded_guard(limits());
    const auto compact=join({left2,right1},compact_guard,2,6);
    const auto padded=join({left2,right1},padded_guard,large_carrier,6);
    check(padded.consistent() && padded.kernel()==compact.kernel() && padded.particular()==compact.particular(),
        "unused common-carrier directions changed exact gluing");
    check(padded.retained_entries()==compact.retained_entries(),"gluing storage scales with unused common-carrier rows");
    check(padded.preparation_steps()==compact.preparation_steps(),"gluing traversed unused common-carrier rows");
    check(padded.retained_entries()<entry_budget && large_carrier>entry_budget,
        "large-carrier certificate did not exercise admission below ambient dimension");
    check(padded.witnesses().size()==11,"zero/absent original occurrences lost from interface witnesses");
    for(const auto& witness:padded.witnesses())
        check(witness.output_dimension==large_carrier && witness.input.common_input.rows()==large_carrier &&
              witness.input.common_input.cols()==2 && witness.input.common_input.nonzeros()==2,
              "interface witness lost original ambient size or introduced phantom directions");
    const auto padded_realized=padded.materialize({},padded_guard);
    for(const auto& state:padded_realized)valid_realization(state,learned);
    const auto sections=section_gluing(learned);

    const std::vector<Stage> stages{
        stage("gluing_initial","0","attach_declared_interface",initial),
        stage("gluing_first_observation","1/4","observe_first_input_in_left_patch",one_reading),
        stage("gluing_second_observation","1/2","observe_second_input_in_right_patch",two_readings),
        stage("gluing_directional_conflict","3/4","candidate_directional_observation",conflict),
        stage("gluing_repaired","1","restore_committed_observation",repaired),
        stage("stalk_expanded","5/4","declare_rectangular_stalk_extension",expanded),
        stage("stalk_new_direction_observed","3/2","observe_new_input_direction",expanded_observed)};
    std::cout<<"{\"experiment\":\"evolving_stalk_gluing\",\"status\":\"passed\",\"assertions\":"<<checks
        <<",\"scalar_domain\":\"exact rational\",\"independent_local_free_directions\":8,\"learned_transport_rows\":";
    quoted_matrix(std::cout,learned);
    std::cout<<",\"repair_policy\":\"Reject the candidate second-direction reading and restore the last committed reading from the same retained observation source; preserve learned transport and metric.\""
        <<",\"provenance\":{\"interface_key\":7001,\"candidate_revision\":2,\"repair_revision\":3,\"observations\":["
        <<"{\"id\":\"left:e1\",\"time\":\"1/4\",\"response\":[\"3/5\",\"4/5\"]},"
        <<"{\"id\":\"right:e2\",\"time\":\"1/2\",\"response\":[\"-4/5\",\"3/5\"]},"
        <<"{\"id\":\"right:e2:candidate\",\"time\":\"3/4\",\"response\":[\"4/5\",\"3/5\"]}]}"
        <<",\"stages\":[";
    for(std::size_t i=0;i<stages.size();++i) {
        const auto& s=stages[i];if(i)std::cout<<',';
        std::cout<<"{\"id\":\""<<s.id<<"\",\"time\":\""<<s.time<<"\",\"event\":\""<<s.event
            <<"\",\"consistent\":"<<(s.consistent?"true":"false")<<",\"left_free_directions\":"<<s.left_free
            <<",\"right_free_directions\":"<<s.right_free<<",\"joint_free_directions\":";
        if(s.consistent)std::cout<<s.joint_free;else std::cout<<"null";
        std::cout<<",\"retained_observations\":"<<s.observations<<",\"interface_equations\":"<<s.equations<<'}';
    }
    std::cout<<"],\"directional_conflict\":{\"both_reading_quadrances\":\"1\",\"difference\":";
    quoted_vector(std::cout,directional_difference);
    std::cout<<",\"difference_quadrance\":\""<<residual_quadrance.get_str()<<"\",\"left_null_witness\":";
    quoted_vector(std::cout,contradiction);
    std::cout<<",\"witness_pairing\":\""<<witness_pairing.get_str()<<"\",\"transpose_action_zero\":true}"
        <<",\"stalk_extension\":{\"old_dimension\":2,\"new_dimension\":3,\"free_new_outputs_before_observation\":3,\"grown_transport_rows\":";
    quoted_matrix(std::cout,grown_map);
    std::cout<<",\"native_temporal_word_defect_nonzeros\":"<<temporal_word.direct.nonzeros()
        <<",\"old_direction_boundary_actions_preserved\":true,\"scope\":\"separate declared stalk extension using the learned transport\"}"
        <<",\"sparse_common_carrier\":{\"declared_dimension\":"<<large_carrier
        <<",\"represented_input_directions\":2,\"entry_budget\":"<<entry_budget
        <<",\"retained_entries\":"<<padded.retained_entries()<<",\"compact_retained_entries\":"<<compact.retained_entries()
        <<",\"preparation_steps\":"<<padded.preparation_steps()<<",\"compact_preparation_steps\":"<<compact.preparation_steps()
        <<",\"retained_occurrence_witnesses\":"<<padded.witnesses().size()
        <<",\"ambient_amplitude_arrays\":0,\"ambient_row_buckets\":0,\"instrumentation_scope\":\"native action entry admission and compact-versus-padded operation/storage counts; not an RSS allocator trace\"}"
        <<",\"section_gluing\":{\"retained_cells\":"<<sections.cells<<",\"present_zero_stalks\":"<<sections.zero_stalks
        <<",\"numerical_nonzeros\":"<<sections.supported_values<<",\"missing_overlap_rejected\":true,\"direction_conflict_rejected\":true}"
        <<",\"retained_source\":{\"grade_cells\":[3,3,1],\"grade_occurrences\":[0,8,3],\"present_zero_occurrences\":1,\"absent_occurrences\":1,\"identities_and_boolean_aspects_preserved\":true}"
        <<",\"scope\":\"Finite exact temporal transport learning, fiber-product gluing, directional conflict and explicit observation repair. No continuum limit, physical sensor dynamics or generic nonlinear learner is claimed.\"}\n";
}
}
int main() {
    try {run();return 0;}
    catch(const std::exception& e){std::cerr<<"evolving stalk gluing: "<<e.what()<<'\n';return 1;}
}
