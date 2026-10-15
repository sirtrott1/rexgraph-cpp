#pragma once
#include "rfc/local/algebraic_fiber_system.hpp"
#include "rfc/topology/attaching_word.hpp"
#include "rfc/topology/local_system_transport.hpp"

namespace rfc::topology {
struct CellularEdgeConnection {
    core::RelationId edge;
    local::AlgebraicModuleMap tail_chart; // edge stalk -> actual tail vertex
    local::AlgebraicModuleMap forward;    // actual tail -> actual head
};
struct CellularFaceChart {
    core::RelationId face;
    local::AlgebraicModuleMap chart; // face stalk -> ordered loop's base vertex
};
struct CellularAttachmentLimits {
    std::size_t max_path_steps{4096},max_face_moves{4096},max_trace_coordinates{1000000};
    std::size_t max_chain_routes{1000000},max_chain_route_coordinates{1000000};
    local::AlgebraicFiberLimits fibers;
};
struct CellularAttachingOccurrence {
    std::size_t source_slot;
    core::CellId edge;
    bool active;
    numeric::Rational coefficient;
    local::AlgebraicModuleMap prefix_before,prefix_after,underlying,realized;
};
struct CellularAttachingFace {
    core::RelationId face;
    std::uint32_t base_vertex;
    local::AlgebraicModuleMap chart,holonomy,holonomy_defect,boundary_defect;
    std::vector<CellularAttachingOccurrence> occurrences;
};
struct CellularChainRoute {
    std::size_t upper_slot,lower_slot;
    core::CellId middle,target;
    bool upper_active,lower_active;
    numeric::Rational upper_coefficient,lower_coefficient;
    local::AlgebraicModuleMap underlying,realized;
};

// Transport follows declared edges between actual vertex modules. Ordered loop
// charts build grade-two maps; all other attachments require complete explicit
// maps. The native algebraic fiber engine validates the whole realized tower.
class PreparedCellularAttachment {
    using Map=local::AlgebraicModuleMap;
    using Q=numeric::Rational;
    struct Edge {
        CellularEdgeConnection declaration;
        Map backwards,tail_inverse;
        Q magnitude;
    };
    struct State {
        local::AlgebraicStalkBundle bundle;
        CellularAttachmentLimits limits;
        std::vector<std::optional<Edge>> edges;
        std::vector<CellularAttachingFace> faces;
        std::vector<std::optional<std::size_t>> face_indices;
        std::optional<local::AlgebraicRelationalFiberSystem> fibers;

