#include "rfc/core/axioms.hpp"
#include "rfc/storage/rfcx.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}


static bool same_sparse(const rfc::numeric::SparseRationalVector& a,
                        const rfc::numeric::SparseRationalVector& b) {
    if (a.size()!=b.size() || a.nonzeros()!=b.nonzeros()) return false;
    for (std::size_t i=0;i<a.entries().size();++i) {
        if (a.entries()[i].index!=b.entries()[i].index || a.entries()[i].value!=b.entries()[i].value) return false;
    }
    return true;
}

static bool same_complex(const rfc::core::RelationalComplex& a,
                         const rfc::core::RelationalComplex& b) {
    if (a.grade_count() != b.grade_count()) return false;
    if (a.semantic_scale() != b.semantic_scale()) return false;
    if (a.signature().aspects().size() != b.signature().aspects().size()) return false;
    if (a.signature().type_count() != b.signature().type_count()) return false;
    for (std::size_t g=0; g<a.grade_count(); ++g) {
        const auto grade = rfc::core::Grade{static_cast<std::uint16_t>(g)};
        const auto& ag=a.grade(grade); const auto& bg=b.grade(grade);
        if (ag.cell_count()!=bg.cell_count() || ag.incidence_count()!=bg.incidence_count()) return false;
        if (ag.identities!=bg.identities || ag.relation_types!=bg.relation_types || ag.relation_identities!=bg.relation_identities) return false;
        if (ag.cell_scales!=bg.cell_scales || ag.offsets!=bg.offsets || ag.lower_index!=bg.lower_index || ag.upper_index!=bg.upper_index) return false;
        if (ag.occurrence_identities!=bg.occurrence_identities || ag.lattice_coefficient!=bg.lattice_coefficient) return false;
        if (ag.lower_offsets!=bg.lower_offsets || ag.incidence_by_lower!=bg.incidence_by_lower) return false;
        if (ag.aspects.aspect_count()!=bg.aspects.aspect_count() || ag.aspects.slots()!=bg.aspects.slots()) return false;
        for (std::size_t aspect=0; aspect<ag.aspects.aspect_count(); ++aspect) {
            const auto& ap=ag.aspects.plane(static_cast<rfc::composite::AspectId>(aspect));
            const auto& bp=bg.aspects.plane(static_cast<rfc::composite::AspectId>(aspect));
            if (ap.size()!=bp.size() || ap.word_count()!=bp.word_count()) return false;
            for (std::size_t w=0; w<ap.word_count(); ++w) if (ap.word(w)!=bp.word(w)) return false;
        }
    }
    return true;
}

int main() {
    try {
        using namespace rfc;
        using R=numeric::Rational;

        core::RelationSignature sig;
        const auto branch=sig.add_branch_type("branch",4,4);
        const auto edge=sig.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
        const auto huge=sig.add_type("huge",core::BoundaryPolicy::DeclaredExact,2,2);
        const auto visible=sig.aspects().add("visible",composite::AspectRole::Visibility,false);

        core::RelationalComplexBuilder builder(sig, core::SemanticScale{3});
        builder.add_weighted_branch_grade_one(branch,{100,101,102,103},0,{R{1,4},R{1,2},R{1,4}});
        builder.add_grade_one(core::RelationKey{42},edge,{
            {100,true,true,R{1},{{visible,true}},core::OccurrenceKey{500}},
            {101,true,false,R{1},{{visible,true}},core::OccurrenceKey{501}}});
        builder.add_grade_one(edge,{{100,true,true,R{1}},{102,true,false,R{1}}});
        builder.add_grade_one(edge,{{100,true,true,R{1}},{103,true,false,R{1}}});
        builder.add_weighted_branch_higher(core::Grade{2},branch,{0,1,2,3},0,{R{1,4},R{1,2},R{1,4}});

        numeric::Integer big=1;
        big <<= 90;
        R huge_value(big);
        builder.add_grade_one(huge,{{900,true,true,huge_value},{901,true,false,huge_value}});
        const auto complex=builder.freeze();
        require(core::bianchi_through(complex,core::Grade{2}).satisfied(),"source tower failed Bianchi");

        const auto path=std::filesystem::temp_directory_path()/"rfc_test_snapshot.rfcx";
        const auto stats=storage::write_rfcx(complex,path);
        require(stats.bytes>0 && stats.cells>0 && stats.incidences>0,"RFCX write stats invalid");

        storage::MappedRfcx mapped(path);
        require(mapped.grade_count()==complex.grade_count(),"mapped grade count mismatch");
        require(mapped.semantic_scale()==core::SemanticScale{3},"mapped semantic scale mismatch");
        require(mapped.grade(core::Grade{0}).find_identity(102).has_value(),"identity index lookup failed");
        const auto mapped_g1=mapped.grade(core::Grade{1});
        require(mapped_g1.find_relation(core::RelationKey{42}).has_value(),"relation key lookup failed");
        require(mapped_g1.find_occurrence(core::OccurrenceKey{500}).has_value(),"occurrence key lookup failed");
        std::size_t edge_count=0;
        mapped_g1.for_each_relation_of_type(edge,[&](std::uint32_t){++edge_count;});
        require(edge_count==3u,"relation type index lookup failed");

        const auto head=sig.aspects().id("head");
        composite::BinaryProgram heads(2);
        heads.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,sig.existence_aspect()});
        heads.append({composite::BinaryOpcode::LoadAspect,1,0,0,0,head});
        heads.append({composite::BinaryOpcode::And,0,0,1,0,0});
        require(mapped.binary_count(core::Grade{1},heads,0)>=1,"mapped composite word-code failed");

        const auto source=numeric::SparseRationalVector::basis(complex.grade(core::Grade{1}).cell_count(),0,R{6});
        const auto in_memory=complex.boundary_apply_sparse(core::Grade{1},source);
        const auto on_disk=mapped.boundary_apply_sparse(core::Grade{1},source.view());
        require(same_sparse(in_memory,on_disk),"mapped boundary differs from in-memory boundary");

        const auto lower=numeric::SparseRationalVector::basis(complex.grade(core::Grade{0}).cell_count(),0,R{3});
        require(same_sparse(complex.transpose_apply_sparse(core::Grade{1},lower),
                            mapped.transpose_apply_sparse(core::Grade{1},lower.view())),
                "mapped transpose differs from in-memory transpose");

        const auto rebuilt=mapped.materialize();
        require(same_complex(complex,rebuilt),"RFCX exact source round trip failed");
        require(core::bianchi_through(rebuilt,core::Grade{2}).satisfied(),"round-trip tower failed Bianchi");

        // The mapped image is self-checking. Corrupt one payload byte and make
        // sure an integrity-checked open rejects it before any query executes.
        const auto corrupt=std::filesystem::temp_directory_path()/"rfc_test_snapshot_corrupt.rfcx";
        std::filesystem::copy_file(path,corrupt,std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream io(corrupt,std::ios::binary|std::ios::in|std::ios::out);
            io.seekg(128);
            char byte{}; io.read(&byte,1);
            byte=static_cast<char>(static_cast<unsigned char>(byte)^0x1u);
            io.seekp(128); io.write(&byte,1);
        }
        bool rejected=false;
        try { storage::MappedRfcx bad(corrupt,true); } catch (const std::exception&) { rejected=true; }
        require(rejected,"RFCX payload corruption was not rejected");
        std::filesystem::remove(corrupt);
        std::filesystem::remove(path);
        std::cout << "RFCX storage tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
