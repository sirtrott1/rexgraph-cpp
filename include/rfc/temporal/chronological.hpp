#pragma once
#include "rfc/temporal/operation_word.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::temporal {

class ChronologicalEvolution {
public:
    ChronologicalEvolution(numeric::RationalVector initial,
                           std::vector<numeric::ExactMatrix> evolution,
                           std::vector<numeric::RationalVector> injections)
        : initial_(std::move(initial)), evolution_(std::move(evolution)), injections_(std::move(injections)) {
        if (injections_.size() != evolution_.size()) throw std::invalid_argument("chronological injection count mismatch");
        std::size_t current = initial_.size();
        for (std::size_t i = 0; i < evolution_.size(); ++i) {
            if (evolution_[i].cols() != current || injections_[i].size() != evolution_[i].rows()) throw std::invalid_argument("chronological evolution dimensions");
            current = evolution_[i].rows();
        }
    }

    [[nodiscard]] std::vector<numeric::RationalVector> states() const {
        std::vector<numeric::RationalVector> out;
        out.reserve(evolution_.size() + 1u);
        out.push_back(initial_);
        auto x = initial_;
        for (std::size_t i = 0; i < evolution_.size(); ++i) {
            x = numeric::add(evolution_[i].apply(x), injections_[i]);
            out.push_back(x);
        }
        return out;
    }

    [[nodiscard]] numeric::RationalVector terminal() const { return states().back(); }
    [[nodiscard]] const std::vector<numeric::ExactMatrix>& maps() const noexcept { return evolution_; }

private:
    numeric::RationalVector initial_;
    std::vector<numeric::ExactMatrix> evolution_;
    std::vector<numeric::RationalVector> injections_;
};

struct ScheduledContact {
    std::uint32_t source{};
    std::uint32_t target{};
    numeric::Rational departure{0};
    numeric::Rational arrival{0};
};

struct TemporalRoute {
    bool reachable{};
    numeric::Rational arrival{0};
    std::size_t contacts{};
    std::vector<std::size_t> contact_indices;
};

namespace detail {
inline void validate_scheduled_contacts(std::size_t object_count,
                                        const std::vector<ScheduledContact>& contacts) {
    for (const auto& contact : contacts) {
        if (contact.source >= object_count || contact.target >= object_count ||
            contact.arrival < contact.departure) {
            throw std::invalid_argument("invalid scheduled contact");
        }
    }
}
} // namespace detail

inline TemporalRoute earliest_arrival(std::size_t object_count,
                                      const std::vector<ScheduledContact>& contacts,
                                      std::uint32_t source,
                                      std::uint32_t target,
                                      numeric::Rational start_time = numeric::Rational{0}) {
    if (source >= object_count || target >= object_count) throw std::out_of_range("temporal route object index");
    detail::validate_scheduled_contacts(object_count, contacts);
    // A witness points to an immutable earlier witness, not a mutable vertex
    // label.  Improving an intermediate arrival must not rewrite an already
    // issued descendant route or leave its recorded contact count stale.
    struct Witness { std::size_t contact; std::optional<std::size_t> previous; };
    struct Label { numeric::Rational time; std::size_t hops; std::optional<std::size_t> witness; };
    std::vector<Witness> witnesses;
    std::vector<std::optional<Label>> labels(object_count);
    labels[source] = Label{start_time, 0u, std::nullopt};
    bool changed = true;
    for (std::size_t pass = 0; pass < object_count && changed; ++pass) {
        changed = false;
        for (std::size_t i = 0; i < contacts.size(); ++i) {
            const auto& c = contacts[i];
            if (!labels[c.source] || labels[c.source]->time > c.departure) continue;
            const auto hops = labels[c.source]->hops + 1u;
            if (!labels[c.target] || c.arrival < labels[c.target]->time ||
                (c.arrival == labels[c.target]->time && hops < labels[c.target]->hops)) {
                const auto previous = labels[c.source]->witness;
                witnesses.push_back(Witness{i, previous});
                labels[c.target] = Label{c.arrival, hops, witnesses.size() - 1u};
                changed = true;
            }
        }
    }
    if (!labels[target]) return {};
    std::vector<std::size_t> reversed;
    for (auto witness = labels[target]->witness; witness;) {
        const auto& step = witnesses[*witness];
        reversed.push_back(step.contact);
        witness = step.previous;
    }
    std::vector<std::size_t> path(reversed.rbegin(), reversed.rend());
    return {true, labels[target]->time, path.size(), std::move(path)};
}

inline TemporalRoute fewest_contacts(std::size_t object_count,
                                     const std::vector<ScheduledContact>& contacts,
                                     std::uint32_t source,
                                     std::uint32_t target,
                                     numeric::Rational start_time = numeric::Rational{0}) {
    if (source >= object_count || target >= object_count) throw std::out_of_range("temporal route object index");
    detail::validate_scheduled_contacts(object_count, contacts);
    // Dynamic labels by contact count. For equal count keep earliest arrival.
    struct Label { std::size_t hops; numeric::Rational time; std::uint32_t vertex; std::vector<std::size_t> path; };
    std::vector<Label> frontier{{0u, start_time, source, {}}};
    for (std::size_t depth = 0; depth <= contacts.size(); ++depth) {
        std::vector<std::optional<Label>> best(object_count);
        for (const auto& label : frontier) {
            if (label.vertex == target) return {true, label.time, label.hops, label.path};
            for (std::size_t i = 0; i < contacts.size(); ++i) {
                const auto& c = contacts[i];
                if (c.source != label.vertex || label.time > c.departure) continue;
                Label next{label.hops + 1u, c.arrival, c.target, label.path}; next.path.push_back(i);
                if (!best[c.target] || next.time < best[c.target]->time) best[c.target] = std::move(next);
            }
        }
        frontier.clear();
        for (auto& item : best) if (item) frontier.push_back(std::move(*item));
        if (frontier.empty()) break;
    }
    return {};
}

inline numeric::ExactMatrix terminal_channel(const std::vector<numeric::ExactMatrix>& evolution,
                                             std::size_t injection_time,
                                             const numeric::ExactMatrix& injection,
                                             const numeric::ExactMatrix& terminal_observation) {
    if (injection_time > evolution.size()) throw std::out_of_range("terminal channel injection time");
    numeric::ExactMatrix current = injection;
    for (std::size_t i = injection_time; i < evolution.size(); ++i) current = evolution[i].multiply(current);
    return terminal_observation.multiply(current);
}

} // namespace rfc::temporal
