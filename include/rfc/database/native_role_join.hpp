#pragma once
#include "rfc/database/native_interpretation.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <bit>

namespace rfc::database {
enum class NativeRoleActivity { Retained, ActiveOnly };
struct NativeRoleJoinRequest {
    std::uint64_t left_kind{}, left_role{}, right_kind{}, right_role{};
    NativeRoleActivity activity{NativeRoleActivity::Retained};
};
struct NativeRoleMatch {
    storage::StableCellKey participant;
    std::uint64_t left_assertion{}, right_assertion{};
    storage::StableOccurrenceKey left_occurrence, right_occurrence;
    std::size_t left_position{}, right_position{};
    bool operator==(const NativeRoleMatch &) const = default;
};
class PreparedNativeRoleJoin;
class DerivedNativeRoleJoin {
  public:
    const std::vector<NativeRoleMatch> &matches() const & { return *matches_; }
    const std::vector<NativeRoleMatch> &matches() const && = delete;
    std::shared_ptr<const std::vector<NativeRoleMatch>> matches_pin() const { return matches_; }
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const { return source_.endpoint(); }
    runtime::WorkAvailability availability() const { return availability_; }
    NativeRoleJoinRequest request() const { return request_; }

  private:
    DerivedNativeRoleJoin(InterpretedSnapshot source, NativeRoleJoinRequest request,
                          runtime::WorkAvailability availability, std::vector<NativeRoleMatch> matches)
        : source_(std::move(source)), request_(request), availability_(availability),
          matches_(std::make_shared<const std::vector<NativeRoleMatch>>(std::move(matches))) {}
    InterpretedSnapshot source_;
    NativeRoleJoinRequest request_;
    runtime::WorkAvailability availability_;
    std::shared_ptr<const std::vector<NativeRoleMatch>> matches_;
    friend class PreparedNativeRoleJoin;
};
// A declared same-publication role composition. Every occurrence pair remains
// distinct; null/missing/empty fibers create no participant identities. Inactive
// occurrences are retained unless the request explicitly selects ActiveOnly.
class PreparedNativeRoleJoin {
    struct Binding {
        std::uint64_t assertion;
        std::size_t position;
        storage::PinnedNativeOccurrence occurrence;
    };
    struct Plan {
        InterpretedSnapshot source;
        NativeRoleJoinRequest request;
        std::vector<Binding> left, right;
        runtime::ExecutionRequirements requirements;
        std::size_t matches{}, missing{}, nulls{};
        Plan(InterpretedSnapshot s, NativeRoleJoinRequest r) : source(std::move(s)), request(r) {}
    };

