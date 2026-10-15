#include "common.hpp"
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query_algebra.hpp"
#include "rfc/database/native_tensor_window.hpp"
#include "rfc/database/prepared_native_query.hpp"
#include <map>
#include <set>

namespace temporal_demo {
namespace {
using namespace rfc;
using namespace rfc::database;
using D = composite::Declaration;
using Value = composite::DeclaredValue;
composite::DeclarationId id(std::uint64_t key) {
    return {97103, key, 1};
}
InterpretationSchema schema() {
    InterpretationSchema s;
    s.source_namespace = database_namespace;
    s.binding_namespace = 97104;
    s.values = composite::DeclarationRegistry(
        {D::rational(id(1)), D::optional(id(2), id(1)), D::finite(id(3), {1, 2, 3}),
         D::finite(id(4), {1, 2, 3, 4, 5, 6, 7}), D::rational_tensor(id(5), {{1, 2, {}}, {2, 3, {}}}),
         D::repeated(id(6), id(4), 0, 8),
         D::product(id(7), {{1, id(1), 0},
                            {2, id(1), 0},
                            {3, id(1), 0},
                            {4, id(2), 0},
                            {5, id(3), 0},
                            {6, id(4), 0},
                            {7, id(5), 0},
                            {8, id(6), 0}})});
    s.subjects = {{1, "TimePoint", "minute", "rational"},
                  {2, "Asset", "asset", "token"},
                  {3, "Operator", "operator", "token"}};
    s.assertions = {{10,
                     "TimeSpan",
                     id(7),
                     {{11, 1, 1, 1, false, false},
                      {12, 1, 1, 1, false, false},
                      {13, 2, 1, 8, false, false},
                      {14, 3, 0, 1, true, true}},
                     {{1, "start"},
                      {2, "end"},
                      {3, "forcing"},
                      {4, "reading"},
                      {5, "category"},
                      {6, "subtype"},
                      {7, "channels"},
                      {8, "tags"}},
                     "event",
                     "producer token",
                     {{11, "start_time"}, {12, "end_time"}, {13, "assets"}, {14, "operator"}}}};
    return s;
}
Value payload(const Event &e) {
    std::vector<numeric::SparseRationalEntry> entries;
    if (e.forcing != 0)
        entries.push_back({0, e.forcing});
    if (e.reading && *e.reading != 0)
        entries.push_back({5, *e.reading});
    std::vector<std::size_t> support{0, 1};
    if (e.reading)
        support.push_back(5);
    const numeric::SparseRationalTensor tensor(numeric::IndexDomain({2, 3}),
                                               numeric::StructuralSupport::sparse(6, support),
                                               numeric::SparseRationalVector(6, entries));
    return Value::product({Value::rational(e.start), Value::rational(e.end), Value::rational(e.forcing),
                           e.reading ? Value::present(Value::rational(*e.reading)) : Value::absent(),
                           Value::symbol(e.category), Value::symbol(e.subtype), Value::tensor(tensor),
                           Value::repeated({Value::symbol(e.subtype), Value::symbol(e.subtype)})});
}
LogicalSnapshot logical(const Frame &frame) {
    LogicalSnapshot out;
    out.revision = "cooling/publication/" + std::to_string(frame.sequence);
    std::set<ExternalOrigin> subjects{{2, "pump"}, {2, "valve"}, {2, "controller"}};
    for (const auto &e : frame.events) {
        subjects.insert({1, e.start.get_str()});
        subjects.insert({1, e.end.get_str()});
        std::vector<ExternalRoleValue> roles{
            {11, ReferenceState::Present, {{"start", {1, e.start.get_str()}, true}}},
            {12, ReferenceState::Present, {{"end", {1, e.end.get_str()}, true}}},
            {13, ReferenceState::Present, {}},
            {14, ReferenceState::Missing, {}}};
        for (std::size_t i = 0; i < e.assets.size(); ++i) {
            const auto &name = e.assets[i];
            const bool active = !name.starts_with('!');
            const auto asset = active ? name : name.substr(1);
            subjects.insert({2, asset});
            roles[2].occurrences.push_back({"asset/" + std::to_string(i), {2, asset}, active});
        }
        if (e.name == "alarm")
            roles[3].state = ReferenceState::Null;
        else if (e.name != "startup") {
            subjects.insert({3, "engineer"});
            roles[3] = {14, ReferenceState::Present, {{"operator", {3, "engineer"}, true}}};
        }
        out.assertions.push_back({{10, e.name}, payload(e), std::move(roles)});
    }
    out.subjects.assign(subjects.begin(), subjects.end());
    return out;
}
NativeQueryRequest overlap_request(const Q &start, const Q &end) {
    return {{{{1}, ExactPredicateKind::Less, Value::rational(end)},
             {{2}, ExactPredicateKind::Greater, Value::rational(start)}},
            std::vector<std::uint64_t>{4},
            NativeQueryOutput::Keys};
}
std::vector<std::string> names(const InterpretedSnapshot &source, const std::vector<std::uint64_t> &keys) {
    std::map<std::uint64_t, std::string> lookup;
    for (const auto &x : source.lineage().assertions)
        lookup.emplace(x.native_key, x.origin.token);
    std::vector<std::string> out;
    for (auto k : keys)
        out.push_back(lookup.at(k));
    return out;
}
void query_record(const InterpretedSnapshot &source, runtime::WorkAvailability available, const Q &start,
                  const Q &end, NativeQueryCache &cache, runtime::CpuTileExecutor &cpu) {
    const auto plan =
        PreparedNativeQuery::prepare(source, 10, overlap_request(start, end), available,
                                     runtime::ExecutionLease{}, 2, NativeQueryIndex::NumericRanges);
    const auto cold = cache.execute(plan, available, cpu), warm = cache.execute(plan, available, cpu);
    require(cold.keys() == warm.keys() && warm.cache_hit(), "repeat native query was not an exact cache hit");
    std::vector<NativeQueryRow> rows;
    for (const auto key : warm.keys())
        rows.push_back({key, 0, rows.size()});
    const auto grouped = group_native_rows(
        source, 10, nullptr, 0, std::move(rows),
        {{NativeQuerySide::Left, {{NativeValueStepKind::Factor, 5}}}},
        {{NativeAggregateKind::Count, {}, false, true},
         {NativeAggregateKind::Sum,
          {NativeQuerySide::Left, {{NativeValueStepKind::Factor, 4}, {NativeValueStepKind::Optional, 0}}},
          true,
          true}},
        available);
    std::vector<std::string> groups;
    for (const auto &g : grouped.groups) {
        std::vector<std::uint64_t> members;
        for (const auto i : g.members)
            members.push_back(grouped.rows.at(i).left);
        groups.push_back(object(
            {{"category", std::to_string(std::get<Value::Symbol>(g.values.at(0).value()->data).key)},
             {"member_keys", numbers(members)},
             {"member_events", strings(names(source, members))},
             {"count", quote(g.aggregates.at(0).value->get_str())},
             {"reading_sum", g.aggregates.at(1).value ? quote(g.aggregates.at(1).value->get_str()) : "null"},
             {"numeric_contributions", std::to_string(g.aggregates.at(1).supported)}}));
    }
    emit({{"chapter", quote("query")},
          {"sequence", std::to_string(source.sequence())},
          {"namespace", std::to_string(database_namespace)},
          {"interval", vector({start, end})},
          {"keys", numbers(warm.keys())},
          {"events", strings(names(source, warm.keys()))},
          {"selected", std::to_string(warm.selected_count())},
          {"reading_sum", warm.sum() ? quote(warm.sum()->get_str()) : "null"},
          {"numeric_contributions", std::to_string(warm.numeric_contributions())},
          {"cache_hit", boolean(warm.cache_hit())},
          {"cutoff", std::to_string(available.max_sequence)},
          {"source_sequence", std::to_string(warm.source_publication()->sequence)},
          {"groups", array(groups)}});
}
} // namespace

std::vector<Event> read_events(const std::filesystem::path &file) {
    std::ifstream in(file);
    if (!in)
        throw std::invalid_argument("event input cannot be opened");
    std::string line;
    std::getline(in, line);
    if (line !=
        "publication\tavailable_minute\tevent\tcategory\tsubtype\tstart\tend\tforcing\treading\tassets")
        throw std::invalid_argument("event input header differs from the declared schema");
    std::vector<Event> out;
    std::uint64_t previous{}, minute{};
    while (std::getline(in, line)) {
        if (line.size() > 4096 || out.size() >= 1000)
            throw std::length_error("event input limit");
        auto row = split(line, '\t');
        if (row.size() != 10)
            throw std::invalid_argument("event input needs ten tab separated fields");
        Event e;
        e.publication = integer(row[0]);
        e.available_minute = integer(row[1]);
        e.name = row[2];
        e.category = integer(row[3]);
        e.subtype = integer(row[4]);
        e.start = rational(row[5]);
        e.end = rational(row[6]);
        e.forcing = rational(row[7]);
        if (row[8] != "-")
            e.reading = rational(row[8]);
        e.assets = split(row[9], '|');
        if (e.name.empty() || e.name.size() > 64 || e.assets.empty() || e.assets.size() > 8 || e.start < 0 ||
            e.start >= e.end || e.end > Q{std::to_string(e.available_minute)} || e.category < 1 ||
            e.category > 3 || e.subtype < 1 || e.subtype > 7 || e.publication > 5)
            throw std::invalid_argument("event lies outside the declared finite scenario");
        if (out.empty()) {
            if (e.publication != 0)
                throw std::invalid_argument("first publication must be zero");
        } else if (e.publication < previous || e.publication > previous + 1 ||
                   (e.publication == previous && e.available_minute != minute) ||
                   (e.publication > previous && e.available_minute <= minute))
            throw std::invalid_argument("publication order or availability clock is invalid");
        previous = e.publication;
        minute = e.available_minute;
        out.push_back(std::move(e));
    }
    if (out.empty())
        throw std::invalid_argument("event input is empty");
    return out;
}

std::vector<Frame> database_demo(const std::vector<Event> &events, const std::filesystem::path &output) {
    std::filesystem::create_directory(output / "database");
    std::vector<Frame> frames;
    std::vector<InterpretedSnapshot> snapshots;
    std::vector<std::vector<std::byte>> frozen;
    std::map<std::string, Event> latest;
    runtime::CpuTileExecutor cpu({2, 2});
    NativeQueryCache cache(1u << 20);
    for (std::uint64_t sequence = 0; sequence <= events.back().publication; ++sequence) {
        Frame f;
        f.sequence = sequence;
        std::set<std::string> updates;
        for (const auto &e : events)
            if (e.publication == sequence) {
                require(updates.insert(e.name).second, "duplicate producer identity in one publication");
                latest[e.name] = e;
                f.available_minute = e.available_minute;
            }
        for (const auto &[name, e] : latest) {
            (void)name;
            f.events.push_back(e);
        }
        const auto input = logical(f);
        snapshots.push_back(snapshots.empty() ? InterpretedSnapshot::interpret(schema(), input)
                                              : snapshots.back().replace(input));
        const auto &source = snapshots.back();
        const runtime::WorkAvailability available{database_namespace, sequence};
        const auto bytes = encode_interpreted_snapshot(source);
        frozen.push_back(bytes);
        write_interpreted_snapshot(source, output / "database" /
                                               ("publication-" + std::to_string(sequence) + ".rfcxi"));
        if (sequence) {
            const auto update = snapshots[sequence - 1].difference(source);
            const auto update_bytes = encode_interpretation_update(update);
            storage::write_file_exact(output / "database" /
                                          ("transition-" + std::to_string(sequence) + ".rfcxiu"),
                                      update_bytes, storage::FilePublication::CreateNew);
            require(encode_interpreted_snapshot(
                        snapshots[sequence - 1].apply(decode_interpretation_update(update_bytes))) == bytes,
                    "native transition replay differs");
        }
        std::vector<std::string> rows;
        for (auto &e : f.events) {
            const auto b =
                std::find_if(source.lineage().assertions.begin(), source.lineage().assertions.end(),
                             [&](const auto &x) { return x.origin.token == e.name; });
            require(b != source.lineage().assertions.end(), "producer identity has no native binding");
            e.native_key = b->native_key;
            // Subsequent field views consume the retained native payload.
            e.start = std::get<Q>(source.payload(e.native_key, {1}).declared_value()->data);
            e.end = std::get<Q>(source.payload(e.native_key, {2}).declared_value()->data);
            e.forcing = std::get<Q>(source.payload(e.native_key, {3}).declared_value()->data);
            e.category =
                std::get<Value::Symbol>(source.payload(e.native_key, {5}).declared_value()->data).key;
            e.subtype = std::get<Value::Symbol>(source.payload(e.native_key, {6}).declared_value()->data).key;
            const auto reading_pin = source.payload(e.native_key, {4});
            const auto &reading = std::get<Value::Optional>(reading_pin.declared_value()->data);
            e.reading =
                reading.element.empty() ? std::optional<Q>{} : std::get<Q>(reading.element.at(0).data);
            const auto role = source.participants(e.native_key, 13),
                       active = source.participants(e.native_key, 13, true);
            const auto window =
                NativeTensorWindow::prepare(source.payload(e.native_key, {7}), {0, 0}, {2, 3}, available);
            require(window.supported_count() == (e.reading ? 3u : 2u),
                    "declared tensor support was flattened");
            rows.push_back(object({{"event", quote(e.name)},
                                   {"key", std::to_string(e.native_key)},
                                   {"first_known_or_corrected", std::to_string(e.publication)},
                                   {"start", quote(e.start.get_str())},
                                   {"end", quote(e.end.get_str())},
                                   {"category", std::to_string(e.category)},
                                   {"subtype", std::to_string(e.subtype)},
                                   {"forcing", quote(e.forcing.get_str())},
                                   {"reading", e.reading ? quote(e.reading->get_str()) : "null"},
                                   {"assets", strings(e.assets)},
                                   {"retained_asset_occurrences", std::to_string(role.size())},
                                   {"active_asset_occurrences", std::to_string(active.size())},
                                   {"operator_state", std::to_string(static_cast<unsigned>(
                                                          source.reference_state(e.native_key, 14)))},
                                   {"tensor_support", std::to_string(window.supported_count())},
                                   {"tensor_nonzeros", std::to_string(window.nonzero_count())}}));
        }
        const auto retained =
            PreparedNativeRoleJoin::prepare(source, {10, 13, 10, 13, NativeRoleActivity::Retained}, available)
                .execute(available);
        const auto active = PreparedNativeRoleJoin::prepare(
                                source, {10, 13, 10, 13, NativeRoleActivity::ActiveOnly}, available)
                                .execute(available);
        std::vector<std::string> time_points;
        for (const auto &subject : source.lineage().subjects)
            if (subject.origin.kind == 1) {
                time_points.push_back(object({{"minute", quote(subject.origin.token)},
                                              {"subject_key", std::to_string(subject.native_key)}}));
                f.time_subjects.push_back({rational(subject.origin.token), subject.native_key});
            }
        emit({{"chapter", quote("database")},
              {"sequence", std::to_string(sequence)},
              {"namespace", std::to_string(database_namespace)},
              {"available_minute", std::to_string(f.available_minute)},
              {"revision", quote(source.revision())},
              {"events", array(rows)},
              {"time_points", array(time_points)},
              {"retained_join_matches", std::to_string(retained.matches().size())},
              {"active_join_matches", std::to_string(active.matches().size())},
              {"snapshot", quote("database/publication-" + std::to_string(sequence) + ".rfcxi")}});
        query_record(source, available, 5, 15, cache, cpu);
        frames.push_back(std::move(f));
    }
    // Build the newer cache first, then return to the actual historical owner.
    const auto old_plan =
        PreparedNativeQuery::prepare(snapshots.front(), 10, overlap_request(5, 15), {database_namespace, 0});
    const auto historical = cache.execute(old_plan, {database_namespace, 0}, cpu);
    require(historical.source_publication() == snapshots.front().endpoint() &&
                historical.selected_count() == 1 && historical.sum() == Q{1},
            "later query cache leaked into the old publication");
    for (std::size_t i = 0; i < snapshots.size(); ++i)
        require(encode_interpreted_snapshot(snapshots[i]) == frozen[i],
                "later publication mutated an older native file");
    bool future_rejected = false, foreign_rejected = false;
    if (snapshots.size() > 1)
        try {
            (void)PreparedNativeQuery::prepare(snapshots.back(), 10, overlap_request(5, 15),
                                               {database_namespace, 0});
        } catch (const std::invalid_argument &) {
            future_rejected = true;
        }
    else
        future_rejected = true;
    try {
        (void)old_plan.execute({database_namespace + 100, 0}, cpu);
    } catch (const std::invalid_argument &) {
        foreign_rejected = true;
    }
    require(future_rejected && foreign_rejected, "query availability authority was bypassed");
    emit({{"chapter", quote("isolation")},
          {"domain", quote("database")},
          {"historical_sequence", "0"},
          {"historical_sum", quote(historical.sum()->get_str())},
          {"historical_events", strings(names(snapshots.front(), historical.keys()))},
          {"future_rejected", boolean(future_rejected)},
          {"foreign_namespace_rejected", boolean(foreign_rejected)},
          {"older_native_bytes_unchanged", "true"}});
    return frames;
}

void query_file(const std::filesystem::path &file, std::uint64_t cutoff, const Q &start, const Q &end) {
    if (start >= end)
        throw std::invalid_argument("query interval must be nonempty");
    const auto source = rfc::database::read_interpreted_snapshot(file);
    if (source.schema().source_namespace != database_namespace)
        throw std::invalid_argument("query file belongs to a different source namespace");
    rfc::database::NativeQueryCache cache(1u << 20);
    rfc::runtime::CpuTileExecutor cpu({1, 1});
    query_record(source, {database_namespace, cutoff}, start, end, cache, cpu);
}
} // namespace temporal_demo
