#pragma once
#include "rfc/local/algebraic_module.hpp"

namespace rfc::local {
// A linear functional on one actual module, not a vector identified by size.
class AlgebraicCovector {
public:
    static AlgebraicCovector declare(const AlgebraicModule& carrier,
                                    std::vector<numeric::AlgebraicElement> coefficients) {
        (void)carrier.vector(coefficients);
        return AlgebraicCovector(carrier,std::move(coefficients));
    }
    const AlgebraicModule& carrier() const & noexcept { return carrier_; }
    const std::vector<numeric::AlgebraicElement>& coefficients() const & noexcept { return coefficients_; }
    const AlgebraicModule& carrier() const && = delete;
    const std::vector<numeric::AlgebraicElement>& coefficients() const && = delete;
    numeric::AlgebraicElement evaluate(const AlgebraicModuleVector& value) const {
        if (!carrier_.same_owner(value.carrier())) throw std::invalid_argument("algebraic dual carrier owner differs");
        auto result=carrier_.field().scalar(numeric::Rational{0});
        for (std::size_t i=0;i<coefficients_.size();++i) result=result+coefficients_[i]*value.coordinates()[i];
        return result;
    }
private:
    AlgebraicCovector(AlgebraicModule carrier,std::vector<numeric::AlgebraicElement> coefficients)
        : carrier_(std::move(carrier)),coefficients_(std::move(coefficients)) {}
    AlgebraicModule carrier_;
    std::vector<numeric::AlgebraicElement> coefficients_;
};

// A*: V* -> U* is defined by evaluation, without a metric or involution.
class AlgebraicDualMap {
public:
    explicit AlgebraicDualMap(AlgebraicModuleMap map) : map_(std::move(map)) {}
    const AlgebraicModuleMap& primal_map() const & noexcept { return map_; }
    const AlgebraicModuleMap& primal_map() const && = delete;
    AlgebraicCovector apply(const AlgebraicCovector& value) const {
        if (!map_.target().same_owner(value.carrier())) throw std::invalid_argument("dual map input owner differs");
        std::vector<numeric::AlgebraicElement> out(map_.source().rank(),map_.source().field().scalar(numeric::Rational{0}));
        for (std::size_t j=0;j<out.size();++j) for (std::size_t i=0;i<map_.target().rank();++i)
            out[j]=out[j]+value.coefficients()[i]*map_.coefficients()[i*out.size()+j];
        return AlgebraicCovector::declare(map_.source(),std::move(out));
    }
    // Diagnostic coordinate transpose on the full rational scalar restriction.
    // Rational coordinate dual coefficients differ from K-valued dual coefficients.
    numeric::ExactMatrix rational_coordinate_transpose() const { return map_.rational_action().transpose(); }
private:
    AlgebraicModuleMap map_;
};
} // namespace rfc::local
