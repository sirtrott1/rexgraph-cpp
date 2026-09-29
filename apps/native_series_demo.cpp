#include "rfc/storage/native_series_codec.hpp"
#include "rfc/storage/native_accession.hpp"
#include <filesystem>
#include <iostream>

using namespace rfc;
using namespace rfc::storage;
namespace {
TemporalEndpoint endpoint(std::uint64_t sequence, const numeric::RationalVector& x) {
    core::RelationSignature sig; const auto pair = sig.add_type("pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(sig); builder.declare_grade(core::Grade{2});
    for (std::size_t i = 0; i < x.size(); ++i)
        builder.add_grade_one(core::RelationKey{100 + i}, pair, {
            {10 + i, true, true, numeric::Rational{1}, {}, core::OccurrenceKey{1000 + 2 * i}},
            {11 + i, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001 + 2 * i}}});
    TemporalEndpoint e(builder.freeze(), sequence);
    e.carriers.push_back({1, TemporalCarrierKind::SourceGrade, core::Grade{1}, {}, 0, numeric::IndexDomain::linear(x.size())});
    TemporalTensor field; field.key = 10; field.role = TemporalTensorRole::Field; field.output = 1;
    std::vector<numeric::SparseRationalEntry> values;
    for (std::size_t i = 0; i < x.size(); ++i) if (x[i] != 0) values.push_back({i, x[i]});
    field.tensor = numeric::SparseRationalTensor(numeric::IndexDomain::linear(x.size()), numeric::StructuralSupport::full(x.size()),
                                                numeric::SparseRationalVector(x.size(), values));
    e.tensors.push_back(field);
    TemporalTensor metric; metric.key = 20; metric.role = TemporalTensorRole::Metric; metric.input = 1; metric.output = 1;
    metric.tensor = temporal_matrix_tensor(numeric::ExactMatrix::identity(x.size())); e.tensors.push_back(metric);
    return e;
}
void save(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); out.close();
    if (!out) throw std::runtime_error("series demonstration write failed");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: rfc_native_series_demo NEW_DIRECTORY");
        const std::filesystem::path dir(argv[1]);
        if (!std::filesystem::create_directory(dir)) throw std::runtime_error("use a fresh output directory");
        const auto a = endpoint(40, {1, 2}), b = endpoint(41, {3, 1, 6}), c = endpoint(42, {13, -7});
        const auto J0 = numeric::ExactMatrix::from_rows({{1, 0}, {0, 1}, {1, 1}});
        const auto J1 = numeric::ExactMatrix::from_rows({{1, 0, 1}, {0, 1, -1}});
        write_rfcx(a.source, dir / "base.rfcx"); write_temporal_endpoint(a, dir / "base.rfcxe");
        append_temporal_segment(dir / "history.rfcxt", a, make_temporal_segment(a, b, {{1, J0}}));
        append_temporal_segment(dir / "history.rfcxt", b, make_temporal_segment(b, c, {{1, J1}}));
        MappedRfcx mapped(dir / "base.rfcx"); auto seed = read_temporal_endpoint(mapped, dir / "base.rfcxe");
        NativeTemporalSeries series(seed, 700); const auto old = series.tail();
        ReadOnlyMappedFile history(dir / "history.rfcxt"); series.append_stream(history.bytes(), 7); series.checkpoint(41);
        save(dir / "history.rfcxs", encode_native_series(series));
        auto loaded = read_native_series(dir / "history.rfcxs");
        const auto requested = loaded.at(42), full = loaded.at(42, true);
        NativeAccessionDirectory views(requested.endpoint(), requested.source_namespace());
        const auto chain = views.pin(NativeAccessionRequest::chain(10, core::Grade{1}));
        const auto range = loaded.difference(40, 42);
        const auto moments = range.moments({{10, {}}}, 20);
        const auto samples = range.sample_moments({{40, 10, {}}, {42, 10, {}}}, 20);
        numeric::Rational innovation_moment = 0;
        for (std::size_t i = 1; i < 3; ++i) for (std::size_t j = 1; j < 3; ++j) innovation_moment += moments.gram.get(i, j);
        if (encode_temporal_endpoint(requested.endpoint()) != encode_temporal_endpoint(c) ||
            encode_temporal_endpoint(full.endpoint()) != encode_temporal_endpoint(c) ||
            old.endpoint().tensors[0].tensor.state(0) != numeric::Rational{1} ||
            innovation_moment != 117 || samples.gram.get(0, 1) != 59 || chain.state(100) != numeric::Rational{13})
            throw std::runtime_error("series exact reconstruction or moment mismatch");
        loaded.repack_checkpoints({42}); save(dir / "repacked.rfcxs", encode_native_series(loaded));
        std::cout << "endpoints=3 retained_frames=" << series.segment_count()
                  << " checkpoint_replays=" << requested.stats().replayed_segments
                  << " full_replays=" << full.stats().replayed_segments
                  << " innovation_moment=" << innovation_moment << " sampled_cross_moment=" << samples.gram.get(0, 1)
                  << " old_pin_preserved=true exact_disk_replay=true\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
