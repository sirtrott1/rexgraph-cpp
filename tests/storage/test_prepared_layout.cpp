#include "rfc/storage/declaration_codec.hpp"
#include "rfc/composite/declaration_evolution.hpp"
#include "rfc/storage/temporal_declarations.hpp"
#include <future>
#include <iostream>

namespace {
using namespace rfc;
using namespace composite;
using V = DeclaredValue;
std::size_t checks{};
DeclarationId id(std::uint64_t key, std::uint32_t version = 1) { return {701, key, version}; }
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F&& operation, const char* message) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
DeclarationRegistry definitions() {
    return DeclarationRegistry({Declaration::integer(id(1)), Declaration::finite(id(2), {7, 3, 9}),
        Declaration::repeated(id(3), id(1), 0, 10),
        Declaration::product(id(4), {{8, id(3), 0}, {5, id(2), 0}}),
        Declaration::rational_tensor(id(5), {{1, 2, {11, 22}}, {2, 3, {33, 44, 55}}})});
}
void shared_views() {
    auto original = definitions();
    PreparedLayoutRegistry retained(original);
    const auto transferred = std::move(retained);
    check(&retained.declarations() == &transferred.declarations(), "rvalue registry transfer invalidated its source owner");
    const auto views = [&] {
        PreparedLayoutRegistry owner(original);
        return std::pair{CompiledLayout(owner, id(1)), CompiledLayout(owner, id(4))};
    }();
    check(&views.first.declarations() == &views.second.declarations(), "prepared roots copied the registry");
    const auto copied = views.second;
    check(&copied.declarations() == &views.second.declarations(), "layout copy detached the registry owner");
    original = DeclarationRegistry({Declaration::boolean(id(1))});
    const auto integer = V::integer(numeric::Integer{-31});
    views.first.validate(integer);
    rejects([&] { views.first.validate(V::boolean(true)); }, "caller mutation changed a retained declaration");
    const auto value = V::product({V::repeated({V::integer(0), integer}), V::symbol(3)});
    const CompiledLayout independent(definitions(), id(4));
    check(&independent.declarations() != &views.second.declarations(), "independent registries were conflated");
    const auto actual = storage::encode_declared_bits(views.second, value);
    const auto expected = storage::encode_declared_bits(independent, value);
    check(actual == expected, "prepared root changed canonical bits");
    check(declared_value_equal(storage::decode_declared_bits(views.second, actual), value), "retained root failed binary reconstruction");
    const auto use = independent.allocation(value);
    DeclarationLimits exact;
    exact.max_value_nodes = use.value_nodes;
    exact.max_materialized_bytes = use.materialized_bytes;
    const CompiledLayout bounded(PreparedLayoutRegistry(definitions(), exact), id(4));
    bounded.validate(value);
    --exact.max_value_nodes;
    rejects([&] { CompiledLayout(PreparedLayoutRegistry(definitions(), exact), id(4)).validate(value); }, "aggregate value node policy was lost");
    exact = {}; exact.max_definitions = 4;
    rejects([&] { (void)PreparedLayoutRegistry(definitions(), exact); }, "complete registry policy was bypassed");
    rejects([&] { (void)CompiledLayout(PreparedLayoutRegistry(definitions()), id(99)); }, "foreign root accepted");
    rejects([&] { (void)CompiledLayout(PreparedLayoutRegistry(definitions()), id(1), static_cast<BooleanLayoutProfile>(255)); }, "unsupported layout profile accepted");
    const auto tensor = V::tensor(numeric::SparseRationalTensor(numeric::IndexDomain({2, 3}),
        numeric::StructuralSupport::sparse(6, {0, 2, 5}),
        numeric::SparseRationalVector(6, {{0, numeric::Rational{1, 3}}, {5, numeric::Rational{-7}}})));
    const CompiledLayout tensor_layout(PreparedLayoutRegistry(definitions()), id(5));
    check(declared_value_equal(storage::decode_declared_bits(tensor_layout, storage::encode_declared_bits(tensor_layout, tensor)), tensor), "prepared tensor lost supported zero or exact values");
    DeclarationLimits wide; wide.max_integer_bits = 2000000;
    PreparedLayoutRegistry wide_owner(DeclarationRegistry({Declaration::integer(id(1))}), wide);
    wide.max_integer_bits = 10;
    numeric::Integer large; mpz_setbit(large.get_mpz_t(), 1599999);
    const CompiledLayout wide_layout(wide_owner, id(1));
    wide_layout.validate(V::integer(large));
    check(wide_layout.limits().max_integer_bits == 2000000, "prepared policy followed a mutable caller");
    rejects([&] { CompiledLayout(DeclarationRegistry({Declaration::integer(id(1))}), id(1)).validate(V::integer(large)); }, "default policy changed through another owner");
    const DeclarationRecoding shared(definitions(), definitions(), identity_declaration_maps(definitions()));
    auto first = std::async(std::launch::async, [&] { return shared.apply(id(4), value); });
    auto second = std::async(std::launch::async, [&] { return shared.apply(id(5), tensor); });
    check(declared_value_equal(first.get(), value) && declared_value_equal(second.get(), tensor), "concurrent recoding changed a value or support");
    const DeclarationRegistry empty({});
    const DeclarationRecoding empty_map(empty, empty, {});
    rejects([&] { (void)empty_map.apply(id(1), integer); }, "empty map accepted a foreign declaration");
    const DeclarationRegistry base({Declaration::integer(id(1))});
    const DeclarationRegistry redefined({Declaration::boolean(id(1)), Declaration::integer(id(9))});
    rejects([&] { (void)DeclarationRecoding(base, redefined, {{id(1), id(9), DeclarationMapKind::Recode, {}}}); }, "unmapped retained identity was redefined");
}
void alternating_roots() {
    storage::TemporalDeclarationUpdate empty;
    empty.target.emplace();
    check(storage::temporal_declaration_detail::update_usage(empty) == std::pair<std::size_t,std::size_t>{0,0},
          "unused update target acquired a new validation requirement");
    storage::TemporalDeclarationState state;
    state.namespace_key = 701;
    state.registry = definitions();
    const auto product = V::product({V::repeated({V::integer(0),V::integer(-31)}),V::symbol(3)});
    for (std::uint64_t i=0;i<100;++i)
        state.values.push_back({i+1,i%2 ? id(4) : id(1),0,0,{},{},i%2 ? product : V::integer(-31)});
    storage::TemporalDeclarationUpdate update;
    update.target = state;
    const auto use = storage::temporal_declaration_detail::update_usage(update);
    auto exact = composite::DeclarationLimits{};
    exact.max_value_nodes = use.first;
    exact.max_materialized_bytes = use.second;
    storage::temporal_declaration_detail::validate_definitions(state,exact);
    check(storage::temporal_declaration_detail::update_usage(update,exact)==use,
          "alternating roots changed aggregate resource accounting");
    --exact.max_value_nodes;
    rejects([&]{storage::temporal_declaration_detail::validate_definitions(state,exact);},
            "alternating roots lost the aggregate node budget");
    rejects([&]{(void)storage::temporal_declaration_detail::update_usage(update,exact);},
            "update roots lost the aggregate node budget");
    ++exact.max_value_nodes;
    --exact.max_materialized_bytes;
    rejects([&]{storage::temporal_declaration_detail::validate_definitions(state,exact);},
            "alternating roots lost the aggregate byte budget");
    rejects([&]{(void)storage::temporal_declaration_detail::update_usage(update,exact);},
            "update roots lost the aggregate byte budget");
}
}
int main() {
    try {
        shared_views();
        alternating_roots();
        std::cout << checks << " prepared registry checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
