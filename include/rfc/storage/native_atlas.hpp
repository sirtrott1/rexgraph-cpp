#pragma once

#include "rfc/storage/native_atlas_types.hpp"

namespace rfc::storage::native_atlas_detail {

template<class T> inline void keyed(std::vector<T>& xs,Budget& records) {
    records.take(xs.size());std::sort(xs.begin(),xs.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    std::uint64_t last=0;for(const auto& x:xs){require(x.key && x.key!=last,"duplicate/zero atlas declaration key");last=x.key;}
}
template<class T> inline const T& named(const std::vector<T>& xs,std::uint64_t key) {
    const auto it=std::lower_bound(xs.begin(),xs.end(),key,[](const auto& a,auto b){return a.key<b;});
    require(it!=xs.end() && it->key==key,"unknown atlas declaration");return *it;
}
template<class Key> struct Equivalences {
    std::map<Key,Key> parents;
    Key root(Key k) const {
        auto it=parents.find(k);
        while(it!=parents.end() && it->second!=k){k=it->second;it=parents.find(k);}return k;
    }
    void join(Key a,Key b){a=root(a);b=root(b);if(a==b)return;if(b<a)std::swap(a,b);parents[b]=a;parents.try_emplace(a,a);}
    void coherent() const {
        std::set<std::pair<Key,std::uint64_t>> seen;
        for(const auto& [k,p]:parents){(void)p;require(seen.emplace(root(k),k.source).second,
            "incoherent atlas cycle identifies distinct cells/occurrences within one source");}
    }
};
struct State {
    NativeAtlasManifest manifest;
    std::map<std::uint64_t,NativeAtlasSource> sources;
    std::map<std::uint64_t,std::unique_ptr<StableSourceIndex>> indexes;
    Equivalences<NativeAtlasCell> cells;
    Equivalences<NativeAtlasOccurrence> occurrences;
    NativeAtlasLimits limits;
    const NativeAtlasSource& source(std::uint64_t key) const {return sources.at(key);}
    const core::RelationalComplex& complex(std::uint64_t key) const{return source(key).endpoint()->source;}
    core::CellId cell(std::uint64_t s,StableCellKey k) const {
        const auto out=indexes.at(s)->cell(k);require(out.has_value(),"atlas cell absent");return *out;
    }
    std::size_t occurrence(std::uint64_t s,StableOccurrenceKey k) const {
        const auto out=indexes.at(s)->occurrence(k);require(out.has_value(),"atlas occurrence absent");return *out;
    }
    PinnedNativeAccession tensor(std::uint64_t s,std::uint64_t k)const{return source(s).access(tensor_request(k));}
    const composite::Declaration& declaration(std::uint64_t s,composite::DeclarationId id) const {
        const auto p=source(s).endpoint();require(p->declarations.has_value(),"atlas source has no F1 declarations");
        return p->declarations->registry.at(id);
    }
};
inline core::RelationalComplex recode(const State& s,std::uint64_t key,const std::optional<SourceDeclarationMigration>& m) {
    return m?recode_temporal_source(s.complex(key),*m,s.limits.source_declarations):s.complex(key);
}
inline void normalize_columns(std::vector<NativeChainColumn>& columns,Budget& records,const NativeAtlasLimits& limits) {
    records.take(columns.size());std::sort(columns.begin(),columns.end(),[](const auto& a,const auto& b){return a.source<b.source;});
    std::optional<StableCellKey> prev;
    for(auto& c:columns){require(!prev || *prev!=c.source,"duplicate atlas chain column");prev=c.source;
        records.take(c.image.size());std::sort(c.image.begin(),c.image.end(),[](const auto& a,const auto& b){return a.target<b.target;});
        std::optional<StableCellKey> last;
        for(const auto& t:c.image){require(t.target.grade==c.source.grade && (!last || *last!=t.target) && t.coefficient!=0,
            "invalid/duplicate/zero atlas chain term");last=t.target;rational(t.coefficient,limits);}
    }
}
inline std::size_t normalize(NativeAtlasManifest& m,const NativeAtlasLimits& limits) {
    require(m.key && limits.max_sources && limits.max_work,"invalid atlas identity/policy");
    Budget records{limits.max_records};
    auto defs=machine::checked_size_add(m.embeddings.size(),m.pushouts.size());
    for(const auto n:{m.products.size(),m.scale_maps.size(),m.scale_paths.size(),m.linear_interfaces.size(),m.finite_interfaces.size()})defs=machine::checked_size_add(defs,n);
    bound(defs,limits.max_definitions,"atlas definition budget");
    keyed(m.embeddings,records);keyed(m.pushouts,records);keyed(m.products,records);keyed(m.scale_maps,records);
    keyed(m.scale_paths,records);keyed(m.linear_interfaces,records);keyed(m.finite_interfaces,records);
    for(auto& e:m.embeddings){
        records.take(e.cells.size());records.take(e.occurrences.size());
        std::sort(e.cells.begin(),e.cells.end(),[](const auto& a,const auto& b){return a.from<b.from;});
        std::sort(e.occurrences.begin(),e.occurrences.end(),[](const auto& a,const auto& b){return a.from<b.from;});
    }
    for(auto& h:m.scale_maps){normalize_columns(h.realization,records,limits);if(h.retraction)normalize_columns(*h.retraction,records,limits);}
    for(const auto& p:m.scale_paths){require(!p.left.empty() && !p.right.empty(),"empty hierarchy path");
        bound(p.left.size(),limits.max_path_length,"hierarchy path length");bound(p.right.size(),limits.max_path_length,"hierarchy path length");
        records.take(p.left.size());records.take(p.right.size());}
    for(const auto& f:m.linear_interfaces){records.take(f.legs.size());for(const auto& l:f.legs)matrix(l.restriction,limits,records);
        if(f.common_operation)matrix(*f.common_operation,limits,records);}
    for(auto& f:m.finite_interfaces){records.take(f.legs.size());for(auto& l:f.legs){records.take(l.restriction.size());
        std::sort(l.restriction.begin(),l.restriction.end());}}
    return limits.max_records-records.remaining;
}
inline void validate_embedding(State& s,const NativeSourceEmbedding& e,Budget& work) {
    require(e.from!=e.to,"self embedding must not identify retained identities");
    const auto from=recode(s,e.from,e.declarations);const auto& to=s.complex(e.to);
    require(from.semantic_scale()==to.semantic_scale(),"source embedding is not a semantic-scale realization");
    require(source_declaration_detail::signature_bytes(from.signature())==source_declaration_detail::signature_bytes(to.signature()),
            "embedding signatures need explicit compatible recoding");
    require(from.grade_count()<=to.grade_count(),"embedding target loses declared grades");
    std::map<StableCellKey,StableCellKey> cm;std::set<StableCellKey> targets;
    for(const auto& p:e.cells){work.take();require(p.from.grade==p.to.grade && cm.emplace(p.from,p.to).second && targets.insert(p.to).second,
        "noninjective/duplicate embedding cell map");(void)s.cell(e.from,p.from);(void)s.cell(e.to,p.to);}
    std::map<StableOccurrenceKey,StableOccurrenceKey> om;std::set<StableOccurrenceKey> ot;
    for(const auto& p:e.occurrences){work.take();require(p.from.upper_grade==p.to.upper_grade && om.emplace(p.from,p.to).second && ot.insert(p.to).second,
        "noninjective/duplicate embedding occurrence map");(void)s.occurrence(e.from,p.from);(void)s.occurrence(e.to,p.to);}
    std::size_t cells=0,occurrences=0;
    for(std::size_t k=0;k<from.grade_count();++k){
        const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto& a=from.grade(g);const auto& b=to.grade(g);
        cells=machine::checked_size_add(cells,a.cell_count());occurrences=machine::checked_size_add(occurrences,a.incidence_count());
        for(std::size_t i=0;i<a.cell_count();++i){work.take();const auto key=stable_cell_key(from,{g,static_cast<std::uint32_t>(i)});
            const auto target=cm.at(key);const auto ti=s.cell(e.to,target).index;
            if(!k)continue;
            require(a.relation_types[i]==b.relation_types[ti],"embedding type mismatch");
            const auto [ab,ae]=a.incidence_range(i);const auto [bb,be]=b.incidence_range(ti);
            require(ae-ab==be-bb,"embedding must preserve complete retained occurrence fiber");
            const bool ordered=from.signature().type(a.relation_types[i]).is_attaching_loop();
            for(auto p=ab;p<ae;++p){work.take(from.signature().aspects().size()+1);
                const StableOccurrenceKey ok{g,a.occurrence_identities[p]};const auto tk=om.at(ok);const auto q=s.occurrence(e.to,tk);
                require(q>=bb && q<be && (!ordered || p-ab==q-bb),"embedding ownership/attaching-order mismatch");
                const auto low=core::Grade{static_cast<std::uint16_t>(k-1)};
                require(cm.at(stable_cell_key(from,{low,a.lower_index[p]}))==stable_cell_key(to,{low,b.lower_index[q]}),"embedding attachment mismatch");
                require(from.incidence_coefficient(g,p)==to.incidence_coefficient(g,q),"embedding exact coefficient mismatch");
                for(std::size_t j=0;j<from.signature().aspects().size();++j){const auto aspect=static_cast<composite::AspectId>(j);
                    require(a.aspects.get(aspect,p)==b.aspects.get(aspect,q),"embedding retained aspect mismatch");}
            }
        }
    }
    require(cm.size()==cells && om.size()==occurrences,"embedding omits retained data");
    for(const auto& p:e.cells)s.cells.join({e.from,p.from},{e.to,p.to});
    for(const auto& p:e.occurrences)s.occurrences.join({e.from,p.from},{e.to,p.to});
}
inline std::vector<geometry::LinearGradeMap> chain_columns(const State& s,std::uint64_t from,std::uint64_t to,
                                                         const std::vector<NativeChainColumn>& columns,Budget& work) {
    const auto& a=s.complex(from);const auto& b=s.complex(to);
    require(b.grade_count()>=a.grade_count(),"hierarchy target grade inventory");
    std::map<StableCellKey,const NativeChainColumn*> index;for(const auto& c:columns){work.take();require(index.emplace(c.source,&c).second,"duplicate hierarchy column");}
    std::vector<geometry::LinearGradeMap> out;std::size_t count=0;
    for(std::size_t k=0;k<a.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto n=a.grade(g).cell_count();work.take(n);
        out.emplace_back(n);count=machine::checked_size_add(count,n);
        for(std::size_t i=0;i<n;++i){const auto key=stable_cell_key(a,{g,static_cast<std::uint32_t>(i)});
            const auto* c=index.at(key);for(const auto& t:c->image){work.take();require(t.target.grade==g,"hierarchy term changes chain grade");
                out.back()[i].push_back({s.cell(to,t.target).index,t.coefficient});}
        }
    }
    require(count==columns.size(),"hierarchy must declare every column, including zero columns");return out;
}
inline void chain_check_budget(const geometry::RelationalChainMap& h,Budget& work) {
    const auto& a=h.source();const auto& b=h.target();
    for(std::size_t k=1;k<a.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto low=core::Grade{static_cast<std::uint16_t>(k-1)};
        const auto& ar=a.grade(g);const auto& br=b.grade(g);work.take(ar.cell_count());
        for(std::size_t c=0;c<ar.cell_count();++c){const auto [begin,end]=ar.incidence_range(c);
            work.take(end-begin);for(auto p=begin;p<end;++p)work.take(h.image(low,ar.lower_index[p]).size());
            for(const auto& t:h.image(g,c)){const auto [x,y]=br.incidence_range(t.target);work.take(y-x+1);}
        }
    }
}
inline geometry::RelationalChainMap chain_map(const State& s,const NativeScaleMap& h,Budget& work,bool reverse=false) {
    if(reverse){require(h.retraction.has_value(),"hierarchy has no retraction");return {s.complex(h.to),s.complex(h.from),chain_columns(s,h.to,h.from,*h.retraction,work)};}
    return {s.complex(h.from),s.complex(h.to),chain_columns(s,h.from,h.to,h.realization,work)};
}
inline geometry::RelationalChainMap compose(const geometry::RelationalChainMap& a,const geometry::RelationalChainMap& b,Budget& work) {
    require(&a.target()==&b.source(),"hierarchy path endpoint mismatch");
    for(std::size_t k=0;k<a.source().grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};work.take(a.source().grade(g).cell_count());
        for(std::size_t i=0;i<a.source().grade(g).cell_count();++i)for(const auto& x:a.image(g,i)){work.take();work.take(b.image(g,x.target).size());}}
    return geometry::compose_chain_maps(a,b);
}
inline geometry::RelationalChainMap path(const State& s,const std::vector<std::uint64_t>& keys,Budget& work) {
    require(!keys.empty(),"empty hierarchy path");auto out=chain_map(s,named(s.manifest.scale_maps,keys.front()),work);
    for(std::size_t i=1;i<keys.size();++i){auto next=chain_map(s,named(s.manifest.scale_maps,keys[i]),work);out=compose(out,next,work);}return out;
}

inline NativeGluedSource glue(const State& s,const NativePushoutDeclaration& p,Budget& work) {
    const auto& el=named(s.manifest.embeddings,p.left_embedding);const auto& er=named(s.manifest.embeddings,p.right_embedding);
    require(el.from==er.from && el.to!=er.to,"pushout requires one common interface and distinct participants");
    const auto left=recode(s,el.to,p.left_common),right=recode(s,er.to,p.right_common);
    require(left.semantic_scale()==right.semantic_scale(),"pushout cannot silently identify semantic scales");
    require(source_declaration_detail::signature_bytes(left.signature())==source_declaration_detail::signature_bytes(right.signature()),"pushout needs common declaration environment");
    Equivalences<NativeAtlasCell> cells;Equivalences<NativeAtlasOccurrence> occurrences;
    std::map<StableCellKey,StableCellKey> rc;for(const auto& x:er.cells)rc.emplace(x.from,x.to);
    for(const auto& x:el.cells)cells.join({el.to,x.to},{er.to,rc.at(x.from)});
    std::map<StableOccurrenceKey,StableOccurrenceKey> ro;for(const auto& x:er.occurrences)ro.emplace(x.from,x.to);
    for(const auto& x:el.occurrences)occurrences.join({el.to,x.to},{er.to,ro.at(x.from)});
    std::map<NativeAtlasCell,StableCellKey> ckeys;std::map<NativeAtlasOccurrence,StableOccurrenceKey> okeys;
    const auto each=[&](auto&& fn){fn(el.to,left);fn(er.to,right);};
    const auto grades=std::max(left.grade_count(),right.grade_count());
    each([&](auto key,const auto& source){for(std::size_t k=0;k<source.grade_count();++k){const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto& a=source.grade(g);
        work.take(a.cell_count());work.take(a.incidence_count());
        for(std::size_t i=0;i<a.cell_count();++i)ckeys.try_emplace(cells.root({key,stable_cell_key(source,{g,static_cast<std::uint32_t>(i)})}),StableCellKey{});
        for(const auto o:a.occurrence_identities)okeys.try_emplace(occurrences.root({key,{g,o}}),StableOccurrenceKey{});
    }});
    // Deterministic fresh materialization keys. The namespaced provenance maps,
    // not these local allocator ordinals, retain external source identity.
    std::uint64_t next=1;for(auto& [origin,key]:ckeys)key={origin.cell.grade,next++};next=1;
    for(auto& [origin,key]:okeys)key={origin.occurrence.upper_grade,core::OccurrenceKey{next++}};
    std::vector<temporal_source_detail::GradeImage> images(grades);
    std::map<NativeAtlasCell,StableCellKey> cm;std::map<NativeAtlasOccurrence,StableOccurrenceKey> om;
    each([&](auto key,const auto& source){for(std::size_t k=0;k<source.grade_count();++k){
        const auto g=core::Grade{static_cast<std::uint16_t>(k)};const auto image=temporal_source_detail::capture(source,k);auto& target=images[k];
        for(const auto old:image.order){const NativeAtlasCell orig{key,{g,old}};const auto nk=ckeys.at(cells.root(orig));cm.emplace(orig,nk);
            const auto type=image.cells.at(old);const auto [it,inserted]=target.cells.emplace(nk.key,type);require(inserted || it->second==type,"shared relation changes type in common declarations");
            if(!k)continue;
            std::vector<std::uint64_t> fiber;
            for(const auto old_o:image.fibers.at(old)){work.take(source.signature().aspects().size()+1);
                const auto oo=image.occurrences.at(old_o);const NativeAtlasOccurrence orig_o{key,{g,core::OccurrenceKey{old_o}}};
                const auto no=okeys.at(occurrences.root(orig_o));om.emplace(orig_o,no);
                const auto low=ckeys.at(cells.root({key,{core::Grade{static_cast<std::uint16_t>(k-1)},oo.lower}}));
                TemporalOccurrence value{no.key.value,nk.key,low.key,oo.coefficient,oo.aspects};
                const auto [oi,added]=target.occurrences.emplace(no.key.value,value);
                require(added || oi->second==value,"shared occurrence has incompatible common declaration/attachment/data");fiber.push_back(no.key.value);
            }
            const auto [fi,added]=target.fibers.emplace(nk.key,fiber);
            if(!added){if(source.signature().type(type).is_attaching_loop())require(fi->second==fiber,"shared attaching order mismatch");
                else {auto a=fi->second;std::sort(a.begin(),a.end());std::sort(fiber.begin(),fiber.end());require(a==fiber,"shared occurrence fiber mismatch");}}
        }
    }});
    for(auto& g:images)for(const auto& [key,type]:g.cells){(void)type;g.order.push_back(key);}
    return {temporal_source_detail::materialize_images(images,left.signature(),left.semantic_scale()),std::move(cm),std::move(om)};
}

inline std::size_t tensor_dimension(const composite::Declaration& d) {
    require(d.kind==composite::DeclarationKind::RationalTensor,"common interface must declare a rational tensor carrier");
    std::size_t n=1;for(const auto& a:d.axes)n=machine::checked_size_mul(n,a.extent);return n;
}
inline NativeLinearInterfaceReport linear_report(const State& s,const NativeLinearInterface& f,Budget& work) {
    require(f.field_identity && f.legs.size()>=2,"interface needs declared field identity and at least two legs");
    const auto size=tensor_dimension(s.declaration(f.common_source,f.common_type));bound(size,s.limits.max_dimension,"common interface carrier budget");
    if(f.common_operation)require(f.common_operation->rows()==size && f.common_operation->cols()==size,"common operation carrier");
    NativeLinearInterfaceReport report;std::set<std::pair<std::uint64_t,std::uint64_t>> legs;
    if(f.common_operation)report.operations_descend=true;
    for(const auto& leg:f.legs){
        require(legs.emplace(leg.source,leg.field).second,"duplicate interface leg");
        const auto access=s.tensor(leg.source,leg.field);const auto tensor=access.tensor_declaration();
        require(tensor->role==TemporalTensorRole::Field && tensor->output.has_value(),"linear interface requires a typed field");
        const auto& t=tensor->tensor;require(leg.restriction.cols()==t.domain().logical_size() && leg.restriction.rows()==size,"restriction carrier mismatch");
        numeric::RationalVector value(size,numeric::Rational{0});work.take(size);
        for(const auto& [ij,q]:leg.restriction.entries()){work.take();require(t.support().contains(ij.second),"restriction reads outside field structural support");
            value[ij.first]+=q*(*t.state(ij.second));}
        for(auto& q:value)q.canonicalize();
        report.restrictions.push_back(std::move(value));
        require(leg.operation.has_value()==f.common_operation.has_value(),"operation descent requires complete local/common operation declarations");
        if(leg.operation){NativeAccessionRequest op_request;op_request.kind=NativeAccessionKind::Operator;op_request.subject=*leg.operation;
            const auto op=s.source(leg.source).access(op_request).tensor_declaration();
            require(op->input==tensor->output && op->output==tensor->output,"local operation carrier identity mismatch");
            const auto matrix=temporal_tensor_matrix(*op);
            auto defect=multiply(leg.restriction,matrix,work).add(multiply(*f.common_operation,leg.restriction,work).scale(numeric::Rational{-1}));
            if(defect.nonzeros())report.operations_descend=false;
            report.operation_defects.push_back(std::move(defect));
        }
    }
    for(std::size_t i=1;i<report.restrictions.size();++i){work.take(size);auto diff=report.restrictions[i];
        for(std::size_t j=0;j<size;++j){diff[j]-=report.restrictions.front()[j];diff[j].canonicalize();if(diff[j]!=0)report.compatible=false;}
        report.differences.push_back(std::move(diff));}
    return report;
}
inline NativeFiniteInterfaceReport finite_report(const State& s,const NativeFiniteInterface& f,Budget& work) {
    require(f.field_identity && f.legs.size()>=2,"finite interface needs a field and legs");
    const auto& common=s.declaration(f.common_source,f.common_type);require(common.kind==composite::DeclarationKind::Finite,"finite interface common carrier");
    const std::set<std::uint64_t> target(common.symbols.begin(),common.symbols.end());NativeFiniteInterfaceReport out;
    std::set<std::pair<std::uint64_t,std::uint64_t>> legs;
    for(const auto& leg:f.legs){work.take(leg.restriction.size());require(legs.emplace(leg.source,leg.value).second,"duplicate finite interface leg");
        const auto a=s.source(leg.source).access(value_request(leg.value));const auto d=a.declaration();
        require(d->kind==composite::DeclarationKind::Finite && leg.restriction.size()==d->symbols.size(),"finite restriction must be total on declared symbols");
        const std::set<std::uint64_t> domain(d->symbols.begin(),d->symbols.end());std::map<std::uint64_t,std::uint64_t> mapping;
        for(const auto& [from,to]:leg.restriction)require(domain.contains(from) && target.contains(to) && mapping.emplace(from,to).second,"invalid finite restriction map");
        const auto value=a.declared_value();const auto symbol=std::get<composite::DeclaredValue::Symbol>(value->data).key;
        out.restrictions.push_back(mapping.at(symbol));
    }
    for(const auto v:out.restrictions)if(v!=out.restrictions.front())out.compatible=false;
    return out;
}
inline std::shared_ptr<const State> build(const std::vector<NativeAtlasSource>& sources,const NativeAtlasManifest& manifest,const NativeAtlasLimits& limits) {
    bound(sources.size(),limits.max_sources,"atlas source budget");require(!sources.empty(),"empty native atlas");
    auto out=std::make_shared<State>();out->limits=limits;out->manifest=manifest;const auto manifest_records=normalize(out->manifest,limits);
    Budget records{limits.max_records-manifest_records},work{limits.max_work};std::set<std::uint64_t> namespaces;
    for(const auto& source:sources){require(out->sources.emplace(source.key(),source).second,"duplicate atlas source key");
        require(namespaces.insert(source.name_space()).second,"one endpoint per source namespace in an atlas publication");
        const auto qs=source.queries();records.take(qs.size());
        bound(qs.size(),limits.observations.max_queries,"atlas source query budget");
        for(const auto& q:qs){records.take(q.subjects.size());bound(q.work_budget,limits.observations.max_work_per_observation,"atlas observation work policy");}
        const auto p=source.endpoint();
        for(std::size_t k=0;k<p->source.grade_count();++k){const auto& g=p->source.grade(core::Grade{static_cast<std::uint16_t>(k)});
            records.take(g.cell_count());records.take(g.incidence_count());
            work.take(machine::checked_size_mul(g.incidence_count(),p->source.signature().aspects().size()));}
        out->indexes.emplace(source.key(),std::make_unique<StableSourceIndex>(p->source));
    }
    // Same immutable declaration identity must retain its full meaning across
    // sources. Renames/reversions need explicit IDs/recodings, not equal tags.
    std::map<composite::DeclarationId,composite::Declaration> definitions;
    for(const auto& [key,source]:out->sources){(void)key;const auto p=source.endpoint();if(!p->declarations)continue;
        for(const auto& d:p->declarations->registry.definitions()){records.take();const auto [it,new_definition]=definitions.emplace(d.id,d);
            require(new_definition || it->second==d,"conflicting same-ID declaration across atlas sources");}}
    for(const auto& e:out->manifest.embeddings)validate_embedding(*out,e,work);
    out->cells.coherent();out->occurrences.coherent();
    for(const auto& p:out->manifest.pushouts)(void)glue(*out,p,work);
    for(const auto& p:out->manifest.products){require(p.left_factor && p.right_factor && p.left_factor!=p.right_factor,"product factor identities");
        (void)out->source(p.left_source);(void)out->source(p.right_source);
        const auto max=static_cast<std::uint32_t>(out->complex(p.left_source).max_grade().value)+out->complex(p.right_source).max_grade().value;
        require(max<=65535u,"product total grade overflow");}
    for(const auto& h:out->manifest.scale_maps){require(h.from!=h.to,"semantic realization requires distinct sources");
        require(out->complex(h.from).semantic_scale().value>out->complex(h.to).semantic_scale().value,"semantic realization must be coarse-to-fine, not nesting-as-grade");
        auto f=chain_map(*out,h,work);chain_check_budget(f,work);require(f.is_chain_map(),"semantic realization does not commute with boundary");
        if(h.retraction){auto r=chain_map(*out,h,work,true);chain_check_budget(r,work);require(r.is_chain_map(),"semantic retraction boundary defect");
            auto roundtrip=compose(f,r,work);require(geometry::same_chain_map(roundtrip,geometry::identity_chain_map(out->complex(h.from))),"semantic retraction is not a left inverse");}
    }
    for(const auto& p:out->manifest.scale_paths){const auto a=path(*out,p.left,work),b=path(*out,p.right,work);require(geometry::same_chain_map(a,b),"incoherent semantic-scale diagram");}
    for(const auto& f:out->manifest.linear_interfaces)(void)linear_report(*out,f,work);
    for(const auto& f:out->manifest.finite_interfaces)(void)finite_report(*out,f,work);
    return out;
}
} // namespace rfc::storage::native_atlas_detail

