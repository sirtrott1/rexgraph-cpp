#include "../native_product_map_associativity/fixture.hpp"
#include "../native_tensor_product/fixture.hpp"
#include "common.hpp"
#include "rfc/storage/native_series_codec.hpp"

namespace temporal_demo {
void product_demo(const std::filesystem::path &output) {
    using namespace rfc;
    const auto history = storage::read_native_series(output / "fields/history.rfcxs");
    core::RelationSignature signature;
    const auto type =
        signature.add_type("declared-asset-interface", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{801}, type, {{1, true, true, 1}, {2, true, false, 1}});
    builder.declare_witness_identity(3);
    storage::NativeAccessionDirectory assets(storage::TemporalEndpoint(builder.freeze(), 0), 97109);
    for (const auto cutoff : std::vector<std::uint64_t>{0, history.last_sequence()}) {
        const auto endpoint = history.at(cutoff);
        storage::NativeAccessionDirectory time(endpoint.endpoint(), field_namespace);
        const inference::NativeProductAvailability available{{field_namespace, cutoff}, {97109, 0}};
        const auto plan =
            native_product_example::Plan::prepare(native_product_example::request(time, assets), available);
        const auto input =
            plan.field(core::Grade{2}, {{native_product_example::cell(1, 0, 1, 0), Q{1}}}, available);
        const auto boundary = plan.boundary(input, available), twice = plan.boundary(boundary, available);
        for (const auto &[cell, value] : twice.field().values()) {
            (void)cell;
            require(value == 0, "time and asset tensor product violates the chain condition");
        }
        numeric::ExactActionGuard guard;
        const auto retained =
            plan.product().retained_boundary(native_product_example::cell(1, 0, 1, 0), guard);
        std::vector<std::string> entries;
        for (const auto &[cell, value] : boundary.field().values())
            entries.push_back(object({{"time_grade", std::to_string(cell.left.grade.value)},
                                      {"time_index", std::to_string(cell.left.index)},
                                      {"asset_grade", std::to_string(cell.right.grade.value)},
                                      {"asset_index", std::to_string(cell.right.index)},
                                      {"value", quote(value.get_str())}}));
        emit({{"chapter", quote("tensor_product")},
              {"sequence", std::to_string(cutoff)},
              {"time_namespace", std::to_string(field_namespace)},
              {"asset_namespace", "97109"},
              {"asset_cutoff", "0"},
              {"input_grade", "2"},
              {"boundary", array(entries)},
              {"retained_occurrences", std::to_string(retained.size())},
              {"supported_second_boundary", std::to_string(twice.field().values().size())},
              {"boundary_squared_zero", "true"}});
    }
    // This separate small reference exhibits the complete owning three-factor
    // map profile. It does not identify these carriers with the incident data.
    using namespace native_associativity_example;
    const auto example = native_associativity_example::plans(6);
    const auto available = available_associativity;
    const core::CellId vertex{{0}, 0};
    const auto x = example.associativity.prefix().first().source().field(
        carrier(0), {0}, {{cell(carrier(0), {vertex, vertex, vertex}), Q{1}}}, available[0]);
    const auto old = std::make_shared<const rfc::inference::NativeProductCompositionLift>(
        example.prefix.apply(x, {available[0], available[1], available[2]}, associativity_lease()));
    const auto continued = example.associativity.continue_from(old, available, associativity_lease());
    const auto direct = example.associativity.apply(x, available, associativity_lease());
    require(continued.prefix_input() == old && continued.last_stage()->prior_input() == old->stages()[1],
            "native three-map continuation lost the prior receipt");
    require(continued.lift().last().image().values() == direct.lift().last().image().values(),
            "native continuation differs from a fresh three-map application");
    const auto numeric_plan = example.associativity.numeric_plan();
    emit({{"chapter", quote("map_coherence")},
          {"scope", quote("separate canonical three-factor reference case")},
          {"original_endpoint_roles", "12"},
          {"parenthesizations", "2"},
          {"triple_factor_paths", std::to_string(numeric_plan.factors()[0][0].contributions.size())},
          {"original_receipt_retained", "true"},
          {"prior_map_receipt_retained", "true"}});
}
} // namespace temporal_demo
