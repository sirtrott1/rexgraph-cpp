#pragma once
#include "rfc/database/native_role_join.hpp"

namespace rfc::database {
struct NativeParticipantLink {
    std::uint64_t channel{};
    storage::StableCellKey left, right;
    bool operator==(const NativeParticipantLink &) const = default;
};
struct NativeParticipantCorrespondence {
    std::uint64_t left_namespace{}, left_sequence{}, right_namespace{}, right_sequence{};
    std::vector<NativeParticipantLink> links;
};
struct NativeCrossRoleMatch {
    storage::StableCellKey left_participant, right_participant;
    std::uint64_t channel{}, left_assertion{}, right_assertion{};
    storage::StableOccurrenceKey left_occurrence, right_occurrence;
    std::size_t left_position{}, right_position{};
};
struct DerivedNativeCrossRoleJoin {
    InterpretedSnapshot left, right;
    NativeParticipantCorrespondence correspondence;
    std::vector<NativeCrossRoleMatch> matches;
    std::size_t unmapped_left_occurrences{};
};
// A correspondence links declared subjects at two named native cutoffs. Equal
// numeric keys, values or dimensions across sources never create a link. The
// composition retains every occurrence pair and every distinct link channel.
class PreparedNativeCrossRoleJoin {
    struct Binding {
        std::uint64_t assertion{};
        std::size_t position{};
        storage::PinnedNativeOccurrence occurrence;
    };
    struct Plan {
        InterpretedSnapshot left, right;
        NativeRoleJoinRequest roles;
        NativeParticipantCorrespondence correspondence;
        std::vector<Binding> left_bindings;
        std::map<storage::StableCellKey, std::vector<Binding>> right_index;
        std::map<storage::StableCellKey, std::vector<std::size_t>> link_index;
        runtime::ExecutionRequirements requirements;
    };

