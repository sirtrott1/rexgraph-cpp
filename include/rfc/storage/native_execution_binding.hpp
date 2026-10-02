#pragma once

#include "rfc/storage/native_accession.hpp"
#include "rfc/storage/temporal_diagnostics.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include <limits>

namespace rfc::storage {

enum class NativeFieldDomain { Rational, Finite, Modular, Approximate };
enum class NativeFieldMaps { ExplicitOnly, DeclaredConstantFiber };
struct NativeFieldRealization {
    NativeFieldDomain domain{NativeFieldDomain::Rational};
    NativeFieldMaps maps{NativeFieldMaps::ExplicitOnly};
    std::uint64_t common_coordinates{};
    static NativeFieldRealization constant_fiber(std::uint64_t coordinates) {
        return {NativeFieldDomain::Rational, NativeFieldMaps::DeclaredConstantFiber, coordinates};
    }
};
struct NativeFieldRequest {
    core::Grade upper_grade{1};
    std::vector<std::uint64_t> stalk_carriers;
    std::vector<PinnedNativeAccession> fields;
};
struct NativeFieldCellBinding {
    StableCellKey cell;
    std::uint64_t carrier{}, coordinates{}, field{};
    std::size_t offset{}, dimension{};
};
struct NativeFieldOccurrenceBinding {
    StableOccurrenceKey occurrence;
    StableCellKey upper, lower;
    std::uint64_t input_carrier{}, output_carrier{};
    std::optional<std::uint64_t> map;
    TemporalTensorRole role{TemporalTensorRole::CoefficientIndependentMap};
    std::size_t source_offset{}, target_offset{};
    bool active{};
    numeric::Rational coefficient;
    // Dependency bits: field=1, carriers=2, map declaration=4,
    // source coefficient=8, retained existence=16. These are rebuild masks,
    // not mathematical identities or a cache key by themselves.
    std::uint32_t dependencies{1u|2u|4u|8u|16u};
};
struct NativeFieldRequirements {
    std::size_t records{}, rational_slots{}, sparse_entries{}, scalar_work{}, integer_bits{};
};

namespace native_execution_detail {
inline std::size_t integer_bits(const numeric::Rational& q) {
    const auto bits = [](mpz_srcptr z) { return mpz_sgn(z) == 0 ? std::size_t{0} : mpz_sizeinbase(z, 2); };
    return std::max(bits(mpq_numref(q.get_mpq_t())), bits(mpq_denref(q.get_mpq_t())));
}
inline std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a,b); }
inline std::size_t mul(std::size_t a, std::size_t b) { return machine::checked_size_mul(a,b); }
} // namespace native_execution_detail

