#pragma once

#include "rfc/local/prepared_field_action.hpp"
#include <atomic>

namespace rfc::local {
// Physical owner ranges over the existing exact row schedule. The runner must
// execute pure tiles synchronously and join all users before returning/throwing.
class PreparedGradeFieldRanges {
    struct State;
    struct Scratch;
public:
    struct Tile {
        std::size_t begin{}, end{}, entry_begin{}, entry_end{};
        bool operator==(const Tile&) const = default;
    };
    class Workspace {
    public:
        Workspace(const Workspace&) = delete;
        Workspace& operator=(const Workspace&) = delete;
        Workspace(Workspace&&) noexcept = default;
        Workspace& operator=(Workspace&&) noexcept = default;
        std::size_t scratch_slots() const {
            if (!scratch_) throw std::invalid_argument("moved-from field range workspace");
            return machine::checked_size_add(scratch_->values.size(),scratch_->products.size());
        }
        std::size_t scratch_object_bytes() const {
            if(!scratch_)throw std::invalid_argument("moved-from field range workspace");
            return machine::checked_size_add(numeric::rational_vector_object_bytes(scratch_->values),
                numeric::rational_vector_object_bytes(scratch_->products));
        }
    private:
        Workspace(std::shared_ptr<const State> owner, std::shared_ptr<Scratch> scratch)
            : owner_(std::move(owner)),scratch_(std::move(scratch)) {}
        std::shared_ptr<const State> owner_;
        std::shared_ptr<Scratch> scratch_;
        friend class PreparedGradeFieldRanges;
    };
    PreparedGradeFieldRanges(const PreparedGradeFieldRanges&) = default;
    PreparedGradeFieldRanges& operator=(const PreparedGradeFieldRanges&) = default;
    PreparedGradeFieldRanges(const PreparedGradeFieldAction& action, std::size_t rows_per_tile)
        : state_(std::make_shared<const State>(action,rows_per_tile)) {}
    static PreparedGradeFieldRanges partition(const PreparedGradeFieldAction& action,
        const std::vector<std::pair<std::size_t,std::size_t>>& rows) {
        return PreparedGradeFieldRanges(std::make_shared<const State>(action,rows));
    }
    const auto& action() const & noexcept { return state_->action; }
    const auto& tiles() const & noexcept { return state_->tiles; }
    const PreparedGradeFieldAction& action() const && = delete;
    const std::vector<Tile>& tiles() const && = delete;
    std::size_t rows_per_tile() const noexcept { return state_->rows_per_tile; }
    Workspace make_workspace() const {
        return {state_,std::make_shared<Scratch>(state_->action.statistics().target_dimension,state_->tiles.size())};
    }
    template<class Runner, class Checkpoint, class Completed>
    numeric::RationalVector apply(const numeric::RationalVector& input, Workspace& workspace,
        Runner&& runner, std::size_t integer_bit_limit, const Checkpoint& checkpoint, const Completed& completed) const {
        if (workspace.owner_ != state_ || !workspace.scratch_)
            throw std::invalid_argument("workspace belongs to another prepared field range owner");
        auto scratch = workspace.scratch_;
        if (scratch->busy.exchange(true,std::memory_order_acquire))
            throw std::logic_error("field range workspace already leased");
        struct Release {
            Scratch& scratch;
            ~Release() { scratch.busy.store(false,std::memory_order_release); }
        } release{*scratch};
        checkpoint(); state_->action.admit_input(input,integer_bit_limit);
        for (std::size_t i=0;i<state_->tiles.size();++i) scratch->finished[i].store(0,std::memory_order_relaxed);
        runner(state_->tiles.size(),[&](std::size_t index, const auto& stop) {
            const auto& tile = state_->tiles.at(index);
            unsigned char expected{};
            if (!scratch->finished[index].compare_exchange_strong(expected,1))
                throw std::logic_error("field range tile executed twice");
            const auto check = [&] { checkpoint(); stop.check(); };
            state_->action.apply_rows(input,scratch->values,scratch->products[index],tile.begin,tile.end,integer_bit_limit,check);
            scratch->finished[index].store(2,std::memory_order_release);
        });
        checkpoint();
        for (std::size_t i=0;i<state_->tiles.size();++i)
            if (scratch->finished[i].load(std::memory_order_acquire)!=2)
                throw std::logic_error("field range runner did not complete every tile");
        completed(); checkpoint();
        return scratch->values;
    }
private:
    struct State {
        PreparedGradeFieldAction action;
        std::size_t rows_per_tile;
        std::vector<Tile> tiles;
        State(const PreparedGradeFieldAction& a,std::size_t rows) : action(a),rows_per_tile(rows) {
            if (!rows) throw std::invalid_argument("field tile row count must be positive");
            const auto count = action.statistics().target_dimension;
            for (std::size_t begin=0;begin<count;) {
                const auto end = begin + std::min(rows,count-begin);
                tiles.push_back({begin,end,action.target_offsets()[begin],action.target_offsets()[end]});
                begin = end;
            }
        }
        State(const PreparedGradeFieldAction& a,const std::vector<std::pair<std::size_t,std::size_t>>& rows)
            : action(a),rows_per_tile(1) {
            std::size_t cursor{};
            for(const auto& [begin,end]:rows) {
                if(begin!=cursor || end<=begin || end>action.statistics().target_dimension)
                    throw std::invalid_argument("field partition must cover ordered disjoint complete rows");
                rows_per_tile=std::max(rows_per_tile,end-begin);
                tiles.push_back({begin,end,action.target_offsets()[begin],action.target_offsets()[end]});cursor=end;
            }
            if(cursor!=action.statistics().target_dimension)throw std::invalid_argument("field partition omitted output rows");
        }
    };
    explicit PreparedGradeFieldRanges(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    struct Scratch {
        std::atomic_bool busy{};
        numeric::RationalVector values,products;
        std::unique_ptr<std::atomic_uchar[]> finished;
        Scratch(std::size_t dimension,std::size_t tiles)
            : values(dimension),products(tiles),finished(std::make_unique<std::atomic_uchar[]>(tiles)) {}
    };
    std::shared_ptr<const State> state_;
};
} // namespace rfc::local
