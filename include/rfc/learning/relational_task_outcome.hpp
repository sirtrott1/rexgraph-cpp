#pragma once

#include "rfc/learning/relational_available_evidence_codec.hpp"
#include <variant>

namespace rfc::learning {
// The requested field is a prospective instruction in this declared basis.
struct RelationalTaskPostconditionDeclaration {
    std::uint64_t key{}, revision{}, use{}, origin{}, basis{};
    std::vector<std::uint64_t> directions;
    numeric::ExactMatrix metric;
    numeric::SparseRationalTensor target{numeric::IndexDomain({0}), numeric::StructuralSupport::full(0),
                                        numeric::SparseRationalVector(0, {})};
};
// Revision identifies this binding of worker, computational machine and
// operation to the complete program and its task observation interface.
struct RelationalWorkerBindingDeclaration {
    std::uint64_t key{}, revision{}, worker{}, machine{}, operation{}, origin{};
    inference::RelationalFieldInterfaceDeclaration interface;
    inference::RelationalFieldViewDeclaration view;
};
using RelationalTaskResponse = std::variant<inference::AvailableRelationalFieldResult,
                                           AvailableRelationalAffineResult>;
enum class RelationalTaskPostconditionStatus : std::uint8_t { Satisfied, Open, Refuted };
// Free coordinates retain the response evidence owner's ordered carrier.
// A feasible shift is a prospective restriction, never a new measurement.
struct RelationalTaskPostconditionCertificate {
    RelationalAffineField residual;
    numeric::RationalVector feasible_shift, contradiction;
    numeric::ExactMatrix feasible_kernel;
    std::size_t rank{};
    RelationalTaskPostconditionStatus status{RelationalTaskPostconditionStatus::Open};
};
namespace task_outcome_detail {
using namespace relational_evidence_detail;
using Bytes = std::vector<std::byte>;
using Context = inference::RelationalAvailabilityContext;
using Origin = inference::OwnedRelationalValueOrigin;
using Role = inference::RelationalDependencyRole;
inline constexpr std::array<std::byte, 8> goal_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'T'}, std::byte{'G'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr auto binding_magic = [] { auto m = goal_magic; m[4] = std::byte{'B'}; return m; }();
inline void task(inference::field_program_codec_detail::Writer& w, const RelationalTaskPostconditionDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.use); w.u64(d.basis);
    w.u64(d.directions.size()); for (const auto k : d.directions) w.u64(k);
    storage::temporal_codec_detail::matrix(w, d.metric);
    storage::temporal_codec_detail::tensor(w, d.target);
}
inline void binding(inference::field_program_codec_detail::Writer& w, const RelationalWorkerBindingDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.worker); w.u64(d.machine); w.u64(d.operation);
    w.u64(d.interface.key); w.u64(d.interface.branch); w.u64(d.interface.cells.size());
    for (const auto& c : d.interface.cells) {
        w.u64(c.node); inference::field_interface_codec_detail::local_cell(w, c.cell);
    }
    inference::field_interface_codec_detail::view(w, d.view);
}
inline inference::AvailableRelationalFieldResult input(const RelationalTaskResponse& response) {
    return std::visit([](const auto& r) -> inference::AvailableRelationalFieldResult {
        if constexpr (std::is_same_v<std::decay_t<decltype(r)>, AvailableRelationalAffineResult>) return r.input();
        else return r;
    }, response);
}
inline inference::OwnedRelationalFieldProgram program(const RelationalTaskResponse& r) {
    return input(r).result().result().program();
}
inline void admit(const RelationalTaskResponse& r, const Context& c, const std::function<void()>& check) {
    std::visit([&](const auto& value) { value.admit(c, check); }, r);
}
inline std::vector<Bytes> input_payloads(const inference::OwnedAccessionFieldResult& input) {
    std::vector<Bytes> out;
    for (const auto& value : input.inputs()) out.push_back(inference::relational_input_accession_payload(value));
    return out;
}
inline Bytes goal_payload(const RelationalTaskPostconditionDeclaration& d, std::span<const std::byte> asset,
                          const std::vector<Bytes>& inputs) {
    using namespace inference::field_program_codec_detail;
    auto w = envelope(goal_magic); task(w, d); blob(w, asset); w.u64(inputs.size());
    for (const auto& i : inputs) blob(w, i);
    return finish(std::move(w));
}
inline Bytes binding_payload(const RelationalWorkerBindingDeclaration& d, std::span<const std::byte> program) {
    using namespace inference::field_program_codec_detail;
    auto w = envelope(binding_magic); binding(w, d); blob(w, program); return finish(std::move(w));
}
inline RelationalTaskPostconditionCertificate solve(RelationalAffineField residual, Guard& guard) {
    RelationalTaskPostconditionCertificate out{std::move(residual), {}, {}, {}, 0,
                                              RelationalTaskPostconditionStatus::Open};
    auto r = out.residual.particular.values().dense(); const auto& a = out.residual.directions;
    guard.vector(r); guard.matrix(a); require(a.rows() == r.size(), "task residual field carrier");
    guard.dimension(add(a.cols(), 1));
    Matrix augmented(a.rows(), add(a.cols(), 1));
    for (const auto& [ij, q] : a.entries()) { guard.step(); augmented.set(ij.first, ij.second, q); }
    for (std::size_t i = 0; i < r.size(); ++i) { guard.step(); augmented.set(i, a.cols(), guard.sub(0, r[i])); }
    const numeric::PreparedExactFrames frame(a, guard), joint(augmented, guard);
    out.rank = frame.rank();
    if (frame.rank() == joint.rank()) {
        out.feasible_shift = V(a.cols()); out.feasible_kernel = frame.kernel();
        for (std::size_t i = 0; i < joint.rank(); ++i) {
            guard.step(); out.feasible_shift[joint.pivots()[i]] = joint.reduced().get(i, a.cols());
        }
        require(numeric::is_zero(sum(r, numeric::checked_apply(a, out.feasible_shift, guard), guard)) &&
                    numeric::checked_multiply(a, out.feasible_kernel, guard).nonzeros() == 0,
                "task feasible-family certificate");
        out.status = numeric::is_zero(r) && a.nonzeros() == 0
                         ? RelationalTaskPostconditionStatus::Satisfied : RelationalTaskPostconditionStatus::Open;
    } else {
        const numeric::PreparedExactFrames dual(a.transpose(), guard);
        for (std::size_t j = 0; j < dual.kernel().cols() && out.contradiction.empty(); ++j) {
            V w(r.size()); numeric::Rational pairing;
            for (const auto& [ij, q] : dual.kernel().entries()) if (ij.second == j) {
                guard.step(); w[ij.first] = q; pairing = guard.add(pairing, guard.mul(q, r[ij.first]));
            }
            if (pairing != 0) out.contradiction = std::move(w);
        }
        require(!out.contradiction.empty() &&
                    numeric::is_zero(numeric::checked_transpose_apply(a, out.contradiction, guard)),
                "task contradiction certificate");
        out.feasible_kernel = Matrix(a.cols(), 0); out.status = RelationalTaskPostconditionStatus::Refuted;
    }
    return out;
}
struct ResponseReading { RelationalAffineField field; Matrix metric; };
inline ResponseReading response_reading(const RelationalTaskResponse& value,
    const RelationalWorkerBindingDeclaration& b, Guard& guard,
    numeric::ExactActionLimits limits, const std::function<void()>& check) {
    require(!b.view.physical, "task field reading requires a declared rational accession");
        const auto original = task_outcome_detail::input(value).result();
        const auto p = original.result().program();
        inference::RelationalFieldInterface face(original, b.interface, limits, check);
        const auto reading = face.observe(b.view, limits, check);
        const auto dimension = face.metric().rows();
        const auto free = std::visit([](const auto& v) -> std::size_t {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, AvailableRelationalAffineResult>) {
                const auto result = v.result(); const auto evidence = result.evidence();
                return evidence.free_parameters().size();
            }
            else return 0;
        }, value);
        V local; local.reserve(dimension); Matrix directions(dimension, free); std::size_t row{};
        std::map<std::uint64_t, std::pair<V, Matrix>> node_fields;
        for (const auto& c : b.interface.cells) {
            const auto offsets = p.fibers().fibers().stalks().offsets(c.cell.grade);
            const auto begin = offsets.at(c.cell.index), end = offsets.at(c.cell.index + 1);
            if (!node_fields.contains(c.node)) {
                auto field = std::visit([&](const auto& v) -> std::pair<V, Matrix> {
                    if constexpr (std::is_same_v<std::decay_t<decltype(v)>, AvailableRelationalAffineResult>) {
                        const auto result = v.result(); const auto& f = result.field(c.node);
                        return {f.particular.values().dense(), f.directions};
                    } else {
                        const auto result = v.result().result();
                        return {result.field(c.node).values().dense(), Matrix(offsets.back(), 0)};
                    }
                }, value);
                guard.vector(field.first); guard.matrix(field.second); node_fields.emplace(c.node, std::move(field));
            }
            const auto& [values, varying] = node_fields.at(c.node);
            require(varying.cols() == free, "task views require one common parameter family");
            for (auto i = begin; i < end; ++i) { guard.step(); local.push_back(values.at(i)); }
            for (const auto& [ij, q] : varying.entries()) if (ij.first >= begin && ij.first < end) {
                guard.step(); directions.set(row + ij.first - begin, ij.second, q);
            }
            row = add(row, end - begin);
        }
        require(row == dimension, "task view omits original field coordinates");
        auto represented = numeric::checked_apply(b.view.accession, local, guard);
        auto varying = numeric::checked_multiply(b.view.accession, directions, guard);
        return {{tensor(represented, guard), std::move(varying)}, reading.metric()};
}
} // namespace task_outcome_detail