// Inspection allocates metadata only. Its private construction prevents callers
// from forging a validated layout; the owning reference is built after admission.
class NativeFieldPlan {
public:
    NativeFieldPlan(const NativeFieldPlan&) = default;
    NativeFieldPlan& operator=(const NativeFieldPlan&) = default;
    static NativeFieldPlan inspect(const PinnedNativeAccession& publication,
                                  const NativeFieldRequest& request,
                                  const NativeFieldRealization& realization = {}) {
        using namespace temporal_endpoint_detail;
        using native_execution_detail::add;
        using native_execution_detail::mul;
        using native_execution_detail::integer_bits;
        require(realization.domain == NativeFieldDomain::Rational, "field reference admits only declared rational arithmetic");
        require(realization.maps == NativeFieldMaps::ExplicitOnly || realization.maps == NativeFieldMaps::DeclaredConstantFiber,
                "unknown field map realization");
        require(realization.maps == NativeFieldMaps::DeclaredConstantFiber ? realization.common_coordinates != 0 : realization.common_coordinates == 0,
                "constant-fiber coordinate declaration mismatch");
        NativeFieldPlan out(publication, realization, request.upper_grade);
        const auto& e = *out.endpoint_;
        // Authentic accession pins were validated and independently frozen by E/F3.
        require(request.upper_grade.value > 0 && request.upper_grade.value < e.source.grade_count(), "field action requires a retained positive grade");
        const auto cs = carriers(e);
        const auto ts = tensors(e);
        std::map<StableCellKey, const TemporalCarrier*> selected;
        for (auto key : request.stalk_carriers) {
            require(cs.contains(key) && cs.at(key)->kind == TemporalCarrierKind::CellStalk, "selected carrier is not a cell stalk");
            const auto* c = cs.at(key);
            require(c->domain.extents().size() == 1 && c->domain.logical_size() > 0, "reference stalk requires a positive rank-one rational domain");
            require(selected.emplace(*c->cell,c).second, "duplicate selected stalk owner");
        }
        std::vector<std::vector<NativeFieldCellBinding>> grades(e.source.grade_count());
        std::size_t cell_count = 0;
        for (std::size_t k=0; k<grades.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = e.source.grade(grade);
            std::size_t offset = 0;
            for (std::size_t i=0; i<arena.cell_count(); ++i) {
                require(i <= std::numeric_limits<std::uint32_t>::max(), "cell arena index exceeds native width");
                const auto cell = stable_cell_key(e.source,{grade,static_cast<std::uint32_t>(i)});
                require(selected.contains(cell), "linear realization requires one selected stalk for every retained cell");
                const auto& c = *selected.at(cell);
                grades[k].push_back({cell,c.key,c.coordinate_system,0,offset,c.domain.logical_size()});
                offset = add(offset,c.domain.logical_size());
                out.selected_.push_back(c.key);
            }
            cell_count = add(cell_count,arena.cell_count());
            if (k == request.upper_grade.value) out.source_dimension_ = offset;
            if (k+1 == request.upper_grade.value) out.target_dimension_ = offset;
        }
        require(selected.size() == cell_count, "foreign selected stalk owner");
        out.inputs_ = grades[request.upper_grade.value];
        out.outputs_ = grades[request.upper_grade.value-1u];
        std::map<StableCellKey, const TemporalTensor*> fields;
        std::size_t value_bits = 1;
        for (const auto& pin : request.fields) {
            require(pin.endpoint().get() == out.endpoint_.get() && pin.source_namespace() == publication.source_namespace(),
                    "field pin belongs to a different publication");
            require(pin.kind() == NativeAccessionKind::Field, "input must be a declared rational field accession");
            const auto t = pin.tensor_declaration();
            require(t->role == TemporalTensorRole::Field && t->owner.kind == TemporalOwnerKind::Cell &&
                    t->owner.cell.grade == request.upper_grade, "field input has the wrong cell owner or grade");
            require(selected.contains(t->owner.cell) && t->output == selected.at(t->owner.cell)->key, "field input carrier identity mismatch");
            require(t->tensor.support().count() == t->tensor.domain().logical_size(), "partial field support requires an explicit extension");
            require(fields.emplace(t->owner.cell,t.get()).second, "duplicate upper field owner");
            for (const auto& v : t->tensor.values().entries()) value_bits = std::max(value_bits,integer_bits(v.value));
        }
        require(fields.size() == out.inputs_.size(), "complete upper field ownership is required");
        for (auto& input : out.inputs_) {
            require(fields.contains(input.cell), "upper field owner missing");
            input.field = fields.at(input.cell)->key;
        }
        std::map<StableOccurrenceKey, const TemporalTensor*> maps;
        for (const auto& [key,t] : ts) {
            (void)key;
            if (t->role != TemporalTensorRole::CoefficientIndependentMap && t->role != TemporalTensorRole::RealizedIncidenceMap) continue;
            require(selected.at(*cs.at(*t->input)->cell)->key == *t->input && selected.at(*cs.at(*t->output)->cell)->key == *t->output,
                    "incidence map belongs to a different selected stalk realization");
            require(t->tensor.support().count() == t->tensor.domain().logical_size(), "incidence map has undeclared coordinates");
            require(maps.emplace(t->owner.occurrence,t).second, "duplicate downward occurrence map");
        }
        std::size_t map_entries = 0, action_entries = 0, occurrence_dimension = 0, retained = 0, map_bits = 1, default_map_coordinates = 0;
        for (std::size_t k=1; k<grades.size(); ++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena = e.source.grade(grade);
            for (std::size_t slot=0; slot<arena.incidence_count(); ++slot) {
                const auto& upper = grades[k].at(arena.upper_index[slot]);
                const auto& lower = grades[k-1].at(arena.lower_index[slot]);
                const StableOccurrenceKey occurrence{grade,arena.occurrence_identities[slot]};
                const auto coefficient = e.source.incidence_coefficient(grade,slot);
                const bool active = arena.aspects.get(e.source.signature().existence_aspect(),slot);
                NativeFieldOccurrenceBinding b{occurrence,upper.cell,lower.cell,upper.carrier,lower.carrier,{},
                    TemporalTensorRole::CoefficientIndependentMap,upper.offset,lower.offset,active,coefficient};
                std::size_t entries = 0, bits = integer_bits(coefficient);
                if (maps.contains(occurrence)) {
                    const auto& t = *maps.at(occurrence);
                    b.map = t.key; b.role = t.role; entries = t.tensor.values().nonzeros();
                    if(t.role == TemporalTensorRole::RealizedIncidenceMap) b.dependencies &= ~8u;
                    for (const auto& v : t.tensor.values().entries()) {
                        const auto n = integer_bits(v.value);
                        bits = std::max(bits,t.role == TemporalTensorRole::CoefficientIndependentMap ? add(n,integer_bits(coefficient)) : n);
                    }
                } else {
                    require(realization.maps == NativeFieldMaps::DeclaredConstantFiber, "every retained occurrence needs an explicit map or named constant-fiber constructor");
                    require(upper.coordinates == realization.common_coordinates && lower.coordinates == realization.common_coordinates &&
                            cs.at(upper.carrier)->domain == cs.at(lower.carrier)->domain,
                            "constant-fiber map requires identical declared coordinates and domains");
                    entries = coefficient == 0 ? 0 : lower.dimension;
                    default_map_coordinates = add(default_map_coordinates,lower.dimension);
                }
                map_entries = add(map_entries,entries);
                map_bits = std::max(map_bits,bits);
                retained = add(retained,1);
                if (k == request.upper_grade.value) {
                    out.occurrences_.push_back(std::move(b));
                    if (active) {
                        action_entries = add(action_entries,entries);
                        occurrence_dimension = add(occurrence_dimension,lower.dimension);
                    }
                }
            }
        }
        // A product needs at most Bx+BT bits per integer. For N rational
        // contributions, the common-denominator sum needs at most N*B+ceil(log2 N).
        // Include extra room for unnormalised pairwise addition intermediates.
        const auto product_bits = add(value_bits,map_bits);
        const auto terms = std::max(std::size_t{1},action_entries);
        std::size_t logarithm = 0;
        for (auto n=terms; n>1; n=n/2+n%2) ++logarithm;
        const auto growth = add(mul(add(terms,2),product_bits),add(logarithm,2));
        out.requirements_.integer_bits = std::max({growth,map_bits,value_bits});
        out.requirements_.records = add(add(cell_count,retained),request.fields.size());
        // Conservative aggregate of stored/copy/temporary reference values,
        // including relation-local maps and the sparse assembly matrix.
        out.requirements_.rational_slots = add(add(mul(out.source_dimension_,3),mul(out.target_dimension_,3)),
                                             add(mul(occurrence_dimension,4),mul(map_entries,4)));
        out.requirements_.sparse_entries = add(mul(map_entries,4),occurrence_dimension);
        out.requirements_.scalar_work = add(add(mul(map_entries,4),mul(occurrence_dimension,4)),
                                          add(add(out.source_dimension_,out.target_dimension_),default_map_coordinates));
        // Keep the inspectable reference alongside the compiled action. Charge
        // its additional blocks, entry schedule, reusable scratch and lowering
        // work rather than reporting the optimized execution alone as the cost
        // of the complete prepared owner. These remain conservative envelopes.
        out.requirements_.records = add(out.requirements_.records,add(out.occurrences_.size(),mul(add(out.target_dimension_,1),2)));
        out.requirements_.rational_slots = add(out.requirements_.rational_slots,add(action_entries,add(out.target_dimension_,1)));
        out.requirements_.sparse_entries = add(out.requirements_.sparse_entries,mul(action_entries,2));
        out.requirements_.scalar_work = add(out.requirements_.scalar_work,add(mul(action_entries,6),add(mul(out.target_dimension_,2),out.occurrences_.size())));
        return out;
    }
    const NativeFieldRequirements& requirements() const & noexcept { return requirements_; }
    const auto& inputs() const & noexcept { return inputs_; }
    const auto& outputs() const & noexcept { return outputs_; }
    const auto& occurrences() const & noexcept { return occurrences_; }
    const NativeFieldRealization& realization() const & noexcept { return realization_; }
    const NativeFieldRequirements& requirements() const && = delete;
    const std::vector<NativeFieldCellBinding>& inputs() const && = delete;
    const std::vector<NativeFieldCellBinding>& outputs() const && = delete;
    const std::vector<NativeFieldOccurrenceBinding>& occurrences() const && = delete;
    const NativeFieldRealization& realization() const && = delete;
    core::Grade upper_grade() const noexcept { return grade_; }
    std::shared_ptr<const TemporalEndpoint> endpoint() const { return endpoint_; }
    std::uint64_t source_namespace() const noexcept { return publication_.source_namespace(); }
    NativeAccessionLimits accession_limits() const noexcept { return publication_.limits(); }
    std::size_t source_dimension() const noexcept { return source_dimension_; }
    std::size_t target_dimension() const noexcept { return target_dimension_; }
    const std::vector<std::uint64_t>& selected_carriers() const & noexcept { return selected_; }
    const std::vector<std::uint64_t>& selected_carriers() const && = delete;
private:
    NativeFieldPlan(const PinnedNativeAccession& pin, NativeFieldRealization r, core::Grade grade)
        : publication_(pin), endpoint_(pin.endpoint()), realization_(r), grade_(grade) {}
    PinnedNativeAccession publication_;
    std::shared_ptr<const TemporalEndpoint> endpoint_;
    NativeFieldRealization realization_;
    core::Grade grade_;
    NativeFieldRequirements requirements_;
    std::vector<std::uint64_t> selected_;
    std::vector<NativeFieldCellBinding> inputs_, outputs_;
    std::vector<NativeFieldOccurrenceBinding> occurrences_;
    std::size_t source_dimension_{}, target_dimension_{};
    friend class NativeFieldBinding;
};

