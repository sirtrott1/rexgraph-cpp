#pragma once
#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <tuple>

namespace rfc::learning {
struct TransportCoordinate { std::size_t occurrence{}, row{}, column{}; };
struct TransportObservation {
    numeric::RationalVector input, target;
    std::vector<std::size_t> observed;
};
struct TransportConstraint { std::size_t sample{}, cell{}, axis{}; };
struct TransportObservationSystem {
    numeric::ExactMatrix design, augmented;
    numeric::RationalVector target;
    std::vector<TransportConstraint> constraints;
};
struct TransportFixedResponse { numeric::RationalVector boundary, occurrences; };

// Observation equations on the original grade-one stalks. Fixed transports
// are already realized. Variable coordinates use the source coefficient once.
class PreparedTransportObservations {
    using Q = numeric::Rational;
    using V = numeric::RationalVector;
    using M = numeric::ExactMatrix;
    using Guard = numeric::ExactActionGuard;
    local::OwnedLinearFiberSystem owner_;
    std::vector<TransportCoordinate> fixed_support_, variables_;
    std::optional<M> basis_;
    std::vector<std::size_t> lower_, upper_, occurrences_;
    V coefficients_;
    std::size_t retained_{}, preparation_steps_{};

    void workspace(std::size_t entries, Guard& guard) const {
        guard.entries(machine::checked_size_add(retained_,entries));
    }
    void coordinate(const TransportCoordinate& s) const {
        const auto& a = owner_.fibers().complex().grade(core::Grade{1});
        if (s.occurrence >= a.incidence_count() ||
            s.row >= lower_[a.lower_index[s.occurrence]+1]-lower_[a.lower_index[s.occurrence]] ||
            s.column >= upper_[a.upper_index[s.occurrence]+1]-upper_[a.upper_index[s.occurrence]])
            throw std::invalid_argument("transport coordinate outside its original stalks");
    }
    void field(const V& input, Guard& guard) const {
        if (input.size() != upper_.back())
            throw std::invalid_argument("complete original input stalk field required");
        guard.vector(input);
    }
    PreparedTransportObservations(local::OwnedLinearFiberSystem owner,
        std::vector<TransportCoordinate> fixed_support, std::vector<TransportCoordinate> variables,
        std::optional<M> basis, Guard& guard)
        : owner_(std::move(owner)), fixed_support_(std::move(fixed_support)),
          variables_(std::move(variables)), basis_(std::move(basis)) {
        const auto start = guard.steps();
        const auto& source = owner_.fibers().complex();
        if (source.grade_count() < 2) throw std::invalid_argument("grade-one transport source required");
        for (std::size_t k=2;k<source.grade_count();++k) {
            guard.step();
            if (source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                throw std::invalid_argument("transport observations require empty higher grades");
        }
        for (std::size_t k=0;k<2;++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
            auto& offsets = k ? upper_ : lower_; offsets.push_back(0);
            guard.dimension(source.grade(grade).cell_count());
            for (std::size_t i=0;i<source.grade(grade).cell_count();++i) {
                guard.step(); offsets.push_back(machine::checked_size_add(offsets.back(),
                    owner_.fibers().stalks().dimension({grade,static_cast<std::uint32_t>(i)})));
                guard.dimension(offsets.back());
            }
        }
        const auto& a = source.grade(core::Grade{1});
        guard.dimension(machine::checked_size_add(variables_.size(),1));
        guard.dimension(machine::checked_size_add(parameter_count(),1));
        occurrences_.push_back(0);
        for (std::size_t p=0;p<a.incidence_count();++p) {
            guard.step(); const auto i = a.lower_index[p], j = a.upper_index[p];
            occurrences_.push_back(machine::checked_size_add(occurrences_.back(),lower_[i+1]-lower_[i]));
            guard.dimension(occurrences_.back());
            const auto& lo = source.grade(core::Grade{0});
            guard.value(lo.cell_scales[i]); guard.value(a.cell_scales[j]); guard.value(a.lattice_coefficient[p]);
            const auto q = guard.div(guard.mul(lo.cell_scales[i],a.lattice_coefficient[p]),a.cell_scales[j]);
            coefficients_.push_back(a.aspects.get(source.signature().existence_aspect(),p) ? q : Q{0});
            const auto& map = owner_.fibers().transport_override(core::Grade{1},p);
            if (!map) throw std::invalid_argument("explicit realized map required for every original occurrence");
            guard.matrix(*map);
            if (coefficients_.back()==0 && map->nonzeros())
                throw std::invalid_argument("inactive or zero incidence has a nonzero realized fixed map");
        }
        retained_ = machine::checked_size_add(lower_.size(),machine::checked_size_add(upper_.size(),
            machine::checked_size_add(occurrences_.size(),machine::checked_size_add(coefficients_.size(),
                machine::checked_size_mul(machine::checked_size_add(fixed_support_.size(),variables_.size()),3)))));
        std::set<std::tuple<std::size_t,std::size_t,std::size_t>> fixed, declared;
        for (const auto& s : fixed_support_) {
            guard.step(); coordinate(s);
            if (!fixed.emplace(s.occurrence,s.row,s.column).second)
                throw std::invalid_argument("distinct fixed support coordinates required");
        }
        declared = fixed;
        for (const auto& s : variables_) {
            guard.step(); coordinate(s);
            if (!declared.emplace(s.occurrence,s.row,s.column).second)
                throw std::invalid_argument("distinct variable coordinates disjoint from fixed support required");
        }
        for (std::size_t p=0;p<a.incidence_count();++p) {
            const auto& map = *owner_.fibers().transport_override(core::Grade{1},p);
            retained_ = machine::checked_size_add(retained_,map.nonzeros());
            for (const auto& [key,q] : map.entries()) {
                (void)q; guard.step();
                if (!fixed.contains({p,key.first,key.second}))
                    throw std::invalid_argument("realized fixed map lies outside declared fixed support");
            }
        }
        if (basis_) {
            if (basis_->rows()!=variables_.size()) throw std::invalid_argument("parameter basis must cover every variable coordinate");
            guard.matrix(*basis_); retained_=machine::checked_size_add(retained_,basis_->nonzeros());
        }
        workspace(machine::checked_size_mul(declared.size(),3),guard);
        preparation_steps_=guard.steps()-start;
    }
public:
    PreparedTransportObservations(local::OwnedLinearFiberSystem owner,
        std::vector<TransportCoordinate> fixed_support, std::vector<TransportCoordinate> variables, Guard& guard)
        : PreparedTransportObservations(std::move(owner),std::move(fixed_support),std::move(variables),std::nullopt,guard) {}
    PreparedTransportObservations(local::OwnedLinearFiberSystem owner,
        std::vector<TransportCoordinate> fixed_support, std::vector<TransportCoordinate> variables, M basis, Guard& guard)
        : PreparedTransportObservations(std::move(owner),std::move(fixed_support),std::move(variables),std::optional<M>{std::move(basis)},guard) {}
    std::size_t parameter_count() const noexcept { return basis_ ? basis_->cols() : variables_.size(); }
    std::size_t retained_entries() const noexcept { return retained_; }
    std::size_t preparation_steps() const noexcept { return preparation_steps_; }
    const local::OwnedLinearFiberSystem& owner() const & { return owner_; }
    const local::OwnedLinearFiberSystem& owner() const && = delete;
    const std::vector<TransportCoordinate>& variable_coordinates() const & { return variables_; }
    const std::vector<TransportCoordinate>& variable_coordinates() const && = delete;
    const std::vector<TransportCoordinate>& fixed_coordinates() const & { return fixed_support_; }
    const std::vector<TransportCoordinate>& fixed_coordinates() const && = delete;
    const std::optional<M>& parameter_basis() const & { return basis_; }
    const std::optional<M>& parameter_basis() const && = delete;

    M observation_map(const V& input, bool shared, Guard& guard) const {
        field(input,guard); M map(shared ? lower_.back() : occurrences_.back(),parameter_count());
        const auto& a=owner_.fibers().complex().grade(core::Grade{1});
        const auto add=[&](std::size_t slot,std::size_t parameter,const Q& weight) {
            guard.step(); const auto& s=variables_[slot];
            const auto row=(shared ? lower_[a.lower_index[s.occurrence]] : occurrences_[s.occurrence])+s.row;
            const auto& value=input[upper_[a.upper_index[s.occurrence]]+s.column];
            if(coefficients_[s.occurrence]==0 || value==0) return;
            const auto product=guard.mul(coefficients_[s.occurrence],value);
            const auto q=basis_ ? guard.mul(weight,product) : product;
            map.set(row,parameter,guard.add(map.get(row,parameter),q)); workspace(map.nonzeros(),guard);
        };
        if (basis_) for(const auto& [key,q]:basis_->entries()) add(key.first,key.second,q);
        else for(std::size_t j=0;j<variables_.size();++j) add(j,j,Q{1});
        return map;
    }
    TransportFixedResponse fixed_response(const V& input, Guard& guard) const {
        field(input,guard); workspace(machine::checked_size_add(lower_.back(),occurrences_.back()),guard);
        TransportFixedResponse out{V(lower_.back()),V(occurrences_.back())};
        const auto& a=owner_.fibers().complex().grade(core::Grade{1});
        for(std::size_t p=0;p<a.incidence_count();++p) {
            guard.step(); const auto& map=*owner_.fibers().transport_override(core::Grade{1},p);
            for(const auto& [key,q]:map.entries()) {
                const auto value=guard.mul(q,input[upper_[a.upper_index[p]]+key.second]);
                auto& local=out.occurrences[occurrences_[p]+key.first]; local=guard.add(local,value);
                auto& global=out.boundary[lower_[a.lower_index[p]]+key.first]; global=guard.add(global,value);
            }
        }
        return out;
    }
    TransportObservationSystem assemble(const std::vector<TransportObservation>& observations, Guard& guard) const {
        guard.dimension(observations.size()); std::size_t rows{}, input_entries{};
        for(const auto& o:observations) {
            guard.step();
            if(o.input.size()!=upper_.back() || o.target.size()!=lower_.back() || o.observed.size()+1!=lower_.size() ||
                std::any_of(o.observed.begin(),o.observed.end(),[](auto n){return n>1;}) ||
                std::none_of(o.observed.begin(),o.observed.end(),[](auto n){return n==1;}))
                throw std::invalid_argument("whole fields and nonempty original lower-stalk mask required");
            input_entries=machine::checked_size_add(input_entries,machine::checked_size_add(o.input.size(),
                machine::checked_size_add(o.target.size(),o.observed.size()))); workspace(input_entries,guard);
            for(std::size_t i=0;i<o.observed.size();++i) if(o.observed[i]) rows=machine::checked_size_add(rows,lower_[i+1]-lower_[i]);
        }
        guard.dimension(rows); const auto n=parameter_count();
        TransportObservationSystem out{M(rows,n),M(rows,machine::checked_size_add(n,1)),{}, {}};
        out.target.reserve(rows); out.constraints.reserve(rows); std::size_t row{};
        for(std::size_t sample=0;sample<observations.size();++sample) {
            const auto& o=observations[sample]; guard.vector(o.target);
            const auto fixed=fixed_response(o.input,guard);
            const auto action=observation_map(o.input,true,guard);
            const auto temporary=machine::checked_size_add(input_entries,machine::checked_size_add(
                machine::checked_size_mul(rows,4),machine::checked_size_add(action.nonzeros(),machine::checked_size_add(
                    lower_.back(),machine::checked_size_add(fixed.boundary.size(),fixed.occurrences.size())))));
            const auto admit=[&]{workspace(machine::checked_size_add(temporary,
                machine::checked_size_add(out.design.nonzeros(),out.augmented.nonzeros())),guard);};
            admit();
            std::vector<std::size_t> selected(lower_.back(),rows);
            for(std::size_t cell=0;cell<o.observed.size();++cell) for(auto i=lower_[cell];i<lower_[cell+1];++i) {
                guard.step();
                if(!o.observed[cell]) {if(o.target[i]!=0) throw std::invalid_argument("unobserved stalk has nonzero target data"); continue;}
                selected[i]=row; const auto q=guard.sub(o.target[i],fixed.boundary[i]);
                out.target.push_back(q); out.augmented.set(row,n,q); admit();
                out.constraints.push_back({sample,cell,i-lower_[cell]}); ++row;
            }
            for(const auto& [key,q]:action.entries()) if(selected[key.first]!=rows) {
                guard.step(); out.design.set(selected[key.first],key.second,q); out.augmented.set(selected[key.first],key.second,q);
                admit();
            }
        }
        return out;
    }
};
} // namespace rfc::learning
