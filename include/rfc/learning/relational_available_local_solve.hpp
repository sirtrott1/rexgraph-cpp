#pragma once

#include "rfc/inference/relational_field_solve.hpp"
#include "rfc/learning/relational_available_evidence_codec.hpp"
#include <variant>

namespace rfc::learning {
using AvailableRelationalSolveRhs = std::variant<inference::AvailableRelationalFieldResult, AvailableRelationalAffineEvaluation>;
struct RelationalSolveReadoutDeclaration {
    // A boundary-closed patch in every original grade; physical partitions are separate.
    std::vector<std::vector<std::uint32_t>> patch;
    inference::RelationalFieldViewDeclaration view;
};
struct RelationalSolveOrigins { std::uint64_t problem{}, interface{}, reconstruction{}, gauge{}; };
namespace available_local_detail {
using namespace inference::relational_solve_detail;
using Origin = inference::OwnedRelationalValueOrigin;
using Context = inference::RelationalAvailabilityContext;
using Role = inference::RelationalDependencyRole;
using Writer = inference::field_program_codec_detail::Writer;
inline inference::OwnedAccessionFieldResult native_rhs(const AvailableRelationalSolveRhs& rhs) {
    return std::visit([](const auto& value) -> inference::OwnedAccessionFieldResult {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, inference::AvailableRelationalFieldResult>) return value.result();
        else return value.result().result();
    }, rhs);
}
inline void admit_rhs(const AvailableRelationalSolveRhs& rhs, const Context& context, const std::function<void()>& check) {
    std::visit([&](const auto& value) { value.admit(context, check); }, rhs);
}
inline std::vector<std::byte> encode_rhs(const AvailableRelationalSolveRhs& rhs) {
    return std::visit([](const auto& value) -> std::vector<std::byte> {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, inference::AvailableRelationalFieldResult>) return inference::encode_available_relational_result(value);
        else return encode_available_relational_affine_evaluation(value);
    }, rhs);
}
inline void declaration(Writer& w, const inference::RelationalFieldSolveDeclaration& d) {
    w.u64(d.key); w.u64(d.revision); w.u64(d.coordinate_carrier); w.u64(d.coordinate_basis); w.u16(d.grade.value);
    w.u8(static_cast<std::uint8_t>(d.kind)); storage::rfcx_detail::write_rational(w, d.lambda); w.u8(d.harmonic_frame ? 1 : 0);
    if (d.harmonic_frame) storage::temporal_codec_detail::matrix(w, *d.harmonic_frame);
    w.u64(d.interior.size()); for (const auto i : d.interior) w.u64(i);
}
inline void readout(Writer& w, const RelationalSolveReadoutDeclaration& d) {
    w.u64(d.patch.size()); for (const auto& cells : d.patch) { w.u64(cells.size()); for (const auto cell : cells) w.u32(cell); }
    inference::field_interface_codec_detail::view(w, d.view);
}
inline std::vector<std::byte> problem_payload(std::span<const std::byte> rhs, std::uint64_t node,
    const inference::RelationalFieldSolveDeclaration& d) {
    Writer w; w.u8(1); inference::field_program_codec_detail::blob(w, rhs); w.u64(node); declaration(w, d); return std::move(w).take_bytes();
}
inline std::vector<std::byte> readout_payload(std::span<const std::byte> problem, const RelationalSolveReadoutDeclaration& d) {
    Writer w; w.u8(1); inference::field_program_codec_detail::blob(w, problem); readout(w, d); return std::move(w).take_bytes();
}
inline std::vector<Origin> origins(const std::vector<std::byte>& problem, const RelationalSolveReadoutDeclaration& read,
    const inference::RelationalFieldSolveDeclaration& d, RelationalSolveOrigins keys, const Context& context) {
    std::vector<Origin> out;
    out.push_back(context.require(keys.problem, Role::Maps, problem));
    out.push_back(context.require(keys.interface, Role::Interface, problem));
    out.push_back(context.require(keys.reconstruction, Role::Reconstruction, readout_payload(problem, read)));
    if (d.harmonic_frame) out.push_back(context.require(keys.gauge, Role::Maps, problem));
    else require(!keys.gauge, "derived harmonic gauge has no independent publisher origin");
    return out;
}
struct Readout {
    algebra::SubcomplexMask patch;
    Matrix metric;
    std::vector<std::size_t> coordinates;
};
inline Readout prepare_readout(const inference::OwnedRelationalFieldProgram& program, core::Grade k,
    const RelationalSolveReadoutDeclaration& d, Guard& guard) {
    const auto& source = program.fibers().fibers().complex();
    require(d.patch.size() == source.grade_count(), "local readout patch grade count");
    algebra::SubcomplexMask patch(source); std::vector<inference::StalkBasisDeclaration> stalks; std::vector<std::size_t> coordinates;
    const auto offsets = program.fibers().fibers().stalks().offsets(k);
    for (std::size_t grade_index = 0; grade_index < d.patch.size(); ++grade_index) {
        const auto g = inference::field_program_detail::grade(grade_index); const auto& cells = d.patch[grade_index];
        for (std::size_t i = 0; i < cells.size(); ++i) {
            guard.step(); require(cells[i] < source.grade(g).cell_count() && (!i || cells[i - 1] < cells[i]), "local readout patch ordered distinct cells");
            patch.grade(g).set(cells[i], true);
            if (g == k) {
                stalks.push_back(inference::field_interface_detail::stalk(program, {g, cells[i]}));
                for (auto j = offsets.at(cells[i]); j < offsets.at(cells[i] + 1u); ++j) { guard.step(); coordinates.push_back(j); }
            }
        }
    }
    local::RelationalLocalCarrier(source).require_closed_patch(patch);
    const auto& view = d.view; require(view.key && view.basis && view.directions.size() == view.accession.rows(), "local readout identity and basis");
    std::set<std::uint64_t> directions;
    for (const auto key : view.directions) { guard.step(); require(key && directions.insert(key).second, "local readout repeated or zero direction"); }
    require(view.accession.cols() == coordinates.size(), "local readout accession patch dimension");
    faithful(view.accession, view.reconstruction, guard);
    if (view.physical) {
        guard.matrix(*view.physical);
        require(view.physical->cols() == coordinates.size() &&
            numeric::checked_multiply(*view.physical, view.reconstruction, guard).cols() == view.accession.rows(), "local readout physical realization shape");
    }
    const auto original_metric = block_metric(stalks, guard);
    auto metric = numeric::checked_multiply(view.reconstruction.transpose(), numeric::checked_multiply(original_metric, view.reconstruction, guard), guard);
    guard.entries(add(coordinates.size(), metric.nonzeros())); return {std::move(patch), std::move(metric), std::move(coordinates)};
}
} // namespace available_local_detail
inline std::vector<std::byte> relational_local_problem_payload(const AvailableRelationalSolveRhs& rhs, std::uint64_t node,
    const inference::RelationalFieldSolveDeclaration& d) { return available_local_detail::problem_payload(available_local_detail::encode_rhs(rhs), node, d); }
