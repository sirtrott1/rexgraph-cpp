#pragma once

// Compatibility forwarding header.
//
// The canonical finite/set-valued local constraint API is
// rfc/local/constraint_system.hpp and rfc::local::FiniteConstraintSystem<Value>.
// This header remains so pre-realignment source that included
// constraint_sheaf.hpp continues to compile.  The compatibility aliases in the
// canonical header do not imply that an arbitrary finite CSP is a sheaf.
#include "rfc/local/constraint_system.hpp"
