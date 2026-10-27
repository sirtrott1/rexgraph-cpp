#include "compact_tensor_fixture.hpp"
#include <iostream>
#include <random>
using namespace compact_fixture;
using namespace rfc::storage;
namespace {
constexpr auto v1=BooleanLayoutProfile::Lsb0OctetsV1;
constexpr auto v2=BooleanLayoutProfile::Lsb0OctetsV2;
std::size_t checks{}, accepted_mutations{}, rejected_mutations{};
void check(bool x,const char* m){++checks;if(!x)throw std::runtime_error(m);}
template<class F> void rejects(F&& f,const char* m){bool b=false;try{f();}catch(const std::exception&){b=true;}check(b,m);}
bool equal(const V& a,const V& b){return declared_value_equal(a,b);}
numeric::Rational value_at(const numeric::SparseRationalVector& v,std::size_t i){
    for(const auto& e:v.entries())if(e.index==i)return e.value;
    return 0;
}
void roundtrip(const CompiledLayout& l,const V& x){
    const auto a=l.allocation(x);const auto b=encode_declared_bits(l,x);
    check(a.bits==b.bit_count && a.bytes==b.bytes.size(),"preflight bit size");
    const auto y=decode_declared_bits(l,b);check(equal(x,y),"value roundtrip");
    check(encode_declared_bits(l,y)==b,"canonical payload");
    machine::BitReader in(b.bytes,b.bit_count,l.limits().max_encoded_bytes);
    declaration_codec_detail::DecodeBudget use{l.limits()};
    (void)declaration_codec_detail::get_value(in,l,l.root(),use);in.finish();
    check(a.value_nodes==use.nodes && a.materialized_bytes==use.allocated,"preflight decode resource agreement");
    const auto wire=encode_declared_archive(l,std::span(&x,1));
    const auto out=decode_declared_archive(wire,l.limits());
    check(out.layout.profile()==l.profile() && out.layout.declarations().definitions()==l.declarations().definitions(),"archive lost declaration/profile");
    check(equal(x,out.values.at(0)) && encode_declared_archive(out.layout,out.values)==wire,"archive roundtrip");
}
machine::EncodedBits malformed(std::size_t tag,std::vector<std::size_t> support,
                               std::vector<numeric::SparseRationalEntry> entries){
    std::size_t bits=16;
    if(tag==0){bits+=8;for(auto i:support)bits+=8*declaration_detail::varuint_bytes(i);}
    for(const auto& e:entries)bits+=8*declaration_detail::varuint_bytes(e.index)+declaration_detail::rational_bits(e.value,{});
    machine::BitWriter w(bits,1024);w.put(tag,8);
    if(tag==0){declaration_codec_detail::put_var(w,support.size());for(auto x:support)declaration_codec_detail::put_var(w,x);}
    declaration_codec_detail::put_var(w,entries.size());
    for(const auto& e:entries){declaration_codec_detail::put_var(w,e.index);declaration_codec_detail::put_rational(w,e.value);}
    return std::move(w).finish();
}
void golden(const std::filesystem::path& old,const std::filesystem::path& output){
    if(!output.empty())std::filesystem::create_directories(output);
    for(const auto& name:{"full","partial","zero","nested","empty","large"}){
        const std::string n=name;const std::size_t size=n=="empty"?0:n=="large"?1000003:3;
        const auto x=n=="nested"?nested():tensor(size,n!="partial",n=="zero");
        const auto root=id(n=="nested"?6:2);const CompiledLayout old_l(registry(size),root),new_l(registry(size),root,v2);
        const auto old_wire=load(old/(n+".rfcxv"));const auto old_raw=load(old/(n+".bits"));std::size_t bitcount;
        std::ifstream(old/(n+".count"))>>bitcount;
        check(encode_declared_archive(old_l,std::span(&x,1))==old_wire,"V1 golden archive changed");
        const auto old_bits=encode_declared_bits(old_l,x);
        check(old_bits.bytes==old_raw && old_bits.bit_count==bitcount,"V1 golden raw changed");
        const auto recovered=decode_declared_archive(old_wire);
        check(equal(recovered.values.at(0),x) && recovered.layout.profile()==v1,"old V1 golden decode");
        check(encode_declared_archive(recovered.layout,recovered.values)==old_wire,"V1 rewrite silently upgraded");
        roundtrip(new_l,x);
        const auto raw=encode_declared_bits(new_l,x);const auto wire=encode_declared_archive(new_l,std::span(&x,1));
        if(!output.empty()){
            save(output/(n+".bits"),raw.bytes);std::ofstream(output/(n+".count"))<<raw.bit_count<<'\n';save(output/(n+".rfcxv"),wire);
        }
        if(n=="large"){
            check(raw.bytes.size()==15,"unexpected compact large payload size");
            check(old_raw.size()==2983514,"baseline size changed");
            const auto y=decode_declared_bits(new_l,raw);
            const auto& t=std::get<numeric::SparseRationalTensor>(y.data);
            check(t.support().storage()==numeric::StructuralSupport::Storage::Full && t.support().count()==1000003,"full support not symbolic");
            check(t.support().contains(500000) && value_at(t.values(),500000)==0,"supported zero lost");
            std::cout<<"large V1_payload="<<old_raw.size()<<" V2_payload="<<raw.bytes.size()<<" V1_archive="<<old_wire.size()<<" V2_archive="<<wire.size()<<" V2_bits="<<raw.bit_count<<'\n';
        }
    }
}
void boundaries(){
    const auto reg=registry();const CompiledLayout l(reg,id(2),v2);
    const auto x=tensor();const auto raw=encode_declared_bits(l,x);const auto allocation=l.allocation(x);
    auto policy=l.limits();policy.max_value_nodes=allocation.value_nodes;policy.max_materialized_bytes=allocation.materialized_bytes;policy.max_encoded_bytes=allocation.bytes;
    const CompiledLayout exact(reg,id(2),v2,policy);check(encode_declared_bits(exact,x)==raw && equal(decode_declared_bits(exact,raw),x),"exact payload budgets");
    --policy.max_value_nodes;const CompiledLayout node_limit(reg,id(2),v2,policy);
    rejects([&]{(void)encode_declared_bits(node_limit,x);},"write record budget ignored");rejects([&]{(void)decode_declared_bits(node_limit,raw);},"read record budget ignored");
    policy=exact.limits();--policy.max_materialized_bytes;const CompiledLayout mem_limit(reg,id(2),v2,policy);
    rejects([&]{(void)encode_declared_bits(mem_limit,x);},"write allocation budget ignored");rejects([&]{(void)decode_declared_bits(mem_limit,raw);},"read allocation budget ignored");
    policy=exact.limits();--policy.max_encoded_bytes;const CompiledLayout byte_limit(reg,id(2),v2,policy);
    rejects([&]{(void)encode_declared_bits(byte_limit,x);},"write byte budget ignored");rejects([&]{(void)decode_declared_bits(byte_limit,raw);},"read byte budget ignored");
    policy=l.limits();policy.max_integer_bits=2;const CompiledLayout int_limit(reg,id(2),v2,policy);
    rejects([&]{(void)encode_declared_bits(int_limit,x);},"write integer budget ignored");rejects([&]{(void)decode_declared_bits(int_limit,raw);},"read integer budget ignored");
    const auto multi=encode_declared_archive(l,std::vector{x,x});policy=l.limits();policy.max_value_nodes=allocation.value_nodes;
    rejects([&]{(void)decode_declared_archive(multi,policy);},"aggregate read records ignored");
    rejects([&]{(void)encode_declared_archive(CompiledLayout(reg,id(2),v2,policy),std::vector{x,x});},"aggregate write records ignored");
    policy=l.limits();policy.max_materialized_bytes=sizeof(V)*2+allocation.materialized_bytes;
    rejects([&]{(void)decode_declared_archive(multi,policy);},"aggregate allocation ignored");
    policy=l.limits();policy.max_definitions=1;rejects([&]{(void)decode_declared_archive(multi,policy);},"definition budget ignored");
    const CompiledLayout nl(reg,id(6),v2);const auto nb=encode_declared_bits(nl,nested());policy=l.limits();policy.max_depth=2;
    rejects([&]{(void)CompiledLayout(reg,id(6),v2,policy);},"depth ignored");
    for(std::size_t n=0;n<raw.bit_count;++n){auto b=raw.bytes;b.resize(machine::ceil_div(n,8));if(n%8)b.back()&=std::byte((1u<<(n%8))-1u);rejects([&]{(void)decode_declared_bits(l,b,n);},"bit truncation accepted");}
    auto padded=raw;padded.bytes.back()|=std::byte{128};rejects([&]{(void)decode_declared_bits(l,padded);},"nonzero padding accepted");
    auto trailing=raw;trailing.bytes.push_back(std::byte{0});rejects([&]{(void)decode_declared_bits(l,trailing);},"trailing byte accepted");
    for(std::size_t n=0;n<multi.size();++n)rejects([&]{(void)decode_declared_archive(std::span(multi).first(n));},"archive truncation accepted");
    for(const auto tag:{2,255})rejects([&]{(void)decode_declared_bits(l,malformed(tag,{},{}));},"reserved tag accepted");
    for(const auto& support:std::vector<std::vector<std::size_t>>{{0,0},{2,0},{3},{0,1,2}})
        rejects([&]{(void)decode_declared_bits(l,malformed(0,support,{}));},"bad support accepted");
    for(const auto& entries:std::vector<std::vector<numeric::SparseRationalEntry>>{{{3,1}},{{0,1},{0,2}},{{2,1},{0,2}},{{1,0}}})
        rejects([&]{(void)decode_declared_bits(l,malformed(1,{},entries));},"bad nonzero accepted");
    rejects([&]{(void)decode_declared_bits(l,malformed(0,{0},{{2,1}}));},"nonzero outside partial support");
    // Same numeric zero, different support is preserved.
    const auto full_zero=tensor(3,true,true);const auto partial_zero=tensor(3,false,true);
    check(!equal(full_zero,partial_zero),"support collapsed to numerical nonzeros");
    check(encode_declared_bits(l,full_zero)!=encode_declared_bits(l,partial_zero),"support payload collision");
    // Full storage variants all have one canonical V2 encoding, including empty.
    const auto& orig=std::get<numeric::SparseRationalTensor>(x.data);
    for(const auto& support:{numeric::StructuralSupport::sparse(3,{0,1,2}),numeric::StructuralSupport::packed(3,{0,1,2})}){
        const auto v=V::tensor(numeric::SparseRationalTensor(orig.domain(),support,orig.values()));
        check(encode_declared_bits(l,v)==raw,"full storage variant not canonical");roundtrip(l,v);
    }
    roundtrip(l,V::tensor(numeric::SparseRationalTensor(orig.domain(),numeric::StructuralSupport::packed(3,{0,2}),orig.values())));
    roundtrip(l,V::tensor(numeric::SparseRationalTensor(orig.domain(),numeric::StructuralSupport::sparse(3,{}),numeric::SparseRationalVector(3))));
    const CompiledLayout empty(registry(0),id(2),v2);roundtrip(empty,tensor(0,false));
    rejects([&]{(void)decode_declared_bits(empty,malformed(0,{},{}));},"empty domain not canonical full");
    // Nonminimal counts and exact rational encodings.
    machine::BitWriter w(24,3);w.put(1,8);w.put(128,8);w.put(0,8);const auto overlong=std::move(w).finish();
    rejects([&]{(void)decode_declared_bits(l,overlong);},"nonminimal count accepted");
    for(int bad=0;bad<4;++bad){machine::BitWriter z(bad<2?49:bad==2?57:65,9);z.put(1,8);z.put(1,8);z.put(0,8);
        if(bad==0){z.put(1,1);z.put(0,8);declaration_codec_detail::put_magnitude(z,1);} // negative zero
        if(bad==1){declaration_codec_detail::put_integer(z,1);declaration_codec_detail::put_magnitude(z,0);}
        if(bad==2){declaration_codec_detail::put_integer(z,2);declaration_codec_detail::put_magnitude(z,4);}
        if(bad==3){z.put(0,1);z.put(2,8);z.put(1,8);z.put(0,8);declaration_codec_detail::put_magnitude(z,1);}
        const auto badq=std::move(z).finish();rejects([&]{(void)decode_declared_bits(l,badq);},"bad rational accepted");}
    auto defs=encode_declarations(l);defs[8]=std::byte{3};rejects([&]{(void)decode_declarations(defs);},"unknown layout accepted");
    rejects([&]{(void)CompiledLayout(reg,id(2),static_cast<BooleanLayoutProfile>(3));},"unknown profile compiled");
    check(CompiledLayout(reg,id(2)).profile()==v1,"default upgraded");
    // Raw payloads are not self describing; these tensor examples fail under the wrong supplied layout.
    rejects([&]{(void)decode_declared_bits(CompiledLayout(reg,id(2)),raw);},"compact sample read as V1");
    const auto oldraw=encode_declared_bits(CompiledLayout(reg,id(2)),x);rejects([&]{(void)decode_declared_bits(l,oldraw);},"legacy sample read as V2");
    // Bounded mutations: any accepted stream must re-encode identically.
    std::mt19937 gen(701);for(int i=0;i<700;++i){auto b=nb;b.bytes[gen()%b.bytes.size()]^=std::byte(1u<<(gen()%8));
        std::optional<V> v;try{v=decode_declared_bits(nl,b);}catch(const std::exception&){++rejected_mutations;}
        if(v){++accepted_mutations;check(encode_declared_bits(nl,*v)==b,"accepted noncanonical mutation");}
    }
}
void additional_types(){
    // Non-tensor layouts retain identical scalar bits in either profile.
    for(auto d:{Declaration::boolean(id(10)),Declaration::integer(id(10)),Declaration::rational(id(10)),Declaration::finite(id(10),{8,9,10})}){
        V x;switch(d.kind){case DeclarationKind::Boolean:x=V::boolean(true);break;case DeclarationKind::Integer:x=V::integer(-127);break;case DeclarationKind::Rational:x=V::rational(numeric::Rational(-7,13));break;default:x=V::symbol(9);}
        const DeclarationRegistry r({d});const CompiledLayout a(r,id(10)),b(r,id(10),v2);
        check(encode_declared_bits(a,x)==encode_declared_bits(b,x),"non-tensor scalar bits changed");roundtrip(b,x);
    }
    for(const auto& axes:std::vector<std::vector<DeclarationAxis>>{{},{{44,2,{4,2}},{55,3,{9,7,8}}},{{44,0,{}},{55,3,{}}}}){
        const auto d=Declaration::rational_tensor(id(10),axes);const DeclarationRegistry r({d});const auto domain=DeclarationRegistry::tensor_domain(d);
        const auto x=V::tensor(numeric::SparseRationalTensor(domain,numeric::StructuralSupport::full(domain.logical_size()),numeric::SparseRationalVector(domain.logical_size())));
        const CompiledLayout l(r,id(10),v2);roundtrip(l,x);check(l.bounds().minimum_bits<=l.allocation(x).bits,"bounds exclude valid tensor");
    }
    // Huge declared size is checked but does not allocate support records.
    const std::size_t n=1000000000000000000ULL;const auto x=tensor(n);const CompiledLayout l(registry(n),id(2),v2);
    const auto raw=encode_declared_bits(l,x);const auto y=decode_declared_bits(l,raw);const auto& t=std::get<numeric::SparseRationalTensor>(y.data);
    check(t.support().count()==n && t.support().storage()==numeric::StructuralSupport::Storage::Full && t.stored_nonzeros()==2,"huge symbolic support");
    check(value_at(t.values(),0)==numeric::Rational(-2,3) && value_at(t.values(),n-1)==numeric::Rational(4,7),"huge exact values");
    check(l.allocation(x).value_nodes==3 && l.allocation(x).materialized_bytes==CompiledLayout(registry(1000003),id(2),v2).allocation(tensor(1000003)).materialized_bytes,"symbolic work grew with extent");
}
}
int main(int argc,char** argv){try{
    const std::filesystem::path fixtures=std::filesystem::path(RFC_FORMAT1_FIXTURES);
    golden(fixtures,argc==2?std::filesystem::path(argv[1]):std::filesystem::path{});
    boundaries();additional_types();
    std::cout<<"compact_declared_tensor checks="<<checks<<" mutations_accepted="<<accepted_mutations<<" mutations_rejected="<<rejected_mutations<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
