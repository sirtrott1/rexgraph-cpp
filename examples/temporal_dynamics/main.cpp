#include "common.hpp"

int main(int argc, char **argv) {
    try {
        using namespace temporal_demo;
        if (argc == 7 && std::string_view(argv[1]) == "query") {
            if (std::string_view(argv[6]) != "--exact")
                throw std::invalid_argument("query requires --exact");
            query_file(argv[2], integer(argv[3]), rational(argv[4]), rational(argv[5]));
            return 0;
        }
        if (argc != 4 || std::string_view(argv[1]) != "run")
            throw std::invalid_argument("usage: temporal_dynamics run EVENTS.tsv NEW_OUTPUT_DIRECTORY | "
                                        "query SNAPSHOT.rfcxi CUTOFF START END --exact");
        const std::filesystem::path output(argv[3]);
        if (!std::filesystem::create_directory(output))
            throw std::invalid_argument("output directory must be new");
        const auto events = read_events(argv[2]);
        progress("1/4  Publish typed temporal relations and query retained endpoints");
        const auto frames = database_demo(events, output);
        progress("2/4  Resolve exact field dynamics and verify historical isolation");
        field_demo(frames, output);
        progress("3/4  Compute ordered propagation, typed moments and exact comparison cases");
        algebra_demo(frames);
        product_demo(output);
        emit({{"chapter", quote("verification")},
              {"checks", std::to_string(checks)},
              {"status", quote("passed")}});
        progress("4/4  Native artifacts and exact observations complete");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
