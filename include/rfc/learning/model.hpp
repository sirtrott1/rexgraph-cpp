#pragma once
#include "rfc/learning/module.hpp"
#include "rfc/learning/scalar_policy.hpp"
#include <string>
#include <vector>

namespace rfc::learning {

template <class Scalar>
struct Prediction {
    std::vector<Scalar> values;
    std::string label;
};

template <class Scalar>
class ProspectiveModel : public Module<Scalar> {
public:
    ~ProspectiveModel() override = default;
    virtual Prediction<Scalar> predict(const RelationalBatch<Scalar>& input) = 0;
    virtual ScalarPolicy scalar_policy() const noexcept = 0;
};

} // namespace rfc::learning
