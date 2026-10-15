#include "rfc/temporal/fold_history.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/storage/rfcx.hpp"
#include <cstdlib>
#include <iostream>

using Q=rfc::numeric::Rational;
using V=rfc::numeric::RationalVector;
using M=rfc::numeric::ExactMatrix;
using Z=rfc::numeric::Integer;
using G=rfc::core::Grade;
using Phase=rfc::numeric::RationalPhase;
using Pose=rfc::geometry::RationalRigidFrame;
namespace temporal=rfc::temporal;
namespace topology=rfc::topology;
static std::size_t checks{};
static void require(bool condition,const char* reason) {if (!condition) throw std::runtime_error(reason); ++checks;}
template<class Exception,class Action> static void rejects(Action action,const char* reason) {
    bool caught=false; try {action();} catch (const Exception&) {caught=true;} require(caught,reason);
}

static rfc::core::RelationalComplex panels() {
    rfc::core::RelationSignature signature;
    const auto edge=signature.add_type("panel_edge",rfc::core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face=signature.add_attaching_loop_type("panel",3,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (const auto& [a,b]:std::vector<std::pair<int,int>>{{10,20},{20,30},{30,10},{10,40},{40,20},{10,50},{50,40}})
        builder.add_grade_one(edge,{{static_cast<rfc::core::IdentityKey>(a),true,true,false,1},
                                   {static_cast<rfc::core::IdentityKey>(b),true,false,false,1}});
    for (const auto& word:std::vector<std::vector<int>>{{1,2,3},{-1,4,5},{-4,6,7}}) {
        std::vector<rfc::core::HigherIncidenceInput> occurrences;
        for (const int letter:word) occurrences.emplace_back(static_cast<std::uint32_t>((letter>0 ? letter : -letter)-1),true,letter<0,false,Q{1});
        // A repeated inactive leg remains retained, independently of geometry.
        occurrences.emplace_back(0,false,false,false,Q{7}); builder.add_higher(G{2},face,occurrences);
    }
    return builder.freeze();
}
static std::vector<V> points(const rfc::core::RelationalComplex& source) {
    std::vector<V> out;
    for (const auto key:source.grade(G{0}).identities) {
        if (key==10) out.push_back({0,0,0});
        else if (key==20) out.push_back({1,0,0});
        else if (key==30) out.push_back({0,1,0});
        else if (key==40) out.push_back({0,-1,0});
        else if (key==50) out.push_back({-1,0,0});
        else throw std::logic_error("unexpected panel vertex");
    }
    return out;
}
static auto assembly() {
    const auto source=panels();
    const Pose second(M::from_rows({{0,1,0},{-1,0,0},{0,0,1}}),{0,0,0});
    return rfc::geometry::RationalPanelAssembly::declare(source,points(source),std::vector<Pose>(3,Pose::identity()),
        {{0,0,1,Pose::identity()},{3,1,2,second}});
}
static std::uint32_t vertex_index(const rfc::core::RelationalComplex& source,rfc::core::IdentityKey key) {
    const auto& ids=source.grade(G{0}).identities;
    return static_cast<std::uint32_t>(std::find(ids.begin(),ids.end(),key)-ids.begin());
}

static void cover_checks() {
    for (std::size_t n=3;n<=9;++n) for (std::size_t branch=0;branch<n;++branch) {
        std::vector<rfc::core::IdentityKey> ids; for (std::size_t i=0;i<n;++i) ids.push_back(100+17*i);
        const auto cover=topology::InfiniteCyclicCover::declare(ids,branch);
        const auto copy=cover; const auto equal=topology::InfiniteCyclicCover::declare(ids,branch);
        require(copy.same_owner(cover) && !equal.same_owner(cover),"cover owner identity");
        for (int initial=-12;initial<=12;++initial) {
            Z position(initial); const auto start=cover.projected_vertex(position);
            std::vector<topology::EdgeStep> steps;
            for (std::size_t j=0;j<32;++j) {
                const bool forward=(j%5)!=1 && (j%5)!=2;
                const Z edge_position=forward ? position : Z(position-1);
                steps.push_back({cover.projected_edge(edge_position),forward}); position+=forward ? 1 : -1;
            }
            const topology::PairwisePath path(cover.source(),start,steps); const auto lift=cover.lift(path,Z(initial));
            require(lift.positions().back()==position && lift.path().target()==cover.projected_vertex(position),"exact unique cover lift");
            const auto cut=lift.prefix(3);
            require(cut.positions().size()==4 && cut.path().steps().size()==3,"lift prefix has no future suffix");
            rejects<std::out_of_range>([&]{(void)cut.prefix(4);},"lift future prefix refusal");
        }
        for (int turns=-3;turns<=3;++turns) {
            const Z initial(-1000000); Z current=initial; std::vector<topology::EdgeStep> steps;
            for (std::size_t j=0;j<n*static_cast<std::size_t>(turns<0 ? -turns : turns);++j) {
                const bool forward=turns>=0;
                const Z position=forward ? current : Z(current-1);
                steps.push_back({cover.projected_edge(position),forward}); current+=forward ? 1 : -1;
            }
            const auto path=topology::PairwisePath(cover.source(),cover.projected_vertex(initial),steps);
            const auto lift=cover.lift(path,initial);
            require(lift.winding()==turns,"integer winding needs the retained closed path");
            const temporal::CyclicPhaseWalk nonfaithful(lift,Phase::identity());
            require(nonfaithful.endpoint()==Phase::identity() && nonfaithful.lift().winding()==turns,"nonfaithful phase does not erase winding");
        }
        const topology::PairwisePath foreign(equal.source(),equal.projected_vertex(0),{});
        rejects<std::invalid_argument>([&]{(void)cover.lift(foreign,0);},"equal bytes do not identify path owner");
        const auto one=cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(0),{{cover.projected_edge(0),true}}),0);
        rejects<std::domain_error>([&]{(void)one.winding();},"open path has no winding reading");
        rejects<std::invalid_argument>([&]{(void)cover.lift(one.path(),1);},"wrong initial lift fiber");
    }
    const auto cover=topology::InfiniteCyclicCover::declare({1,2,3,4},0);
    std::vector<topology::EdgeStep> steps{{0,true},{1,true},{2,true},{3,true}};
    const auto lift=cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(0),steps),0);
    const Phase quarter(0,1); const temporal::CyclicPhaseWalk walk(lift,quarter);
    require(walk.endpoint()==Phase::identity() && walk.lift().winding()==1,"identity endpoint with nonzero winding");
    const auto zero=walk.prefix(0);
    require(zero.endpoint()==walk.endpoint() && zero.lift().winding()==0,"equal endpoints retain different histories");
    const Z huge("1844674407370955161600");
    require(cover.lift(topology::PairwisePath(cover.source(),cover.projected_vertex(huge),steps),huge).winding()==1,"lift uses arbitrary integers");
    auto assigned=lift;
    assigned=[] {const auto c=topology::InfiniteCyclicCover::declare({10,20,30},1);
        return c.lift(topology::PairwisePath(c.source(),c.projected_vertex(0),{{1,true},{2,true},{0,true}}),0);}();
    require(assigned.winding()==1 && &assigned.path().complex()==&assigned.cover().source(),"copy assignment pins actual path source");
    rejects<std::invalid_argument>([&]{(void)temporal::CyclicPhaseWalk(lift,Phase::cayley(Q{1,2}));},"incompatible cyclic phase generator");
    topology::CyclicCoverLimits limits; limits.max_path_steps=3;
    const auto bounded=topology::InfiniteCyclicCover::declare({1,2,3,4},0,limits);
    rejects<std::length_error>([&]{(void)bounded.lift(topology::PairwisePath(bounded.source(),bounded.projected_vertex(0),steps),0);},"cover path admission");
    limits.max_integer_bits=8;
    const auto tiny=topology::InfiniteCyclicCover::declare({1,2,3},0,limits);
    rejects<std::length_error>([&]{(void)tiny.projected_vertex(huge);},"cover integer admission");
    rejects<std::out_of_range>([]{(void)topology::InfiniteCyclicCover::declare({1,2,3},3);},"cover branch admission");
}

