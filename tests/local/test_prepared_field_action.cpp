#include "rfc/core/builder.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include "rfc/local/tensor_field.hpp"
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
bool fail_allocation = false;
std::size_t checks = 0;
[[gnu::noinline]] void release(void *p) noexcept {
    std::free(p);
}
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F &&f, const char *message) {
    ++checks;
    try {
        f();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace
// This executable injects failure at the owning vector copy after arithmetic.
// GMP has its own allocator; this hook does not simulate recoverable GMP OOM.
void *operator new(std::size_t size) {
    if (fail_allocation) {
        fail_allocation = false;
        throw std::bad_alloc();
    }
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *p) noexcept {
    release(p);
}
void operator delete[](void *p) noexcept {
    release(p);
}
void operator delete(void *p, std::size_t) noexcept {
    release(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    release(p);
}

int main() {
    try {
        using namespace rfc;
        using Q = numeric::Rational;
        std::uint64_t seed = 0x504f32;
        const auto next = [&]() {
            seed = seed * 6364136223846793005ULL + 1;
            return seed >> 24;
        };
        for (std::size_t trial = 0; trial < 96; ++trial) {
            core::RelationSignature signature;
            const auto type = signature.add_type("declared", core::BoundaryPolicy::DeclaredExact, 0, 100);
            core::RelationalComplexBuilder builder(signature);
            const auto relations = 1 + next() % 4;
            std::uint64_t occurrence = 1000;
            for (std::size_t upper = 0; upper < relations; ++upper) {
                std::vector<core::BoundaryOccurrenceInput> legs;
                const auto count = next() % 16;
                for (std::size_t i = 0; i < count; ++i)
                    legs.push_back({10 + next() % 5,
                                    next() % 4 != 0,
                                    next() % 2 != 0,
                                    Q{1, 3},
                                    {},
                                    core::OccurrenceKey{occurrence++}});
                builder.add_grade_one(core::RelationKey{100 + upper}, type, legs);
            }
            const auto source = builder.freeze();
            local::LinearStalkBundle stalks(source, 1);
            for (std::size_t k = 0; k < 2; ++k)
                for (std::size_t i = 0;
                     i < source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(); ++i)
                    stalks.set_dimension(
                        {core::Grade{static_cast<std::uint16_t>(k)}, static_cast<std::uint32_t>(i)},
                        1 + next() % 4);
            local::LinearRelationalFiberSystem fibers(std::move(stalks));
            const auto &arena = source.grade(core::Grade{1});
            for (std::size_t i = 0; i < arena.incidence_count(); ++i) {
                numeric::ExactMatrix map(fibers.stalks().dimension({core::Grade{0}, arena.lower_index[i]}),
                                         fibers.stalks().dimension({core::Grade{1}, arena.upper_index[i]}));
                for (std::size_t row = 0; row < map.rows(); ++row)
                    for (std::size_t col = 0; col < map.cols(); ++col)
                        if (next() % 3 != 0)
                            map.set(row, col, Q{static_cast<long>(next() % 21) - 10, 1 + next() % 19});
                fibers.set_transport(core::Grade{1}, i, std::move(map));
            }
            local::GradeTensorField reference(fibers, core::Grade{1});
            local::PreparedGradeFieldAction action(fibers, core::Grade{1});
            auto workspace = action.make_workspace();
            numeric::RationalVector input(reference.source_dimension());
            for (auto &q : input) {
                q = Q{static_cast<long>(next() % 21) - 10, 1 + next() % 19};
                q.canonicalize();
            }
            const auto prior = reference.apply(input);
            check(action.apply(input, workspace) == prior,
                  "random rectangular action differs from reference");
            check(action.apply(input, workspace) == reference.matrix().apply(input),
                  "prepared action differs from explicit operator");
            for (std::size_t i = 0; i < action.entries().size(); ++i)
                check(action.physical_entry(action.logical_entry(i)) == i,
                      "random physical layout lost its logical coordinate");
            auto other = local::PreparedGradeFieldAction(fibers, core::Grade{1});
            rejects([&] { other.apply(input, workspace); }, "foreign local workspace accepted");
            rejects([&] { action.apply(input, workspace, 1); }, "local growth cap bypassed");
            auto wrong = input;
            wrong.emplace_back(0);
            rejects([&] { action.apply(wrong, workspace); }, "wrong input domain accepted");
            if (!prior.empty()) {
                const auto before = input;
                fail_allocation = true;
                bool failed = false;
                try {
                    const auto unpublished = action.apply(input, workspace);
                    (void)unpublished;
                } catch (const std::bad_alloc &) {
                    failed = true;
                }
                fail_allocation = false;
                check(failed && input == before,
                      "owning result allocation failure modified input or returned output");
                check(action.apply(input, workspace) == prior, "allocation failure damaged reusable scratch");
            }
            numeric::Integer n = 1;
            mpz_mul_2exp(n.get_mpz_t(), n.get_mpz_t(), 1024);
            n += 37;
            auto wide = input;
            for (auto &q : wide)
                q = Q(n, numeric::Integer{41});
            check(action.apply(wide, workspace) == reference.apply(wide),
                  "wide reusable workspace truncated rational limbs");
            check(action.apply(input, workspace) == prior, "wide scratch reuse damaged small result");
            auto zero = input;
            for (auto &q : zero)
                q = 0;
            check(action.apply(zero, workspace) == numeric::RationalVector(prior.size()),
                  "zero input exposed prior accumulator contents");
            check(action.apply(input, workspace) == prior, "zero scratch reuse damaged prior output");
            const auto owned = action.apply(input, workspace);
            if (arena.incidence_count() > 0) {
                const auto flag = fibers.flag_map(core::Grade{1}, 0);
                fibers.set_transport(core::Grade{1}, 0,
                                     numeric::ExactMatrix(flag.transport.rows(), flag.transport.cols()));
            }
            check(action.apply(input, workspace) == owned,
                  "mutable source adapter rebound prepared coefficient storage");
        }
        auto escaped = [] {
            core::RelationSignature signature;
            const auto type = signature.add_type("one", core::BoundaryPolicy::DeclaredExact, 1, 1);
            core::RelationalComplexBuilder builder(signature);
            builder.add_grade_one(type, {{10, true, false, false, Q{2}}});
            const auto source = builder.freeze();
            local::LinearRelationalFiberSystem fibers{local::LinearStalkBundle(source)};
            return local::PreparedGradeFieldAction(fibers, core::Grade{1});
        }();
        auto scratch = escaped.make_workspace();
        check(escaped.apply({Q{7}}, scratch) == numeric::RationalVector({Q{14}}),
              "prepared local action borrowed destroyed fibers/source");
        std::cout << "checks " << checks << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "prepared field failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
