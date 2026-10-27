#include <rfc/inference/relational_finite_source_codec.hpp>
#include <rfc/storage/file_io.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::size_t number(const std::string& text) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected a nonnegative decimal integer");
    std::size_t consumed{};
    const auto value = std::stoull(text, &consumed);
    if (consumed != text.size() || value > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("integer exceeds native size");
    return static_cast<std::size_t>(value);
}
}

// Search execution is selected explicitly. Every path consumes the same owned
// source and returns a whole witness or an honest bounded-search status.
int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 5)
            throw std::invalid_argument("usage: rfc_finite_solve ARCHIVE MAX_STEPS [bounded|propagated|distinct] [RELATION_INDICES]");
        const auto start = std::chrono::steady_clock::now();
        const std::string profile = argc >= 4 ? argv[3] : "bounded";
        if (profile != "bounded" && profile != "propagated" && profile != "distinct")
            throw std::invalid_argument("unknown finite search profile");
        if ((profile == "distinct") != (argc == 5))
            throw std::invalid_argument("distinct search requires explicit grade-one relation indices");
        const auto max_steps = number(argv[2]);
        std::vector<rfc::core::RelationId> relations;
        if (profile == "distinct") {
            const std::string text = argv[4];
            if (text.empty() || text.back() == ',') throw std::invalid_argument("empty distinct relation index");
            std::istringstream input(text); std::string index;
            while (std::getline(input, index, ',')) {
                const auto value = number(index);
                if (value > std::numeric_limits<std::uint32_t>::max())
                    throw std::invalid_argument("distinct relation index exceeds cell range");
                const rfc::core::RelationId relation{{1}, static_cast<std::uint32_t>(value)};
                if (std::find(relations.begin(), relations.end(), relation) != relations.end())
                    throw std::invalid_argument("duplicate distinct relation index");
                relations.push_back(relation);
            }
        }
        // Validate cheap request arguments before opening or decoding the source.
        // The bounded reader checks the opened file's extent and EOF; a separate
        // path-size check followed by an unbounded iterator admits file growth.
        const auto original = rfc::storage::read_file_exact(argv[1], 256u * 1024u * 1024u);
        rfc::numeric::ExactActionLimits limits;
        limits.dimension = 65536; limits.entries = 16000000; limits.steps = 64000000;
        const auto model = rfc::inference::decode_relational_finite_source(original, {}, limits);
        if (rfc::inference::encode_relational_finite_source(model) != original)
            throw std::logic_error("canonical source roundtrip changed");
        const auto built = std::chrono::steady_clock::now();
        const rfc::local::FiniteSearchLimits admission{65536, max_steps, 16000000};
        const auto solved = profile == "distinct" ? model.distinct_global_section(relations, {}, admission)
            : profile == "propagated" ? model.propagated_global_section({}, admission)
            : model.bounded_global_section({}, admission);
        const auto end = std::chrono::steady_clock::now();
        const char* status = "invalid";
        using S = rfc::local::FiniteSearchStatus;
        switch (solved.status) {
            case S::Satisfiable: status = "satisfiable"; break;
            case S::Unsatisfiable: status = "unsatisfiable"; break;
            case S::BudgetExceeded: status = "budget_exceeded"; break;
            case S::Cancelled: status = "cancelled"; break;
        }
        if (solved.witness) {
            for (const auto& variable : model.declaration().variables) {
                const auto value = solved.witness->at(variable.cell);
                if (std::find(variable.domain.begin(), variable.domain.end(), value) == variable.domain.end())
                    throw std::logic_error("witness outside original domain");
            }
            for (const auto& chart : model.declaration().charts) {
                std::vector<std::uint64_t> tuple;
                for (const auto cell : chart.support) tuple.push_back(solved.witness->at(cell));
                if (std::find(chart.allowed.begin(), chart.allowed.end(), tuple) == chart.allowed.end())
                    throw std::logic_error("witness violates original chart");
            }
        }
        if (rfc::inference::encode_relational_finite_source(model) != original)
            throw std::logic_error("search changed original source");
        const auto& w = solved.work;
        std::cout << "{\"status\":\"" << status << "\",\"source_canonical\":true,\"profile\":\"" << profile << '"';
#ifdef RFC_CONSUMER_ENGINE_DIGEST
        std::cout << ",\"engine_headers\":\"" << RFC_CONSUMER_ENGINE_DIGEST << '"';
#endif
        std::cout << ",\"steps\":" << w.steps << ",\"value_trials\":" << w.value_trials
            << ",\"chart_tests\":" << w.chart_tests << ",\"tuple_tests\":" << w.tuple_tests
            << ",\"coordinate_tests\":" << w.coordinate_tests << ",\"variables\":" << w.variables
            << ",\"peak_assigned\":" << w.peak_assigned << ",\"peak_entries\":" << w.peak_entries
            << ",\"domain_reductions\":" << w.domain_reductions << ",\"search_nodes\":" << w.search_nodes
            << ",\"distinct_groups\":" << w.distinct_groups << ",\"matching_tests\":" << w.matching_tests
            << ",\"build_nanoseconds\":" << std::chrono::duration_cast<std::chrono::nanoseconds>(built-start).count()
            << ",\"solve_nanoseconds\":" << std::chrono::duration_cast<std::chrono::nanoseconds>(end-built).count()
            << ",\"witness\":";
        if (!solved.witness) std::cout << "null";
        else {
            std::cout << '['; bool first = true;
            for (const auto& variable : model.declaration().variables) {
                if (!first) std::cout << ',';
                first = false; std::cout << solved.witness->at(variable.cell);
            }
            std::cout << ']';
        }
        std::cout << "}\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
