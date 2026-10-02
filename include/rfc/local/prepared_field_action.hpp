#pragma once

#include "rfc/local/fiber_system.hpp"
#include "rfc/machine/checked_size.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rfc::local {

// A final-field action retains one block per occurrence. Entries are never
// coalesced across occurrences; the target schedule changes physical traversal
// only. All coefficients are already realized T, including source signing.
class PreparedGradeFieldRanges;
class PreparedGradeFieldAction {
    struct State;

  public:
    struct Block {
        std::size_t incidence_slot{}, source_begin{}, target_begin{}, rows{}, columns{}, entry_begin{},
            entry_end{};
        bool active{};
    };
    struct Entry {
        std::size_t source{}, target{}, block{};
        numeric::Rational coefficient;
    };
    struct LogicalEntry {
        std::size_t incidence_slot{}, row{}, column{};
        bool operator==(const LogicalEntry &) const = default;
    };
    struct Statistics {
        std::size_t retained_occurrences{}, active_occurrences{}, entries{}, map_materializations{},
            coefficient_copies{};
        std::size_t source_dimension{}, target_dimension{}, scratch_slots{};
        std::size_t scalar_identity_blocks{}, general_map_entries{};
        bool factored{};
    };
    class Workspace {
      public:
        Workspace(const Workspace &) = delete;
        Workspace &operator=(const Workspace &) = delete;
        Workspace(Workspace &&) noexcept = default;
        Workspace &operator=(Workspace &&) noexcept = default;
        std::size_t scratch_slots() const noexcept {
            return accumulator_.size() + 1u;
        }

      private:
        explicit Workspace(std::shared_ptr<const State> owner, std::size_t dimension)
            : owner_(std::move(owner)), accumulator_(dimension) {}
        std::shared_ptr<const State> owner_;
        numeric::RationalVector accumulator_;
        numeric::Rational product_;
        friend class PreparedGradeFieldAction;
    };

    PreparedGradeFieldAction(const PreparedGradeFieldAction &) = default;
    PreparedGradeFieldAction &operator=(const PreparedGradeFieldAction &) = default;
    explicit PreparedGradeFieldAction(const LinearRelationalFiberSystem &fibers, core::Grade grade)
        : state_(compile(fibers, grade)) {}
    static PreparedGradeFieldAction factored(const LinearRelationalFiberSystem& fibers,core::Grade grade) {
        return PreparedGradeFieldAction(compile_factored(fibers,grade));
    }
    const Statistics &statistics() const & noexcept {
        return state_->statistics;
    }
    const auto &blocks() const & noexcept {
        return state_->blocks;
    }
    const auto &entries() const & {
        ensure_canonical();
        return state_->entries;
    }
    const auto &target_offsets() const & noexcept {
        return state_->target_offsets;
    }
    const auto &target_order() const & {
        ensure_canonical();
        return state_->target_order;
    }
    const Statistics &statistics() const && = delete;
    const std::vector<Block> &blocks() const && = delete;
    const std::vector<Entry> &entries() const && = delete;
    const std::vector<std::size_t> &target_offsets() const && = delete;
    const std::vector<std::size_t> &target_order() const && = delete;

    LogicalEntry logical_entry(std::size_t physical_entry) const {
        ensure_canonical();
        const auto &entry = state_->entries.at(physical_entry);
        const auto &block = state_->blocks.at(entry.block);
        return {block.incidence_slot, entry.target - block.target_begin, entry.source - block.source_begin};
    }
    std::size_t physical_entry(LogicalEntry logical) const {
        ensure_canonical();
        const auto &block = state_->blocks.at(logical.incidence_slot);
        if (logical.row >= block.rows || logical.column >= block.columns)
            throw std::out_of_range("prepared map coordinate");
        const auto target = machine::checked_size_add(block.target_begin, logical.row);
        const auto source = machine::checked_size_add(block.source_begin, logical.column);
        for (std::size_t i = block.entry_begin; i < block.entry_end; ++i)
            if (state_->entries[i].target == target && state_->entries[i].source == source)
                return i;
        throw std::out_of_range("coordinate has no stored arithmetic entry");
    }
    Workspace make_workspace() const {
        return Workspace(state_, state_->statistics.target_dimension);
    }

