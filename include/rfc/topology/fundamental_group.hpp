#pragma once
#include "rfc/numeric/smith_normal_form.hpp"
#include "rfc/topology/attaching_word.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rfc::topology {

struct FreeGroupLetter {
    std::uint32_t generator{};
    bool positive{true};
    constexpr auto operator<=>(const FreeGroupLetter&) const = default;
};

class FreeGroupWord {
public:
    FreeGroupWord() = default;
    explicit FreeGroupWord(std::vector<FreeGroupLetter> letters) : letters_(std::move(letters)) { reduce_in_place(); }
    [[nodiscard]] const std::vector<FreeGroupLetter>& letters() const noexcept { return letters_; }
    [[nodiscard]] bool empty() const noexcept { return letters_.empty(); }
    [[nodiscard]] FreeGroupWord inverse() const {
        std::vector<FreeGroupLetter> out;
        out.reserve(letters_.size());
        for (auto it = letters_.rbegin(); it != letters_.rend(); ++it) out.push_back({it->generator,!it->positive});
        return FreeGroupWord(std::move(out));
    }
    [[nodiscard]] static FreeGroupWord compose(const FreeGroupWord& first, const FreeGroupWord& second) {
        std::vector<FreeGroupLetter> out = first.letters_;
        out.insert(out.end(),second.letters_.begin(),second.letters_.end());
        return FreeGroupWord(std::move(out));
    }
private:
    std::vector<FreeGroupLetter> letters_;
    void reduce_in_place() {
        std::vector<FreeGroupLetter> reduced;
        reduced.reserve(letters_.size());
        for (const auto& letter : letters_) {
            if (!reduced.empty() && reduced.back().generator == letter.generator &&
                reduced.back().positive != letter.positive) reduced.pop_back();
            else reduced.push_back(letter);
        }
        letters_ = std::move(reduced);
    }
};

struct FundamentalGroupPresentation {
    const core::RelationalComplex* complex{};
    std::uint32_t basepoint{};
    std::vector<std::uint32_t> tree_relations;
    std::vector<std::uint32_t> generator_relations;
    std::vector<PairwisePath> generator_loops;
    std::vector<FreeGroupWord> relators;
};

inline FundamentalGroupPresentation fundamental_group_presentation(const core::RelationalComplex& complex,
                                                                   std::uint32_t basepoint = 0u) {
    if (complex.grade_count() < 2u) throw std::invalid_argument("fundamental group requires a grade-one skeleton");
    const auto vertex_count = complex.grade(core::Grade{0}).cell_count();
    const auto edge_count = complex.grade(core::Grade{1}).cell_count();
    if (basepoint >= vertex_count) throw std::out_of_range("fundamental group basepoint");

    struct Adj { std::uint32_t edge{}; std::uint32_t other{}; };
    std::vector<std::vector<Adj>> adjacency(vertex_count);
    std::vector<PairwiseEndpoints> endpoints(edge_count);
    for (std::size_t e = 0; e < edge_count; ++e) {
        endpoints[e] = pairwise_endpoints(complex,e);
        const auto u=endpoints[e].tail, v=endpoints[e].head;
        if (u != v) { adjacency[u].push_back({static_cast<std::uint32_t>(e),v}); adjacency[v].push_back({static_cast<std::uint32_t>(e),u}); }
    }

    std::vector<bool> visited(vertex_count,false), is_tree(edge_count,false);
    std::vector<std::uint32_t> parent(vertex_count,basepoint);
    std::vector<EdgeStep> parent_step(vertex_count);
    std::deque<std::uint32_t> queue;
    visited[basepoint]=true; queue.push_back(basepoint);
    while(!queue.empty()) {
        const auto u=queue.front(); queue.pop_front();
        for (const auto& a:adjacency[u]) if(!visited[a.other]) {
            visited[a.other]=true; is_tree[a.edge]=true; parent[a.other]=u;
            const auto ep=endpoints[a.edge];
            const bool forward=(ep.tail==u && ep.head==a.other);
            parent_step[a.other]=EdgeStep{a.edge,forward};
            queue.push_back(a.other);
        }
    }
    for (const auto seen:visited) if(!seen) throw std::invalid_argument("fundamental group presentation currently requires a connected pairwise 1-skeleton");

    std::vector<std::uint32_t> tree, generators;
    std::unordered_map<std::uint32_t,std::uint32_t> generator_index;
    for (std::uint32_t e=0;e<edge_count;++e) {
        if(is_tree[e]) tree.push_back(e);
        else { generator_index.emplace(e,static_cast<std::uint32_t>(generators.size())); generators.push_back(e); }
    }

    const auto tree_path = [&](std::uint32_t vertex) {
        std::vector<EdgeStep> reverse;
        while(vertex!=basepoint) {
            reverse.push_back(parent_step[vertex]);
            vertex=parent[vertex];
        }
        std::vector<EdgeStep> forward;
        forward.reserve(reverse.size());
        for(auto it=reverse.rbegin();it!=reverse.rend();++it) forward.push_back(*it);
        return PairwisePath(complex,basepoint,std::move(forward));
    };

    std::vector<PairwisePath> generator_loops;
    generator_loops.reserve(generators.size());
    for(const auto e:generators) {
        const auto ep=endpoints[e];
        const auto to_tail=tree_path(ep.tail);
        const PairwisePath edge_path(complex,ep.tail,{EdgeStep{e,true}});
        const auto to_head=tree_path(ep.head);
        generator_loops.push_back(PairwisePath::compose(PairwisePath::compose(to_tail,edge_path),to_head.inverse()));
    }

    std::vector<FreeGroupWord> relators;
    for (const auto& attaching:attaching_words(complex)) {
        std::vector<FreeGroupLetter> letters;
        for (const auto& step:attaching.loop.steps()) {
            if (const auto it=generator_index.find(step.relation); it!=generator_index.end()) letters.push_back({it->second,step.forward});
        }
        relators.emplace_back(std::move(letters));
    }
    return FundamentalGroupPresentation{&complex,basepoint,std::move(tree),std::move(generators),std::move(generator_loops),std::move(relators)};
}

inline numeric::IntegerMatrix presentation_abelianization_matrix(const FundamentalGroupPresentation& presentation) {
    numeric::IntegerMatrix out(presentation.generator_relations.size(),presentation.relators.size());
    for(std::size_t j=0;j<presentation.relators.size();++j) {
        for(const auto& letter:presentation.relators[j].letters()) out.at(letter.generator,j) += letter.positive ? 1 : -1;
    }
    return out;
}

struct PresentedAbelianGroup {
    std::size_t free_rank{};
    std::vector<numeric::Integer> torsion_invariants;
};

inline PresentedAbelianGroup abelianization(const FundamentalGroupPresentation& presentation) {
    const auto matrix=presentation_abelianization_matrix(presentation);
    const auto snf=numeric::smith_normal_form(matrix);
    std::vector<numeric::Integer> torsion;
    for(const auto& d:snf.invariant_factors) if(d>1) torsion.push_back(d);
    return PresentedAbelianGroup{presentation.generator_relations.size()-snf.rank,std::move(torsion)};
}

} // namespace rfc::topology
