#include "rfc/core/builder.hpp"
#include "rfc/learning/integer_network.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace rfc;
using Clock = std::chrono::steady_clock;

static double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size()/2];
}

int main(int argc, char** argv) {
    try {
        const auto cells = argc > 1 ? std::stoull(argv[1]) : 4096u;
        const auto samples = argc > 2 ? std::stoull(argv[2]) : 32u;
        const auto repeats = argc > 3 ? std::stoull(argv[3]) : 9u;
        if (cells < 2 || !samples || !repeats) throw std::invalid_argument("positive cells, samples and repeats required");
        core::RelationSignature signature;
        const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        for (std::size_t i = 0; i < cells; ++i)
            builder.add_grade_one(core::RelationKey{100000+i}, edge, {{i,true,false,1},{(i+1)%cells,true,true,1}});
        const auto complex = builder.freeze();
        const core::Grade grade{1};
        using C = learning::OperatorChannelKind;
        const std::vector<C> channels{C::Identity,C::HodgeDown,C::HodgeUp,C::Hodge};
        learning::IntegerRelationalNetwork network(complex, grade);
        network.add_layer({complex, grade, channels, {1,1,-1,1}, 1, learning::IntegerActivation::ReLU});
        network.add_layer({complex, grade, channels, {2,-1,1,0}, 0, learning::IntegerActivation::Identity});
        learning::IntegerNetworkWorkspace workspace(complex, grade, samples);
        std::vector<std::int64_t> input(learning::IntegerNetworkWorkspace::slots(cells,samples));
        for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<std::int64_t>(i%7)-3;
        std::vector<field::MachineLatticeField> columns;
        for (std::size_t b = 0; b < samples; ++b) {
            std::vector<std::int64_t> column(cells);
            for (std::size_t i = 0; i < cells; ++i) column[i] = input[i*samples+b];
            columns.emplace_back(complex,grade,std::move(column));
        }
        auto scalar = [&] {
            std::vector<std::vector<std::int64_t>> result;
            result.reserve(samples);
            for (const auto& column : columns) result.push_back(network.forward(column).values());
            return result;
        };
        auto batch = [&] { return network.forward_batch(input,samples,workspace); };
        const auto original = scalar(); const auto packed = batch();
        for (std::size_t i = 0; i < cells; ++i) for (std::size_t b = 0; b < samples; ++b)
            if (original[b][i] != packed[i*samples+b]) throw std::runtime_error("batch parity failed");
        std::vector<double> scalar_ms, batch_ms;
        std::int64_t witness = 0;
        auto time_scalar = [&] {
            const auto start = Clock::now(); const auto result = scalar();
            scalar_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            witness = numeric::checked_add_i64(witness,result.back().back());
        };
        auto time_batch = [&] {
            const auto start = Clock::now(); const auto result = batch();
            batch_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            witness = numeric::checked_add_i64(witness,result.back());
        };
        for (std::size_t i = 0; i < repeats; ++i) {
            if (i%2) { time_batch(); time_scalar(); } else { time_scalar(); time_batch(); }
        }
        std::cout << "{\"cells\":" << cells << ",\"samples\":" << samples << ",\"repeats\":" << repeats
                  << ",\"scalar_ms\":" << median(scalar_ms) << ",\"batch_ms\":" << median(batch_ms)
                  << ",\"speedup\":" << median(scalar_ms)/median(batch_ms)
                  << ",\"workspace_slots\":" << workspace.retained_slots() << ",\"witness\":" << witness << "}\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
