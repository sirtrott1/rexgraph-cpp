#pragma once

#include "rfc/database/native_field_cache.hpp"
#include <span>

namespace rfc::database {
class NativeFieldVectorView;

// An intrinsic stalk value and its declared coordinates remain owned together.
// Numerical spans are physical views; state() preserves structural absence.
class NativeStalkVectorView {
public:
    const storage::NativeFieldCellBinding& binding() const & noexcept { return binding_; }
    const numeric::IndexDomain& domain() const & noexcept { return *domain_; }
    const numeric::StructuralSupport& support() const & noexcept { return *support_; }
    const storage::NativeFieldCellBinding& binding() const && = delete;
    const numeric::IndexDomain& domain() const && = delete;
    const numeric::StructuralSupport& support() const && = delete;
    std::span<const numeric::Rational> numerical_values() const & {
        return std::span<const numeric::Rational>(*values_).subspan(binding_.offset,binding_.dimension);
    }
    std::span<const numeric::Rational> numerical_values() const && = delete;
    std::optional<numeric::Rational> state(std::size_t coordinate) const {
        if (coordinate>=binding_.dimension) throw std::out_of_range("native stalk coordinate");
        if (!support_->contains(coordinate)) return std::nullopt;
        return values_->at(binding_.offset+coordinate);
    }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const noexcept { return source_; }
    std::shared_ptr<const numeric::RationalVector> values_pin() const noexcept { return values_; }
private:
    NativeStalkVectorView(storage::NativeFieldCellBinding binding,
        std::shared_ptr<const numeric::RationalVector> values,
        std::shared_ptr<const storage::TemporalEndpoint> source,
        std::shared_ptr<const numeric::IndexDomain> domain,
        std::shared_ptr<const numeric::StructuralSupport> support)
        : binding_(binding),values_(std::move(values)),source_(std::move(source)),
          domain_(std::move(domain)),support_(std::move(support)) {}
    storage::NativeFieldCellBinding binding_;
    std::shared_ptr<const numeric::RationalVector> values_;
    std::shared_ptr<const storage::TemporalEndpoint> source_;
    std::shared_ptr<const numeric::IndexDomain> domain_;
    std::shared_ptr<const numeric::StructuralSupport> support_;
    friend class NativeFieldVectorView;
};

class NativeFieldVectorView {
    struct State {
        NativeFieldAnalysis analysis;
        std::shared_ptr<const numeric::RationalVector> values;
        std::vector<NativeStalkVectorView> stalks;
        std::map<storage::StableCellKey,std::size_t> index;
        bool input;
        State(NativeFieldAnalysis a,std::shared_ptr<const numeric::RationalVector> v,bool in)
            : analysis(std::move(a)),values(std::move(v)),input(in) {}
    };
public:
    static NativeFieldVectorView input(const PreparedParallelNativeField& operation) {
        return make(operation.analysis(),operation.input_pin(),true);
    }
    static NativeFieldVectorView output(const DerivedParallelNativeField& result) {
        return make(result.analysis(),result.values_pin(),false);
    }
    static NativeFieldVectorView output(const CachedNativeField& result) {
        return make(result.analysis(),result.values_pin(),false);
    }
    const NativeStalkVectorView& at_cell(storage::StableCellKey cell) const & {
        const auto& s=state(); const auto it=s.index.find(cell);
        if (it==s.index.end()) throw std::out_of_range("cell is outside native vector view");
        return s.stalks.at(it->second);
    }
    const NativeStalkVectorView& at_cell(storage::StableCellKey) const && = delete;
    const std::vector<NativeStalkVectorView>& stalks() const & { return state().stalks; }
    const std::vector<NativeStalkVectorView>& stalks() const && = delete;
    const NativeFieldAnalysis& analysis() const & { return state().analysis; }
    const NativeFieldAnalysis& analysis() const && = delete;
    bool is_input() const { return state().input; }
    std::shared_ptr<const numeric::RationalVector> values_pin() const { return state().values; }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return state().analysis.source_publication(); }
private:
    static NativeFieldVectorView make(const NativeFieldAnalysis& analysis,
        std::shared_ptr<const numeric::RationalVector> values,bool input) {
        const auto& plan=analysis.binding();
        if (!values || values->size()!=(input?plan.source_dimension():plan.target_dimension()))
            throw std::invalid_argument("sealed native vector dimension");
        auto state=std::make_shared<State>(analysis,std::move(values),input);
        const auto source=analysis.source_publication();
        const auto carriers=storage::temporal_endpoint_detail::carriers(*source);
        const auto tensors=storage::temporal_endpoint_detail::tensors(*source);
        const auto& bindings=input?plan.inputs():plan.outputs();
        state->stalks.reserve(bindings.size());
        for (const auto& binding:bindings) {
            std::shared_ptr<const numeric::IndexDomain> domain;
            std::shared_ptr<const numeric::StructuralSupport> support;
            if (input) {
                const auto& tensor=tensors.at(binding.field)->tensor;
                domain={source,&tensor.domain()}; support={source,&tensor.support()};
            } else {
                domain={source,&carriers.at(binding.carrier)->domain};
                support=std::make_shared<const numeric::StructuralSupport>(numeric::StructuralSupport::full(binding.dimension));
            }
            if (!state->index.emplace(binding.cell,state->stalks.size()).second)
                throw std::invalid_argument("native vector contains a duplicate stalk");
            state->stalks.push_back(NativeStalkVectorView(binding,state->values,source,std::move(domain),std::move(support)));
        }
        return NativeFieldVectorView(std::move(state));
    }
    explicit NativeFieldVectorView(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    const State& state() const {
        if (!state_) throw std::invalid_argument("moved-from native vector view");
        return *state_;
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::database
