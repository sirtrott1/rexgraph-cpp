#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/field/integral_symplectic_basis.hpp"
#include <array>
#include <limits>

namespace rfc::field {
enum class SquareTiledOrientation { Positive, Negative };
class PreparedSquareTiledPeriods;

// Translations glue the right/up sides of equally subdivided oriented squares.
// Retain the permutations, every corner identification and actual ordered face
// flags. No anonymous matrix is interpreted as a manifold or a cup declaration.
class PreparedSquareTiledSurface {
    using Z=numeric::IntegerMatrix;using Q=numeric::Rational;using I=numeric::Integer;using Guard=numeric::ExactActionGuard;
public:
    struct Work {std::size_t squares,faces,edges,vertices,genus,retained_entry_allowance,preparation_steps;};
    static PreparedSquareTiledSurface prepare(std::vector<std::uint32_t> right,std::vector<std::uint32_t> up,
        std::uint32_t subdivision,SquareTiledOrientation orientation,std::uint64_t key,std::uint64_t revision,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();const auto s=right.size();
        if(!s||up.size()!=s||!subdivision||!key||!revision)throw std::invalid_argument("square surface needs equally sized nonempty permutations, positive subdivision and source identity/revision");
        if(orientation!=SquareTiledOrientation::Positive&&orientation!=SquareTiledOrientation::Negative)throw std::invalid_argument("square surface needs a declared coherent orientation");
        guard.dimension(s);const auto n=static_cast<std::size_t>(subdivision),f=mul(s,mul(n,n)),e=mul(2,f),corners=mul(4,f);
        guard.dimension(corners);guard.dimension(e);
        if(corners>std::numeric_limits<std::uint32_t>::max())throw std::length_error("square surface cell/occurrence index limit");
        const auto retained=add(128,add(mul(2048,f),mul(128,s)));auto action=phase(guard,retained);
        for(const auto* permutation:{&right,&up}){
            std::vector<bool> used(s,false);for(const auto v:*permutation){action.step();if(v>=s||used[v])throw std::invalid_argument("square side gluing must be a bijection");used[v]=true;}
        }
        std::vector<std::size_t> order{0};std::vector<bool> reached(s,false);reached[0]=true;
        for(std::size_t i=0;i<order.size();++i)for(const auto target:{right[order[i]],up[order[i]]}){
            action.step();if(!reached[target]){reached[target]=true;order.push_back(target);}
        }
        if(order.size()!=s)throw std::invalid_argument("square surface gluing must be connected");
        const auto index=[n](std::size_t square,std::size_t x,std::size_t y){return (square*n+x)*n+y;};
        std::vector<std::uint32_t> refined_right(f),refined_up(f);
        std::vector<std::size_t> parent(corners);for(std::size_t i=0;i<corners;++i){action.step();parent[i]=i;}
        const auto root=[&](std::size_t a){while(parent[a]!=a){action.step();parent[a]=parent[parent[a]];a=parent[a];}return a;};
        const auto join=[&](std::size_t a,std::size_t b){action.step();a=root(a);b=root(b);if(a!=b)parent[std::max(a,b)]=std::min(a,b);};
        for(std::size_t square=0;square<s;++square)for(std::size_t x=0;x<n;++x)for(std::size_t y=0;y<n;++y){
            action.step();const auto cell=index(square,x,y),r=x+1<n?index(square,x+1,y):index(right[square],0,y),u=y+1<n?index(square,x,y+1):index(up[square],x,0);
            refined_right[cell]=static_cast<std::uint32_t>(r);refined_up[cell]=static_cast<std::uint32_t>(u);
            join(4*cell+1,4*r);join(4*cell+2,4*r+3);join(4*cell+3,4*u);join(4*cell+2,4*u+1);
        }
        std::map<std::size_t,std::uint32_t> ids;std::vector<std::array<std::uint32_t,4>> vertices(f),face_edges(f);
        for(std::size_t cell=0;cell<f;++cell)for(std::size_t c=0;c<4;++c){action.step();const auto representative=root(4*cell+c);vertices[cell][c]=ids.emplace(representative,static_cast<std::uint32_t>(ids.size())).first->second;}
        const auto nv=ids.size();guard.dimension(nv);
        core::RelationSignature signature;const auto edge=signature.add_type("square-tiled-unit-edge",core::BoundaryPolicy::DeclaredExact,2,2);
        const auto square_type=signature.add_attaching_loop_type("square-tiled-oriented-face",4,4);core::RelationalComplexBuilder builder(signature);
        for(std::size_t v=0;v<nv;++v){action.step();if(builder.declare_witness_identity(v)!=v)throw std::logic_error("square vertex order residual");}
        for(std::size_t cell=0;cell<f;++cell){action.step();builder.add_grade_one(edge,{{vertices[cell][0],true,true,false,1},{vertices[cell][1],true,false,false,1}});}
        for(std::size_t cell=0;cell<f;++cell){action.step();builder.add_grade_one(edge,{{vertices[cell][0],true,true,false,1},{vertices[cell][3],true,false,false,1}});}
        Z::SparseVector fundamental;std::vector<PeriodCupTerm> cup;
        std::vector<std::vector<std::size_t>> edge_flags(e);
        for(std::size_t cell=0;cell<f;++cell){
            action.step();auto& edges=face_edges[cell];edges={static_cast<std::uint32_t>(cell),static_cast<std::uint32_t>(f+refined_right[cell]),refined_up[cell],static_cast<std::uint32_t>(f+cell)};
            builder.add_higher(core::Grade{2},square_type,{{edges[0],true,false,false,1},{edges[1],true,false,false,1},{edges[2],true,true,false,1},{edges[3],true,true,false,1}});
            fundamental.emplace(cell,I{orientation==SquareTiledOrientation::Positive?1:-1});
            cup.push_back({static_cast<std::uint32_t>(cell),4*cell,4*cell+1,Q{1}});cup.push_back({static_cast<std::uint32_t>(cell),4*cell+3,4*cell+2,Q{-1}});
            for(std::size_t side=0;side<4;++side)edge_flags[edges[side]].push_back(4*cell+side);
        }
        auto source=builder.freeze();
        // Each edge has two opposite flags. Corner links certify a single
        // circle at every vertex, including self-gluings and cone vertices.
        std::vector<std::array<std::size_t,2>> links(corners);std::vector<unsigned> degree(corners,0);
        const auto connect=[&](std::size_t a,std::size_t b){action.step();if(degree[a]>=2||degree[b]>=2)throw std::invalid_argument("square vertex link is not a circle");links[a][degree[a]++]=b;links[b][degree[b]++]=a;};
        const auto& faces=source.grade(core::Grade{2});
        for(const auto& flags:edge_flags){
            action.step();if(flags.size()!=2||faces.lattice_coefficient[flags[0]]!=-faces.lattice_coefficient[flags[1]])throw std::invalid_argument("square edge is nonmanifold or inconsistently oriented");
            const auto a=flags[0],b=flags[1],an=4*(a/4)+(a%4+1)%4,bn=4*(b/4)+(b%4+1)%4;
            connect(a,bn);connect(an,b);
        }
        std::vector<bool> seen(corners,false);std::vector<bool> vertex_seen(nv,false);
        for(std::size_t c=0;c<corners;++c)if(!seen[c]){
            const auto vertex=vertices[c/4][c%4];if(vertex_seen[vertex])throw std::invalid_argument("square vertex has a disconnected link");vertex_seen[vertex]=true;
            std::vector<std::size_t> queue{c};seen[c]=true;
            for(std::size_t j=0;j<queue.size();++j){const auto a=queue[j];action.step();if(degree[a]!=2||vertices[a/4][a%4]!=vertex)throw std::invalid_argument("square link carrier mismatch");
                for(const auto b:links[a])if(!seen[b]){seen[b]=true;queue.push_back(b);}}
        }
        const auto b1=boundary(source,1,action),b2=boundary(source,2,action);
        if(b1.multiply(b2,action).nonzeros()||!b2.apply_sparse(fundamental,action).empty())throw std::logic_error("oriented square fundamental chain residual");
        const I twice_genus=I{2}+I{e}-I{nv}-I{f};action.value(Q{twice_genus});
        if(twice_genus<2||twice_genus%2!=0)throw std::logic_error("connected translation surface Euler characteristic residual");
        const auto genus=static_cast<std::size_t>(twice_genus.get_ui()/2);guard.step();Work work{s,f,e,nv,genus,retained,guard.steps()-start};
        return PreparedSquareTiledSurface(std::make_shared<const State>(State{std::move(right),std::move(up),subdivision,orientation,key,revision,std::move(source),std::move(refined_right),std::move(refined_up),std::move(vertices),std::move(face_edges),std::move(links),std::move(fundamental),std::move(cup),work}));
    }
    template<class N> requires (std::is_floating_point_v<N>)
    static PreparedSquareTiledSurface prepare(std::vector<std::uint32_t>,std::vector<std::uint32_t>,N,SquareTiledOrientation,std::uint64_t,std::uint64_t,Guard&)=delete;
    template<class N> requires (std::is_integral_v<N> && !std::is_same_v<N,bool>)
    static PreparedSquareTiledSurface prepare(std::vector<std::uint32_t> r,std::vector<std::uint32_t> u,N n,SquareTiledOrientation o,std::uint64_t key,std::uint64_t revision,Guard& guard){
        if(!std::in_range<std::uint32_t>(n))throw std::out_of_range("square subdivision narrowing");
        return prepare(std::move(r),std::move(u),static_cast<std::uint32_t>(n),o,key,revision,guard);
    }
    const core::RelationalComplex& source() const & noexcept{return state_->source;}
    const core::RelationalComplex& source() const &&=delete;
    const std::vector<std::uint32_t>& right() const & noexcept{return state_->right;}
    const std::vector<std::uint32_t>& right() const &&=delete;
    const std::vector<std::uint32_t>& up() const & noexcept{return state_->up;}
    const std::vector<std::uint32_t>& up() const &&=delete;
    const std::vector<std::uint32_t>& refined_right() const & noexcept{return state_->refined_right;}
    const std::vector<std::uint32_t>& refined_right() const &&=delete;
    const std::vector<std::uint32_t>& refined_up() const & noexcept{return state_->refined_up;}
    const std::vector<std::uint32_t>& refined_up() const &&=delete;
    const std::vector<std::array<std::uint32_t,4>>& corner_vertices() const & noexcept{return state_->vertices;}
    const std::vector<std::array<std::uint32_t,4>>& corner_vertices() const &&=delete;
    const std::vector<std::array<std::uint32_t,4>>& face_edges() const & noexcept{return state_->face_edges;}
    const std::vector<std::array<std::uint32_t,4>>& face_edges() const &&=delete;
    const std::vector<std::array<std::size_t,2>>& corner_links() const & noexcept{return state_->links;}
    const std::vector<std::array<std::size_t,2>>& corner_links() const &&=delete;
    const Z::SparseVector& fundamental_cycle() const & noexcept{return state_->fundamental;}
    const Z::SparseVector& fundamental_cycle() const &&=delete;
    const std::vector<PeriodCupTerm>& cup_terms() const & noexcept{return state_->cup;}
    const std::vector<PeriodCupTerm>& cup_terms() const &&=delete;
    const Work& work() const & noexcept{return state_->work;}
    const Work& work() const &&=delete;
    std::uint32_t subdivision() const noexcept{return state_->subdivision;}
    SquareTiledOrientation orientation() const noexcept{return state_->orientation;}
    std::uint64_t key() const noexcept{return state_->key;}
    std::uint64_t revision() const noexcept{return state_->revision;}
    bool same_owner(const PreparedSquareTiledSurface& other) const noexcept{return state_==other.state_;}
    PreparedSquareTiledPeriods integral_periods(Guard& guard) const;
    PreparedSquareTiledPeriods integral_periods(ExactMetric metric,Guard& guard) const;
private:
    struct State {
        std::vector<std::uint32_t> right,up;std::uint32_t subdivision;SquareTiledOrientation orientation;std::uint64_t key,revision;
        core::RelationalComplex source;std::vector<std::uint32_t> refined_right,refined_up;
        std::vector<std::array<std::uint32_t,4>> vertices,face_edges;std::vector<std::array<std::size_t,2>> links;
        Z::SparseVector fundamental;std::vector<PeriodCupTerm> cup;Work work;
    };
    explicit PreparedSquareTiledSurface(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};

// Owns both the original surface declaration and its actual period/symplectic
// owners. Gluing cycles are certified integral generators, not a trial basis.
class PreparedSquareTiledPeriods {
    using Z=numeric::IntegerMatrix;using I=numeric::Integer;using Q=numeric::Rational;using Guard=numeric::ExactActionGuard;
public:
    const PreparedSquareTiledSurface& surface() const & noexcept{return state_->surface;}
    const PreparedSquareTiledSurface& surface() const &&=delete;
    const PreparedIntegralPeriodConstruction& construction() const & noexcept{return state_->construction;}
    const PreparedIntegralPeriodConstruction& construction() const &&=delete;
    const PreparedIntegralSymplecticBasis& symplectic() const & noexcept{return state_->symplectic;}
    const PreparedIntegralSymplecticBasis& symplectic() const &&=delete;
    const Z& gluing_cycles() const & noexcept{return state_->gluing;}
    const Z& gluing_cycles() const &&=delete;
    const Z& cylinder_cycles() const & noexcept{return state_->cylinders;}
    const Z& cylinder_cycles() const &&=delete;
    const Z& gluing_coordinates() const & noexcept{return state_->coordinates;}
    const Z& gluing_coordinates() const &&=delete;
    const numeric::SmithNormalFormActions& gluing_smith() const & noexcept{return state_->smith;}
    const numeric::SmithNormalFormActions& gluing_smith() const &&=delete;
    struct Work {std::size_t retained_entry_allowance,preparation_steps;};
    const Work& work() const & noexcept{return state_->work;}
    const Work& work() const &&=delete;
private:
    friend class PreparedSquareTiledSurface;
    struct State {PreparedSquareTiledSurface surface;PreparedIntegralPeriodConstruction construction;PreparedIntegralSymplecticBasis symplectic;Z gluing,cylinders,coordinates;numeric::SmithNormalFormActions smith;Work work;};
    explicit PreparedSquareTiledPeriods(std::shared_ptr<const State> state):state_(std::move(state)){}
    static PreparedSquareTiledPeriods prepare(PreparedSquareTiledSurface surface,ExactMetric metric,Guard& guard){
        using namespace period_construction_detail;
        const auto start=guard.steps();guard.step();const auto s=surface.work().squares,n=static_cast<std::size_t>(surface.subdivision()),e=surface.work().edges,f=surface.work().faces;
        auto retained=add(surface.work().retained_entry_allowance,mul(16,mul(e,add(s,1))));
        auto action=phase(guard,retained);
        auto construction=PreparedIntegralPeriodConstruction::prepare(surface.source(),std::move(metric),surface.fundamental_cycle(),surface.cup_terms(),PeriodCoordinateBasis::CoordinatedIntegralLattice,surface.key(),surface.revision(),action);
        if(construction.lattice().genus()!=surface.work().genus)throw std::logic_error("surface topology/period genus residual");
        retained=add(retained,construction.work().retained_entry_allowance);auto frame_guard=phase(guard,retained);
        auto symplectic=PreparedIntegralSymplecticBasis::prepare(construction.lattice(),frame_guard);
        retained=add(retained,symplectic.work().retained_entry_allowance);auto build=phase(guard,retained);
        const auto missing=s;std::vector<std::size_t> parent(s,missing),direction(s,0),order{0};parent[0]=0;
        for(std::size_t i=0;i<order.size();++i)for(std::size_t d=0;d<2;++d){
            build.step();const auto target=d==0?surface.right()[order[i]]:surface.up()[order[i]];
            if(parent[target]==missing){parent[target]=order[i];direction[target]=d;order.push_back(target);}
        }
        const auto increment=[&](Z::SparseVector& v,std::size_t i,int sign){const auto q=build.add(Q{v[i]},Q{sign});if(q==0)v.erase(i);else v[i]=q.get_num();};
        const auto step=[&](Z::SparseVector& v,std::size_t square,std::size_t d,int sign){for(std::size_t t=0;t<n;++t){build.step();increment(v,d==0?(square*n+t)*n:f+square*n*n+t,sign);}};
        const auto path=[&](Z::SparseVector& v,std::size_t square,int sign){while(square!=0){build.step();step(v,parent[square],direction[square],sign);square=parent[square];}};
        Z gluing(e,add(s,1)),coordinates(construction.lattice().period_rank(),add(s,1));std::size_t col=0;
        for(std::size_t square=0;square<s;++square)for(std::size_t d=0;d<2;++d){
            const auto target=d==0?surface.right()[square]:surface.up()[square];
            if(target!=0&&parent[target]==square&&direction[target]==d)continue;
            Z::SparseVector v;path(v,square,1);step(v,square,d,1);path(v,target,-1);
            for(const auto& [i,z]:v){build.step();gluing.set(i,col,z);}
            const auto coords=construction.homology_coordinates(construction.lattice().chain(std::move(v),build),build);
            for(std::size_t i=0;i<coords.size();++i){build.step();coordinates.set(i,col,coords[i]);}++col;
        }
        if(col!=s+1)throw std::logic_error("gluing spanning-tree cycle count residual");
        auto smith=numeric::smith_normal_form_actions(coordinates,build);
        if(smith.rank!=construction.lattice().period_rank())throw std::logic_error("gluing cycles omit homology directions");
        for(const auto& z:smith.invariant_factors)if(z!=1)throw std::logic_error("gluing cycles generate a proper integral sublattice");
        std::vector<Z::SparseVector> cores;
        for(std::size_t d=0;d<2;++d){std::vector<bool> seen(s,false);
            for(std::size_t first=0;first<s;++first)if(!seen[first]){
                Z::SparseVector v;auto square=first;
                do{build.step();seen[square]=true;step(v,square,d,1);square=d==0?surface.right()[square]:surface.up()[square];}while(square!=first);
                cores.push_back(std::move(v));
            }}
        Z cylinders(e,cores.size());for(std::size_t j=0;j<cores.size();++j)for(const auto& [i,z]:cores[j]){build.step();cylinders.set(i,j,z);}
        retained=add(retained,action_entries(smith));guard.entries(retained);guard.step();Work work{retained,guard.steps()-start};
        return PreparedSquareTiledPeriods(std::make_shared<const State>(State{std::move(surface),std::move(construction),std::move(symplectic),std::move(gluing),std::move(cylinders),std::move(coordinates),std::move(smith),work}));
    }
    std::shared_ptr<const State> state_;
};
inline PreparedSquareTiledPeriods PreparedSquareTiledSurface::integral_periods(Guard& guard) const{return integral_periods(ExactMetric::identity(work().edges),guard);}
inline PreparedSquareTiledPeriods PreparedSquareTiledSurface::integral_periods(ExactMetric metric,Guard& guard) const{return PreparedSquareTiledPeriods::prepare(*this,std::move(metric),guard);}
} // namespace rfc::field
