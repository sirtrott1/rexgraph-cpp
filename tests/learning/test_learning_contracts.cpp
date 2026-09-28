#include "rfc/learning/field_batch.hpp"
#include "rfc/learning/scalar_policy.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    const rfc::learning::ScalarPolicy default_policy{};
    if (default_policy.domain != rfc::learning::ScalarDomain::ExactInteger) return EXIT_FAILURE;

    rfc::learning::RelationalBatch<std::int16_t> batch;
    batch.batch_size = 1;
    batch.channels.push_back({"green", 1, 0, {1,2,3}});
    batch.relation_ids = {10,11,12};
    if (batch.channels.front().values.size() != 3u) return EXIT_FAILURE;

    const rfc::learning::ScalarPolicy quantized{rfc::learning::ScalarDomain::Int16, 0, false};
    if (rfc::learning::to_string(quantized.domain) != "int16") return EXIT_FAILURE;
    std::cout << "learning scalar-policy tests passed\n";
    return EXIT_SUCCESS;
}
