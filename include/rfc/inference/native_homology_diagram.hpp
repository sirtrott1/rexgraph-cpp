#pragma once

#include "rfc/topology/finite_chain_diagram.hpp"
#include "rfc/topology/integral_homology.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/storage/native_accession.hpp"
#include "rfc/runtime/execution_policy.hpp"
#include <variant>

namespace rfc::inference {
enum class HomologyDiagramProduct { RationalZigzag, IntegralModules };
struct NativeHomologyDiagramVertex {
    storage::PinnedNativeAccession publication;
    std::vector<std::uint64_t> grade_carriers;
};
struct NativeHomologyDiagramArrow {
    bool forward{true};
    // Complete rectangular support in the two explicitly named source grades.
    std::vector<numeric::ExactMatrix> maps;
};
struct NativeHomologyDiagramRequest {
    HomologyDiagramProduct product{HomologyDiagramProduct::RationalZigzag};
    core::Grade grade{};
    std::vector<NativeHomologyDiagramVertex> vertices;
    std::vector<NativeHomologyDiagramArrow> arrows;
};
namespace native_diagram_detail {
using topology::zigzag_detail::require;
using topology::zigzag_detail::add;
using Guard=numeric::ExactActionGuard;
inline numeric::IntegerMatrix integer_matrix(const numeric::ExactMatrix& matrix,Guard& guard) {
    guard.matrix(matrix);numeric::IntegerMatrix out(matrix.rows(),matrix.cols());
    for(const auto& [ij,q]:matrix.entries()){guard.step();require(q.get_den()==1,"integral diagram requires integral maps");out.set(ij.first,ij.second,q.get_num());}
    return out;
}
inline void carriers(const NativeHomologyDiagramVertex& v,Guard& guard) {
    const auto endpoint=v.publication.endpoint();
    require(v.publication.source_namespace()!=0,"homology diagram requires a source namespace");
    require(v.grade_carriers.size()==endpoint->source.grade_count(),"homology diagram requires every actual grade carrier");
    std::map<std::uint64_t,const storage::TemporalCarrier*> declared;
    for(const auto& c:endpoint->carriers){guard.step();declared.emplace(c.key,&c);}
    for(std::size_t k=0;k<v.grade_carriers.size();++k){
        const auto it=declared.find(v.grade_carriers[k]);require(it!=declared.end(),"homology diagram carrier absent");const auto& c=*it->second;
        require(c.kind==storage::TemporalCarrierKind::SourceGrade && c.grade.value==k &&
            c.domain==numeric::IndexDomain::linear(endpoint->source.grade(c.grade).cell_count()),"homology diagram requires an authentic source grade carrier");
    }
}
} // namespace native_diagram_detail

class PreparedNativeHomologyDiagram {
    using Product=std::variant<topology::PreparedRationalChainDiagram,topology::PreparedIntegralModuleDiagram>;
    struct State {
        NativeHomologyDiagramRequest declaration;
        Product product;
        numeric::ExactActionLimits limits;
        std::size_t steps;
        State(NativeHomologyDiagramRequest d,Product p,numeric::ExactActionLimits l,std::size_t work)
            :declaration(std::move(d)),product(std::move(p)),limits(l),steps(work){}
    };
    std::shared_ptr<const State> state_;
    explicit PreparedNativeHomologyDiagram(std::shared_ptr<const State> s):state_(std::move(s)){}
public:
    static PreparedNativeHomologyDiagram prepare(NativeHomologyDiagramRequest request,
        runtime::ExecutionLease lease=runtime::ExecutionLease{}) {
        using namespace native_diagram_detail;
        lease.check_cancelled();require(!request.vertices.empty() && request.arrows.size()==request.vertices.size()-1,"native diagram requires consecutive declared arrows");
        require(request.product==HomologyDiagramProduct::RationalZigzag || request.product==HomologyDiagramProduct::IntegralModules,"native diagram product");
        runtime::ExecutionRequirements payload;std::size_t dimensions=0,grades=0;
        auto work=lease.policy().limits.scalar_work;
        for(const auto& v:request.vertices){lease.check_cancelled();const auto& source=v.publication.endpoint()->source;
            grades=std::max(grades,source.grade_count());payload.records=add(payload.records,v.grade_carriers.size());
            work=std::min(work,v.publication.limits().max_action_work);
            for(std::size_t k=0;k<source.grade_count();++k){const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
                dimensions=add(dimensions,arena.cell_count());payload.records=add(payload.records,add(arena.cell_count(),arena.incidence_count()));
                payload.rational_slots=add(payload.rational_slots,add(arena.cell_count(),arena.incidence_count()));}
        }
        require(request.grade.value<grades,"native diagram selected grade absent from every source");
        for(const auto& a:request.arrows)for(const auto& m:a.maps)payload.rational_slots=add(payload.rational_slots,m.nonzeros());
        payload.sparse_entries=payload.rational_slots;runtime::admit_execution(payload,lease.policy());
        const auto slots=std::min(lease.policy().limits.rational_slots,lease.policy().limits.sparse_entries);
        require(payload.rational_slots<=slots,"native diagram resident payload exceeds action policy");
        const numeric::ExactActionLimits limits{add(add(machine::checked_size_mul(dimensions,2),request.vertices.size()),grades),
            slots-payload.rational_slots,work,lease.policy().limits.integer_bits};
        Guard guard(limits,[&lease]{lease.check_cancelled();});
        for(const auto& v:request.vertices){carriers(v,guard);field::hodge_frame_detail::source_values(v.publication.endpoint()->source,guard);}
        if(request.product==HomologyDiagramProduct::RationalZigzag){
            std::vector<topology::RationalChainVertex> vertices;std::vector<topology::RationalChainArrow> arrows;
            for(const auto& v:request.vertices){const auto& source=v.publication.endpoint()->source;topology::RationalChainVertex vertex;
                std::size_t previous=0;
                for(std::size_t k=0;k<grades;++k){
                    const auto n=k<source.grade_count()?source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count():0;
                    if(k && k<source.grade_count())vertex.boundaries.push_back(field::hodge_frame_detail::boundary(source,core::Grade{static_cast<std::uint16_t>(k)},guard));
                    else vertex.boundaries.emplace_back(previous,n);
                    previous=n;
                }
                vertices.push_back(std::move(vertex));
            }
            for(const auto& a:request.arrows)arrows.push_back({a.forward,a.maps});
            auto product=topology::PreparedRationalChainDiagram(std::move(vertices),std::move(arrows),request.grade.value,guard);
            return PreparedNativeHomologyDiagram(std::make_shared<State>(std::move(request),std::move(product),limits,guard.steps()));
        }
        std::vector<topology::IntegralChainVertex> vertices;std::vector<topology::IntegralChainArrow> arrows;
        for(const auto& v:request.vertices){const auto& source=v.publication.endpoint()->source;topology::IntegralChainVertex vertex;
            std::size_t previous=0;
            for(std::size_t k=0;k<grades;++k){const auto n=k<source.grade_count()?source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count():0;
                if(k<source.grade_count()){
                    const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
                    guard.entries(arena.incidence_count());
                    for(std::size_t p=0;p<arena.incidence_count();++p)guard.step();
                    auto boundary=topology::integral_boundary_matrix(source,core::Grade{static_cast<std::uint16_t>(k)});boundary.validate(guard);
                    vertex.boundaries.push_back(std::move(boundary));
                }else vertex.boundaries.emplace_back(previous,n);
                previous=n;
            }
            vertices.push_back(std::move(vertex));
        }
        for(const auto& a:request.arrows){topology::IntegralChainArrow arrow;arrow.forward=a.forward;
            for(const auto& m:a.maps)arrow.maps.push_back(integer_matrix(m,guard));
            arrows.push_back(std::move(arrow));}
        auto product=topology::PreparedIntegralModuleDiagram(std::move(vertices),std::move(arrows),request.grade.value,guard);
        return PreparedNativeHomologyDiagram(std::make_shared<State>(std::move(request),std::move(product),limits,guard.steps()));
    }
    const NativeHomologyDiagramRequest& declaration() const & {return state_->declaration;}
    const topology::PreparedRationalChainDiagram& rational() const & {return std::get<topology::PreparedRationalChainDiagram>(state_->product);}
    const topology::PreparedIntegralModuleDiagram& integral() const & {return std::get<topology::PreparedIntegralModuleDiagram>(state_->product);}
    numeric::ExactActionLimits limits() const noexcept {return state_->limits;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}
    const NativeHomologyDiagramRequest& declaration() const &&=delete;
    const topology::PreparedRationalChainDiagram& rational() const &&=delete;
    const topology::PreparedIntegralModuleDiagram& integral() const &&=delete;
};
} // namespace rfc::inference
