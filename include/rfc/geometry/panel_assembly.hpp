#pragma once
#include "rfc/core/axioms.hpp"
#include "rfc/core/boundary_fiber.hpp"
#include "rfc/geometry/rational_rotation.hpp"
#include <algorithm>
#include <memory>

namespace rfc::geometry {
namespace panel_detail {
inline void admit(const numeric::Rational& q,std::size_t bits) {
    if (mpz_sizeinbase(mpq_numref(q.get_mpq_t()),2)>bits ||
        mpz_sizeinbase(mpq_denref(q.get_mpq_t()),2)>bits)
        throw std::length_error("panel rational integer bit budget");
}
inline void admit(const numeric::ExactMatrix& matrix,std::size_t bits) {
    for (const auto& entry:matrix.entries()) admit(entry.second,bits);
}
inline numeric::RationalVector difference(const numeric::RationalVector& a,const numeric::RationalVector& b) {
    if (a.size()!=b.size()) throw std::invalid_argument("panel coordinate dimension mismatch");
    numeric::RationalVector out; out.reserve(a.size());
    for (std::size_t i=0;i<a.size();++i) out.emplace_back(a[i]-b[i]);
    return out;
}
inline std::pair<std::uint32_t,std::uint32_t> ends(const core::RelationalComplex& source,std::size_t edge) {
    const core::BoundaryOccurrenceFiber fiber(source,core::Grade{1},edge);
    std::vector<core::BoundaryOccurrenceView> active;
    for (const auto& occurrence:fiber.occurrences()) if (occurrence.exists) active.push_back(occurrence);
    if (active.size()!=2 || active[0].coefficient==0 || active[0].coefficient!=-active[1].coefficient)
        throw std::invalid_argument("panel edge requires two opposite nonzero endpoint occurrences");
    if (active[0].coefficient<0) return {active[0].lower.index,active[1].lower.index};
    return {active[1].lower.index,active[0].lower.index};
}
} // namespace panel_detail

// A declared right-handed Euclidean Q^3 frame, represented by its exact affine
// map from local to reference coordinates. It is a geometric value; the owning
// assembly binds it to a retained panel or crease identity.
class RationalRigidFrame {
    using M=numeric::ExactMatrix;
public:
    RationalRigidFrame(const RationalRigidFrame&)=default;
    RationalRigidFrame& operator=(const RationalRigidFrame&)=default;
    RationalRigidFrame(M orientation,numeric::RationalVector origin) {
        if (orientation.rows()!=3 || orientation.cols()!=3 || origin.size()!=3)
            throw std::invalid_argument("rigid frame requires three coordinates");
        origin=numeric::canonical_vector(std::move(origin));
        if (orientation.transpose().multiply(orientation)!=M::identity(3) || orientation.determinant()!=1)
            throw std::invalid_argument("rigid frame requires a proper exact orthogonal orientation");
        affine_=M::identity(4);
        for (std::size_t i=0;i<3;++i) {
            affine_.set(i,3,origin[i]);
            for (std::size_t j=0;j<3;++j) affine_.set(i,j,orientation.get(i,j));
        }
    }
    static RationalRigidFrame identity() {return {M::identity(3),{0,0,0}};}
    static RationalRigidFrame from_affine(const M& map) {
        if (map.rows()!=4 || map.cols()!=4 || map.get(3,3)!=1 ||
            map.get(3,0)!=0 || map.get(3,1)!=0 || map.get(3,2)!=0)
            throw std::invalid_argument("rigid affine map requires a homogeneous final row");
        M orientation(3,3); numeric::RationalVector origin;
        for (std::size_t i=0;i<3;++i) {
            origin.push_back(map.get(i,3));
            for (std::size_t j=0;j<3;++j) orientation.set(i,j,map.get(i,j));
        }
        return {std::move(orientation),std::move(origin)};
    }
    const M& affine() const & noexcept {return affine_;}
    const M& affine() const && = delete;
    numeric::RationalVector apply(const numeric::RationalVector& point) const {
        if (point.size()!=3) throw std::invalid_argument("rigid point requires three coordinates");
        auto lifted=point; lifted.push_back(1); auto value=affine_.apply(lifted); value.pop_back(); return value;
    }
    RationalRigidFrame inverse() const {return from_affine(affine_.inverse());}
    RationalRigidFrame compose(const RationalRigidFrame& applied_first) const {
        return from_affine(affine_.multiply(applied_first.affine_));
    }
private:
    M affine_;
};

struct PanelCrease {
    std::uint32_t edge{};
    std::uint32_t fixed_panel{};
    std::uint32_t moving_panel{};
    RationalRigidFrame reference_frame; // local X axis is the oriented crease
};
struct PanelAssemblyLimits {
    std::size_t max_cells{4096};
    std::size_t max_occurrences{65536};
    std::size_t max_creases{4096};
    std::size_t max_integer_bits{16384};
};

// A complete grade-0..2 polygonal panel source and its declared Q^3 realization.
// All source occurrences survive. A panel frame declares its plane z=0; a
// crease declares the exact shared primary edge and the two incident panels.
class RationalPanelAssembly {
    struct State {
        core::RelationalComplex source;
        std::vector<numeric::RationalVector> vertices;
        std::vector<RationalRigidFrame> frames;
        std::vector<PanelCrease> creases;
        std::vector<std::vector<std::uint32_t>> panel_vertices;
        PanelAssemblyLimits limits;
    };
public:
    RationalPanelAssembly(const RationalPanelAssembly&)=default;
    RationalPanelAssembly& operator=(const RationalPanelAssembly&)=default;
    static RationalPanelAssembly declare(const core::RelationalComplex& source,
        std::vector<numeric::RationalVector> vertices,std::vector<RationalRigidFrame> panel_frames,
        std::vector<PanelCrease> creases,PanelAssemblyLimits limits={}) {
        if (source.grade_count()!=3 || panel_frames.empty() ||
            panel_frames.size()!=source.grade(core::Grade{2}).cell_count() ||
            vertices.size()!=source.grade(core::Grade{0}).cell_count())
            throw std::invalid_argument("panel assembly requires complete grade-0..2 source and declared frames/vertices");
        std::size_t cells{},occurrences{};
        for (std::uint16_t k=0;k<3;++k) {
            cells=machine::checked_size_add(cells,source.grade(core::Grade{k}).cell_count());
            occurrences=machine::checked_size_add(occurrences,source.grade(core::Grade{k}).incidence_count());
        }
        if (cells>limits.max_cells || occurrences>limits.max_occurrences || creases.size()>limits.max_creases)
            throw std::length_error("panel assembly source budget");
        if (!core::bianchi_through(source,source.max_grade()).satisfied())
            throw std::invalid_argument("panel source fails complete chain family");
        for (auto& vertex:vertices) {
            if (vertex.size()!=3) throw std::invalid_argument("panel vertex requires three coordinates");
            vertex=numeric::canonical_vector(std::move(vertex));
            for (const auto& q:vertex) panel_detail::admit(q,limits.max_integer_bits);
        }
        for (const auto& frame:panel_frames) panel_detail::admit(frame.affine(),limits.max_integer_bits);
        std::vector<std::pair<std::uint32_t,std::uint32_t>> endpoints;
        for (std::size_t i=0;i<source.grade(core::Grade{1}).cell_count();++i)
            endpoints.push_back(panel_detail::ends(source,i));
        std::vector<std::vector<std::uint32_t>> supports;
        for (std::size_t panel=0;panel<panel_frames.size();++panel) {
            const auto type=source.grade(core::Grade{2}).relation_types[panel];
            if (!source.signature().type(type).is_attaching_loop())
                throw std::invalid_argument("panel requires a declared ordered attaching loop");
            const core::BoundaryOccurrenceFiber boundary(source,core::Grade{2},panel);
            std::vector<std::uint32_t> support; std::uint32_t start{},current{}; bool first=true;
            for (const auto& occurrence:boundary.occurrences()) {
                if (!occurrence.exists) continue;
                if (occurrence.coefficient!=1 && occurrence.coefficient!=-1)
                    throw std::invalid_argument("panel attaching occurrence requires a unit sign");
                auto [from,to]=endpoints.at(occurrence.lower.index);
                if (occurrence.coefficient<0) std::swap(from,to);
                if (first) {start=from; current=from; first=false;}
                if (from!=current) throw std::invalid_argument("panel attaching order is disconnected");
                current=to; support.push_back(from); support.push_back(to);
            }
            if (first || current!=start) throw std::invalid_argument("panel requires a closed nonempty attaching path");
            std::sort(support.begin(),support.end()); support.erase(std::unique(support.begin(),support.end()),support.end());
            const auto inverse=panel_frames[panel].inverse(); std::vector<numeric::RationalVector> local;
            for (const auto v:support) {
                auto point=inverse.apply(vertices[v]);
                if (point[2]!=0) throw std::invalid_argument("panel vertex is outside its declared frame plane");
                local.push_back(std::move(point));
            }
            bool spans_plane=false;
            for (std::size_t i=1;i<local.size();++i) for (std::size_t j=i+1;j<local.size();++j) {
                const auto a=panel_detail::difference(local[i],local[0]);
                const auto b=panel_detail::difference(local[j],local[0]);
                if (a[0]*b[1]-a[1]*b[0]!=0) spans_plane=true;
            }
            if (!spans_plane) throw std::invalid_argument("panel realization is degenerate");
            supports.push_back(std::move(support));
        }
        for (const auto& crease:creases) {
            if (crease.fixed_panel>=panel_frames.size() || crease.moving_panel>=panel_frames.size() ||
                crease.fixed_panel==crease.moving_panel || crease.edge>=endpoints.size())
                throw std::invalid_argument("crease panel or primary edge index");
            for (const auto panel:{crease.fixed_panel,crease.moving_panel}) {
                const core::BoundaryOccurrenceFiber boundary(source,core::Grade{2},panel); bool present=false;
                for (const auto& occurrence:boundary.occurrences())
                    if (occurrence.exists && occurrence.lower.index==crease.edge) present=true;
                if (!present) throw std::invalid_argument("crease edge is not in both declared panels");
            }
            panel_detail::admit(crease.reference_frame.affine(),limits.max_integer_bits);
            const auto inverse=crease.reference_frame.inverse(); const auto [tail,head]=endpoints[crease.edge];
            const auto a=inverse.apply(vertices[tail]); const auto b=inverse.apply(vertices[head]);
            if (a!=numeric::RationalVector({0,0,0}) || b[0]<=0 || b[1]!=0 || b[2]!=0)
                throw std::invalid_argument("crease frame must start at tail and orient its X axis toward head");
        }
        return RationalPanelAssembly(std::make_shared<const State>(State{source,std::move(vertices),std::move(panel_frames),
            std::move(creases),std::move(supports),limits}));
    }
    const core::RelationalComplex& source() const & noexcept {return state_->source;}
    const std::vector<numeric::RationalVector>& vertices() const & noexcept {return state_->vertices;}
    const std::vector<RationalRigidFrame>& frames() const & noexcept {return state_->frames;}
    const std::vector<PanelCrease>& creases() const & noexcept {return state_->creases;}
    const PanelAssemblyLimits& limits() const & noexcept {return state_->limits;}
    const core::RelationalComplex& source() const && = delete;
    const std::vector<numeric::RationalVector>& vertices() const && = delete;
    const std::vector<RationalRigidFrame>& frames() const && = delete;
    const std::vector<PanelCrease>& creases() const && = delete;
    const PanelAssemblyLimits& limits() const && = delete;
    bool same_owner(const RationalPanelAssembly& other) const noexcept {return state_==other.state_;}
    bool panel_contains(std::size_t panel,std::uint32_t vertex) const {
        const auto& support=state_->panel_vertices.at(panel);
        return std::binary_search(support.begin(),support.end(),vertex);
    }
    std::pair<std::uint32_t,std::uint32_t> crease_endpoints(std::size_t crease) const {
        return panel_detail::ends(source(),state_->creases.at(crease).edge);
    }
private:
    explicit RationalPanelAssembly(std::shared_ptr<const State> state):state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::geometry