static void phase_checks() {
    const auto source=panels(); const Phase p=Phase::cayley(Q{1,2}),q=Phase::cayley(Q{2,3});
    const auto begin=temporal::RationalPhaseHistory::begin(source,{G{2},1},p);
    const auto bytes=rfc::storage::encode_rfcx(begin.source()).bytes;
    const auto old=begin.append({91,p,true}).append({91,q,true}).append({91,p,false});
    require(old.endpoint()==q*p*p && old.operations().size()==3,"ordered phase operations and inactive step");
    const auto view=old.observe(); const auto cut=old.prefix(1);
    require(cut.operations().size()==1 && cut.phase_at(1)==p*p && cut.same_carrier(old),"owned phase prefix");
    rejects<std::out_of_range>([&]{(void)cut.phase_at(2);},"phase prefix future refusal");
    const auto proposal=old.propose(0,p.inverse(),true);
    require(old.same_history(old) && old.endpoint()==view.phase(),"proposal does not publish");
    auto edited=old.apply(proposal); edited=edited.apply(edited.propose(1,q.inverse(),true));
    require(edited.endpoint()==q.inverse() && view.phase()==old.endpoint() && view.operation_count()==3,"phase edit replays and leaves observation immutable");
    require(rfc::storage::encode_rfcx(edited.source()).bytes==bytes,"operation history does not mutate retained source");
    rejects<std::invalid_argument>([&]{(void)edited.apply(proposal);},"stale phase proposal rejects");
    require(old.defect(edited).exact_identity(),"canonical finite word defect");
    const auto terms=old.attribution(edited);
    require(terms.size()==7 && temporal::sum_factor_interactions(terms,2,2)==
        edited.composite().add(old.composite().scale(Q{-1})),"all finite factor interactions retained");
    require(begin.composite()==M::identity(2) && begin.attribution(begin).empty() && begin.defect(begin).exact_identity(),"typed empty history identity");
    const auto other=temporal::RationalPhaseHistory::begin(source,{G{2},1},p).append({91,p,true}).append({91,q,true}).append({91,p,false});
    rejects<std::invalid_argument>([&]{(void)old.attribution(other);},"equal phase output does not identify carrier");
    temporal::ExactHistoryLimits limits; limits.max_steps=0;
    const auto bound=temporal::RationalPhaseHistory::begin(source,{G{2},0},p,limits);
    rejects<std::length_error>([&]{(void)bound.append({0,p,false});},"inactive operation still consumes history budget");
    limits.max_steps=100; limits.max_interaction_terms=2;
    const auto bound2=temporal::RationalPhaseHistory::begin(source,{G{2},0},p,limits).append({1,p,true}).append({2,q,true});
    rejects<std::length_error>([&]{(void)bound2.attribution(bound2);},"exponential attribution admission");
    rejects<std::out_of_range>([&]{(void)temporal::RationalPhaseHistory::begin(source,{G{2},3},p);},"carrier cell bounds");
    const auto field=rfc::numeric::CertifiedAlgebraicField::declare({1,-1,1},rfc::numeric::IrreducibleReduction{2});
    const auto involution=rfc::numeric::AlgebraicInvolution::declare(field,field.scalar(1)-field.generator());
    const rfc::numeric::AlgebraicUnitPhase root(involution,field.generator());
    const auto h=temporal::AlgebraicPhaseHistory::begin(source,{G{1},0},root.power(0));
    const auto a=h.append({1,root,true}).append({1,root,true}).append({1,root,false});
    const auto b=a.apply(a.propose(0,root.inverse(),true));
    require(a.endpoint()==root.power(2) && b.endpoint()==root.power(0),"algebraic phase history");
    require(a.defect(b).exact_identity() && temporal::sum_factor_interactions(a.attribution(b),2,2)==
        b.composite().add(a.composite().scale(Q{-1})),"algebraic scalar restriction reuses word calculus");
    const auto equal_involution=rfc::numeric::AlgebraicInvolution::declare(field,field.scalar(1)-field.generator());
    const rfc::numeric::AlgebraicUnitPhase foreign(equal_involution,field.generator());
    rejects<std::invalid_argument>([&]{(void)a.append({1,foreign,false});},"inactive algebraic operation checks actual involution owner");
    rejects<std::invalid_argument>([&]{(void)a.propose(1,foreign,true);},"algebraic edit domain owner");
    const auto c=topology::InfiniteCyclicCover::declare({1,2,3,4,5,6},2);
    std::vector<topology::EdgeStep> steps;
    for (std::size_t i=0;i<12;++i) steps.push_back({c.projected_edge(Z(i)),true});
    const temporal::CyclicPhaseWalk walk(c.lift(topology::PairwisePath(c.source(),c.projected_vertex(0),steps),0),root);
    require(walk.endpoint()==root.power(0) && walk.lift().winding()==2,"algebraic phase endpoint and cover winding separate");
    const temporal::OperationWord unbound;
    rejects<std::logic_error>([&]{(void)unbound.input_dim();},"unbound word input carrier rejects");
    rejects<std::logic_error>([&]{(void)unbound.output_dim();},"unbound word output carrier rejects");
    rejects<std::logic_error>([&]{(void)unbound.composite();},"unbound word cannot infer identity");
    rejects<std::logic_error>([&]{(void)unbound.apply({});},"unbound word apply rejects");
}

