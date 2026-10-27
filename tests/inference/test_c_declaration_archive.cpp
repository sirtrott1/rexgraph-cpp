#include "c_declaration_fixtures.hpp"
#include "c_declaration_observations.hpp"
#include <fstream>
#include <iostream>
using namespace c_declaration_fixtures;
static std::size_t checks{};
static void check(bool yes, const char *why) {
    ++checks;
    if (!yes)
        throw std::runtime_error(why);
}
template <class F> static void reject(F f, const char *why) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, why);
}
using namespace c_declaration_observations;
static void controls(const Recipe &recipe, const CDeclarationArchive &original,
                     const std::vector<std::byte> &image) {
    G g;
    auto restored = decode_c_declaration_archive(image, g);
    auto bytes = encode_c_declaration_archive(restored);
    check(bytes == image, "canonical original declaration bytes");
    check(observe(recipe.name, original, g) == observe(recipe.name, restored, g),
          "fresh certificate differs from producer");
    check(restored.metadata().producer_namespace == 77 && restored.metadata().revision == 3 &&
              restored.metadata().evidence_namespace == 99 && restored.metadata().evidence_cutoff == 12,
          "original producer/evidence data");
    check(restored.exports() == original.exports(), "export order/aliases lost");
    for (std::size_t i = 0; i < recipe.records.size(); ++i) {
        const auto &r = recipe.records[i];
        if (r.kind == K::RefinementMap && recipe.name == "refinement") {
            const auto &map = restored.get<PreparedRefinementMap>(i);
            if (map.target().size(0) == 2 && map.source().size(0) == 1) {
                const auto &terms = map.declaration().grades[0][0];
                check(terms.size() == 4 && terms[1].coefficient == 0 && terms[2].coefficient == 2 &&
                          terms[3].coefficient == -2,
                      "raw supported-zero and cancelling map history lost");
            }
        }
        if (r.kind == K::Source) {
            check(!original.get<core::OwnedRelationalSource>(i).same_owner(
                      restored.get<core::OwnedRelationalSource>(i)),
                  "cold decode reused original source owner");
            check(encode_rfcx(original.get<core::OwnedRelationalSource>(i).source()).bytes ==
                      encode_rfcx(restored.get<core::OwnedRelationalSource>(i).source()).bytes,
                  "source flags changed");
        }
    }
    for (std::size_t n : {std::size_t{0}, image.size() / 2, image.size() - 1})
        reject(
            [&] {
                G guard;
                (void)decode_c_declaration_archive(std::span(image).first(n), guard);
            },
            "truncated archive accepted");
    auto changed = image;
    changed[8] = std::byte{2};
    auto checksum = P{}.u64(rfcx_detail::fnv1a64(std::span(changed).first(changed.size() - 8))).take();
    std::copy(checksum.begin(), checksum.end(), changed.end() - 8);
    reject(
        [&] {
            G guard;
            (void)decode_c_declaration_archive(changed, guard);
        },
        "unknown/corrupt archive accepted");
    auto bad = recipe.records;
    bad.back().dependencies.push_back(bad.size());
    reject(
        [&] {
            G guard;
            (void)CDeclarationArchive::prepare(original.metadata(), bad, recipe.exports, guard);
        },
        "forward/foreign graph edge accepted");
    bad = recipe.records;
    bad.back().payload.push_back(std::byte{0});
    reject(
        [&] {
            G guard;
            (void)CDeclarationArchive::prepare(original.metadata(), bad, recipe.exports, guard);
        },
        "trailing operation fields accepted");
    CDeclarationLimits limits;
    limits.max_records = 1;
    reject(
        [&] {
            G guard;
            (void)decode_c_declaration_archive(image, guard, limits);
        },
        "record resource limit bypassed");
    limits = {};
    limits.max_depth = 1;
    reject(
        [&] {
            G guard;
            (void)decode_c_declaration_archive(image, guard, limits);
        },
        "history depth limit bypassed");
    limits = {};
    limits.max_words = 1;
    reject(
        [&] {
            G guard;
            (void)decode_c_declaration_archive(image, guard, limits);
        },
        "aggregate words limit bypassed");
    if (recipe.name == "surface-factors") {
        CDeclarationLimits derived;
        derived.max_dimension = 2;
        reject(
            [&] {
                G guard;
                (void)decode_c_declaration_archive(image, guard, derived);
            },
            "derived native construction escaped archive dimension cap");
    }
    G measured;
    (void)decode_c_declaration_archive(image, measured);
    for (auto stop : {std::size_t{1}, measured.steps() / 2, measured.steps()}) {
        std::size_t calls{};
        std::optional<CDeclarationArchive> result;
        G cancelled({}, [&] {
            if (++calls == stop)
                throw std::runtime_error("cancel");
        });
        reject([&] { result = decode_c_declaration_archive(image, cancelled); },
               "cancelled decode published certificate");
        check(!result && calls == stop, "partial archive/cancellation boundary");
    }
}
static std::vector<std::byte> untrusted(const Recipe &c) {
    P p;
    p.u64(0x31444358434652ULL)
        .u64(1)
        .u64(77)
        .u64(88)
        .u64(3)
        .u64(99)
        .u64(12)
        .string("declaration-archive exact cold replay: " + c.name)
        .u64(c.records.size());
    for (const auto &d : c.records) {
        p.u64(static_cast<std::uint64_t>(d.kind)).u64(d.key).u64(d.revision).u64(d.dependencies.size());
        for (auto i : d.dependencies)
            p.u64(i);
        p.blob(d.payload);
    }
    p.u64(c.exports.size());
    for (auto i : c.exports)
        p.u64(i);
    auto image = p.take();
    auto suffix = P{}.u64(rfcx_detail::fnv1a64(image)).take();
    image.insert(image.end(), suffix.begin(), suffix.end());
    return image;
}
static void semantic_controls(Recipe c) {
    auto replace = [&](K kind, std::vector<std::byte> p) {
        for (auto &d : c.records)
            if (d.kind == kind) {
                d.payload = std::move(p);
                return;
            }
        throw std::runtime_error("control kind absent");
    };
    if (c.name == "characters")
        replace(K::CyclicChain, P{}.u64(6).u64(0).u64(2).indices({}).indices({0, 6}).take());
    else if (c.name == "branch")
        replace(K::Branch, P{}.vector(V{Q{1, 2}, Q{1, 3}})
                               .indices({11, 12, 13, 14, 15})
                               .indices({101, 102, 103})
                               .u64(2)
                               .take());
    else if (c.name == "periods") {
        G g;
        auto surface = field::PreparedSquareTiledSurface::prepare(
            {0}, {0}, 1, field::SquareTiledOrientation::Positive, 1, 1, g);
        P p;
        p.matrix(M::identity(2)).u64(1).u64(0).rational(1).u64(surface.cup_terms().size());
        for (const auto &t : surface.cup_terms())
            p.u64(t.face).u64(t.first_slot).u64(t.second_slot).rational(Q{2 * t.coefficient});
        replace(K::PeriodConstruction, p.take());
    } else if (c.name == "surface-factors")
        replace(K::Normalization, P{}.u64(1).vector(V{2}).vector(V{1}).take());
    else if (c.name == "radical-normalization")
        replace(K::Normalization, P{}.u64(1).vector(V{2, 0}).vector(V{0, -1}).take());
    else if (c.name == "univariate") {
        replace(K::Expression, P{}.vector(V{0, 1}).vector(V{0}).take());
    } else if (c.name == "jets-and-words") {
        replace(K::JetBase, P{}.vectors({{}, V{1, 1}})
                                .matrices({M::from_rows({{-1}}), M::identity(2)})
                                .vectors({V{3}, V{0, 0}})
                                .take());
    } else if (c.name == "rational-family") {
        replace(K::LayerFamily, P{}.u64(2).u64(0).take());
    } else if (c.name == "holonomy")
        replace(K::SurfaceCocycle, P{}.vector(V{2, 0}).take());
    else if (c.name == "rotating")
        replace(K::RotatingSystem, P{}.vector(V{1, 2}).matrices({M::identity(1), M::identity(2)}).take());
    else if (c.name == "refinement") {
        P p;
        limits(p);
        p.u64(1).u64(1).u64(1).u64(99).rational(1);
        replace(K::RefinementMap, p.take());
    } else if (c.name == "orbit") {
        auto original = c.records[1];
        auto at = std::size_t{3};
        for (auto &row : c.records)
            for (auto &dep : row.dependencies)
                if (dep >= at)
                    ++dep;
        for (auto &dep : c.exports)
            if (dep >= at)
                ++dep;
        original.key = 1000;
        c.records.insert(c.records.begin() + static_cast<std::ptrdiff_t>(at), original);
        for (auto &row : c.records)
            if (row.kind == K::NativeField)
                row.dependencies = {at};
    }
    auto forged = untrusted(c);
    reject(
        [&] {
            G g;
            (void)decode_c_declaration_archive(forged, g);
        },
        "rechecksummed semantic/owner tamper issued certificate");
}
int main(int argc, char **argv) {
    try {
        const bool dump = argc == 2 && std::string_view(argv[1]) == "--dump";
        const bool write = argc == 3 && std::string_view(argv[1]) == "--write";
        if (write)
            std::filesystem::create_directories(argv[2]);
        std::ofstream expected;
        if (write)
            expected.open(std::filesystem::path(argv[2]) / "expected.tsv");
        if (dump)
            std::cout << '{';
        bool first = true;
        for (const auto &recipe : recipes()) {
            G g;
            auto a = recipe.prepare(g);
            auto image = encode_c_declaration_archive(a);
            if (write)
                expected << recipe.name << "\t" << observe(recipe.name, a, g) << "\n";
            if (write)
                write_file_exact(std::filesystem::path(argv[2]) / (recipe.name + ".bin"), image);
            if (dump) {
                if (!first)
                    std::cout << ',';
                first = false;
                std::cout << '"' << recipe.name << "\":\"" << observe(recipe.name, a, g) << '"';
            } else {
                controls(recipe, a, image);
                semantic_controls(recipe);
                std::cout << recipe.name << " passed\n";
            }
        }
        if (dump)
            std::cout << "}\n";
        else
            std::cout << checks
                      << " bounded C declaration, fresh-owner and certificate replay checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
