#include "../temporal_dynamics/common.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/field/prepared_hodge_frames.hpp"
#include "rfc/inference/native_hodge_frames.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include "rfc/statistics/ensemble.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include "rfc/storage/native_accession.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <memory>

namespace temporal_demo {
namespace {
using namespace rfc;
using Pin = storage::PinnedNativeAccession;
const std::vector<std::string> feature_names{
    "forcing", "reading value", "reading present", "start / 10", "end / 10",
    "duration / 10", "pump occurrences", "valve occurrences", "controller occurrences"};

Pin pin(const storage::NativeAccessionDirectory &directory, storage::NativeAccessionKind kind,
        std::uint64_t key) {
    storage::NativeAccessionRequest request;
    request.kind = kind;
    request.subject = key;
    return directory.pin(request);
}
Q fraction(const Q &a, const Q &b) {
    Q result = a / b;
    result.canonicalize();
    return result;
}
V column(const M &a, std::size_t j) {
    V result(a.rows());
    for (std::size_t i = 0; i < a.rows(); ++i)
        result[i] = a.get(i, j);
    return result;
}
storage::TemporalTensor field_tensor(std::uint64_t key, const V &value) {
    storage::TemporalTensor result;
    result.key = key;
    result.output = 10;
    result.role = storage::TemporalTensorRole::Field;
    std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < value.size(); ++i)
        if (value[i] != 0)
            entries.push_back({i, value[i]});
    result.tensor = {numeric::IndexDomain::linear(value.size()),
                     numeric::StructuralSupport::full(value.size()),
                     numeric::SparseRationalVector(value.size(), entries)};
    return result;
}
std::vector<std::string> cluster_fields(const M &centered, const M &metric,
                                       const std::filesystem::path &output, std::uint64_t sequence,
                                       std::vector<std::string> &edges, std::vector<std::string> &constant) {
    const auto gram = centered.transpose().multiply(metric).multiply(centered);
    core::RelationSignature signature;
    const auto type = signature.add_type("positive-field-affinity", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_grade(core::Grade{1});
    for (std::size_t j = 0; j < gram.rows(); ++j) {
        builder.declare_witness_identity(100 + j);
        if (gram.get(j, j) == 0)
            constant.push_back(feature_names[j]);
    }
    for (std::size_t i = 0; i < gram.rows(); ++i)
        for (std::size_t j = i + 1; j < gram.cols(); ++j) {
            const auto ij = gram.get(i, j), ii = gram.get(i, i), jj = gram.get(j, j);
            if (ii == 0 || jj == 0)
                continue;
            const auto affinity = fraction(Q{ij * ij}, Q{ii * jj});
            if (ij > 0 && affinity >= rational("3/4")) {
                builder.add_grade_one(type, {{100 + i, true, true, 1}, {100 + j, true, false, 1}});
                edges.push_back(object({{"a", quote(feature_names[i])}, {"b", quote(feature_names[j])},
                                        {"squared_cosine", quote(affinity.get_str())}}));
            }
        }
    const auto source = builder.freeze();
    storage::write_file_exact(output / ("field-groups-" + std::to_string(sequence) + ".rfcx"),
                             storage::encode_rfcx(source).bytes, storage::FilePublication::CreateNew);
    const auto owner = std::make_shared<const core::RelationalComplex>(source);
    const field::PreparedHodgeFrames plan(owner,
        {M::identity(gram.rows()), M::identity(source.grade(core::Grade{1}).cell_count())}, core::Grade{0});
    const auto &basis = plan.harmonic_frame();
    std::vector<std::string> groups;
    for (std::size_t j = 0; j < basis.cols(); ++j) {
        std::vector<std::string> members;
        for (std::size_t i = 0; i < basis.rows(); ++i) {
            require(basis.get(i, j) == 0 || basis.get(i, j) == 1, "pair-graph component frame is not an indicator");
            if (basis.get(i, j) != 0 && gram.get(i, i) != 0)
                members.push_back(feature_names[i]);
        }
        if (!members.empty())
            groups.push_back(strings(members));
    }
    return groups;
}

storage::TemporalEndpoint principal_endpoint(std::uint64_t sequence) {
    core::RelationalComplexBuilder builder(core::RelationSignature{});
    for (std::uint64_t key = 1; key <= 3; ++key)
        builder.declare_witness_identity(key);
    storage::TemporalEndpoint result{builder.freeze(), sequence};
    result.carriers.push_back({10, storage::TemporalCarrierKind::SourceGrade, core::Grade{0}, {}, 0,
                               numeric::IndexDomain::linear(3)});
    storage::TemporalTensor metric;
    metric.key = 20;
    metric.role = storage::TemporalTensorRole::Metric;
    metric.input = metric.output = 10;
    metric.tensor = storage::temporal_matrix_tensor(M::identity(3));
    result.tensors.push_back(std::move(metric));
    const std::array<std::size_t, 5> axes{0, 1, 2, 1, 2};
    const std::array<Q, 5> amplitudes{Q{2}, Q{1}, rational("1/2"), Q{3}, Q{4}};
    for (std::size_t pair = 0; pair < std::min<std::size_t>(5, sequence + 1); ++pair) {
        const auto amplitude = sequence >= 5 && pair == 0 ? Q{0} : amplitudes[pair];
        for (std::size_t side = 0; side < 2; ++side) {
            V value(3);
            value[axes[pair]] = side == 0 ? amplitude : Q{-amplitude};
            result.tensors.push_back(field_tensor(100 + 2 * pair + side, value));
        }
    }
    storage::validate_temporal_endpoint(result);
    return result;
}
void principal_demo(const std::vector<Frame> &frames, const std::filesystem::path &output) {
    constexpr std::uint64_t name_space = 97401;
    std::optional<storage::NativeTemporalSeries> history;
    std::optional<storage::TemporalEndpoint> previous;
    for (const auto &frame : frames) {
        const auto endpoint = principal_endpoint(frame.sequence);
        if (!history)
            history.emplace(endpoint, name_space);
        else
            history->append(storage::make_temporal_segment(*previous, endpoint));
        previous = endpoint;
        const auto selected = history->at(frame.sequence);
        storage::NativeAccessionDirectory directory(selected.endpoint(), name_space);
        M samples(3, selected.endpoint().tensors.size() - 1);
        std::vector<std::uint64_t> keys;
        for (const auto &tensor : selected.endpoint().tensors)
            if (tensor.role == storage::TemporalTensorRole::Field) {
                const auto field = pin(directory, storage::NativeAccessionKind::Field, tensor.key);
                const auto j = keys.size();
                keys.push_back(tensor.key);
                for (const auto &e : field.tensor()->values().entries())
                    samples.set(e.index, j, e.value);
            }
        V mean(3);
        for (std::size_t j = 0; j < samples.cols(); ++j)
            mean = numeric::add(mean, column(samples, j));
        require(mean == V(3), "symmetric reference samples are not centered");
        const auto covariance = samples.multiply(samples.transpose()).scale(fraction(1, Q{samples.cols()}));
        V variances(3);
        std::vector<std::uint64_t> order{0, 1, 2};
        for (std::size_t i = 0; i < 3; ++i) {
            variances[i] = covariance.get(i, i);
            V axis(3);
            axis[i] = 1;
            V expected(3);
            expected[i] = variances[i];
            require(covariance.apply(axis) == expected, "named reference direction is not principal");
        }
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return variances[a] > variances[b]; });
        const numeric::PreparedExactFrames reduction(samples);
        require(reduction.image().multiply(reduction.coordinates()) == samples,
                "principal reference lost sample directions");
        emit({{"chapter", quote("principal")}, {"sequence", std::to_string(frame.sequence)},
              {"source_namespace", std::to_string(name_space)}, {"carrier", "10"},
              {"channels", strings({"A", "B", "C"})}, {"sample_keys", numbers(keys)},
              {"samples", matrix(samples.transpose())}, {"covariance", matrix(covariance)},
              {"variances", vector(variances)}, {"principal_order", numbers(order)},
              {"rank", std::to_string(reduction.rank())},
              {"native_method", quote("named orthogonal channel certificate")},
              {"scope", quote("This constructed diagonal covariance has known principal axes. General covariance eigenvectors are not produced by exact frame extraction.")}});
    }
    storage::write_file_exact(output / "principal-history.rfcxs", storage::encode_native_series(*history),
                             storage::FilePublication::CreateNew);
}
} // namespace

