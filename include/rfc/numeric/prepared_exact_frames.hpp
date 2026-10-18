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
            // Reduction row coordinates are temporary basis coordinates. Keep
            // only represented source rows; the original carrier and both
            // certificate maps below retain their declared full dimensions.
            // This API explicitly returns every kernel/free-column axis, so
            // admit that index family before allocating it.
            guard.entries(original.cols());
            std::vector<std::map<std::size_t,Rational>> rows;
            std::map<std::size_t,std::set<std::size_t>> columns;
            std::optional<std::size_t> previous_row;
            std::size_t entries{};
            const auto admit=[&](std::size_t count,std::size_t row_count,std::size_t column_count){
                guard.entries(machine::checked_size_add(original.nonzeros(),machine::checked_size_add(
                    machine::checked_size_mul(count,2),machine::checked_size_add(row_count,column_count))));
            };
            for(const auto& [key,value]:original.entries()){
                guard.step();
                const bool new_row=!previous_row || *previous_row!=key.first;
                const bool new_column=!columns.contains(key.second);
                admit(machine::checked_size_add(entries,1),machine::checked_size_add(rows.size(),new_row?1u:0u),
                    machine::checked_size_add(columns.size(),new_column?1u:0u));
                if(new_row){rows.emplace_back();previous_row=key.first;}
                rows.back().emplace(key.second,value);
                columns[key.second].insert(rows.size()-1);++entries;
            }
            for(std::size_t j=0,k=0;j<original.cols() && k<rows.size();++j){
                guard.step();std::size_t pivot=rows.size(),best=std::numeric_limits<std::size_t>::max();
                const auto column_found=columns.find(j);if(column_found==columns.end())continue;
                auto& support=column_found->second;
                for(auto at=support.lower_bound(k);at!=support.end();++at){
                    guard.step();const auto i=*at;
                    if(rows[i].size()<best){pivot=i;best=rows[i].size();}
                }
                if(pivot==rows.size())continue;
                if(pivot!=k){
                    for(const auto& [column,value]:rows[k]){(void)value;guard.step();columns.at(column).erase(k);}
                    for(const auto& [column,value]:rows[pivot]){(void)value;guard.step();columns.at(column).erase(pivot);}
                    std::swap(rows[pivot],rows[k]);
                    for(const auto& [column,value]:rows[k]){(void)value;guard.step();columns.at(column).insert(k);}
                    for(const auto& [column,value]:rows[pivot]){(void)value;guard.step();columns.at(column).insert(pivot);}
                }
                const Rational scale=rows[k].at(j);
                for(auto& [column,value]:rows[k]){(void)column;value=guard.div(value,scale);}
                // Updating one row can erase only its own position from this
                // column's support. Advance before elimination, without a copy.
                for(auto at=support.begin();at!=support.end();){
                    const auto i=*at;++at;guard.step();if(i==k)continue;
                    const Rational ratio=rows[i].at(j);
                    for(const auto& [column,value]:rows[k]){
                        const auto found=rows[i].find(column);
                        const Rational old=found==rows[i].end()?Rational{0}:found->second;
                        auto updated=guard.sub(old,guard.mul(ratio,value));
                        if(updated==0){if(found!=rows[i].end()){
                            rows[i].erase(found);columns.at(column).erase(i);--entries;
                        }}
                        else{
                            if(found==rows[i].end()){
                                admit(machine::checked_size_add(entries,1),rows.size(),columns.size());
                                columns.at(column).insert(i);++entries;
                            }
                            rows[i][column]=std::move(updated);
                        }
                    }
                }
                pivots.push_back(j);++k;
            }
            columns.clear();
            reduced=ExactMatrix(original.rows(),original.cols());
            coordinates=ExactMatrix(pivots.size(),original.cols());
            image=ExactMatrix(original.rows(),pivots.size());
            for(std::size_t i=0;i<rows.size();++i)for(const auto& [j,value]:rows[i]){
                guard.step();reduced.set(i,j,value);if(i<pivots.size())coordinates.set(i,j,value);
                guard.entries(machine::checked_size_add(original.nonzeros(),machine::checked_size_add(entries,
                    machine::checked_size_add(reduced.nonzeros(),coordinates.nonzeros()))));
            }
            rows.clear();
            std::vector<std::size_t> pivot_axis(original.cols(),original.cols());
            for(std::size_t i=0;i<pivots.size();++i){guard.step();pivot_axis[pivots[i]]=i;}
            for(std::size_t j=0;j<original.cols();++j){guard.step();if(pivot_axis[j]==original.cols())free_columns.push_back(j);}
            for(const auto& [key,value]:original.entries()){
                guard.step();if(pivot_axis[key.second]!=original.cols())image.set(key.first,pivot_axis[key.second],value);
            }
            kernel=ExactMatrix(original.cols(),free_columns.size());
            for(auto& axis:pivot_axis){guard.step();axis=original.cols();}
            for(std::size_t i=0;i<free_columns.size();++i){
                guard.step();kernel.set(free_columns[i],i,1);pivot_axis[free_columns[i]]=i;
            }
            // Only represented free coefficients contribute to pivot rows.
            // Keep every free axis, including structurally zero columns.
            for(const auto& [key,value]:coordinates.entries()){
                guard.step();const auto free=pivot_axis[key.second];
                if(free!=original.cols())kernel.set(pivots[key.first],free,guard.sub(0,value));
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
