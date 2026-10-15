#include "rfc/core/builder.hpp"
#include "rfc/field/execution_tower.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace rfc;
using Clock = std::chrono::steady_clock;
using Q = numeric::Rational;

static double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size()/2];
}

int main(int argc, char** argv) {
    try {
        const auto cells = argc > 1 ? std::stoull(argv[1]) : 1024u;
        const auto samples = argc > 2 ? std::stoull(argv[2]) : 8u;
        const auto repeats = argc > 3 ? std::stoull(argv[3]) : 9u;
        if (cells < 2 || !samples || !repeats) throw std::invalid_argument("positive cells, samples and repeats required");
        core::RelationSignature signature;
        const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        for (std::size_t i = 0; i < cells; ++i)
            builder.add_grade_one(core::RelationKey{100000+i}, edge, {{i,true,false,1},{(i+1)%cells,true,true,1}});
        const auto source = std::make_shared<const core::RelationalComplex>(builder.freeze());
        std::vector<field::ExactMetric> metrics;
        for (unsigned k = 0; k < 2; ++k) {
            numeric::RationalVector diagonal(cells);
            for (std::size_t i = 0; i < cells; ++i) diagonal[i] = Q(2+k) + Q(static_cast<unsigned long>(i%(5+k)), 3+k);
            metrics.push_back(field::ExactMetric::diagonal(std::move(diagonal)));
        }
        const field::PreparedExactExecutionTower tower(source, std::move(metrics));
        const field::OperatorRequest operation{field::OperatorKind::Hodge, core::Grade{1}, 0};
        auto workspace = tower.make_batch_workspace(core::Grade{1}, samples);
        numeric::RationalVector input(machine::checked_size_mul(cells,samples));
        std::vector<numeric::RationalVector> columns(samples,numeric::RationalVector(cells));
        for (std::size_t i = 0; i < cells; ++i) for (std::size_t s = 0; s < samples; ++s) {
            input[i*samples+s] = Q(static_cast<long>((i+3*s)%11)-5, static_cast<unsigned long>(7+i%3));
            input[i*samples+s].canonicalize();
            columns[s][i] = input[i*samples+s];
        }
        const auto scalar = [&] {
            std::vector<numeric::RationalVector> result; result.reserve(samples);
            for (const auto& column : columns) result.push_back(tower.apply(operation, column));
            return result;
        };
        const auto batch = [&] {
            numeric::RationalVector output(input.size());
            tower.apply_batch_into(operation, input, output, samples, workspace);
            return output;
        };
        const auto reference = scalar();
        const auto packed = batch();
        for (std::size_t i = 0; i < cells; ++i) for (std::size_t s = 0; s < samples; ++s)
            if (reference[s][i] != packed[i*samples+s]) throw std::runtime_error("rational batch parity failed");
        std::vector<double> scalar_ms, batch_ms;
        Q witness = 0;
        const auto time_scalar = [&] {
            const auto start = Clock::now(); const auto result = scalar();
            scalar_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            witness += result.back().back();
        };
        const auto time_batch = [&] {
            const auto start = Clock::now(); const auto result = batch();
            batch_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            witness += result.back();
        };
        for (std::size_t i = 0; i < repeats; ++i)
            if (i%2) { time_batch(); time_scalar(); } else { time_scalar(); time_batch(); }
        std::cout << "{\"cells\":" << cells << ",\"samples\":" << samples << ",\"repeats\":" << repeats
                  << ",\"scalar_ms\":" << median(scalar_ms) << ",\"batch_ms\":" << median(batch_ms)
                  << ",\"speedup\":" << median(scalar_ms)/median(batch_ms)
                  << ",\"workspace_slots\":" << workspace.retained_slots() << ",\"witness\":\"" << witness << "\"}\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