static void fold_checks() {
    const auto prepared=assembly(); const auto begin=temporal::RationalFoldHistory::begin(prepared);
    const auto bytes=rfc::storage::encode_rfcx(prepared.source()).bytes;
    const auto c=vertex_index(prepared.source(),40),d=vertex_index(prepared.source(),50);
    const Phase p=Phase::cayley(Q{1,2}),q=Phase::cayley(Q{2,3});
    rejects<std::invalid_argument>([&]{(void)begin.append({1,0,{1},p,true});},"moving group must preserve every shared vertex");
    const auto h1=begin.append({1,0,{1,2},p,true});
    const auto old=h1.append({2,1,{2},q,true}).append({1,0,{1,2},p,false});
    require(old.vertex(1,c)==V({0,Q{-3,5},Q{-4,5}}),"rational fold point");
    const auto Rx=rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::X,p);
    const auto Ry=rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::Y,q.inverse());
    require(old.vertex(2,d)==Rx.multiply(Ry).apply({-1,0,0}),"downstream crease follows fixed panel");
    const auto factors=old.actions(2);
    require(factors[0].multiply(factors[1])!=factors[1].multiply(factors[0]),"noncommuting world actions retained");
    require(temporal::OperationWord(factors).composite()==old.poses()[2].affine(),"chronological column convention");
    const auto same=begin.append({2,1,{2},q,true}).append({1,0,{1,2},p,true});
    require(same.poses()[2].affine()==old.poses()[2].affine() &&
        same.poses_at(1)[2].affine()!=h1.poses()[2].affine(),"same tree configuration retains different intermediate histories");
    const auto endpoint=old.observe(); const auto prefix=old.prefix(1);
    require(prefix.operations().size()==1 && prefix.poses_at(1)[2].affine()==h1.poses()[2].affine(),"owned fold prefix");
    rejects<std::out_of_range>([&]{(void)prefix.poses_at(2);},"fold prefix future refusal");
    const auto proposal=old.propose(0,p.inverse(),true);
    require(old.poses()[2].affine()==endpoint.poses()[2].affine(),"fold proposal remains prospective");
    auto edited=old.apply(proposal); edited=edited.apply(edited.propose(1,q.inverse(),true));
    require(edited.vertex(2,d)==rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::X,p.inverse())
        .multiply(rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::Y,q)).apply({-1,0,0}),"edited upstream fold replays downstream frames");
    const auto terms=old.attribution(edited,2);
    require(terms.size()==7 && temporal::sum_factor_interactions(terms,4,4)==
        edited.poses()[2].affine().add(old.poses()[2].affine().scale(Q{-1})),"fold full finite interaction identity");
    require(terms[2].changed_positions==std::vector<std::size_t>({0,1}) && terms[2].contribution.nonzeros()!=0,"fold two-position interaction survives");
    require(old.defect(edited,2).exact_identity(),"fold canonical telescoping identity");
    require(endpoint.poses()[2].affine()==old.poses()[2].affine() && old.same_carrier(edited) && !old.same_history(edited),"fold endpoints pin earlier observation");
    rejects<std::invalid_argument>([&]{(void)edited.apply(proposal);},"stale fold edit rejects");
    const auto unfolded=old.append({3,1,{2},q.inverse(),true}).append({4,0,{1,2},p.inverse(),true});
    for (const auto& pose:unfolded.poses()) require(pose.affine()==M::identity(4),"exact fold inverse replays to identity");
    require(unfolded.operations().size()==5 && rfc::storage::encode_rfcx(unfolded.assembly().source()).bytes==bytes,"unfolding retains all source/history data");
    for (int i=-3;i<=3;++i) for (int j=-3;j<=3;++j) {
        const auto a=Phase::cayley(Q(i,3)),b=Phase::cayley(Q(j,2));
        const auto h=begin.append({1,0,{1,2},a,true}).append({2,1,{2},b,true});
        const auto x=rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::X,a);
        const auto y=rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::Y,b.inverse());
        require(h.vertex(2,d)==x.multiply(y).apply({-1,0,0}),"exact fold grid realization");
        for (std::size_t crease=0;crease<prepared.creases().size();++crease) {
            const auto& interface=prepared.creases()[crease]; const auto [tail,head]=prepared.crease_endpoints(crease);
            require(h.vertex(interface.fixed_panel,tail)==h.vertex(interface.moving_panel,tail) &&
                h.vertex(interface.fixed_panel,head)==h.vertex(interface.moving_panel,head),"all crease interfaces glue");
        }
        const auto inverse=h.append({3,1,{2},b.inverse(),true}).append({4,0,{1,2},a.inverse(),true});
        require(inverse.poses()[2].affine()==M::identity(4),"grid unfolds exactly");
    }
    // A translated and rotated reference frame exercises full affine crease
    // conjugation. Transpose is not the inverse of a translated affine frame.
    const Pose global(rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::Z,p),{2,-3,5});
    std::vector<V> transformed;
    for (const auto& point:prepared.vertices()) transformed.push_back(global.apply(point));
    std::vector<rfc::geometry::PanelCrease> creases=prepared.creases();
    for (auto& crease:creases) crease.reference_frame=global.compose(crease.reference_frame);
    const auto transformed_assembly=rfc::geometry::RationalPanelAssembly::declare(prepared.source(),transformed,std::vector<Pose>(3,global),creases);
    const auto transformed_history=temporal::RationalFoldHistory::begin(transformed_assembly)
        .append({1,0,{1,2},p,true}).append({2,1,{2},q,true});
    const auto global_inverse=global.inverse();
    require(transformed_history.vertex(2,d)==global.apply(old.vertex(2,d)),"fold commutes with declared rigid reference change");
    for (std::size_t panel=0;panel<3;++panel) {
        require(transformed_history.poses()[panel].affine()==global.affine().multiply(old.poses()[panel].affine()).multiply(global_inverse.affine()),
            "complete affine comparison map transported");
        const auto new_frame=transformed_history.frame(panel),old_frame=old.frame(panel);
        require(new_frame.affine()==global.affine().multiply(old_frame.affine()),"panel frame transported with pose");
    }
    require(global.affine().transpose()!=global_inverse.affine(),"affine inverse and coordinate transpose stay separate");
    const auto copied=std::move(transformed_history);
    require(transformed_history.same_history(copied),"rvalue history preserves actual owner pin");
    rejects<std::invalid_argument>([&]{(void)begin.append({1,0,{0,1,2},p,false});},"inactive fold still validates declared group");
    rejects<std::invalid_argument>([&]{(void)begin.append({1,0,{1,1,2},p,true});},"duplicate moving panel");
    rejects<std::invalid_argument>([&]{(void)begin.append({1,0,{2},p,true});},"missing crease moving panel");
    rejects<std::out_of_range>([&]{(void)begin.vertex(3,0);},"panel accessor bounds");
    rejects<std::invalid_argument>([&]{(void)begin.vertex(0,d);},"accessor retains panel incidence support");
    temporal::ExactHistoryLimits limits; limits.max_steps=0;
    const auto bound=temporal::RationalFoldHistory::begin(prepared,limits);
    rejects<std::length_error>([&]{(void)bound.append({1,0,{1,2},p,false});},"inactive fold budget");
    limits.max_steps=1024; limits.max_coordinate_slots=48;
    const auto coords=temporal::RationalFoldHistory::begin(prepared,limits);
    rejects<std::length_error>([&]{(void)coords.append({1,0,{1,2},p,true});},"fold endpoint coordinate admission");
    limits.max_coordinate_slots=1000000;limits.max_integer_bits=4;
    const auto bits=temporal::RationalFoldHistory::begin(prepared,limits).append({1,0,{1,2},p,true});
    rejects<std::length_error>([&]{(void)bits.append({2,1,{2},q,true});},"composed fold integer growth admission");
    const auto kept=[] {return temporal::RationalFoldHistory::begin(assembly()).append({1,0,{1,2},Phase(0,1),true}).observe();}();
    require(kept.assembly().source().grade(G{2}).cell_count()==3 && kept.poses()[1].apply({0,-1,0})==V({0,0,-1}),"fold observation owns expired caller geometry");
}

