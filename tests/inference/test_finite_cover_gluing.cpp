#include "../../examples/finite_cover_data.hpp"
#include <future>
#include <iostream>
using namespace finite_cover_example;
std::size_t checks{};
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,m);}
bool equal(const S& a,const S& b){return a.size()==b.size() && a.entries()==b.entries();}
Q dot(const S& a,const S& b){Q out=0;for(const auto& x:a.entries())for(const auto& y:b.entries())if(x.index==y.index)out+=x.value*y.value;return out;}
LocalStalkPartition raw_partition(std::size_t p,std::size_t n,std::vector<StalkCoordinateBlock> blocks){
    const auto f=make(100,10,20,n);const auto local=f.contract.downward.stalks.back();return {{p,local,18000+p,local.declaration,StalkChartMap::identity(n),StalkChartMap::identity(n)},std::move(blocks)};
}
void primitive(){
    std::vector<LocalStalkPartition> parts;for(std::size_t p=0;p<3;++p)parts.push_back(raw_partition(p,3,{block(1),block(2),block(3+p)}));
    const std::vector<StalkBlockLink> links{{0,0,1,0},{0,1,1,1},{1,0,2,0},{0,0,2,0}};Guard g;SplitStalkCover c(parts,links,g);
    check(c.dimension()==6,"redundant triple overlap is not subtracted twice");
    SplitStalkCover disjoint(parts,{},g);check(disjoint.dimension()==9,"equal labels alone do not identify directions");
    auto redundant=links;redundant.push_back(links[0]);SplitStalkCover duplicate(parts,redundant,g);check(duplicate.dimension()==6,"duplicate interface is a redundant constraint");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)for(int d=-2;d<=2;++d){
        const S x(6,{{0,a},{1,b},{2,d},{3,Q{1,3}},{4,Q{-2,7}},{5,5}});
        std::vector<S> locals;for(std::size_t p=0;p<3;++p)locals.push_back(c.restrict(p,x,g));
        check(equal(c.glue(locals,g),x),"all restrictions recover the complete field");
        std::map<std::size_t,Q> sum;
        for(std::size_t p=0;p<3;++p){const auto h=c.representative(p,x,g);const auto j=c.include(p,h,g);for(const auto& e:j.entries())sum[e.index]+=e.value;
            check(dot(c.include(p,locals[p],g),x)==dot(locals[p],c.include_transpose(p,x,g)),"chain injection transpose");
            check(dot(c.representative(p,x,g),locals[p])==dot(x,c.representative_transpose(p,locals[p],g)),"representative transpose");}
        std::vector<numeric::SparseRationalEntry> es;for(const auto& [i,q]:sum)if(q!=0)es.push_back({i,q});check(equal(S(6,es),x),"sum j_p h_p is identity");
    }
    const S shared=S::basis(3,0,2);const auto j0=c.include(0,shared,g),j1=c.include(1,shared,g);check(equal(j0,j1),"difference of common injections is zero");
    check(equal(c.glue({shared,shared,shared},g),S::basis(6,0,2)),"three section copies counted once");
    rejects([&]{c.glue({shared,S::basis(3,0,3),shared},g);},"conflicting triple field rejected");
    auto collapse=parts;for(auto& p:collapse){p.blocks[1].declaration=p.blocks[0].declaration;}
    rejects([&]{SplitStalkCover v(collapse,{{0,0,1,0},{1,0,2,0},{2,0,0,1}},g);},"individually injective cycle cannot kill local difference");
    auto wrong=parts;wrong[1].blocks[0].declaration.axes[0].key+=1;rejects([&]{SplitStalkCover v(wrong,links,g);},"wrong complete block definition");
    wrong=parts;wrong[0].blocks[1].name=wrong[0].blocks[0].name;rejects([&]{SplitStalkCover v(wrong,links,g);},"ambiguous local block name");
    wrong=parts;wrong[0].blocks.pop_back();rejects([&]{SplitStalkCover v(wrong,links,g);},"missing private direction refused");
    wrong=parts;wrong[0].chart.from_common=StalkChartMap::identity(3,2);rejects([&]{SplitStalkCover v(wrong,links,g);},"false inverse refused");
    wrong=parts;wrong[0].chart.common_declaration.axes[0].coordinates={1,2,3};rejects([&]{SplitStalkCover v(wrong,links,g);},"named split cannot mislabel ordinal blocks");
}
void descent_reference(){
    // Independent explicit small matrices are a TEST oracle, never the runtime path.
    for(int pattern=0;pattern<160;++pattern){
        const std::vector<std::size_t> selected=(pattern%2)?std::vector<std::size_t>{2,0}:std::vector<std::size_t>{1,3};
        const split_cover_detail::BlockEmbedding e{{0,1,2},{0,1,2,3,4},selected};Guard g;
        M correction(4,4),common(2,2);const Q scale=(pattern%3)-1;
        for(std::size_t i=0;i<2;++i)for(std::size_t j=0;j<2;++j){const Q q=int((pattern+3*i+7*j)%5)-2;common.set(i,j,q);correction.set(selected[i],selected[j],q);}
        StalkChartMap L(scale,correction),K(scale,common);
        if(pattern%4==0){correction.set((selected[0]+1)%4,selected[1],7);L=StalkChartMap(scale,correction);}
        M E(4,2),Lm(4,4),Km(2,2);for(std::size_t i=0;i<2;++i)E.set(selected[i],i,1);
        for(const auto& [ij,q]:L.correction().entries())Lm.set(ij.first,ij.second,q);
        for(const auto& [ij,q]:K.correction().entries())Km.set(ij.first,ij.second,q);
        for(std::size_t i=0;i<4;++i)Lm.set(i,i,Lm.get(i,i)+scale);
        for(std::size_t i=0;i<2;++i)Km.set(i,i,Km.get(i,i)+scale);
        const auto lhs=numeric::checked_multiply(Lm,E,g),rhs=numeric::checked_multiply(E,Km,g);
        check(split_cover_detail::descends(L,K,e,e,g)==(lhs.entries()==rhs.entries()),"symbolic full-map descent matches independent expansion");
    }
    // Distinct lower and upper block placement; explicit correction cancels a diagonal.
    Guard g;const split_cover_detail::BlockEmbedding low{{0,2},{0,2,4},{1}},up{{0,2},{0,2,4},{0}};
    M c(4,4);c.set(0,0,-1);c.set(1,1,-1);c.set(2,0,1);c.set(3,1,1);
    check(split_cover_detail::descends(StalkChartMap(1,c),StalkChartMap::identity(2),low,up,g),"shifted implicit diagonal canceled by exact sparse terms");
    c.set(3,1,2);check(!split_cover_detail::descends(StalkChartMap(1,c),StalkChartMap::identity(2),low,up,g),"uncanceled shifted diagonal rejected");
}
void native(){
    const auto d=make_cover(true);const auto p=plan_cover(d);const auto r=cover_result(p,d);const auto& v=p.view();Guard g;
    check(v.cell_count(g0)==2 && v.cell_count(g1)==1 && v.occurrence_count(g1)==6,"shared source and occurrence identities once, including zero/inactive/cancelling");
    check(v.dimension(g1)==6 && r.total==3228 && r.matches_reference==true,"three independent owners feed exact action moment engine");
    check(r.components[0].self==166 && r.components[1].self==490 && r.components[2].self==1732,"component self moments");
    check(2*dot(r.components[0].field.sparse_values(),r.components[1].field.sparse_values())==168,"AB interaction");
    check(2*dot(r.components[0].field.sparse_values(),r.components[2].field.sparse_values())==672,"AC interaction");
    check(dot(r.components[1].field.sparse_values(),r.components[2].field.sparse_values())==0,"BC zero interaction");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){
        const S x(6,{{0,a},{1,b},{2,5},{3,7},{4,11},{5,13}});const Q s=Q{a}+29,t=Q{b}+14;
        const S expected(12,{{0,-s},{1,-t},{2,-5},{3,-7},{4,-11},{5,-13},{6,s},{7,t},{8,5},{9,7},{10,11},{11,13}});
        const auto y=v.boundary(g1).apply(x,g);check(equal(y,expected),"sparse descended full action matches independent formula");
        check(dot(y,y)==dot(x,v.boundary(g1).transpose_apply(y,g)),"global algebraic transpose reverses factors");
        std::vector<field::SparseStalkField> local;for(std::size_t i=0;i<3;++i)local.push_back(v.restrict_field(g1,x,i,g));check(equal(v.glue_fields(g1,local,g),x),"private/shared field reconstruction across cover");
    }
    for(std::size_t side=0;side<3;++side){field::SparseStalkFieldActions fa(v.patches()[side].fibers());for(std::size_t i=0;i<3;++i){const auto x=fa.bind(g1,{{0,S::basis(3,i)}}),y=fa.boundary(x).field;
        check(equal(v.include_chain_field(g0,y,side,g),v.boundary(g1).apply(v.include_chain_field(g1,x,side,g),g)),"whole local inclusion square");}}
    std::array<std::size_t,3> order{0,1,2};do{const auto dd=permute(d,order);const auto pp=plan_cover(dd);check(cover_result(pp,dd).total==3228,"all patch orders preserve total");}while(std::next_permutation(order.begin(),order.end()));
    auto no_cycle=d;no_cycle.interfaces.pop_back();check(cover_result(plan_cover(no_cycle),no_cycle).total==3228,"redundant cyclic interface does not shrink quotient");
    auto duplicated=d;duplicated.interfaces.push_back(d.interfaces.front());check(cover_result(plan_cover(duplicated),duplicated).total==3228,"duplicate common source retains redundant overlap");
    auto bad=d;bad.partitions.pop_back();rejects([&]{plan_cover(bad);},"incomplete local partition inventory");
    bad=d;bad.interfaces[0].block_images[1]={0,0};rejects([&]{plan_cover(bad);},"noninjective common block map");
    bad=d;bad.interfaces[0].maps[1].occurrences[1][0]=1;rejects([&]{plan_cover(bad);},"wrong occurrence mapping");
    bad=d;bad.partitions[0].chart.local.carrier.coordinate_system+=1;rejects([&]{plan_cover(bad);},"wrong local coordinate owner");
    bad=d;bad.partitions[0].chart.accession=bad.partitions[0].chart.reconstruction;rejects([&]{plan_cover(bad);},"wrong stored map direction/role");
    bad=d;for(auto& f:bad.locals)set_field(f,g1,S(3));
    for(auto& t:bad.locals[0].endpoint.tensors)if(t.role==TemporalTensorRole::RealizedIncidenceMap){auto m=temporal_tensor_matrix(t);m.set(2,0,m.get(2,0)+1);t.tensor=temporal_matrix_tensor(m);break;}
    rejects([&]{plan_cover(bad);},"zero supplied field cannot hide shared-to-private leakage");
    bad=d;set_field(bad.locals[1],g1,S::basis(3,0,99));const auto mismatch=plan_cover(bad);rejects([&]{cover_result(mismatch,bad);},"incompatible observation after valid map construction");
    const auto foreign=plan_cover(d);const auto x=foreign.glue_field(g1,inputs(d.locals,1),available(d));rejects([&]{p.boundary(g1).execute(x,available(d));},"foreign quotient owner rejected");
    auto lim=numeric::ExactActionLimits{};lim.steps=0;rejects([&]{prepare_cover(d,lim);},"preparation work refusal");lim={};lim.entries=1;rejects([&]{prepare_cover(d,lim);},"preparation storage refusal");lim={};lim.integer_bits=1;rejects([&]{prepare_cover(d,lim);},"coefficient growth refusal");
    auto av=available(d);av.back().max_sequence=6;rejects([&]{p.glue_field(g1,inputs(d.locals,1),av);},"every interface publication required at cutoff");
    Guard cancel({},[]{throw std::runtime_error("stop");});rejects([&]{v.boundary(g1).apply(r.input.sparse_values(),cancel);},"cooperative cancellation");check(equal(v.boundary(g1).apply(r.input.sparse_values(),g),r.aggregate.sparse_values()),"reuse after cancellation");
    auto task=std::async(std::launch::async,[p,d]{return cover_result(p,d).total;});check(task.get()==3228,"immutable plan concurrent independent invocation");
}
void higher(){
    CoverDataset d;
    for(std::size_t p=0;p<3;++p){core::RelationalComplexBuilder b(signature());b.declare_grade({2});
        b.add_grade_one(core::RelationKey{10},0,{{10,true,true,1},{20,true,false,1}});
        b.add_grade_one(core::RelationKey{20},0,{{20,true,true,1},{30+10*p,true,false,1}});
        b.add_grade_one(core::RelationKey{30},0,{{10,true,true,1},{30+10*p,true,false,1}});
        b.add_higher({2},core::RelationKey{40},0,{{0,true,false,1},{1,true,false,1},{2,true,true,1}});
        auto ch=changed(make_source(b.freeze(),3),p,19000+p);const std::vector bs{block(1),block(2),block(3+p)};declare_blocks(ch.field,bs);const auto ps=records(ch,bs);d.partitions.insert(d.partitions.end(),ps.begin(),ps.end());d.locals.push_back(ch.field);
    }
    for(std::size_t i=0;i<3;++i){const auto n=i==0?2u:1u;auto ch=changed(make(10,10,20,n),0,19500+i);std::vector bs{block(1)};if(n==2)bs.push_back(block(2));declare_blocks(ch.field,bs);const auto m=identity_map(ch.field.endpoint.source);const std::vector<std::size_t> im=n==2?std::vector<std::size_t>{0,1}:std::vector<std::size_t>{0};const std::array<std::size_t,3> left{0,1,0},right{1,2,2};d.interfaces.push_back({ch.field,records(ch,bs),left[i],right[i],{m,m},{im,im}});}
    const auto p=plan_cover(d);const auto& v=p.view();Guard g;
    check(v.cell_count(g0)==5 && v.cell_count(g1)==7 && v.cell_count({2})==3,"three triangles retain shared edge once");
    check(v.dimension(g0)==21 && v.dimension(g1)==24 && v.dimension({2})==9,"multi-interface private higher-grade directions retained");
    for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b){const S x(9,{{0,a},{1,Q{1,3}},{3,b},{5,Q{-2,7}},{8,4}});check(v.boundary(g1).apply(v.boundary({2}).apply(x,g),g).nonzeros()==0,"three-patch grade-two chain condition");}
    for(std::size_t side=0;side<3;++side){field::SparseStalkFieldActions fa(v.patches()[side].fibers());for(std::size_t i=0;i<3;++i){const auto x=fa.bind({2},{{0,S::basis(3,i)}}),y=fa.boundary(x).field;check(equal(v.include_chain_field(g1,y,side,g),v.boundary({2}).apply(v.include_chain_field({2},x,side,g),g)),"higher entire local inclusion square");}}
}
void symbolic(){
    std::size_t steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        numeric::ExactActionLimits lim;lim.dimension=std::numeric_limits<std::size_t>::max();lim.entries=100000;Guard prep(lim);
        std::vector<LocalStalkPartition> ps;for(std::size_t p=0;p<3;++p)ps.push_back(raw_partition(p,n,{block(1),block(30+p,n-1)}));
        M f(n,n),b(n,n);f.set(0,n-1,1);b.set(0,n-1,-1);ps[0].chart.to_common=StalkChartMap(1,f);ps[0].chart.from_common=StalkChartMap(1,b);
        SplitStalkCover c(ps,{{0,0,1,0},{1,0,2,0},{2,0,0,0}},prep);check(c.dimension()==3*n-2,"huge cover dimension without redundant cycle subtraction");Guard g(lim);
        const auto x=c.glue({S(n,{{0,-1},{n-1,3}}),S(n,{{0,2},{n-1,5}}),S(n,{{0,2},{n-1,7}})},g);
        check(equal(x,S(3*n-2,{{0,2},{n-1,3},{2*n-2,5},{3*n-3,7}})),"huge private coordinates remain exact and symbolic");
        check(equal(c.restrict(0,x,g),S(n,{{0,-1},{n-1,3}})),"huge inverse chart restriction");
        if(steps)check(steps==g.steps(),"represented work independent of extent");else steps=g.steps();
        const split_cover_detail::BlockEmbedding e{{0,n-1},{0,1,n},{1}};
        check(split_cover_detail::descends(StalkChartMap::identity(n),StalkChartMap::identity(n-1),e,e,prep),"huge proper common identity descent stays symbolic");
    }
    std::cout<<"finite_cover_symbolic_steps="<<steps<<'\n';
}
void symbolic_view(){
    std::size_t steps=0;
    for(const std::size_t n:{7ull,1000003ull,1000000000000000000ull}){
        numeric::ExactActionLimits lim;lim.dimension=std::numeric_limits<std::size_t>::max();lim.entries=200000;
        std::vector<DeclaredLocalFieldBinding> bindings;std::vector<LocalStalkPartition> ps;
        for(std::size_t p=0;p<3;++p){const auto f=make(100,10,20,n);bindings.emplace_back(f.endpoint,f.contract,lim);
            for(const auto& b:f.contract.downward.stalks){M forward(n,n),back(n,n);if(p==0){forward.set(0,n-1,1);back.set(0,n-1,-1);}
                ps.push_back({{p,b,23000+p,b.declaration,StalkChartMap(1,forward),StalkChartMap(1,back)},{block(1),block(30+p,n-1)}});}}
        const auto common=make(100,10,20);const DeclaredLocalFieldBinding cb(common.endpoint,common.contract,lim);const auto m=identity_map(common.endpoint.source);
        std::vector<FiniteCoverInterface> interfaces;
        for(const auto pair:std::array<std::array<std::size_t,2>,3>{{{0,1},{1,2},{0,2}}}){std::vector<CoverInterfaceStalk> ss;
            for(const auto& b:common.contract.downward.stalks)ss.push_back({{{0,b,24000,b.declaration,StalkChartMap::identity(1),StalkChartMap::identity(1)},{block(1)}},{{{0},{0}}}});
            interfaces.push_back({cb,pair[0],pair[1],{m,m},ss});}
        const FiniteCoverLocalFieldView v(bindings,ps,interfaces,lim);std::vector<field::SparseStalkField> xs;
        for(std::size_t p=0;p<3;++p)xs.push_back(field::SparseStalkFieldActions(bindings[p].fibers(),{},lim).bind(g1,{{0,S(n,{{0,p?Q{2}:Q{-1}},{n-1,Q{3+2*static_cast<long>(p)}}})}}));
        Guard g(lim);const auto x=v.glue_fields(g1,xs,g),y=v.boundary(g1).apply(x,g);const auto local=v.restrict_field(g1,x,0,g);
        check(x.size()==3*n-2 && x.nonzeros()==4 && y.size()==6*n-4 && y.nonzeros()==8,"huge actual cover field and boundary remain sparse");
        check(equal(local.values().at(0),xs[0].values().at(0)),"huge local field recovered through chart");
        if(steps)check(g.steps()==steps,"actual cover glue/action/restriction work independent of extent");else steps=g.steps();
    }
    std::cout<<"finite_cover_symbolic_view_steps="<<steps<<'\n';
}
int main(){try{primitive();descent_reference();native();higher();symbolic();symbolic_view();std::cout<<"finite_cover_gluing checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
