#include "rfc/storage/temporal_seed.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    using namespace rfc;
    using namespace rfc::storage;
    using namespace rfc::composite;
    namespace fs=std::filesystem;
    try {
        if(argc!=2)throw std::invalid_argument("usage: rfc_declaration_temporal_demo FRESH_OUTPUT_DIRECTORY");
        const fs::path out(argv[1]);
        if(fs::exists(out))throw std::invalid_argument("output directory must be fresh");
        fs::create_directories(out);
        const auto id=[](std::uint64_t key,std::uint64_t v=1){return DeclarationId{900,key,v};};
        core::RelationSignature sig;
        const auto producer=sig.aspects().add("producer",AspectRole::RelationRole);
        const auto consumer=sig.aspects().add("consumer",AspectRole::RelationRole);
        const auto type=sig.add_type("transfer");
        core::RelationalComplexBuilder builder(sig);
        builder.declare_grade(core::Grade{2});
        builder.add_grade_one(core::RelationKey{7},type,{
            {10,true,false,numeric::Rational{0},{{producer,true},{consumer,false}},core::OccurrenceKey{100}},
            {11,true,false,numeric::Rational{0},{{producer,false},{consumer,true}},core::OccurrenceKey{101}},
            {11,false,false,numeric::Rational{0},{{producer,false},{consumer,true}},core::OccurrenceKey{102}}});
        TemporalEndpoint base(builder.freeze());
        TemporalDeclarationState state;state.namespace_key=900;
        state.registry=DeclarationRegistry({Declaration::boolean(id(1)),Declaration::finite(id(2),{10,20,30}),
            Declaration::rational(id(3)),Declaration::product(id(4),{{1,id(2),0},{2,id(3),0}}),
            Declaration::product(id(5),{{100,id(1),0},{200,id(1),0}})});
        TemporalDeclaredValue value;value.key=1;value.type=id(4);value.branch=1;
        value.cell=StableCellKey{core::Grade{1},7};
        value.value=DeclaredValue::product({DeclaredValue::symbol(20),DeclaredValue::rational(numeric::Rational{1,3})});
        state.values={value};
        state.branches={{1,"transfer",{id(6),id(4),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
                                      {{100,"producer",1,1},{200,"consumer",2,2}}}}};
        state.source_layouts={{1,id(5),{{100,"producer"},{200,"consumer"}}}};
        TemporalSourceLayoutProgram program{1,1,BinaryProgram(1),0};
        program.program.append({BinaryOpcode::LoadAspect,0,0,0,0,0});
        state.programs={program};base.declarations=state;
        base.carriers={{10,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(1)}};
        TemporalTensor field;field.key=1;field.role=TemporalTensorRole::Field;field.output=10;
        field.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{7}}}));
        base.tensors={field};validate_temporal_endpoint(base);

        // New source positions/defaults do not reinterpret old values.
        AspectSchema aspects;aspects.add("receiver",AspectRole::RelationRole);
        aspects.add("sign",AspectRole::Orientation,true);aspects.add("present",AspectRole::Existence,false);
        aspects.add("sender",AspectRole::RelationRole);aspects.add("reviewed",AspectRole::Provenance,true);
        core::RelationSignature target_signature(aspects);target_signature.add_type("reserved");target_signature.add_type("transfer-v2");
        SourceDeclarationMigration migration{sig,target_signature,
            {{"exists","present"},{"orientation","sign"},{"producer","sender"},{"consumer","receiver"}},
            {{"transfer","transfer-v2"}},{{"reviewed",false,{}}}};
        auto target=base;target.sequence=1;target.source=recode_temporal_source(base.source,migration);
        auto& next=*target.declarations;
        next.registry=DeclarationRegistry({Declaration::boolean(id(1,2)),Declaration::finite(id(2,2),{30,20,10,40}),
            Declaration::rational(id(3,2)),Declaration::product(id(4,2),{{2,id(3,2),0},{1,id(2,2),0},{3,id(1,2),0}}),
            Declaration::product(id(5,2),{{200,id(1,2),0},{100,id(1,2),0}})});
        const std::vector<DeclarationMap> maps{
            {id(1),id(1,2),DeclarationMapKind::Recode,{}},
            {id(2),id(2,2),DeclarationMapKind::Embedding,{}},
            {id(3),id(3,2),DeclarationMapKind::Recode,{}},
            {id(4),id(4,2),DeclarationMapKind::Embedding,{{3,DeclaredValue::boolean(false)}}},
            {id(5),id(5,2),DeclarationMapKind::Recode,{}}};
        const DeclarationRecoding recoding(state.registry,next.registry,maps);
        next.values[0].value=recoding.apply(value.type,value.value);next.values[0].type=id(4,2);
        next.branches={{1,"transfer-v2",{id(6,2),id(4,2),core::BranchPopulation::Retained,core::BranchSemantics::Generic,true,
                                       {{100,"sender",1,1},{200,"receiver",2,2}}}}};
        next.source_layouts={{1,id(5,2),{{100,"sender"},{200,"receiver"}}}};
        next.programs={recode_declared_program(state,next,program,1,maps)};
        auto update=make_temporal_declaration_update(base.declarations,target.declarations,maps);
        const auto segment=make_temporal_segment(base,target,{}, {},RelationalCodeEncoding::Automatic,migration,update);
        write_rfcx(base.source,out/"base.rfcx");
        const auto sidecar_bytes=write_temporal_endpoint(base,out/"base.rfcxe");
        const auto segment_bytes=append_temporal_segment(out/"history.rfcxt",base,segment);
        // Replay depends only on the files; the application does not reconstruct declarations.
        auto replay=read_temporal_endpoint(MappedRfcx(out/"base.rfcx"),out/"base.rfcxe");
        visit_temporal_segments(out/"history.rfcxt",[&](const auto& s,auto){replay=replay_temporal_segment(replay,s);});
        if(encode_temporal_endpoint(replay)!=encode_temporal_endpoint(target) ||
           encode_rfcx(replay.source).bytes!=encode_rfcx(target.source).bytes)
            throw std::runtime_error("disk replay differs from target");
        const auto old_bits=execute_declared_source_program(base.source,state,1,core::Grade{1});
        const auto new_bits=execute_declared_source_program(replay.source,*replay.declarations,1,core::Grade{1});
        if(old_bits.words()!=new_bits.words())throw std::runtime_error("source program recoding changed output");
        if(!segment.tensors.empty() || !update.replaced_values.empty())throw std::runtime_error("recoding invented a value/field edit");
        std::cout<<"source_aspects="<<sig.aspects().size()<<"->"<<target_signature.aspects().size()
                 <<" declarations="<<next.registry.definitions().size()<<" retained_occurrences="<<replay.source.grade(core::Grade{1}).incidence_count()
                 <<" value_replacements="<<update.replaced_values.size()<<" field_updates="<<segment.tensors.size()
                 <<" sidecar_bytes="<<sidecar_bytes<<" segment_bytes="<<segment_bytes
                 <<" rebound_program=true exact_disk_replay=true\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
