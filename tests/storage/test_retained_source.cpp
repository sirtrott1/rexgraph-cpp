#include <rfc/storage/native_series_codec.hpp>
#include <cstdlib>
#include <iostream>
#include <random>
using namespace rfc;
namespace {
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F&& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "invalid retained source accepted");
}
core::RelationalComplex reference(const storage::MappedRfcx& image) {
        core::RelationalComplexBuilder builder(image.signature(), image.semantic_scale());
        builder.declare_grade(core::Grade{static_cast<std::uint16_t>(image.grade_count() - 1u)});
        const auto g0 = image.grade(core::Grade{0});
        for (std::size_t i=0; i<g0.cell_count(); ++i) builder.declare_witness_identity(g0.identity(i));
        const auto exists = image.signature().existence_aspect();
        const auto orientation = image.signature().orientation_aspect();
        for (std::size_t gi=1; gi<image.grade_count(); ++gi) {
            const auto g = image.grade(core::Grade{static_cast<std::uint16_t>(gi)});
            for (std::size_t cell=0; cell<g.cell_count(); ++cell) {
                const auto [b,e] = g.incidence_range(cell);
                if (gi == 1) {
                    std::vector<core::BoundaryOccurrenceInput> incidences;
                    incidences.reserve(e-b);
                    for (std::size_t p=b; p<e; ++p) {
                        std::vector<core::AspectValue> extras;
                        extras.reserve(image.signature().aspects().size());
                        for (std::size_t a=0; a<image.signature().aspects().size(); ++a) {
                            const auto aid = static_cast<composite::AspectId>(a);
                            if (aid == exists || aid == orientation) continue;
                            extras.push_back({aid, g.aspect(aid,p)});
                        }
                        auto coeff = g.incidence_coefficient(p);
                        if (coeff < 0) coeff = -coeff;
                        incidences.emplace_back(g0.identity(g.lower_index(p)), g.aspect(exists,p),
                                                g.aspect(orientation,p), std::move(coeff), std::move(extras),
                                                g.occurrence_key(p));
                    }
                    builder.add_grade_one(g.relation_key(cell), g.relation_type(cell), incidences);
                } else {
                    std::vector<core::HigherIncidenceInput> incidences;
                    incidences.reserve(e-b);
                    for (std::size_t p=b; p<e; ++p) {
                        std::vector<core::AspectValue> extras;
                        extras.reserve(image.signature().aspects().size());
                        for (std::size_t a=0; a<image.signature().aspects().size(); ++a) {
                            const auto aid = static_cast<composite::AspectId>(a);
                            if (aid == exists || aid == orientation) continue;
                            extras.push_back({aid, g.aspect(aid,p)});
                        }
                        auto coeff = g.incidence_coefficient(p);
                        if (coeff < 0) coeff = -coeff;
                        incidences.emplace_back(g.lower_index(p), g.aspect(exists,p), g.aspect(orientation,p),
                                                std::move(coeff), std::move(extras), g.occurrence_key(p));
                    }
                    builder.add_higher(core::Grade{static_cast<std::uint16_t>(gi)}, g.relation_key(cell),
                                       g.relation_type(cell), incidences);
                }
            }
        }
        return builder.freeze();
    }

