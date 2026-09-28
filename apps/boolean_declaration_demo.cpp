#include "rfc/storage/declaration_codec.hpp"
#include "rfc/core/declaration_binding.hpp"
#include "rfc/core/builder.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using namespace rfc;
    using namespace rfc::composite;
    try {
        if (argc != 2) throw std::invalid_argument("usage: rfc_boolean_declaration_demo FRESH_OUTPUT_DIRECTORY");
        const fs::path out(argv[1]);
        if (fs::exists(out)) throw std::invalid_argument("output directory must be fresh");
        fs::create_directories(out);
        auto id=[](std::uint64_t key){return DeclarationId{100,key,1};};
        auto declarations=DeclarationRegistry({
            Declaration::finite(id(1), {10,20,30,40,50,60}),
            Declaration::rational(id(2)), Declaration::optional(id(3),id(2)),
            Declaration::boolean(id(4)), Declaration::repeated(id(5),id(4),0,10000),
            Declaration::product(id(6),{{1,id(1),0},{2,id(3),0},{3,id(5),0}})
        });
        CompiledLayout layout(declarations,id(6));
        std::vector<DeclaredValue> values = {
            DeclaredValue::product({DeclaredValue::symbol(30),DeclaredValue::absent(),
                DeclaredValue::repeated({DeclaredValue::boolean(true),DeclaredValue::boolean(false)})}),
            DeclaredValue::product({DeclaredValue::symbol(50),DeclaredValue::present(DeclaredValue::rational(numeric::rational(1,3))),
                DeclaredValue::repeated(std::vector<DeclaredValue>(129,DeclaredValue::boolean(true)))})
        };
        core::RelationSignature signature;
        const auto left=signature.aspects().add("left",AspectRole::RelationRole);
        const auto right=signature.aspects().add("right",AspectRole::RelationRole);
        const auto type=signature.add_type("collective");
        core::RelationalComplexBuilder builder(signature);
        std::vector<core::BoundaryOccurrenceInput> occurrences;
        for (std::size_t i=0;i<5;++i) occurrences.push_back({10+i%2,true,false,numeric::Rational{0},{{left,i<2},{right,i>=2}},core::OccurrenceKey{100+i}});
        const auto relation=builder.add_grade_one(core::RelationKey{7},type,occurrences);
        const auto source=builder.freeze();
        core::BranchDeclaration branch{id(7),id(6),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
                                      {{1,"left",2,2},{2,"right",3,3}}};
        const core::BranchDeclarationBinding binding(signature,type,branch,layout);
        const auto counts=binding.validate(source,relation,values[0]);
        const auto bytes=storage::encode_declared_archive(layout,values);
        {
            std::ofstream file(out/"values.rfcxv",std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            if (!file) throw std::runtime_error("archive write failed");
        }
        std::ifstream file(out/"values.rfcxv",std::ios::binary);file.seekg(0,std::ios::end);const auto size=file.tellg();
        if(size<0)throw std::runtime_error("archive read failed");
        std::vector<std::byte> loaded(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(loaded.data()),size);
        if(!file)throw std::runtime_error("archive read failed");
        const auto replay=storage::decode_declared_archive(loaded);
        if(bytes!=storage::encode_declared_archive(replay.layout,replay.values) || !declared_value_equal(values[1],replay.values.at(1)))
            throw std::runtime_error("archive replay mismatch");
        std::cout<<"declarations="<<replay.layout.declarations().definitions().size()<<" values="<<replay.values.size()
                 <<" branch_roles="<<counts[0]<<','<<counts[1]<<" retained_occurrences="<<source.grade(core::Grade{1}).incidence_count()
                 <<" archive_bytes="<<bytes.size()<<" exact_value_disk_replay=true\n";
        // The branch validator remains explicit in-memory configuration in F1.
        // This archive is not a source-bound temporal endpoint or migration record.
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
