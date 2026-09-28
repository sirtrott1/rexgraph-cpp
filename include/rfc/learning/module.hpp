#pragma once
#include "rfc/learning/field_batch.hpp"
#include <memory>
#include <string_view>

namespace rfc::learning {

template <class Scalar>
class Module {
public:
    virtual ~Module() = default;
    virtual std::string_view name() const noexcept = 0;
    virtual RelationalBatch<Scalar> forward(const RelationalBatch<Scalar>& input) = 0;
};

template <class Scalar>
using ModulePtr = std::unique_ptr<Module<Scalar>>;

} // namespace rfc::learning
