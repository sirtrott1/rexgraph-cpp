#pragma once

#include "rfc/temporal/chronological.hpp"
#include <span>

namespace rfc::temporal {
// Retains exactly the declared forward prefix. Later maps/injections are neither
// copied nor validated nor used to compute this observation.
class ChronologicalPrefix {
public:
    static ChronologicalPrefix capture(numeric::RationalVector initial,
        std::span<const numeric::ExactMatrix> maps,std::span<const numeric::RationalVector> injections,
        std::size_t steps) {
        if(steps>maps.size() || steps>injections.size())throw std::out_of_range("chronological prefix exceeds available steps");
        const auto prefix_maps=maps.first(steps);
        const auto prefix_injections=injections.first(steps);
        return ChronologicalPrefix(std::move(initial),
            std::vector<numeric::ExactMatrix>(prefix_maps.begin(),prefix_maps.end()),
            std::vector<numeric::RationalVector>(prefix_injections.begin(),prefix_injections.end()));
    }
    std::size_t steps() const noexcept {return evolution_.maps().size();}
    std::vector<numeric::RationalVector> states() const {return evolution_.states();}
    numeric::RationalVector terminal() const {return evolution_.terminal();}
private:
    ChronologicalPrefix(numeric::RationalVector initial,std::vector<numeric::ExactMatrix> maps,
                        std::vector<numeric::RationalVector> injections)
        :evolution_(std::move(initial),std::move(maps),std::move(injections)){}
    ChronologicalEvolution evolution_;
};
} // namespace rfc::temporal
