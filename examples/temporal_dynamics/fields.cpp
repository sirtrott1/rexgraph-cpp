#include "common.hpp"
#include "rfc/backend/packed_binary.hpp"
#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/database/native_field_cache.hpp"
#include "rfc/inference/native_green_response.hpp"
#include "rfc/inference/native_hodge_frames.hpp"
#include "rfc/inference/native_linear_observation.hpp"
#include "rfc/storage/native_durable_store.hpp"
#include "rfc/temporal/multitime.hpp"
#include <chrono>
#include <map>

namespace temporal_demo {
namespace {
using namespace rfc;
using Pin = storage::PinnedNativeAccession;
using Clock = std::chrono::steady_clock;
std::uint64_t ns(Clock::time_point start) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}
Pin pin(const storage::NativeAccessionDirectory &d, storage::NativeAccessionKind kind, std::uint64_t key) {
    storage::NativeAccessionRequest request;
    request.kind = kind;
    request.subject = key;
    return d.pin(request);
}
storage::TemporalTensor
map_tensor(std::uint64_t key, std::uint64_t in, std::uint64_t out, const M &matrix,
           storage::TemporalTensorRole role = storage::TemporalTensorRole::SourceRealization) {
    storage::TemporalTensor t;
    t.key = key;
    t.input = in;
    t.output = out;
    t.role = role;
    t.tensor = storage::temporal_matrix_tensor(matrix);
    return t;
}
storage::TemporalTensor field_tensor(std::uint64_t key, std::uint64_t carrier, const V &values) {
    storage::TemporalTensor t;
    t.key = key;
    t.output = carrier;
    t.role = storage::TemporalTensorRole::Field;
    std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] != 0)
            entries.push_back({i, values[i]});
    t.tensor = {numeric::IndexDomain::linear(values.size()), numeric::StructuralSupport::full(values.size()),
                numeric::SparseRationalVector(values.size(), entries)};
    return t;
}
core::RelationalComplex span_source(const Frame &frame, std::map<Q, std::uint64_t> &times) {
    core::RelationSignature signature;
    std::vector<core::RelationTypeId> span_types;
    for (const auto *name :
         {"startup-span", "alarm-span", "baseline-span", "inspection-span", "repair-span", "episode-span"})
        span_types.push_back(signature.add_type(name, core::BoundaryPolicy::DeclaredExact, 0, 8));
    const auto closure =
        signature.add_type("declared-episode-closure", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_grade(core::Grade{2});
    const auto time_key = [&](const Q &q) {
        if (!times.contains(q))
            times.emplace(q, 100 + times.size());
        return times.at(q);
    };
    std::map<std::string, std::uint32_t> indices;
    std::uint32_t next{};
    const Event *close{};
    for (const auto &e : frame.events) {
        if (e.subtype == 7) {
            close = &e;
            continue;
        }
        const auto a = time_key(e.start), b = time_key(e.end);
        indices.emplace(e.name, next++);
        const auto occurrence = e.native_key * 16;
        builder.add_grade_one(core::RelationKey{e.native_key}, span_types.at(e.subtype - 1),
                              {{a, true, true, 1, {}, core::OccurrenceKey{occurrence + 1}},
                               {b, true, false, 1, {}, core::OccurrenceKey{occurrence + 2}},
                               {a, true, false, 2, {}, core::OccurrenceKey{occurrence + 3}},
                               {a, true, true, 2, {}, core::OccurrenceKey{occurrence + 4}},
                               {b, false, false, 7, {}, core::OccurrenceKey{occurrence + 5}},
                               {b, true, false, 0, {}, core::OccurrenceKey{occurrence + 6}}});
    }
    if (close) {
        for (const auto *name : {"startup", "alarm", "repair", "episode"})
            if (!indices.contains(name))
                throw std::invalid_argument("episode closure needs its four declared boundary relations");
        builder.add_higher(core::Grade{2}, core::RelationKey{close->native_key}, closure,
                           {{indices.at("startup"), true, false, 1},
                            {indices.at("alarm"), true, false, 1},
                            {indices.at("repair"), true, false, 1},
                            {indices.at("episode"), true, true, 1}});
    }
    const auto source = builder.freeze();
    const auto bianchi = core::bianchi_through(source, source.max_grade());
    for (const auto &check : bianchi.interfaces)
        require(check.satisfied, "a declared higher relation violates the chain condition");
    return source;
}
M correspondence(const core::RelationalComplex &old, const core::RelationalComplex &next, core::Grade grade) {
    const auto &a = old.grade(grade).relation_identities;
    const auto &b = next.grade(grade).relation_identities;
    M j(b.size(), a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto found = std::find(b.begin(), b.end(), a[i]);
        if (found != b.end())
            j.set(static_cast<std::size_t>(found - b.begin()), i, 1);
    }
    return j;
}
struct PreparedFrame {
    storage::TemporalEndpoint endpoint;
    std::vector<field::ExactMetric> metrics;
    std::vector<M> forms;
    V forcing;
    std::vector<std::string> names;
};
PreparedFrame prepare_frame(const Frame &frame, std::map<Q, std::uint64_t> &times) {
    storage::TemporalEndpoint endpoint(span_source(frame, times), frame.sequence);
    const auto &source = endpoint.source;
    std::map<std::uint64_t, const Event *> events;
    for (const auto &e : frame.events)
        events.emplace(e.native_key, &e);
    std::vector<field::ExactMetric> metrics;
    std::vector<M> forms;
    for (std::uint16_t k = 0; k < 3; ++k) {
        const auto grade = core::Grade{k};
        const auto n = source.grade(grade).cell_count();
        V diagonal(n, Q{1});
        if (k == 1)
            for (std::size_t i = 0; i < n; ++i) {
                const auto &e = *events.at(source.grade(grade).relation_identities.at(i).value);
                diagonal[i] = Q{e.end - e.start};
            }
        metrics.push_back(field::ExactMetric::diagonal(diagonal));
        M form(n, n);
        for (std::size_t i = 0; i < n; ++i)
            form.set(i, i, diagonal[i]);
        forms.push_back(form);
        endpoint.carriers.push_back({std::uint64_t{10} + k,
                                     storage::TemporalCarrierKind::SourceGrade,
                                     grade,
                                     {},
                                     0,
                                     numeric::IndexDomain::linear(n)});
        endpoint.tensors.push_back(
            map_tensor(50 + k, 10 + k, 10 + k, form, storage::TemporalTensorRole::Metric));
    }
    const auto n = source.grade(core::Grade{1}).cell_count();
    V forcing(n);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < n; ++i) {
        const auto &e = *events.at(source.grade(core::Grade{1}).relation_identities.at(i).value);
        forcing[i] = e.forcing;
        names.push_back(e.name);
    }
    endpoint.carriers.push_back({201,
                                 storage::TemporalCarrierKind::Explicit,
                                 core::Grade{0},
                                 {},
                                 97105,
                                 numeric::IndexDomain::linear(n)});
    endpoint.carriers.push_back({301,
                                 storage::TemporalCarrierKind::Explicit,
                                 core::Grade{0},
                                 {},
                                 97107,
                                 numeric::IndexDomain::linear(n)});
    // The small native operator interface explicitly retains the exact action
    // matrix. Its columns come from the library Hodge action, not a new solver.
    const auto l = field::ExactHodgeOperator(source, core::Grade{1}, metrics).materialize();
    endpoint.tensors.push_back(map_tensor(1001, 11, 11, l));
    endpoint.tensors.push_back(map_tensor(1002, 201, 11, M::identity(n)));
    endpoint.tensors.push_back(map_tensor(1003, 11, 11, M::identity(n).add(l)));
    endpoint.tensors.push_back(
        map_tensor(1004, 11, 301, M::identity(n), storage::TemporalTensorRole::Accession));
    endpoint.tensors.push_back(field_tensor(2001, 201, forcing));
    endpoint.tensors.push_back(field_tensor(2002, 11, forcing));
    for (std::uint16_t k = 0; k < 3; ++k)
        for (std::size_t i = 0; i < source.grade(core::Grade{k}).cell_count(); ++i) {
            const auto key = k == 0 ? source.grade(core::Grade{0}).identities.at(i)
                                    : source.grade(core::Grade{k}).relation_identities.at(i).value;
            const auto carrier = 10000 + 1000 * k + key;
            endpoint.carriers.push_back({carrier, storage::TemporalCarrierKind::CellStalk, core::Grade{k},
                                         storage::StableCellKey{core::Grade{k}, key}, 97106,
                                         numeric::IndexDomain::linear(2)});
            if (k == 1) {
                const auto &e = *events.at(key);
                auto t = field_tensor(3000 + key, carrier, {e.forcing, Q{e.end - e.start}});
                t.owner.kind = storage::TemporalOwnerKind::Cell;
                t.owner.cell = {core::Grade{1}, key};
                endpoint.tensors.push_back(std::move(t));
            }
        }
    storage::validate_temporal_endpoint(endpoint);
    return {std::move(endpoint), std::move(metrics), std::move(forms), std::move(forcing), std::move(names)};
}
void binary_observation(const core::RelationalComplex &source, std::uint64_t sequence) {
    const auto &arena = source.grade(core::Grade{1});
    composite::BinaryProgram program(4);
    program.append({composite::BinaryOpcode::LoadAspect, 0, 0, 0, 0, source.signature().existence_aspect()});
    program.append(
        {composite::BinaryOpcode::LoadAspect, 1, 0, 0, 0, source.signature().orientation_aspect()});
    program.append({composite::BinaryOpcode::And, 2, 0, 1});
    program.append({composite::BinaryOpcode::AndNot, 3, 0, 1});
    const backend::PreparedPackedBinary prepared(program, arena.aspects, {2, 3});
    auto result = prepared.empty_result(1u << 20);
    prepared.cpu_words(0, prepared.words(), result);
    std::size_t negative{}, positive{}, inactive{}, zero{};
    for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
        const auto exists = arena.aspects.get(source.signature().existence_aspect(), p);
        const auto orientation = arena.aspects.get(source.signature().orientation_aspect(), p);
        require(result[0].get(p) == (exists && orientation) && result[1].get(p) == (exists && !orientation),
                "packed occurrence masks changed the source aspects");
        negative += result[0].get(p);
        positive += result[1].get(p);
        inactive += !exists;
        zero += exists && source.incidence_coefficient(core::Grade{1}, p) == 0;
    }
    emit({{"chapter", quote("binary")},
          {"sequence", std::to_string(sequence)},
          {"occurrences", std::to_string(arena.incidence_count())},
          {"active_negative", std::to_string(negative)},
          {"active_positive", std::to_string(positive)},
          {"inactive_retained", std::to_string(inactive)},
          {"active_zero", std::to_string(zero)},
          {"instructions", std::to_string(program.instruction_count())},
          {"packed_words", std::to_string(prepared.words())},
          {"backend", quote("exact CPU packed words")}});
}
} // namespace

