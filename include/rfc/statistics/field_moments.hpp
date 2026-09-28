#pragma once
#include "rfc/field/exact_hodge.hpp"
#include "rfc/numeric/exact_matrix.hpp"
#include <cstddef>
#include <stdexcept>

namespace rfc::statistics {

struct AdjacentFieldMoment {
    numeric::Rational down{0};
    numeric::Rational up{0};
    numeric::Rational total{0};
    numeric::Rational oriented{0};
};

inline AdjacentFieldMoment adjacent_field_moment(const field::ExactHodgeOperator& hodge,
                                                 const numeric::RationalVector& x,
                                                 const numeric::RationalVector& y) {
    if(x.size()!=hodge.size() || y.size()!=hodge.size()) throw std::invalid_argument("adjacent moment field size mismatch");
    const auto down_y=hodge.apply_down(y);
    const auto up_y=hodge.apply_up(y);
    const auto down=hodge.metric().inner(x,down_y);
    const auto up=hodge.metric().inner(x,up_y);
    numeric::Rational total=down+up; total.canonicalize();
    numeric::Rational oriented=down-up; oriented.canonicalize();
    return {down,up,total,oriented};
}

inline numeric::RationalVector hodge_power_apply(const field::ExactHodgeOperator& hodge,
                                                 std::size_t power,
                                                 numeric::RationalVector value) {
    if(value.size()!=hodge.size()) throw std::invalid_argument("Hodge power field size mismatch");
    for(std::size_t i=0;i<power;++i) value=hodge.apply(value);
    return value;
}

inline numeric::RationalVector down_power_apply(const field::ExactHodgeOperator& hodge,
                                                std::size_t power,
                                                numeric::RationalVector value) {
    if(value.size()!=hodge.size()) throw std::invalid_argument("down power field size mismatch");
    for(std::size_t i=0;i<power;++i) value=hodge.apply_down(value);
    return value;
}

inline numeric::RationalVector up_power_apply(const field::ExactHodgeOperator& hodge,
                                              std::size_t power,
                                              numeric::RationalVector value) {
    if(value.size()!=hodge.size()) throw std::invalid_argument("up power field size mismatch");
    for(std::size_t i=0;i<power;++i) value=hodge.apply_up(value);
    return value;
}

struct PowerMoment {
    std::size_t order{};
    numeric::Rational down{0};
    numeric::Rational up{0};
    numeric::Rational total{0};
    numeric::Rational oriented{0};
};

// mu_{k,n}=<x,L^n y>, nu_{k,n}=<x,K L^{n-1}y>.  The chain law makes
// L_down L_up=L_up L_down=0, so the two sector powers may be evaluated
// independently and their sum/difference recovers the total/oriented moments.
inline PowerMoment power_moment(const field::ExactHodgeOperator& hodge,
                                const numeric::RationalVector& x,
                                const numeric::RationalVector& y,
                                std::size_t order) {
    if(order==0) throw std::invalid_argument("power moment order must be positive");
    if(x.size()!=hodge.size() || y.size()!=hodge.size()) throw std::invalid_argument("power moment field size mismatch");
    const auto down_y=down_power_apply(hodge,order,y);
    const auto up_y=up_power_apply(hodge,order,y);
    const auto down=hodge.metric().inner(x,down_y);
    const auto up=hodge.metric().inner(x,up_y);
    numeric::Rational total=down+up; total.canonicalize();
    numeric::Rational oriented=down-up; oriented.canonicalize();

    // Independent action cross-check of the defining formulas.  This is cheap
    // for the small exact orders used by diagnostics and catches any future
    // violation of the down/up annihilation contract.
    const auto Ly=hodge_power_apply(hodge,order,y);
    const auto mu=hodge.metric().inner(x,Ly);
    auto prefix=hodge_power_apply(hodge,order-1u,y);
    const auto Ky=numeric::sub(hodge.apply_down(prefix),hodge.apply_up(prefix));
    const auto nu=hodge.metric().inner(x,Ky);
    if(mu!=total || nu!=oriented) throw std::logic_error("Hodge power-moment sector identity failed");
    return {order,down,up,total,oriented};
}

struct DiracAntiDiracMomentTensor {
    numeric::Rational dd{0};
    numeric::Rational da{0};
    numeric::Rational ad{0};
    numeric::Rational aa{0};
};

inline DiracAntiDiracMomentTensor dirac_antidirac_moment_tensor(
    const field::ExactHodgeOperator& hodge,
    const numeric::RationalVector& x,
    const numeric::RationalVector& y) {
    const auto moment=adjacent_field_moment(hodge,x,y);
    return {moment.total,moment.oriented,moment.oriented,moment.total};
}

inline numeric::ExactMatrix dirac_antidirac_moment_matrix(const field::ExactHodgeOperator& hodge,
                                                           const numeric::RationalVector& x,
                                                           const numeric::RationalVector& y) {
    const auto m=dirac_antidirac_moment_tensor(hodge,x,y);
    numeric::ExactMatrix out(2,2);
    if(m.dd!=0) out.set(0,0,m.dd);
    if(m.da!=0) out.set(0,1,m.da);
    if(m.ad!=0) out.set(1,0,m.ad);
    if(m.aa!=0) out.set(1,1,m.aa);
    return out;
}

} // namespace rfc::statistics