inline std::vector<std::byte> relational_local_readout_payload(const AvailableRelationalSolveRhs& rhs, std::uint64_t node,
    const inference::RelationalFieldSolveDeclaration& d, const RelationalSolveReadoutDeclaration& readout) {
    return available_local_detail::readout_payload(relational_local_problem_payload(rhs, node, d), readout);
}
class AvailableRelationalLocalResult;
class PreparedAvailableRelationalLocalSolve {
    struct State {
        AvailableRelationalSolveRhs rhs;
        std::uint64_t node{};
        inference::RelationalFieldSolveDeclaration declaration;
        RelationalSolveReadoutDeclaration readout;
        RelationalSolveOrigins keys;
        std::vector<available_local_detail::Origin> obligations;
        available_local_detail::Readout prepared_readout;
        inference::PreparedRelationalFieldSolve native;
    };
public:
    PreparedAvailableRelationalLocalSolve(AvailableRelationalSolveRhs rhs, std::uint64_t node,
        inference::RelationalFieldSolveDeclaration declaration, RelationalSolveReadoutDeclaration readout, RelationalSolveOrigins keys,
        const inference::RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace available_local_detail;
        const auto frozen_rhs = rhs; const auto frozen_d = declaration; const auto frozen_readout = readout;
        const auto authority = context; const auto callback = std::move(check);
        admit_rhs(frozen_rhs, authority, callback);
        const auto payload = problem_payload(encode_rhs(frozen_rhs), node, frozen_d);
        auto obligations = origins(payload, frozen_readout, frozen_d, keys, authority);
        Guard guard(limits, callback); guard.step(); const auto parent = native_rhs(frozen_rhs).result().program();
        require(inference::field_interface_detail::node(parent, node).output_grade == frozen_d.grade, "local RHS grade");
        auto prepared_readout = prepare_readout(parent, frozen_d.grade, frozen_readout, guard);
        auto native = inference::PreparedRelationalFieldSolve(parent, frozen_d, limits, [&] { guard.step(); }); guard.step();
        state_ = std::make_shared<const State>(State{frozen_rhs, node, frozen_d, frozen_readout, keys, std::move(obligations), std::move(prepared_readout), std::move(native)});
    }
    AvailableRelationalSolveRhs rhs() const { return state_->rhs; }
    std::uint64_t node() const { return state_->node; }
    inference::PreparedRelationalFieldSolve problem() const { return state_->native; }
    const inference::RelationalFieldSolveDeclaration& declaration() const & { return state_->declaration; }
    const RelationalSolveReadoutDeclaration& readout() const & { return state_->readout; }
    const numeric::ExactMatrix& readout_metric() const & { return state_->prepared_readout.metric; }
    const algebra::SubcomplexMask& patch() const & { return state_->prepared_readout.patch; }
    const std::vector<available_local_detail::Origin>& obligations() const & { return state_->obligations; }
    const inference::RelationalFieldSolveDeclaration& declaration() const && = delete;
    const RelationalSolveReadoutDeclaration& readout() const && = delete;
    const numeric::ExactMatrix& readout_metric() const && = delete;
    const algebra::SubcomplexMask& patch() const && = delete;
    const std::vector<available_local_detail::Origin>& obligations() const && = delete;
    RelationalSolveOrigins origin_keys() const { return state_->keys; }
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        available_local_detail::admit_rhs(pin->rhs, authority, callback);
        for (const auto& origin : pin->obligations) { if (callback) callback(); authority.recheck(origin); }
    }
    AvailableRelationalLocalResult execute(const inference::RelationalAvailabilityContext& context,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class AvailableRelationalLocalResult {
    struct State {
        PreparedAvailableRelationalLocalSolve parent;
        inference::RelationalFieldSolveResult result;
        numeric::SparseRationalTensor patch_field, visible, reconstructed;
    };
public:
    PreparedAvailableRelationalLocalSolve parent() const { return state_->parent; }
    inference::RelationalFieldSolveResult result() const { return state_->result; }
    const numeric::SparseRationalTensor& patch_field() const & { return state_->patch_field; }
    const numeric::SparseRationalTensor& visible() const & { return state_->visible; }
    const numeric::SparseRationalTensor& reconstructed() const & { return state_->reconstructed; }
    const numeric::SparseRationalTensor& patch_field() const && = delete;
    const numeric::SparseRationalTensor& visible() const && = delete;
    const numeric::SparseRationalTensor& reconstructed() const && = delete;
    void admit(const inference::RelationalAvailabilityContext& context, std::function<void()> check = {}) const { state_->parent.admit(context, std::move(check)); }
private:
    friend class PreparedAvailableRelationalLocalSolve;
    explicit AvailableRelationalLocalResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline AvailableRelationalLocalResult PreparedAvailableRelationalLocalSolve::execute(const inference::RelationalAvailabilityContext& context,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace available_local_detail;
    const auto parent = *this; const auto pin = parent.state_; const auto authority = context; const auto callback = std::move(check); parent.admit(authority, callback);
    Guard guard(limits, callback); auto result = pin->native.apply(native_rhs(pin->rhs), pin->node, limits, [&] { guard.step(); });
    numeric::RationalVector patch_values; patch_values.reserve(pin->prepared_readout.coordinates.size());
    for (const auto i : pin->prepared_readout.coordinates) { guard.step(); patch_values.push_back(result.field().values().coefficient(i)); }
    auto patch = tensor(patch_values, guard); auto visible = act(pin->readout.view.accession, patch, guard);
    auto reconstructed = act(pin->readout.view.reconstruction, visible, guard);
    require(equal(patch, reconstructed), "local readout reconstruction mismatch"); guard.step();
    return AvailableRelationalLocalResult({parent, std::move(result), std::move(patch), std::move(visible), std::move(reconstructed)});
}
} // namespace rfc::learning
