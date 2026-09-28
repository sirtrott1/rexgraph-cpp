#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/topology/attaching_relation.hpp"
#include "rfc/topology/path_groupoid.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::topology {

struct AttachingWord {
    core::RelationId face{};
    PairwisePath loop;
};

inline AttachingWord attaching_word(const core::RelationalComplex& complex,
                                    std::size_t face_index) {
    if (complex.grade_count() < 3u || face_index >= complex.grade(core::Grade{2}).cell_count()) {
        throw std::out_of_range("attaching face index");
    }
    const auto attaching = higher_attaching_relation(complex, core::Grade{2}, face_index);
    const auto type = attaching.relation_type();
    if (!complex.signature().type(type).is_attaching_loop()) {
        throw std::invalid_argument("grade-two relation is not declared as an attaching loop");
    }
    std::vector<EdgeStep> steps;
    steps.reserve(attaching.fiber().size());
    for (const auto& occurrence : attaching.fiber().occurrences()) {
        if (!occurrence.exists) continue;
        const auto coefficient = occurrence.coefficient;
        try {
            (void)pairwise_endpoints(complex, occurrence.lower.index);
        } catch (const std::exception&) {
            throw std::invalid_argument("pairwise attaching-word projection requires pairwise grade-one participants; branching and other nonpairwise relations remain native higher attachments");
        }
        if (coefficient != numeric::Rational{1} && coefficient != numeric::Rational{-1}) {
            throw std::invalid_argument("pairwise attaching-word projection requires unit signed incidences; use higher_attaching_relation for the native boundary");
        }
        steps.push_back(EdgeStep{occurrence.lower.index, coefficient > 0});
    }
    if (steps.empty()) throw std::invalid_argument("attaching word must contain at least one active step");
    const auto first = pairwise_endpoints(complex, steps.front().relation);
    const auto source = steps.front().forward ? first.tail : first.head;
    PairwisePath loop(complex, source, std::move(steps));
    if (!loop.is_loop()) throw std::invalid_argument("declared attaching word is not a closed relational path");
    return AttachingWord{core::RelationId{core::Grade{2}, static_cast<std::uint32_t>(face_index)}, std::move(loop)};
}

inline std::vector<AttachingWord> attaching_words(const core::RelationalComplex& complex) {
    std::vector<AttachingWord> words;
    if (complex.grade_count() < 3u) return words;
    const auto& grade2 = complex.grade(core::Grade{2});
    for (std::size_t face = 0; face < grade2.cell_count(); ++face) {
        const auto type = grade2.relation_types[face];
        if (complex.signature().type(type).is_attaching_loop()) words.push_back(attaching_word(complex,face));
    }
    return words;
}

struct FundamentalGroupoidPresentation {
    const core::RelationalComplex* complex{};
    std::vector<AttachingWord> relators;
};

inline FundamentalGroupoidPresentation fundamental_groupoid_presentation(const core::RelationalComplex& complex) {
    return FundamentalGroupoidPresentation{&complex, attaching_words(complex)};
}

struct FaceHomotopyMove {
    std::uint32_t face{};
    std::size_t cyclic_shift{};
    bool inverse{false};
    bool insert{false};
    std::size_t position{};
};

inline std::vector<EdgeStep> cyclic_steps(const std::vector<EdgeStep>& word, std::size_t shift) {
    if (word.empty()) return {};
    shift %= word.size();
    std::vector<EdgeStep> out;
    out.reserve(word.size());
    for (std::size_t i = 0; i < word.size(); ++i) out.push_back(word[(shift+i)%word.size()]);
    return out;
}

inline std::vector<EdgeStep> inverse_steps(const std::vector<EdgeStep>& word) {
    std::vector<EdgeStep> out;
    out.reserve(word.size());
    for (auto it = word.rbegin(); it != word.rend(); ++it) out.push_back(EdgeStep{it->relation,!it->forward});
    return out;
}

inline std::uint32_t path_vertex_at(const PairwisePath& path, std::size_t position) {
    if (position > path.steps().size()) throw std::out_of_range("path vertex position");
    std::uint32_t current = path.source();
    for (std::size_t i = 0; i < position; ++i) {
        const auto& step = path.steps()[i];
        const auto endpoints = pairwise_endpoints(path.complex(),step.relation);
        current = step.forward ? endpoints.head : endpoints.tail;
    }
    return current;
}

inline PairwisePath apply_face_homotopy(const FundamentalGroupoidPresentation& presentation,
                                        const PairwisePath& input,
                                        const FaceHomotopyMove& move) {
    if (presentation.complex == nullptr || &input.complex() != presentation.complex) {
        throw std::invalid_argument("homotopy presentation complex mismatch");
    }
    const AttachingWord* relator = nullptr;
    for (const auto& candidate : presentation.relators) if (candidate.face.index == move.face) { relator = &candidate; break; }
    if (relator == nullptr) throw std::out_of_range("unknown attaching face in homotopy move");

    auto word = cyclic_steps(relator->loop.steps(),move.cyclic_shift);
    if (move.inverse) word = inverse_steps(word);
    if (word.empty()) throw std::logic_error("empty attaching relation");
    const auto first = pairwise_endpoints(input.complex(),word.front().relation);
    const auto loop_source = word.front().forward ? first.tail : first.head;
    if (path_vertex_at(input,move.position) != loop_source) throw std::invalid_argument("face move basepoint mismatch");

    auto steps = input.steps();
    if (move.insert) {
        steps.insert(steps.begin()+static_cast<std::ptrdiff_t>(move.position),word.begin(),word.end());
    } else {
        if (move.position + word.size() > steps.size()) throw std::invalid_argument("face removal exceeds path length");
        for (std::size_t i = 0; i < word.size(); ++i) if (steps[move.position+i] != word[i]) {
            throw std::invalid_argument("path does not contain requested attaching-word representative");
        }
        steps.erase(steps.begin()+static_cast<std::ptrdiff_t>(move.position),
                    steps.begin()+static_cast<std::ptrdiff_t>(move.position+word.size()));
    }
    return PairwisePath(input.complex(),input.source(),std::move(steps)).reduced();
}

inline bool certify_relational_homotopy(const FundamentalGroupoidPresentation& presentation,
                                        const PairwisePath& first,
                                        const PairwisePath& second,
                                        const std::vector<FaceHomotopyMove>& certificate) {
    PairwisePath current = first.reduced();
    for (const auto& move : certificate) current = apply_face_homotopy(presentation,current,move);
    const auto lhs = current.reduced();
    const auto rhs = second.reduced();
    return lhs.source() == rhs.source() && lhs.target() == rhs.target() && lhs.steps() == rhs.steps();
}

} // namespace rfc::topology
