#include "rfc/storage/native_atlas_codec.hpp"
#include <iostream>

int main(int argc,char** argv){
    using namespace rfc;using namespace rfc::storage;using Q=numeric::Rational;
    try{
        if(argc!=2)throw std::invalid_argument("usage: rfc_native_atlas_demo FRESH_OUTPUT_DIRECTORY");
        const std::filesystem::path out(argv[1]);if(std::filesystem::exists(out))throw std::invalid_argument("output directory exists");
        std::filesystem::create_directories(out);
        core::RelationSignature sig;const auto edge=sig.add_type("path-edge",core::BoundaryPolicy::DeclaredExact,2,2);
        core::RelationalComplexBuilder k(sig);k.declare_witness_identity(2);
        core::RelationalComplexBuilder a(sig),b(sig);
        a.add_grade_one(core::RelationKey{10},edge,{{1,true,true,Q{1},{},core::OccurrenceKey{101}}, {2,true,false,Q{1},{},core::OccurrenceKey{102}}});
        b.add_grade_one(core::RelationKey{20},edge,{{20,true,true,Q{1},{},core::OccurrenceKey{201}}, {3,true,false,Q{1},{},core::OccurrenceKey{202}}});
        NativeAtlasManifest plan;plan.key=100;plan.revision=1;
        plan.embeddings={{1,1,2,{},{{{core::Grade{0},2},{core::Grade{0},2}}},{}},
                         {2,1,3,{},{{{core::Grade{0},2},{core::Grade{0},20}}},{}}};
        plan.pushouts={{1,1,2,{},{}}};plan.products={{1,2,3,71,72}};
        NativeAtlas atlas({NativeAtlasSource(1,11,TemporalEndpoint(k.freeze())),NativeAtlasSource(2,22,TemporalEndpoint(a.freeze())),
                           NativeAtlasSource(3,33,TemporalEndpoint(b.freeze()))},plan);
        write_native_atlas(atlas,out/"base.rfcxa");const auto disk=read_native_atlas(out/"base.rfcxa");
        const auto glued=disk.glue(1);const auto product=disk.product(1);
        const NativeProductChain x{core::Grade{2},{{{{core::Grade{1},10},{core::Grade{1},20}},Q{1}}}};
        const auto dx=product.boundary(x),ddx=product.boundary(dx);bool zero=true;for(const auto& [c,q]:ddx.values){(void)c;if(q!=0)zero=false;}
        const bool exact=encode_native_atlas(disk)==encode_native_atlas(atlas);
        if(!zero || !exact)throw std::runtime_error("exact atlas replay/product check failed");
        std::cout<<"sources="<<disk.sources().size()<<" glued_vertices="<<glued.source.grade(core::Grade{0}).cell_count()
                 <<" glued_edges="<<glued.source.grade(core::Grade{1}).cell_count()<<" product_routes="<<product.retained_boundary(x.values.begin()->first).size()
                 <<" product_boundary_squared_zero="<<std::boolalpha<<zero<<" exact_disk_replay="<<exact<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
