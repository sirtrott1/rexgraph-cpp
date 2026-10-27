#include "rfc/composite/declaration_evolution.hpp"
#include "rfc/storage/declaration_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
using namespace rfc;
using namespace rfc::composite;
using namespace rfc::storage;
using Q=numeric::Rational;
using T=numeric::SparseRationalTensor;
using Support=numeric::StructuralSupport;
using V=DeclaredValue;
namespace {
std::size_t checks{};
void check(bool ok,const char* m){++checks;if(!ok)throw std::runtime_error(m);}
template<class F> void rejects(F&& f,const char* m){bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit,m);}
constexpr auto v1=BooleanLayoutProfile::Lsb0OctetsV1,v2=BooleanLayoutProfile::Lsb0OctetsV2;
DeclarationId id(std::uint64_t key,std::uint64_t version=1){return {4201,key,version};}
Q at(const numeric::SparseRationalVector& v,std::size_t index){
    for(const auto& e:v.entries())if(e.index==index)return e.value;
    return Q{0};
}
V tensor(const numeric::IndexDomain& d,Support s,std::vector<numeric::SparseRationalEntry> es={}){
    return V::tensor(T(d,std::move(s),numeric::SparseRationalVector(d.logical_size(),std::move(es))));
}
std::vector<DeclarationMap> mapping(std::uint64_t last,DeclarationMapKind kind=DeclarationMapKind::Recode){
    std::vector<DeclarationMap> out;for(std::uint64_t k=1;k<=last;++k)out.push_back({id(k),id(k,2),kind,{}});return out;
}
void permutations(){
    const DeclarationRegistry a({Declaration::rational_tensor(id(1),{{11,2,{10,20}},{22,3,{30,40,50}}})});
    std::vector<std::uint64_t> rows{10,20},cols{30,40,50};
    do { cols={30,40,50}; do { for(bool swap:{false,true}) {
        std::vector<DeclarationAxis> axes{{11,2,rows},{22,3,cols}};if(swap)std::reverse(axes.begin(),axes.end());
        const DeclarationRegistry b({Declaration::rational_tensor(id(1,2),axes)});
        for(auto profile:{v1,v2}) {
            DeclarationRecoding f(a,b,mapping(1),{},{profile,profile});
            DeclarationRecoding back(b,a,{{id(1,2),id(1),DeclarationMapKind::Recode,{}}},{},{profile,profile});
            for(int variant=0;variant<5;++variant) {
                const bool full=variant<3;
                std::vector<std::size_t> old_indices=full?std::vector<std::size_t>{0,1,2,3,4,5}:
                    variant==3?std::vector<std::size_t>{0,2,5}:std::vector<std::size_t>{};
                auto support=variant==0?Support::full(6):variant==2?Support::packed(6,old_indices):Support::sparse(6,old_indices);
                auto x=tensor(numeric::IndexDomain({2,3}),support,variant==4?std::vector<numeric::SparseRationalEntry>{}:
                    std::vector<numeric::SparseRationalEntry>{{0,Q{-2,3}},{5,Q{4,7}}});
                const auto y=f.apply(id(1),x);const auto& t=std::get<T>(y.data);
                check(t.domain()==DeclarationRegistry::tensor_domain(b.at(id(1,2))),"axis order");
                check(t.support().count()==old_indices.size(),"membership population");
                for(std::size_t r=0;r<2;++r)for(std::size_t c=0;c<3;++c){
                    // Independent small reference: locate original named coordinates
                    // in each target axis and construct its row-major position.
                    const auto rr=static_cast<std::size_t>(std::find(rows.begin(),rows.end(),10+10*r)-rows.begin());
                    const auto cc=static_cast<std::size_t>(std::find(cols.begin(),cols.end(),30+10*c)-cols.begin());
                    const auto j=swap?cc*2+rr:rr*3+cc;
                    check(t.support().contains(j)==std::get<T>(x.data).support().contains(r*3+c),"named support");
                    check(at(t.values(),j)==at(std::get<T>(x.data).values(),r*3+c),"named rational value");
                }
                if(full)check(t.support().storage()==Support::Storage::Full,"complete membership materialized");
                check(declared_value_equal(back.apply(id(1,2),y),x),"inverse recoding");
                const CompiledLayout target(b,id(1,2),profile);
                const auto bytes=encode_declared_archive(target,std::span(&y,1));
                const auto decoded=decode_declared_archive(bytes);
                check(declared_value_equal(decoded.values.front(),y),"archive round trip");
                check(encode_declared_archive(decoded.layout,decoded.values)==bytes,"canonical archive");
                check(decoded.layout.profile()==profile,"profile changed");
            }
        }
    }}while(std::next_permutation(cols.begin(),cols.end()));}while(std::next_permutation(rows.begin(),rows.end()));
}
void inclusions(){
    const DeclarationRegistry a({Declaration::rational_tensor(id(1),{{11,2,{10,20}},{22,3,{30,40,50}}})});
    const DeclarationRegistry b({Declaration::rational_tensor(id(1,2),{{22,4,{50,30,40,99}},{11,3,{20,10,88}}})});
    DeclarationRecoding f(a,b,mapping(1,DeclarationMapKind::Embedding),{},{v2,v2});
    const auto x=tensor(numeric::IndexDomain({2,3}),Support::full(6),{{0,Q{-2,3}},{5,Q{4,7}}});
    const auto y=f.apply(id(1),x);const auto& t=std::get<T>(y.data);
    check(t.support().storage()==Support::Storage::SparseIndices,"inclusion made complement supported");
    check(*t.support().sparse_indices()==std::vector<std::size_t>({0,1,3,4,6,7}),"inclusion image");
    check(at(t.values(),4)==Q{-2,3} && at(t.values(),0)==Q{4,7},"inclusion values");
    check(t.support().contains(1) && at(t.values(),1)==0 && !t.support().contains(2),"supported zero vs complement");
    rejects([&]{DeclarationRecoding bad(a,b,mapping(1));},"growth called bijection");
    rejects([&]{DeclarationRecoding bad(b,a,{{id(1,2),id(1),DeclarationMapKind::Embedding,{}}});},"lossy reverse accepted");
    const DeclarationRegistry renamed({Declaration::rational_tensor(id(1,2),{{11,2,{10,99}},{22,3,{30,40,50}}})});
    rejects([&]{DeclarationRecoding bad(a,renamed,mapping(1));},"same extent with missing identity");
    const DeclarationRegistry ordinal({Declaration::rational_tensor(id(1,2),{{11,2,{}},{22,3,{}}})});
    rejects([&]{DeclarationRecoding bad(a,ordinal,mapping(1));},"named/ordinal conflated");
    auto tight=DeclarationLimits{};tight.max_value_nodes=3;
    rejects([&]{(void)DeclarationRecoding(a,b,mapping(1,DeclarationMapKind::Embedding),tight,{v2,v2}).apply(id(1),x);},"image work budget");
    tight={};tight.max_materialized_bytes=CompiledLayout(a,id(1),v2).allocation(x).materialized_bytes+6*sizeof(std::size_t)-1;
    rejects([&]{(void)DeclarationRecoding(a,b,mapping(1,DeclarationMapKind::Embedding),tight,{v2,v2}).apply(id(1),x);},"image byte budget");
    // An embedding may be bijective even when its kind permits growth.
    const DeclarationRegistry equal({Declaration::rational_tensor(id(1,2),a.at(id(1)).axes)});
    const auto eq=DeclarationRecoding(a,equal,mapping(1,DeclarationMapKind::Embedding),{},{v2,v2}).apply(id(1),x);
    check(std::get<T>(eq.data).support().storage()==Support::Storage::Full,"bijective embedding expanded");
}
void huge(){
    for(const auto n:{std::size_t{1000003},std::size_t{1000000000}}){
        const numeric::IndexDomain d({n,n}); const auto total=d.logical_size();
        DeclarationRegistry a({Declaration::rational_tensor(id(1),{{11,n,{}},{22,n,{}}})});
        DeclarationRegistry b({Declaration::rational_tensor(id(1,2),{{22,n,{}},{11,n,{}}})});
        const auto x=tensor(d,Support::full(total),{{n-1,Q{-2,3}},{total-1,Q{4,7}}});
        DeclarationLimits limits;limits.max_value_nodes=3;limits.max_materialized_bytes=4096;limits.max_encoded_bytes=128;
        const auto y=DeclarationRecoding(a,b,mapping(1),limits,{v2,v2}).apply(id(1),x);
        const auto& t=std::get<T>(y.data);
        check(t.support().storage()==Support::Storage::Full && t.support().count()==total,"huge support");
        check(t.stored_nonzeros()==2 && at(t.values(),(n-1)*n)==Q{-2,3} && at(t.values(),total-1)==Q{4,7},"huge transpose");
        check(t.support().contains(1) && at(t.values(),1)==0,"huge supported zero");
        const auto z=DeclarationRecoding(b,a,{{id(1,2),id(1),DeclarationMapKind::Recode,{}}},limits,{v2,v2}).apply(id(1,2),y);
        check(declared_value_equal(z,x),"huge equality/inverse scanned full extent");
        const CompiledLayout layout(b,id(1,2),v2,limits);
        const auto bits=encode_declared_bits(layout,y);check(bits.bytes.size()<128,"huge wire expansion");
        check(declared_value_equal(decode_declared_bits(layout,bits),y),"huge codec round trip");
        auto low=limits;low.max_value_nodes=2;
        rejects([&]{(void)DeclarationRecoding(a,b,mapping(1),low,{v2,v2}).apply(id(1),x);},"nonzero work not charged");
        low=limits;low.max_encoded_bytes=1;
        rejects([&]{(void)DeclarationRecoding(a,b,mapping(1),low,{v2,v2}).apply(id(1),x);},"wire limit ignored");
        rejects([&]{(void)DeclarationRecoding(a,b,mapping(1)).apply(id(1),x);},"default V1 upgraded implicitly");
        rejects([&]{(void)DeclarationRecoding(a,b,mapping(1),limits,{v2,v1}).apply(id(1),x);},"target V1 costs ignored");
        const auto zero=tensor(d,Support::full(total));
        const auto rz=DeclarationRecoding(a,b,mapping(1),limits,{v2,v2}).apply(id(1),zero);
        check(std::get<T>(rz.data).stored_nonzeros()==0 && std::get<T>(rz.data).support().storage()==Support::Storage::Full,"huge zero field");
        std::cout<<"symbolic grid="<<total<<" nonzeros=2 raw_bytes="<<bits.bytes.size()<<" value_nodes="<<layout.allocation(y).value_nodes<<'\n';
    }
}
void mixed_profiles(){
    const DeclarationRegistry a({Declaration::rational_tensor(id(1),{{11,3,{10,20,30}}})}),
        b({Declaration::rational_tensor(id(1,2),{{11,3,{30,10,20}}})});
    const auto x=tensor(numeric::IndexDomain::linear(3),Support::full(3),{{0,Q{-2,3}}});
    for(auto from:{v1,v2})for(auto to:{v1,v2}) {
        const auto y=DeclarationRecoding(a,b,mapping(1),{},{from,to}).apply(id(1),x);
        const auto wire=encode_declared_archive(CompiledLayout(b,id(1,2),to),std::span(&y,1));
        const auto reopened=decode_declared_archive(wire);
        check(reopened.layout.profile()==to,"explicit mixed-profile target lost");
        check(at(std::get<T>(y.data).values(),1)==Q{-2,3},"mixed-profile value");
    }
}
void nested_and_empty(){
    const auto defs=[](std::uint64_t v){return DeclarationRegistry({Declaration::rational_tensor(id(1,v),{{11,1000003,{}}}),
        Declaration::optional(id(2,v),id(1,v)),Declaration::alternative(id(3,v),{{10,id(2,v),0}}),
        Declaration::repeated(id(4,v),id(3,v),0,3),Declaration::product(id(5,v),{{10,id(4,v),2}})});};
    const auto x=tensor(numeric::IndexDomain::linear(1000003),Support::full(1000003),{{0,Q{4,7}}});
    const auto value=V::product({V::repeated({V::alternative(10,V::present(x)),V::alternative(10,V::absent())})});
    const auto mapped=DeclarationRecoding(defs(1),defs(2),mapping(5),{},{v2,v2}).apply(id(5),value);
    check(declared_value_equal(value,mapped),"nested tensor values");
    for(auto profile:{v1,v2}){
        const DeclarationRegistry a({Declaration::rational_tensor(id(1),{{11,0,{}},{22,2,{}}})});
        const DeclarationRegistry b({Declaration::rational_tensor(id(1,2),{{22,2,{}},{11,0,{}}})});
        const auto empty=tensor(numeric::IndexDomain({0,2}),Support::full(0));
        const auto e=DeclarationRecoding(a,b,mapping(1),{},{profile,profile}).apply(id(1),empty);
        check(std::get<T>(e.data).support().storage()==Support::Storage::Full,"empty permutation");
        const DeclarationRegistry still_empty({Declaration::rational_tensor(id(1,2),{{22,3,{}},{11,0,{}}})});
        const auto ee=DeclarationRecoding(a,still_empty,mapping(1,DeclarationMapKind::Embedding),{},{profile,profile}).apply(id(1),empty);
        check(std::get<T>(ee.data).support().storage()==Support::Storage::Full,"empty target membership");
        const DeclarationRegistry grow({Declaration::rational_tensor(id(1,2),{{22,2,{}},{11,1,{}}})});
        const auto g=DeclarationRecoding(a,grow,mapping(1,DeclarationMapKind::Embedding),{},{profile,profile}).apply(id(1),empty);
        check(std::get<T>(g.data).support().count()==0 && std::get<T>(g.data).domain().logical_size()==2,"empty image grew support");
        const DeclarationRegistry scalar1({Declaration::rational_tensor(id(1),{})}),scalar2({Declaration::rational_tensor(id(1,2),{})});
        const auto scalar=tensor(numeric::IndexDomain(std::vector<std::size_t>{}),Support::full(1),{{0,Q{2,7}}});
        check(declared_value_equal(DeclarationRecoding(scalar1,scalar2,mapping(1),{},{profile,profile}).apply(id(1),scalar),scalar),"rank zero");
    }
    const DeclarationRegistry a({Declaration::product(id(1),{})});
    const DeclarationRegistry b({Declaration::product(id(1,2),{{9,id(2,2),0}}),
        Declaration::optional(id(2,2),id(3,2)),Declaration::rational_tensor(id(3,2),{{11,1000003,{}}})});
    const auto fill=V::present(x);
    const auto filled=DeclarationRecoding(a,b,{{id(1),id(1,2),DeclarationMapKind::Embedding,{{9,fill}}}},{},{v1,v2}).apply(id(1),V::product({}));
    check(declared_value_equal(filled,V::product({fill})),"explicit large fill lost");
    const DeclarationRegistry c({Declaration::product(id(1,2),{{9,id(2,2),0}}),Declaration::rational_tensor(id(2,2),{{11,3,{}}})});
    for(auto s:{Support::sparse(3,{0,1,2}),Support::packed(3,{0,1,2})}){
        const auto supplied=tensor(numeric::IndexDomain::linear(3),s,{{1,Q{2,7}}});
        const auto copied=DeclarationRecoding(a,c,{{id(1),id(1,2),DeclarationMapKind::Embedding,{{9,supplied}}}},{},{v1,v2}).apply(id(1),V::product({}));
        const auto& t=std::get<T>(std::get<V::Product>(copied.data).elements.front().data);
        check(t.support().storage()==Support::Storage::Full,"full explicit fill copied backing array");
        check(at(t.values(),1)==Q{2,7},"fill value changed");
    }
    rejects([&]{(void)DeclarationRecoding(a,c,{{id(1),id(1,2),DeclarationMapKind::Embedding,{}}},{},{v1,v2});},"implicit fill");
    rejects([&]{(void)DeclarationRecoding(a,a,identity_declaration_maps(a),{},{static_cast<BooleanLayoutProfile>(99),v2});},"bad profile");
}
}
int main(){try{permutations();inclusions();huge();mixed_profiles();nested_and_empty();std::cout<<"symbolic_recoding checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
