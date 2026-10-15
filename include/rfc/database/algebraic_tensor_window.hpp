#pragma once
#include "rfc/database/algebraic_snapshot.hpp"

namespace rfc::database {
class AlgebraicTensorWindow {
    struct Plan {
        AlgebraicRecordPin record;
        numeric::IndexDomain source,window;
        std::vector<std::size_t> lower,upper;
        runtime::ExecutionRequirements requirements;
    };
public:
    static AlgebraicTensorWindow prepare(const AlgebraicRecordPin& record,
        const std::vector<std::size_t>& lower,const std::vector<std::size_t>& upper,
        runtime::WorkAvailability availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(record.source_namespace(),record.sequence(),availability);
        const auto source=record.domain();
        if (lower.size()!=source.rank() || upper.size()!=source.rank()) throw std::invalid_argument("algebraic tensor window rank differs");
        runtime::ExecutionRequirements required;
        required.records=machine::checked_size_add(1,machine::checked_size_mul(4,source.rank()));
        runtime::admit_execution(required,lease.policy());
        std::vector<std::size_t> extents;extents.reserve(source.rank());
        for (std::size_t axis=0;axis<source.rank();++axis) {
            if (lower[axis]>upper[axis] || upper[axis]>source.extents()[axis]) throw std::out_of_range("algebraic tensor window bounds");
            extents.push_back(upper[axis]-lower[axis]);
        }
        numeric::IndexDomain window(std::move(extents));
        const auto carrier=record.source_carrier();const auto degree=carrier.field().degree();
        required.rational_slots=machine::checked_size_mul(2,degree);
        required.scalar_work=machine::checked_size_mul(window.logical_size(),machine::checked_size_add(degree,source.rank()));
        runtime::admit_execution(required,lease.policy());
        auto p=std::make_shared<Plan>(Plan{record,source,std::move(window),lower,upper,required});
        std::size_t maximum_bits=1;
        for (std::size_t i=0;i<p->window.logical_size();++i) {
            lease.check_cancelled();maximum_bits=std::max(maximum_bits,algebraic_database_detail::bits(record.coefficients()[index(*p,i)]));
        }
        if (p->window.logical_size()) p->requirements.integer_bits=machine::checked_size_mul(2,machine::checked_size_add(1,
            machine::checked_size_mul(p->window.logical_size(),machine::checked_size_add(maximum_bits,1))));
        runtime::admit_execution(p->requirements,lease.policy());lease.check_cancelled();
        return AlgebraicTensorWindow(std::move(p));
    }
    const numeric::IndexDomain& domain() const & { return plan().window; }
    const numeric::IndexDomain& domain() const && = delete;
    const numeric::IndexDomain& source_domain() const & { return plan().source; }
    const numeric::IndexDomain& source_domain() const && = delete;
    const AlgebraicRecordPin& record() const & { return plan().record; }
    const AlgebraicRecordPin& record() const && = delete;
    const std::vector<std::size_t>& lower() const & { return plan().lower; }
    const std::vector<std::size_t>& lower() const && = delete;
    const std::vector<std::size_t>& upper() const & { return plan().upper; }
    const std::vector<std::size_t>& upper() const && = delete;
    std::size_t source_index(std::size_t coordinate) const { return index(plan(),coordinate); }
    std::optional<std::size_t> window_index(std::size_t coordinate) const {
        const auto& p=plan();
        if (coordinate>=p.source.logical_size()) throw std::out_of_range("algebraic tensor source coordinate");
        std::size_t out{};
        for (std::size_t axis=0;axis<p.source.rank();++axis) {
            const auto position=(coordinate/p.source.strides()[axis])%p.source.extents()[axis];
            if (position<p.lower[axis] || position>=p.upper[axis]) return std::nullopt;
            out+=(position-p.lower[axis])*p.window.strides()[axis];
        }
        return out;
    }
    numeric::AlgebraicElement coordinate(std::size_t position) const { return record().coefficients().at(source_index(position)); }
    std::size_t supported_count() const { return domain().logical_size(); }
    runtime::ExecutionRequirements requirements() const { return plan().requirements; }
    // This is a declared coefficient contraction, not a carrier vector, a trace,
    // a metric quadrance or an embedding projection. An empty box has no sum.
    std::optional<numeric::AlgebraicElement> sum(runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        lease.check_cancelled();runtime::admit_work_availability(record().source_namespace(),record().sequence(),availability);
        runtime::admit_execution(requirements(),lease.policy());
        if (!supported_count()) return std::nullopt;
        const auto carrier=record().source_carrier();auto out=carrier.field().scalar(numeric::Rational{0});
        for (std::size_t i=0;i<supported_count();++i) {
            lease.check_cancelled();out=out+coordinate(i);
            const auto bits=algebraic_database_detail::bits(out);
            if (bits>lease.policy().limits.integer_bits) throw runtime::ExecutionLimitExceeded("algebraic tensor sum bits",bits,lease.policy().limits.integer_bits);
        }
        lease.check_cancelled();return out;
    }
private:
    static std::size_t index(const Plan& p,std::size_t coordinate) {
        if (coordinate>=p.window.logical_size()) throw std::out_of_range("algebraic tensor window coordinate");
        std::size_t out{};
        for (std::size_t axis=0;axis<p.window.rank();++axis)
            out+=((coordinate/p.window.strides()[axis])%p.window.extents()[axis]+p.lower[axis])*p.source.strides()[axis];
        return out;
    }
    explicit AlgebraicTensorWindow(std::shared_ptr<const Plan> p):plan_(std::move(p)) {}
    const Plan& plan() const { if (!plan_) throw std::invalid_argument("moved algebraic tensor window");return *plan_; }
    std::shared_ptr<const Plan> plan_;
};
} // namespace rfc::database