    // Selected rows name a different output contract. Input and the complete
    // occurrence owner remain intact; only the requested accumulators are made.
    template<class Checkpoint>
    numeric::RationalVector apply_selected(const numeric::RationalVector& input,
        const std::vector<std::size_t>& rows,std::size_t limit,const Checkpoint& checkpoint) const {
        checkpoint();admit_input(input,limit);
        for(std::size_t i=0;i<rows.size();++i)
            if(rows[i]>=state_->statistics.target_dimension || (i && rows[i]<=rows[i-1]))
                throw std::invalid_argument("selected field rows must be ordered, distinct and in range");
        numeric::RationalVector out(rows.size());numeric::Rational product;
        for(std::size_t i=0;i<rows.size();) {
            auto end=i+1;while(end<rows.size() && rows[end]==rows[end-1]+1)++end;
            apply_rows(input,out,product,rows[i],rows[end-1]+1,limit,checkpoint,rows[i]-i);i=end;
        }
        checkpoint();return out;
    }

    // Scratch is private and operation-bound. No input/output span can alias it.
    // The returned vector owns its values; failures leave input and prior results
    // untouched. Scratch may contain an unpublished partial sum after failure.
    numeric::RationalVector
    apply(const numeric::RationalVector &input, Workspace &workspace,
          std::size_t integer_bit_limit = std::numeric_limits<std::size_t>::max()) const {
        if (workspace.owner_.get() != state_.get())
            throw std::invalid_argument("workspace belongs to a different prepared field action");
        admit_input(input, integer_bit_limit);
        apply_rows(input, workspace.accumulator_, workspace.product_, 0,
                   workspace.accumulator_.size(), integer_bit_limit, [] {});
        return workspace.accumulator_;
    }

