#pragma once

#include "rfc/storage/temporal_endpoint.hpp"
#include "rfc/core/owned_source.hpp"
#include "rfc/field/sparse_stalk_actions.hpp"
#include "rfc/local/sparse_sheaf_actions.hpp"
#include <limits>

namespace rfc::storage {

// Caller-owned admission contract, NOT authority recovered from a file. The full
// expected definitions check names, coordinate order and ordinal/named semantics.
// A file still owns its actual carrier and declaration records through the existing
// endpoint codec. Changing this contract requires an explicit caller decision.
struct DeclaredStalkBinding {
    TemporalCarrier carrier;
    composite::Declaration declaration;
};
struct DeclaredOccurrenceMapBinding {
    std::uint64_t tensor{};
    TemporalTensorRole role{TemporalTensorRole::CoefficientIndependentMap};
};
struct DeclaredLocalFamily {
    std::vector<DeclaredStalkBinding> stalks;
    std::vector<DeclaredOccurrenceMapBinding> maps;
    bool allow_constant_identity{false};
};
enum class DeclaredDownwardProfile : std::uint8_t {
    CoefficientFactored, CompleteRealizedOverrides
};
struct DeclaredLocalFieldContract {
    DeclaredLocalFamily downward, upward;
    DeclaredDownwardProfile downward_profile{DeclaredDownwardProfile::CoefficientFactored};
};
struct DeclaredFieldInput {
    std::uint64_t tensor{}, value{}; // Explicit link; matching numeric keys is not required.
};
// Existing endpoint records, ready to append to an endpoint and validate/encode.
struct DeclaredFieldRecords {
    std::vector<TemporalTensor> tensors;
    std::vector<TemporalDeclaredValue> values;
};

// Initial adapter: exact rational, positive, one-axis cell stalks; two complete
// independently declared directional families. No new solver or wire grammar.
// Constructing this binding does NOT certify flatness, observedness, cohomology,
// publication, or an implicit relation between the two map directions.
class DeclaredLocalFieldBinding {
    using Q = numeric::Rational;
    using S = numeric::SparseRationalVector;
    using Family = std::map<StableCellKey, DeclaredStalkBinding>;
    using Maps = std::map<StableOccurrenceKey, const TemporalTensor*>;
    static void require(bool ok, const char* message) {
        if (!ok) throw std::invalid_argument(message);
    }
    struct State {
        TemporalEndpoint retained;
        core::OwnedRelationalSource source;
        DeclaredLocalFieldContract contract;
        StableSourceIndex index;
        Family down, up;
        std::optional<local::OwnedLinearFiberSystem> fibers;
        std::optional<local::OwnedLinearCellularSheaf> sheaf;
        State(const TemporalEndpoint& endpoint, DeclaredLocalFieldContract requested, numeric::ExactActionLimits limits)
            : retained(endpoint), source(endpoint.source), contract(std::move(requested)),
              index(source.source()) {
            // retained.source is serialization data, not a second execution owner.
            // Both actual native map families below use exactly source.source().
            const auto cs = temporal_endpoint_detail::carriers(retained);
            const auto ts = temporal_endpoint_detail::tensors(retained);
            down = family(contract.downward, cs);
            up = family(contract.upward, cs);
            numeric::ExactActionGuard guard(limits);
            std::size_t retained_entries=0;
            const auto dm = maps(contract.downward, down, ts, false);
            const auto um = maps(contract.upward, up, ts, true);
            local::LinearStalkBundle db(source.source()), ub(source.source());
            for (const auto& [key,b] : down) { guard.dimension(b.carrier.domain.logical_size()); db.set_dimension(*index.cell(key), b.carrier.domain.logical_size()); }
            for (const auto& [key,b] : up) { guard.dimension(b.carrier.domain.logical_size()); ub.set_dimension(*index.cell(key), b.carrier.domain.logical_size()); }
            local::LinearRelationalFiberSystem df(std::move(db));
            local::LinearCellularSheaf uf(ub);
            for (std::size_t k=1; k<source.source().grade_count(); ++k) {
                const core::Grade grade{static_cast<std::uint16_t>(k)};
                const auto& arena = source.source().grade(grade);
                for (std::size_t slot=0; slot<arena.incidence_count(); ++slot) {
                    const StableOccurrenceKey key{grade, arena.occurrence_identities[slot]};
                    guard.step();
                    const auto coefficient = source.source().incidence_coefficient(grade, slot);
                    guard.value(coefficient);
                    if (dm.contains(key)) {
                        const auto& t = *dm.at(key);
                        auto matrix = temporal_tensor_matrix(t);
                        guard.matrix(matrix);
                        retained_entries=machine::checked_size_add(retained_entries,matrix.nonzeros()); guard.entries(retained_entries);
                        if (t.role == TemporalTensorRole::CoefficientIndependentMap) {
                            // Phi is retained unmodified in the endpoint. Native overrides
                            // are complete realized maps, so multiply exactly once here.
                            numeric::ExactMatrix realized(matrix.rows(), matrix.cols());
                            for (const auto& [ij,v] : matrix.entries()) realized.set(ij.first,ij.second,guard.mul(coefficient,v));
                            df.set_transport(grade,slot,std::move(realized));
                        } else {
                            if (contract.downward_profile == DeclaredDownwardProfile::CoefficientFactored && coefficient == 0)
                                require(matrix.entries().empty(), "nonzero realized map cannot factor through a zero coefficient");
                            df.set_transport(grade,slot,std::move(matrix));
                        }
                    } else identity(contract.downward,down,grade,slot);
                    if (um.contains(key)) {
                        auto matrix=temporal_tensor_matrix(*um.at(key)); guard.matrix(matrix);
                        retained_entries=machine::checked_size_add(retained_entries,matrix.nonzeros()); guard.entries(retained_entries);
                        uf.set_restriction(grade,slot,std::move(matrix));
                    }
                    else identity(contract.upward,up,grade,slot);
                }
            }
            fibers.emplace(source,df);
            sheaf.emplace(source,uf);
        }
        template<class CarrierIndex> Family family(const DeclaredLocalFamily& f, const CarrierIndex& cs) const {
            Family result;
            const auto& registry=retained.declarations->registry;
            std::set<std::uint64_t> ids;
            for (const auto& b : f.stalks) {
                const auto& c=b.carrier; const auto& d=b.declaration;
                require(cs.contains(c.key) && *cs.at(c.key)==c, "stalk carrier identity/domain/coordinate system differs from contract");
                require(c.kind==TemporalCarrierKind::CellStalk && c.cell && index.cell(*c.cell), "binding needs an actual source cell stalk");
                require(registry.at(d.id)==d, "stalk definition or coordinate order differs from contract");
                require(d.kind==composite::DeclarationKind::RationalTensor && d.axes.size()==1 &&
                        d.axes.front().extent>0 && composite::DeclarationRegistry::tensor_domain(d)==c.domain,
                        "initial stalk profile requires a positive one-axis rational tensor");
                require(ids.insert(c.key).second && result.emplace(*c.cell,b).second, "duplicate selected stalk carrier/cell");
            }
            std::size_t count=0;
            for (std::size_t k=0; k<source.source().grade_count(); ++k) {
                const core::Grade g{static_cast<std::uint16_t>(k)};
                for (std::size_t i=0; i<source.source().grade(g).cell_count(); ++i) {
                    ++count;
                    require(result.contains(stable_cell_key(source.source(),{g,static_cast<std::uint32_t>(i)})),
                            "every source cell needs a stalk in each selected family");
                }
            }
            require(result.size()==count,"foreign selected stalk");
            return result;
        }
        template<class TensorIndex> Maps maps(const DeclaredLocalFamily& f, const Family& cells,
                                               const TensorIndex& ts, bool upward) const {
            Maps result;
            for (const auto& b : f.maps) {
                require(ts.contains(b.tensor), "selected occurrence map is absent");
                const auto& t=*ts.at(b.tensor);
                require(t.role==b.role && (upward ? t.role==TemporalTensorRole::SheafRestriction :
                    t.role==TemporalTensorRole::CoefficientIndependentMap || t.role==TemporalTensorRole::RealizedIncidenceMap),
                    "selected map role/direction differs from contract");
                require(t.owner.kind==TemporalOwnerKind::Occurrence && index.occurrence(t.owner.occurrence), "map has no retained occurrence owner");
                const auto g=t.owner.occurrence.upper_grade;
                const auto p=*index.occurrence(t.owner.occurrence);
                const auto& arena=source.source().grade(g);
                const auto upper=stable_cell_key(source.source(),{g,arena.upper_index[p]});
                const auto lower=stable_cell_key(source.source(),{{static_cast<std::uint16_t>(g.value-1)},arena.lower_index[p]});
                const auto& in=cells.at(upward?lower:upper).carrier;
                const auto& out=cells.at(upward?upper:lower).carrier;
                require(t.input==in.key && t.output==out.key,"map does not use selected input/output coordinates");
                require(t.tensor.domain()==numeric::IndexDomain({out.domain.logical_size(),in.domain.logical_size()}),"map dimensions differ from stalks");
                require(t.tensor.support().count()==t.tensor.domain().logical_size(),"partial map support requires an explicit extension");
                require(result.emplace(t.owner.occurrence,&t).second,"duplicate selected occurrence map");
            }
            // A stored map on these coordinates may not silently disappear behind
            // an identity default just because it was omitted from the request.
            std::set<std::uint64_t> carriers;
            for (const auto& [key,b] : cells) { (void)key; carriers.insert(b.carrier.key); }
            for (const auto& [key,t] : ts) {
                (void)key;
                const bool relevant=upward ? t->role==TemporalTensorRole::SheafRestriction :
                    t->role==TemporalTensorRole::CoefficientIndependentMap || t->role==TemporalTensorRole::RealizedIncidenceMap;
                if (relevant && t->input && t->output && (carriers.contains(*t->input) || carriers.contains(*t->output)))
                    require(result.contains(t->owner.occurrence) && result.at(t->owner.occurrence)==t,"stored map omitted or foreign selected coordinates");
            }
            return result;
        }
        void identity(const DeclaredLocalFamily& f, const Family& cells, core::Grade g, std::size_t slot) const {
            require(f.allow_constant_identity,"retained occurrence needs an explicit map or a declared identity default");
            const auto& a=source.source().grade(g);
            const auto& u=cells.at(stable_cell_key(source.source(),{g,a.upper_index[slot]}));
            const auto& l=cells.at(stable_cell_key(source.source(),{{static_cast<std::uint16_t>(g.value-1)},a.lower_index[slot]}));
            require(u.declaration==l.declaration && u.carrier.coordinate_system==l.carrier.coordinate_system &&
                    u.carrier.domain==l.carrier.domain,"identity default requires identical declared coordinates, not equal dimensions");
        }
    };
    std::shared_ptr<const State> state_;
    field::SparseStalkValues inputs(const Family& family, core::Grade grade,
                                    const std::vector<DeclaredFieldInput>& requested) const {
        require(grade.value < state_->source.source().grade_count(),"input grade absent");
        const auto ts=temporal_endpoint_detail::tensors(state_->retained);
        const auto vs=temporal_declaration_detail::keyed(state_->retained.declarations->values);
        field::SparseStalkValues values;
        std::set<std::uint64_t> used;
        for (const auto& b : requested) {
            require(ts.contains(b.tensor) && vs.contains(b.value),"field tensor or typed value missing");
            const auto& t=*ts.at(b.tensor); const auto& v=*vs.at(b.value);
            require(t.role==TemporalTensorRole::Field && t.owner.kind==TemporalOwnerKind::Cell &&
                    t.owner.cell.grade==grade && family.contains(t.owner.cell),"wrong field role, grade or owner");
            const auto& stalk=family.at(t.owner.cell);
            require(!t.input && t.output==stalk.carrier.key && t.tensor.domain()==stalk.carrier.domain,"field uses different stalk coordinates");
            require(v.cell==t.owner.cell && !v.occurrence && !v.block && !v.branch && v.type==stalk.declaration.id,"typed value does not bind this field owner/type");
            const auto* tensor=std::get_if<numeric::SparseRationalTensor>(&v.value.data);
            require(tensor && temporal_endpoint_detail::tensor_equal(t.tensor,*tensor),"typed value and field data/support disagree");
            require(t.tensor.support().count()==t.tensor.domain().logical_size(),"partial input does not authorize total field execution");
            require(used.insert(b.value).second && values.emplace(state_->index.cell(t.owner.cell)->index,t.tensor.values()).second,"duplicate input owner or typed value");
        }
        require(values.size()==state_->source.source().grade(grade).cell_count(),"every input stalk needs a supplied total field record, including zero");
        return values;
    }
    DeclaredFieldRecords records(const Family& family, core::Grade grade, const field::SparseStalkValues& values,
                                 std::uint64_t tensor_key, std::uint64_t value_key) const {
        const auto n=state_->source.source().grade(grade).cell_count();
        require(tensor_key && value_key && n<=std::numeric_limits<std::uint64_t>::max()-tensor_key &&
                n<=std::numeric_limits<std::uint64_t>::max()-value_key,"output record key range invalid");
        DeclaredFieldRecords result;
        for (std::size_t i=0;i<n;++i) {
            const auto key=stable_cell_key(state_->source.source(),{grade,static_cast<std::uint32_t>(i)});
            const auto& b=family.at(key); const auto size=b.carrier.domain.logical_size();
            TemporalTensor t; t.key=tensor_key+i; t.role=TemporalTensorRole::Field;
            t.owner.kind=TemporalOwnerKind::Cell; t.owner.cell=key; t.output=b.carrier.key;
            t.tensor=numeric::SparseRationalTensor(b.carrier.domain,numeric::StructuralSupport::full(size),
                values.contains(static_cast<std::uint32_t>(i)) ? values.at(static_cast<std::uint32_t>(i)) : S(size));
            TemporalDeclaredValue v; v.key=value_key+i; v.type=b.declaration.id; v.cell=key;
            v.value=composite::DeclaredValue::tensor(t.tensor);
            result.tensors.push_back(std::move(t)); result.values.push_back(std::move(v));
        }
        return result;
    }
public:
    // Full existing endpoint validation precedes snapshot publication. Decode raw
    // bytes with the existing bounded codecs first. No pointer/owner is restored.
    static numeric::ExactActionLimits default_limits() {
        numeric::ExactActionLimits limits; limits.dimension=std::numeric_limits<std::size_t>::max(); return limits;
    }
    explicit DeclaredLocalFieldBinding(const TemporalEndpoint& endpoint, DeclaredLocalFieldContract contract,
                                        numeric::ExactActionLimits limits=default_limits()) {
        require(endpoint.declarations.has_value(),"typed local binding needs declarations");
        require(contract.downward_profile==DeclaredDownwardProfile::CoefficientFactored ||
                contract.downward_profile==DeclaredDownwardProfile::CompleteRealizedOverrides,"unknown downward admission profile");
        validate_temporal_endpoint(endpoint);
        state_=std::make_shared<const State>(endpoint,std::move(contract),limits);
    }
    const core::OwnedRelationalSource& source_owner() const & noexcept { return state_->source; }
    const core::OwnedRelationalSource& source_owner() const && = delete;
    const local::OwnedLinearFiberSystem& fibers() const & noexcept { return *state_->fibers; }
    const local::OwnedLinearFiberSystem& fibers() const && = delete;
    const local::OwnedLinearCellularSheaf& sheaf() const & noexcept { return *state_->sheaf; }
    const local::OwnedLinearCellularSheaf& sheaf() const && = delete;
    const TemporalEndpoint& endpoint() const & noexcept { return state_->retained; }
    const TemporalEndpoint& endpoint() const && = delete;
    bool same_owner(const DeclaredLocalFieldBinding& other) const noexcept { return state_==other.state_; }
    const DeclaredLocalFieldContract& contract() const & noexcept { return state_->contract; }
    const DeclaredLocalFieldContract& contract() const && = delete;
    field::SparseStalkField bind_downward(core::Grade grade, const std::vector<DeclaredFieldInput>& requested,
                                         numeric::ExactActionLimits limits=default_limits()) const {
        return field::SparseStalkFieldActions(fibers(),{},limits).bind(grade,inputs(state_->down,grade,requested));
    }
    local::SparseSheafCochain bind_upward(core::Grade grade, const std::vector<DeclaredFieldInput>& requested,
                                         numeric::ExactActionLimits limits=default_limits()) const {
        return local::SparseSheafActions(sheaf(),limits).bind(grade,inputs(state_->up,grade,requested));
    }
    DeclaredFieldRecords export_downward(const field::SparseStalkField& value,
                                         std::uint64_t first_tensor, std::uint64_t first_value) const {
        require(value.owner().same_owner(fibers()),"foreign field owner in export");
        return records(state_->down,value.grade(),value.values(),first_tensor,first_value);
    }
    DeclaredFieldRecords export_upward(const local::SparseSheafCochain& value,
                                       std::uint64_t first_tensor, std::uint64_t first_value) const {
        require(value.owner().same_owner(sheaf()),"foreign sheaf owner in export");
        return records(state_->up,value.grade(),value.values(),first_tensor,first_value);
    }
};
} // namespace rfc::storage