        State(const local::AlgebraicStalkBundle& owner,const std::vector<CellularEdgeConnection>& connections,
              const std::vector<CellularFaceChart>& charts,const std::vector<local::AlgebraicOccurrenceMap>& explicit_maps,
              local::AlgebraicChainPolicy policy,CellularAttachmentLimits admitted)
            : bundle(owner),limits(admitted) {
            const auto& source=bundle.source(); const auto& field=bundle.field();
            std::vector<std::vector<std::optional<Map>>> maps(source.grade_count());
            for (std::size_t k=1;k<source.grade_count();++k)
                maps[k].resize(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).incidence_count());
            const auto assign=[&](core::Grade grade,std::size_t slot,const Map& map) {
                if (!grade.value || grade.value>=source.grade_count()) throw std::out_of_range("cellular attachment occurrence grade");
                const auto& arena=source.grade(grade); auto& destination=maps[grade.value].at(slot);
                const auto upper=bundle.stalk({grade,arena.upper_index.at(slot)});
                const auto lower=bundle.stalk({core::Grade{static_cast<std::uint16_t>(grade.value-1u)},arena.lower_index.at(slot)});
                if (!upper.same_owner(map.source()) || !lower.same_owner(map.target()))
                    throw std::invalid_argument("cellular attachment occurrence carrier owners differ");
                if (destination) throw std::invalid_argument("duplicate generated or explicit attachment map");
                destination=map;
            };
            for (const auto& map:explicit_maps) assign(map.grade,map.incidence_slot,map.underlying);
            edges.resize(source.grade(core::Grade{1}).cell_count());
            for (const auto& connection:connections) {
                if (connection.edge.grade!=core::Grade{1}) throw std::invalid_argument("cellular connection must name a grade-one cell");
                auto& destination=edges.at(connection.edge.index);
                if (destination) throw std::invalid_argument("duplicate cellular edge connection");
                const auto ends=pairwise_endpoints(source,connection.edge.index);
                const auto edge=bundle.stalk(connection.edge),tail=bundle.stalk({core::Grade{0},ends.tail}),head=bundle.stalk({core::Grade{0},ends.head});
                if (!edge.same_owner(connection.tail_chart.source()) || !tail.same_owner(connection.tail_chart.target()) ||
                    !tail.same_owner(connection.forward.source()) || !head.same_owner(connection.forward.target()))
                    throw std::invalid_argument("cellular edge charts must bind their actual edge and endpoint carriers");
                const auto [tail_slot,head_slot]=pairwise_incidence_slots(source,connection.edge.index);
                const auto magnitude=source.incidence_coefficient(core::Grade{1},head_slot);
                destination=Edge{connection,connection.forward.inverse(),connection.tail_chart.inverse(),magnitude};
                assign(core::Grade{1},tail_slot,connection.tail_chart);
                assign(core::Grade{1},head_slot,connection.tail_chart.then(connection.forward));
            }
            if (source.grade_count()>2) face_indices.resize(source.grade(core::Grade{2}).cell_count());
            for (const auto& declared:charts) {
                if (declared.face.grade!=core::Grade{2}) throw std::invalid_argument("cellular face chart grade");
                auto& index=face_indices.at(declared.face.index);
                if (index) throw std::invalid_argument("duplicate cellular face chart");
                const auto word=attaching_word(source,declared.face.index);
                const auto face=bundle.stalk(declared.face),base=bundle.stalk({core::Grade{0},word.loop.source()});
                if (!face.same_owner(declared.chart.source()) || !base.same_owner(declared.chart.target()))
                    throw std::invalid_argument("cellular face chart must bind the actual face and ordered base vertex");
                auto prefix=Map::identity(base);
                std::vector<CellularAttachingOccurrence> occurrences;
                const auto& arena=source.grade(core::Grade{2}); const auto [begin,end]=arena.incidence_range(declared.face.index);
                occurrences.reserve(end-begin);
                for (std::size_t slot=begin;slot<end;++slot) {
                    const bool active=arena.aspects.get(source.signature().existence_aspect(),slot);
                    const auto coefficient=source.incidence_coefficient(core::Grade{2},slot);
                    const core::CellId edge{core::Grade{1},arena.lower_index[slot]}; const auto before=prefix;
                    if (active) {
                        const auto& connection=edges.at(edge.index);
                        if (!connection) throw std::invalid_argument("charted loop step has no declared cellular edge connection");
                        prefix=prefix.then(coefficient>0?connection->declaration.forward:connection->backwards);
                        const auto& endpoint_prefix=coefficient>0?before:prefix;
                        const auto map=declared.chart.then(endpoint_prefix).then(connection->tail_inverse)
                            .scale(field.scalar(Q(Q{1}/connection->magnitude)));
                        assign(core::Grade{2},slot,map);
                    }
                    const auto& map=maps[2].at(slot);
                    if (!map) throw std::invalid_argument("inactive loop occurrence requires a complete explicit map");
                    occurrences.push_back({slot,edge,active,coefficient,before,prefix,*map,
                        map->scale(field.scalar(active?coefficient:Q{0}))});
                }
                const auto defect=prefix.add(Map::identity(base).scale(field.scalar(Q{-1})));
                index=faces.size();
                faces.push_back({declared.face,word.loop.source(),declared.chart,prefix,defect,declared.chart.then(defect),std::move(occurrences)});
            }
            for (std::size_t i=0;i<face_indices.size();++i) {
                const auto& arena=source.grade(core::Grade{2});
                if (source.signature().type(arena.relation_types[i]).is_attaching_loop() && !face_indices[i])
                    throw std::invalid_argument("every declared attaching loop requires its own cellular chart");
            }
            std::vector<local::AlgebraicOccurrenceMap> complete;
            for (std::size_t k=1;k<source.grade_count();++k) for (std::size_t slot=0;slot<maps[k].size();++slot) {
                if (!maps[k][slot]) throw std::invalid_argument("missing complete cellular attachment map");
                complete.push_back({core::Grade{static_cast<std::uint16_t>(k)},slot,*maps[k][slot]});
            }
            // Check every interface even when the caller declares deformation.
            fibers=local::AlgebraicRelationalFiberSystem::declare(bundle,complete,policy,limits.fibers);
            const auto d=field.degree();
            if (!faces.empty()) {
                const auto lower=bundle.offsets(core::Grade{0}),upper=bundle.offsets(core::Grade{2});
                const auto& defect=fibers->rational_chain_defect(core::Grade{2});
                for (const auto& face:faces) for (std::size_t row=0;row<defect.rows();++row)
                    for (std::size_t j=0;j<face.boundary_defect.rational_action().cols();++j) {
                        const auto start=lower[face.base_vertex]*d;
                        const auto value=row>=start && row-start<face.boundary_defect.rational_action().rows()?
                            face.boundary_defect.rational_action().get(row-start,j):Q{0};
                        if (defect.get(row,upper[face.face.index]*d+j)!=value)
                            throw std::logic_error("multi-vertex ordered boundary telescoping identity failed");
                    }
            }
        }
        const CellularAttachingFace& face(std::size_t index) const {
            const auto found=face_indices.at(index);
            if (!found) throw std::invalid_argument("higher attachment has no pairwise loop chart");
            return faces.at(*found);
        }
        Map transport(const PairwisePath& path) const {
            auto result=Map::identity(bundle.stalk({core::Grade{0},path.source()}));
            for (const auto& step:path.steps()) {
                const auto& edge=edges.at(step.relation);
                if (!edge) throw std::invalid_argument("path step has no declared edge connection; nonpairwise fibers remain native attachments");
                result=result.then(step.forward?edge->declaration.forward:edge->backwards);
            }
            return result;
        }
    };
public:
    class Path {
    public:
        Path(const Path&)=default;
        Path& operator=(const Path&)=default;
        std::uint32_t source() const noexcept { return path_.source(); }
        std::uint32_t target() const noexcept { return path_.target(); }
        const std::vector<EdgeStep>& steps() const & noexcept { return path_.steps(); }
        const Map& transport() const & noexcept { return transport_; }
        PairwisePath borrowed() const & { return path_; }
        const std::vector<EdgeStep>& steps() const && = delete;
        const Map& transport() const && = delete;
        PairwisePath borrowed() const && = delete;
        Path prefix(std::size_t count) const {
            if (count>path_.steps().size()) throw std::out_of_range("cellular path prefix");
            return Path(owner_,path_.source(),std::vector<EdgeStep>(path_.steps().begin(),path_.steps().begin()+static_cast<std::ptrdiff_t>(count)));
        }
        Path inverse() const { const auto reverse=path_.inverse(); return Path(owner_,reverse.source(),reverse.steps()); }
        Path then(const Path& next) const {
            if (owner_!=next.owner_) throw std::invalid_argument("cellular path connection owners differ");
            if (target()!=next.source()) throw std::invalid_argument("cellular path endpoints differ");
            if (machine::checked_size_add(steps().size(),next.steps().size())>owner_->limits.max_path_steps)
                throw std::length_error("cellular path step budget");
            auto joined=steps(); joined.insert(joined.end(),next.steps().begin(),next.steps().end());
            return Path(owner_,source(),std::move(joined)); // Retain the complete unreduced operation word.
        }
    private:
        friend class PreparedCellularAttachment;
        Path(std::shared_ptr<const State> owner,std::uint32_t source,std::vector<EdgeStep> steps)
            : owner_(std::move(owner)),path_(checked_path(*owner_,source,std::move(steps))),transport_(owner_->transport(path_)) {}
        static PairwisePath checked_path(const State& owner,std::uint32_t source,std::vector<EdgeStep> steps) {
            if (steps.size()>owner.limits.max_path_steps) throw std::length_error("cellular path step budget");
            return PairwisePath(owner.bundle.source(),source,std::move(steps));
        }
        std::shared_ptr<const State> owner_;
        PairwisePath path_;
        Map transport_;
    };
    class MoveReceipt {
    public:
        MoveReceipt(const MoveReceipt&)=default;
        MoveReceipt& operator=(const MoveReceipt&)=default;
        const Path& before() const & noexcept { return before_; }
        const Path& after() const & noexcept { return after_; }
        const std::vector<FaceHomotopyMove>& moves() const & noexcept { return moves_; }
        const Path& before() const && = delete;
        const Path& after() const && = delete;
        const std::vector<FaceHomotopyMove>& moves() const && = delete;
        bool full_transport_preserved() const noexcept { return preserved_; }
        bool every_used_loop_trivial() const noexcept { return trivial_; }
    private:
        friend class PreparedCellularAttachment;
        MoveReceipt(Path before,Path after,std::vector<FaceHomotopyMove> moves,bool trivial)
            : before_(std::move(before)),after_(std::move(after)),moves_(std::move(moves)),
              preserved_(before_.transport().rational_action()==after_.transport().rational_action()),trivial_(trivial) {}
        Path before_,after_;
        std::vector<FaceHomotopyMove> moves_;
        bool preserved_,trivial_;
    };
    PreparedCellularAttachment(const PreparedCellularAttachment&)=default;
    PreparedCellularAttachment& operator=(const PreparedCellularAttachment&)=default;
    static PreparedCellularAttachment declare(const local::AlgebraicStalkBundle& bundle,
        const std::vector<CellularEdgeConnection>& edges,const std::vector<CellularFaceChart>& faces,
        const std::vector<local::AlgebraicOccurrenceMap>& explicit_maps,local::AlgebraicChainPolicy policy,
        CellularAttachmentLimits limits={}) {
        const auto& source=bundle.source();
        if (source.grade_count()<2) throw std::invalid_argument("cellular transport requires a declared one-skeleton");
        if (policy!=local::AlgebraicChainPolicy::RequireFlat && policy!=local::AlgebraicChainPolicy::DeclaredDeformation)
            throw std::invalid_argument("unknown cellular attachment chain policy");
        std::size_t peak{},occurrences{};
        for (std::size_t k=0;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            occurrences=machine::checked_size_add(occurrences,source.grade(grade).incidence_count());
            for (std::size_t i=0;i<source.grade(grade).cell_count();++i)
                peak=std::max(peak,bundle.stalk({grade,static_cast<std::uint32_t>(i)}).rank());
        }
        if (occurrences>limits.fibers.max_occurrences || explicit_maps.size()>occurrences ||
            edges.size()>source.grade(core::Grade{1}).cell_count() ||
            faces.size()>(source.grade_count()>2?source.grade(core::Grade{2}).cell_count():0))
            throw std::length_error("cellular attachment declaration budget");
        const auto width=machine::checked_size_mul(peak,bundle.field().degree());
        // Conservative complete blocks cover stored charts/inverses and traces.
        const auto blocks=machine::checked_size_add(machine::checked_size_mul(4,edges.size()),
            machine::checked_size_add(machine::checked_size_mul(4,faces.size()),machine::checked_size_mul(4,occurrences)));
        if (machine::checked_size_mul(blocks,machine::checked_size_mul(width,width))>limits.max_trace_coordinates)
            throw std::length_error("cellular attachment trace coordinate budget");
        return PreparedCellularAttachment(std::make_shared<const State>(bundle,edges,faces,explicit_maps,policy,limits));
    }
    const local::AlgebraicStalkBundle& bundle() const & noexcept { return state_->bundle; }
    const local::AlgebraicRelationalFiberSystem& realization() const & noexcept { return state_->fibers.value(); }
    const std::vector<CellularAttachingFace>& faces() const & noexcept { return state_->faces; }
    const local::AlgebraicStalkBundle& bundle() const && = delete;
    const local::AlgebraicRelationalFiberSystem& realization() const && = delete;
    const std::vector<CellularAttachingFace>& faces() const && = delete;
    bool same_owner(const PreparedCellularAttachment& other) const noexcept { return state_==other.state_; }
    Path path(std::uint32_t source,std::vector<EdgeStep> steps) const { return Path(state_,source,std::move(steps)); }
    MoveReceipt replay_face_moves(const Path& input,const std::vector<FaceHomotopyMove>& moves) const {
        if (input.owner_!=state_) throw std::invalid_argument("cellular path move owner differs");
        if (moves.size()>state_->limits.max_face_moves) throw std::length_error("cellular face move budget");
        FundamentalGroupoidPresentation presentation{&state_->bundle.source(),{}};
        for (const auto& face:state_->faces) presentation.relators.push_back(attaching_word(state_->bundle.source(),face.face.index));
        auto current=input.borrowed(); bool trivial=true;
        for (const auto& move:moves) {
            const auto& face=state_->face(move.face);
            if (face.holonomy_defect.rational_action().nonzeros()) trivial=false;
            if (move.insert && machine::checked_size_add(current.steps().size(),attaching_word(state_->bundle.source(),move.face).loop.steps().size())>state_->limits.max_path_steps)
                throw std::length_error("cellular face insertion path budget");
            current=apply_face_homotopy(presentation,current,move);
        }
        return MoveReceipt(input,Path(state_,current.source(),current.steps()),moves,trivial);
    }
    std::vector<CellularChainRoute> chain_routes(core::Grade grade,std::size_t cell) const {
        const auto& source=state_->bundle.source();
        if (grade.value<2 || grade.value>=source.grade_count()) throw std::out_of_range("cellular route requires an adjacent chain interface");
        const auto& upper=source.grade(grade); const auto [begin,end]=upper.incidence_range(cell);
        const auto middle_grade=core::Grade{static_cast<std::uint16_t>(grade.value-1u)},target_grade=core::Grade{static_cast<std::uint16_t>(grade.value-2u)};
        const auto& middle=source.grade(middle_grade); const auto d=state_->bundle.field().degree();
        std::size_t count{},slots{}; const auto rank=state_->bundle.stalk({grade,static_cast<std::uint32_t>(cell)}).rank();
        for (std::size_t p=begin;p<end;++p) {
            const auto [lower_begin,lower_end]=middle.incidence_range(upper.lower_index[p]);
            count=machine::checked_size_add(count,lower_end-lower_begin);
            for (std::size_t q=lower_begin;q<lower_end;++q) {
                const auto target=state_->bundle.stalk({target_grade,middle.lower_index[q]});
                slots=machine::checked_size_add(slots,machine::checked_size_mul(2,machine::checked_size_mul(
                    machine::checked_size_mul(rank,d),machine::checked_size_mul(target.rank(),d))));
            }
        }
        if (count>state_->limits.max_chain_routes || slots>state_->limits.max_chain_route_coordinates)
            throw std::length_error("cellular chain route coordinate budget");
        std::vector<CellularChainRoute> result; result.reserve(count); const auto exists=source.signature().existence_aspect();
        for (std::size_t p=begin;p<end;++p) {
            const auto [lower_begin,lower_end]=middle.incidence_range(upper.lower_index[p]);
            for (std::size_t q=lower_begin;q<lower_end;++q) {
                const bool a=upper.aspects.get(exists,p),b=middle.aspects.get(exists,q);
                const auto u=source.incidence_coefficient(grade,p),l=source.incidence_coefficient(middle_grade,q);
                const auto map=realization().occurrence_map(grade,p).then(realization().occurrence_map(middle_grade,q));
                result.push_back({p,q,{middle_grade,upper.lower_index[p]},{target_grade,middle.lower_index[q]},a,b,u,l,map,
                    map.scale(state_->bundle.field().scalar(a&&b?Q(u*l):Q{0}))});
            }
        }
        return result;
    }
private:
    explicit PreparedCellularAttachment(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::topology
