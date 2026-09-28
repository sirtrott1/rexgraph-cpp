#include "native_atlas_fixture.hpp"
#include "native_accession_fixture.hpp"
#include <iostream>
#include <random>
using namespace native_atlas_fixture;
namespace {
std::size_t checks{};
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
template<class F>void rejects(F&& fn,const char* message){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,message);}
void save(const std::filesystem::path& path,std::span<const std::byte> bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!out)throw std::runtime_error("fixture write");}
void rehash(std::vector<std::byte>& bytes){const auto hash=rfcx_detail::fnv1a64(std::span<const std::byte>(bytes).subspan(32));for(std::size_t i=0;i<8;++i)bytes[24+i]=static_cast<std::byte>((hash>>(8*i))&255u);}
void basics(const std::filesystem::path& out){
    auto a=atlas();const auto bytes=encode_native_atlas(a);save(out/"atlas.rfcxa",bytes);auto copy=decode_native_atlas(bytes);
    check(encode_native_atlas(copy)==bytes,"atlas canonical roundtrip");
    check(copy.sources().size()==3,"source manifest count");
    check(a.identified(NativeAtlasCell{2,cell(0,10)},NativeAtlasCell{3,cell(0,20)}),"declared shared identity");
    check(!a.identified(NativeAtlasCell{2,cell(0,12)},NativeAtlasCell{3,cell(0,22)}),"unrelated equal-valued cells merged");
    check(a.identified(NativeAtlasOccurrence{2,occ(1,1002)},NativeAtlasOccurrence{3,occ(1,2002)}),"inactive occurrence mapping lost");
    auto glued=a.glue(1);check(glued.source.grade(core::Grade{0}).cell_count()==4,"shared vertices double counted");
    check(glued.source.grade(core::Grade{1}).cell_count()==3,"shared relations double counted");
    check(glued.source.grade(core::Grade{1}).incidence_count()==9,"inactive/repeated legs lost");
    check(glued.source.grade_count()==3 && glued.source.grade(core::Grade{2}).cell_count()==0,"empty grade lost in gluing");
    check(glued.cells.at({2,cell(1,100)})==glued.cells.at({3,cell(1,200)}),"shared relation provenance");
    check(glued.occurrences.at({2,occ(1,1000)})!=glued.occurrences.at({2,occ(1,1002)}),"repeated participant occurrence collapsed");
    auto vals=a.glue_scalar_fields(1,99,core::Grade{0},5001,5001);
    check(vals.size()==4 && vals.at(glued.cells.at({2,cell(0,10)}))==Q{1},"glued scalar overlap was added");
    save(out/"glued.rfcx",encode_rfcx(glued.source).bytes);
    auto ss=sources();auto changed=endpoint(3);changed.tensors[0]=make_field(5001,501,{9,2,4});ss[2]=NativeAtlasSource(3,33,changed);
    NativeAtlas bad_values(ss,manifest());rejects([&]{bad_values.glue_scalar_fields(1,99,core::Grade{0},5001,5001);},"incompatible field silently glued");
    auto mutable_manifest=manifest();auto source_list=sources();NativeAtlas immutable(source_list,mutable_manifest);
    mutable_manifest.embeddings[0].cells.clear();check(immutable.glue(1).source.grade_count()==3,"manifest aliases caller mutation");
    auto field_view=a.access(2,NativeAccessionRequest::chain(5001,core::Grade{0}));auto source=a.source(2);
    check(field_view.endpoint()==source.endpoint(),"atlas accession duplicates endpoint");
    check(field_view.cell(10).source_schema()->aspects().size()==4,"schema-aware accession");
    NativeAccessionDirectory directory(endpoint(2),22);const auto handle=directory.intern(NativeAccessionRequest::chain(5001,core::Grade{0}));
    const auto pin=directory.resolve(handle);NativeAtlasSource from_directory(2,directory);
    check(from_directory.endpoint()==directory.pin_snapshot(),"atlas did not pin F3 source");
    auto next=endpoint(2);next.sequence=1;next.tensors[0]=make_field(5001,501,{1,2,30});directory.advance(make_temporal_segment(endpoint(2),next));
    check(*from_directory.access(NativeAccessionRequest::chain(5001,core::Grade{0})).state(12)==Q{3} && pin.state(12)==Q{3},"source pin silently rebound");
    rejects([&]{(void)directory.resolve(handle);},"stale F3 handle survived publication");
    auto image=encode_rfcx(endpoint(1).source).bytes;MappedRfcx mapped{std::span<const std::byte>(image)};auto gv=mapped.grade(core::Grade{1});
    image.assign(image.size(),std::byte{0});check(gv.relation_key(0).value==100,"embedded RFCX borrowed caller bytes");
    MappedRfcx moved(std::move(mapped));check(gv.relation_key(0).value==100,"embedded RFCX owner move");
    auto wrong=manifest();wrong.embeddings[1].declarations.reset();rejects([&]{NativeAtlas x(sources(),wrong);},"cross-schema embedding inferred meaning");
    wrong=manifest();wrong.embeddings[1].occurrences.pop_back();rejects([&]{NativeAtlas x(sources(),wrong);},"inactive occurrence omitted");
    wrong=manifest();wrong.embeddings[1].cells[1].to=wrong.embeddings[1].cells[0].to;rejects([&]{NativeAtlas x(sources(),wrong);},"noninjective embedding");
    wrong=manifest();wrong.embeddings[1].occurrences[1].to=wrong.embeddings[1].occurrences[0].to;rejects([&]{NativeAtlas x(sources(),wrong);},"occurrence identity merge");
    wrong=manifest();wrong.pushouts[0].left_common.reset();rejects([&]{NativeAtlas x(sources(),wrong);},"pushout without common declaration");
    wrong=manifest();wrong.pushouts[0].left_common->introduced[0].fill=true;rejects([&]{NativeAtlas x(sources(),wrong);},"incompatible introduced bits on overlap");
    ss=sources();ss.emplace_back(9,22,endpoint(2));rejects([&]{NativeAtlas x(ss,manifest());},"two endpoints in one source namespace");
    rejects([&]{a.access(2,NativeAccessionRequest::chain(999999,core::Grade{0}));},"unknown atlas accession accepted");
}
void interfaces(const std::filesystem::path& out){
    auto a=atlas();const auto r=a.linear_interface(1);
    check(r.compatible && r.operations_descend==true,"compatible linear restrictions/descent");
    check(r.restrictions==std::vector<numeric::RationalVector>{{Q{3}},{Q{3}}},"unequal carrier restrictions");
    check(a.finite_interface(1).compatible,"finite restrictions");
    auto m=manifest();m.linear_interfaces[0].legs[1].restriction.set(0,1,Q{1});NativeAtlas mismatch(sources(),m);
    check(!mismatch.linear_interface(1).compatible && mismatch.linear_interface(1).operations_descend==false,"field/operation obstructions erased");
    save(out/"obstruction.rfcxa",encode_native_atlas(mismatch));
    auto ss_scaled=sources();auto e_scaled=endpoint(3);e_scaled.tensors[1]=make_field(6001,601,{Q{3,2},7,9});ss_scaled[2]=NativeAtlasSource(3,33,e_scaled);
    auto scaled_manifest=manifest();scaled_manifest.linear_interfaces[0].legs[1].restriction.set(0,0,Q{2});
    NativeAtlas scaled(ss_scaled,scaled_manifest);check(scaled.linear_interface(1).compatible && scaled.linear_interface(1).operations_descend==true,"nonidentity interface coordinate bridge");
    save(out/"scaled-interface.rfcxa",encode_native_atlas(scaled));

    auto ss=sources();auto e=endpoint(3);auto op=temporal_tensor_matrix(e.tensors[2]);op.set(0,2,Q{5});e.tensors[2].tensor=temporal_matrix_tensor(op);
    // Set the affected direction to zero: current restricted values still agree,
    // but the complete operator must expose the nonzero descent defect.
    e.tensors[1]=make_field(6001,601,{3,7,0});ss[2]=NativeAtlasSource(3,33,e);NativeAtlas unused(ss,manifest());
    check(unused.linear_interface(1).compatible && unused.linear_interface(1).operations_descend==false,"unused operator direction lost");
    m=manifest();m.linear_interfaces[0].common_operation.reset();for(auto& leg:m.linear_interfaces[0].legs)leg.operation.reset();
    NativeAtlas uncertified(sources(),m);check(!uncertified.linear_interface(1).operations_descend.has_value(),"absent descent mistaken for proof");
    m=manifest();m.linear_interfaces[0].legs[0].operation.reset();rejects([&]{NativeAtlas x(sources(),m);},"partial descent declaration accepted");
    ss=sources();e=endpoint(3);e.declarations->values[0].value=composite::DeclaredValue::symbol(31);ss[2]=NativeAtlasSource(3,33,e);
    NativeAtlas finite_conflict(ss,manifest());check(!finite_conflict.finite_interface(1).compatible,"finite obstruction silently repaired");
    m=manifest();m.finite_interfaces[0].legs[0].restriction.pop_back();rejects([&]{NativeAtlas x(sources(),m);},"partial finite map accepted");
    m=manifest();m.finite_interfaces[0].legs[0].restriction[0].second=999;rejects([&]{NativeAtlas x(sources(),m);},"finite map outside target alphabet");
    ss=sources();e=endpoint(3);auto defs=e.declarations->registry.definitions();defs[0].axes[0].extent=2;
    e.declarations->registry=composite::DeclarationRegistry(defs);ss[2]=NativeAtlasSource(3,33,e);
    rejects([&]{NativeAtlas x(ss,manifest());},"conflicting immutable type identity across sources");
    const auto context=a.context(2,6020,{0},{0});check(context.sufficient(),"local row context not sufficient");
    check(context.evaluate({Q{11,3}})==numeric::RationalVector{Q{22,3}},"exact context evaluation");
    auto incomplete=a.context(2,6020,{0},{1});check(!incomplete.sufficient() && incomplete.report().missing_input==std::vector<std::size_t>{1},"missing exterior input not reported");
    rejects([&]{incomplete.evaluate({Q{2}});},"insufficient context executed");
    rejects([&]{context.evaluate({});},"wrong context input arity");
    rejects([&]{a.context(2,6020,{0,0},{0});},"duplicate context coordinate");
    // An explicitly stored global response exposes nonlocal row dependencies.
    ss=sources();e=endpoint(3);auto response=numeric::ExactMatrix::from_rows({{Q{5,8},Q{1,4},Q{1,8}},{Q{1,4},Q{1,2},Q{1,4}},{Q{1,8},Q{1,4},Q{5,8}}});
    e.tensors[2].tensor=temporal_matrix_tensor(response);ss[2]=NativeAtlasSource(3,33,e);m=manifest();m.linear_interfaces.clear();NativeAtlas global(ss,m);
    check(!global.context(3,6020,{0},{0}).sufficient(),"global Green row falsely certified local");
    check(global.context(3,6020,{0,1,2},{0}).evaluate({Q{0},Q{0},Q{8}})==numeric::RationalVector{Q{1}},"global response with complete context");
    std::ofstream log(out/"interfaces.tsv");for(const auto& v:r.restrictions)log<<v[0]<<'\n';
}
void products(const std::filesystem::path& out){
    auto a=atlas();auto p=a.product(1),reversed=a.product(2);const NativeProductCell cell_pair{cell(1,100),cell(1,200)};
    const auto terms=p.retained_boundary(cell_pair);check(terms.size()==6,"product lost inactive factor occurrences");
    std::size_t inactive=0,flips=0;for(const auto& term:terms){if(!term.exists)++inactive;if(term.koszul_flip)++flips;check(term.upper==cell_pair,"product upper provenance");}
    check(inactive==2 && flips==3,"existence and Koszul channels collapsed");
    NativeProductChain x{core::Grade{2},{{cell_pair,Q{3,7}}}};const auto dx=p.boundary(x);const auto ddx=p.boundary(dx);
    check(dx.values.size()==4 && ddx.values.size()==4,"product structural support shape");
    for(const auto& [c,q]:ddx.values){(void)c;check(q==0,"product Bianchi failure");}
    check(reversed.boundary(p.braid_to(reversed,x)).values==p.braid_to(reversed,dx).values,"graded braiding boundary law");
    check(reversed.braid_to(p,p.braid_to(reversed,x)).values==x.values,"braiding involution");
    auto zero=x;zero.values.begin()->second=0;check(p.boundary(zero).values.size()==4,"zero amplitude lost product support");
    rejects([&]{p.boundary(NativeProductChain{core::Grade{0},{}});},"product grade zero boundary");
    rejects([&]{p.retained_boundary({cell(1,99999),cell(0,20)});},"unknown product factor");
    rejects([&]{p.braid_to(p,x);},"unrelated product treated as reversed");
    auto independent=atlas();rejects([&]{p.braid_to(independent.product(2),x);},"unbound product publication accepted");
    auto m=manifest();m.products[0].left_factor=m.products[0].right_factor;rejects([&]{NativeAtlas y(sources(),m);},"equal factor identity accepted");
    std::ofstream stream(out/"products.tsv");stream<<"trial\tupper_left\tupper_right\tcoefficient\tlower_left_grade\tlower_left\tlower_right_grade\tlower_right\tresult\n";
    std::mt19937 rng(71043);
    for(unsigned trial=0;trial<128;++trial){NativeProductChain q{core::Grade{2},{}};
        for(std::uint64_t l=100;l<102;++l)for(std::uint64_t r=200;r<202;++r){Q v{static_cast<long>(rng()%11)-5,static_cast<unsigned long>(rng()%7+1)};v.canonicalize();q.values[{{core::Grade{1},l},{core::Grade{1},r}}]=v;}
        const auto d=p.boundary(q),dd=p.boundary(d);
        check(reversed.boundary(p.braid_to(reversed,q)).values==p.braid_to(reversed,d).values,"random graded braiding law");
        for(const auto& [c,v]:dd.values){(void)c;check(v==0,"random product Bianchi");}
        for(const auto& [c,v]:q.values)stream<<trial<<'\t'<<c.left.key<<'\t'<<c.right.key<<'\t'<<v<<"\t-1\t0\t0\t0\t0\n";
        for(const auto& [c,v]:d.values)stream<<trial<<"\t0\t0\t0\t"<<c.left.grade.value<<'\t'<<c.left.key<<'\t'<<c.right.grade.value<<'\t'<<c.right.key<<'\t'<<v<<'\n';
    }
    const auto pinned=[&]{auto temporary=atlas();return temporary.product(1);}();check(pinned.boundary(x).values==dx.values,"product factors dangle after atlas destruction");
}
void hierarchies(const std::filesystem::path& out){
    const auto ss=hierarchy_sources();const auto m=hierarchy_manifest();NativeAtlas a(ss,m);
    check(a.realize(1,core::Grade{1},{Q{7}})==numeric::RationalVector({Q{7},Q{7}}),"coarse edge did not realize as path");
    check(a.realize(1,core::Grade{1},{Q{7},Q{7}},true)==numeric::RationalVector({Q{7}}),"hierarchy retraction");
    check(a.realize(3,core::Grade{1},{Q{7}})==a.realize(2,core::Grade{1},a.realize(1,core::Grade{1},{Q{7}})),"hierarchy diagram");
    save(out/"hierarchy.rfcxa",encode_native_atlas(a));check(encode_native_atlas(decode_native_atlas(encode_native_atlas(a)))==encode_native_atlas(a),"hierarchy canonical roundtrip");
    auto bad=m;bad.scale_maps[0].realization.back().image[0].coefficient=2;rejects([&]{NativeAtlas b(ss,bad);},"non-chain semantic realization");
    bad=m;bad.scale_maps[0].from=5;bad.scale_maps[0].to=4;rejects([&]{NativeAtlas b(ss,bad);},"semantic direction confused with grade");
    bad=m;bad.scale_maps[0].realization.pop_back();rejects([&]{NativeAtlas b(ss,bad);},"missing hierarchy column");
    bad=m;bad.scale_maps[0].retraction->back().image[0].coefficient=2;rejects([&]{NativeAtlas b(ss,bad);},"invalid retraction");
    // Zero chain map is individually valid but disagrees with the two-step path.
    bad=m;for(auto& column:bad.scale_maps[2].realization)column.image.clear();rejects([&]{NativeAtlas b(ss,bad);},"incoherent hierarchy triangle");
    bad.scale_paths.clear();NativeAtlas unasserted(ss,bad);check(unasserted.realize(3,core::Grade{1},{Q{7}})==numeric::RationalVector({Q{0},Q{0}}),"zero chain map invalidated without diagram claim");
    auto next=*ss[1].endpoint();next.sequence=1;const auto segment=make_temporal_segment(*ss[1].endpoint(),next);
    auto updated=a.advanced({{5,segment}});check(updated.source(5).sequence()==1 && a.source(5).sequence()==0,"hierarchy snapshot update mutated old atlas");
    check(updated.realize(1,core::Grade{1},{Q{5}})==numeric::RationalVector({Q{5},Q{5}}),"hierarchy maps not revalidated on replay");
    save(out/"hierarchy-next.rfcxa",encode_native_atlas(updated));
    auto remapped=m;remapped.revision=2;
    for(auto& column:remapped.scale_maps[0].realization)for(auto& t:column.image)t.coefficient*=2;
    for(auto& column:*remapped.scale_maps[0].retraction)for(auto& t:column.image)t.coefficient/=2;
    for(auto& column:remapped.scale_maps[2].realization)for(auto& t:column.image)t.coefficient*=2;
    const auto blueprint=encode_native_atlas_manifest(remapped);save(out/"hierarchy-change.rfcxam",blueprint);
    const auto source_delta=encode_temporal_segment(segment);save(out/"hierarchy-change.rfcxt",source_delta);
    const auto base_disk=decode_native_atlas(encode_native_atlas(a));
    const auto mapped=base_disk.advanced({{5,decode_temporal_segment(source_delta)}},decode_native_atlas_manifest(blueprint));
    check(mapped.realize(1,core::Grade{1},{Q{7}})==numeric::RationalVector({Q{14},Q{14}}),"changed complete hierarchy map did not replay");
    check(mapped.realize(1,core::Grade{1},{Q{14},Q{14}},true)==numeric::RationalVector({Q{7}}),"changed inverse map did not replay");
    check(encode_native_atlas_manifest(decode_native_atlas_manifest(blueprint))==blueprint,"manifest canonical recoding");
    save(out/"hierarchy-remapped.rfcxa",encode_native_atlas(mapped));

}
void triples(){
    core::RelationalComplexBuilder builder(signature());builder.declare_witness_identity(10);builder.declare_witness_identity(11);builder.declare_grade(core::Grade{2});
    TemporalEndpoint endpoint0(builder.freeze());std::vector<NativeAtlasSource> ss{{1,111,endpoint0},{2,222,endpoint0},{3,333,endpoint0}};
    const auto e=[](std::uint64_t key,std::uint64_t from,std::uint64_t to,bool swap){NativeSourceEmbedding x;x.key=key;x.from=from;x.to=to;
        x.cells={{cell(0,10),cell(0,swap?11:10)},{cell(0,11),cell(0,swap?10:11)}};return x;};
    NativeAtlasManifest m;m.key=703;m.embeddings={e(1,1,2,false),e(2,2,3,false),e(3,1,3,false)};
    NativeAtlas a(ss,m);check(a.identified(NativeAtlasCell{1,cell(0,10)},NativeAtlasCell{3,cell(0,10)}),"coherent triple identity");
    m.embeddings[2]=e(3,1,3,true);rejects([&]{NativeAtlas b(ss,m);},"pairwise-valid inconsistent triple accepted");
}
void structural_edges(){
    // Raw stable key equality across source namespaces is not an interface.
    core::RelationalComplexBuilder empty(signature());empty.declare_grade(core::Grade{2});
    std::vector<NativeAtlasSource> ss{{1,11,TemporalEndpoint(empty.freeze())},{2,22,endpoint(2)},{3,33,endpoint(2)}};
    NativeAtlasManifest m;m.key=706;m.embeddings={{1,1,2,{}, {},{}},{2,1,3,{}, {},{}}};m.pushouts={{1,1,2,{},{}}};
    NativeAtlas disjoint(ss,m);const auto g=disjoint.glue(1);
    check(g.source.grade(core::Grade{0}).cell_count()==6 && g.source.grade(core::Grade{1}).cell_count()==4,"equal raw keys merged without an interface");
    check(g.cells.at({2,cell(0,10)})!=g.cells.at({3,cell(0,10)}),"source namespace erased by quotient allocator");
    // A declared attaching word cannot be reduced to its zero boundary matrix.
    const auto loop=[](bool shuffled,bool ordered){
        core::RelationSignature sig;const auto edge=sig.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
        const auto face=ordered?sig.add_attaching_loop_type("face",4,4):sig.add_type("face",core::BoundaryPolicy::DeclaredExact,4,4);
        core::RelationalComplexBuilder b(sig);b.declare_witness_identity(1);
        for(std::uint64_t i=0;i<2;++i)b.add_grade_one(core::RelationKey{10+i},edge,{{1,true,true,Q{1},{},core::OccurrenceKey{100+i*2}}, {1,true,false,Q{1},{},core::OccurrenceKey{101+i*2}}});
        std::vector<core::HigherIncidenceInput> legs{{0,true,false,Q{1},{},core::OccurrenceKey{200}}, {1,true,false,Q{1},{},core::OccurrenceKey{201}},
            {0,true,true,Q{1},{},core::OccurrenceKey{202}}, {1,true,true,Q{1},{},core::OccurrenceKey{203}}};
        if(shuffled)std::swap(legs[1],legs[2]);
        b.add_higher(core::Grade{2},core::RelationKey{20},face,legs);return b.freeze();
    };
    for(const bool ordered:{false,true}){
        std::vector<NativeAtlasSource> pair{{1,101,TemporalEndpoint(loop(false,ordered))},{2,102,TemporalEndpoint(loop(true,ordered))}};
        NativeSourceEmbedding e;e.key=1;e.from=1;e.to=2;e.cells={{cell(0,1),cell(0,1)},{cell(1,10),cell(1,10)},{cell(1,11),cell(1,11)},{cell(2,20),cell(2,20)}};
        for(std::uint64_t k=100;k<104;++k)e.occurrences.push_back({occ(1,k),occ(1,k)});
        for(std::uint64_t k=200;k<204;++k)e.occurrences.push_back({occ(2,k),occ(2,k)});
        NativeAtlasManifest plan;plan.key=707;plan.embeddings={e};
        if(ordered)rejects([&]{NativeAtlas a(pair,plan);},"ordered attaching word erased by embedding");
        else {NativeAtlas a(pair,plan);check(a.identified(NativeAtlasCell{1,cell(2,20)},NativeAtlasCell{2,cell(2,20)}),"unordered arena reorder rejected");}
    }
    // Occurrence-only cocycle failure: cell maps agree, two indistinguishable
    // inactive legs have distinct identities that cannot be swapped around a loop.
    core::RelationSignature sig;const auto type=sig.add_type("empty",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder b(sig);b.add_grade_one(core::RelationKey{10},type,{{1,false,false,Q{0},{},core::OccurrenceKey{100}}, {1,false,false,Q{0},{},core::OccurrenceKey{101}}});
    auto e0=TemporalEndpoint(b.freeze());ss={{1,11,e0},{2,22,e0},{3,33,e0}};m={};m.key=708;
    for(std::uint64_t i=0;i<3;++i){NativeSourceEmbedding e;e.key=i+1;e.from=i==1?2:1;e.to=i==0?2:3;
        e.cells={{cell(0,1),cell(0,1)},{cell(1,10),cell(1,10)}};
        e.occurrences={{occ(1,100),occ(1,i==2?101:100)},{occ(1,101),occ(1,i==2?100:101)}};m.embeddings.push_back(e);}
    rejects([&]{NativeAtlas a(ss,m);},"occurrence-only incoherent triple accepted");
    ss=sources();auto limited=endpoint(2);limited.tensors[1].tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(2),numeric::StructuralSupport::sparse(2,{1}),numeric::SparseRationalVector(2,{{1,Q{4}}}));
    ss[1]=NativeAtlasSource(2,22,limited);rejects([&]{NativeAtlas a(ss,manifest());},"restriction implicitly extended absent field support by zero");
}
void temporal_replay(const std::filesystem::path& out){
    auto a=atlas();const auto old_view=a.access(2,NativeAccessionRequest::chain(5001,core::Grade{0}));
    auto next=endpoint(2);next.sequence=1;next.tensors[0]=make_field(5001,501,{1,2,33});
    const auto segment=make_temporal_segment(endpoint(2),next);auto b=a.advanced({{2,segment}});
    check(a.manifest().revision==1 && b.manifest().revision==2,"atlas revision");
    check(old_view.state(12)==Q{3} && b.access(2,NativeAccessionRequest::chain(5001,core::Grade{0})).state(12)==Q{33},"temporal accession pin");
    check(b.glue_scalar_fields(1,99,core::Grade{0},5001,5001).size()==4,"updated scalar gluing");
    save(out/"atlas-next.rfcxa",encode_native_atlas(b));save(out/"one-segment.rfcxt",encode_temporal_segment(segment));
    rejects([&]{a.advanced({{2,segment},{2,segment}});},"duplicate source update accepted");
    auto target=manifest();target.revision=2;target.embeddings[0].cells.clear();rejects([&]{a.advanced({{2,segment}},target);},"late atlas failure published");
    check(a.source(2).sequence()==0 && old_view.state(12)==Q{3},"failed atlas update modified old publication");
    auto raw=encode_native_atlas(a);for(const auto width:{1u,7u,63u,64u,65u,127u,256u,4096u}){std::vector<std::byte> collected;
        for(std::size_t i=0;i<raw.size();i+=width)collected.insert(collected.end(),raw.begin()+static_cast<std::ptrdiff_t>(i),raw.begin()+static_cast<std::ptrdiff_t>(std::min(raw.size(),i+width)));
        check(encode_native_atlas(decode_native_atlas(collected))==raw,"chunked archive delivery changed bytes");}
    // An endpoint signature change must update the atlas embeddings explicitly.
    // Old manifestation is not silently reinterpreted in the new bit layout.
    auto recoded=endpoint(2);recoded.sequence=1;const auto recoding=migration(signature(),signature(true));
    recoded.source=recode_temporal_source(recoded.source,recoding);
    const auto recoding_segment=make_temporal_segment(endpoint(2),recoded,{}, {},RelationalCodeEncoding::Automatic,recoding);
    rejects([&]{a.advanced({{2,recoding_segment}});},"old atlas declaration maps silently rebound after signature migration");
    auto recoded_manifest=manifest();recoded_manifest.revision=2;
    recoded_manifest.embeddings[0].declarations=recoding;recoded_manifest.pushouts[0].left_common.reset();
    const auto recoded_atlas=a.advanced({{2,recoding_segment}},recoded_manifest);
    check(encode_rfcx(recoded_atlas.glue(1).source).bytes==encode_rfcx(a.glue(1).source).bytes,"equivalent common-signature pushout changed after exact recoding");
    check(a.source(2).endpoint()->source.signature().aspects().size()==4 && recoded_atlas.source(2).endpoint()->source.signature().aspects().size()==5,"signature migration changed pinned old source");
    const auto recoded_bytes=encode_native_atlas(recoded_atlas);save(out/"atlas-recoded.rfcxa",recoded_bytes);
    check(encode_native_atlas(decode_native_atlas(recoded_bytes))==recoded_bytes,"cross-signature atlas update disk reconstruction");
    // Existing E query plans travel with the source publication, not its buckets.
    auto e=native_accession_fixture::endpoint();NativeAccessionDirectory d(e,888,native_accession_fixture::queries());
    NativeAtlasManifest config;config.key=704;NativeAtlas observer({NativeAtlasSource(9,d)},config);
    NativeAccessionRequest query;query.kind=NativeAccessionKind::Observation;query.subject=1;query.secondary=10;
    const auto p=observer.access(9,query);check(p.observation()!=nullptr,"atlas observation pin");
    auto decoded=decode_native_atlas(encode_native_atlas(observer));check(decoded.source(9).queries().size()==5 && decoded.access(9,query).observation()!=nullptr,"E subscriptions not reconstructed");
    e.sequence=1;auto updated=observer.advanced({{9,make_temporal_segment(*observer.source(9).endpoint(),e)}});
    check(updated.source(9).queries().size()==5 && updated.access(9,query).observation()!=nullptr,"E plans lost on atlas update");
    save(out/"observations.rfcxa",encode_native_atlas(updated));
}
void malformed(const std::filesystem::path& out){
    auto a=atlas();const auto good=encode_native_atlas(a);std::size_t truncations=0;
    for(std::size_t size=0;size<good.size();++size){rejects([&]{decode_native_atlas(std::span<const std::byte>(good).first(size));},"truncated atlas accepted");++truncations;}
    const auto blueprint=encode_native_atlas_manifest(a.manifest());
    save(out/"atlas.rfcxam",blueprint);
    for(std::size_t size=0;size<blueprint.size();++size){rejects([&]{decode_native_atlas_manifest(std::span<const std::byte>(blueprint).first(size));},"truncated blueprint accepted");++truncations;}
    auto bad=good;bad.push_back(std::byte{0});rejects([&]{decode_native_atlas(bad);},"atlas trailing bytes");
    bad=good;bad[12]=std::byte{1};rejects([&]{decode_native_atlas(bad);},"unknown atlas feature bit");
    NativeAtlasLimits limits;limits.max_sources=2;rejects([&]{decode_native_atlas(good,limits);},"source decode budget");
    limits={};limits.max_records=5;rejects([&]{NativeAtlas x(sources(),manifest(),limits);},"atlas retained-record budget");
    limits={};limits.max_work=1;rejects([&]{NativeAtlas x(sources(),manifest(),limits);},"atlas work budget");
    limits={};limits.max_archive_bytes=good.size()-1;rejects([&]{decode_native_atlas(good,limits);},"atlas byte budget");
    limits={};limits.max_archive_bytes=good.size();check(encode_native_atlas(NativeAtlas(sources(),manifest(),limits))==good,"exact archive budget boundary");
    // Canonical reordering is rejected, rather than silently rewritten.
    bad=good;std::swap_ranges(bad.begin()+40,bad.begin()+48,bad.begin()+48);rehash(bad);
    rejects([&]{decode_native_atlas(bad);},"altered source identity escaped canonical binding");
    std::mt19937 rng(20260928);std::size_t accepted=0,rejected=0;
    for(unsigned i=0;i<500;++i){bad=good;const auto position=32u+static_cast<std::size_t>(rng()%(bad.size()-32));bad[position]^=std::byte{static_cast<unsigned char>(1u<<(rng()%8))};rehash(bad);
        std::optional<NativeAtlas> result;
        try{result=decode_native_atlas(bad);}catch(const std::exception&){++rejected;}
        if(result){check(encode_native_atlas(*result)==bad,"accepted mutation not canonical");++accepted;}}
    check(accepted+rejected==500,"mutation count");std::ofstream report(out/"mutation_counts.txt");report<<"truncations="<<truncations<<"\naccepted="<<accepted<<"\nrejected="<<rejected<<'\n';
}
}
int main(int argc,char** argv){try{
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"rfc-native-atlas-tests";
    std::filesystem::create_directories(out);
    basics(out);interfaces(out);products(out);hierarchies(out);triples();structural_edges();temporal_replay(out);malformed(out);
    std::cout<<"Native atlas F4: "<<checks<<" checks passed\n";
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