  public:
    static PreparedNativeRoleJoin prepare(const InterpretedSnapshot &source, NativeRoleJoinRequest request,
                                          runtime::WorkAvailability availability,
                                          const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) {
        lease.check_cancelled();
        runtime::admit_work_availability(source.schema().source_namespace, source.sequence(), availability);
        if (request.activity != NativeRoleActivity::Retained &&
            request.activity != NativeRoleActivity::ActiveOnly)
            throw std::invalid_argument("unknown role activity policy");
        const auto role = [&](auto kind, auto key) -> const ExternalRole & {
            const auto &types = source.schema().assertions;
            const auto type =
                std::find_if(types.begin(), types.end(), [&](const auto &t) { return t.key == kind; });
            if (type == types.end())
                throw std::invalid_argument("join assertion kind absent");
            const auto entry = std::find_if(type->roles.begin(), type->roles.end(),
                                            [&](const auto &r) { return r.key == key; });
            if (entry == type->roles.end())
                throw std::invalid_argument("join role absent");
            return *entry;
        };
        if (role(request.left_kind, request.left_role).participant_kind !=
            role(request.right_kind, request.right_role).participant_kind)
            throw std::invalid_argument("join roles have different declared participant kinds");
        auto p = std::make_shared<Plan>(source, request);
        auto &required = p->requirements;
        const auto collect = [&](auto kind, auto key, std::vector<Binding> &out) {
            for (const auto assertion : source.assertions(kind)) {
                lease.check_cancelled();
                const auto cell = source.source().cell({core::Grade{1}, assertion});
                required.scalar_work = machine::checked_size_add(
                    required.scalar_work,
                    machine::checked_size_add(machine::checked_size_mul(cell.occurrence_count(), 4), 8));
                required.records = machine::checked_size_add(
                    required.records, machine::checked_size_mul(cell.occurrence_count(), 2));
                interpretation_detail::bound(required.scalar_work, source.limits().max_work,
                                             "role traversal work limit");
                interpretation_detail::bound(required.records, source.limits().max_records,
                                             "role traversal record limit");
                runtime::admit_execution(required, lease.policy());
                const auto state = source.reference_state(assertion, key);
                if (state == ReferenceState::Missing)
                    ++p->missing;
                if (state == ReferenceState::Null)
                    ++p->nulls;
                const auto fiber = source.participants(assertion, key);
                required.scalar_work = machine::checked_size_add(required.scalar_work, fiber.size() + 1);
                for (std::size_t i = 0; i < fiber.size(); ++i) {
                    lease.check_cancelled();
                    if (request.activity == NativeRoleActivity::ActiveOnly && !fiber[i].aspect("exists"))
                        continue;
                    required.records = machine::checked_size_add(required.records, 1);
                    interpretation_detail::bound(required.records, source.limits().max_records,
                                                 "role index record limit");
                    runtime::admit_execution(required, lease.policy());
                    out.push_back({assertion, i, fiber[i]});
                }
            }
        };
        collect(request.left_kind, request.left_role, p->left);
        collect(request.right_kind, request.right_role, p->right);
        required.scalar_work = machine::checked_size_add(
            required.scalar_work,
            machine::checked_size_mul(p->right.size(),
                                      static_cast<std::size_t>(std::bit_width(p->right.size())) + 1));
        interpretation_detail::bound(required.scalar_work, source.limits().max_work, "role index work limit");
        runtime::admit_execution(required, lease.policy());
        std::stable_sort(p->right.begin(), p->right.end(), [&](const auto &a, const auto &b) {
            lease.check_cancelled();
            return a.occurrence.participant() < b.occurrence.participant();
        });
        for (const auto &left : p->left) {
            lease.check_cancelled();
            const auto bounds = range(*p, left.occurrence.participant());
            p->matches =
                machine::checked_size_add(p->matches, static_cast<std::size_t>(bounds.second - bounds.first));
            interpretation_detail::bound(p->matches, source.limits().max_records, "role join output limit");
        }
        required.records = machine::checked_size_add(required.records, p->matches);
        required.scalar_work = machine::checked_size_add(
            required.scalar_work,
            machine::checked_size_add(
                p->matches,
                machine::checked_size_mul(
                    p->left.size(), 2 * static_cast<std::size_t>(std::bit_width(p->right.size())) + 1)));
        interpretation_detail::bound(required.scalar_work, source.limits().max_work, "role join work limit");
        runtime::admit_execution(required, lease.policy());
        return PreparedNativeRoleJoin(std::move(p));
    }
    DerivedNativeRoleJoin execute(runtime::WorkAvailability availability,
                                  const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) const {
        const auto &p = *plan_;
        lease.check_cancelled();
        runtime::admit_work_availability(p.source.schema().source_namespace, p.source.sequence(),
                                         availability);
        runtime::admit_execution(p.requirements, lease.policy());
        std::vector<NativeRoleMatch> matches;
        matches.reserve(p.matches);
        for (const auto &left : p.left) {
            lease.check_cancelled();
            const auto [first, last] = range(p, left.occurrence.participant());
            for (auto right = first; right != last; ++right) {
                lease.check_cancelled();
                matches.push_back({left.occurrence.participant(), left.assertion, right->assertion,
                                   left.occurrence.key(), right->occurrence.key(), left.position,
                                   right->position});
            }
        }
        return DerivedNativeRoleJoin(p.source, p.request, availability, std::move(matches));
    }
    runtime::ExecutionRequirements requirements() const { return plan_->requirements; }
    std::size_t match_count() const { return plan_->matches; }
    std::size_t missing_fibers() const { return plan_->missing; }
    std::size_t null_fibers() const { return plan_->nulls; }

  private:
    using Iterator = std::vector<Binding>::const_iterator;
    static std::pair<Iterator, Iterator> range(const Plan &p, storage::StableCellKey participant) {
        const auto first =
            std::lower_bound(p.right.begin(), p.right.end(), participant,
                             [](const auto &a, auto b) { return a.occurrence.participant() < b; });
        const auto last = std::upper_bound(first, p.right.end(), participant, [](auto a, const auto &b) {
            return a < b.occurrence.participant();
        });
        return std::make_pair(first, last);
    }
    explicit PreparedNativeRoleJoin(std::shared_ptr<const Plan> p) : plan_(std::move(p)) {}
    std::shared_ptr<const Plan> plan_;
};
} // namespace rfc::database
