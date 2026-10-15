#pragma once
#include "rfc/algebra/owned_tensor_product.hpp"
#include "rfc/storage/native_accession.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <array>

namespace rfc::inference {
struct NativeProductAvailability {
    runtime::WorkAvailability left, right;
};
struct NativeTensorMapAvailability { NativeProductAvailability source, target; };
struct NativeTensorProductRequest {
    storage::PinnedNativeAccession left, right;
    std::uint64_t left_factor{}, right_factor{};
    algebra::RelationalProductDeclaration declaration;
    std::size_t factor_dimension_limit{64};
};
enum class NativeProductAction { DeclaredField, Boundary, TensorChainMap };
struct NativeProductProvenance {
    NativeProductAction action{NativeProductAction::DeclaredField};
    std::uint64_t declaration{}, revision{}, left_factor{}, right_factor{};
    std::uint64_t left_namespace{}, right_namespace{}, left_sequence{}, right_sequence{};
    NativeProductAvailability preparation_availability, execution_availability;
    runtime::ExecutionRequirements requirements;
    numeric::ExactActionLimits action_limits;
    std::size_t preparation_steps{}, execution_steps{};
    std::string preparation_policy, execution_policy;
    bool fallback{};
};
namespace native_product_detail {
using Pin = storage::PinnedNativeAccession;
using Guard = numeric::ExactActionGuard;
using algebra::owned_product_detail::add;
using algebra::owned_product_detail::mul;
using algebra::owned_product_detail::require;
inline void available(const NativeTensorProductRequest& r, NativeProductAvailability a) {
    runtime::admit_work_availability(r.left.source_namespace(), r.left.sequence(), a.left);
    runtime::admit_work_availability(r.right.source_namespace(), r.right.sequence(), a.right);
}
inline numeric::ExactActionLimits limits(const runtime::ExecutionLease& lease, std::span<const Pin> pins,
                                         std::size_t dimension) {
    const auto& cap = lease.policy().limits;
    auto work = cap.scalar_work, bits = cap.integer_bits;
    for (const auto& pin : pins) {
        work = std::min(work, pin.limits().max_action_work);
        bits = std::min(bits, mul(pin.limits().max_integer_bytes, 8));
    }
    return {dimension, std::min(cap.rational_slots, cap.sparse_entries), work, bits};
}
inline runtime::ExecutionRequirements requirements(std::size_t words, std::size_t envelope) {
    return {words, envelope, envelope, 0, 0};
}
struct ProductState {
    NativeTensorProductRequest request;
    algebra::OwnedRelationalTensorProduct product;
    NativeProductAvailability availability;
    std::size_t preparation_steps{};
    std::string policy;
    bool fallback{};
};
struct MapState;
} // namespace native_product_detail

class DerivedNativeProductField;
class PreparedNativeTensorChainMap;
class PreparedNativeTensorProduct {
public:
    using Values = algebra::OwnedRelationalTensorProduct::Values;
    static PreparedNativeTensorProduct prepare(NativeTensorProductRequest request, NativeProductAvailability available,
                                                runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_product_detail;
        lease.check_cancelled(); native_product_detail::available(request, available);
        require(request.left_factor && request.right_factor && request.left_factor != request.right_factor,
                "native product needs two distinct declared factor identities");
        require(request.factor_dimension_limit > 0, "native product factor dimension limit must be positive");
        for (const auto* p : {&request.left, &request.right})
            require(p->kind() == storage::NativeAccessionKind::Chain && p->support_count() == 0,
                    "native product source anchor must be an empty owning Chain pin");
        const auto a = request.left.endpoint(), b = request.right.endpoint();
        auto words = add(algebra::owned_product_detail::source_words(a->source), algebra::owned_product_detail::source_words(b->source));
        words = add(words, mul(request.declaration.types.size(), 3));
        const auto envelope = add(1, mul(words, 8));
        runtime::admit_execution(requirements(words, envelope), lease.policy());
        const std::array<Pin, 2> pins{request.left, request.right};
        Guard guard(limits(lease, pins, request.factor_dimension_limit), [&lease] { lease.check_cancelled(); });
        auto product = algebra::OwnedRelationalTensorProduct::prepare(a->source, b->source, request.declaration, guard);
        lease.check_cancelled();
        return PreparedNativeTensorProduct(std::make_shared<const native_product_detail::ProductState>(
            native_product_detail::ProductState{std::move(request), std::move(product), available,
                guard.steps(), lease.policy().name, lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    algebra::OwnedRelationalTensorProduct product() const { return state_->product; }
    storage::PinnedNativeAccession factor(bool right) const { return right ? state_->request.right : state_->request.left; }
    NativeProductAvailability preparation_availability() const { return state_->availability; }
    bool same_owner(const PreparedNativeTensorProduct& other) const noexcept { return state_ == other.state_; }
    DerivedNativeProductField field(core::Grade grade, Values values, NativeProductAvailability available,
                                     runtime::ExecutionLease lease = runtime::ExecutionLease{}) const;
    DerivedNativeProductField boundary(DerivedNativeProductField input, NativeProductAvailability available,
                                        runtime::ExecutionLease lease = runtime::ExecutionLease{}) const;
private:
    friend class PreparedNativeTensorChainMap;
    friend class DerivedNativeProductField;
    explicit PreparedNativeTensorProduct(std::shared_ptr<const native_product_detail::ProductState> state) : state_(std::move(state)) {}
    std::shared_ptr<const native_product_detail::ProductState> state_;
};
class DerivedNativeProductField {
public:
    const algebra::OwnedProductField& field() const & { return field_; }
    const algebra::OwnedProductField& field() const && = delete;
    const NativeProductProvenance& provenance() const & { return report_; }
    const NativeProductProvenance& provenance() const && = delete;
    PreparedNativeTensorProduct product() const { return owner_; }
    std::shared_ptr<const DerivedNativeProductField> input() const { return input_; }
    std::optional<PreparedNativeTensorChainMap> chain_map() const;
    std::optional<NativeTensorMapAvailability> map_execution_availability() const { return map_availability_; }
private:
    friend class PreparedNativeTensorProduct;
    friend class PreparedNativeTensorChainMap;
    DerivedNativeProductField(PreparedNativeTensorProduct owner, algebra::OwnedProductField field, NativeProductProvenance report,
                              std::shared_ptr<const DerivedNativeProductField> input = {},
                              std::shared_ptr<const native_product_detail::MapState> map = {},
                              std::optional<NativeTensorMapAvailability> map_availability = {})
        : owner_(std::move(owner)), field_(std::move(field)), report_(std::move(report)), input_(std::move(input)),
          map_(std::move(map)), map_availability_(map_availability) {}
    PreparedNativeTensorProduct owner_;
    algebra::OwnedProductField field_;
    NativeProductProvenance report_;
    std::shared_ptr<const DerivedNativeProductField> input_;
    std::shared_ptr<const native_product_detail::MapState> map_;
    std::optional<NativeTensorMapAvailability> map_availability_;
};
namespace native_product_detail {
inline NativeProductProvenance report(const ProductState& s, NativeProductAction action, NativeProductAvailability a,
                                       const runtime::ExecutionLease& lease, Guard& guard, runtime::ExecutionRequirements required) {
    const auto& r = s.request;
    return {action, r.declaration.key, r.declaration.revision, r.left_factor, r.right_factor,
            r.left.source_namespace(), r.right.source_namespace(), r.left.sequence(), r.right.sequence(),
            s.availability, a, required, guard.limits(), s.preparation_steps, guard.steps(), s.policy, lease.policy().name,
            s.fallback || lease.policy().backend != runtime::ExecutionBackend::ExactCpu};
}
inline void revalidate(const ProductState& s, NativeProductAvailability a) {
    available(s.request, s.availability); available(s.request, a);
}
} // namespace native_product_detail
inline DerivedNativeProductField PreparedNativeTensorProduct::field(core::Grade grade, Values values, NativeProductAvailability available,
                                                                    runtime::ExecutionLease lease) const {
    using namespace native_product_detail;
    const auto pin = state_; lease.check_cancelled(); revalidate(*pin, available);
    const auto required = requirements(pin->product.retained_words(), add(pin->product.entry_envelope(), mul(values.size(), 2)));
    runtime::admit_execution(required, lease.policy());
    const std::array<Pin, 2> pins{pin->request.left, pin->request.right};
    Guard guard(limits(lease, pins, pin->request.factor_dimension_limit), [&lease] { lease.check_cancelled(); });
    auto result = pin->product.field(grade, std::move(values), guard); lease.check_cancelled();
    return DerivedNativeProductField(PreparedNativeTensorProduct(pin), std::move(result),
        report(*pin, NativeProductAction::DeclaredField, available, lease, guard, required));
}
inline DerivedNativeProductField PreparedNativeTensorProduct::boundary(DerivedNativeProductField input, NativeProductAvailability available,
                                                                       runtime::ExecutionLease lease) const {
    using namespace native_product_detail;
    const auto pin = state_; lease.check_cancelled(); revalidate(*pin, available);
    require(pin == input.owner_.state_, "native product boundary requires its actual product field owner");
    const auto required = requirements(pin->product.retained_words(), add(pin->product.entry_envelope(), mul(input.field_.values().size(), 2)));
    runtime::admit_execution(required, lease.policy());
    const std::array<Pin, 2> pins{pin->request.left, pin->request.right};
    Guard guard(limits(lease, pins, pin->request.factor_dimension_limit), [&lease] { lease.check_cancelled(); });
    auto result = pin->product.boundary(input.field_, guard); lease.check_cancelled();
    return DerivedNativeProductField(PreparedNativeTensorProduct(pin), std::move(result),
        report(*pin, NativeProductAction::Boundary, available, lease, guard, required),
        std::make_shared<const DerivedNativeProductField>(std::move(input)));
}
namespace native_product_detail {
struct MapState {
    PreparedNativeTensorProduct source, target;
    algebra::PreparedTensorChainMap map;
    NativeTensorMapAvailability availability;
    std::size_t preparation_steps{};
    std::string policy;
    bool fallback{};
};
} // namespace native_product_detail
class PreparedNativeTensorChainMap {
public:
    static PreparedNativeTensorChainMap prepare(PreparedNativeTensorProduct source, PreparedNativeTensorProduct target,
                                                 std::vector<numeric::ExactMatrix> left, std::vector<numeric::ExactMatrix> right,
                                                 NativeTensorMapAvailability available,
                                                 runtime::ExecutionLease lease = runtime::ExecutionLease{}) {
        using namespace native_product_detail;
        lease.check_cancelled(); revalidate(*source.state_, available.source); revalidate(*target.state_, available.target);
        auto words = add(left.size(), right.size());
        for (const auto* maps : {&left, &right}) for (const auto& m : *maps) words = add(words, m.nonzeros());
        auto envelope = add(add(source.product().entry_envelope(), target.product().entry_envelope()), mul(words, 8));
        runtime::admit_execution(requirements(add(source.product().retained_words(), target.product().retained_words()), envelope), lease.policy());
        const std::array<Pin, 4> pins{source.factor(false), source.factor(true), target.factor(false), target.factor(true)};
        Guard guard(limits(lease, pins, std::min(source.state_->request.factor_dimension_limit, target.state_->request.factor_dimension_limit)),
                    [&lease] { lease.check_cancelled(); });
        auto map = algebra::PreparedTensorChainMap::prepare(source.product(), target.product(), std::move(left), std::move(right), guard);
        lease.check_cancelled();
        return PreparedNativeTensorChainMap(std::make_shared<const native_product_detail::MapState>(native_product_detail::MapState{
            std::move(source), std::move(target), std::move(map), available, guard.steps(), lease.policy().name,
            lease.policy().backend != runtime::ExecutionBackend::ExactCpu}));
    }
    PreparedNativeTensorProduct source() const { return state_->source; }
    PreparedNativeTensorProduct target() const { return state_->target; }
    const algebra::PreparedTensorChainMap& numeric_plan() const & { return state_->map; }
    algebra::PreparedTensorChainMap numeric_plan() const && { return state_->map; }
    NativeTensorMapAvailability preparation_availability() const { return state_->availability; }
    std::size_t preparation_steps() const noexcept { return state_->preparation_steps; }
    const std::string& preparation_policy() const & { return state_->policy; }
    const std::string& preparation_policy() const && = delete;
    DerivedNativeProductField execute(DerivedNativeProductField input, NativeTensorMapAvailability available,
                                       runtime::ExecutionLease lease = runtime::ExecutionLease{}) const {
        using namespace native_product_detail;
        const auto pin = state_; lease.check_cancelled();
        require(pin->source.same_owner(input.owner_), "native tensor map requires its actual product input owner");
        revalidate(*pin->source.state_, pin->availability.source); revalidate(*pin->target.state_, pin->availability.target);
        revalidate(*pin->source.state_, available.source); revalidate(*pin->target.state_, available.target);
        const auto required = requirements(add(pin->source.product().retained_words(), pin->target.product().retained_words()),
                                            add(pin->map.entry_envelope(), mul(input.field_.values().size(), 2)));
        runtime::admit_execution(required, lease.policy());
        const std::array<Pin, 4> pins{pin->source.factor(false), pin->source.factor(true), pin->target.factor(false), pin->target.factor(true)};
        Guard guard(limits(lease, pins, std::min(pin->source.state_->request.factor_dimension_limit, pin->target.state_->request.factor_dimension_limit)),
                    [&lease] { lease.check_cancelled(); });
        auto output = pin->map.apply(input.field_, guard); lease.check_cancelled();
        auto receipt = report(*pin->target.state_, NativeProductAction::TensorChainMap, available.target, lease, guard, required);
        receipt.fallback = receipt.fallback || pin->source.state_->fallback || pin->fallback;
        receipt.preparation_steps = pin->preparation_steps; receipt.preparation_policy = pin->policy;
        return DerivedNativeProductField(pin->target, std::move(output), std::move(receipt),
                                          std::make_shared<const DerivedNativeProductField>(std::move(input)), pin, available);
    }
private:
    friend class DerivedNativeProductField;
    explicit PreparedNativeTensorChainMap(std::shared_ptr<const native_product_detail::MapState> state) : state_(std::move(state)) {}
    std::shared_ptr<const native_product_detail::MapState> state_;
};
inline std::optional<PreparedNativeTensorChainMap> DerivedNativeProductField::chain_map() const {
    return map_ ? std::optional{PreparedNativeTensorChainMap(map_)} : std::nullopt;
}
} // namespace rfc::inference
