#pragma once
#include "rfc/local/owned_fiber_system.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <tuple>
#include "rfc/numeric/sparse_vector.hpp"
#include <variant>

namespace rfc::learning {
struct TransportCoordinate { std::size_t occurrence{}, row{}, column{}; };
struct TransportObservation {
    numeric::RationalVector input, target;
    std::vector<std::size_t> observed;
};
// A supplied TOTAL sparse input; omitted numerical entries mean exact zero.
// Only listed target coordinates are observed. An explicit zero is a constraint;
// an absent target is not. Coordinates belong to the model's original stalks.
struct SparseTransportTarget { std::size_t cell{}, axis{}; numeric::Rational value{0}; };
struct SparseTransportObservation {
    numeric::SparseRationalVector input;
    std::vector<SparseTransportTarget> targets;
};
enum class TransportObservationDomain : std::uint8_t { LowerStalk, OriginalOccurrenceImage };
struct TransportMappedObservation {
    numeric::RationalVector input, target;
    numeric::ExactMatrix projection;
    std::vector<std::size_t> coordinates;
};
using TransportFieldObservation = std::variant<TransportObservation, TransportMappedObservation>;
struct TransportConstraint {
    std::size_t sample{}, cell{}, axis{};
    TransportObservationDomain domain{TransportObservationDomain::LowerStalk};
};
struct TransportObservationSystem {
    numeric::ExactMatrix design, augmented;
    numeric::RationalVector target;
    std::vector<TransportConstraint> constraints;
};
struct TransportFixedResponse { numeric::RationalVector boundary, occurrences; };

// Observation equations on declared original stalk grades. Fixed transports
// are already realized. Variable coordinates use the source coefficient once.
class PreparedTransportObservations {
    using Q = numeric::Rational;
    using V = numeric::RationalVector;
    using M = numeric::ExactMatrix;
    using Guard = numeric::ExactActionGuard;
    local::OwnedLinearFiberSystem owner_;
    core::Grade grade_{1};
    std::vector<TransportCoordinate> fixed_support_, variables_;
    std::optional<M> basis_;
    std::vector<std::size_t> lower_, upper_, occurrences_;
    V coefficients_;
    std::size_t retained_{}, preparation_steps_{};

