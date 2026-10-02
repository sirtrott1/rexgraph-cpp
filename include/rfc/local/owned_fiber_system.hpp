#pragma once

#include "rfc/local/sheaf.hpp"
#include <memory>

namespace rfc::local {
// Freeze the complete source and maps at a stable address. Copying this handle
// keeps that owner; equal dimensions or values never identify different owners.
class OwnedLinearFiberSystem {
    struct State {
        core::RelationalComplex complex;
        LinearRelationalFiberSystem fibers;
        explicit State(const LinearRelationalFiberSystem& source)
            :complex(source.complex()),fibers(make_bundle(complex,source)) {
            for(std::size_t k=1;k<complex.grade_count();++k) {
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
                for(std::size_t slot=0;slot<complex.grade(grade).incidence_count();++slot)
                    if(const auto& map=source.transport_override(grade,slot))fibers.set_transport(grade,slot,*map);
            }
        }
        static LinearStalkBundle make_bundle(const core::RelationalComplex& complex,const LinearRelationalFiberSystem& source) {
            LinearStalkBundle bundle(complex);
            for(std::size_t k=0;k<complex.grade_count();++k) {
                const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
                for(std::size_t i=0;i<complex.grade(grade).cell_count();++i) {
                    const core::CellId cell{grade,static_cast<std::uint32_t>(i)};bundle.set_dimension(cell,source.stalks().dimension(cell));
                }
            }
            return bundle;
        }
    };
public:
    OwnedLinearFiberSystem(const OwnedLinearFiberSystem&)=default;
    OwnedLinearFiberSystem& operator=(const OwnedLinearFiberSystem&)=default;
    explicit OwnedLinearFiberSystem(const LinearRelationalFiberSystem& source):state_(std::make_shared<const State>(source)){}
    const LinearRelationalFiberSystem& fibers() const & { return state_->fibers; }
    const LinearRelationalFiberSystem& fibers() const &&=delete;
    bool same_owner(const OwnedLinearFiberSystem& other) const noexcept { return state_==other.state_; }
private:
    std::shared_ptr<const State> state_;
};
class OwnedLinearSection {
public:
    OwnedLinearSection(OwnedLinearFiberSystem owner,algebra::SubcomplexMask patch,LinearSection values)
        :owner_(std::move(owner)),patch_(std::move(patch)),values_(std::move(values)) {
        const RelationalLocalCarrier carrier(owner_.fibers().complex());carrier.require_closed_patch(patch_);
        std::size_t count{};
        for(std::size_t k=0;k<patch_.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            patch_.grade(grade).for_each_set([&](std::size_t i) {
                const core::CellId cell{grade,static_cast<std::uint32_t>(i)};++count;
                if(!values_.contains(cell) || values_.at(cell).size()!=owner_.fibers().stalks().dimension(cell))
                    throw std::invalid_argument("owned section must contain every patch stalk with declared dimension");
            });
        }
        if(count!=values_.size())throw std::invalid_argument("section contains values outside its patch");
        for(auto& [cell,stalk_values]:values_) {
            (void)cell;
            for(auto& value:stalk_values)value=numeric::canonical_rational(std::move(value));
        }
    }
    const LinearSection& values() const & { return values_; }
    const algebra::SubcomplexMask& patch() const & { return patch_; }
    const LinearSection& values() const &&=delete;
    const algebra::SubcomplexMask& patch() const &&=delete;
    OwnedLinearSection restrict_to(const algebra::SubcomplexMask& patch) const {
        if(meet(patch_,patch).grade_count()!=patch_.grade_count())throw std::invalid_argument("section patch shape mismatch");
        for(std::size_t k=0;k<patch.grade_count();++k) {
            const auto grade=core::Grade{static_cast<std::uint16_t>(k)};
            if((patch.grade(grade)&patch_.grade(grade)).count()!=patch.grade(grade).count())
                throw std::invalid_argument("section restriction exceeds its source patch");
        }
        return {owner_,patch,LinearRelationalSheaf(owner_.fibers()).restrict_to(values_,patch)};
    }
    OwnedLinearSection glue_compatible(const OwnedLinearSection& other) const {
        require_owner(other);return {owner_,join(patch_,other.patch_),LinearRelationalSheaf(owner_.fibers()).glue(values_,patch_,other.values_,other.patch_)};
    }
    // Arithmetic contribution assembly does not assert section compatibility.
    OwnedLinearSection sum_contributions(const OwnedLinearSection& other) const {
        require_owner(other);auto values=values_;
        for(const auto& [cell,value]:other.values_) {
            const auto [it,inserted]=values.emplace(cell,value);
            if(!inserted)for(std::size_t j=0;j<value.size();++j)it->second[j]+=value[j];
        }
        return {owner_,join(patch_,other.patch_),std::move(values)};
    }
private:
    void require_owner(const OwnedLinearSection& other) const {
        if(!owner_.same_owner(other.owner_))throw std::invalid_argument("section interfaces require the same declared fiber owner");
    }
    OwnedLinearFiberSystem owner_;
    algebra::SubcomplexMask patch_;
    LinearSection values_;
};
} // namespace rfc::local