inline task_outcome_detail::Bytes relational_task_goal_payload(const RelationalTaskPostconditionDeclaration& d,
    const inference::OwnedRelationalInputUseAsset& asset, const RelationalTaskResponse& response) {
    return task_outcome_detail::goal_payload(d, inference::encode_relational_input_use_asset(asset),
        task_outcome_detail::input_payloads(task_outcome_detail::input(response).result()));
}
inline task_outcome_detail::Bytes relational_worker_binding_payload(const RelationalWorkerBindingDeclaration& d,
    const RelationalTaskResponse& response) {
    return task_outcome_detail::binding_payload(d, inference::encode_relational_field_program(task_outcome_detail::program(response)));
}

// A returned computation and a prospective response family have separate
// meanings. Neither callback completion nor a feasible parameter setting is an
// observed task success. The complete response and its original inputs remain owned.
class OwnedRelationalTaskOutcome {
    struct State {
        RelationalTaskPostconditionDeclaration task;
        inference::OwnedRelationalInputUseAsset asset;
        RelationalWorkerBindingDeclaration binding;
        RelationalTaskResponse response;
        task_outcome_detail::Origin goal_origin, binding_origin;
        RelationalTaskPostconditionCertificate certificate;
        numeric::ExactActionLimits limits;
    };
public:
    OwnedRelationalTaskOutcome(RelationalTaskPostconditionDeclaration task,
        inference::OwnedRelationalInputUseAsset asset, RelationalWorkerBindingDeclaration binding,
        RelationalTaskResponse response, const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace task_outcome_detail;
        const auto& t = task; const auto& a = asset; const auto& b = binding; const auto& value = response;
        const auto authority = context; Guard guard(limits, check);
        task_outcome_detail::admit(value, authority, check);
        const auto goal = authority.require(t.origin, Role::Prospective, relational_task_goal_payload(t, a, value));
        const auto provider = authority.require(b.origin, Role::Maps, relational_worker_binding_payload(b, value));
        require(t.key && t.revision && t.use && t.basis && b.key && b.revision && b.worker && b.machine && b.operation,
                "task and worker/machine/operation identities required");
        (void)a.use(t.use); admit_field(t.target, t.directions.size(), guard);
        require(!t.directions.empty() && b.view.basis == t.basis && b.view.directions == t.directions && !b.view.physical,
                "task needs an explicit rational field view in its declared basis");
        std::set<std::uint64_t> axes;
        for (const auto d : t.directions) { guard.step(); require(d && axes.insert(d).second, "distinct task direction identities required"); }
        auto observed = response_reading(value, b, guard, limits, check);
        require(observed.metric == t.metric, "task observation metric differs from actual transported metric");
        auto residual = observed.field.particular.values().dense();
        const auto target = t.target.values().dense();
        for (std::size_t i = 0; i < residual.size(); ++i) residual[i] = guard.sub(residual[i], target.at(i));
        auto varying = std::move(observed.field.directions);
        auto certificate = solve({tensor(residual, guard), std::move(varying)}, guard);
        state_ = std::make_shared<const State>(State{std::move(task), std::move(asset), std::move(binding),
            std::move(response), goal, provider, std::move(certificate), limits});
    }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context;
        task_outcome_detail::admit(pin->response, authority, check);
        if (check) check();
        authority.recheck(pin->goal_origin); authority.recheck(pin->binding_origin);
    }
    RelationalTaskPostconditionStatus status(const inference::RelationalAvailabilityContext& context) const {
        admit(context); return state_->certificate.status;
    }
    RelationalTaskPostconditionCertificate certificate(const inference::RelationalAvailabilityContext& context) const {
        admit(context); return state_->certificate;
    }
    const RelationalTaskPostconditionDeclaration& task() const & { return state_->task; }
    const RelationalWorkerBindingDeclaration& binding() const & { return state_->binding; }
    const RelationalTaskPostconditionDeclaration& task() const && = delete;
    const RelationalWorkerBindingDeclaration& binding() const && = delete;
    inference::OwnedRelationalInputUseAsset contract_asset() const { return state_->asset; }
    RelationalTaskResponse response() const { return state_->response; }
    numeric::ExactActionLimits limits() const { return state_->limits; }
    // This distinguishes a native execution result from a field family. The
    // caller retains any external callback or job receipt separately.
    bool executed_response() const noexcept { return state_->response.index() == 0; }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::learning
