#pragma once
#include "rfc/local/algebraic_module.hpp"
#include "rfc/local/tensor_field.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include <optional>

namespace rfc::local {
enum class AlgebraicChainPolicy : std::uint8_t { RequireFlat,DeclaredDeformation };
struct AlgebraicOccurrenceMap { core::Grade grade; std::size_t incidence_slot; AlgebraicModuleMap underlying; };
struct AlgebraicFiberLimits { std::size_t max_occurrences{65536},max_retained_map_coordinates{1000000}; };

// Phi is declared for every retained occurrence, including inactive and zero
// legs. Native T = retained coefficient * Phi; the source is never overwritten.
class AlgebraicRelationalFiberSystem {
    struct State {
        AlgebraicStalkBundle bundle;
        AlgebraicChainPolicy policy;
        std::vector<std::vector<std::optional<AlgebraicModuleMap>>> maps;
        LinearRelationalFiberSystem native;
        std::vector<numeric::ExactMatrix> boundaries,defects;
        std::vector<std::optional<PreparedGradeFieldAction>> actions;
        bool flat{true};
        State(const AlgebraicStalkBundle& declared,const std::vector<AlgebraicOccurrenceMap>& supplied,AlgebraicChainPolicy selected)
            : bundle(declared),policy(selected),native(make_bundle(bundle)) {
            const auto& source=bundle.source(); maps.resize(source.grade_count()); boundaries.resize(source.grade_count());
            defects.resize(source.grade_count()); actions.resize(source.grade_count());
            numeric::algebraic_detail::Work work{bundle.field().limits()};
            for (std::size_t k=1;k<source.grade_count();++k) {
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
                maps[k].resize(source.grade(grade).incidence_count());
            }
            for (const auto& supplied_map:supplied) {
                const auto k=supplied_map.grade.value;
                if (!k || k>=source.grade_count()) throw std::out_of_range("algebraic occurrence map grade");
                auto& slot=maps[k].at(supplied_map.incidence_slot);
                if (slot) throw std::invalid_argument("duplicate algebraic occurrence map");
                slot=supplied_map.underlying;
                auto realized=slot->rational_action().scale(source.incidence_coefficient(supplied_map.grade,supplied_map.incidence_slot));
                admit(realized,work);
                native.set_transport(supplied_map.grade,supplied_map.incidence_slot,std::move(realized));
            }
            for (std::size_t k=1;k<source.grade_count();++k) {
                for (const auto& map:maps[k]) if (!map) throw std::invalid_argument("missing retained algebraic occurrence map");
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
                boundaries[k]=GradeTensorField(native,grade).matrix(); admit(boundaries[k],work);
                if (k>=2) {
                    defects[k]=boundaries[k-1].multiply(boundaries[k]); admit(defects[k],work);
                    if (!defects[k].entries().empty()) flat=false;
                }
            }
            if (!flat && policy==AlgebraicChainPolicy::RequireFlat)
                throw std::invalid_argument("algebraic transports violate the complete realized chain family");
            for (std::size_t k=1;k<source.grade_count();++k)
                actions[k].emplace(native,core::Grade{static_cast<std::uint16_t>(k)});
        }
        static void admit(const numeric::ExactMatrix& matrix,numeric::algebraic_detail::Work& work) {
            for (const auto& [key,value]:matrix.entries()) { (void)key; work.admit(value); }
        }
        static LinearStalkBundle make_bundle(const AlgebraicStalkBundle& bundle) {
            LinearStalkBundle out(bundle.source()); const auto& source=bundle.source();
            for (std::size_t k=0;k<source.grade_count();++k) {
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
                for (std::size_t i=0;i<source.grade(grade).cell_count();++i) {
                    const core::CellId cell{grade,static_cast<std::uint32_t>(i)}; const auto module=bundle.stalk(cell);
                    out.set_dimension(cell,machine::checked_size_mul(module.rank(),bundle.field().degree()));
                }
            }
            return out;
        }
    };
public:
    AlgebraicRelationalFiberSystem(const AlgebraicRelationalFiberSystem&)=default;
    AlgebraicRelationalFiberSystem& operator=(const AlgebraicRelationalFiberSystem&)=default;
    static AlgebraicRelationalFiberSystem declare(const AlgebraicStalkBundle& bundle,
        const std::vector<AlgebraicOccurrenceMap>& maps,AlgebraicChainPolicy policy,AlgebraicFiberLimits limits={}) {
        if (policy!=AlgebraicChainPolicy::RequireFlat && policy!=AlgebraicChainPolicy::DeclaredDeformation)
            throw std::invalid_argument("unknown algebraic chain policy");
        const auto& source=bundle.source(); const auto d=bundle.field().degree();
        std::size_t occurrences{},slots{};
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            const auto& arena=source.grade(grade);
            occurrences=machine::checked_size_add(occurrences,arena.incidence_count());
            const auto rows=machine::checked_size_mul(bundle.offsets(core::Grade{static_cast<std::uint16_t>(k-1u)}).back(),d);
            const auto cols=machine::checked_size_mul(bundle.offsets(grade).back(),d);
            slots=machine::checked_size_add(slots,machine::checked_size_mul(rows,cols));
            if (k>=2) {
                const auto lower_rows=machine::checked_size_mul(bundle.offsets(core::Grade{static_cast<std::uint16_t>(k-2u)}).back(),d);
                slots=machine::checked_size_add(slots,machine::checked_size_mul(lower_rows,cols));
            }
        }
        if (occurrences>limits.max_occurrences) throw std::length_error("algebraic fiber occurrence budget");
        if (maps.size()!=occurrences) throw std::invalid_argument("algebraic fiber must declare every retained occurrence");
        // Admit actual carriers on the original declaration before constructing
        // native bundles, matrices or immutable execution state.
        for (const auto& map:maps) {
            const auto k=map.grade.value;
            if (!k || k>=source.grade_count()) throw std::out_of_range("algebraic occurrence map grade");
            const auto& arena=source.grade(map.grade);
            const auto upper=bundle.stalk({map.grade,arena.upper_index.at(map.incidence_slot)});
            const auto lower=bundle.stalk({core::Grade{static_cast<std::uint16_t>(k-1u)},arena.lower_index.at(map.incidence_slot)});
            if (!upper.same_owner(map.underlying.source()) || !lower.same_owner(map.underlying.target()))
                throw std::invalid_argument("algebraic occurrence map carrier owners differ");
            slots=machine::checked_size_add(slots,machine::checked_size_mul(map.underlying.rational_action().rows(),map.underlying.rational_action().cols()));
        }
        if (slots>limits.max_retained_map_coordinates) throw std::length_error("algebraic fiber matrix coordinate budget");
        return AlgebraicRelationalFiberSystem(std::make_shared<const State>(bundle,maps,policy));
    }
    const AlgebraicStalkBundle& bundle() const & noexcept { return state_->bundle; }
    const LinearRelationalFiberSystem& native_fibers() const & noexcept { return state_->native; }
    const AlgebraicModuleMap& occurrence_map(core::Grade grade,std::size_t slot) const & {
        if (!grade.value) throw std::out_of_range("algebraic occurrence requires a positive grade");
        return state_->maps.at(grade.value).at(slot).value();
    }
    const numeric::ExactMatrix& rational_boundary(core::Grade grade) const & {
        if (!grade.value) throw std::out_of_range("algebraic boundary requires a positive grade");
        return state_->boundaries.at(grade.value);
    }
    const numeric::ExactMatrix& rational_chain_defect(core::Grade grade) const & {
        if (grade.value<2) throw std::out_of_range("algebraic chain defect requires grade at least two");
        return state_->defects.at(grade.value);
    }
    const AlgebraicStalkBundle& bundle() const && = delete;
    const LinearRelationalFiberSystem& native_fibers() const && = delete;
    const AlgebraicModuleMap& occurrence_map(core::Grade,std::size_t) const && = delete;
    const numeric::ExactMatrix& rational_boundary(core::Grade) const && = delete;
    const numeric::ExactMatrix& rational_chain_defect(core::Grade) const && = delete;
    bool same_owner(const AlgebraicRelationalFiberSystem& other) const noexcept { return state_==other.state_; }
    bool flat() const noexcept { return state_->flat; }
    AlgebraicChainPolicy chain_policy() const noexcept { return state_->policy; }
    PreparedGradeFieldAction prepared_action(core::Grade grade) const {
        if (!grade.value) throw std::out_of_range("prepared algebraic boundary requires a positive grade");
        return state_->actions.at(grade.value).value();
    }
    std::vector<AlgebraicModuleVector> apply(core::Grade grade,const std::vector<AlgebraicModuleVector>& values) const {
        const auto& source=state_->bundle.source();
        if (!grade.value || grade.value>=source.grade_count()) throw std::out_of_range("algebraic field grade");
        if (values.size()!=source.grade(grade).cell_count()) throw std::invalid_argument("algebraic field must contain every source stalk");
        numeric::RationalVector input;
        for (std::size_t i=0;i<values.size();++i) {
            const auto module=state_->bundle.stalk({grade,static_cast<std::uint32_t>(i)});
            if (!module.same_owner(values[i].carrier())) throw std::invalid_argument("algebraic grade field carrier owner differs");
            const auto coordinates=values[i].rational_coordinates(); input.insert(input.end(),coordinates.begin(),coordinates.end());
        }
        const auto action=prepared_action(grade); auto workspace=action.make_workspace();
        const auto output=action.apply(input,workspace,state_->bundle.field().limits().max_integer_bits);
        const auto lower=core::Grade{static_cast<std::uint16_t>(grade.value-1u)};
        std::vector<AlgebraicModuleVector> result; result.reserve(source.grade(lower).cell_count());
        std::size_t offset{}; const auto d=state_->bundle.field().degree();
        for (std::size_t i=0;i<source.grade(lower).cell_count();++i) {
            const auto module=state_->bundle.stalk({lower,static_cast<std::uint32_t>(i)});
            std::vector<numeric::AlgebraicElement> coordinates; coordinates.reserve(module.rank());
            for (std::size_t j=0;j<module.rank();++j) {
                numeric::RationalVector value(d);
                for (std::size_t k=0;k<d;++k) value[k]=output[offset++];
                coordinates.push_back(state_->bundle.field().element(value));
            }
            result.push_back(module.vector(std::move(coordinates)));
        }
        return result;
    }
    std::vector<std::size_t> homology_dimensions_over_field() const {
        if (!flat()) throw std::logic_error("deformed nonflat fibers have no chain homology");
        const auto& source=state_->bundle.source(); std::vector<std::size_t> ranks(source.grade_count()+1,0),out(source.grade_count());
        const auto d=state_->bundle.field().degree();
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto rank=state_->boundaries[k].rank();
            if (rank%d) throw std::logic_error("algebraic boundary scalar restriction rank invariant");
            ranks[k]=rank/d;
        }
        for (std::size_t k=0;k<source.grade_count();++k) {
            const auto size=state_->bundle.offsets(core::Grade{static_cast<std::uint16_t>(k)}).back();
            if (ranks[k]>size || ranks[k+1]>size-ranks[k]) throw std::logic_error("algebraic homology dimension invariant");
            out[k]=size-ranks[k]-ranks[k+1];
        }
        return out;
    }
private:
    explicit AlgebraicRelationalFiberSystem(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::local