static void declaration_checks() {
    const auto source=panels(); const auto verts=points(source); const auto frames=std::vector<Pose>(3,Pose::identity());
    rejects<std::invalid_argument>([]{(void)Pose(M::from_rows({{-1,0,0},{0,1,0},{0,0,1}}),{0,0,0});},"frame rejects reflection");
    rejects<std::invalid_argument>([]{(void)Pose(M::from_rows({{2,0,0},{0,1,0},{0,0,1}}),{0,0,0});},"frame rejects scale");
    rejects<std::invalid_argument>([]{auto m=M::identity(4);m.set(3,0,1);(void)Pose::from_affine(m);},"frame rejects projective final row");
    rejects<std::invalid_argument>([&]{auto v=verts;v[vertex_index(source,30)][2]=1;
        (void)rfc::geometry::RationalPanelAssembly::declare(source,v,frames,{});},"panel plane is explicit");
    rejects<std::invalid_argument>([&]{auto v=verts;v[vertex_index(source,30)]={2,0,0};
        (void)rfc::geometry::RationalPanelAssembly::declare(source,v,frames,{});},"degenerate panel realization rejects");
    rejects<std::invalid_argument>([&]{(void)rfc::geometry::RationalPanelAssembly::declare(source,verts,frames,{{1,0,1,Pose::identity()}});},"crease must belong to both panels");
    rejects<std::invalid_argument>([&]{(void)rfc::geometry::RationalPanelAssembly::declare(source,verts,frames,{{0,0,1,Pose(M::identity(3),{1,0,0})}});},"crease frame anchor is actual tail");
    rfc::geometry::PanelAssemblyLimits limits; limits.max_cells=1;
    rejects<std::length_error>([&]{(void)rfc::geometry::RationalPanelAssembly::declare(source,verts,frames,{},limits);},"assembly source admission");
    const auto a=assembly(),b=assembly(); require(!a.same_owner(b) && a.same_owner(a),"actual assembly owner distinct from equal coordinates");
    require(a.source().grade(G{2}).incidence_count()==12,"inactive panel occurrences remain retained");
    const auto unbound=topology::PairwisePath{};
    rejects<std::logic_error>([&]{(void)unbound.complex();},"unbound path has no source owner");
}

int main() {
    try {cover_checks();phase_checks();fold_checks();declaration_checks();
        std::cout<<"{\"checks\":"<<checks<<",\"status\":\"passed\"}\n"; return EXIT_SUCCESS;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return EXIT_FAILURE;}
}
