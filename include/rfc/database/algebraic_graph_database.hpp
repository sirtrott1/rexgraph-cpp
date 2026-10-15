#pragma once
#include "rfc/database/algebraic_snapshot.hpp"

namespace rfc::database {
// Archive selections share the actual graph revision and stable database pins.
// Source associations remain primary maps, not scalar or stalk comparisons.
class AlgebraicGraphDatabase {
    struct NamedArchive {std::uint64_t key;AlgebraicSnapshot snapshot;};
    struct State {
        storage::AlgebraicGraphRevision revision;
        std::vector<NamedArchive> archives;
        runtime::ExecutionRequirements requirements;
    };
public:
    static AlgebraicGraphDatabase select(const storage::AlgebraicGraphRevision& revision,runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(revision.source_namespace(),revision.sequence(),availability);
        const auto& bindings=revision.snapshot().archive_bindings();
        runtime::ExecutionRequirements required;required.records=machine::checked_size_add(1,machine::checked_size_mul(3,bindings.size()));
        runtime::admit_execution(required,lease.policy());
        std::vector<NamedArchive> archives;archives.reserve(bindings.size());
        for (const auto& row:bindings) {
            lease.check_cancelled();archives.push_back({row.archive,AlgebraicSnapshot::select_graph_archive(revision,row.archive,availability,lease)});
        }
        lease.check_cancelled();return AlgebraicGraphDatabase(std::make_shared<const State>(State{revision,std::move(archives),required}));
    }
    const storage::AlgebraicGraphRevision& revision() const & {return state().revision;}
    const storage::AlgebraicGraphRevision& revision() const &&=delete;
    std::size_t archive_count() const {return state().archives.size();}
    runtime::ExecutionRequirements requirements() const {return state().requirements;}
    AlgebraicSnapshot archive(std::uint64_t key,runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        lease.check_cancelled();runtime::admit_work_availability(revision().source_namespace(),revision().sequence(),availability);
        runtime::admit_execution(requirements(),lease.policy());const auto& archives=state().archives;
        const auto found=std::lower_bound(archives.begin(),archives.end(),key,[](const auto& row,auto value){return row.key<value;});
        if (found==archives.end() || found->key!=key) throw std::out_of_range("selected graph database archive absent");
        return found->snapshot;
    }
    bool same_owner(const AlgebraicGraphDatabase& other) const noexcept {return state_ && state_==other.state_;}
private:
    explicit AlgebraicGraphDatabase(std::shared_ptr<const State> state):state_(std::move(state)){}
    const State& state() const {if (!state_) throw std::invalid_argument("moved graph database");return *state_;}
    std::shared_ptr<const State> state_;
};
} // namespace rfc::database
