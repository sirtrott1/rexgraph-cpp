#pragma once
#include "rfc/numeric/checked_exact_actions.hpp"

namespace rfc::numeric {

// One exact reduction supplies both complete frames and A = image * coordinates.
// Basis axes are derived coordinates; they are not native carrier identities.
class PreparedExactFrames {
    struct State {
        ExactMatrix original, reduced, image, coordinates, kernel;
        std::vector<std::size_t> pivots, free_columns;
        std::size_t retained{}, preparation{};
        State(ExactMatrix a, ExactActionGuard& guard):original(std::move(a)) {
            const auto start=guard.steps();guard.step();guard.matrix(original);
            std::vector<std::map<std::size_t,Rational>> rows(original.rows());
            std::size_t entries=original.nonzeros();
            for(const auto& [key,value]:original.entries()){guard.step();rows[key.first].emplace(key.second,value);}
            guard.entries(machine::checked_size_add(original.nonzeros(),entries));
            for(std::size_t j=0,k=0;j<original.cols() && k<rows.size();++j){
                guard.step();std::size_t pivot=rows.size(),best=std::numeric_limits<std::size_t>::max();
                for(std::size_t i=k;i<rows.size();++i){
                    guard.step();if(rows[i].contains(j) && rows[i].size()<best){pivot=i;best=rows[i].size();}
                }
                if(pivot==rows.size())continue;
                if(pivot!=k)std::swap(rows[pivot],rows[k]);
                const Rational scale=rows[k].at(j);
                for(auto& [column,value]:rows[k]){(void)column;value=guard.div(value,scale);}
                for(std::size_t i=0;i<rows.size();++i){
                    guard.step();if(i==k || !rows[i].contains(j))continue;
                    const Rational ratio=rows[i].at(j);
                    for(const auto& [column,value]:rows[k]){
                        const auto found=rows[i].find(column);
                        const Rational old=found==rows[i].end()?Rational{0}:found->second;
                        auto updated=guard.sub(old,guard.mul(ratio,value));
                        if(updated==0){if(found!=rows[i].end()){rows[i].erase(found);--entries;}}
                        else{
                            if(found==rows[i].end())++entries;
                            guard.entries(machine::checked_size_add(original.nonzeros(),entries));
                            rows[i][column]=std::move(updated);
                        }
                    }
                }
                pivots.push_back(j);++k;
            }
            reduced=ExactMatrix(original.rows(),original.cols());
            coordinates=ExactMatrix(pivots.size(),original.cols());
            image=ExactMatrix(original.rows(),pivots.size());
            for(std::size_t i=0;i<rows.size();++i)for(const auto& [j,value]:rows[i]){
                guard.step();reduced.set(i,j,value);if(i<pivots.size())coordinates.set(i,j,value);
            }
            std::vector<std::size_t> pivot_axis(original.cols(),original.cols());
            for(std::size_t i=0;i<pivots.size();++i){guard.step();pivot_axis[pivots[i]]=i;}
            for(std::size_t j=0;j<original.cols();++j){guard.step();if(pivot_axis[j]==original.cols())free_columns.push_back(j);}
            for(const auto& [key,value]:original.entries()){
                guard.step();if(pivot_axis[key.second]!=original.cols())image.set(key.first,pivot_axis[key.second],value);
            }
            kernel=ExactMatrix(original.cols(),free_columns.size());
            for(std::size_t i=0;i<free_columns.size();++i){
                guard.step();kernel.set(free_columns[i],i,1);
                for(std::size_t j=0;j<pivots.size();++j){guard.step();kernel.set(pivots[j],i,guard.sub(0,reduced.get(j,free_columns[i])));}
            }
            for(const auto* map:{&original,&reduced,&image,&coordinates,&kernel}){
                guard.matrix(*map);retained=machine::checked_size_add(retained,map->nonzeros());guard.entries(retained);
            }
            guard.entries(machine::checked_size_add(pivots.size(),free_columns.size()));
            if(checked_multiply(image,coordinates,guard)!=original || checked_multiply(original,kernel,guard).nonzeros())
                throw std::logic_error("complete exact frame certificate failed");
            preparation=guard.steps()-start;
        }
    };
public:
    PreparedExactFrames(const PreparedExactFrames&)=default;
    PreparedExactFrames& operator=(const PreparedExactFrames&)=default;
    PreparedExactFrames(ExactMatrix a,ExactActionGuard& guard):state_(std::make_shared<const State>(std::move(a),guard)){}
    explicit PreparedExactFrames(ExactMatrix a,ExactActionLimits limits={},std::function<void()> check={}) {
        ExactActionGuard guard(limits,std::move(check));state_=std::make_shared<const State>(std::move(a),guard);
    }
    const ExactMatrix& original() const & {return state_->original;}
    const ExactMatrix& reduced() const & {return state_->reduced;}
    const ExactMatrix& image() const & {return state_->image;}
    const ExactMatrix& coordinates() const & {return state_->coordinates;}
    const ExactMatrix& kernel() const & {return state_->kernel;}
    const std::vector<std::size_t>& pivots() const & {return state_->pivots;}
    const std::vector<std::size_t>& free_columns() const & {return state_->free_columns;}
    const ExactMatrix& original() const &&=delete;
    const ExactMatrix& reduced() const &&=delete;
    const ExactMatrix& image() const &&=delete;
    const ExactMatrix& coordinates() const &&=delete;
    const ExactMatrix& kernel() const &&=delete;
    const std::vector<std::size_t>& pivots() const &&=delete;
    const std::vector<std::size_t>& free_columns() const &&=delete;
    std::size_t rank() const noexcept {return state_->pivots.size();}
    std::size_t stored_entries() const noexcept {return state_->retained;}
    std::size_t stored_index_entries() const noexcept {return state_->original.cols();}
    std::size_t preparation_steps() const noexcept {return state_->preparation;}
    void validate(ExactActionGuard& guard) const {
        const auto pin=state_;guard.step();guard.entries(pin->retained);guard.entries(pin->original.cols());
        for(const auto* map:{&pin->original,&pin->reduced,&pin->image,&pin->coordinates,&pin->kernel})guard.matrix(*map);
        for(const auto* indices:{&pin->pivots,&pin->free_columns})for(const auto i:*indices){
            guard.step();if(i>=pin->original.cols())throw std::logic_error("retained frame coordinate");
        }
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::numeric
