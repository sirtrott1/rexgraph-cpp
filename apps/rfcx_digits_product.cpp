#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/storage/rfcx.hpp"
#include "rfc/storage/local_blocks.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace {
using Clock = std::chrono::steady_clock;
using R = rfc::numeric::Rational;

struct DigitsData {
    std::vector<std::array<unsigned,64>> pixels;
    std::vector<unsigned> labels;
};

DigitsData load_digits(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("unable to open digits CSV");
    DigitsData out;
    std::string line;
    while (std::getline(in,line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::array<unsigned,65> row{};
        std::string field;
        std::size_t col=0;
        while (std::getline(ss,field,',')) {
            if (col>=row.size()) throw std::runtime_error("digits row has too many columns");
            const auto v=std::stoul(field);
            row[col++]=static_cast<unsigned>(v);
        }
        if (col!=65) throw std::runtime_error("digits row must have 65 columns");
        std::array<unsigned,64> image{};
        for (std::size_t i=0;i<64;++i) {
            if (row[i]>16) throw std::runtime_error("digits intensity outside 0..16");
            image[i]=row[i];
        }
        if (row[64]>9) throw std::runtime_error("digits label outside 0..9");
        out.pixels.push_back(image);
        out.labels.push_back(row[64]);
    }
    if (out.pixels.empty()) throw std::runtime_error("digits CSV is empty");
    return out;
}

struct BuiltDigits {
    rfc::core::RelationalComplex complex;
    rfc::core::RelationTypeId pixel_reading{};
    rfc::core::RelationTypeId profile{};
    rfc::core::RelationTypeId class_assignment{};
    rfc::core::RelationTypeId closure{};
    std::uint64_t active_pixels{};
};

BuiltDigits build_digits(const DigitsData& data) {
    using namespace rfc;
    core::RelationSignature sig;
    const auto pixel_reading=sig.add_type("pixel-reading",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto profile=sig.add_branch_type("image-profile",2,0);
    const auto class_assignment=sig.add_type("class-assignment",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto closure=sig.add_branch_type("profile-closure",2,0);

    core::RelationalComplexBuilder builder(sig);
    constexpr core::IdentityKey pixel_base=10'000;
    constexpr core::IdentityKey image_base=1'000'000;
    constexpr core::IdentityKey class_base=2'000'000;
    for (std::size_t p=0;p<64;++p) builder.declare_witness_identity(pixel_base+p);
    for (std::size_t image=0; image<data.pixels.size(); ++image) builder.declare_witness_identity(image_base+image);
    for (std::size_t c=0;c<10;++c) builder.declare_witness_identity(class_base+c);

    std::uint64_t active_pixels=0;
    for (std::size_t image=0; image<data.pixels.size(); ++image) {
        const auto image_key=image_base+image;
        std::vector<std::uint32_t> pair_indices;
        std::vector<core::IdentityKey> profile_ids;
        std::vector<R> shares;
        unsigned total=0;
        for (const auto value:data.pixels[image]) total+=value;
        if (total==0) throw std::runtime_error("digits image has zero total intensity");
        profile_ids.push_back(image_key);
        for (std::size_t p=0;p<64;++p) {
            const auto intensity=data.pixels[image][p];
            if (intensity==0) continue;
            ++active_pixels;
            const auto pair=builder.add_grade_one(pixel_reading,{
                {image_key,true,true,R{1}},
                {pixel_base+p,true,false,R{1}}});
            pair_indices.push_back(pair.index);
            profile_ids.push_back(pixel_base+p);
            R share(static_cast<unsigned long>(intensity),static_cast<unsigned long>(total));
            share.canonicalize();
            shares.push_back(std::move(share));
        }
        const auto profile_relation=builder.add_weighted_branch_grade_one(
            profile,profile_ids,0,shares);
        builder.add_grade_one(class_assignment,{
            {image_key,true,true,R{1}},
            {class_base+data.labels[image],true,false,R{1}}});

        std::vector<std::uint32_t> closure_cells;
        closure_cells.reserve(pair_indices.size()+1u);
        closure_cells.push_back(profile_relation.index);
        closure_cells.insert(closure_cells.end(),pair_indices.begin(),pair_indices.end());
        builder.add_weighted_branch_higher(rfc::core::Grade{2},closure,closure_cells,0,shares);
    }
    return BuiltDigits{builder.freeze(),pixel_reading,profile,class_assignment,closure,active_pixels};
}


std::vector<rfc::storage::LocalBlockDefinition> build_class_local_blocks(
    const rfc::core::RelationalComplex& complex,
    const DigitsData& data) {
    using namespace rfc;
    constexpr core::IdentityKey image_base=1'000'000;
    const auto& g0=complex.grade(core::Grade{0});
    const auto& g1=complex.grade(core::Grade{1});
    const auto& g2=complex.grade(core::Grade{2});
    const auto exists=complex.signature().existence_aspect();

    std::unordered_map<core::IdentityKey,std::uint32_t> g0_index;
    g0_index.reserve(g0.identities.size());
    for (std::size_t i=0;i<g0.identities.size();++i) g0_index.emplace(g0.identities[i],static_cast<std::uint32_t>(i));

    std::vector<algebra::SubcomplexMask> masks;
    masks.reserve(10);
    for (std::size_t c=0;c<10;++c) masks.emplace_back(complex);

    for (std::size_t image=0;image<data.pixels.size();++image) {
        const auto found=g0_index.find(image_base+image);
        if (found==g0_index.end()) throw std::runtime_error("image identity missing while building local blocks");
        auto& mask=masks.at(data.labels[image]);
        const auto [b,e]=g1.coface_incidence_range(found->second);
        for (std::size_t q=b;q<e;++q) {
            const auto p=g1.incidence_by_lower[q];
            if (!g1.aspects.get(exists,p)) continue;
            const auto relation=g1.upper_index[p];
            mask.select(core::CellId{core::Grade{1},relation});
            const auto [ub,ue]=g2.coface_incidence_range(relation);
            for (std::size_t uq=ub;uq<ue;++uq) {
                const auto up=g2.incidence_by_lower[uq];
                if (!g2.aspects.get(exists,up)) continue;
                mask.select(core::CellId{core::Grade{2},g2.upper_index[up]});
            }
        }
    }
    std::vector<storage::LocalBlockDefinition> blocks;
    blocks.reserve(10);
    for (std::size_t c=0;c<10;++c) {
        masks[c].close_boundaries(complex);
        blocks.push_back(storage::make_strict_local_block(complex,10'000u+c,masks[c]));
    }
    return blocks;
}

template<class F>
double millis(F&& fn) {
    const auto start=Clock::now();
    fn();
    const auto end=Clock::now();
    return std::chrono::duration<double,std::milli>(end-start).count();
}

struct HighArityResult {
    std::size_t arity{};
    std::uint64_t incidences{};
    std::uint64_t bytes{};
    std::uint64_t clique_cells{};
    double build_ms{};
    double write_ms{};
    double open_ms{};
    double boundary_ms{};
};

HighArityResult run_high_arity(const std::filesystem::path& path, std::size_t tail_count, std::size_t iterations) {
    using namespace rfc;
    if (tail_count<2 || tail_count>100000u) throw std::invalid_argument("high arity tail count outside product-test range");
    core::RelationSignature sig;
    const auto branch=sig.add_branch_type("wide-branch",2,0);
    const auto edge=sig.add_type("wide-pair",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto closure=sig.add_branch_type("wide-closure",2,0);
    std::optional<core::RelationalComplex> complex;
    const auto build_time=millis([&]{
        core::RelationalComplexBuilder builder(sig);
        constexpr core::IdentityKey base=50'000'000;
        std::vector<core::IdentityKey> ids; ids.reserve(tail_count+1u);
        ids.push_back(base);
        for (std::size_t i=0;i<tail_count;++i) ids.push_back(base+1u+i);
        numeric::Rational share=numeric::Rational{1}/numeric::Rational(static_cast<unsigned long>(tail_count));
        share.canonicalize();
        numeric::RationalVector shares(tail_count,share);
        const auto profile=builder.add_weighted_branch_grade_one(branch,ids,0,shares);
        std::vector<std::uint32_t> cells; cells.reserve(tail_count+1u); cells.push_back(profile.index);
        for (std::size_t i=0;i<tail_count;++i) {
            const auto pair=builder.add_grade_one(edge,{{base,true,true,R{1}},{base+1u+i,true,false,R{1}}});
            cells.push_back(pair.index);
        }
        builder.add_weighted_branch_higher(core::Grade{2},closure,cells,0,shares);
        complex.emplace(builder.freeze());
    });
    if (!core::bianchi_through(*complex,core::Grade{2}).satisfied()) throw std::runtime_error("high-arity fixture failed Bianchi");
    storage::RfcxWriteStats stats;
    const auto write_time=millis([&]{stats=storage::write_rfcx(*complex,path);});
    std::optional<storage::MappedRfcx> mapped;
    const auto open_time=millis([&]{mapped.emplace(path,true);});
    const auto g1=mapped->grade(core::Grade{1});
    const auto source=numeric::SparseRationalVector::basis(g1.cell_count(),0,R{1});
    numeric::SparseRationalVector result;
    const auto boundary_time=millis([&]{for(std::size_t i=0;i<iterations;++i) result=mapped->boundary_apply_sparse(core::Grade{1},source.view());});
    if (result.nonzeros()!=tail_count+1u) throw std::runtime_error("high-arity boundary support mismatch");
    const auto arity=tail_count+1u;
    const auto clique=static_cast<std::uint64_t>(arity)*(static_cast<std::uint64_t>(arity)-1u)/2u;
    return HighArityResult{arity,stats.incidences,stats.bytes,clique,build_time,write_time,open_time,boundary_time};
}

std::optional<std::uint32_t> profile_for_image(const rfc::storage::MappedRfcx& mapped,
                                               rfc::core::IdentityKey image_key,
                                               rfc::core::RelationTypeId profile_type) {
    const auto g0=mapped.grade(rfc::core::Grade{0});
    const auto g1=mapped.grade(rfc::core::Grade{1});
    const auto image=g0.find_identity(image_key);
    if (!image) return std::nullopt;
    const auto [b,e]=g1.coface_incidence_range(*image);
    for (std::size_t q=b;q<e;++q) {
        const auto p=g1.incidence_by_lower(q);
        const auto relation=g1.upper_index(p);
        if (g1.relation_type(relation)==profile_type) return relation;
    }
    return std::nullopt;
}

} // namespace

int main(int argc,char** argv) {
    try {
        if (argc<3) {
            std::cerr << "usage: rfc_rfcx_digits_product <digits.csv> <snapshot.rfcx> [iterations]\n";
            return EXIT_FAILURE;
        }
        const std::filesystem::path csv=argv[1];
        const std::filesystem::path snapshot=argv[2];
        const std::size_t iterations=argc>3 ? static_cast<std::size_t>(std::stoull(argv[3])) : 1000u;

        DigitsData data;
        const auto load_ms=millis([&]{data=load_digits(csv);});
        std::optional<BuiltDigits> built;
        const auto build_ms=millis([&]{built.emplace(build_digits(data));});
        const auto& complex=built->complex;
        if (!rfc::core::bianchi_through(complex,rfc::core::Grade{2}).satisfied()) throw std::runtime_error("digits tower failed Bianchi");

        rfc::storage::RfcxWriteStats write_stats;
        const auto write_ms=millis([&]{write_stats=rfc::storage::write_rfcx(complex,snapshot);});
        std::optional<rfc::storage::MappedRfcx> mapped;
        const auto open_ms=millis([&]{mapped.emplace(snapshot,true);});

        const auto local_path=std::filesystem::path(snapshot.string()+".local.rfcxl");
        std::vector<rfc::storage::LocalBlockDefinition> local_blocks;
        const auto local_build_ms=millis([&]{local_blocks=build_class_local_blocks(complex,data);});
        rfc::storage::LocalBlockWriteStats local_stats;
        const auto local_write_ms=millis([&]{local_stats=rfc::storage::write_local_blocks(complex,write_stats.payload_hash,local_blocks,local_path);});
        std::optional<rfc::storage::MappedLocalBlocks> mapped_local;
        const auto local_open_ms=millis([&]{mapped_local.emplace(local_path,true); mapped_local->validate_against(*mapped);});

        const auto g0=mapped->grade(rfc::core::Grade{0});
        const auto g1=mapped->grade(rfc::core::Grade{1});
        const auto g2=mapped->grade(rfc::core::Grade{2});
        constexpr rfc::core::IdentityKey image0=1'000'000;
        constexpr rfc::core::IdentityKey pixel_center=10'000+27;
        constexpr rfc::core::IdentityKey class0=2'000'000;

        const auto local_center_shared=mapped_local->find_shared_cell(
            rfc::storage::StableCellKey{rfc::core::Grade{0},pixel_center});
        if (!local_center_shared) throw std::runtime_error("center pixel missing from local glue index");
        const auto local_center_members=mapped_local->memberships(*local_center_shared);

        std::uint64_t identity_hits=0;
        const auto identity_lookup_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) {
                identity_hits+=g0.find_identity((i&1u)==0u ? image0 : pixel_center).has_value() ? 1u : 0u;
            }
        });

        const auto center_index=g0.find_identity(pixel_center);
        if (!center_index) throw std::runtime_error("center pixel identity missing");
        std::uint64_t reverse_visits=0;
        const auto reverse_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) {
                const auto [b,e]=g1.coface_incidence_range(*center_index);
                for (std::size_t q=b;q<e;++q) {
                    const auto p=g1.incidence_by_lower(q);
                    if (g1.relation_type(g1.upper_index(p))==built->pixel_reading) ++reverse_visits;
                }
            }
        });

        std::uint64_t type_visits=0;
        const auto type_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) {
                g1.for_each_relation_of_type(built->profile,[&](std::uint32_t){++type_visits;});
            }
        });

        const auto profile_index=profile_for_image(*mapped,image0,built->profile);
        if (!profile_index) throw std::runtime_error("image profile lookup failed");
        const auto profile_source=rfc::numeric::SparseRationalVector::basis(g1.cell_count(),*profile_index,R{1});
        rfc::numeric::SparseRationalVector mapped_boundary;
        const auto mapped_boundary_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) mapped_boundary=mapped->boundary_apply_sparse(rfc::core::Grade{1},profile_source.view());
        });
        rfc::numeric::SparseRationalVector memory_boundary;
        const auto memory_boundary_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) memory_boundary=complex.boundary_apply_sparse(rfc::core::Grade{1},profile_source);
        });
        if (mapped_boundary.entries().size()!=memory_boundary.entries().size()) throw std::runtime_error("mapped/in-memory boundary size mismatch");
        for (std::size_t i=0;i<mapped_boundary.entries().size();++i) {
            if (mapped_boundary.entries()[i].index!=memory_boundary.entries()[i].index ||
                mapped_boundary.entries()[i].value!=memory_boundary.entries()[i].value) {
                throw std::runtime_error("mapped/in-memory boundary mismatch");
            }
        }

        const auto head=mapped->signature().aspects().id("head");
        rfc::composite::BinaryProgram head_query(3);
        head_query.append({rfc::composite::BinaryOpcode::LoadAspect,0,0,0,0,mapped->signature().existence_aspect()});
        head_query.append({rfc::composite::BinaryOpcode::LoadAspect,1,0,0,0,head});
        head_query.append({rfc::composite::BinaryOpcode::And,2,0,1,0,0});
        std::uint64_t head_total=0;
        const auto binary_ms=millis([&]{
            for (std::size_t i=0;i<iterations;++i) head_total+=mapped->binary_count(rfc::core::Grade{1},head_query,2);
        });

        const auto [co_b,co_e]=g2.coface_incidence_range(*profile_index);
        if (co_e-co_b!=1u) throw std::runtime_error("profile should have one retained closure coface");

        const auto class_idx=g0.find_identity(class0);
        if (!class_idx) throw std::runtime_error("class identity missing");
        std::size_t class_zero_images=0;
        {
            const auto [b,e]=g1.coface_incidence_range(*class_idx);
            for (std::size_t q=b;q<e;++q) {
                const auto p=g1.incidence_by_lower(q);
                if (g1.relation_type(g1.upper_index(p))==built->class_assignment) ++class_zero_images;
            }
        }

        std::optional<rfc::core::RelationalComplex> roundtrip;
        const auto roundtrip_ms=millis([&]{roundtrip.emplace(mapped->materialize());});
        if (!rfc::core::bianchi_through(*roundtrip,rfc::core::Grade{2}).satisfied()) throw std::runtime_error("roundtrip Bianchi failure");

        auto high_path=snapshot;
        high_path += ".higharity.rfcx";
        const auto high=run_high_arity(high_path,4096u,std::max<std::size_t>(1u,iterations/10u));

        std::cout << "RFCX_DIGITS_PRODUCT_V1\n";
        std::cout << "samples=" << data.pixels.size() << " active_pixels=" << built->active_pixels << " load_ms=" << load_ms << '\n';
        std::cout << "grade0_cells=" << g0.cell_count() << " grade1_cells=" << g1.cell_count() << " grade2_cells=" << g2.cell_count() << '\n';
        std::cout << "grade1_incidences=" << g1.incidence_count() << " grade2_incidences=" << g2.incidence_count() << '\n';
        std::cout << "build_ms=" << build_ms << " write_ms=" << write_ms << " open_verify_ms=" << open_ms << " roundtrip_ms=" << roundtrip_ms << '\n';
        std::cout << "rfcx_bytes=" << write_stats.bytes << " bytes_per_occurrence="
                  << static_cast<double>(write_stats.bytes)/static_cast<double>(write_stats.incidences) << '\n';
        std::cout << "local_blocks=" << local_stats.blocks << " local_shared_cells=" << local_stats.shared_cells
                  << " local_index_bytes=" << local_stats.bytes << " center_pixel_block_memberships=" << local_center_members.size() << '\n';
        std::cout << "local_build_ms=" << local_build_ms << " local_write_ms=" << local_write_ms
                  << " local_open_validate_ms=" << local_open_ms << '\n';
        std::cout << "identity_lookup_iterations=" << iterations << " identity_lookup_ms=" << identity_lookup_ms << " hits=" << identity_hits << '\n';
        std::cout << "reverse_incidence_iterations=" << iterations << " reverse_incidence_ms=" << reverse_ms << " filtered_visits=" << reverse_visits << '\n';
        std::cout << "type_index_iterations=" << iterations << " type_index_ms=" << type_ms << " visits=" << type_visits << '\n';
        std::cout << "binary_wordcode_iterations=" << iterations << " binary_wordcode_ms=" << binary_ms << " accumulated_heads=" << head_total << '\n';
        std::cout << "mapped_boundary_iterations=" << iterations << " mapped_boundary_ms=" << mapped_boundary_ms << '\n';
        std::cout << "memory_boundary_iterations=" << iterations << " memory_boundary_ms=" << memory_boundary_ms << '\n';
        std::cout << "profile_boundary_nonzeros=" << mapped_boundary.nonzeros() << " profile_higher_cofaces=" << (co_e-co_b) << '\n';
        std::cout << "class_zero_images=" << class_zero_images << '\n';
        std::cout << "high_arity=" << high.arity << " high_incidences=" << high.incidences
                  << " high_rfcx_bytes=" << high.bytes << " implied_clique_cells=" << high.clique_cells << '\n';
        std::cout << "high_build_ms=" << high.build_ms << " high_write_ms=" << high.write_ms
                  << " high_open_ms=" << high.open_ms << " high_boundary_ms=" << high.boundary_ms << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
