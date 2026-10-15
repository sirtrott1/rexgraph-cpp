#pragma once
#include "rfc/algebra/polygon.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include <memory>

namespace rfc::topology {
struct CyclicCoverLimits {
    std::size_t max_vertices{4096};
    std::size_t max_path_steps{65536};
    std::size_t max_integer_bits{16384};
};
class CyclicPathLift;

// The declared infinite relational cover has vertices k in Z and edges
// k -> k+1. Its projection is the retained oriented polygon, with an explicit
// branch vertex at k=0. This indexed family is not a materialized finite cover.
class InfiniteCyclicCover {
    struct State {
        core::RelationalComplex source;
        std::vector<std::uint32_t> vertex_at;
        std::size_t branch;
        CyclicCoverLimits limits;
    };
public:
    InfiniteCyclicCover(const InfiniteCyclicCover&)=default;
    InfiniteCyclicCover& operator=(const InfiniteCyclicCover&)=default;
    static InfiniteCyclicCover declare(const std::vector<core::IdentityKey>& vertices,
                                      std::size_t branch_vertex,CyclicCoverLimits limits={}) {
        if (vertices.size()>limits.max_vertices) throw std::length_error("cyclic cover vertex budget");
        if (branch_vertex>=vertices.size()) throw std::out_of_range("cyclic cover branch vertex");
        auto source=algebra::oriented_polygon(vertices,algebra::PolygonFilling::BoundaryOnly);
        std::vector<std::uint32_t> index; index.reserve(vertices.size());
        for (std::size_t i=0;i<vertices.size();++i) index.push_back(pairwise_endpoints(source,i).tail);
        auto cover=InfiniteCyclicCover(std::make_shared<const State>(State{std::move(source),std::move(index),branch_vertex,limits}));
        cover.admit(numeric::Integer{0}); return cover;
    }
    const core::RelationalComplex& source() const & noexcept {return state_->source;}
    const core::RelationalComplex& source() const && = delete;
    const CyclicCoverLimits& limits() const & noexcept {return state_->limits;}
    const CyclicCoverLimits& limits() const && = delete;
    std::size_t period() const noexcept {return state_->vertex_at.size();}
    std::size_t branch_vertex() const noexcept {return state_->branch;}
    bool same_owner(const InfiniteCyclicCover& other) const noexcept {return state_==other.state_;}
    std::size_t residue(const numeric::Integer& position) const {
        admit(position);
        const numeric::Integer modulus(period()); numeric::Integer r;
        mpz_fdiv_r(r.get_mpz_t(),position.get_mpz_t(),modulus.get_mpz_t());
        return static_cast<std::size_t>(r.get_ui());
    }
    std::uint32_t projected_vertex(const numeric::Integer& position) const {
        return state_->vertex_at[base_position(position)];
    }
    // Positive cover edge k projects to this primary edge, including its type
    // and occurrence aspects. Negative traversal uses that same edge backwards.
    std::uint32_t projected_edge(const numeric::Integer& position) const {
        return static_cast<std::uint32_t>(base_position(position));
    }
    CyclicPathLift lift(const PairwisePath& path,const numeric::Integer& initial_position) const;
private:
    friend class CyclicPathLift;
    explicit InfiniteCyclicCover(std::shared_ptr<const State> state):state_(std::move(state)) {}
    std::size_t base_position(const numeric::Integer& position) const {
        const auto r=residue(position); const auto remaining=period()-state_->branch;
        return r>=remaining ? r-remaining : state_->branch+r;
    }
    void admit(const numeric::Integer& value) const {
        if (mpz_sizeinbase(value.get_mpz_t(),2)>state_->limits.max_integer_bits)
            throw std::length_error("cyclic lift integer bit budget");
    }
    std::shared_ptr<const State> state_;
};

class CyclicPathLift {
public:
    CyclicPathLift(const CyclicPathLift&)=default;
    CyclicPathLift& operator=(const CyclicPathLift& other) {
        if (this!=&other) {
            auto path=other.path_; auto positions=other.positions_; // finish allocations before replacing the pin
            cover_=other.cover_; path_=std::move(path); positions_=std::move(positions);
        }
        return *this;
    }
    const InfiniteCyclicCover& cover() const & noexcept {return cover_;}
    const InfiniteCyclicCover& cover() const && = delete;
    const PairwisePath& path() const & noexcept {return path_;}
    const PairwisePath& path() const && = delete;
    const std::vector<numeric::Integer>& positions() const & noexcept {return positions_;}
    const std::vector<numeric::Integer>& positions() const && = delete;
    numeric::Integer displacement() const {
        numeric::Integer value=positions_.back()-positions_.front(); cover_.admit(value); return value;
    }
    numeric::Integer winding() const {
        if (!path_.is_loop()) throw std::domain_error("cyclic winding requires a closed retained path");
        return numeric::Integer(displacement()/numeric::Integer(cover_.period()));
    }
    // A prefix owns only the requested path and lift positions. No suffix is
    // available through this observation, even if the original path survives.
    CyclicPathLift prefix(std::size_t steps) const {
        if (steps>path_.steps().size()) throw std::out_of_range("cyclic lift prefix");
        return cover_.lift(PairwisePath(cover_.source(),path_.source(),
            std::vector<EdgeStep>(path_.steps().begin(),path_.steps().begin()+static_cast<std::ptrdiff_t>(steps))),positions_.front());
    }
private:
    friend class InfiniteCyclicCover;
    CyclicPathLift(InfiniteCyclicCover cover,PairwisePath path,std::vector<numeric::Integer> positions)
        :cover_(std::move(cover)),path_(std::move(path)),positions_(std::move(positions)) {}
    InfiniteCyclicCover cover_;
    PairwisePath path_;
    std::vector<numeric::Integer> positions_;
};

inline CyclicPathLift InfiniteCyclicCover::lift(const PairwisePath& path,const numeric::Integer& initial_position) const {
    if (&path.complex()!=&source()) throw std::invalid_argument("cyclic path source owner differs");
    if (path.steps().size()>state_->limits.max_path_steps) throw std::length_error("cyclic path step budget");
    if (projected_vertex(initial_position)!=path.source()) throw std::invalid_argument("cyclic lift starts in a different fiber");
    std::vector<numeric::Integer> positions{initial_position}; positions.reserve(path.steps().size()+1);
    for (const auto& step:path.steps()) {
        numeric::Integer next=positions.back()+(step.forward ? 1 : -1); admit(next);
        const auto edge=projected_edge(step.forward ? positions.back() : next);
        if (edge!=step.relation) throw std::invalid_argument("retained path is incompatible with cyclic cover");
        positions.push_back(std::move(next));
    }
    return CyclicPathLift(*this,PairwisePath(source(),path.source(),path.steps()),std::move(positions));
}
} // namespace rfc::topology