std::vector<std::byte> rewrite(const std::vector<std::byte>& bytes, const std::vector<std::pair<std::size_t,std::uint64_t>>& edits) {
    storage::rfcx_detail::Writer writer;
    writer.raw(bytes);
    for (const auto& [offset, value] : edits) writer.patch_u64(offset, value);
    writer.patch_u64(72, storage::rfcx_detail::fnv1a64(std::span<const std::byte>(writer.bytes()).subspan(storage::rfcx_detail::header_size)));
    return std::move(writer).take_bytes();
}
void compare(const std::vector<std::byte>& bytes) {
    const storage::MappedRfcx mapped(bytes);
    const auto actual = mapped.materialize();
    const auto expected = reference(mapped);
    check(storage::encode_rfcx(actual).bytes == storage::encode_rfcx(expected).bytes, "source differs from original reader");
    check(core::bianchi_through(actual, actual.max_grade()).satisfied(), "loaded source chain failure");
    check(actual.grade_count() == expected.grade_count(), "grade domain changed");
    for (std::size_t k = 0; k < actual.grade_count(); ++k) {
        const auto grade = core::Grade{static_cast<std::uint16_t>(k)};
        const auto& a = actual.grade(grade);
        const auto& e = expected.grade(grade);
        check(a.cell_scales == e.cell_scales && a.lattice_coefficient == e.lattice_coefficient, "retained factorization changed");
        check(a.identities == e.identities && a.relation_identities == e.relation_identities && a.occurrence_identities == e.occurrence_identities, "retained identity changed");
        check(a.offsets == e.offsets && a.lower_offsets == e.lower_offsets && a.incidence_by_lower == e.incidence_by_lower, "retained order changed");
        for (std::size_t cell = 0; k && cell < a.cell_count(); ++cell) {
            const auto input = numeric::SparseRationalVector::basis(a.cell_count(), cell, numeric::Rational{3,7});
            check(actual.boundary_apply_sparse(grade,input).entries() == mapped.boundary_apply_sparse(grade,input.view()).entries(), "mapped boundary disagreement");
        }
    }
    const auto frozen = [&] { storage::MappedRfcx reader(bytes); return reader.materialize(); }();
    check(storage::encode_rfcx(frozen).bytes == storage::encode_rfcx(actual).bytes, "source outlived reader incorrectly");
}
core::RelationalComplex tower(bool include_higher = true) {
    core::RelationSignature signature;
    const auto edge = signature.add_type("edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    const auto face = signature.add_type("face", core::BoundaryPolicy::DeclaredExact, 4, 4);
    const auto higher = signature.add_type("higher", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature, core::SemanticScale{7});
    builder.declare_grade(core::Grade{include_higher ? std::uint16_t{5} : std::uint16_t{1}});
    builder.declare_witness_identity(900);
    for (std::uint64_t i = 0; i < 4; ++i)
        builder.add_grade_one(core::RelationKey{100-i},edge,{{i,true,true,numeric::Rational{1}},{(i+1)%4,true,false,numeric::Rational{1}}});
    if (!include_higher) return builder.freeze();
    const std::vector<core::HigherIncidenceInput> boundary{{0,true,false,numeric::Rational{1,3}}, {1,true,false,numeric::Rational{1,3}}, {2,true,false,numeric::Rational{1,3}}, {3,true,false,numeric::Rational{1,3}}};
    builder.add_higher(core::Grade{2},core::RelationKey{88},face,boundary);
    builder.add_higher(core::Grade{2},core::RelationKey{33},face,boundary);
    builder.add_higher(core::Grade{3},higher,{{0,true,false,numeric::Rational{1,11}},{1,true,true,numeric::Rational{1,11}}});
    return builder.freeze();
}
void random_sources() {
    std::mt19937_64 random(971337);
    for (std::size_t trial = 0; trial < 180; ++trial) {
        core::RelationSignature signature;
        const auto flag = signature.aspects().add("measured",composite::AspectRole::Custom,trial%2);
        const auto type = signature.add_type("observation",core::BoundaryPolicy::DeclaredExact,0,0);
        core::RelationalComplexBuilder builder(signature,core::SemanticScale{static_cast<std::uint16_t>(trial)});
        builder.declare_witness_identity(800+trial);
        builder.declare_grade(core::Grade{trial%3 == 0 ? std::uint16_t{1} : std::uint16_t{0}});
        std::uint64_t occurrence = 9000;
        for (std::size_t cell = 0; cell < trial%19; ++cell) {
            std::vector<core::BoundaryOccurrenceInput> incidences;
            for (std::size_t slot = 0; slot < random()%13; ++slot) {
                numeric::Integer magnitude = random()%17;
                if (trial%9 == 0) magnitude <<= 130;
                const auto q = numeric::rational(static_cast<long>(random()%19), static_cast<long>(1+random()%31));
                const numeric::Rational value = trial%2 ? numeric::Rational(magnitude)*q : numeric::Rational(magnitude);
                incidences.emplace_back(random()%7,random()%4!=0,random()%2,value,
                    std::vector<core::AspectValue>{{flag,static_cast<bool>(random()%2)}},core::OccurrenceKey{occurrence++});
            }
            builder.add_grade_one(core::RelationKey{1000-cell},type,incidences);
        }
        compare(storage::encode_rfcx(builder.freeze()).bytes);
    }
}
void normalization() {
    const auto bytes = storage::encode_rfcx(tower(false)).bytes;
    const storage::MappedRfcx mapped(bytes);
    const auto d = mapped.grade(core::Grade{1}).desc;
    const auto nonminimal = rewrite(bytes,{{static_cast<std::size_t>(d.cell_scales),2},
        {static_cast<std::size_t>(d.lattice),static_cast<std::uint64_t>(std::int64_t{-2})},
        {static_cast<std::size_t>(d.lattice+8),2}});
    compare(nonminimal);
    const auto lower = mapped.grade(core::Grade{1}).coface_incidence_range(1);
    check(lower.second-lower.first == 2,"reverse order fixture");
    const auto a = mapped.grade(core::Grade{1}).incidence_by_lower(lower.first);
    const auto b = mapped.grade(core::Grade{1}).incidence_by_lower(lower.first+1);
    compare(rewrite(bytes,{{static_cast<std::size_t>(d.incidence_by_lower+8*lower.first),b},
        {static_cast<std::size_t>(d.incidence_by_lower+8*(lower.first+1)),a}}));
}
void refusals() {
    const auto bytes = storage::encode_rfcx(tower()).bytes;
    const storage::MappedRfcx mapped(bytes);
    const auto d = mapped.grade(core::Grade{2}).desc;
    const auto invalid_chain = rewrite(bytes,{{static_cast<std::size_t>(d.lattice),2}});
    std::vector<std::pair<std::size_t,std::uint64_t>> duplicate_edits{{static_cast<std::size_t>(d.occurrence_keys),0}};
    if (d.occurrence_key_index) duplicate_edits.emplace_back(static_cast<std::size_t>(d.occurrence_key_index),0);
    const auto duplicate = rewrite(bytes,duplicate_edits);
    for (const auto* input : {&invalid_chain,&duplicate}) {
        const storage::MappedRfcx reader(*input);
        rejects([&]{ (void)reference(reader); });
        rejects([&]{ (void)reader.materialize(); });
    }
    core::RelationSignature signature;
    const auto branch = signature.add_branch_type("branch",4,4);
    core::RelationalComplexBuilder builder(signature);
    builder.add_canonical_grade_one(branch,{10,20,30,40},0);
    const auto branch_bytes = storage::encode_rfcx(builder.freeze()).bytes;
    const storage::MappedRfcx branch_reader(branch_bytes);
    const auto descriptor = branch_reader.grade(core::Grade{1}).desc;
    const auto aspects = descriptor.aspect_words_offset;
    const auto head = signature.type(branch).distinguished_role_aspect().value();
    const auto head_offset = static_cast<std::size_t>(aspects+8*head);
    const auto lattice = static_cast<std::size_t>(descriptor.lattice);
    const std::vector<std::vector<std::pair<std::size_t,std::uint64_t>>> edits{
        {{static_cast<std::size_t>(aspects),14},{lattice,0}},
        {{head_offset,3}},
        {{static_cast<std::size_t>(aspects+8),3},{lattice+8,static_cast<std::uint64_t>(std::int64_t{-1})}},
        {{lattice,static_cast<std::uint64_t>(std::int64_t{-6})}},
        {{lattice+8,0}},
        {{lattice+8,2}}
    };
    for (const auto& edit : edits) {
        const storage::MappedRfcx reader(rewrite(branch_bytes,edit));
        rejects([&]{ (void)reference(reader); });
        rejects([&]{ (void)reader.materialize(); });
    }
    const auto series = storage::NativeTemporalSeries(storage::TemporalEndpoint(tower(false)), 291);
    auto source = storage::encode_rfcx(tower(false)).bytes;
    const storage::MappedRfcx original(source);
    const auto g = original.grade(core::Grade{1}).desc;
    source = rewrite(source,{{static_cast<std::size_t>(g.cell_scales),2},
        {static_cast<std::size_t>(g.lattice),static_cast<std::uint64_t>(std::int64_t{-2})},
        {static_cast<std::size_t>(g.lattice+8),2}});
    const storage::MappedRfcx changed(source);
    const auto channels = storage::encode_temporal_endpoint(storage::TemporalEndpoint(changed.materialize()));
    storage::temporal_codec_detail::Writer writer;
    writer.raw(storage::native_series_codec_detail::magic);writer.u32(1);writer.u32(0);writer.u64(0);writer.u64(0);writer.u64(series.source_namespace());
    storage::native_series_codec_detail::blob(writer,source);storage::native_series_codec_detail::blob(writer,channels);writer.u64(0);writer.u64(0);
    writer.patch_u64(16,writer.size());writer.patch_u64(24,storage::rfcx_detail::fnv1a64(std::span<const std::byte>(writer.bytes()).subspan(32)));
    rejects([&]{ (void)storage::decode_native_series(writer.bytes()); });
}
}
int main() {
    try {
        random_sources();
        compare(storage::encode_rfcx(tower()).bytes);
        normalization();
        refusals();
        std::cout << "PASS " << checks << " retained source checks\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