  public:
    static PreparedNativeCrossRoleJoin
    prepare(InterpretedSnapshot left, InterpretedSnapshot right, NativeRoleJoinRequest roles,
            NativeParticipantCorrespondence correspondence, runtime::WorkAvailability left_available,
            runtime::WorkAvailability right_available,
            const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) {
        using interpretation_detail::require;
        lease.check_cancelled();
        runtime::admit_work_availability(left.schema().source_namespace, left.sequence(), left_available);
        runtime::admit_work_availability(right.schema().source_namespace, right.sequence(), right_available);
        require(correspondence.left_namespace == left.schema().source_namespace &&
                    correspondence.left_sequence == left.sequence() &&
                    correspondence.right_namespace == right.schema().source_namespace &&
                    correspondence.right_sequence == right.sequence(),
                "participant correspondence does not bind both exact endpoints");
        require(roles.activity == NativeRoleActivity::Retained ||
                    roles.activity == NativeRoleActivity::ActiveOnly,
                "unknown cross-role activity");
        const auto role_kind = [](const InterpretedSnapshot &source, std::uint64_t kind, std::uint64_t role) {
            const auto a = std::find_if(source.schema().assertions.begin(), source.schema().assertions.end(),
                                        [&](const auto &x) { return x.key == kind; });
            interpretation_detail::require(a != source.schema().assertions.end(),
                                           "cross-role assertion kind absent");
            const auto r =
                std::find_if(a->roles.begin(), a->roles.end(), [&](const auto &x) { return x.key == role; });
            interpretation_detail::require(r != a->roles.end(), "cross-role declaration absent");
            return r->participant_kind;
        };
        const auto left_subject_kind = role_kind(left, roles.left_kind, roles.left_role),
                   right_subject_kind = role_kind(right, roles.right_kind, roles.right_role);
        auto p = std::make_shared<Plan>(
            Plan{std::move(left), std::move(right), roles, std::move(correspondence), {}, {}, {}, {}});
        interpretation_detail::bound(p->correspondence.links.size(),
                                     std::min(p->left.limits().max_records, p->right.limits().max_records),
                                     "participant link budget");
        const auto subjects = [](const InterpretedSnapshot &source, std::uint64_t kind) {
            std::set<storage::StableCellKey> keys;
            std::set<storage::StableCellKey> current;
            const auto &complex = source.endpoint()->source;
            const auto &grade = complex.grade(core::Grade{0});
            for (std::size_t i = 0; i < grade.cell_count(); ++i)
                current.insert(
                    storage::stable_cell_key(complex, {core::Grade{0}, static_cast<std::uint32_t>(i)}));
            for (const auto &binding : source.lineage().subjects)
                if (binding.origin.kind == kind && current.contains({core::Grade{0}, binding.native_key}))
                    keys.insert({core::Grade{0}, binding.native_key});
            return keys;
        };
        runtime::ExecutionRequirements preliminary;
        preliminary.records = p->correspondence.links.size();
        preliminary.scalar_work =
            machine::checked_size_add(p->left.lineage().subjects.size(), p->right.lineage().subjects.size());
        preliminary.scalar_work = machine::checked_size_add(preliminary.scalar_work, preliminary.records);
        interpretation_detail::bound(preliminary.scalar_work,
                                     std::min(p->left.limits().max_work, p->right.limits().max_work),
                                     "cross-role declaration work budget");
        runtime::admit_execution(preliminary, lease.policy());
        p->requirements = preliminary;
        const auto valid_left = subjects(p->left, left_subject_kind),
                   valid_right = subjects(p->right, right_subject_kind);
        std::set<std::uint64_t> channels;
        for (std::size_t i = 0; i < p->correspondence.links.size(); ++i) {
            const auto &link = p->correspondence.links[i];
            require(link.channel && channels.insert(link.channel).second && valid_left.contains(link.left) &&
                        valid_right.contains(link.right),
                    "invalid, stale, wrong-kind or repeated participant link identity");
            p->link_index[link.left].push_back(i);
        }
        const auto collect = [&](const InterpretedSnapshot &source, std::uint64_t kind, std::uint64_t role,
                                 bool lhs) {
            for (const auto assertion : source.assertions(kind)) {
                lease.check_cancelled();
                const auto fiber = source.participants(assertion, role);
                p->requirements.scalar_work =
                    machine::checked_size_add(p->requirements.scalar_work, fiber.size() + 1);
                interpretation_detail::bound(p->requirements.scalar_work,
                                             std::min(p->left.limits().max_work, p->right.limits().max_work),
                                             "cross-role binding work budget");
                runtime::admit_execution(p->requirements, lease.policy());
                for (std::size_t i = 0; i < fiber.size(); ++i) {
                    if (roles.activity == NativeRoleActivity::ActiveOnly && !fiber[i].aspect("exists"))
                        continue;
                    p->requirements.records = machine::checked_size_add(p->requirements.records, 1);
                    interpretation_detail::bound(
                        p->requirements.records,
                        std::min(p->left.limits().max_records, p->right.limits().max_records),
                        "cross-role binding budget");
                    runtime::admit_execution(p->requirements, lease.policy());
                    if (lhs)
                        p->left_bindings.push_back({assertion, i, fiber[i]});
                    else
                        p->right_index[fiber[i].participant()].push_back({assertion, i, fiber[i]});
                }
            }
        };
        collect(p->left, roles.left_kind, roles.left_role, true);
        collect(p->right, roles.right_kind, roles.right_role, false);
        runtime::admit_execution(p->requirements, lease.policy());
        return PreparedNativeCrossRoleJoin(std::move(p));
    }
    DerivedNativeCrossRoleJoin
    execute(runtime::WorkAvailability left_available, runtime::WorkAvailability right_available,
            const runtime::ExecutionLease &lease = runtime::ExecutionLease{}) const {
        lease.check_cancelled();
        const auto &p = *plan_;
        runtime::admit_work_availability(p.left.schema().source_namespace, p.left.sequence(), left_available);
        runtime::admit_work_availability(p.right.schema().source_namespace, p.right.sequence(),
                                         right_available);
        runtime::admit_execution(p.requirements, lease.policy());
        DerivedNativeCrossRoleJoin result{p.left, p.right, p.correspondence, {}, 0};
        std::size_t work = p.requirements.scalar_work;
        for (const auto &left : p.left_bindings) {
            lease.check_cancelled();
            const auto links = p.link_index.find(left.occurrence.participant());
            if (links == p.link_index.end()) {
                ++result.unmapped_left_occurrences;
                continue;
            }
            for (const auto i : links->second) {
                lease.check_cancelled();
                work = machine::checked_size_add(work, 1);
                interpretation_detail::bound(
                    work, std::min(p.left.limits().max_work, p.right.limits().max_work),
                    "cross-role correspondence work budget");
                auto lookup_required = p.requirements;
                lookup_required.records = machine::checked_size_add(lookup_required.records, result.matches.size());
                lookup_required.scalar_work = work;
                runtime::admit_execution(lookup_required, lease.policy());
                const auto &link = p.correspondence.links[i];
                const auto rhs = p.right_index.find(link.right);
                if (rhs == p.right_index.end())
                    continue;
                for (const auto &right : rhs->second) {
                    lease.check_cancelled();
                    work = machine::checked_size_add(work, 1);
                    interpretation_detail::bound(
                        work, std::min(p.left.limits().max_work, p.right.limits().max_work),
                        "cross-role work budget");
                    interpretation_detail::bound(
                        result.matches.size() + 1,
                        std::min(p.left.limits().max_records, p.right.limits().max_records),
                        "cross-role match budget");
                    auto required = p.requirements;
                    required.records = machine::checked_size_add(required.records, result.matches.size() + 1);
                    required.scalar_work = work;
                    runtime::admit_execution(required, lease.policy());
                    result.matches.push_back({link.left, link.right, link.channel, left.assertion,
                                              right.assertion, left.occurrence.key(), right.occurrence.key(),
                                              left.position, right.position});
                }
            }
        }
        return result;
    }

  private:
    explicit PreparedNativeCrossRoleJoin(std::shared_ptr<const Plan> plan) : plan_(std::move(plan)) {}
    std::shared_ptr<const Plan> plan_;
};
} // namespace rfc::database
