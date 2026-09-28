#pragma once
#include "rfc/field/lattice_boundary.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace rfc::learning {

enum class OperatorChannelKind : std::uint8_t {
    Identity,
    HodgeDown,
    HodgeUp,
    Hodge
};

inline std::string_view operator_channel_name(OperatorChannelKind kind) noexcept {
    switch (kind) {
        case OperatorChannelKind::Identity: return "identity";
        case OperatorChannelKind::HodgeDown: return "hodge_down";
        case OperatorChannelKind::HodgeUp: return "hodge_up";
        case OperatorChannelKind::Hodge: return "hodge";
    }
    return "unknown";
}

inline field::MachineLatticeField apply_operator_channel(
    OperatorChannelKind kind,
    const field::MachineLatticeField& input) {
    const auto& complex = input.complex();
    const auto grade = input.grade();
    if (kind == OperatorChannelKind::Identity) return input;

    std::vector<std::int64_t> result(input.values().size(), 0);
    if (kind == OperatorChannelKind::HodgeDown || kind == OperatorChannelKind::Hodge) {
        if (grade.value > 0) {
            field::LatticeBoundaryOperator boundary(complex, grade);
            const auto lower = boundary.apply(input);
            const auto returned = boundary.transpose_apply(lower);
            for (std::size_t i = 0; i < result.size(); ++i) {
                result[i] = numeric::checked_add_i64(result[i], returned.values()[i]);
            }
        }
    }
    if (kind == OperatorChannelKind::HodgeUp || kind == OperatorChannelKind::Hodge) {
        if (grade.value < complex.max_grade().value) {
            const auto upper_grade = core::Grade{static_cast<std::uint16_t>(grade.value + 1u)};
            field::LatticeBoundaryOperator boundary(complex, upper_grade);
            const auto higher = boundary.transpose_apply(input);
            const auto returned = boundary.apply(higher);
            for (std::size_t i = 0; i < result.size(); ++i) {
                result[i] = numeric::checked_add_i64(result[i], returned.values()[i]);
            }
        }
    }
    return field::MachineLatticeField(complex, grade, std::move(result));
}

inline field::ExactLatticeField apply_operator_channel(
    OperatorChannelKind kind,
    const field::ExactLatticeField& input) {
    const auto& complex = input.complex();
    const auto grade = input.grade();
    if (kind == OperatorChannelKind::Identity) return input;

    std::vector<numeric::Integer> result(input.values().size(), numeric::Integer{0});
    if (kind == OperatorChannelKind::HodgeDown || kind == OperatorChannelKind::Hodge) {
        if (grade.value > 0) {
            field::LatticeBoundaryOperator boundary(complex, grade);
            const auto lower = boundary.apply(input);
            const auto returned = boundary.transpose_apply(lower);
            for (std::size_t i = 0; i < result.size(); ++i) result[i] += returned.values()[i];
        }
    }
    if (kind == OperatorChannelKind::HodgeUp || kind == OperatorChannelKind::Hodge) {
        if (grade.value < complex.max_grade().value) {
            const auto upper_grade = core::Grade{static_cast<std::uint16_t>(grade.value + 1u)};
            field::LatticeBoundaryOperator boundary(complex, upper_grade);
            const auto higher = boundary.transpose_apply(input);
            const auto returned = boundary.apply(higher);
            for (std::size_t i = 0; i < result.size(); ++i) result[i] += returned.values()[i];
        }
    }
    return field::ExactLatticeField(complex, grade, std::move(result));
}

} // namespace rfc::learning
