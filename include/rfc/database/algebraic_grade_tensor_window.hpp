#pragma once
#include "rfc/database/algebraic_grade_tensor.hpp"

namespace rfc::database {
// An explicit reversible shape of the full stalk direct sum. Shape axes do not
// introduce primary cells, transports, semantic scales or metric adjoints.
class AlgebraicGradeTensorWindow {
    struct Plan {
        AlgebraicGradeTensor tensor;
        numeric::IndexDomain source,window;
        std::vector<std::size_t> lower,upper;
        runtime::ExecutionRequirements requirements;
    };
public:
    static AlgebraicGradeTensorWindow prepare(const AlgebraicGradeTensor& tensor,const numeric::IndexDomain& shape,
        const std::vector<std::size_t>& lower,const std::vector<std::size_t>& upper,runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(tensor.source().source_namespace(),tensor.source().sequence(),availability);
        if (shape.logical_size()!=tensor.domain().logical_size()) throw std::invalid_argument("tensor shape must cover the complete original stalk direct sum");
        if (lower.size()!=shape.rank() || upper.size()!=shape.rank()) throw std::invalid_argument("grade tensor window rank differs");
        runtime::ExecutionRequirements required;required.records=machine::checked_size_add(1,machine::checked_size_mul(4,shape.rank()));
        runtime::admit_execution(required,lease.policy());std::vector<std::size_t> extents;extents.reserve(shape.rank());
        for (std::size_t i=0;i<shape.rank();++i) {
            if (lower[i]>upper[i] || upper[i]>shape.extents()[i]) throw std::out_of_range("grade tensor window bounds");
            extents.push_back(upper[i]-lower[i]);
        }
        auto window=numeric::IndexDomain(std::move(extents));lease.check_cancelled();
        return AlgebraicGradeTensorWindow(std::make_shared<const Plan>(Plan{tensor,shape,std::move(window),lower,upper,required}));
    }
    const AlgebraicGradeTensor& tensor() const & {return plan().tensor;}
    const AlgebraicGradeTensor& tensor() const &&=delete;
    const numeric::IndexDomain& source_domain() const & {return plan().source;}
    const numeric::IndexDomain& source_domain() const &&=delete;
    const numeric::IndexDomain& domain() const & {return plan().window;}
    const numeric::IndexDomain& domain() const &&=delete;
    runtime::ExecutionRequirements requirements() const {return plan().requirements;}
    std::size_t supported_count() const {return domain().logical_size();}
    std::size_t source_index(std::size_t position) const {
        const auto& p=plan();if (position>=p.window.logical_size()) throw std::out_of_range("grade tensor window coordinate");
        std::size_t out{};
        for (std::size_t i=0;i<p.window.rank();++i)
            out+=((position/p.window.strides()[i])%p.window.extents()[i]+p.lower[i])*p.source.strides()[i];
        return out;
    }
    std::optional<std::size_t> window_index(std::size_t source_position) const {
        const auto& p=plan();if (source_position>=p.source.logical_size()) throw std::out_of_range("grade tensor source coordinate");
        std::size_t out{};
        for (std::size_t i=0;i<p.source.rank();++i) {
            const auto coordinate=(source_position/p.source.strides()[i])%p.source.extents()[i];
            if (coordinate<p.lower[i] || coordinate>=p.upper[i]) return std::nullopt;
            out+=(coordinate-p.lower[i])*p.window.strides()[i];
        }
        return out;
    }
    AlgebraicStalkCoordinate source_coordinate(std::size_t position) const {return tensor().source_coordinate(source_index(position));}
    numeric::AlgebraicElement coordinate(std::size_t position) const {return tensor().coordinate(source_index(position));}
    std::optional<numeric::AlgebraicElement> sum(runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        lease.check_cancelled();runtime::admit_work_availability(tensor().source().source_namespace(),tensor().source().sequence(),availability);
        auto required=requirements();const auto& field=tensor().source().archive().bundle().field();
        required.rational_slots=machine::checked_size_mul(2,field.degree());
        required.scalar_work=machine::checked_size_mul(supported_count(),machine::checked_size_add(field.degree(),source_domain().rank()));
        runtime::admit_execution(required,lease.policy());std::size_t bits=1;
        for (std::size_t i=0;i<supported_count();++i) {
            lease.check_cancelled();bits=std::max(bits,algebraic_database_detail::bits(coordinate(i)));
        }
        if (supported_count()) required.integer_bits=machine::checked_size_mul(2,machine::checked_size_add(1,
            machine::checked_size_mul(supported_count(),machine::checked_size_add(bits,1))));
        runtime::admit_execution(required,lease.policy());if (!supported_count()) return std::nullopt;
        auto out=field.scalar(0);
        for (std::size_t i=0;i<supported_count();++i) {
            lease.check_cancelled();out=out+coordinate(i);
            const auto actual=algebraic_database_detail::bits(out);
            if (actual>lease.policy().limits.integer_bits) throw runtime::ExecutionLimitExceeded("grade tensor contraction bits",actual,lease.policy().limits.integer_bits);
        }
        lease.check_cancelled();return out;
    }
private:
    explicit AlgebraicGradeTensorWindow(std::shared_ptr<const Plan> plan):plan_(std::move(plan)){}
    const Plan& plan() const {if (!plan_) throw std::invalid_argument("moved grade tensor window");return *plan_;}
    std::shared_ptr<const Plan> plan_;
};
} // namespace rfc::database
