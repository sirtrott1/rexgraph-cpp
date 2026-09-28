#include "rfc/storage/native_accession.hpp"
#include <filesystem>
#include <iostream>

int main(int argc,char** argv) {
    using namespace rfc;
    using namespace rfc::storage;
    using Q=numeric::Rational;
    try {
        if(argc!=2)throw std::invalid_argument("usage: rfc_native_accession_demo FRESH_OUTPUT_DIRECTORY");
        const std::filesystem::path out(argv[1]);
        if(std::filesystem::exists(out))throw std::invalid_argument("output directory must be fresh");
        std::filesystem::create_directories(out);
        const auto source=[](bool flip) {
            core::RelationSignature sig;const auto type=sig.add_type("branch",core::BoundaryPolicy::DeclaredExact,3,3);
            core::RelationalComplexBuilder b(sig);
            b.add_grade_one(core::RelationKey{7},type,{
                {10,true,flip,Q{1},{},core::OccurrenceKey{100}},
                {11,true,true,Q{1},{},core::OccurrenceKey{101}},
                {11,false,false,Q{0},{},core::OccurrenceKey{102}}});
            return b.freeze();
        };
        TemporalEndpoint base(source(false));
        base.carriers={{1,TemporalCarrierKind::SourceGrade,core::Grade{1},{},0,numeric::IndexDomain::linear(1)},
                       {2,TemporalCarrierKind::SourceGrade,core::Grade{0},{},0,numeric::IndexDomain::linear(2)}};
        TemporalTensor x;x.key=1;x.role=TemporalTensorRole::Field;x.output=1;
        x.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{3}}}));
        TemporalTensor f;f.key=2;f.role=TemporalTensorRole::Field;f.output=2;
        f.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(2),numeric::StructuralSupport::full(2),numeric::SparseRationalVector(2,{{0,Q{2}},{1,Q{5}}}));
        base.tensors={x,f};
        const composite::DeclarationId bit{900,1,1},product{900,2,1};
        TemporalDeclarationState declarations;declarations.namespace_key=900;
        declarations.registry=composite::DeclarationRegistry({composite::Declaration::boolean(bit),
            composite::Declaration::product(product,{{100,bit,0},{200,bit,0}})});
        declarations.source_layouts={{1,product,{{100,"exists"},{200,"orientation"}}}};
        TemporalDeclaredValue value;value.key=1;value.type=bit;value.cell=StableCellKey{core::Grade{1},7};
        value.value=composite::DeclaredValue::boolean(true);declarations.values={value};base.declarations=declarations;
        auto next=base;next.sequence=1;next.source=source(true);
        next.tensors[0].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,Q{4}}}));
        const auto segment=make_temporal_segment(base,next);
        write_rfcx(base.source,out/"base.rfcx");write_temporal_endpoint(base,out/"base.rfcxe");
        append_temporal_segment(out/"history.rfcxt",base,segment);
        auto disk=read_temporal_endpoint(MappedRfcx(out/"base.rfcx"),out/"base.rfcxe");
        // Deliberately collide every descriptor. Full key comparison still
        // separates chain, cochain and source-bound Boolean content.
        NativeAccessionDirectory directory(disk,42,{},[](auto){return 0;});
        auto chain_request=NativeAccessionRequest::chain(1,core::Grade{1});
        auto cochain_request=NativeAccessionRequest::cochain(2,core::Grade{0});
        const auto chain_handle=directory.intern(chain_request);
        const auto cochain_handle=directory.intern(cochain_request);
        NativeAccessionRequest bits;bits.kind=NativeAccessionKind::SourceAspects;bits.subject=1;
        bits.occurrence=StableOccurrenceKey{core::Grade{1},core::OccurrenceKey{100}};
        const auto bit_handle=directory.intern(bits);
        const auto old_chain=directory.resolve(chain_handle),old_cochain=directory.resolve(cochain_handle),old_bits=directory.resolve(bit_handle);
        const auto paired=old_cochain.pair_with(old_chain.boundary());
        if(paired!=old_cochain.coboundary().pair_with(old_chain))throw std::runtime_error("exact pairing law failed");
        visit_temporal_segments(out/"history.rfcxt",[&](const auto& s,auto){directory.advance(s);});
        const auto current=directory.resolve(directory.find(chain_request).value());
        const auto current_bits=directory.resolve(directory.find(bits).value());
        bool stale_rejected=false;
        try{(void)directory.resolve(chain_handle);}catch(const std::invalid_argument&){stale_rejected=true;}
        if(!stale_rejected || old_chain.state(7)!=Q{3} || current.state(7)!=Q{4} ||
           old_bits.source_bit(200) || !current_bits.source_bit(200))throw std::runtime_error("snapshot accession mismatch");
        const auto pin=directory.pin_snapshot();
        if(encode_temporal_endpoint(*pin)!=encode_temporal_endpoint(next) || encode_rfcx(pin->source).bytes!=encode_rfcx(next.source).bytes)
            throw std::runtime_error("disk endpoint mismatch");
        const auto before=directory.stats();const auto handle=directory.find(chain_request).value();const auto hot_start=directory.stats();
        for(std::size_t i=0;i<10000;++i)(void)directory.resolve(handle);
        const auto after=directory.stats();
        if(after.cold_bindings!=before.cold_bindings || after.descriptor_hashes!=hot_start.descriptor_hashes)
            throw std::runtime_error("hot lookup rebuilt a key or binding");
        std::cout<<"retained_occurrences="<<current.cell(7).occurrence_count()
                 <<" declared_values="<<current.cell(7).declared_values().size()
                 <<" pairing="<<paired<<" pinned_field="<<*old_chain.state(7)<<"->"<<*current.state(7)
                 <<" hot_resolutions="<<after.handle_resolutions-hot_start.handle_resolutions
                 <<" stale_rejected=true collision_safe=true exact_disk_replay=true\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
