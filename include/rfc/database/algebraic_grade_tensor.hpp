#pragma once
#include "rfc/database/algebraic_snapshot.hpp"

namespace rfc::database {
struct AlgebraicStalkCoordinate {
    core::CellId cell;
    std::size_t coordinate;
    bool operator==(const AlgebraicStalkCoordinate&) const=default;
};

// The full direct sum of the original grade's stalks. Selection names one
// actual vector per cell; zero values never supply missing cells or carriers.
class AlgebraicGradeTensor {
    struct Plan {
        AlgebraicSnapshot source;
        core::Grade grade;
        std::vector<std::shared_ptr<const local::AlgebraicModuleVector>> values;
        std::vector<std::uint64_t> keys;
        std::vector<std::size_t> offsets;
        numeric::IndexDomain domain;
        runtime::ExecutionRequirements requirements;
        std::shared_ptr<const Plan> input;
    };
public:
    static AlgebraicGradeTensor select(const AlgebraicSnapshot& source,core::Grade grade,const std::vector<std::uint64_t>& keys,
        runtime::WorkAvailability availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        admit(source,availability,lease);
        const auto& bundle=source.archive().bundle();const auto count=bundle.source().grade(grade).cell_count();
        if (keys.size()!=count) throw std::invalid_argument("grade tensor requires one explicit vector for every original cell");
        runtime::ExecutionRequirements required;required.records=machine::checked_size_add(2,machine::checked_size_mul(3,count));
        runtime::admit_execution(required,lease.policy());
        std::vector<std::shared_ptr<const local::AlgebraicModuleVector>> values;values.reserve(count);
        for (std::size_t i=0;i<count;++i) {
            lease.check_cancelled();auto record=source.record(keys[i]);const auto value=record.value_pin();
            const auto* vector=std::get_if<local::AlgebraicModuleVector>(value.get());
            if (!vector || !vector->carrier().same_owner(bundle.stalk({grade,static_cast<std::uint32_t>(i)})))
                throw std::invalid_argument("grade tensor vector must belong to its actual original cell stalk");
            for (const auto& coefficient:vector->coordinates()) required.integer_bits=std::max(required.integer_bits,algebraic_database_detail::bits(coefficient));
            values.emplace_back(value,vector);
        }
        runtime::admit_execution(required,lease.policy());auto offsets=bundle.offsets(grade);
        auto domain=numeric::IndexDomain::linear(offsets.back());
        lease.check_cancelled();return AlgebraicGradeTensor(std::make_shared<const Plan>(Plan{
            source,grade,std::move(values),keys,std::move(offsets),std::move(domain),required,{}}));
    }
    const AlgebraicSnapshot& source() const & {return plan().source;}
    const AlgebraicSnapshot& source() const &&=delete;
    core::Grade grade() const {return plan().grade;}
    std::size_t cell_count() const {return plan().values.size();}
    const numeric::IndexDomain& domain() const & {return plan().domain;}
    const numeric::IndexDomain& domain() const &&=delete;
    const std::vector<std::size_t>& stalk_offsets() const & {return plan().offsets;}
    const std::vector<std::size_t>& stalk_offsets() const &&=delete;
    const std::vector<std::uint64_t>& record_keys() const & {return plan().keys;}
    const std::vector<std::uint64_t>& record_keys() const &&=delete;
    runtime::ExecutionRequirements requirements() const {return plan().requirements;}
    std::shared_ptr<const local::AlgebraicModuleVector> stalk_pin(core::CellId cell) const {
        if (cell.grade!=grade()) throw std::invalid_argument("tensor stalk grade differs");
        return plan().values.at(cell.index);
    }
    AlgebraicStalkCoordinate source_coordinate(std::size_t position) const {
        const auto& p=plan();if (position>=p.domain.logical_size()) throw std::out_of_range("grade tensor coordinate");
        const auto found=std::upper_bound(p.offsets.begin(),p.offsets.end(),position);
        const auto cell=static_cast<std::size_t>(found-p.offsets.begin()-1);
        return {{p.grade,static_cast<std::uint32_t>(cell)},position-p.offsets[cell]};
    }
    std::size_t source_index(AlgebraicStalkCoordinate coordinate) const {
        const auto value=stalk_pin(coordinate.cell);
        if (coordinate.coordinate>=value->carrier().rank()) throw std::out_of_range("tensor local stalk coordinate");
        return machine::checked_size_add(plan().offsets.at(coordinate.cell.index),coordinate.coordinate);
    }
    numeric::AlgebraicElement coordinate(std::size_t position) const {
        const auto selected=source_coordinate(position);return stalk_pin(selected.cell)->coordinates().at(selected.coordinate);
    }
    bool derived() const {return static_cast<bool>(plan().input);}
    std::optional<AlgebraicGradeTensor> input() const {
        if (!plan().input) return std::nullopt;
        return AlgebraicGradeTensor(plan().input);
    }
    bool same_owner(const AlgebraicGradeTensor& other) const noexcept {return plan_ && plan_==other.plan_;}

