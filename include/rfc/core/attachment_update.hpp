#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/core/axioms.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rfc::core {

inline std::vector<AspectValue> copied_declared_aspects(const RelationalComplex& complex,
                                                        const GradeArena& arena,
                                                        std::size_t incidence) {
    std::vector<AspectValue> out;
    const auto& schema = complex.signature().aspects();
    out.reserve(schema.size());
    for (std::size_t a = 0; a < schema.size(); ++a) {
        const auto id = static_cast<composite::AspectId>(a);
        const auto role = schema.at(id).role;
        if (role == composite::AspectRole::Existence || role == composite::AspectRole::Orientation) continue;
        out.push_back(AspectValue{id, arena.aspects.get(id, incidence)});
    }
    return out;
}

inline RelationalComplexBuilder builder_from_complex(const RelationalComplex& complex) {
    RelationalComplexBuilder builder(complex.signature(), complex.semantic_scale());
    const auto& grade0 = complex.grade(Grade{0});
    for (const auto identity : grade0.identities) builder.declare_witness_identity(identity);
    const auto exists_id = complex.signature().existence_aspect();
    const auto orientation_id = complex.signature().orientation_aspect();
    if (complex.grade_count() > 1) {
        const auto& g1 = complex.grade(Grade{1});
        for (std::size_t cell = 0; cell < g1.cell_count(); ++cell) {
            std::vector<BoundaryOccurrenceInput> occurrences;
            const auto [begin,end] = g1.incidence_range(cell);
            occurrences.reserve(end-begin);
            for (std::size_t p = begin; p < end; ++p) {
                const auto coeff = complex.incidence_coefficient(Grade{1},p);
                numeric::Rational magnitude = coeff < 0 ? -coeff : coeff;
                occurrences.push_back(BoundaryOccurrenceInput{
                    grade0.identities.at(g1.lower_index[p]),
                    g1.aspects.get(exists_id,p),
                    g1.aspects.get(orientation_id,p),
                    magnitude,
                    copied_declared_aspects(complex,g1,p),
                    g1.occurrence_identities.at(p)
                });
            }
            builder.add_grade_one(g1.relation_identities.at(cell),g1.relation_types.at(cell),occurrences);
        }
    }
    for (std::size_t k = 2; k < complex.grade_count(); ++k) {
        const auto grade = Grade{static_cast<std::uint16_t>(k)};
        const auto& arena = complex.grade(grade);
        for (std::size_t cell = 0; cell < arena.cell_count(); ++cell) {
            std::vector<HigherIncidenceInput> incidences;
            const auto [begin,end] = arena.incidence_range(cell);
            incidences.reserve(end-begin);
            for (std::size_t p = begin; p < end; ++p) {
                const auto coeff = complex.incidence_coefficient(grade,p);
                numeric::Rational magnitude = coeff < 0 ? -coeff : coeff;
                incidences.push_back(HigherIncidenceInput{
                    arena.lower_index[p],
                    arena.aspects.get(exists_id,p),
                    arena.aspects.get(orientation_id,p),
                    magnitude,
                    copied_declared_aspects(complex,arena,p),
                    arena.occurrence_identities.at(p)
                });
            }
            builder.add_higher(grade,arena.relation_identities.at(cell),arena.relation_types.at(cell),incidences);
        }
    }
    return builder;
}

struct AttachmentUpdateResult {
    RelationalComplex complex;
    BianchiReport bianchi;
};

inline AttachmentUpdateResult append_higher_attachment(const RelationalComplex& source,
                                                        Grade grade,
                                                        RelationTypeId type,
                                                        const std::vector<HigherIncidenceInput>& incidences,
                                                        bool require_bianchi = true) {
    if (grade.value < 2) throw std::invalid_argument("higher attachment update requires grade >= 2");
    if (grade.value > source.max_grade().value + 1u) throw std::invalid_argument("attachment update cannot skip grades");
    auto builder = builder_from_complex(source);
    builder.add_higher(grade,type,incidences);
    auto updated = builder.freeze();
    // add_higher() already certified the newly introduced adjacent chain
    // interface, and all pre-existing interfaces came from the canonical
    // source.  Preserve that certificate instead of rescanning the tower.
    auto report = canonical_bianchi_certificate(updated);
    if (require_bianchi && !report.satisfied()) throw std::invalid_argument("higher attachment violates Bianchi tower constraints");
    return {std::move(updated),std::move(report)};
}

} // namespace rfc::core
