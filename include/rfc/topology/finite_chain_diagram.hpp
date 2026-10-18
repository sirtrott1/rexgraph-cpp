#pragma once

#include "rfc/topology/rational_zigzag.hpp"
#include "rfc/numeric/smith_normal_form.hpp"

namespace rfc::topology {
struct RationalChainVertex { std::vector<numeric::ExactMatrix> boundaries; };
struct RationalChainArrow { bool forward{true}; std::vector<numeric::ExactMatrix> maps; };
struct RationalHomologyVertex {
    numeric::ExactMatrix cycles, boundaries, representatives, quotient_frame;
};
struct RationalHomologyArrow {
    numeric::ExactMatrix cycle_coordinates, boundary_coordinates, homology_map;
};
namespace chain_diagram_detail {
using namespace zigzag_detail;
using IntegerMatrix=numeric::IntegerMatrix;
inline IntegerMatrix integer_block(const IntegerMatrix& a,std::size_t row,std::size_t count,Guard& guard) {
    require(row<=a.rows() && count<=a.rows()-row,"integral diagram row range");IntegerMatrix out(count,a.cols());
    for(const auto& [ij,q]:a.entries()){guard.step();if(ij.first>=row && ij.first-row<count)out.set(ij.first-row,ij.second,q);}
    return out;
}
inline void integral_zero(const IntegerMatrix& a,Guard& guard,const char* message) {
    for(const auto& [ij,q]:a.entries()){(void)ij;guard.step();require(q==0,message);}
}
inline void rational_chain(const RationalChainVertex& vertex,Guard& guard) {
    require(!vertex.boundaries.empty(),"chain diagram requires every grade including grade zero");
    guard.dimension(vertex.boundaries.size());
    for(std::size_t k=0;k<vertex.boundaries.size();++k){const auto& b=vertex.boundaries[k];guard.matrix(b);
        require(b.rows()==(k?vertex.boundaries[k-1].cols():0),"chain boundary shape");
        if(k)require(!numeric::checked_multiply(vertex.boundaries[k-1],b,guard).nonzeros(),"chain boundary square failed");}
}
inline RationalHomologyVertex rational_homology(const RationalChainVertex& v,std::size_t grade,Guard& guard) {
    Frames cycle(v.boundaries[grade],guard);
    const auto n=v.boundaries[grade].cols();
    Matrix upper(n,0);if(grade+1<v.boundaries.size())upper=v.boundaries[grade+1];
    Frames boundaries(upper,guard);
    Frames quotient(join(boundaries.image(),cycle.kernel(),guard),guard);
    require(quotient.rank()==cycle.kernel().cols(),"homology quotient cycle rank");
    auto representatives=block(quotient.image(),0,n,boundaries.rank(),quotient.rank()-boundaries.rank(),guard);
    require(numeric::checked_multiply(v.boundaries[grade],quotient.image(),guard).nonzeros()==0,"homology representatives are not cycles");
    return {cycle.kernel(),boundaries.image(),std::move(representatives),quotient.image()};
}
} // namespace chain_diagram_detail

class PreparedRationalChainDiagram {
    struct State {
        std::vector<RationalChainVertex> vertices;
        std::vector<RationalChainArrow> arrows;
        std::size_t grade;
        std::vector<RationalHomologyVertex> homology;
        std::vector<RationalHomologyArrow> induced;
        std::optional<PreparedRationalZigzag> zigzag;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedRationalChainDiagram(std::vector<RationalChainVertex> vertices,std::vector<RationalChainArrow> arrows,
        std::size_t grade,numeric::ExactActionGuard& guard) {
        using namespace chain_diagram_detail;
        require(!vertices.empty() && arrows.size()==vertices.size()-1,"chain diagram consecutive arrow family");
        const auto grades=vertices.front().boundaries.size();require(grade<grades,"chain diagram selected grade");
        for(const auto& v:vertices){require(v.boundaries.size()==grades,"chain diagram explicit common grade range");rational_chain(v,guard);}
        auto s=std::make_shared<State>();s->vertices=std::move(vertices);s->arrows=std::move(arrows);s->grade=grade;
        std::vector<std::size_t> dimensions;std::vector<RationalZigzagArrow> induced;
        for(const auto& v:s->vertices){s->homology.push_back(rational_homology(v,grade,guard));dimensions.push_back(s->homology.back().representatives.cols());}
        for(std::size_t i=0;i<s->arrows.size();++i){const auto& a=s->arrows[i];const auto u=a.forward?i:i+1,v=a.forward?i+1:i;
            require(a.maps.size()==grades,"chain diagram requires every grade map");
            for(std::size_t k=0;k<grades;++k){guard.matrix(a.maps[k]);
                require(a.maps[k].cols()==s->vertices[u].boundaries[k].cols() && a.maps[k].rows()==s->vertices[v].boundaries[k].cols(),"chain diagram map shape");
                if(k)require(numeric::checked_multiply(s->vertices[v].boundaries[k],a.maps[k],guard)==
                    numeric::checked_multiply(a.maps[k-1],s->vertices[u].boundaries[k],guard),"chain map identity failed");}
            const auto& source=s->homology[u];const auto& target=s->homology[v];
            const auto mapped=numeric::checked_multiply(a.maps[grade],source.representatives,guard);
            const auto coords=coordinates(target.quotient_frame,mapped,guard);
            auto h=block(coords,target.boundaries.cols(),target.representatives.cols(),0,source.representatives.cols(),guard);
            const auto mapped_boundaries=numeric::checked_multiply(a.maps[grade],source.boundaries,guard);
            auto boundary_coords=coordinates(target.boundaries,mapped_boundaries,guard);
            s->induced.push_back({coords,std::move(boundary_coords),h});induced.push_back({a.forward,std::move(h)});
        }
        s->zigzag.emplace(std::move(dimensions),std::move(induced),guard);state_=std::move(s);
    }
    const std::vector<RationalChainVertex>& vertices() const & {return state_->vertices;}
    const std::vector<RationalChainArrow>& arrows() const & {return state_->arrows;}
    const std::vector<RationalHomologyVertex>& homology() const & {return state_->homology;}
    const std::vector<RationalHomologyArrow>& induced() const & {return state_->induced;}
    const PreparedRationalZigzag& zigzag() const & {return *state_->zigzag;}
    std::size_t grade() const noexcept {return state_->grade;}
    const std::vector<RationalChainVertex>& vertices() const &&=delete;
    const std::vector<RationalChainArrow>& arrows() const &&=delete;
    const std::vector<RationalHomologyVertex>& homology() const &&=delete;
    const std::vector<RationalHomologyArrow>& induced() const &&=delete;
    const PreparedRationalZigzag& zigzag() const &&=delete;
};

struct IntegralChainVertex { std::vector<numeric::IntegerMatrix> boundaries; };
struct IntegralChainArrow { bool forward{true}; std::vector<numeric::IntegerMatrix> maps; };
struct IntegralModuleVertex {
    numeric::IntegerMatrix cycle_basis, boundary_coordinates;
    numeric::SmithNormalFormActions chain_smith, boundary_smith;
    std::size_t free_rank{};
    std::vector<numeric::Integer> torsion;
};
struct IntegralModuleArrow {
    // Cycle lattice map and its conjugate on the Smith presentation generators.
    numeric::IntegerMatrix cycle_map, smith_map;
    // Complete map on original upper chains witnesses preservation of relations.
    numeric::IntegerMatrix relation_witness;
};
class PreparedIntegralModuleDiagram {
    struct State {
        std::vector<IntegralChainVertex> vertices;
        std::vector<IntegralChainArrow> arrows;
        std::size_t grade{};
        std::vector<IntegralModuleVertex> modules;
        std::vector<IntegralModuleArrow> induced;
    };
    std::shared_ptr<const State> state_;
public:
    PreparedIntegralModuleDiagram(std::vector<IntegralChainVertex> vertices,std::vector<IntegralChainArrow> arrows,
        std::size_t grade,numeric::ExactActionGuard& guard) {
        using namespace chain_diagram_detail;
        require(!vertices.empty() && arrows.size()==vertices.size()-1,"integral diagram consecutive arrows");
        guard.dimension(vertices.size());const auto grades=vertices.front().boundaries.size();require(grade<grades,"integral diagram grade");
        auto s=std::make_shared<State>();s->vertices=std::move(vertices);s->arrows=std::move(arrows);s->grade=grade;
        for(const auto& v:s->vertices){
            require(v.boundaries.size()==grades,"integral diagram explicit grade range");
            for(std::size_t k=0;k<grades;++k){const auto& b=v.boundaries[k];b.validate(guard);
                require(b.rows()==(k?v.boundaries[k-1].cols():0),"integral boundary shape");
                if(k)integral_zero(v.boundaries[k-1].multiply(b,guard),guard,"integral boundary square failed");}
            const auto n=v.boundaries[grade].cols();auto chain=numeric::smith_normal_form_actions(v.boundaries[grade],guard);
            auto cycles=chain.right_columns(chain.rank,n-chain.rank,guard);
            IntegerMatrix upper(n,0);if(grade+1<grades)upper=v.boundaries[grade+1];
            const auto transformed=chain.right_inverse.apply(upper,guard);
            integral_zero(integer_block(transformed,0,chain.rank,guard),guard,"integral boundary outside cycle lattice");
            auto boundary=integer_block(transformed,chain.rank,n-chain.rank,guard);
            require(cycles.multiply(boundary,guard)==upper,"integral lattice boundary reconstruction");
            auto smith=numeric::smith_normal_form_actions(boundary,guard);std::vector<numeric::Integer> torsion;
            for(const auto& d:smith.invariant_factors)if(d>1)torsion.push_back(d);
            const auto free_rank=cycles.cols()-smith.rank;
            s->modules.push_back({std::move(cycles),std::move(boundary),std::move(chain),std::move(smith),free_rank,std::move(torsion)});
        }
        for(std::size_t i=0;i<s->arrows.size();++i){const auto& a=s->arrows[i];const auto u=a.forward?i:i+1,v=a.forward?i+1:i;
            require(a.maps.size()==grades,"integral diagram requires every grade map");
            for(std::size_t k=0;k<grades;++k){a.maps[k].validate(guard);
                require(a.maps[k].cols()==s->vertices[u].boundaries[k].cols() && a.maps[k].rows()==s->vertices[v].boundaries[k].cols(),"integral diagram map shape");
                if(k)require(s->vertices[v].boundaries[k].multiply(a.maps[k],guard)==a.maps[k-1].multiply(s->vertices[u].boundaries[k],guard),"integral chain map identity failed");}
            const auto& source=s->modules[u];const auto& target=s->modules[v];
            const auto mapped=a.maps[grade].multiply(source.cycle_basis,guard);
            const auto transformed=target.chain_smith.right_inverse.apply(mapped,guard);
            integral_zero(integer_block(transformed,0,target.chain_smith.rank,guard),guard,"integral map did not preserve cycles");
            auto cycle_map=integer_block(transformed,target.chain_smith.rank,target.cycle_basis.cols(),guard);
            require(target.cycle_basis.multiply(cycle_map,guard)==mapped,"integral induced cycle reconstruction");
            IntegerMatrix witness(target.boundary_coordinates.cols(),source.boundary_coordinates.cols());
            if(grade+1<grades)witness=a.maps[grade+1];
            require(cycle_map.multiply(source.boundary_coordinates,guard)==target.boundary_coordinates.multiply(witness,guard),"integral map not well defined modulo boundaries");
            auto smith_map=target.boundary_smith.left.apply(source.boundary_smith.left.apply_inverse_on_right(cycle_map,guard),guard);
            s->induced.push_back({std::move(cycle_map),std::move(smith_map),std::move(witness)});
        }
        state_=std::move(s);
    }
    const std::vector<IntegralChainVertex>& vertices() const & {return state_->vertices;}
    const std::vector<IntegralChainArrow>& arrows() const & {return state_->arrows;}
    const std::vector<IntegralModuleVertex>& modules() const & {return state_->modules;}
    const std::vector<IntegralModuleArrow>& induced() const & {return state_->induced;}
    std::size_t grade() const noexcept {return state_->grade;}
    const std::vector<IntegralChainVertex>& vertices() const &&=delete;
    const std::vector<IntegralChainArrow>& arrows() const &&=delete;
    const std::vector<IntegralModuleVertex>& modules() const &&=delete;
    const std::vector<IntegralModuleArrow>& induced() const &&=delete;
};
} // namespace rfc::topology
