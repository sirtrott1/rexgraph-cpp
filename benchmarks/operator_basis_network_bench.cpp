#include "rfc/core/builder.hpp"
#include "rfc/learning/integer_network.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

static rfc::core::RelationalComplex make_ring(std::size_t n) {
    rfc::core::RelationSignature signature;
    const auto edge = signature.add_type("edge", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < n; ++i) {
        builder.add_grade_one(edge, {{i,true,true,false,1},{(i+1u)%n,true,false,false,1}});
    }
    return builder.freeze();
}

static std::int64_t checksum(const rfc::field::MachineLatticeField& field) {
    std::int64_t acc = 0;
    for (const auto value : field.values()) acc ^= value;
    return acc;
}

int main() {
    using namespace rfc::learning;
    constexpr std::size_t n = 20000;
    constexpr std::size_t repeats = 30;
    const auto complex = make_ring(n);
    const auto grade = rfc::core::Grade{1};
    const std::vector<OperatorChannelKind> candidates{
        OperatorChannelKind::Identity,
        OperatorChannelKind::HodgeDown,
        OperatorChannelKind::Hodge};
    const auto compile_begin = std::chrono::steady_clock::now();
    const auto basis = reduce_operator_channels(complex, grade, candidates);
    const auto compile_end = std::chrono::steady_clock::now();
    const auto compile_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(compile_end-compile_begin).count();
    // At top grade, Hodge == HodgeDown. The compiled layer removes that duplicate.
    CompiledIntegerRelationalLayer redundant(complex, grade, candidates, {1,1,1});
    const auto reduced_gains = reduce_machine_channel_gains(basis, {1,1,1});
    CompiledIntegerRelationalLayer compiled(complex, grade, basis.basis_channels(), reduced_gains);
    std::vector<std::int64_t> values(n, 0);
    for (std::size_t i = 0; i < n; ++i) values[i] = static_cast<std::int64_t>((i % 7u)) - 3;
    rfc::field::MachineLatticeField input(complex, grade, std::move(values));

    auto run = [&](const CompiledIntegerRelationalLayer& layer) {
        const auto begin = std::chrono::steady_clock::now();
        std::int64_t sum = 0;
        for (std::size_t r = 0; r < repeats; ++r) sum ^= checksum(layer.forward(input));
        const auto end = std::chrono::steady_clock::now();
        return std::pair{std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count(), sum};
    };
    const auto [redundant_ns, redundant_sum] = run(redundant);
    const auto [compiled_ns, compiled_sum] = run(compiled);
    std::cout << "relations=" << n << " repeats=" << repeats
              << " candidates=" << candidates.size() << " rank=" << basis.rank()
              << " compile_ns=" << compile_ns << '\n';
    std::cout << "redundant_ns=" << redundant_ns << " compiled_ns=" << compiled_ns
              << " checksum_equal=" << (redundant_sum == compiled_sum ? 1 : 0) << '\n';
    return redundant_sum == compiled_sum ? 0 : 1;
}