    // The original prepared occurrence action already contains e/sign/q once.
    // The result remains on the actual lower stalks and retains its full input.
    AlgebraicGradeTensor boundary(runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        admit(source(),availability,lease);runtime::admit_execution(requirements(),lease.policy());
        if (!grade().value) throw std::out_of_range("grade-zero tensor has no lower boundary");
        const auto& declared=source().archive().contents().fibers;
        if (!declared) throw std::invalid_argument("grade tensor boundary requires the complete declared fiber system");
        const auto action=declared->prepared_action(grade());const auto& stats=action.statistics();
        const auto& bundle=source().archive().bundle();const auto lower=core::Grade{static_cast<std::uint16_t>(grade().value-1u)};
        const auto count=bundle.source().grade(lower).cell_count(),degree=bundle.field().degree();
        runtime::ExecutionRequirements required;
        required.records=machine::checked_size_add(2,machine::checked_size_add(
            machine::checked_size_mul(3,count),stats.target_dimension));
        required.rational_slots=machine::checked_size_add(stats.source_dimension,
            machine::checked_size_add(machine::checked_size_mul(2,stats.target_dimension),machine::checked_size_mul(2,degree)));
        required.sparse_entries=stats.entries;
        required.scalar_work=machine::checked_size_add(machine::checked_size_mul(2,stats.entries),
            machine::checked_size_add(stats.source_dimension,stats.target_dimension));
        runtime::admit_execution(required,lease.policy());
        std::size_t input_bits=1,coefficient_bits=1;
        for (const auto& value:plan().values) for (const auto& coefficient:value->coordinates()) {
            lease.check_cancelled();input_bits=std::max(input_bits,algebraic_database_detail::bits(coefficient));
        }
        for (const auto& entry:action.entries()) {
            lease.check_cancelled();coefficient_bits=std::max(coefficient_bits,std::max(
                composite::declaration_detail::integer_bits(entry.coefficient.get_num()),
                composite::declaration_detail::integer_bits(entry.coefficient.get_den())));
        }
        const auto terms=std::max(std::size_t{1},stats.entries);std::size_t logarithm{};
        for (auto n=terms;n>1;n=n/2+n%2) ++logarithm;
        required.integer_bits=machine::checked_size_add(machine::checked_size_mul(machine::checked_size_add(terms,2),
            machine::checked_size_add(input_bits,coefficient_bits)),machine::checked_size_add(logarithm,2));
        runtime::admit_execution(required,lease.policy());
        if (required.integer_bits>bundle.field().limits().max_integer_bits) throw std::length_error("grade boundary exceeds declared field integer budget");
        numeric::RationalVector input;input.reserve(stats.source_dimension);
        for (const auto& value:plan().values) for (const auto& coefficient:value->coordinates()) {
            lease.check_cancelled();input.insert(input.end(),coefficient.coordinates().begin(),coefficient.coordinates().end());
        }
        std::vector<std::size_t> rows;rows.reserve(stats.target_dimension);
        for (std::size_t i=0;i<stats.target_dimension;++i) {lease.check_cancelled();rows.push_back(i);}
        const auto output=action.apply_selected(input,rows,std::min(lease.policy().limits.integer_bits,bundle.field().limits().max_integer_bits),
            [&]{lease.check_cancelled();});
        std::vector<local::AlgebraicModuleVector> values;values.reserve(count);std::size_t offset{};
        for (std::size_t i=0;i<count;++i) {
            lease.check_cancelled();const auto carrier=bundle.stalk({lower,static_cast<std::uint32_t>(i)});
            std::vector<numeric::AlgebraicElement> coordinates;coordinates.reserve(carrier.rank());
            for (std::size_t j=0;j<carrier.rank();++j) {
                lease.check_cancelled();numeric::RationalVector coefficients;coefficients.reserve(degree);
                for (std::size_t k=0;k<degree;++k) coefficients.push_back(output.at(offset++));
                coordinates.push_back(bundle.field().element(coefficients));
            }
            values.push_back(carrier.vector(std::move(coordinates)));
        }
        if (offset!=output.size()) throw std::logic_error("grade boundary scalar restriction extent differs");
        auto owner=std::make_shared<const std::vector<local::AlgebraicModuleVector>>(std::move(values));
        std::vector<std::shared_ptr<const local::AlgebraicModuleVector>> pins;pins.reserve(count);
        for (const auto& value:*owner) pins.emplace_back(owner,&value);
        auto offsets=bundle.offsets(lower);auto domain=numeric::IndexDomain::linear(offsets.back());
        lease.check_cancelled();return AlgebraicGradeTensor(std::make_shared<const Plan>(Plan{
            source(),lower,std::move(pins),{},std::move(offsets),std::move(domain),required,plan_}));
    }
private:
    static void admit(const AlgebraicSnapshot& source,runtime::WorkAvailability availability,const runtime::ExecutionLease& lease) {
        lease.check_cancelled();runtime::admit_work_availability(source.source_namespace(),source.sequence(),availability);
    }
    explicit AlgebraicGradeTensor(std::shared_ptr<const Plan> plan):plan_(std::move(plan)){}
    const Plan& plan() const {if (!plan_) throw std::invalid_argument("moved algebraic grade tensor");return *plan_;}
    std::shared_ptr<const Plan> plan_;
};
} // namespace rfc::database