class NativeFieldBinding {
    struct State;
public:
    class Workspace {
    public:
        Workspace(const Workspace&) = delete;
        Workspace& operator=(const Workspace&) = delete;
        Workspace(Workspace&&) noexcept = default;
        Workspace& operator=(Workspace&&) noexcept = default;
        std::size_t scratch_slots() const noexcept { return scratch_.scratch_slots(); }
    private:
        Workspace(std::shared_ptr<const State> owner, local::PreparedGradeFieldAction::Workspace scratch)
            : owner_(std::move(owner)), scratch_(std::move(scratch)) {}
        std::shared_ptr<const State> owner_;
        local::PreparedGradeFieldAction::Workspace scratch_;
        friend class NativeFieldBinding;
    };
    NativeFieldBinding(const NativeFieldBinding&) = default;
    NativeFieldBinding& operator=(const NativeFieldBinding&) = default;
    explicit NativeFieldBinding(const NativeFieldPlan& plan) : state_(std::make_shared<const State>(plan)) {}
    const NativeFieldPlan& plan() const & noexcept { return state_->plan; }
    const NativeFieldPlan& plan() const && = delete;
    numeric::RationalVector apply_reference() const { return state_->field.apply(state_->input); }
    const local::PreparedGradeFieldAction& prepared_action() const & noexcept { return state_->prepared; }
    const local::PreparedGradeFieldAction& prepared_action() const && = delete;
    Workspace make_workspace() const { return Workspace(state_,state_->prepared.make_workspace()); }
    numeric::RationalVector apply_prepared(Workspace& workspace, std::size_t bit_limit) const {
        if(workspace.owner_.get()!=state_.get()) throw std::invalid_argument("workspace belongs to a different native field binding");
        return state_->prepared.apply(state_->input,workspace.scratch_,bit_limit);
    }
private:
    struct State {
        NativeFieldPlan plan;
        local::LinearRelationalFiberSystem fibers;
        local::GradeTensorField field;
        local::PreparedGradeFieldAction prepared;
        numeric::RationalVector input;
        explicit State(const NativeFieldPlan& p)
            : plan(p), fibers(temporal_linear_fiber_system(*plan.endpoint_,plan.selected_)),
              field(fibers,plan.grade_), prepared(fibers,plan.grade_), input(plan.source_dimension_) {
            const auto tensors = temporal_endpoint_detail::tensors(*plan.endpoint_);
            for (const auto& b : plan.inputs_)
                for (const auto& v : tensors.at(b.field)->tensor.values().entries()) input[b.offset+v.index] = v.value;
        }
        State(const State&) = delete;
        State(State&&) = delete;
    };
    std::shared_ptr<const State> state_;
};

} // namespace rfc::storage