  private:
    friend class PreparedGradeFieldRanges;
    void admit_input(const numeric::RationalVector &input, std::size_t integer_bit_limit) const {
        if (input.size() != state_->statistics.source_dimension)
            throw std::invalid_argument("prepared field input dimension");
        std::size_t input_bits = 1;
        for (const auto &q : input)
            input_bits = std::max(input_bits, bits(q));
        const auto terms = std::max(std::size_t{1}, state_->statistics.entries);
        std::size_t logarithm = 0;
        for (auto n = terms; n > 1; n = n / 2 + n % 2)
            ++logarithm;
        const auto bound = machine::checked_size_add(
            machine::checked_size_mul(machine::checked_size_add(terms, 2),
                                      machine::checked_size_add(input_bits, state_->coefficient_bits)),
            machine::checked_size_add(logarithm, 2));
        if (bound > integer_bit_limit)
            throw std::length_error("prepared exact integer growth bound exceeds limit");
    }
    template<class Checkpoint>
    void apply_rows(const numeric::RationalVector &input, numeric::RationalVector &accumulator,
                    numeric::Rational &product, std::size_t begin, std::size_t end,
                    std::size_t integer_bit_limit, const Checkpoint &checkpoint,std::size_t output_base=0) const {
        if (state_->statistics.factored) {
            apply_factored_rows(input,accumulator,product,begin,end,integer_bit_limit,checkpoint,output_base); return;
        }
        for (std::size_t row = begin; row < end; ++row) {
            checkpoint();
            auto *sum = accumulator[row-output_base].get_mpq_t();
            mpq_set_ui(sum, 0, 1);
            for (std::size_t i = state_->target_offsets[row]; i < state_->target_offsets[row + 1]; ++i) {
                checkpoint();
                const auto &entry = state_->entries[state_->target_order[i]];
                mpq_mul(product.get_mpq_t(), entry.coefficient.get_mpq_t(),
                        input[entry.source].get_mpq_t());
                mpq_add(sum, sum, product.get_mpq_t());
            }
            if (bits(accumulator[row-output_base]) > integer_bit_limit)
                throw std::length_error("prepared result integer bits exceed limit");
        }
    }
    struct State {
        Statistics statistics;
        std::vector<Block> blocks;
        mutable std::vector<Entry> entries;
        std::vector<std::size_t> target_offsets;
        mutable std::vector<std::size_t> target_order;
        struct Factor { numeric::Rational coefficient; bool scalar{}; std::vector<Entry> sparse; };
        std::vector<Factor> factors;
        std::vector<std::size_t> cell_offsets,cell_begin,cell_order,row_owner;
        mutable std::once_flag canonical_once;
        std::size_t coefficient_bits{1};
    };
    explicit PreparedGradeFieldAction(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    void ensure_canonical() const {
        if (!state_->statistics.factored) return;
        std::call_once(state_->canonical_once,[&] {
            std::vector<Entry> entries; entries.reserve(state_->statistics.entries);
            for (std::size_t slot=0;slot<state_->blocks.size();++slot) {
                const auto& block=state_->blocks[slot];const auto& factor=state_->factors[slot];
                if (!block.active) continue;
                if (factor.scalar) {
                    if (factor.coefficient!=0) for(std::size_t j=0;j<block.rows;++j)
                        entries.push_back({block.source_begin+j,block.target_begin+j,slot,factor.coefficient});
                } else entries.insert(entries.end(),factor.sparse.begin(),factor.sparse.end());
            }
            std::vector<std::size_t> order(entries.size());auto cursor=state_->target_offsets;
            for(std::size_t i=0;i<entries.size();++i)order[cursor[entries[i].target]++]=i;
            state_->entries.swap(entries);state_->target_order.swap(order);
        });
    }
    template<class Checkpoint>
    void apply_factored_rows(const numeric::RationalVector& input,numeric::RationalVector& accumulator,
        numeric::Rational& product,std::size_t begin,std::size_t end,std::size_t limit,const Checkpoint& checkpoint,
        std::size_t output_base=0) const {
        for(std::size_t row=begin;row<end;++row) { checkpoint();mpq_set_ui(accumulator[row-output_base].get_mpq_t(),0,1); }
        if(begin==end)return;
        for(std::size_t cell=state_->row_owner[begin];cell<=state_->row_owner[end-1];++cell) {
            const auto first=std::max(begin,state_->cell_offsets[cell]);
            const auto last=std::min(end,state_->cell_offsets[cell+1]);
            for(std::size_t i=state_->cell_begin[cell];i<state_->cell_begin[cell+1];++i) {
                const auto slot=state_->cell_order[i];const auto& block=state_->blocks[slot];
                const auto& factor=state_->factors[slot];checkpoint();
                if(factor.scalar) {
                    if(factor.coefficient==0)continue;
                    for(std::size_t row=first;row<last;++row) {
                        checkpoint();mpq_mul(product.get_mpq_t(),factor.coefficient.get_mpq_t(),
                            input[block.source_begin+row-block.target_begin].get_mpq_t());
                        mpq_add(accumulator[row-output_base].get_mpq_t(),accumulator[row-output_base].get_mpq_t(),product.get_mpq_t());
                    }
                } else {
                    const auto entry_begin=std::lower_bound(factor.sparse.begin(),factor.sparse.end(),first,
                        [](const auto& entry,std::size_t row){return entry.target<row;});
                    for(auto entry=entry_begin;entry!=factor.sparse.end() && entry->target<last;++entry) {
                        checkpoint();mpq_mul(product.get_mpq_t(),entry->coefficient.get_mpq_t(),input[entry->source].get_mpq_t());
                        auto* sum=accumulator[entry->target-output_base].get_mpq_t();mpq_add(sum,sum,product.get_mpq_t());
                    }
                }
            }
        }
        for(std::size_t row=begin;row<end;++row) {
            checkpoint();if(bits(accumulator[row-output_base])>limit)throw std::length_error("prepared result integer bits exceed limit");
        }
    }
    static std::size_t bits(const numeric::Rational &q) {
        const auto n = [](mpz_srcptr z) { return mpz_sgn(z) == 0 ? std::size_t{0} : mpz_sizeinbase(z, 2); };
        return std::max(n(mpq_numref(q.get_mpq_t())), n(mpq_denref(q.get_mpq_t())));
    }
    static std::shared_ptr<const State> compile_factored(const LinearRelationalFiberSystem& fibers,core::Grade grade) {
        if(grade.value==0 || grade.value>=fibers.complex().grade_count())
            throw std::out_of_range("prepared field requires a positive retained grade");
        auto state=std::make_shared<State>();auto& stats=state->statistics;stats.factored=true;
        const auto source=fibers.stalks().offsets(grade);
        state->cell_offsets=fibers.stalks().offsets(core::Grade{static_cast<std::uint16_t>(grade.value-1u)});
        stats.source_dimension=source.back();stats.target_dimension=state->cell_offsets.back();
        stats.scratch_slots=machine::checked_size_add(stats.target_dimension,1);
        state->target_offsets.assign(machine::checked_size_add(stats.target_dimension,1),0);
        state->cell_begin.assign(state->cell_offsets.size(),0);
        state->row_owner.resize(stats.target_dimension);
        for(std::size_t cell=0;cell+1<state->cell_offsets.size();++cell)
            for(std::size_t row=state->cell_offsets[cell];row<state->cell_offsets[cell+1];++row)state->row_owner[row]=cell;
        const auto& arena=fibers.complex().grade(grade);
        state->blocks.reserve(arena.incidence_count());state->factors.reserve(arena.incidence_count());
        for(std::size_t slot=0;slot<arena.incidence_count();++slot) {
            const auto& override=fibers.transport_override(grade,slot); // Includes inactive transport validation.
            const auto upper=arena.upper_index[slot],lower=arena.lower_index[slot];
            const bool active=arena.aspects.get(fibers.complex().signature().existence_aspect(),slot);
            Block block{slot,source.at(upper),state->cell_offsets.at(lower),
                state->cell_offsets.at(lower+1)-state->cell_offsets.at(lower),source.at(upper+1)-source.at(upper),
                stats.entries,stats.entries,active};
            typename State::Factor factor;
            if(active) {
                ++stats.active_occurrences;++state->cell_begin.at(lower+1);
                if(!override) {
                    // The existing fiber's named/default identity transport contract.
                    // Native preparation admits this only through declared constant coordinates.
                    factor.scalar=true;factor.coefficient=fibers.complex().incidence_coefficient(grade,slot);
                    ++stats.scalar_identity_blocks;++stats.coefficient_copies;
                    state->coefficient_bits=std::max(state->coefficient_bits,bits(factor.coefficient));
                    if(factor.coefficient!=0) {
                        stats.entries=machine::checked_size_add(stats.entries,block.rows);
                        for(std::size_t row=block.target_begin;row<block.target_begin+block.rows;++row)
                            ++state->target_offsets.at(row+1);
                    }
                } else {
                    ++stats.map_materializations;
                    factor.sparse.reserve(override->nonzeros());
                    for(const auto& [coordinate,value]:override->entries()) {
                        const auto target=machine::checked_size_add(block.target_begin,coordinate.first);
                        factor.sparse.push_back({machine::checked_size_add(block.source_begin,coordinate.second),target,slot,value});
                        ++state->target_offsets.at(target+1);
                        state->coefficient_bits=std::max(state->coefficient_bits,bits(value));
                    }
                    stats.entries=machine::checked_size_add(stats.entries,factor.sparse.size());
                    stats.general_map_entries=machine::checked_size_add(stats.general_map_entries,factor.sparse.size());
                    stats.coefficient_copies=machine::checked_size_add(stats.coefficient_copies,factor.sparse.size());
                }
                block.entry_end=stats.entries;
            }
            state->blocks.push_back(block);state->factors.push_back(std::move(factor));
        }
        stats.retained_occurrences=state->blocks.size();
        for(std::size_t i=1;i<state->target_offsets.size();++i)
            state->target_offsets[i]=machine::checked_size_add(state->target_offsets[i],state->target_offsets[i-1]);
        for(std::size_t i=1;i<state->cell_begin.size();++i)
            state->cell_begin[i]=machine::checked_size_add(state->cell_begin[i],state->cell_begin[i-1]);
        state->cell_order.resize(stats.active_occurrences);auto cursor=state->cell_begin;
        for(std::size_t slot=0;slot<state->blocks.size();++slot)if(state->blocks[slot].active)
            state->cell_order[cursor[arena.lower_index[slot]]++]=slot;
        return state;
    }
    static std::shared_ptr<const State> compile(const LinearRelationalFiberSystem &fibers,
                                                core::Grade grade) {
        if (grade.value == 0 || grade.value >= fibers.complex().grade_count())
            throw std::out_of_range("prepared field requires a positive retained grade");
        auto state = std::make_shared<State>();
        const auto source_offsets = fibers.stalks().offsets(grade);
        const auto target_offsets =
            fibers.stalks().offsets(core::Grade{static_cast<std::uint16_t>(grade.value - 1u)});
        auto &stats = state->statistics;
        stats.source_dimension = source_offsets.back();
        stats.target_dimension = target_offsets.back();
        stats.scratch_slots = machine::checked_size_add(stats.target_dimension, 1);
        const auto &arena = fibers.complex().grade(grade);
        state->blocks.reserve(arena.incidence_count());
        for (std::size_t slot = 0; slot < arena.incidence_count(); ++slot) {
            fibers.validate_transport(grade, slot); // Also validate inactive occurrences.
            const std::size_t upper = arena.upper_index[slot], lower = arena.lower_index[slot];
            const bool active = arena.aspects.get(fibers.complex().signature().existence_aspect(), slot);
            Block block{slot,
                        source_offsets.at(upper),
                        target_offsets.at(lower),
                        target_offsets.at(lower + 1u) - target_offsets.at(lower),
                        source_offsets.at(upper + 1u) - source_offsets.at(upper),
                        state->entries.size(),
                        state->entries.size(),
                        active};
            if (active) {
                ++stats.active_occurrences;
                const auto flag = fibers.flag_map(grade, slot);
                ++stats.map_materializations;
                for (const auto &[coordinate, value] : flag.transport.entries()) {
                    state->entries.push_back(
                        {machine::checked_size_add(block.source_begin, coordinate.second),
                         machine::checked_size_add(block.target_begin, coordinate.first), slot, value});
                    state->coefficient_bits = std::max(state->coefficient_bits, bits(value));
                }
                block.entry_end = state->entries.size();
            }
            state->blocks.push_back(block);
        }
        stats.retained_occurrences = state->blocks.size();
        stats.entries = state->entries.size();
        stats.coefficient_copies = stats.entries;
        state->target_offsets.assign(machine::checked_size_add(stats.target_dimension, 1), 0);
        for (const auto &entry : state->entries)
            ++state->target_offsets.at(entry.target + 1u);
        for (std::size_t i = 1; i < state->target_offsets.size(); ++i)
            state->target_offsets[i] =
                machine::checked_size_add(state->target_offsets[i], state->target_offsets[i - 1]);
        state->target_order.resize(stats.entries);
        auto cursor = state->target_offsets;
        for (std::size_t i = 0; i < stats.entries; ++i)
            state->target_order[cursor[state->entries[i].target]++] = i;
        return state;
    }
    std::shared_ptr<const State> state_;
};

} // namespace rfc::local