void analysis_demo(const std::vector<Frame> &frames, const std::filesystem::path &output) {
    using namespace rfc;
    const auto analysis = output / "analysis";
    std::filesystem::create_directory(analysis);
    const auto history_bytes = storage::read_file_exact(output / "fields/history.rfcxs", 16u * 1024u * 1024u);
    const auto history = storage::decode_native_series(history_bytes);
    require(history.source_namespace() == field_namespace, "analysis selected an unrelated field history");
    for (const auto &frame : frames) {
        const auto selected = history.at(frame.sequence);
        storage::NativeAccessionDirectory directory(selected.endpoint(), field_namespace);
        const auto forcing = pin(directory, storage::NativeAccessionKind::Field, 2002);
        const auto metric = storage::temporal_tensor_matrix(*pin(directory, storage::NativeAccessionKind::Tensor, 51).tensor_declaration());
        const auto &arena = selected.endpoint().source.grade(core::Grade{1});
        std::vector<const Event *> events;
        std::vector<std::string> names, links;
        V durations;
        M data(arena.cell_count(), feature_names.size());
        for (std::size_t i = 0; i < arena.cell_count(); ++i) {
            const auto key = arena.relation_identities[i].value;
            const auto found = std::find_if(frame.events.begin(), frame.events.end(),
                                           [&](const Event &event) { return event.subtype != 7 && event.native_key == key; });
            require(found != frame.events.end(), "producer's declared database-to-span link is incomplete");
            require(forcing.tensor()->values().dense().at(i) == found->forcing, "linked native field disagrees with the stored payload");
            events.push_back(&*found);
            names.push_back(found->name);
            durations.push_back(Q{found->end - found->start});
            require(metric.get(i, i) == durations.back(), "native duration metric differs from the source span");
            links.push_back(object({{"database_namespace", std::to_string(database_namespace)},
                {"database_key", std::to_string(key)}, {"field_namespace", std::to_string(field_namespace)},
                {"field_grade", "1"}, {"field_key", std::to_string(key)}}));
            data.set(i, 0, found->forcing);
            data.set(i, 1, found->reading.value_or(Q{0}));
            data.set(i, 2, found->reading ? 1 : 0);
            data.set(i, 3, fraction(found->start, 10));
            data.set(i, 4, fraction(found->end, 10));
            data.set(i, 5, fraction(durations.back(), 10));
            for (std::size_t j = 0; j < 3; ++j) {
                const std::string asset = std::array<std::string, 3>{"pump", "valve", "controller"}[j];
                const auto count = std::count(found->assets.begin(), found->assets.end(), asset);
                data.set(i, 6 + j, Q{static_cast<unsigned long>(count)});
            }
        }
        Q duration_sum{0};
        for (const auto &duration : durations)
            duration_sum += duration;
        V mean(data.cols());
        M centered(data.rows(), data.cols());
        for (std::size_t j = 0; j < data.cols(); ++j) {
            Q numerator{0};
            for (std::size_t i = 0; i < data.rows(); ++i)
                numerator += durations[i] * data.get(i, j);
            mean[j] = fraction(numerator, duration_sum);
            for (std::size_t i = 0; i < data.rows(); ++i)
                centered.set(i, j, Q{data.get(i, j) - mean[j]});
        }
        const numeric::PreparedExactFrames reduction(centered);
        require(reduction.image().multiply(reduction.coordinates()) == centered,
                "independent native components failed exact reconstruction");
        std::vector<std::string> edges, constant;
        const auto groups = cluster_fields(centered, metric, analysis, frame.sequence, edges, constant);
        const auto event_gram = data.multiply(data.transpose());
        M distances(data.rows(), data.rows());
        for (std::size_t i = 0; i < data.rows(); ++i)
            for (std::size_t j = 0; j < data.rows(); ++j)
                distances.set(i, j, Q{event_gram.get(i, i) + event_gram.get(j, j) - 2 * event_gram.get(i, j)});
        std::vector<std::uint64_t> pivots;
        for (const auto index : reduction.pivots())
            pivots.push_back(index);
        emit({{"chapter", quote("native_vectors")}, {"sequence", std::to_string(frame.sequence)},
              {"source_namespace", std::to_string(field_namespace)}, {"carrier", "11"},
              {"event_names", strings(names)}, {"feature_names", strings(feature_names)},
              {"links", array(links)}, {"values", matrix(data)}, {"metric", matrix(metric)},
              {"weighted_mean", vector(mean)}, {"centered", matrix(centered)},
              {"rank", std::to_string(reduction.rank())}, {"frame", matrix(reduction.image())},
              {"pivot_columns", numbers(pivots)}, {"event_squared_distances", matrix(distances)},
              {"coefficients", matrix(reduction.coordinates())}, {"dependencies", matrix(reduction.kernel())},
              {"gram", matrix(centered.transpose().multiply(metric).multiply(centered))},
              {"groups", array(groups)}, {"group_edges", array(edges)}, {"constant_fields", strings(constant)},
              {"threshold_squared_cosine", quote("3/4")}, {"source_reconstruction_exact", "true"},
              {"numeric_view", quote("Fixed named columns; time values in ten-minute units. Missing reading is represented by a value column plus a separate presence column. The original optional payload and role occurrences remain stored.")}});

        V points;
        for (const auto *event : events) {
            points.push_back(event->start);
            points.push_back(event->end);
        }
        std::sort(points.begin(), points.end());
        points.erase(std::unique(points.begin(), points.end()), points.end());
        M activity(points.size() - 1, events.size());
        V lengths(points.size() - 1);
        for (std::size_t i = 0; i < lengths.size(); ++i) {
            lengths[i] = points[i + 1] - points[i];
            for (std::size_t j = 0; j < events.size(); ++j)
                if (events[j]->start <= points[i] && events[j]->end >= points[i + 1])
                    activity.set(i, j, 1);
        }
        const auto overlap = activity.transpose().multiply(M::diagonal(lengths)).multiply(activity);
        for (std::size_t i = 0; i < events.size(); ++i)
            for (std::size_t j = 0; j < events.size(); ++j) {
                const Q expected = std::max(Q{0}, Q{std::min(events[i]->end, events[j]->end) -
                                                        std::max(events[i]->start, events[j]->start)});
                require(overlap.get(i, j) == expected, "time-span pullback disagrees with exact overlap");
            }
        const numeric::PreparedExactFrames time_frames(activity);
        require(time_frames.image().multiply(time_frames.coordinates()) == activity, "time observation reconstruction");
        emit({{"chapter", quote("span_geometry")}, {"sequence", std::to_string(frame.sequence)},
              {"event_names", strings(names)}, {"points", vector(points)}, {"lengths", vector(lengths)},
              {"activity_map", matrix(activity)}, {"overlap", matrix(overlap)},
              {"observation_kernel", matrix(time_frames.kernel())},
              {"observation_rank", std::to_string(time_frames.rank())},
              {"scope", quote("Exact piecewise-constant activity observation at this cutoff. This pullback can have a kernel; it is not a substitute positive metric or an identity between distinct events.")}});

        std::vector<storage::NativeSeriesSampleRequest> requests;
        for (std::uint64_t sequence = 0; sequence <= frame.sequence; ++sequence)
            requests.push_back({sequence, 2002, {}});
        const auto sample = history.difference(0, frame.sequence).sample_moments(requests, 51);
        M sampled(data.rows(), requests.size());
        V sample_mean(data.rows());
        for (std::size_t j = 0; j < sample.channels.size(); ++j) {
            sample_mean = numeric::add(sample_mean, sample.channels[j].target_field);
            for (std::size_t i = 0; i < data.rows(); ++i)
                sampled.set(i, j, sample.channels[j].target_field[i]);
        }
        for (auto &value : sample_mean)
            value = fraction(value, Q{requests.size()});
        M fluctuations(sampled.rows(), sampled.cols());
        for (std::size_t i = 0; i < sampled.rows(); ++i)
            for (std::size_t j = 0; j < sampled.cols(); ++j)
                fluctuations.set(i, j, Q{sampled.get(i, j) - sample_mean[i]});
        const auto covariance = fluctuations.multiply(fluctuations.transpose()).scale(fraction(1, Q{requests.size()}));
        const auto gram = fluctuations.transpose().multiply(metric).multiply(fluctuations);
        const auto variance = statistics::trace_product(metric, covariance);
        Q gram_variance{0}, gram_second{0};
        for (std::size_t i = 0; i < gram.rows(); ++i)
            gram_variance += gram.get(i, i);
        for (const auto &[key, value] : gram.entries()) {
            (void)key;
            gram_second += value * value;
        }
        gram_variance = fraction(gram_variance, Q{requests.size()});
        gram_second = fraction(gram_second, Q{requests.size() * requests.size()});
        require(variance == gram_variance, "sample and coordinate variance disagree");
        const numeric::PreparedExactFrames fluctuations_frame(fluctuations);
        std::string effective = "null";
        if (variance != 0) {
            const auto moment = statistics::effective_dimension_2(metric, covariance);
            require(moment.second_contraction == gram_second && moment.dimension >= 1 &&
                    moment.dimension <= Q{fluctuations_frame.rank()}, "effective-dimension identity or rank bound");
            effective = quote(moment.dimension.get_str());
            M change = M::identity(metric.rows());
            if (change.rows() >= 2)
                change.set(0, 1, 1);
            const auto inverse = change.inverse();
            const auto changed_metric = change.transpose().multiply(metric).multiply(change);
            const auto changed_covariance = inverse.multiply(covariance).multiply(inverse.transpose());
            const auto changed = statistics::effective_dimension_2(changed_metric, changed_covariance);
            require(changed.variance == moment.variance && changed.second_contraction == moment.second_contraction &&
                    changed.dimension == moment.dimension, "complete change of frame changed ensemble moments");
        }
        emit({{"chapter", quote("historical_components")}, {"sequence", std::to_string(frame.sequence)},
              {"terminal_cutoff", std::to_string(sample.terminal_sequence)}, {"metric", "51"},
              {"event_names", strings(names)}, {"samples", matrix(sampled)}, {"mean", vector(sample_mean)},
              {"centered", matrix(fluctuations)}, {"covariance", matrix(covariance)},
              {"sample_gram", matrix(gram)}, {"variance", quote(variance.get_str())},
              {"second_contraction", quote(gram_second.get_str())}, {"effective_dimension", effective},
              {"rank", std::to_string(fluctuations_frame.rank())},
              {"frame", matrix(fluctuations_frame.image())}, {"coefficients", matrix(fluctuations_frame.coordinates())},
              {"scope", quote("Samples from publications zero through this cutoff, transported by the declared complete forward maps into this cutoff's span carrier. Zero extension on a new direction is a map value, not an earlier observation of that event.")}});
    }
    principal_demo(frames, analysis);
}
} // namespace temporal_demo
