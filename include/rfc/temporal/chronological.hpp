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

inline TemporalRoute earliest_arrival(std::size_t object_count,
                                      const std::vector<ScheduledContact>& contacts,
                                      std::uint32_t source,
                                      std::uint32_t target,
                                      numeric::Rational start_time = numeric::Rational{0}) {
    if (source >= object_count || target >= object_count) throw std::out_of_range("temporal route object index");
    std::vector<std::optional<numeric::Rational>> arrival(object_count);
    std::vector<std::size_t> hops(object_count, static_cast<std::size_t>(-1));
    std::vector<std::optional<std::size_t>> parent_contact(object_count);
    arrival[source] = start_time; hops[source] = 0;
    bool changed = true;
    for (std::size_t pass = 0; pass < object_count && changed; ++pass) {
        changed = false;
        for (std::size_t i = 0; i < contacts.size(); ++i) {
            const auto& c = contacts[i];
            if (c.source >= object_count || c.target >= object_count || c.arrival < c.departure) throw std::invalid_argument("invalid scheduled contact");
            if (!arrival[c.source] || *arrival[c.source] > c.departure) continue;
            if (!arrival[c.target] || c.arrival < *arrival[c.target]) {
                arrival[c.target] = c.arrival;
                hops[c.target] = hops[c.source] + 1u;
                parent_contact[c.target] = i;
                changed = true;
            }
        }
    }
    if (!arrival[target]) return {};
    std::vector<std::size_t> reversed;
    for (auto v = target; v != source;) {
        if (!parent_contact[v]) break;
        const auto idx = *parent_contact[v]; reversed.push_back(idx); v = contacts[idx].source;
    }
    std::vector<std::size_t> path(reversed.rbegin(), reversed.rend());
    return {true, *arrival[target], hops[target], std::move(path)};
}

inline TemporalRoute fewest_contacts(std::size_t object_count,
                                     const std::vector<ScheduledContact>& contacts,
                                     std::uint32_t source,
                                     std::uint32_t target,
                                     numeric::Rational start_time = numeric::Rational{0}) {
    if (source >= object_count || target >= object_count) throw std::out_of_range("temporal route object index");
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
