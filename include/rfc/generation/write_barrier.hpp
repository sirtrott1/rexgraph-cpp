#pragma once
#include "rfc/capability/epistemic.hpp"
#include <stdexcept>

namespace rfc::generation {

class CanonicalWriteBarrier {
public:
    static void require_canonical_write(capability::EpistemicClass epistemic) {
        if (epistemic == capability::EpistemicClass::Prospective ||
            epistemic == capability::EpistemicClass::Proposed ||
            epistemic == capability::EpistemicClass::Unknown) {
            throw std::logic_error("prospective/proposed outputs require validation before canonical write");
        }
    }
};

} // namespace rfc::generation