namespace rfc::storage {
class NativeAtlas;
class PinnedNativeProduct {
public:
    const NativeProductManifest& manifest()const & {return native_atlas_detail::named(state_->manifest.products,key_);}
    const NativeProductManifest& manifest()const &&=delete;
    PinnedNativeCell factor_cell(bool right,StableCellKey key) const {
        const auto& m=manifest();return state_->source(right?m.right_source:m.left_source).cell(key);
    }
    std::vector<NativeProductOccurrence> retained_boundary(NativeProductCell upper) const {
        using namespace native_atlas_detail;require(upper.grade().value>0,"product grade zero boundary");
        const auto& m=manifest();(void)factor_cell(false,upper.left);(void)factor_cell(true,upper.right);
        Budget work{state_->limits.max_work};std::vector<NativeProductOccurrence> out;
        for(const bool right:{false,true}){const auto cell=right?upper.right:upper.left;if(!cell.grade.value)continue;
            const auto factor=factor_cell(right,cell);work.take(factor.occurrence_count());
            for(std::size_t i=0;i<factor.occurrence_count();++i){const auto o=factor.occurrence(i);auto lower=upper;
                if(right)lower.right=o.participant();else lower.left=o.participant();const bool flip=right && upper.left.grade.value%2!=0;
                numeric::Rational q=o.coefficient();if(flip)q=-q;q.canonicalize();
                out.push_back({upper,lower,right?m.right_factor:m.left_factor,right?m.right_source:m.left_source,o.key(),right,flip,
                    o.aspect(state_->complex(right?m.right_source:m.left_source).signature().existence_aspect()),q});}
        }
        return out;
    }
    NativeProductChain boundary(const NativeProductChain& input) const {
        using namespace native_atlas_detail;require(input.grade.value>0,"product chain grade zero boundary");Budget work{state_->limits.max_work};
        NativeProductChain out{core::Grade{static_cast<std::uint16_t>(input.grade.value-1)},{}};
        const auto& m=manifest();const auto& a=state_->complex(m.left_source);const auto& b=state_->complex(m.right_source);
        algebra::RelationalTensorProduct product(a,b);std::vector<algebra::SparseProductEntry> entries;
        for(const auto& [cell,value]:input.values){work.take();require(cell.grade()==input.grade,"product support mixes total grades");rational(value,state_->limits);
            const auto l=state_->cell(m.left_source,cell.left),r=state_->cell(m.right_source,cell.right);
            for(const auto& route:retained_boundary(cell)){work.take();out.values.try_emplace(route.lower,numeric::Rational{0});}
            if(value!=0)entries.push_back({{l,r},value});}
        const auto result=product.boundary_apply_lazy(algebra::SparseProductField(input.grade,std::move(entries)));
        for(const auto& x:result.entries()){work.take();const NativeProductCell cell{stable_cell_key(a,x.cell.left),stable_cell_key(b,x.cell.right)};
            require(out.values.contains(cell),"native product realization escaped retained support");out.values.at(cell)=x.value;}
        return out;
    }
    // Explicit graded swap into a separately declared reversed product. No
    // source/factor identity is inferred from coincidental dimensions or values.
    NativeProductChain braid_to(const PinnedNativeProduct& reversed,const NativeProductChain& input) const {
        using namespace native_atlas_detail;require(state_==reversed.state_,"product braiding must share an exact atlas publication");
        const auto& a=manifest();const auto& b=reversed.manifest();
        require(a.left_source==b.right_source && a.right_source==b.left_source && a.left_factor==b.right_factor && a.right_factor==b.left_factor,"product braiding factor provenance mismatch");
        NativeProductChain out{input.grade,{}};Budget work{state_->limits.max_work};
        for(const auto& [c,q]:input.values){work.take();require(c.grade()==input.grade,"braid chain grade");(void)factor_cell(false,c.left);(void)factor_cell(true,c.right);rational(q,state_->limits);
            const bool flip=(c.left.grade.value%2u)&&(c.right.grade.value%2u);out.values.emplace(NativeProductCell{c.right,c.left},flip?-q:q);}
        return out;
    }
private:
    std::shared_ptr<const native_atlas_detail::State> state_;std::uint64_t key_;
    PinnedNativeProduct(std::shared_ptr<const native_atlas_detail::State> s,std::uint64_t key):state_(std::move(s)),key_(key){}
    friend class NativeAtlas;
};

// An unforgeable, snapshot-bound sufficient-context result for a stored
// exact linear map. No caller-provided boolean can authorize omitted inputs.
class PinnedNativeOperatorContext {
public:
    bool sufficient() const noexcept{return report_.sufficient();}
    const NativeContextReport& report()const & noexcept{return report_;}
    const NativeContextReport& report()const &&=delete;
    NativeAccessionDescriptor descriptor()const{return operation_.descriptor();}
    const std::vector<std::size_t>& inputs()const & noexcept{return inputs_;}
    const std::vector<std::size_t>& inputs()const &&=delete;
    const std::vector<std::size_t>& outputs()const & noexcept{return outputs_;}
    const std::vector<std::size_t>& outputs()const &&=delete;
    numeric::RationalVector evaluate(const numeric::RationalVector& selected_values)const {
        using namespace native_atlas_detail;
        require(sufficient(),"incomplete operator context cannot evaluate without exterior information");
        require(selected_values.size()==inputs_.size(),"context input arity");Budget work{limits_.max_work};
        for(const auto& q:selected_values)rational(q,limits_);
        const auto matrix=temporal_tensor_matrix(*operation_.tensor_declaration());
        numeric::RationalVector out(outputs_.size(),numeric::Rational{0});work.take(inputs_.size());work.take(outputs_.size());
        for(const auto& [ij,q]:matrix.entries()){
            work.take();const auto row=std::lower_bound(outputs_.begin(),outputs_.end(),ij.first);
            if(row==outputs_.end() || *row!=ij.first)continue;
            const auto col=std::lower_bound(inputs_.begin(),inputs_.end(),ij.second);
            require(col!=inputs_.end() && *col==ij.second,"context certificate internal dependency mismatch");
            out[static_cast<std::size_t>(row-outputs_.begin())]+=q*selected_values[static_cast<std::size_t>(col-inputs_.begin())];
        }
        for(auto& q:out)q.canonicalize();
        return out;
    }
private:
    PinnedNativeAccession operation_;
    std::vector<std::size_t> inputs_,outputs_;
    NativeContextReport report_;NativeAtlasLimits limits_;
    PinnedNativeOperatorContext(PinnedNativeAccession op,std::vector<std::size_t> in,std::vector<std::size_t> out,
                               NativeContextReport report,NativeAtlasLimits limits)
        :operation_(std::move(op)),inputs_(std::move(in)),outputs_(std::move(out)),report_(std::move(report)),limits_(limits){}
    friend class NativeAtlas;
};

class NativeAtlas {
public:
    NativeAtlas(const std::vector<NativeAtlasSource>& sources,const NativeAtlasManifest& manifest,NativeAtlasLimits limits={})
        :state_(native_atlas_detail::build(sources,manifest,limits)){}
    const NativeAtlasManifest& manifest()const & noexcept{return state_->manifest;}
    const NativeAtlasManifest& manifest()const &&=delete;
    NativeAtlasLimits limits() const{return state_->limits;}
    NativeAtlasSource source(std::uint64_t key)const{return state_->source(key);}
    std::vector<NativeAtlasSource> sources()const {std::vector<NativeAtlasSource> out;for(const auto& [k,s]:state_->sources){(void)k;out.push_back(s);}return out;}
    PinnedNativeAccession access(std::uint64_t source_key,const NativeAccessionRequest& request) const{return source(source_key).access(request);}
    bool identified(NativeAtlasCell a,NativeAtlasCell b)const {
        (void)state_->cell(a.source,a.cell);(void)state_->cell(b.source,b.cell);return state_->cells.root(a)==state_->cells.root(b);
    }
    bool identified(NativeAtlasOccurrence a,NativeAtlasOccurrence b)const {
        (void)state_->occurrence(a.source,a.occurrence);(void)state_->occurrence(b.source,b.occurrence);return state_->occurrences.root(a)==state_->occurrences.root(b);
    }
    NativeGluedSource glue(std::uint64_t key)const {
        native_atlas_detail::Budget work{state_->limits.max_work};return native_atlas_detail::glue(*state_,native_atlas_detail::named(manifest().pushouts,key),work);
    }
    // Same-field scalar sections glue by equality, NEVER by adding the overlap.
    // Different fields should retain different field_identity declarations.
    std::map<StableCellKey,numeric::Rational> glue_scalar_fields(std::uint64_t pushout,std::uint64_t field_identity,
        core::Grade grade,std::uint64_t left_field,std::uint64_t right_field)const {
        using namespace native_atlas_detail;require(field_identity!=0,"gluing needs a field identity");const auto& p=named(manifest().pushouts,pushout);
        const auto left=named(manifest().embeddings,p.left_embedding).to,right=named(manifest().embeddings,p.right_embedding).to;
        const auto glued=glue(pushout);std::map<StableCellKey,numeric::Rational> result;Budget work{state_->limits.max_work};
        for(const auto& [source_key,field]:{std::pair{left,left_field},std::pair{right,right_field}}){
            const auto view=access(source_key,NativeAccessionRequest::chain(field,grade));
            require(view.support_count()==state_->complex(source_key).grade(grade).cell_count(),"partial field requires explicit restriction/extension");
            view.for_each_cell([&](auto cell){work.take();const auto mapped=glued.cells.at({source_key,{grade,cell}});const auto q=*view.state(cell);
                const auto [it,inserted]=result.emplace(mapped,q);require(inserted || it->second==q,"same-field gluing obstruction");});
        }
        return result;
    }
    PinnedNativeProduct product(std::uint64_t key)const {(void)native_atlas_detail::named(manifest().products,key);return {state_,key};}
    NativeLinearInterfaceReport linear_interface(std::uint64_t key)const {
        native_atlas_detail::Budget work{state_->limits.max_work};return native_atlas_detail::linear_report(*state_,native_atlas_detail::named(manifest().linear_interfaces,key),work);
    }
    NativeFiniteInterfaceReport finite_interface(std::uint64_t key)const {
        native_atlas_detail::Budget work{state_->limits.max_work};return native_atlas_detail::finite_report(*state_,native_atlas_detail::named(manifest().finite_interfaces,key),work);
    }
    numeric::RationalVector realize(std::uint64_t key,core::Grade grade,const numeric::RationalVector& values,bool retract=false)const {
        using namespace native_atlas_detail;Budget work{state_->limits.max_work};const auto& h=named(manifest().scale_maps,key);auto map=chain_map(*state_,h,work,retract);
        bound(values.size(),state_->limits.max_dimension,"hierarchy input dimension");for(const auto& q:values)rational(q,state_->limits);
        return map.push_field(grade,values);
    }
    // Complete row-dependency certificate for ONE stored exact linear operator.
    // It does not certify a Green/Hodge response from local incidence alone.
    PinnedNativeOperatorContext context(std::uint64_t source_key,std::uint64_t op_key,std::vector<std::size_t> inputs,std::vector<std::size_t> outputs)const {
        using namespace native_atlas_detail;Budget work{state_->limits.max_work};work.take(inputs.size());work.take(outputs.size());
        NativeAccessionRequest request;request.kind=NativeAccessionKind::Operator;request.subject=op_key;
        const auto view=access(source_key,request);const auto op=view.tensor_declaration();const auto matrix=temporal_tensor_matrix(*op);
        std::sort(inputs.begin(),inputs.end());std::sort(outputs.begin(),outputs.end());
        require(std::adjacent_find(inputs.begin(),inputs.end())==inputs.end() && std::adjacent_find(outputs.begin(),outputs.end())==outputs.end(),"duplicate context coordinate");
        for(const auto i:inputs)require(i<matrix.cols(),"context input outside carrier");
        for(const auto i:outputs)require(i<matrix.rows(),"context output outside carrier");
        std::set<std::size_t> missing;
        for(const auto& [ij,q]:matrix.entries()){(void)q;work.take();if(std::binary_search(outputs.begin(),outputs.end(),ij.first) && !std::binary_search(inputs.begin(),inputs.end(),ij.second))missing.insert(ij.second);}
        return {view,std::move(inputs),std::move(outputs),{{missing.begin(),missing.end()}},state_->limits};
    }
    // Functional publication: all endpoint replays and every affected atlas law
    // are checked before a new immutable publication escapes. Existing pins keep
    // the old atlas. Complete target manifests allow changes in map definitions.
    NativeAtlas advanced(const std::vector<std::pair<std::uint64_t,TemporalSegment>>& changes,
                         std::optional<NativeAtlasManifest> target=std::nullopt)const {
        using namespace native_atlas_detail;std::set<std::uint64_t> seen;auto sources_next=sources();
        for(const auto& [key,segment]:changes){require(seen.insert(key).second,"duplicate atlas endpoint update");auto old=source(key);
            auto next=replay_temporal_segment(*old.endpoint(),segment,state_->limits.source_declarations);
            auto it=std::find_if(sources_next.begin(),sources_next.end(),[&](const auto& x){return x.key()==key;});
            *it=NativeAtlasSource(key,old.name_space(),next,state_->limits.accession,old.queries(),state_->limits.observations);}
        auto m=target?*target:manifest();require(m.key==manifest().key,"atlas update identity changed");
        if(!target){require(m.revision!=std::numeric_limits<std::uint64_t>::max(),"atlas revision overflow");++m.revision;}
        require(m.revision>manifest().revision,"atlas target revision must advance");return NativeAtlas(sources_next,m,state_->limits);
    }
private:
    std::shared_ptr<const native_atlas_detail::State> state_;
};
} // namespace rfc::storage
