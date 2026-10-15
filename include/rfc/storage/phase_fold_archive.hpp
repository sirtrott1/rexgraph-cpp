#pragma once
#include "rfc/temporal/fold_history.hpp"
#include <variant>

namespace rfc::storage {
using RationalPhaseWalk=temporal::CyclicPhaseWalk<numeric::RationalPhase>;
using AlgebraicPhaseWalk=temporal::CyclicPhaseWalk<numeric::AlgebraicUnitPhase>;
using PhaseFoldWord=std::variant<temporal::RationalPhaseHistory,temporal::AlgebraicPhaseHistory,
    temporal::RationalFoldHistory,RationalPhaseWalk,AlgebraicPhaseWalk>;
using PhaseFoldValue=std::variant<numeric::RationalPhase,numeric::AlgebraicUnitPhase>;
struct PhaseFoldProposal {
    std::uint64_t key;
    std::size_t position;
    PhaseFoldValue replacement;
    bool active;
};

// One complete original word and a separate table of prospective replacements.
// A proposal is bound to this word, never an observed operation. Applying one
// branches the actual carrier and replays the suffix through the temporal API.
class PhaseFoldArchive {
    struct State {PhaseFoldWord word;std::vector<PhaseFoldProposal> proposals;};
public:
    static PhaseFoldArchive declare(PhaseFoldWord word) {
        if (word.valueless_by_exception()) throw std::invalid_argument("phase/fold archive needs a declared word");
        return PhaseFoldArchive(std::make_shared<const State>(State{std::move(word),{}}));
    }
    const PhaseFoldWord& word() const & noexcept {return state_->word;}
    const PhaseFoldWord& word() const && = delete;
    const std::vector<PhaseFoldProposal>& proposals() const & noexcept {return state_->proposals;}
    const std::vector<PhaseFoldProposal>& proposals() const && = delete;
    bool same_owner(const PhaseFoldArchive& other) const noexcept {return state_==other.state_;}
    const core::RelationalComplex& source() const & noexcept {
        return std::visit([](const auto& word) -> const core::RelationalComplex& {
            using T=std::decay_t<decltype(word)>;
            if constexpr (std::same_as<T,temporal::RationalFoldHistory>) return word.assembly().source();
            else if constexpr (std::same_as<T,RationalPhaseWalk> || std::same_as<T,AlgebraicPhaseWalk>) return word.lift().cover().source();
            else return word.source();
        },word());
    }
    const core::RelationalComplex& source() const && = delete;
    std::size_t operation_count() const noexcept {
        return std::visit([](const auto& word) {
            using T=std::decay_t<decltype(word)>;
            if constexpr (std::same_as<T,RationalPhaseWalk> || std::same_as<T,AlgebraicPhaseWalk>) return word.lift().path().steps().size();
            else return word.operations().size();
        },word());
    }
    PhaseFoldArchive propose(std::uint64_t key,std::size_t position,numeric::RationalPhase phase,bool active) const {
        if (const auto* h=std::get_if<temporal::RationalPhaseHistory>(&word())) (void)h->propose(position,phase,active);
        else if (const auto* fold=std::get_if<temporal::RationalFoldHistory>(&word())) (void)fold->propose(position,phase,active);
        else throw std::invalid_argument("rational proposal requires a rational phase or fold word");
        return add({key,position,std::move(phase),active});
    }
    PhaseFoldArchive propose(std::uint64_t key,std::size_t position,numeric::AlgebraicUnitPhase phase,bool active) const {
        const auto* h=std::get_if<temporal::AlgebraicPhaseHistory>(&word());
        if (!h) throw std::invalid_argument("algebraic proposal requires an algebraic phase word");
        (void)h->propose(position,phase,active);
        return add({key,position,std::move(phase),active});
    }
    PhaseFoldArchive apply(std::uint64_t key) const {
        const auto found=std::find_if(proposals().begin(),proposals().end(),[&](const auto& proposal){return proposal.key==key;});
        if (found==proposals().end()) throw std::out_of_range("phase/fold proposal absent");
        return std::visit([&](const auto& h) -> PhaseFoldArchive {
            using T=std::decay_t<decltype(h)>;
            if constexpr (std::same_as<T,temporal::RationalPhaseHistory> || std::same_as<T,temporal::RationalFoldHistory>)
                return declare(h.apply(h.propose(found->position,std::get<numeric::RationalPhase>(found->replacement),found->active)));
            else if constexpr (std::same_as<T,temporal::AlgebraicPhaseHistory>)
                return declare(h.apply(h.propose(found->position,std::get<numeric::AlgebraicUnitPhase>(found->replacement),found->active)));
            else throw std::invalid_argument("cyclic walk replacement requires a separately declared path");
        },word());
    }
    // No pointer to a suffix-bearing archive or pending replacement is retained.
    PhaseFoldArchive observed_prefix(std::size_t count) const {
        return std::visit([&](const auto& h){return declare(h.prefix(count));},word());
    }
private:
    explicit PhaseFoldArchive(std::shared_ptr<const State> state):state_(std::move(state)) {}
    PhaseFoldArchive add(PhaseFoldProposal proposal) const {
        if (!proposal.key || std::any_of(proposals().begin(),proposals().end(),[&](const auto& p){return p.key==proposal.key;}))
            throw std::invalid_argument("phase/fold proposal names must be nonzero and unique");
        auto next=std::make_shared<State>(*state_);next->proposals.push_back(std::move(proposal));
        return PhaseFoldArchive(std::move(next));
    }
    std::shared_ptr<const State> state_;
};
} // namespace rfc::storage
