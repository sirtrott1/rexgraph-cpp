#pragma once

#include "rfc/inference/relational_field_interface.hpp"

namespace rfc::inference {
struct RelationalKnownTensorSquareDeclaration {
    std::uint64_t key{}, revision{}, lifted_basis{};
    core::CellId context;
    // Row-major ordered pairs of the actual context's original directions.
    std::vector<std::uint64_t> pair_directions;
};
struct RelationalTensorPairCoordinate {
    core::CellId context;
    std::uint64_t original_basis{}, left{}, right{}, lifted_direction{};
};
class RelationalKnownTensorSquareInput;

// A known-input operation on one actual stalk. It preserves the complete
// accession parent and is neither a graded source product nor multiplication
// of unresolved parameter families. Its image is {(x, x tensor x)}.
class OwnedRelationalKnownTensorSquare {
    using Guard = numeric::ExactActionGuard;
    using Tensor = numeric::SparseRationalTensor;
    using Matrix = numeric::ExactMatrix;
    struct State {
        RelationalAccessionInput parent;
        RelationalKnownTensorSquareDeclaration declaration;
        StalkBasisDeclaration stalk;
        core::CellId source_context;
        Tensor context, square, lifted;
        Matrix metric;
    };
public:
    OwnedRelationalKnownTensorSquare(RelationalAccessionInput parent,
        RelationalKnownTensorSquareDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace field_interface_detail;
        const auto original = parent; const auto d = declaration;
        Guard guard(limits, std::move(check));
        const auto accession = original.accession(); const auto program = accession.program();
        const auto& input_node = node(program, accession.declaration().node);
        require(d.key && d.revision && d.lifted_basis && d.context.grade == input_node.output_grade,
            "known tensor square needs identity and an actual input context stalk");
        const auto s = stalk(program, d.context); const auto n = s.directions.size();
        require(d.lifted_basis != s.basis, "known tensor square requires a declared enlarged basis");
        const auto pairs = machine::checked_size_mul(n, n); const auto extent = add(n, pairs);
        guard.dimension(extent); guard.entries(add(n, pairs));
        require(d.pair_directions.size() == pairs, "known tensor square omits ordered pair directions");
        std::set<std::uint64_t> directions(s.directions.begin(), s.directions.end());
        for (const auto key : d.pair_directions) {
            guard.step(); require(key && directions.insert(key).second,
                "known tensor square pair direction is zero or repeated");
        }
        const auto asset = accession.asset(); const auto publication = asset.publication();
        const auto& source = publication.endpoint().source;
        require(d.context.grade.value < source.grade_count() &&
            source.grade(d.context.grade).cell_count() == 1,
            "known tensor square input profile requires one original context at its grade");
        const auto& instances = accession.declaration().instances.cells.at(d.context.grade.value);
        require(instances.size() == 1 && instances[0] == d.context.index,
            "known tensor square context is not the retained original source context");
        const core::CellId source_context{d.context.grade, 0};
        const auto fibers = program.fibers(); const auto offsets = fibers.fibers().stalks().offsets(d.context.grade);
        const auto begin = offsets.at(d.context.index), end = offsets.at(add(d.context.index, 1));
        require(end - begin == n, "known tensor square actual context basis differs");
        admit_field(original.represented(), offsets.back(), guard);
        std::vector<numeric::SparseRationalEntry> entries;
        for (const auto& entry : original.represented().values().entries()) {
            guard.step(); if (entry.index >= begin && entry.index < end)
                entries.push_back({entry.index - begin, entry.value});
        }
        Tensor context{numeric::IndexDomain({n}), numeric::StructuralSupport::full(n),
            numeric::SparseRationalVector(n, std::move(entries))};
        const auto nonzeros = context.values().nonzeros();
        guard.entries(add(nonzeros, machine::checked_size_mul(nonzeros, nonzeros)));
        std::vector<numeric::SparseRationalEntry> products, lifted;
        for (const auto& left : context.values().entries()) {
            lifted.push_back(left);
            for (const auto& right : context.values().entries()) {
                const auto index = add(machine::checked_size_mul(left.index, n), right.index);
                const auto value = guard.mul(left.value, right.value);
                products.push_back({index, value}); lifted.push_back({add(n, index), value});
            }
        }
        Tensor square{numeric::IndexDomain({n, n}), numeric::StructuralSupport::full(pairs),
            numeric::SparseRationalVector(pairs, std::move(products))};
        Tensor whole{numeric::IndexDomain({extent}), numeric::StructuralSupport::full(extent),
            numeric::SparseRationalVector(extent, std::move(lifted))};
        Matrix metric(extent, extent);
        const auto metric_nonzeros = s.metric.nonzeros();
        guard.entries(add(metric_nonzeros, machine::checked_size_mul(metric_nonzeros, metric_nonzeros)));
        for (const auto& [ij, value] : s.metric.entries()) {
            guard.step(); metric.set(ij.first, ij.second, value);
            for (const auto& [kl, other] : s.metric.entries()) {
                const auto row = add(n, add(machine::checked_size_mul(ij.first, n), kl.first));
                const auto column = add(n, add(machine::checked_size_mul(ij.second, n), kl.second));
                metric.set(row, column, guard.mul(value, other));
            }
        }
        guard.matrix(metric); guard.step();
        state_ = std::make_shared<const State>(State{original, d, s, source_context,
            std::move(context), std::move(square), std::move(whole), std::move(metric)});
    }
    RelationalAccessionInput parent() const { return state_->parent; }
    const RelationalKnownTensorSquareDeclaration& declaration() const & { return state_->declaration; }
    const RelationalKnownTensorSquareDeclaration& declaration() const && = delete;
    const StalkBasisDeclaration& original_stalk() const & { return state_->stalk; }
    const StalkBasisDeclaration& original_stalk() const && = delete;
    core::CellId source_context() const noexcept { return state_->source_context; }
    const Tensor& context_field() const & { return state_->context; }
    const Tensor& context_field() const && = delete;
    const Tensor& tensor_square() const & { return state_->square; }
    const Tensor& tensor_square() const && = delete;
    const Tensor& lifted_field() const & { return state_->lifted; }
    const Tensor& lifted_field() const && = delete;
    const Matrix& metric() const & { return state_->metric; }
    const Matrix& metric() const && = delete;
    bool same_owner(const OwnedRelationalKnownTensorSquare& other) const noexcept { return state_ == other.state_; }
    RelationalTensorPairCoordinate pair_coordinate(std::size_t pair) const {
        const auto& s = state_->stalk; const auto n = s.directions.size();
        if (pair >= state_->declaration.pair_directions.size()) throw std::out_of_range("known tensor pair coordinate");
        return {state_->declaration.context, s.basis, s.directions[pair / n], s.directions[pair % n],
            state_->declaration.pair_directions[pair]};
    }
    void require_image(const Tensor& field, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const {
        const auto pin = state_; const auto value = field; Guard guard(limits, std::move(check));
        field_program_detail::admit_field(value, pin->lifted.domain().logical_size(), guard);
        field_program_detail::require(field_interface_detail::equal(value, pin->lifted),
            "known tensor square field does not belong to its actual operand image");
    }
    RelationalKnownTensorSquareInput admit(OwnedRelationalInputAccession target,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};

// Retain this owner through serving/replay, rather than detaching input().
// Availability of either parent is a separate native provider contract.
class RelationalKnownTensorSquareInput {
public:
    OwnedRelationalKnownTensorSquare square() const { return square_; }
    RelationalAccessionInput input() const { return input_; }
private:
    friend class OwnedRelationalKnownTensorSquare;
    RelationalKnownTensorSquareInput(OwnedRelationalKnownTensorSquare square, RelationalAccessionInput input)
        : square_(std::move(square)), input_(std::move(input)) {}
    OwnedRelationalKnownTensorSquare square_;
    RelationalAccessionInput input_;
};
inline RelationalKnownTensorSquareInput OwnedRelationalKnownTensorSquare::admit(
    OwnedRelationalInputAccession target, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_interface_detail;
    const auto pin = *this; const auto a = target; Guard guard(limits, check);
    const auto original = pin.parent(); const auto parent_accession = original.accession();
    const auto parent_program = parent_accession.program(); const auto next_program = a.program();
    require(a.asset().same_owner(parent_accession.asset()), "known tensor target refers to a foreign original asset owner");
    require(next_program.declaration().key != parent_program.declaration().key ||
        next_program.declaration().revision != parent_program.declaration().revision,
        "known tensor enlarged input requires a new program key or revision");
    const auto& d = a.declaration(); const auto& square = pin.declaration(); const auto& s = pin.original_stalk();
    require(d.source_grade == pin.source_context().grade && d.source_stalks.size() == 1,
        "known tensor target source grade/context differs");
    const auto& lifted = d.source_stalks.front();
    require(lifted.cell == pin.source_context() && lifted.basis == square.lifted_basis,
        "known tensor target changes retained context or enlarged basis identity");
    auto directions = s.directions; directions.insert(directions.end(), square.pair_directions.begin(), square.pair_directions.end());
    guard.entries(directions.size()); guard.matrix(lifted.metric);
    require(lifted.directions == directions && lifted.metric == pin.metric(),
        "known tensor target changes original/pair basis order or product metric");
    return {pin, a.admit(original.use(), pin.lifted_field(), limits, std::move(check))};
}
} // namespace rfc::inference
