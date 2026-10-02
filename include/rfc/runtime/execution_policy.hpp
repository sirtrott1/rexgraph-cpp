#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace rfc::runtime {

enum class ExecutionBackend { ExactCpu, ExactGpu };
enum class BackendFallback { Reject, AllowExactCpuFallback };
enum class DecisionKind { Declared, Derived, Profile, Measured };
struct ExecutionDecision {
    DecisionKind kind;
    std::string choice, origin, version, value, constraints, revalidate;
};

// Counts apply to the named reference containers, not the process or GMP heap.
struct ExecutionRequirements {
    std::size_t records{}, rational_slots{}, sparse_entries{}, scalar_work{}, integer_bits{};
    bool operator==(const ExecutionRequirements&) const = default;
};
struct ExecutionHostPolicy {
    std::string name{"serial-reference-v1"};
    ExecutionBackend backend{ExecutionBackend::ExactCpu};
    BackendFallback fallback{BackendFallback::Reject};
    bool allow_allocation{true};
    ExecutionRequirements limits{4000000, 16000000, 16000000, 64000000, 1u << 24};
};
class ExecutionLimitExceeded : public std::length_error {
public:
    ExecutionLimitExceeded(std::string resource, std::size_t required, std::size_t limit)
        : std::length_error(resource + " requires " + std::to_string(required) +
                            ", limit " + std::to_string(limit)),
          resource_(std::move(resource)), required_(required), limit_(limit) {}
    const std::string& resource() const noexcept { return resource_; }
    std::size_t required() const noexcept { return required_; }
    std::size_t limit() const noexcept { return limit_; }
private:
    std::string resource_;
    std::size_t required_, limit_;
};
class ExecutionCancelled : public std::runtime_error {
public:
    ExecutionCancelled() : std::runtime_error("exact execution cancelled at a phase boundary") {}
};

inline ExecutionBackend resolve_reference_backend(const ExecutionHostPolicy& policy) {
    if (policy.name.empty()) throw std::invalid_argument("execution host policy needs a name");
    if (policy.fallback != BackendFallback::Reject && policy.fallback != BackendFallback::AllowExactCpuFallback)
        throw std::invalid_argument("unknown backend fallback policy");
    switch (policy.backend) {
    case ExecutionBackend::ExactCpu: return ExecutionBackend::ExactCpu;
    case ExecutionBackend::ExactGpu:
        if (policy.fallback == BackendFallback::AllowExactCpuFallback) return ExecutionBackend::ExactCpu;
        throw std::invalid_argument("no registered exact GPU field implementation");
    }
    throw std::invalid_argument("unknown execution backend");
}
inline void admit_execution(const ExecutionRequirements& required, const ExecutionHostPolicy& policy) {
    (void)resolve_reference_backend(policy);
    if (!policy.allow_allocation) throw std::invalid_argument("reference execution requires owning allocations");
    const auto check = [](std::size_t value, std::size_t limit, const char* name) {
        if (value > limit) throw ExecutionLimitExceeded(name, value, limit);
    };
    check(required.records, policy.limits.records, "binding records");
    check(required.rational_slots, policy.limits.rational_slots, "reference rational slots");
    check(required.sparse_entries, policy.limits.sparse_entries, "reference sparse entries");
    check(required.scalar_work, policy.limits.scalar_work, "reference scalar work envelope");
    check(required.integer_bits, policy.limits.integer_bits, "exact integer growth bound");
}

// No borrowed output/scratch buffers are accepted. Cancellation is shared
// explicitly and observed before/after the synchronous scalar reference call.
class ExecutionLease {
public:
    explicit ExecutionLease(const ExecutionHostPolicy& policy = {},
                            std::shared_ptr<const std::atomic_bool> cancellation = {})
        : policy_(policy), cancellation_(std::move(cancellation)) {}
    const ExecutionHostPolicy& policy() const & noexcept { return policy_; }
    const ExecutionHostPolicy& policy() const && = delete;
    void check_cancelled() const {
        if (cancellation_ && cancellation_->load(std::memory_order_relaxed)) throw ExecutionCancelled();
    }
private:
    ExecutionHostPolicy policy_;
    std::shared_ptr<const std::atomic_bool> cancellation_;
};

} // namespace rfc::runtime
