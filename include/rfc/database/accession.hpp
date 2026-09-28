#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/machine/packed_bits.hpp"
#include <cstddef>

namespace rfc::database {

inline machine::PackedBits select_relation_type(const core::RelationalComplex& complex,
                                                core::Grade grade,
                                                core::RelationTypeId type) {
    const auto& arena = complex.grade(grade);
    machine::PackedBits result(arena.cell_count(), false);
    for (std::size_t i = 0; i < arena.relation_types.size(); ++i) {
        if (arena.relation_types[i] == type) result.set(i, true);
    }
    return result;
}

inline machine::PackedBits select_cells(const core::RelationalComplex& complex,
                                        core::Grade grade,
                                        core::RelationTypeId type,
                                        const machine::PackedBits* active = nullptr) {
    auto result = select_relation_type(complex, grade, type);
    if (active != nullptr) result = result & *active;
    return result;
}

} // namespace rfc::database
