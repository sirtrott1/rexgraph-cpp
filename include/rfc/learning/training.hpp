#pragma once
#include "rfc/learning/model.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace rfc::learning {

template <class Scalar>
struct TrainableParameter {
    std::string name;
    std::vector<Scalar> values;
    std::vector<Scalar> gradients;
};

template <class Scalar>
class Optimizer {
public:
    virtual ~Optimizer() = default;
    virtual void step(std::vector<TrainableParameter<Scalar>>& parameters) = 0;
};

template <class Scalar>
class Loss {
public:
    virtual ~Loss() = default;
    virtual Scalar evaluate(const Prediction<Scalar>& prediction,
                            const std::vector<Scalar>& target) const = 0;
};

template <class Scalar>
struct TrainingStep {
    std::size_t step{};
    Scalar loss{};
};

} // namespace rfc::learning
