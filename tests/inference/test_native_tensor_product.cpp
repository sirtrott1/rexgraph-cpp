#include "../../examples/native_tensor_product/fixture.hpp"
#include "rfc/algebra/tensor_product.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace rfc;
using namespace native_product_example;
using NumericMap = algebra::PreparedTensorChainMap;
std::size_t checks{}, cases{};
void check(bool yes, const char* message) { ++checks; if (!yes) throw std::runtime_error(message); }
template<class Error, class F> void rejects(F action, const char* message) {
    bool caught = false; try { action(); } catch (const Error&) { caught = true; } check(caught, message);
}
Product own(core::RelationalComplex a, core::RelationalComplex b) {
    const auto d = declaration(a, b); Guard guard;
    return Product::prepare(std::move(a), std::move(b), d, guard);
}
bool zero(const algebra::OwnedProductField& f) {
    for (const auto& [id, q] : f.values()) { (void)id; if (q != 0) return false; } return true;
}
bool zero(const inference::DerivedNativeProductField& f) { return zero(f.field()); }
Product::Values values(const algebra::OwnedProductField& f) { return f.values(); }
Product::Values values(const inference::DerivedNativeProductField& f) { return f.field().values(); }
bool equal_values(const algebra::OwnedProductField& a, const algebra::OwnedProductField& b) {
    if (!a.product().same_owner(b.product()) || a.grade() != b.grade()) return false;
    for (const auto& [id, q] : a.values()) {
        const auto it = b.values().find(id); if (q != (it == b.values().end() ? Q{0} : it->second)) return false;
    }
    for (const auto& [id, q] : b.values()) {
        const auto it = a.values().find(id); if (q != (it == a.values().end() ? Q{0} : it->second)) return false;
    }
    return true;
}
void json(const algebra::OwnedProductField& field) {
    std::cout << '['; bool comma = false;
    for (const auto& [id, q] : field.values()) {
        if (comma) std::cout << ',';
        comma = true;
        std::cout << '[' << id.left.grade.value << ',' << id.left.index << ',' << id.right.grade.value << ','
                  << id.right.index << ",\"" << q << "\"]";
    }
    std::cout << ']';
}
void json(const Matrix& a) {
    std::cout << "{\"rows\":" << a.rows() << ",\"cols\":" << a.cols() << ",\"entries\":["; bool comma = false;
    for (const auto& [ij, q] : a.entries()) {
        if (comma) std::cout << ',';
        comma = true;
        std::cout << '[' << ij.first << ',' << ij.second << ",\"" << q << "\"]";
    }
    std::cout << "]}";
}
void json(const std::vector<Matrix>& maps) {
    std::cout << '['; for (std::size_t k = 0; k < maps.size(); ++k) { if (k) std::cout << ','; json(maps[k]); } std::cout << ']';
}
void json(const core::RelationalComplex& source) {
    std::cout << "{\"sizes\":[";
    for (std::size_t k = 0; k < source.grade_count(); ++k) { if (k) std::cout << ','; std::cout << source.grade(core::Grade{static_cast<std::uint16_t>(k)}).cell_count(); }
    std::cout << "],\"occurrences\":["; bool comma = false;
    for (std::size_t k = 1; k < source.grade_count(); ++k) {
        const auto g = core::Grade{static_cast<std::uint16_t>(k)}; const auto& a = source.grade(g);
        for (std::size_t p = 0; p < a.incidence_count(); ++p) {
            if (comma) std::cout << ',';
            comma = true;
            std::cout << '[' << k << ',' << a.upper_index[p] << ',' << a.lower_index[p] << ','
                      << a.occurrence_identities[p].value << ',' << a.aspects.get(source.signature().existence_aspect(), p)
                      << ",\"" << source.incidence_coefficient(g, p) << "\",[";
            for (std::size_t j = 0; j < source.signature().aspects().size(); ++j) {
                if (j) std::cout << ',';
                std::cout << a.aspects.get(static_cast<composite::AspectId>(j), p);
            }
            std::cout << "]]";
        }
    }
    std::cout << "]}";
}
void emit(const Map& map, const inference::DerivedNativeProductField& input,
          const inference::DerivedNativeProductField& output, const std::optional<inference::DerivedNativeProductField>& before,
          const std::optional<inference::DerivedNativeProductField>& after) {
    std::cout << "{\"grade\":" << input.field().grade().value << ",\"sources\":[";
    json(map.source().factor(false).endpoint()->source); std::cout << ','; json(map.source().factor(true).endpoint()->source);
    std::cout << "],\"targets\":[";
    json(map.target().factor(false).endpoint()->source); std::cout << ','; json(map.target().factor(true).endpoint()->source);
    std::cout << "],\"maps\":["; json(map.numeric_plan().factor_maps(false)); std::cout << ','; json(map.numeric_plan().factor_maps(true));
    std::cout << "],\"input\":"; json(input.field()); std::cout << ",\"output\":"; json(output.field());
    std::cout << ",\"source_boundary\":"; if (before) json(before->field()); else std::cout << "null";
    std::cout << ",\"target_boundary\":"; if (after) json(after->field()); else std::cout << "null";
    std::cout << "}\n";
}
void samples(bool output_records) {
    const std::array<Q, 3> scales{Q{1}, Q{2, 3}, Q{5, 2}};
    for (const auto& a : scales) for (const auto& b : scales) for (unsigned family = 0; family < 2; ++family) {
        storage::NativeAccessionDirectory da(storage::TemporalEndpoint(interval(a, true), 7), 900),
            db(storage::TemporalEndpoint(interval(b, true), 8), 901),
            dc(storage::TemporalEndpoint(interval(Q{3, 4}, true), 9), 902),
            dd(storage::TemporalEndpoint(interval(Q{2}, true), 10), 903);
        const auto source = Plan::prepare(request(da, db), available), target = Plan::prepare(request(dc, dd), target_available);
        const auto t = family ? Q{-2, 3} : Q{0};
        const auto left = factor_map(a, Q{3, 4}, t, Q{1, 5}, Q{-1}, Q{-3, 5});
        const auto right = factor_map(b, Q{2}, t, Q{0}, Q{2}, Q{family ? 1 : 0});
        const auto map = Map::prepare(source, target, left, right, {available, target_available});
        for (std::uint16_t p = 0; p < 3; ++p) for (std::uint16_t q = 0; q < 3; ++q)
            for (std::uint32_t i = 0; i < (p ? 1u : 2u); ++i) for (std::uint32_t j = 0; j < (q ? 1u : 2u); ++j)
                for (const Q& value : {Q{0}, Q{-3, 7}}) {
                    const auto id = cell(p, i, q, j);
                    const auto input = source.field(id.grade(), {{id, value}}, available);
                    const auto image = map.execute(input, {available, target_available});
                    check(input.field().values().size() == 1 && input.field().values().at(id) == value, "supported input zero retained");
                    check(image.product().same_owner(target) && image.input()->product().same_owner(source) && image.chain_map().has_value(), "map result owns actual input, both products and whole map");
                    check(image.chain_map()->numeric_plan().factor_maps(false) == left && image.chain_map()->numeric_plan().factor_maps(true) == right, "whole factor maps retained");
                    std::optional<inference::DerivedNativeProductField> before, after;
                    if (id.grade().value) {
                        before = source.boundary(input, available); after = target.boundary(image, target_available);
                        const auto transported = map.execute(*before, {available, target_available});
                        check(equal_values(transported.field(), after->field()), "whole tensor map commutes with boundary");
                        if (id.grade().value > 1) {
                            check(zero(source.boundary(*before, available)), "complete product chain family on every basis direction");
                            check(zero(target.boundary(*after, target_available)), "complete target product chain family");
                        }
                    }
                    if (output_records) emit(map, input, image, before, after);
                    ++cases;
                }
    }
}
void numeric_contracts() {
    const auto source = interval(); const auto product = own(source, source);
    Guard guard;
    const auto top = cell(1, 0, 1, 0); const auto certificate = product.cell(top);
    check(certificate.source_identity.left.key == 100 && certificate.source_identity.right.key == 100 && certificate.type != product.cell(cell(0, 0, 0, 0)).type, "grade-zero type token and native identity pair");
    const auto saved_cell = [] { return own(interval(), interval()).cell(cell(1, 0, 1, 0)); }();
    check(saved_cell.product().cell(saved_cell.cell).source_identity == saved_cell.source_identity, "standalone cell certificate retains its complete source owner");
    const auto saved_occurrence = [] { Guard local; return own(interval(), interval()).retained_boundary(cell(1, 0, 1, 0), local).at(9); }();
    const auto occurrence_owner = saved_occurrence.product();
    check(saved_occurrence.factor_occurrence.value == 104 && occurrence_owner.cell(saved_occurrence.upper).source_identity.left.key == 100 &&
          occurrence_owner.cell(saved_occurrence.lower).source_identity.right.key == 10, "standalone occurrence certificate retains both full factor sources");
    const auto word = product.retained_boundary(top, guard);
    check(word.size() == 12, "full repeated cancelling inactive and zero occurrence fibers");
    for (std::size_t i = 0; i < word.size(); ++i) {
        const auto& o = word[i]; const bool right = i >= 6; const auto position = i % 6;
        check(o.side == (right ? algebra::ProductBoundarySide::Right : algebra::ProductBoundarySide::Left) && o.attaching_position == position && o.factor_occurrence.value == 101 + position, "original factor occurrence and attaching order");
        check(o.koszul_flip == right && o.coefficient == (right ? -o.factor_coefficient : o.factor_coefficient), "Koszul parity remains separate");
        check(o.factor_aspects.size() == 3 && o.factor_aspects[2] == (position % 2 == 0) && o.exists == (position != 4), "full original composite aspect word");
    }
    auto input = product.field(core::Grade{2}, {{top, Q{1}}}, guard);
    const auto d = product.boundary(input, guard), dd = product.boundary(d, guard);
    check(d.values() == Product::Values{{cell(0, 0, 1, 0), -1}, {cell(0, 1, 1, 0), 1}, {cell(1, 0, 0, 0), 1}, {cell(1, 0, 0, 1), -1}}, "canonical oriented square");
    check(dd.values().size() == 4 && zero(dd), "supported vertices survive exact cancellation");
    check(values(product.boundary(product.field(core::Grade{2}, {{top, Q{0}}}, guard), guard)).size() == 4, "zero input retains boundary support");
    const auto old_bytes = storage::encode_rfcx(source).bytes;
    check(storage::encode_rfcx(product.factor(false)).bytes == old_bytes, "owned source byte preservation");
    auto declaration_word = declaration(source, source); std::reverse(declaration_word.types.begin(), declaration_word.types.end());
    const auto reordered = Product::prepare(source, source, declaration_word, guard);
    check(reordered.declaration().types == declaration_word.types && reordered.cell(top).type == certificate.type, "type declaration order retained separately from lookup");
    rejects<std::invalid_argument>([&] { reordered.boundary(input, guard); }, "equal declarations never bind an unrelated owner");
    auto incomplete = declaration(source, source); incomplete.types.pop_back();
    rejects<std::invalid_argument>([&] { Product::prepare(source, source, incomplete, guard); }, "unused declared type pairs cannot be omitted");
    incomplete = declaration(source, source); incomplete.types.back() = incomplete.types.front();
    rejects<std::invalid_argument>([&] { Product::prepare(source, source, incomplete, guard); }, "duplicate product type pair");
    incomplete = declaration(source, source); incomplete.types.back().key = incomplete.types.front().key;
    rejects<std::invalid_argument>([&] { Product::prepare(source, source, incomplete, guard); }, "duplicate product output identity");
    incomplete = declaration(source, source); incomplete.types.back().left = 999;
    rejects<std::invalid_argument>([&] { Product::prepare(source, source, incomplete, guard); }, "undeclared factor type");
    incomplete = declaration(source, source); incomplete.key = 0;
    rejects<std::invalid_argument>([&] { Product::prepare(source, source, incomplete, guard); }, "constructor identity required");
    rejects<std::out_of_range>([&] { product.field(core::Grade{3}, {}, guard); }, "empty field grade still bounded");
    rejects<std::out_of_range>([&] { product.field(core::Grade{1}, {{cell(1, 2, 0, 0), 0}}, guard); }, "zero input factor index still checked");
    rejects<std::invalid_argument>([&] { product.field(core::Grade{1}, {{top, 0}}, guard); }, "zero input grade still checked");
    rejects<std::invalid_argument>([&] { product.boundary(product.field(core::Grade{0}, {}, guard), guard); }, "grade zero boundary rejects");
    const auto large_source = interval(1, false, 3000); const auto large = own(large_source, large_source);
    check(large.cell_count(core::Grade{0}) == 9012004, "large logical product carrier");
    const auto sparse = large.boundary(large.field(core::Grade{1}, {{cell(1, 0, 0, 3001), 1}}, guard), guard);
    check(sparse.values().size() == 2, "sparse action never allocates ambient product coordinates");
    const auto empty = own(unit(true, 2), unit(true, 1));
    check(empty.max_grade().value == 3 && empty.cell_count(core::Grade{3}) == 0 && values(empty.boundary(empty.field(core::Grade{3}, {}, guard), guard)).empty(), "empty grades remain declared");
    rejects<std::overflow_error>([&] { own(unit(true, 65535), unit(true, 1)); }, "total grade overflow rejects before narrowing");
    const auto point = own(unit(), unit());
    const auto face_product = own(interval(1, true), interval(1, true));
    const std::vector<Matrix> include{Matrix::from_rows({{2}, {3}}), Matrix(1, 0), Matrix(1, 0)},
        fold{Matrix::from_rows({{1, 1}}), Matrix(0, 1), Matrix(0, 1)};
    const auto injection = NumericMap::prepare(point, face_product, include, include, guard);
    auto point_field = point.field(core::Grade{0}, {{cell(0, 0, 0, 0), 1}}, guard);
    const auto included = injection.apply(point_field, guard);
    check(included.values().size() == 4 && included.values().at(cell(0, 1, 0, 1)) == 9, "rectangular factors include all target grades");
    const auto padded_point = own(unit(false, 2), unit(false, 2));
    const auto collapse = NumericMap::prepare(face_product, padded_point, fold, fold, guard);
    check(values(collapse.apply(included, guard)).begin()->second == 25, "rectangular chain map is not silently an inverse");
    rejects<std::invalid_argument>([&] { NumericMap::prepare(face_product, point, fold, fold, guard); }, "target empty grades must be explicitly declared");
    auto bad = include; bad.pop_back();
    rejects<std::invalid_argument>([&] { NumericMap::prepare(point, face_product, bad, include, guard); }, "unused target grades must be declared");
    auto identities = factor_map(1, 1, 1, 0, 0); bad = identities; bad[0].set(0, 1, 1);
    rejects<std::invalid_argument>([&] { NumericMap::prepare(face_product, face_product, bad, identities, guard); }, "bad unused map direction rejects complete chain identity");
    const auto cancel_maps = factor_map(1, 1, 0, 1, 1);
    const auto cancelling = NumericMap::prepare(face_product, face_product, cancel_maps, cancel_maps, guard);
    const auto cancel_field = face_product.field(core::Grade{0}, {{cell(0, 0, 0, 0), 1}, {cell(0, 1, 0, 0), -1}}, guard);
    const auto cancelled = cancelling.apply(cancel_field, guard);
    check(cancelled.values().size() == 4 && zero(cancelled), "tensor map retains numerical support through cancellation");
    auto handle = injection; bool changed = false;
    Guard callback({}, [&] { if (!changed) { changed = true; handle = collapse; } });
    check(values(handle.apply(point_field, callback)) == included.values() && changed, "map handle pinned before replacement checkpoint");
    auto owner = product; changed = false;
    Guard product_callback({}, [&] { if (!changed) { changed = true; owner = point; } });
    check(values(owner.boundary(input, product_callback)) == d.values(), "product handle pinned before replacement checkpoint");
    auto left_argument = source, right_argument = source; auto type_argument = declaration(source, source); changed = false;
    Guard prepare_callback({}, [&] { if (!changed) { changed = true; left_argument = unit(); right_argument = unit(); type_argument.types.clear(); } });
    const auto captured = Product::prepare(left_argument, right_argument, type_argument, prepare_callback);
    check(captured.cell_count(core::Grade{2}) == 1 && captured.declaration().types.size() == 9, "complete source and type arguments owned before preparation callback");
    auto values_argument = Product::Values{{top, Q{1}}}; changed = false;
    Guard field_callback({}, [&] { if (!changed) { changed = true; values_argument.clear(); } });
    check(product.field(core::Grade{2}, values_argument, field_callback).product().same_owner(product), "input value word captured before callback");
    auto left_maps_argument = include, right_maps_argument = include; changed = false;
    Guard maps_callback({}, [&] { if (!changed) { changed = true; left_maps_argument.clear(); right_maps_argument.clear(); } });
    const auto captured_map = NumericMap::prepare(point, face_product, left_maps_argument, right_maps_argument, maps_callback);
    check(captured_map.factor_maps(false) == include && captured_map.factor_maps(true) == include, "complete factor map arguments captured before callback");
    Guard measured; (void)product.boundary(input, measured);
    numeric::ExactActionLimits exact_cap; exact_cap.steps = measured.steps() - 1; Guard just_below(exact_cap);
    rejects<std::length_error>([&] { product.boundary(input, just_below); }, "exact current work threshold enforced inside the action");
    numeric::ExactActionLimits cap; cap.dimension = 1; Guard dimension_guard(cap);
    rejects<std::length_error>([&] { product.boundary(input, dimension_guard); }, "complete unused factor dimensions checked");
    cap = {}; cap.integer_bits = 0; Guard bit_guard(cap);
    rejects<std::length_error>([&] { empty.field(core::Grade{0}, {}, bit_guard); }, "empty action validates retained grade-zero constants");
    cap = {}; cap.entries = product.entry_envelope() - 1; Guard entries_guard(cap);
    rejects<std::length_error>([&] { product.field(core::Grade{0}, {}, entries_guard); }, "whole retained container cap on empty field");
    cap = {}; cap.steps = 0; Guard work_guard(cap);
    rejects<std::length_error>([&] { product.field(core::Grade{0}, {}, work_guard); }, "current work cap on empty field");
    Guard cancel_guard({}, [] { throw runtime::ExecutionCancelled(); });
    rejects<runtime::ExecutionCancelled>([&] { product.boundary(input, cancel_guard); }, "owned exact cancellation");
    check(values(product.boundary(input, guard)) == d.values(), "failed action preserves source and prior field");
    auto malformed = Q{1}; mpz_set_ui(mpq_denref(malformed.get_mpq_t()), 0);
    rejects<std::invalid_argument>([&] { product.field(core::Grade{2}, {{top, malformed}}, guard); }, "malformed exact denominator rejects before canonicalization");
}
void native_contracts() {
    storage::NativeAccessionDirectory a(storage::TemporalEndpoint(interval(), 7), 900), b(storage::TemporalEndpoint(interval(), 8), 901);
    auto r = request(a, b); const auto plan = Plan::prepare(r, available);
    const auto top = cell(1, 0, 1, 0); auto input = plan.field(core::Grade{2}, {{top, 1}}, available);
    const auto first = plan.boundary(input, available); const auto second = plan.boundary(first, available);
    check(second.input()->input()->field().values() == input.field().values(), "ordered native action inputs remain retained");
    auto future = available; ++future.left.max_sequence; ++future.right.max_sequence;
    check(values(plan.boundary(input, future)) == first.field().values(), "larger current cutoff never advances selected source");
    auto stale = available; --stale.left.max_sequence;
    rejects<std::invalid_argument>([&] { Plan::prepare(r, stale); }, "future source rejected during preparation");
    rejects<std::invalid_argument>([&] { plan.boundary(input, stale); }, "current cutoff remains binding");
    stale = available; stale.right.source_namespace = 777;
    rejects<std::invalid_argument>([&] { plan.field(core::Grade{0}, {}, stale); }, "wrong current namespace on empty action");
    auto bad = r; bad.right_factor = bad.left_factor;
    rejects<std::invalid_argument>([&] { Plan::prepare(bad, available); }, "distinct declared factors required even for same source");
    bad = r; bad.factor_dimension_limit = 1;
    rejects<std::length_error>([&] { Plan::prepare(bad, available); }, "factor preparation dimension bound");
    const auto twin = Plan::prepare(r, available);
    rejects<std::invalid_argument>([&] { twin.boundary(input, available); }, "equal native bytes do not bind product field owners");
    storage::NativeAccessionDirectory unrelated(storage::TemporalEndpoint(interval(), 7), 900);
    const auto foreign = Plan::prepare(request(unrelated, b), available);
    rejects<std::invalid_argument>([&] { foreign.boundary(input, available); }, "foreign same namespace keys and values remain distinct");
    const auto old = a.pin_snapshot(); auto next = *old; next.sequence = 8;
    a.advance(storage::make_temporal_segment(*old, next));
    check(plan.factor(false).sequence() == 7 && plan.factor(false).endpoint() == old && values(plan.boundary(input, available)) == first.field().values(), "old source remains pinned through advancement");
    rejects<std::invalid_argument>([&] { Plan::prepare(request(a, b), available); }, "later pin cannot enter an earlier declared cutoff");
    auto newer_available = available; newer_available.left.max_sequence = 8;
    const auto newer = Plan::prepare(request(a, b), newer_available);
    rejects<std::invalid_argument>([&] { plan.boundary(newer.field(core::Grade{2}, {{top, 1}}, newer_available), newer_available); }, "future equal field cannot enter old product owner");
    runtime::ExecutionHostPolicy policy; policy.allow_allocation = false;
    rejects<std::invalid_argument>([&] { plan.boundary(input, available, runtime::ExecutionLease(policy)); }, "explicit owning allocation admission");
    policy = {}; policy.backend = runtime::ExecutionBackend::ExactGpu;
    rejects<std::invalid_argument>([&] { plan.boundary(input, available, runtime::ExecutionLease(policy)); }, "unimplemented exact backend rejects");
    policy.fallback = runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback = plan.boundary(input, available, runtime::ExecutionLease(policy));
    check(fallback.provenance().fallback, "only declared CPU fallback");
    policy = {}; policy.limits.integer_bits = 1;
    rejects<std::length_error>([&] { plan.field(core::Grade{0}, {}, available, runtime::ExecutionLease(policy)); }, "unused source coefficient integer growth checked on empty field");
    policy = {}; policy.limits.scalar_work = 0;
    rejects<std::length_error>([&] { plan.field(core::Grade{0}, {}, available, runtime::ExecutionLease(policy)); }, "current host work cap");
    policy = {}; policy.limits.records = 0;
    rejects<runtime::ExecutionLimitExceeded>([&] { plan.field(core::Grade{0}, {}, available, runtime::ExecutionLease(policy)); }, "current full source record cap");
    policy = {}; policy.limits.rational_slots = plan.product().entry_envelope() - 1;
    rejects<runtime::ExecutionLimitExceeded>([&] { plan.field(core::Grade{0}, {}, available, runtime::ExecutionLease(policy)); }, "current full source scalar envelope");
    auto stop = std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&] { plan.boundary(input, available, runtime::ExecutionLease({}, stop)); }, "native cancellation owned by caller");
    std::vector<std::future<inference::DerivedNativeProductField>> jobs;
    for (unsigned i = 0; i < 6; ++i) jobs.push_back(std::async(std::launch::async, [plan, input] { return plan.boundary(input, available); }));
    for (auto& job : jobs) check(values(job.get()) == first.field().values(), "concurrent calls use independent scratch");
    auto left = factor_map(1, 1, 1, 0, 0); left.pop_back();
    const auto map = Map::prepare(plan, plan, left, left, {available, available});
    const auto image = map.execute(input, {available, available});
    check(image.chain_map()->source().same_owner(plan) && image.chain_map()->target().same_owner(plan), "native map retains both actual product owners");
    const auto later_image = map.execute(input, {future, available});
    check(later_image.map_execution_availability()->source.left.max_sequence == future.left.max_sequence &&
          later_image.map_execution_availability()->target.right.max_sequence == available.right.max_sequence &&
          later_image.provenance().preparation_steps == map.preparation_steps(), "both current map cutoffs and actual preparation receipt retained");
    rejects<std::invalid_argument>([&] { map.execute(twin.field(core::Grade{2}, {{top, 1}}, available), {available, available}); }, "native tensor map rejects foreign equal-valued field");
    stale = available; --stale.right.max_sequence;
    rejects<std::invalid_argument>([&] { map.execute(input, {available, stale}); }, "map target current cutoff checked independently");
    rejects<std::invalid_argument>([&] { Map::prepare(plan, plan, left, left, {stale, available}); }, "map preparation source cutoff checked independently");
    policy = {}; policy.limits.integer_bits = 1;
    rejects<std::length_error>([&] { map.execute(plan.field(core::Grade{2}, {{top, 0}}, available), {available, available}, runtime::ExecutionLease(policy)); }, "zero map action checks complete saved maps and sources");
    check(plan.boundary(image, available).input()->chain_map().has_value(), "boundary retains preceding complete map receipt");
    policy = {}; policy.limits.rational_slots = map.numeric_plan().entry_envelope() - 1;
    rejects<runtime::ExecutionLimitExceeded>([&] { map.execute(input, {available, available}, runtime::ExecutionLease(policy)); }, "whole saved map and both factor source container cap");
    auto native_limits = storage::NativeAccessionLimits{}; native_limits.max_action_work = 1;
    storage::NativeAccessionDirectory limited(storage::TemporalEndpoint(interval(), 7), 900, {},
        [](auto bytes) { return storage::rfcx_detail::fnv1a64(bytes); }, native_limits);
    rejects<std::length_error>([&] { Plan::prepare(request(limited, b), available); }, "original native work authority also bounds product preparation");
}
} // namespace
int main(int argc, char** argv) {
    try {
        const bool emit_records = argc == 2 && std::string_view(argv[1]) == "--emit";
        if (argc > 2 || (argc == 2 && !emit_records)) throw std::invalid_argument("usage: rfc_native_tensor_product_tests [--emit]");
        samples(emit_records);
        if (!emit_records) { numeric_contracts(); native_contracts(); std::cout << "Checks: " << checks << "; native tensor cases: " << cases << '\n'; }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
