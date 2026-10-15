#include "../../examples/native_homotopy_sequence/fixture.hpp"
#include <future>
#include <iostream>
#include <string_view>

namespace {
using namespace native_homotopy_sequence_example;
std::size_t checks{}, witnesses{}, pushes{};
using Output = inference::DerivedNativeSequencedHomotopyChain;
Values vals(Output out) { return out.chain().chain().values(); }
void check(bool ok,const char* message) { ++checks;if(!ok)throw std::runtime_error(message); }
template<class E=std::invalid_argument,class F>void rejects(F action,const char* message) {
    bool caught=false;try{action();}catch(const E&){caught=true;}check(caught,message);
}
void json(const Values& v) {
    std::cout<<'[';bool comma=false;for(const auto& [i,q]:v){if(comma)std::cout<<',';comma=true;std::cout<<'['<<i<<",\""<<q<<"\"]";}std::cout<<']';
}
void json(algebra::ProductCell id) { std::cout<<'['<<id.left.grade.value<<','<<id.left.index<<','<<id.right.grade.value<<','<<id.right.index<<']'; }
void json(const core::RelationalComplex& c) {
    std::cout<<"{\"sizes\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<c.grade(topology::owned_cylinder_detail::grade(k)).cell_count();}
    std::cout<<"],\"types\":[";
    for(std::size_t i=0;i<c.signature().type_count();++i){if(i)std::cout<<',';std::cout<<'"'<<c.signature().type(static_cast<core::RelationTypeId>(i)).name<<'"';}
    std::cout<<"],\"cell_types\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';std::cout<<'[';const auto& a=c.grade(topology::owned_cylinder_detail::grade(k));
        for(std::size_t i=0;i<a.cell_count();++i){if(i)std::cout<<',';if(k)std::cout<<a.relation_types[i];else std::cout<<"null";}std::cout<<']';}
    std::cout<<"],\"flags\":[";
    for(std::size_t k=0;k<c.grade_count();++k){if(k)std::cout<<',';const auto g=topology::owned_cylinder_detail::grade(k);const auto& a=c.grade(g);std::cout<<'[';
        for(std::size_t p=0;p<a.incidence_count();++p){if(p)std::cout<<',';std::cout<<'['<<a.upper_index[p]<<','<<a.lower_index[p]<<','<<a.occurrence_identities[p].value<<",\""<<c.incidence_coefficient(g,p)<<"\",[";
            for(std::size_t j=0;j<c.signature().aspects().size();++j){if(j)std::cout<<',';std::cout<<a.aspects.get(static_cast<composite::AspectId>(j),p);}std::cout<<"]]";}std::cout<<']';}
    std::cout<<"]}";
}
void json(const algebra::RetainedSourceMap& m) {
    const auto rows=[](const auto& matrix){std::cout<<'[';for(std::size_t k=0;k<matrix.size();++k){if(k)std::cout<<',';std::cout<<'[';for(std::size_t i=0;i<matrix[k].size();++i){if(i)std::cout<<',';std::cout<<matrix[k][i];}std::cout<<']';}std::cout<<']';};
    std::cout<<"{\"cells\":";rows(m.cells);std::cout<<",\"flags\":";rows(m.occurrences);std::cout<<'}';
}
void source_record(const inference::PreparedNativeCylinder& cylinder, const topology::OwnedRelationalHomotopy& numeric,unsigned kind) {
    const auto carrier=numeric.carrier();
    std::cout<<"{\"kind\":\"source\",\"id\":"<<kind<<",\"source\":";json(cylinder.factor(false).endpoint()->source);
    std::cout<<",\"interval\":";json(cylinder.factor(true).endpoint()->source);
    std::cout<<",\"carrier\":";json(carrier.carrier());std::cout<<",\"target\":";json(numeric.target());
    std::cout<<",\"type_interpretation\":[";const auto& d=carrier.declaration();const auto product=cylinder.tensor_product();
    for(std::size_t i=0;i<product.declaration().types.size();++i){if(i)std::cout<<',';const auto& t=product.declaration().types[i];std::cout<<'[';
        if(t.left)std::cout<<*t.left;else std::cout<<"null";std::cout<<',';if(t.right)std::cout<<*t.right;else std::cout<<"null";
        const auto it=std::find_if(d.types.begin(),d.types.end(),[&](const auto& value){return value.product_type==t.key;});std::cout<<',';if(it->carrier_type)std::cout<<*it->carrier_type;else std::cout<<"null";std::cout<<']';}
    std::cout<<"],\"cells\":[";
    for(std::size_t i=0;i<d.cells.size();++i){if(i)std::cout<<',';std::cout<<'[';json(d.cells[i].from);std::cout<<','<<d.cells[i].to<<']';}
    std::cout<<"],\"flags\":[";
    for(std::size_t i=0;i<d.occurrences.size();++i){if(i)std::cout<<',';const auto& f=d.occurrences[i];std::cout<<'[';json(f.from.upper);std::cout<<','<<static_cast<unsigned>(f.from.side)<<','<<f.from.occurrence.value<<','<<f.to<<']';}
    std::cout<<"],\"h\":";json(numeric.declaration().cylinder_target);std::cout<<",\"first\":";json(numeric.declaration().first);
    std::cout<<",\"second\":";json(numeric.declaration().second);std::cout<<"}\n";
}
core::RelationalComplex rebuild(const core::RelationalComplex& c,std::optional<std::size_t> flip={},bool extra=false) {
    core::RelationalComplexBuilder b(c.signature(),c.semantic_scale());b.declare_grade(c.max_grade());
    for(const auto key:c.grade(grade(0)).identities)b.declare_witness_identity(key);
    for(std::uint16_t k=1;k<c.grade_count();++k){const auto g=grade(k);const auto& a=c.grade(g);
        for(std::size_t i=0;i<a.cell_count();++i){std::vector<core::BoundaryOccurrenceInput> low;std::vector<core::HigherIncidenceInput> high;const auto [begin,end]=a.incidence_range(i);
            for(auto p=begin;p<end;++p){std::vector<core::AspectValue> aspects;
                for(std::size_t j=0;j<c.signature().aspects().size();++j)if(j!=c.signature().existence_aspect() && j!=c.signature().orientation_aspect())aspects.push_back({static_cast<composite::AspectId>(j),a.aspects.get(static_cast<composite::AspectId>(j),p)});
                const auto q=c.incidence_coefficient(g,p);const auto magnitude=q<0?-q:q;
                const bool negative=a.aspects.get(c.signature().orientation_aspect(),p)!=(k==1 && flip && *flip==p);
                const bool exists=a.aspects.get(c.signature().existence_aspect(),p);
                if(k==1)low.emplace_back(c.grade(grade(0)).identities[a.lower_index[p]],exists,negative,magnitude,aspects,a.occurrence_identities[p]);
                else high.emplace_back(a.lower_index[p],exists,negative,magnitude,aspects,a.occurrence_identities[p]);
            }
            if(k==1)b.add_grade_one(a.relation_identities[i],a.relation_types[i],low);else b.add_higher(g,a.relation_identities[i],a.relation_types[i],high);
        }
        if(k==1 && extra){const auto vertex=c.grade(grade(0)).identities.front();b.add_grade_one(core::RelationKey{999999},0,
            {{vertex,true,true,Q{257},{},core::OccurrenceKey{999991}}, {vertex,true,false,Q{257},{},core::OccurrenceKey{999992}}});}
    }return b.freeze();
}
std::vector<runtime::WorkAvailability*> roles(Availability& a) {
    std::vector<runtime::WorkAvailability*> out{&a.root.cylinder.left,&a.root.cylinder.right,&a.root.carrier,&a.root.target};
    for(auto& s:a.stages)out.insert(out.end(),{&s.domain.left,&s.domain.right,&s.carrier,&s.target});
    return out;
}
void record_witness(unsigned id,std::uint16_t k,const Values& v,const topology::RelationalHomotopyWitness& w) {
    std::cout<<"{\"kind\":\"witness\",\"id\":"<<id<<",\"grade\":"<<k<<",\"input\":";json(v);
    std::cout<<",\"source_boundary\":";const auto& cw=w.cylinder_witness();if(cw.source_boundary())json(cw.source_boundary()->values());else std::cout<<"null";
    std::cout<<",\"first\":";json(w.first().values());std::cout<<",\"second\":";json(w.second().values());std::cout<<",\"h\":";json(w.homotopy().values());
    std::cout<<",\"dh\":";json(w.boundary_homotopy().values());std::cout<<",\"hd\":";json(w.homotopy_boundary().values());
    std::cout<<",\"difference\":";json(w.endpoint_difference().values());std::cout<<",\"residual\":";json(w.residual().values());std::cout<<"}\n";
}
void exercise(unsigned kind,bool records) {
    const auto full=prepare(kind);const auto full_numeric=full.numeric_plan();
    auto p=Plan::prepare(full.root(),{1900,1,6},initial());std::optional<inference::NativeSequencedHomotopyWitness> earlier;
    for(unsigned depth=0;depth<=3;++depth) {
        auto a=full.preparation_availability();a.stages.resize(depth);
        if(depth)p=p.append(full.stage_domain(depth-1),full.stage_declaration(depth-1),a);
        const auto n=p.numeric_plan();const auto h=n.current();const auto domain=p.domain();const auto da=depth?a.stages.back().domain:a.root.cylinder;
        const auto& source=domain.factor(false).endpoint()->source;const unsigned id=kind*10+depth;
        check(p.stage_count()==depth && n.stage_count()==depth,"append lost stages");
        check(p.root().same_owner(full.root()) && n.root().same_owner(full_numeric.root()),"append changed root owner");
        if(depth) {
            const auto stage=n.stage(depth-1);const auto prev=depth==1?n.root():n.stage(depth-2).numeric_plan();
            check(stage.original().same_owner(prev),"numeric stage loses actual preceding homotopy");
            check(!n.same_owner(earlier->plan().numeric_plan()) && earlier->plan().stage_count()==depth-1,"append rewrites earlier result");
        } else check(!n.certificate(),"root invented identity operations");
        if(records) {
            source_record(domain,h,id);
            if(depth) {
                const auto stage=n.stage(depth-1);const auto& d=stage.declaration();const auto& c=*n.certificate();
                std::cout<<"{\"kind\":\"composition\",\"id\":"<<id<<",\"original\":"<<id-1<<",\"root\":"<<kind*10<<",\"depth\":"<<depth<<",\"before\":";json(d.before);
                std::cout<<",\"interval\":";json(d.interval);std::cout<<",\"after\":";json(d.after);std::cout<<",\"transport\":";json(stage.carrier_map());
                std::cout<<",\"cumulative\":{\"before\":";json(c.before);std::cout<<",\"interval\":";json(c.interval);
                std::cout<<",\"after\":";json(c.after);std::cout<<",\"transport\":";json(c.carrier);std::cout<<"}}\n";
            }
        }
        for(std::uint16_t k=0;k<source.grade_count();++k) {
            std::vector<Values> cases{{}};Values all;
            for(std::uint32_t i=0;i<source.grade(grade(k)).cell_count();++i) {
                for(const auto& q:std::array<Q,3>{Q{0},Q{2,5},Q{-3,7}})cases.push_back({{i,q}});
                all.emplace(i,i%2?Q{-2,9}:Q{4,11});
            }
            if(!all.empty())cases.push_back(all);
            if((kind==6 || kind==7) && k==1 && depth)cases.push_back({{0,Q{1}},{1,Q{-1}}});
            for(const auto& v:cases) {
                const auto input=domain.chain(native_chain(domain,grade(k),v),da);const auto receipt=p.witness(input,a);const auto& w=receipt.witness().witness();++witnesses;
                check(zero(w.residual().values()),"sequenced identity failed");check(receipt.plan().same_owner(p) && receipt.plan().root().same_owner(full.root()),"witness loses sequence or root");
                check(receipt.input()->native_input()->same_execution_authority(domain.factor(false)),"input authority lost");
                check(receipt.provenance().sequences.size()==4+4*depth && receipt.provenance().stages.size()==depth,"publication/history roles lost");
                check(receipt.provenance().root_preparation_availability.cylinder.left.max_sequence==11,"root preparation history changed");
                for(std::size_t j=0;j<depth;++j)check(receipt.provenance().stages[j].preparation_availability.stages.size()==j+1,"stage preparation history flattened");
                const auto out=p.homotopy(input,a);check(vals(out)==w.homotopy().values(),"prism differs");
                const auto boundary=p.boundary(out,a);check(vals(boundary)==w.boundary_homotopy().values(),"boundary differs");
                check(boundary.input()->plan().same_owner(p) && boundary.chain().input()->plan().same_owner(n),"boundary predecessor lost");
                check(vals(p.endpoint(input,Endpoint::Start,a))==w.first().values(),"start differs");
                check(vals(p.endpoint(input,Endpoint::Finish,a))==w.second().values(),"finish differs");
                const auto product=domain.homotopy(input,da);const auto push=p.push(product,a);++pushes;
                check(vals(push)==w.homotopy().values() && push.product_input()->plan().same_owner(domain),"product action or owner differs");
                if((kind==6 || kind==7) && k==1 && depth && v.size()==2 && v.at(0)==1)check(vals(out).size()==1 && zero(vals(out)),"cancellation loses support");
                if(records)record_witness(id,k,v,w);
            }
        }
        earlier=p.witness(domain.chain(grade(0),{},da),a);
    }
}
void contracts() {
    const auto root=seed();const auto p=prepare();const auto a=p.preparation_availability();const auto input=p.domain().chain(grade(1),{{0,Q{1}}},a.stages.back().domain);
    for(unsigned i=0;i<3;++i) {
        auto d=root.declaration();if(i==0)d.key=0;if(i==1)d.revision=0;if(i==2)d.max_stages=0;
        rejects([&]{Plan::prepare(root.root(),d,initial());},"invalid root declaration accepted");
    }
    auto bad=initial();bad.stages.push_back(a.stages[0]);rejects([&]{Plan::prepare(root.root(),root.declaration(),bad);},"future root availability accepted");
    const auto n=next(p,4);
    const auto wrong=[&](auto change) {auto r=n.request;change(r);rejects([&]{p.append(n.domain,r,n.availability);},"invalid stage accepted");};
    wrong([](auto& r){r.key=0;});wrong([](auto& r){r.revision=0;});wrong([](auto& r){r.realization.key=0;});
    wrong([](auto& r){r.key=1900;});wrong([](auto& r){r.key=1700;});wrong([](auto& r){r.key=1600;});
    wrong([](auto& r){r.before.cells.pop_back();});wrong([](auto& r){r.before.occurrences.pop_back();});wrong([](auto& r){r.before.occurrences.push_back(r.before.occurrences.front());});
    wrong([](auto& r){r.interval.cells.pop_back();});wrong([](auto& r){std::swap(r.interval.cells[0].to,r.interval.cells[1].to);});
    wrong([](auto& r){r.after.cells.pop_back();});wrong([](auto& r){r.after.occurrences.pop_back();});
    wrong([](auto& r){r.before.to=71;});wrong([](auto& r){r.interval.to=72;});wrong([](auto& r){r.after.from=74;});
    wrong([](auto& r){r.after.key=r.before.key;});wrong([](auto& r){r.before.key=1701;});wrong([](auto& r){r.before.key=1601;});
    wrong([](auto& r){r.realization.types.pop_back();});wrong([](auto& r){r.realization.cells.pop_back();});wrong([](auto& r){r.realization.occurrences.pop_back();});
    wrong([](auto& r){std::swap(r.realization.occurrences[0].to,r.realization.occurrences[1].to);});
    wrong([](auto& r){r.carrier_source=r.target_source;});wrong([](auto& r){r.carrier_source=73;});
    wrong([](auto& r){r.before.occurrences[4].to=r.before.occurrences[5].to;});
    const auto other=prepare();const auto oa=other.preparation_availability();const auto foreign=other.domain().chain(grade(1),{{0,Q{1}}},oa.stages.back().domain);
    rejects([&]{p.homotopy(foreign,a);},"equal values identify another domain");rejects([&]{p.push(other.domain().homotopy(foreign,oa.stages.back().domain),a);},"foreign product accepted");
    rejects([&]{p.boundary(other.homotopy(foreign,oa),a);},"foreign output accepted");
    rejects([&]{p.homotopy(p.stage_domain(0).chain(grade(1),{{0,Q{1}}},a.stages[0].domain),a);},"old domain substitutes for current domain");
    rejects([&]{p.endpoint(input,static_cast<Endpoint>(9),a);},"unknown endpoint accepted");
    rejects([&]{p.boundary(p.endpoint(p.domain().chain(grade(0),{},a.stages.back().domain),Endpoint::Start,a),a);},"negative target grade invented");
    auto short_root=seed(0,1);const auto short_plan=append(short_root,1);const auto second=next(short_plan,2);
    rejects([&]{short_plan.append(second.domain,second.request,second.availability);},"stage bound ignored");
    auto incomplete=a;incomplete.stages.pop_back();rejects([&]{p.witness(input,incomplete);},"missing prefix role accepted");
    auto extra=a;extra.stages.push_back(a.stages.back());rejects([&]{p.witness(input,extra);},"future stage role accepted");
    const auto numeric=p.numeric_plan();const auto stage=numeric.stage(2);auto d=stage.declaration();d.key=2900;auto* alias=&d.before.cells[1][0];bool changed=false;
    Guard capture({},[&]{if(!changed){changed=true;*alias=999;}});
    const auto stage_homotopy=stage.numeric_plan();
    const auto captured=numeric.append(stage.domain(),stage_homotopy.target(),std::move(d),capture);const auto captured_stage=captured.stage(3);
    check(changed && captured_stage.declaration().before.cells[1][0]==0,"moved map alias changes admitted stage");
    auto victim=numeric;bool replaced=false;const auto other_numeric=other.numeric_plan();Guard replacement({},[&]{if(!replaced){replaced=true;victim=other_numeric;}});
    const auto retained=victim.homotopy(input.chain(),replacement);check(replaced && retained.plan().same_owner(numeric) && victim.same_owner(other_numeric),"callback replacement changes active sequence");
    const auto plain=root.numeric_plan();Guard g;const auto same_root=topology::OwnedHomotopySequence::prepare(plain.root(),{1950,1,2},g);
    const auto xi=root.domain().chain(grade(1),{{0,Q{1}}},initial().root.cylinder);
    const auto first=plain.homotopy(xi.chain(),g);rejects([&]{Guard b;same_root.boundary(first,b);},"same root homotopy merges sequence output owners");
    auto future=std::async(std::launch::async,[p,input,a]{return vals(p.homotopy(input,a));});check(future.get()==vals(p.homotopy(input,a)),"concurrent scratch changes output");
    runtime::ExecutionHostPolicy policy;policy.backend=runtime::ExecutionBackend::ExactGpu;
    rejects([&]{p.witness(input,a,runtime::ExecutionLease(policy));},"unregistered GPU accepted");policy.fallback=runtime::BackendFallback::AllowExactCpuFallback;
    const auto fallback=p.witness(input,a,runtime::ExecutionLease(policy));check(fallback.provenance().fallback,"CPU fallback unreported");
    auto stop=std::make_shared<std::atomic_bool>(true);
    rejects<runtime::ExecutionCancelled>([&]{p.append(n.domain,n.request,n.availability,runtime::ExecutionLease({},stop));},"cancelled append runs");
    rejects<runtime::ExecutionCancelled>([&]{p.witness(input,a,runtime::ExecutionLease({},stop));},"cancelled action runs");
    for(unsigned i=0;i<6;++i) {
        policy={};if(i==0)policy.limits.records=0;if(i==1)policy.limits.rational_slots=0;if(i==2)policy.limits.sparse_entries=0;
        if(i==3)policy.limits.scalar_work=0;
        if(i==4)policy.limits.integer_bits=0;
        if(i==5)policy.allow_allocation=false;
        rejects<std::exception>([&]{p.witness(input,a,runtime::ExecutionLease(policy));},"action skips resource admission");
        rejects<std::exception>([&]{p.append(n.domain,n.request,n.availability,runtime::ExecutionLease(policy));},"append skips resource admission");
        rejects<std::exception>([&]{Plan::prepare(root.root(),root.declaration(),initial(),runtime::ExecutionLease(policy));},"root skips resource admission");
    }
    const auto h=numeric.current();const auto dc=h.carrier();auto cd=dc.declaration();const auto product=dc.cylinder().product();
    const auto unused=std::find_if(product.declaration().types.begin(),product.declaration().types.end(),[](const auto& t){return t.left==std::optional<core::RelationTypeId>{1} && t.right==std::optional<core::RelationTypeId>{0};});
    for(auto& t:cd.types)if(t.product_type==unused->key)t.carrier_type=0;
    Guard rg;const auto recoded=topology::OwnedCylinderCarrier::prepare(dc.cylinder(),dc.carrier(),cd,rg);d=stage.declaration();d.key=2901;
    rejects([&]{Guard b;numeric.append(recoded,h.target(),d,b);},"unused type interpretations lost after repetition");
    policy={};policy.limits.integer_bits=8;auto target_request=n.request;
    storage::NativeAccessionDirectory unused_target(storage::TemporalEndpoint(rebuild(target_request.target.endpoint()->source,{},true),target_request.target.sequence()),target_request.target.source_namespace());
    target_request.target=native_cylinder_example::anchor(unused_target);const auto with_unused=p.append(n.domain,target_request,n.availability);
    const auto empty=n.domain.chain(grade(1),{},n.availability.stages.back().domain);
    rejects<std::length_error>([&]{with_unused.homotopy(empty,n.availability,runtime::ExecutionLease(policy));},"empty input bypasses unused coefficients");
}
void associations(bool records) {
    const auto root=seed(9);const auto aa=append(root,1,1),ab=append(aa,2,2),bb=append(root,1,2),ba=append(bb,2,1),identity_twice=append(aa,2,1);
    const auto an=ab.numeric_plan(),bn=ba.numeric_plan(),id=identity_twice.numeric_plan();const auto original=root.numeric_plan();
    check(an.certificate()->before.occurrences!=bn.certificate()->before.occurrences,"noncommuting original flag maps collapsed");
    check(id.certificate()->before.cells==identity(root.domain().factor(false).endpoint()->source).cells && id.certificate()->before.occurrences==identity(root.domain().factor(false).endpoint()->source).occurrences,"explicit flag inverse pair fails source identity");
    const auto identity_h=id.current(),root_h=original.current();
    check(identity_h.declaration().cylinder_target.cells==root_h.declaration().cylinder_target.cells && identity_h.declaration().cylinder_target.occurrences==root_h.declaration().cylinder_target.occurrences,"identity composition changes full H");
    check(id.stage_count()==2 && !id.same_owner(original),"identity operations erased from history");
    const auto ai=ab.domain().chain(grade(1),{{0,Q{1}}},ab.preparation_availability().stages.back().domain),bi=ba.domain().chain(grade(1),{{0,Q{1}}},ba.preparation_availability().stages.back().domain);
    check(vals(ab.homotopy(ai,ab.preparation_availability()))==vals(ba.homotopy(bi,ba.preparation_availability())),"zero flag permutations change numbers");
    const auto& c=*an.certificate();const auto current=an.current();auto r=ab.stage_declaration(1);r.key=3100;r.before.key=3101;r.interval.key=3102;r.after.key=3103;
    const auto domain=ab.domain(),old=root.domain();
    r.before=source_map(domain.factor(false).endpoint()->source,old.factor(false).endpoint()->source,c.before,3101,domain.declaration().left_factor,old.declaration().left_factor);
    r.interval=source_map(domain.factor(true).endpoint()->source,old.factor(true).endpoint()->source,c.interval,3102,domain.declaration().right_factor,old.declaration().right_factor);
    const auto native_root=root.root();
    r.after=source_map(native_root.declaration().target.endpoint()->source,r.target.endpoint()->source,c.after,3103,native_root.declaration().target_source,r.target_source);
    const auto flat=root.append(domain,r,{initial().root,{ab.preparation_availability().stages.back()}});const auto fn=flat.numeric_plan();
    const auto flat_h=fn.current();
    check(flat_h.declaration().cylinder_target.cells==current.declaration().cylinder_target.cells && flat_h.declaration().cylinder_target.occurrences==current.declaration().cylinder_target.occurrences,"full source association fails");
    check(flat_h.declaration().first.occurrences==current.declaration().first.occurrences && flat_h.declaration().second.occurrences==current.declaration().second.occurrences,"association changes restrictions");
    check(flat.stage_count()==1 && ab.stage_count()==2 && !flat.same_owner(ab),"association erased intermediate history");
    if(records) {
        std::cout<<"{\"kind\":\"association\",\"ab\":";json(an.certificate()->before);std::cout<<",\"ba\":";json(bn.certificate()->before);
        std::cout<<",\"identity\":";json(id.certificate()->before);std::cout<<",\"source_identity\":";json(identity(old.factor(false).endpoint()->source));
        std::cout<<",\"sequential_h\":";json(current.declaration().cylinder_target);std::cout<<",\"flat_h\":";json(flat_h.declaration().cylinder_target);std::cout<<"}\n";
    }
    // Three explicit identity operations reuse the actual four publications.
    auto same=root;const auto cylinder=root.domain();const auto& original_request=native_root.declaration();
    for(std::uint64_t j=0;j<3;++j) {
        const auto key=4000+10*j;
        inference::NativeHomotopyCompositionRequest request{key,1,original_request.carrier_source,original_request.target_source,original_request.carrier,original_request.target,original_request.realization,
            source_map(cylinder.factor(false).endpoint()->source,cylinder.factor(false).endpoint()->source,identity(cylinder.factor(false).endpoint()->source),key+1,71,71),
            source_map(cylinder.factor(true).endpoint()->source,cylinder.factor(true).endpoint()->source,identity(cylinder.factor(true).endpoint()->source),key+2,72,72),
            source_map(original_request.target.endpoint()->source,original_request.target.endpoint()->source,identity(original_request.target.endpoint()->source),key+3,74,74)};
        auto a=same.preparation_availability();a.stages.push_back({initial().root.cylinder,initial().root.carrier,initial().root.target});same=same.append(cylinder,request,a);
    }
    const auto a=same.preparation_availability();const auto x=cylinder.chain(grade(1),{{0,Q{1}}},initial().root.cylinder);const auto receipt=same.witness(x,a);
    const auto zero_stage=root.witness(x,initial());check(receipt.witness().witness().homotopy().values()==zero_stage.witness().witness().homotopy().values(),"explicit source identity changes action");
    check(receipt.provenance().sequences.size()==16 && same.domain().same_owner(cylinder),"shared roles or owners lost");
    for(std::size_t j=0;j<16;++j){auto bad=a;--roles(bad)[j]->max_sequence;rejects([&]{same.witness(x,bad);},"shared role cutoff omitted");}
    auto more=next(same,4);more.request.carrier_source=original_request.carrier_source;storage::NativeAccessionDirectory duplicate(*original_request.carrier.endpoint(),original_request.carrier.source_namespace());
    more.request.carrier=native_cylinder_example::anchor(duplicate);more.availability.stages.back().carrier=initial().root.carrier;
    rejects([&]{same.append(more.domain,more.request,more.availability);},"equal publication values merge distinct authority");
}
void temporal_cases() {
    const auto p=prepare(1);const auto a=p.preparation_availability();const auto domain=p.domain();
    const auto input=domain.chain(native_chain(domain,grade(1),{{0,Q{1}}}),a.stages.back().domain);const auto receipt=p.witness(input,a);
    const auto product=domain.homotopy(input,a.stages.back().domain);const auto out=p.homotopy(input,a);const auto n=next(p,4);
    for(std::size_t i=0;i<16;++i) {
        auto bad=a;--roles(bad)[i]->max_sequence;
        rejects([&]{p.witness(input,bad);},"witness omits prior cutoff");rejects([&]{p.homotopy(input,bad);},"action omits prior cutoff");
        rejects([&]{p.endpoint(input,Endpoint::Start,bad);},"start omits prior cutoff");rejects([&]{p.endpoint(input,Endpoint::Finish,bad);},"finish omits prior cutoff");
        rejects([&]{p.push(product,bad);},"push omits prior cutoff");rejects([&]{p.boundary(out,bad);},"boundary omits prior cutoff");
        auto append_bad=n.availability;--roles(append_bad)[i]->max_sequence;rejects([&]{p.append(n.domain,n.request,append_bad);},"append omits prior cutoff");
        bad=a;++roles(bad)[i]->source_namespace;rejects([&]{p.witness(input,bad);},"foreign namespace accepted");
    }
    for(std::size_t i=16;i<20;++i) {auto bad=n.availability;--roles(bad)[i]->max_sequence;rejects([&]{p.append(n.domain,n.request,bad);},"new stage omits cutoff");}
    // Advance every directory; construct a later sequence from its own actual pins.
    const auto native_root=p.root();
    std::vector<storage::PinnedNativeAccession> pins{native_root.cylinder().factor(false),native_root.cylinder().factor(true),native_root.declaration().carrier,native_root.declaration().target};
    for(std::size_t j=0;j<p.stage_count();++j)pins.insert(pins.end(),{p.stage_domain(j).factor(false),p.stage_domain(j).factor(true),p.stage_declaration(j).carrier,p.stage_declaration(j).target});
    std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
    for(const auto& pin:pins){auto dir=std::make_unique<storage::NativeAccessionDirectory>(*pin.endpoint(),pin.source_namespace());const auto old=dir->pin_snapshot();auto next_endpoint=*old;++next_endpoint.sequence;
        dir->advance(storage::make_temporal_segment(*old,next_endpoint));dirs.push_back(std::move(dir));}
    auto later=a;for(auto* role:roles(later))++role->max_sequence;
    const auto root_cylinder=p.root().cylinder();auto dc=root_cylinder.declaration();dc.left=native_cylinder_example::anchor(*dirs[0]);dc.right=native_cylinder_example::anchor(*dirs[1]);
    const auto nc=Cylinder::prepare(dc,later.root.cylinder);auto rr=native_root.declaration();rr.carrier=native_cylinder_example::anchor(*dirs[2]);rr.target=native_cylinder_example::anchor(*dirs[3]);
    auto newer=Plan::prepare(inference::PreparedNativeRelationalHomotopy::prepare(nc,rr,later.root),p.declaration(),{later.root,{}});
    for(std::size_t j=0;j<3;++j) {
        const auto old_domain=p.stage_domain(j);auto d=old_domain.declaration();d.left=native_cylinder_example::anchor(*dirs[4+4*j]);d.right=native_cylinder_example::anchor(*dirs[5+4*j]);
        const auto cylinder=Cylinder::prepare(d,later.stages[j].domain);auto r=p.stage_declaration(j);r.carrier=native_cylinder_example::anchor(*dirs[6+4*j]);r.target=native_cylinder_example::anchor(*dirs[7+4*j]);
        auto cutoff=later;cutoff.stages.resize(j+1);auto bad=cutoff;--roles(bad)[4+4*j]->max_sequence;
        rejects([&]{newer.append(cylinder,r,bad);},"later source accepted at earlier cutoff");newer=newer.append(cylinder,r,cutoff);
    }
    const auto new_input=newer.domain().chain(grade(1),{{0,Q{1}}},later.stages.back().domain);
    rejects([&]{newer.homotopy(input,later);},"old input adopts later owner");rejects([&]{p.homotopy(new_input,a);},"future input leaks to retained owner");
    const auto unchanged=p.witness(input,a);
    check(unchanged.witness().witness().homotopy().values()==receipt.witness().witness().homotopy().values(),"advancing directories changes old result");
    const auto fresh=newer.witness(new_input,later);for(std::size_t i=0;i<16;++i)check(fresh.provenance().sequences[i]==receipt.provenance().sequences[i]+1,"later history substitutes old cutoff");
}
void native_limit_cases() {
    const auto reference=prepare(1);const auto a=reference.preparation_availability();const auto reference_domain=reference.domain();
    const auto x=reference_domain.chain(grade(1),{{0,Q{253,251}}},a.stages.back().domain);const auto receipt=reference.witness(x,a);
    const auto work_cap=receipt.provenance().execution_steps-1;const auto native_root=reference.root();
    std::vector<storage::PinnedNativeAccession> pins{native_root.cylinder().factor(false),native_root.cylinder().factor(true),native_root.declaration().carrier,native_root.declaration().target};
    for(std::size_t j=0;j<3;++j)pins.insert(pins.end(),{reference.stage_domain(j).factor(false),reference.stage_domain(j).factor(true),reference.stage_declaration(j).carrier,reference.stage_declaration(j).target});
    for(unsigned mode=0;mode<2;++mode)for(std::size_t role=0;role<16;++role) {
        std::vector<std::unique_ptr<storage::NativeAccessionDirectory>> dirs;
        for(std::size_t i=0;i<pins.size();++i) {
            storage::NativeAccessionLimits limits;
            if(i==role) {if(mode==0)limits.max_action_work=work_cap;else limits.max_integer_bytes=1;}
            dirs.push_back(std::make_unique<storage::NativeAccessionDirectory>(*pins[i].endpoint(),pins[i].source_namespace(),
                std::vector<storage::RelationalCodeObservationQuery>{},[](auto bytes){return storage::rfcx_detail::fnv1a64(bytes);},limits));
        }
        const auto root_cylinder=native_root.cylinder();auto d=root_cylinder.declaration();d.left=native_cylinder_example::anchor(*dirs[0]);d.right=native_cylinder_example::anchor(*dirs[1]);
        const auto cylinder=Cylinder::prepare(d,a.root.cylinder);auto root_request=native_root.declaration();root_request.carrier=native_cylinder_example::anchor(*dirs[2]);root_request.target=native_cylinder_example::anchor(*dirs[3]);
        auto p=Plan::prepare(inference::PreparedNativeRelationalHomotopy::prepare(cylinder,root_request,a.root),reference.declaration(),{a.root,{}});
        for(std::size_t j=0;j<3;++j) {
            const auto old=reference.stage_domain(j);auto domain_request=old.declaration();domain_request.left=native_cylinder_example::anchor(*dirs[4+4*j]);domain_request.right=native_cylinder_example::anchor(*dirs[5+4*j]);
            const auto domain=Cylinder::prepare(domain_request,a.stages[j].domain);auto r=reference.stage_declaration(j);r.carrier=native_cylinder_example::anchor(*dirs[6+4*j]);r.target=native_cylinder_example::anchor(*dirs[7+4*j]);
            auto availability=a;availability.stages.resize(j+1);p=p.append(domain,r,availability);
        }
        const auto input=p.domain().chain(grade(1),{{0,Q{253,251}}},a.stages.back().domain);
        rejects<std::length_error>([&]{p.witness(input,a);},"an earlier native publication cap is ignored");
        // Empty work still charges the whole spine under every scalar-work cap.
        if(mode==0) {
            const auto empty=p.domain().chain(grade(1),{},a.stages.back().domain);
            const auto observed=p.homotopy(empty,a);
            check(observed.provenance().limits.steps==work_cap,"retained work cap absent from empty action");
        }
    }
}
template<class T>concept TemporaryDeclaration=requires(T t){std::move(t).declaration();};
template<class T>concept TemporaryChain=requires(T t){std::move(t).chain();};
template<class T>concept TemporaryWitness=requires(T t){std::move(t).witness();};
template<class T>concept TemporaryCertificate=requires(T t){std::move(t).certificate();};
template<class T>concept TemporaryStage=requires(T t){std::move(t).stage_declaration(0);};
static_assert(!TemporaryDeclaration<Plan> && !TemporaryDeclaration<topology::OwnedHomotopySequence> && !TemporaryStage<Plan>);
static_assert(!TemporaryChain<Output> && !TemporaryChain<topology::SequencedHomotopyChain>);
static_assert(!TemporaryWitness<inference::NativeSequencedHomotopyWitness> && !TemporaryWitness<topology::SequencedHomotopyWitness> && !TemporaryCertificate<topology::OwnedHomotopySequence>);
} // namespace
int main(int argc,char** argv) {
    try {
        const bool records=argc==2 && std::string_view(argv[1])=="--records";
        for(unsigned kind=0;kind<10;++kind)exercise(kind,records);
        contracts();associations(records);temporal_cases();native_limit_cases();
        if(!records)std::cout<<"Checks: "<<checks<<"; witnesses: "<<witnesses<<"; product pushes: "<<pushes<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