    void workspace(std::size_t entries, Guard& guard) const {
        guard.entries(machine::checked_size_add(retained_,entries));
    }
    void coordinate(const TransportCoordinate& s) const {
        const auto& a = owner_.fibers().complex().grade(grade_);
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
        std::optional<M> basis, Guard& guard, core::Grade selected = core::Grade{1}, bool graded = false)
        : owner_(std::move(owner)), fixed_support_(std::move(fixed_support)),
          variables_(std::move(variables)), basis_(std::move(basis)) {
        grade_=selected;
        const auto start = guard.steps();
        const auto& source = owner_.fibers().complex();
        if (!grade_.value || grade_.value >= source.grade_count())
            throw std::invalid_argument("positive original transport grade required");
        for (std::size_t k=2;!graded && k<source.grade_count();++k) {
            guard.step();
            if (source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                throw std::invalid_argument("transport observations require empty higher grades");
        }
        for (std::size_t k=0;k<2;++k) {
            const auto grade = core::Grade{static_cast<std::uint16_t>(grade_.value-1+k)};
            auto& offsets = k ? upper_ : lower_; offsets.push_back(0);
            guard.dimension(source.grade(grade).cell_count());
            for (std::size_t i=0;i<source.grade(grade).cell_count();++i) {
                guard.step(); offsets.push_back(machine::checked_size_add(offsets.back(),
                    owner_.fibers().stalks().dimension({grade,static_cast<std::uint32_t>(i)})));
                guard.dimension(offsets.back());
            }
        }
        const auto& a = source.grade(grade_);
        guard.dimension(machine::checked_size_add(variables_.size(),1));
        guard.dimension(machine::checked_size_add(parameter_count(),1));
        occurrences_.push_back(0);
        for (std::size_t p=0;p<a.incidence_count();++p) {
            guard.step(); const auto i = a.lower_index[p], j = a.upper_index[p];
            occurrences_.push_back(machine::checked_size_add(occurrences_.back(),lower_[i+1]-lower_[i]));
            guard.dimension(occurrences_.back());
            const auto& lo = source.grade(core::Grade{static_cast<std::uint16_t>(grade_.value-1)});
            guard.value(lo.cell_scales[i]); guard.value(a.cell_scales[j]); guard.value(a.lattice_coefficient[p]);
            const auto q = guard.div(guard.mul(lo.cell_scales[i],a.lattice_coefficient[p]),a.cell_scales[j]);
            coefficients_.push_back(a.aspects.get(source.signature().existence_aspect(),p) ? q : Q{0});
            const auto& map = owner_.fibers().transport_override(grade_,p);
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
            const auto& map = *owner_.fibers().transport_override(grade_,p);
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
    // Assemble observations at a declared original grade. Chain compatibility
    // is a separate constraint on the complete transport parameter family.
    static PreparedTransportObservations at_grade(local::OwnedLinearFiberSystem owner,core::Grade grade,
        std::vector<TransportCoordinate> fixed,std::vector<TransportCoordinate> variables,Guard& guard,
        std::optional<M> basis = std::nullopt) {
        return {std::move(owner),std::move(fixed),std::move(variables),std::move(basis),guard,grade,true};
    }
    core::Grade grade() const noexcept { return grade_; }
    void require_grade_one_profile() const {
        if(grade_.value!=1)throw std::invalid_argument("grade-one transport profile required");
        const auto& source=owner_.fibers().complex();
        for(std::size_t k=2;k<source.grade_count();++k)
            if(source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count())
                throw std::invalid_argument("grade-one transport profile requires empty higher grades");
    }
    PreparedTransportObservations(local::OwnedLinearFiberSystem owner,
        std::vector<TransportCoordinate> fixed_support, std::vector<TransportCoordinate> variables, Guard& guard)
        : PreparedTransportObservations(std::move(owner),std::move(fixed_support),std::move(variables),std::nullopt,guard) {}
    PreparedTransportObservations(local::OwnedLinearFiberSystem owner,
        std::vector<TransportCoordinate> fixed_support, std::vector<TransportCoordinate> variables, M basis, Guard& guard)
        : PreparedTransportObservations(std::move(owner),std::move(fixed_support),std::move(variables),std::optional<M>{std::move(basis)},guard) {}
    const std::vector<std::size_t>& input_stalk_offsets() const & {return upper_;}
    const std::vector<std::size_t>& input_stalk_offsets() const &&=delete;
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
        const auto& a=owner_.fibers().complex().grade(grade_);
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
        const auto& a=owner_.fibers().complex().grade(grade_);
        for(std::size_t p=0;p<a.incidence_count();++p) {
            guard.step(); const auto& map=*owner_.fibers().transport_override(grade_,p);
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
    // Selected-coordinate assembly shares the parameterization and fixed realized
    // maps of the dense API, but allocates no whole-stalk input/target/mask array.
    TransportObservationSystem assemble_sparse(
        std::span<const SparseTransportObservation> samples, Guard& guard) const {
        const auto add=machine::checked_size_add;
        const auto mul=machine::checked_size_mul;
        guard.dimension(samples.size());
        std::size_t rows{}, input_entries{};
        for (const auto& sample:samples) {
            guard.step();
            if (sample.input.size()!=upper_.back())
                throw std::invalid_argument("complete original sparse input carrier required");
            rows=add(rows,sample.targets.size());
            input_entries=add(input_entries,add(mul(sample.input.nonzeros(),2),mul(sample.targets.size(),3)));
            workspace(input_entries,guard);
            guard.sparse_vector(sample.input);
        }
        guard.dimension(rows);
        const auto n=parameter_count();
        TransportObservationSystem out{M(rows,n),M(rows,add(n,1)),{}, {}};
        workspace(add(input_entries,mul(rows,8)),guard);
        out.target.reserve(rows); out.constraints.reserve(rows);
        const auto& arena=owner_.fibers().complex().grade(grade_);
        std::size_t offset{};
        for (std::size_t sample_index=0;sample_index<samples.size();++sample_index) {
            const auto& sample=samples[sample_index];
            std::map<std::size_t,std::size_t> selected;
            V fixed(sample.targets.size(),Q{0});
            const auto admit=[&] {
                workspace(add(input_entries,add(mul(rows,8),add(out.design.nonzeros(),out.augmented.nonzeros()))),guard);
            };
            std::optional<std::pair<std::size_t,std::size_t>> previous;
            for (std::size_t i=0;i<sample.targets.size();++i) {
                guard.step(); const auto& t=sample.targets[i];
                if (t.cell>=lower_.size()-1 || t.axis>=lower_[t.cell+1]-lower_[t.cell])
                    throw std::invalid_argument("observed coordinate outside original lower stalk");
                const auto key=std::pair{t.cell,t.axis};
                if (previous && key<=*previous)
                    throw std::invalid_argument("sparse target coordinates must be ordered and distinct");
                previous=key;
                guard.value(t.value);
                selected.emplace(add(lower_[t.cell],t.axis),i);
                out.constraints.push_back({sample_index,t.cell,t.axis});
            }
            const auto value=[&](std::size_t index)->Q {
                // Charge the actual binary-search comparisons, independent of extent.
                const auto& entries=sample.input.entries(); std::size_t lo=0,hi=entries.size();
                while (lo<hi) {guard.step();const auto mid=lo+(hi-lo)/2;
                    if(entries[mid].index<index)lo=mid+1;else hi=mid;}
                return lo<entries.size() && entries[lo].index==index ? entries[lo].value : Q{0};
            };
            for (std::size_t p=0;p<arena.incidence_count();++p) {
                guard.step(); const auto& map=*owner_.fibers().transport_override(grade_,p);
                for (const auto& [key,q]:map.entries()) {
                    guard.step(); const auto at=selected.find(add(lower_[arena.lower_index[p]],key.first));
                    if (at==selected.end())continue;
                    const Q x=value(add(upper_[arena.upper_index[p]],key.second));
                    if (x!=0)fixed[at->second]=guard.add(fixed[at->second],guard.mul(q,x));
                }
            }
            const auto direction=[&](std::size_t slot,std::size_t parameter,const Q& weight) {
                guard.step(); const auto& c=variables_[slot];
                const auto at=selected.find(add(lower_[arena.lower_index[c.occurrence]],c.row));
                if(at==selected.end() || coefficients_[c.occurrence]==0)return;
                const Q x=value(add(upper_[arena.upper_index[c.occurrence]],c.column));
                if(x==0)return;
                Q q=guard.mul(coefficients_[c.occurrence],x);
                if(basis_)q=guard.mul(weight,q);
                const auto row=add(offset,at->second);
                // Admit potential new entries before inserting either copy.
                workspace(add(input_entries,add(mul(rows,8),add(2,add(out.design.nonzeros(),out.augmented.nonzeros())))),guard);
                q=guard.add(out.design.get(row,parameter),q);
                out.design.set(row,parameter,q);out.augmented.set(row,parameter,q);admit();
            };
            if(basis_)for(const auto& [key,q]:basis_->entries())direction(key.first,key.second,q);
            else for(std::size_t i=0;i<variables_.size();++i)direction(i,i,Q{1});
            for(std::size_t i=0;i<sample.targets.size();++i) {
                guard.step();const Q q=guard.sub(numeric::canonical_rational(sample.targets[i].value),fixed[i]);
                out.target.push_back(q);out.augmented.set(add(offset,i),n,q);admit();
            }
            offset=add(offset,sample.targets.size());
        }
        return out;
    }

    TransportObservationSystem assemble_mapped(const std::vector<TransportMappedObservation>& observations, Guard& guard) const {
        guard.dimension(observations.size()); std::size_t count{}, retained{};
        for (const auto& o : observations) {
            guard.step(); field(o.input, guard); guard.vector(o.target); guard.matrix(o.projection);
            if (o.target.empty() || o.projection.rows() != o.target.size() || o.projection.cols() != occurrences_.back() ||
                    o.coordinates.size() != o.target.size())
                throw std::invalid_argument("mapped observation requires original flag action, target and coordinate identities");
            std::set<std::size_t> unique;
            for (const auto coordinate : o.coordinates) {
                guard.step(); if (!unique.insert(coordinate).second) throw std::invalid_argument("mapped observation repeats a coordinate");
            }
            count = machine::checked_size_add(count, o.target.size()); guard.dimension(count);
            retained = machine::checked_size_add(retained, machine::checked_size_add(o.input.size(),
                machine::checked_size_add(o.target.size(), machine::checked_size_add(o.coordinates.size(), o.projection.nonzeros()))));
            workspace(retained, guard);
        }
        const auto n = parameter_count();
        TransportObservationSystem out{M(count,n),M(count,machine::checked_size_add(n,1)),{}, {}};
        out.target.reserve(count); out.constraints.reserve(count); std::size_t offset{};
        for (std::size_t sample=0; sample<observations.size(); ++sample) {
            const auto& o=observations[sample]; const auto h=observation_map(o.input,false,guard);
            const auto fixed=fixed_response(o.input,guard);
            const auto action=numeric::checked_multiply(o.projection,h,guard);
            const auto base=numeric::checked_apply(o.projection,fixed.occurrences,guard);
            const auto scratch=machine::checked_size_add(retained,machine::checked_size_add(h.nonzeros(),
                machine::checked_size_add(action.nonzeros(),machine::checked_size_add(base.size(),
                machine::checked_size_add(fixed.boundary.size(),fixed.occurrences.size())))));
            const auto admit=[&] { workspace(machine::checked_size_add(scratch,machine::checked_size_add(
                machine::checked_size_mul(count,5),machine::checked_size_add(out.design.nonzeros(),out.augmented.nonzeros()))),guard); };
            admit();
            for (std::size_t i=0;i<o.target.size();++i) {
                guard.step();const auto q=guard.sub(o.target[i],base[i]);out.target.push_back(q);out.augmented.set(offset+i,n,q);
                out.constraints.push_back({sample,o.coordinates[i],0,TransportObservationDomain::OriginalOccurrenceImage});admit();
            }
            for (const auto& [key,q]:action.entries()) {
                guard.step();out.design.set(offset+key.first,key.second,q);out.augmented.set(offset+key.first,key.second,q);admit();
            }
            offset=machine::checked_size_add(offset,o.target.size());
        }
        return out;
    }

};
} // namespace rfc::learning