void field_demo(const std::vector<Frame> &frames, const std::filesystem::path &output) {
    std::filesystem::create_directory(output / "fields");
    std::map<Q, std::uint64_t> times;
    std::vector<PreparedFrame> prepared;
    std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> directories;
    std::vector<V> responses, innovations;
    std::vector<M> comparisons;
    database::NativeFieldCache cache;
    std::unique_ptr<storage::DurableNativeStore> store;
    std::optional<storage::NativeTemporalSeries> historical;
    for (const auto &frame : frames) {
        auto start = Clock::now();
        prepared.push_back(prepare_frame(frame, times));
        auto &f = prepared.back();
        const runtime::WorkAvailability available{field_namespace, frame.sequence};
        if (!store) {
            store.reset(new storage::DurableNativeStore(
                storage::DurableNativeStore::create(output / "field-store", f.endpoint, field_namespace)));
            historical = store->snapshot();
        } else {
            const auto &previous = prepared[prepared.size() - 2].endpoint;
            const auto j = correspondence(previous.source, f.endpoint.source, core::Grade{1});
            const auto segment = storage::make_temporal_segment(previous, f.endpoint, {{201, j}, {301, j}});
            const auto bytes = storage::encode_temporal_segment(segment);
            storage::write_file_exact(output / "fields" /
                                          ("transition-" + std::to_string(frame.sequence) + ".rfcxt"),
                                      bytes, storage::FilePublication::CreateNew);
            const std::vector<std::vector<std::byte>> batch{bytes};
            const auto receipt = store->append(100 + frame.sequence, store->root(), batch);
            require(receipt.root.sequence == frame.sequence, "native durable publication sequence differs");
        }
        const auto selected = store->snapshot().at(frame.sequence);
        directories.push_back(
            std::make_unique<storage::NativeAccessionDirectory>(selected.endpoint(), field_namespace));
        const auto &d = *directories.back();
        const auto op = pin(d, storage::NativeAccessionKind::Operator, 1003),
                   input = pin(d, storage::NativeAccessionKind::Field, 2002);
        const auto base =
            inference::PreparedNativeWoodburySolve::prepare_base(op, runtime::ExecutionLease{}, available);
        const auto solve = inference::PreparedNativeLinearObservation::prepare(
            base, {pin(d, storage::NativeAccessionKind::Operator, 1004)}, available);
        const auto prepare_ns = ns(start);
        start = Clock::now();
        const auto solved = solve.execute(input, available);
        const auto cold_ns = ns(start);
        start = Clock::now();
        const auto warm = solve.execute(input, available);
        const auto warm_ns = ns(start);
        require(solved.values() == warm.values(), "prepared Green reuse changed the exact response");
        responses.push_back(solved.values());
        const field::ExactHodgeOperator hodge(f.endpoint.source, core::Grade{1}, f.metrics);
        require(numeric::add(solved.values(), hodge.apply(solved.values())) == f.forcing,
                "exact native Green residual is nonzero");
        inference::NativeHodgeFrameRequest req;
        req.grade = core::Grade{1};
        req.metrics = inference::NativeMetricSelection::DeclaredDiagonal;
        for (std::uint64_t k = 0; k < 3; ++k) {
            req.grade_carriers.push_back(10 + k);
            req.metric_tensors.push_back(pin(d, storage::NativeAccessionKind::Tensor, 50 + k));
        }
        const auto sectors =
            inference::PreparedNativeHodgeFrames::prepare(input, req, available).execute(input, available);
        require(sectors.values().reconstructed == f.forcing,
                "native Hodge sectors do not reconstruct the forcing");
        const auto &source = f.endpoint.source;
        const auto boundary = field::BoundaryOperator(source, core::Grade{1}).apply(f.forcing);
        V time_coordinates;
        std::vector<std::string> time_links;
        for (const auto key : source.grade(core::Grade{0}).identities) {
            const auto found = std::find_if(times.begin(), times.end(),
                                            [&](const auto &item) { return item.second == key; });
            require(found != times.end(), "time coordinate lacks its declared identity");
            time_coordinates.push_back(found->first);
            const auto subject = std::find_if(frame.time_subjects.begin(), frame.time_subjects.end(),
                                              [&](const auto &item) { return item.first == found->first; });
            require(subject != frame.time_subjects.end(), "time point lacks its native database owner");
            time_links.push_back(object({{"minute", quote(found->first.get_str())},
                                         {"database_namespace", std::to_string(database_namespace)},
                                         {"database_subject_kind", "1"},
                                         {"database_key", std::to_string(subject->second)},
                                         {"field_namespace", std::to_string(field_namespace)},
                                         {"chain_grade", "0"},
                                         {"chain_key", std::to_string(key)}}));
        }
        std::vector<std::string> span_links;
        for (std::size_t i = 0; i < f.names.size(); ++i)
            span_links.push_back(object(
                {{"event", quote(f.names[i])},
                 {"relation_type",
                  quote(source.signature().type(source.grade(core::Grade{1}).relation_types.at(i)).name)},
                 {"database_namespace", std::to_string(database_namespace)},
                 {"database_kind", "10"},
                 {"database_key",
                  std::to_string(source.grade(core::Grade{1}).relation_identities.at(i).value)},
                 {"field_namespace", std::to_string(field_namespace)},
                 {"chain_grade", "1"},
                 {"chain_key",
                  std::to_string(source.grade(core::Grade{1}).relation_identities.at(i).value)}}));
        emit({{"chapter", quote("field")},
              {"sequence", std::to_string(frame.sequence)},
              {"namespace", std::to_string(field_namespace)},
              {"relations", strings(f.names)},
              {"declared_record_links", array(span_links)},
              {"time_coordinates", vector(time_coordinates)},
              {"declared_time_links", array(time_links)},
              {"grade_dimensions",
               numbers({source.grade(core::Grade{0}).cell_count(), source.grade(core::Grade{1}).cell_count(),
                        source.grade(core::Grade{2}).cell_count()})},
              {"boundary_matrix", matrix(field::materialize_boundary_matrix(source, core::Grade{1}))},
              {"higher_boundary", matrix(field::materialize_boundary_matrix(source, core::Grade{2}))},
              {"forcing", vector(f.forcing)},
              {"boundary_forcing", vector(boundary)},
              {"response", vector(solved.values())},
              {"gradient", vector(sectors.values().gradient)},
              {"curl", vector(sectors.values().curl)},
              {"harmonic", vector(sectors.values().harmonic)},
              {"sector_dimensions",
               numbers({sectors.provenance().gradient_dimension, sectors.provenance().curl_dimension,
                        sectors.provenance().harmonic_dimension})},
              {"metric", matrix(f.forms[1])},
              {"hodge_operator", matrix(hodge.materialize())},
              {"response_quadrance", quote(f.metrics[1].inner(solved.values(), solved.values()).get_str())},
              {"prepare_ns", std::to_string(prepare_ns)},
              {"cold_action_ns", std::to_string(cold_ns)},
              {"warm_action_ns", std::to_string(warm_ns)},
              {"chain_interfaces_checked", std::to_string(sectors.provenance().chain_interfaces_checked)},
              {"residual_exact_zero", "true"}});
        binary_observation(source, frame.sequence);
        storage::NativeFieldRequest fiber_req;
        fiber_req.upper_grade = core::Grade{1};
        for (const auto &c : f.endpoint.carriers)
            if (c.kind == storage::TemporalCarrierKind::CellStalk)
                fiber_req.stalk_carriers.push_back(c.key);
        for (const auto key : source.grade(core::Grade{1}).relation_identities)
            fiber_req.fields.push_back(pin(d, storage::NativeAccessionKind::Field, 3000 + key.value));
        const auto analysis = database::NativeFieldAnalysis::inspect(
            d.pin(storage::NativeAccessionRequest::sparse_chain(core::Grade{1}, {}, {})), fiber_req,
            storage::NativeFieldRealization::constant_fiber(97106));
        const auto first = cache.evaluate(analysis), second = cache.evaluate(analysis);
        require(first.values() == second.values() && second.reuse().result,
                "native stalk result reuse failed");
        const auto stats = cache.stats();
        emit({{"chapter", quote("fibers")},
              {"sequence", std::to_string(frame.sequence)},
              {"fiber_coordinates", strings({"forcing", "duration"})},
              {"boundary_values", vector(second.values())},
              {"compiled_plans", std::to_string(stats.compiled_plans)},
              {"applications", std::to_string(stats.applications)},
              {"result_hits", std::to_string(stats.result_hits)},
              {"source_sequence", std::to_string(second.provenance().sequence)},
              {"warm_result_hit", boolean(second.reuse().result)}});
        if (frame.sequence) {
            const auto &old = *directories[directories.size() - 2];
            const auto old_l = pin(old, storage::NativeAccessionKind::Operator, 1001),
                       new_l = pin(d, storage::NativeAccessionKind::Operator, 1001);
            const auto j =
                correspondence(prepared[prepared.size() - 2].endpoint.source, source, core::Grade{1});
            comparisons.push_back(j);
            const inference::NativeGreenAvailability at{{field_namespace, frame.sequence - 1}, available};
            const inference::NativeGreenResponseRequest request{
                old_l,
                new_l,
                pin(old, storage::NativeAccessionKind::Operator, 1002),
                pin(d, storage::NativeAccessionKind::Operator, 1002),
                {old_l, new_l, 11, 11, storage::temporal_matrix_tensor(j)},
                {old_l, new_l, 201, 201, storage::temporal_matrix_tensor(j)},
                1};
            const auto delta = inference::PreparedNativeGreenResponseDelta::prepare(request, at)
                                   .execute(pin(old, storage::NativeAccessionKind::Field, 2001),
                                            pin(d, storage::NativeAccessionKind::Field, 2001), at);
            const auto &values = delta.values();
            require(values.new_response == responses.back() &&
                        values.old_response == responses[responses.size() - 2],
                    "interval Green response differs from retained endpoint solves");
            innovations.push_back(values.aggregate);
            std::vector<temporal::MultiTimeChannel> channels{{"realization", values.realization_term},
                                                             {"innovation", values.innovation_term},
                                                             {"operator", values.operator_term}};
            const auto gram = temporal::multi_time_channel_gram(channels, f.metrics[1]);
            Q total{};
            for (const auto &[key, value] : gram.entries()) {
                (void)key;
                total += value;
            }
            require(total == f.metrics[1].inner(values.aggregate, values.aggregate),
                    "interval pairing contraction omitted interactions");
            const auto elapsed =
                Q{std::to_string(frame.available_minute - frames[frame.sequence - 1].available_minute)};
            V rate = values.aggregate;
            for (auto &q : rate)
                q /= elapsed;
            emit({{"chapter", quote("transition")},
                  {"old_sequence", std::to_string(frame.sequence - 1)},
                  {"sequence", std::to_string(frame.sequence)},
                  {"correspondence", matrix(j)},
                  {"realization", vector(values.realization_term)},
                  {"innovation", vector(values.innovation_term)},
                  {"operator", vector(values.operator_term)},
                  {"delta", vector(values.aggregate)},
                  {"channel_gram", matrix(gram)},
                  {"quadrance", quote(total.get_str())},
                  {"availability_elapsed_minutes", quote(elapsed.get_str())},
                  {"rate", vector(rate)},
                  {"retained_old_cutoff", std::to_string(delta.provenance().old_endpoint.sequence)},
                  {"retained_new_cutoff", std::to_string(delta.provenance().new_endpoint.sequence)}});
        }
        const auto accumulated =
            temporal::accumulate_innovations(responses.front(), comparisons, innovations);
        require(accumulated.terminal == responses.back(), "chronological innovation reconstruction failed");
        std::vector<temporal::MultiTimeChannel> channels{{"initial", accumulated.transported_initial}};
        for (std::size_t i = 0; i < accumulated.propagated_innovations.size(); ++i)
            channels.push_back(
                {"transition/" + std::to_string(i + 1), accumulated.propagated_innovations[i]});
        const auto gram = temporal::multi_time_channel_gram(channels, f.metrics[1]);
        Q total{};
        for (const auto &[key, value] : gram.entries()) {
            (void)key;
            total += value;
        }
        require(total == f.metrics[1].inner(responses.back(), responses.back()),
                "multi-time moment omitted interactions");
        emit({{"chapter", quote("accumulation")},
              {"sequence", std::to_string(frame.sequence)},
              {"terminal_cutoff", std::to_string(frame.sequence)},
              {"initial_transported", vector(accumulated.transported_initial)},
              {"innovation_channels", std::to_string(accumulated.propagated_innovations.size())},
              {"terminal", vector(accumulated.terminal)},
              {"gram", matrix(gram)},
              {"quadrance", quote(total.get_str())}});
    }
    const auto retained = store->snapshot();
    storage::write_file_exact(output / "fields/history.rfcxs", storage::encode_native_series(retained),
                              storage::FilePublication::CreateNew);
    store.reset();
    const auto reopened = storage::DurableNativeStore::open(output / "field-store");
    require(reopened.snapshot().same_history(retained), "native durable history differs after reopening");
    const auto restored = storage::read_native_series(output / "fields/history.rfcxs");
    require(restored.same_history(retained), "portable native history differs");
    const auto old_input = pin(*directories.front(), storage::NativeAccessionKind::Field, 2002);
    const auto old_base = inference::PreparedNativeWoodburySolve::prepare_base(
        pin(*directories.front(), storage::NativeAccessionKind::Operator, 1003), runtime::ExecutionLease{},
        {runtime::WorkAvailability{field_namespace, 0}});
    const auto old_plan = inference::PreparedNativeLinearObservation::prepare(
        old_base, {pin(*directories.front(), storage::NativeAccessionKind::Operator, 1004)},
        {field_namespace, 0});
    require(old_plan.execute(old_input, {field_namespace, 0}).values() == responses.front(),
            "later native publication changed an old field action");
    bool future_rejected = false, prefix_rejected = false;
    if (frames.size() > 1)
        try {
            (void)old_plan.execute(pin(*directories.back(), storage::NativeAccessionKind::Field, 2002),
                                   {field_namespace, 0});
        } catch (const std::invalid_argument &) {
            future_rejected = true;
        }
    else
        future_rejected = true;
    try {
        (void)historical->at(1);
    } catch (const std::invalid_argument &) {
        prefix_rejected = true;
    }
    require(future_rejected && prefix_rejected, "native field history accepted future data");
    emit({{"chapter", quote("isolation")},
          {"domain", quote("fields")},
          {"historical_sequence", "0"},
          {"historical_response", vector(responses.front())},
          {"future_rejected", boolean(future_rejected)},
          {"old_history_tail", std::to_string(historical->last_sequence())},
          {"future_endpoint_absent", boolean(prefix_rejected)},
          {"durable_tail", std::to_string(restored.last_sequence())},
          {"reopen_exact", "true"}});
}
} // namespace temporal_demo
