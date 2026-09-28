#pragma once
#include "rfc/field/lattice_tower.hpp"
#include "rfc/numeric/checked_int.hpp"
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace rfc::learning {

enum class TowerOperatorChannelKind : std::uint8_t {
    Identity,
    Lowering,
    Raising,
    Dirac
};

inline std::string_view tower_operator_channel_name(TowerOperatorChannelKind kind) noexcept {
    switch (kind) {
        case TowerOperatorChannelKind::Identity: return "identity";
        case TowerOperatorChannelKind::Lowering: return "lowering";
        case TowerOperatorChannelKind::Raising: return "raising";
        case TowerOperatorChannelKind::Dirac: return "dirac";
    }
    return "unknown";
}

inline field::MachineLatticeTowerField apply_tower_operator_channel(
    TowerOperatorChannelKind kind,
    const field::MachineLatticeTowerField& input) {
    switch (kind) {
        case TowerOperatorChannelKind::Identity: return input;
        case TowerOperatorChannelKind::Lowering: return field::lattice_lowering_apply(input);
        case TowerOperatorChannelKind::Raising: return field::lattice_raising_apply(input);
        case TowerOperatorChannelKind::Dirac: return field::lattice_dirac_apply(input);
    }
    throw std::logic_error("unknown tower operator channel");
}

} // namespace rfc::learning
