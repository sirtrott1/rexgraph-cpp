#pragma once

#include "rfc/database/interpretation_types.hpp"

namespace rfc::database {
struct InterpretationUpdate {
    std::uint64_t source_namespace{}, binding_namespace{}, version{};
    std::string base_revision, target_revision;
    InterpretationLineage base, target;
    storage::TemporalSegment segment;
};

namespace interpretation_detail {
struct LineageIndex {
    std::map<ExternalOrigin,std::uint64_t> subjects, assertions;
    std::map<OccurrenceOrigin,std::uint64_t> occurrences;
    std::map<std::uint64_t,ExternalOrigin> subject_origins, assertion_origins;
    std::map<std::uint64_t,OccurrenceOrigin> occurrence_origins;
    std::uint64_t subject_next{1}, assertion_next{1}, occurrence_next{1};
};
inline LineageIndex validate_lineage(const InterpretationLineage& ledger,const CompiledInterpretation& plan,
                                     const InterpretationLimits& limits) {
    LineageIndex out;std::size_t records=0,bytes=0;
    std::set<std::uint64_t> subject_kinds;for(const auto& s:plan.schema.subjects)subject_kinds.insert(s.key);
    const auto ingest=[&](const auto& entries,auto& index,auto& reverse,auto& next,bool subjects) {
        add(records,entries.size(),limits.max_records,"aggregate lineage record budget");
        std::optional<ExternalOrigin> previous;
        for(const auto& entry:entries) {
            origin(entry.origin,bytes,limits);
            require(!previous || *previous<entry.origin,"noncanonical/duplicate lineage origin");previous=entry.origin;
            require(entry.native_key && entry.native_key!=std::numeric_limits<std::uint64_t>::max(),"invalid native lineage key");
            require(index.emplace(entry.origin,entry.native_key).second && reverse.emplace(entry.native_key,entry.origin).second,"duplicate native lineage key");
            next=std::max(next,entry.native_key+1);
            if(subjects)require(subject_kinds.contains(entry.origin.kind),"unknown subject lineage kind");
            else (void)plan.assertion(entry.origin.kind);
        }
    };
    ingest(ledger.subjects,out.subjects,out.subject_origins,out.subject_next,true);
    ingest(ledger.assertions,out.assertions,out.assertion_origins,out.assertion_next,false);
    add(records,ledger.occurrences.size(),limits.max_records,"aggregate lineage record budget");
    std::optional<OccurrenceOrigin> previous;
    for(const auto& entry:ledger.occurrences) {
        origin(entry.origin.assertion,bytes,limits);token(entry.origin.lineage,bytes,limits);
        require(!previous || *previous<entry.origin,"noncanonical/duplicate occurrence lineage");previous=entry.origin;
        require(out.assertions.contains(entry.origin.assertion),"occurrence lineage lacks owning assertion");
        require(entry.native_key && entry.native_key!=std::numeric_limits<std::uint64_t>::max(),"invalid occurrence lineage key");
        require(out.occurrences.emplace(entry.origin,entry.native_key).second && out.occurrence_origins.emplace(entry.native_key,entry.origin).second,"duplicate occurrence lineage key");
        out.occurrence_next=std::max(out.occurrence_next,entry.native_key+1);
    }
    return out;
}
inline LogicalSnapshot normalize_snapshot(const LogicalSnapshot& input,const CompiledInterpretation& plan,
                                           const InterpretationLimits& limits) {
    std::size_t records=input.subjects.size(),bytes=0,nodes=0,materialized=0,copy_nodes=0,copy_bytes=0;
    add(records,input.assertions.size(),limits.max_records,"snapshot record budget");token(input.revision,bytes,limits);
    std::set<ExternalOrigin> subjects,assertions;std::set<std::uint64_t> kinds;
    for(const auto& s:plan.schema.subjects)kinds.insert(s.key);
    for(const auto& s:input.subjects) {
        origin(s,bytes,limits);require(kinds.contains(s.kind),"undeclared subject kind");
        require(subjects.insert(s).second,"duplicate subject origin");
    }
    for(const auto& a:input.assertions) {
        origin(a.origin,bytes,limits);require(assertions.insert(a.origin).second,"duplicate assertion lineage; equal records need distinct producer identities");
        const auto& type=plan.assertion(a.origin.kind);
        account_value_copy(a.payload,limits,copy_nodes,copy_bytes);
        const composite::CompiledLayout layout(plan.registry,type.payload,composite::BooleanLayoutProfile::Lsb0OctetsV1,limits.declarations);
        const auto use=layout.allocation(a.payload);
        add(nodes,use.value_nodes,limits.declarations.max_value_nodes,"aggregate input value work");
        add(materialized,use.materialized_bytes,limits.declarations.max_materialized_bytes,"aggregate input value allocation");
        const auto generated=[&](std::size_t count,std::size_t extra_bytes=0) {
            add(nodes,count,limits.declarations.max_value_nodes,"generated native wrapper/order work");
            add(materialized,machine::checked_size_add(machine::checked_size_mul(count,sizeof(DeclaredValue)),extra_bytes),
                limits.declarations.max_materialized_bytes,"generated native wrapper/order allocation");
        };
        generated(3); // Wrapper, state product, order product (payload counted above).
        require(a.roles.size()==type.roles.size(),"every declared role needs an explicit missing/null/present state");
        add(records,a.roles.size(),limits.max_records,"snapshot role budget");
        std::map<std::uint64_t,const ExternalRoleValue*> roles;std::set<std::string> lineages;
        for(const auto& r:a.roles)require(roles.emplace(r.role,&r).second,"duplicate role value");
        for(const auto& role:type.roles) {
            generated(2); // State and repeated native occurrence-key list.
            const auto it=roles.find(role.key);require(it!=roles.end(),"missing declared role value");const auto& value=*it->second;
            switch(value.state) {
                case ReferenceState::Missing:require(role.allow_missing && value.occurrences.empty(),"missing role not admitted or carries occurrences");break;
                case ReferenceState::Null:require(role.allow_null && value.occurrences.empty(),"null role not admitted or carries occurrences");break;
                case ReferenceState::Present:require(value.occurrences.size()>=role.minimum && value.occurrences.size()<=role.maximum,"role population outside declared bounds");break;
                default:throw std::invalid_argument("unknown reference state");
            }
            add(records,value.occurrences.size(),limits.max_records,"snapshot occurrence budget");
            generated(value.occurrences.size(),machine::checked_size_mul(value.occurrences.size(),sizeof(std::uint64_t)));
            for(const auto& occurrence:value.occurrences) {
                token(occurrence.lineage,bytes,limits);origin(occurrence.participant,bytes,limits);
                require(lineages.insert(occurrence.lineage).second,"duplicate occurrence lineage within assertion");
                require(occurrence.participant.kind==role.participant_kind,"participant kind violates role declaration");
                require(subjects.contains(occurrence.participant),"dangling external reference");
            }
        }
    }
    LogicalSnapshot out=input;
    std::sort(out.subjects.begin(),out.subjects.end());
    std::sort(out.assertions.begin(),out.assertions.end(),[](const auto& a,const auto& b){return a.origin<b.origin;});
    for(auto& a:out.assertions)std::sort(a.roles.begin(),a.roles.end(),[](const auto& x,const auto& y){return x.role<y.role;});
    return out;
}
inline InterpretationLineage extend_lineage(const InterpretationLineage& old,const LogicalSnapshot& normalized,
                                             const CompiledInterpretation& plan,const InterpretationLimits& limits) {
    auto index=validate_lineage(old,plan,limits);
    const auto insert=[](auto& map,const auto& key,std::uint64_t& next) {
        if(!map.contains(key)) {
            require(next!=std::numeric_limits<std::uint64_t>::max(),"lineage key exhaustion");map.emplace(key,next++);
        }
    };
    for(const auto& s:normalized.subjects)insert(index.subjects,s,index.subject_next);
    for(const auto& a:normalized.assertions) {
        insert(index.assertions,a.origin,index.assertion_next);
        // New occurrence keys are allocated by lineage, never by role/scan order.
        std::set<OccurrenceOrigin> occurrences;
        for(const auto& r:a.roles)for(const auto& o:r.occurrences)occurrences.insert({a.origin,o.lineage});
        for(const auto& o:occurrences)insert(index.occurrences,o,index.occurrence_next);
    }
    InterpretationLineage out;
    for(const auto& [o,k]:index.subjects)out.subjects.push_back({o,k});
    for(const auto& [o,k]:index.assertions)out.assertions.push_back({o,k});
    for(const auto& [o,k]:index.occurrences)out.occurrences.push_back({o,k});
    (void)validate_lineage(out,plan,limits);return out;
}
inline storage::TemporalEndpoint build_endpoint(const CompiledInterpretation& plan,const LogicalSnapshot& input,
                                                const InterpretationLineage& ledger,std::uint64_t sequence,
                                                const InterpretationLimits& limits) {
    const auto index=validate_lineage(ledger,plan,limits);
    core::RelationalComplexBuilder builder(plan.signature);builder.declare_grade(core::Grade{1});
    std::vector<std::uint64_t> subjects;for(const auto& s:input.subjects)subjects.push_back(index.subjects.at(s));
    std::sort(subjects.begin(),subjects.end());for(auto s:subjects)builder.declare_witness_identity(s);
    std::vector<const ExternalAssertion*> assertions;for(const auto& a:input.assertions)assertions.push_back(&a);
    std::sort(assertions.begin(),assertions.end(),[&](const auto* a,const auto* b){return index.assertions.at(a->origin)<index.assertions.at(b->origin);});
    storage::TemporalDeclarationState state;state.namespace_key=plan.schema.binding_namespace;state.registry=plan.registry;state.resource_limits=limits.declarations;
    for(const auto* a:assertions) {
        const auto key=index.assertions.at(a->origin);
        std::vector<core::BoundaryOccurrenceInput> incidences;std::vector<DeclaredValue> states,orders;
        for(const auto& role:a->roles) {
            states.push_back(DeclaredValue::symbol(static_cast<std::uint64_t>(role.state)));
            std::vector<DeclaredValue> order;
            for(const auto& o:role.occurrences) {
                const auto occurrence_key=index.occurrences.at({a->origin,o.lineage});
                order.push_back(DeclaredValue::integer(numeric::Integer(std::to_string(occurrence_key))));
                incidences.push_back({index.subjects.at(o.participant),o.active,false,numeric::Rational{0},
                    {{plan.aspects.at({a->origin.kind,role.role}),true}},core::OccurrenceKey{occurrence_key}});
            }
            orders.push_back(DeclaredValue::repeated(std::move(order)));
        }
        std::sort(incidences.begin(),incidences.end(),[](const auto& left,const auto& right){return *left.occurrence_key<*right.occurrence_key;});
        builder.add_grade_one(core::RelationKey{key},plan.types.at(a->origin.kind),incidences);
        storage::TemporalDeclaredValue v;v.key=key;v.type=plan.wrappers.at(a->origin.kind);
        v.cell=storage::StableCellKey{core::Grade{1},key};if(!a->roles.empty())v.branch=a->origin.kind;
        v.value=DeclaredValue::product({a->payload,DeclaredValue::product(std::move(states)),DeclaredValue::product(std::move(orders))});state.values.push_back(std::move(v));
    }
    // Persist generic role declarations. Null/missing cardinality is an adapter
    // invariant; native roles admit zero, and never acquire normalized head rules.
    for(const auto& a:plan.schema.assertions) {
        core::BranchDeclaration branch;branch.id=plan.wrappers.at(a.key);branch.payload=plan.wrappers.at(a.key);
        branch.population=core::BranchPopulation::Retained;branch.semantics=core::BranchSemantics::Generic;branch.partition=true;
        for(const auto& r:a.roles)branch.roles.push_back({r.key,role_aspect(a.key,r.key),0,r.maximum});
        // Empty role families do not need a branching contract.
        if(!a.roles.empty()) {
            state.branches.push_back({a.key,source_type(a.key),std::move(branch)});
            storage::TemporalSourceAspectLayout layout;layout.key=a.key;layout.type=plan.layouts.at(a.key);
            for(const auto& r:a.roles)layout.factors.push_back({r.key,role_aspect(a.key,r.key)});
            state.source_layouts.push_back(std::move(layout));
        }
    }
    storage::TemporalEndpoint out(builder.freeze(),sequence);out.declarations=std::move(state);
    storage::temporal_declaration_detail::validate_definitions(*out.declarations,limits.declarations);storage::validate_temporal_endpoint(out);return out;
}
} // namespace interpretation_detail

// Immutable native publication. Export lineage is separate from query indexes;
// payload/access/traversal methods do not resolve external tokens.
class InterpretedSnapshot {
public:
    static InterpretedSnapshot interpret(const InterpretationSchema& schema,const LogicalSnapshot& input,
                                        const InterpretationLimits& limits={}) {
        interpretation_detail::CompiledInterpretation plan(schema,limits);
        const auto normalized=interpretation_detail::normalize_snapshot(input,plan,limits);
        auto ledger=interpretation_detail::extend_lineage({},normalized,plan,limits);
        auto endpoint=interpretation_detail::build_endpoint(plan,normalized,ledger,0,limits);
        return InterpretedSnapshot(std::move(plan),std::move(ledger),std::move(endpoint),normalized.revision,limits);
    }
    InterpretedSnapshot replace(const LogicalSnapshot& input) const {
        using namespace interpretation_detail;
        require(sequence()!=std::numeric_limits<std::uint64_t>::max(),"interpretation sequence exhausted");
        require(input.revision!=revision(),"replacement must declare a new producer snapshot revision");
        const auto normalized=normalize_snapshot(input,data_->plan,data_->limits);
        auto ledger=extend_lineage(data_->ledger,normalized,data_->plan,data_->limits);
        auto endpoint=build_endpoint(data_->plan,normalized,ledger,sequence()+1,data_->limits);
        return InterpretedSnapshot(data_->plan,std::move(ledger),std::move(endpoint),normalized.revision,data_->limits);
    }
    InterpretedSnapshot replace(const InterpretationSchema& supplied,const LogicalSnapshot& input) const {
        const interpretation_detail::CompiledInterpretation p(supplied,data_->limits);
        const auto& a=schema();const auto& b=p.schema;
        interpretation_detail::require(a.source_namespace==b.source_namespace && a.binding_namespace==b.binding_namespace && a.version==b.version &&
            a.subjects==b.subjects && a.assertions==b.assertions && a.values.definitions()==b.values.definitions(),
            "external declaration/presentation changed; explicit migration required");
        return replace(input);
    }
    static InterpretedSnapshot restore(const InterpretationSchema& schema,const InterpretationLineage& ledger,
                                      const storage::TemporalEndpoint& endpoint,const std::string& revision,const InterpretationLimits& limits={}) {
        using namespace interpretation_detail;
        CompiledInterpretation plan(schema,limits);(void)validate_lineage(ledger,plan,limits);
        if(endpoint.declarations)storage::temporal_declaration_detail::validate_definitions(*endpoint.declarations,limits.declarations);
        storage::validate_temporal_endpoint(endpoint);
        auto out=InterpretedSnapshot(std::move(plan),ledger,endpoint,revision,limits);
        // Reconstruct the interpretation from native contents, then compare the
        // complete canonical endpoint. Extra channels, drifted roles and partial
        // manifests cannot be silently discarded by an importer/exporter.
        const auto logical=normalize_snapshot(out.export_snapshot(),out.data_->plan,limits);
        const auto reconstructed=build_endpoint(out.data_->plan,logical,ledger,endpoint.sequence,limits);
        require(storage::encode_rfcx(reconstructed.source).bytes==storage::encode_rfcx(endpoint.source).bytes &&
                storage::encode_temporal_endpoint(reconstructed)==storage::encode_temporal_endpoint(endpoint),
                "native endpoint is outside this interpretation's exact image");
        return out;
    }
    const std::string& revision() const noexcept { return data_->revision; }
    const InterpretationSchema& schema() const noexcept { return data_->plan.schema; }
    bool same_schema(const InterpretedSnapshot& b) const {
        const auto& a=schema();const auto& c=b.schema();return a.source_namespace==c.source_namespace && a.binding_namespace==c.binding_namespace &&
            a.version==c.version && a.subjects==c.subjects && a.assertions==c.assertions && a.values.definitions()==c.values.definitions();
    }
    const InterpretationLineage& lineage() const noexcept { return data_->ledger; }
    const InterpretationLimits& limits() const noexcept { return data_->limits; }
    std::uint64_t sequence() const { return data_->source.sequence(); }
    storage::NativeAtlasSource source(std::uint64_t atlas_key=1) const {
        interpretation_detail::require(atlas_key==1,"use explicit atlas source construction to rename an interpretation source");return data_->source;
    }
    std::shared_ptr<const storage::TemporalEndpoint> endpoint() const { return data_->source.endpoint(); }
    const std::vector<std::uint64_t>& assertions(std::uint64_t kind) const {
        (void)data_->plan.assertion(kind);return data_->assertions.at(kind);
    }
    storage::PinnedNativeAccession payload(std::uint64_t assertion,std::vector<std::uint64_t> factors={}) const {
        storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::DeclaredValue;request.subject=assertion;
        request.factors.push_back(1);request.factors.insert(request.factors.end(),factors.begin(),factors.end());return data_->source.access(request);
    }
    ReferenceState reference_state(std::uint64_t assertion,std::uint64_t role) const {
        storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::DeclaredValue;request.subject=assertion;request.factors={2,role};
        const auto v=interpretation_detail::symbol(*data_->source.access(request).declared_value());
        interpretation_detail::require(v<=2,"invalid native reference state");return static_cast<ReferenceState>(v);
    }
    std::vector<storage::PinnedNativeOccurrence> participants(std::uint64_t assertion,std::uint64_t role,bool active_only=false) const {
        const auto cell=data_->source.cell({core::Grade{1},assertion});
        const auto kind=data_->type_kinds.at(*cell.type());
        const auto aspect=data_->plan.aspects.at({kind,role});
        using interpretation_detail::require;
        std::vector<storage::PinnedNativeOccurrence> out;
        interpretation_detail::bound(cell.occurrence_count(),data_->limits.max_work,"native participant traversal budget");
        std::map<std::uint64_t,storage::PinnedNativeOccurrence> retained;
        for(std::size_t i=0;i<cell.occurrence_count();++i) {
            auto o=cell.occurrence(i);
            if(o.aspect(aspect))require(retained.emplace(o.key().key.value,std::move(o)).second,"duplicate retained occurrence identity");
        }
        storage::NativeAccessionRequest request;request.kind=storage::NativeAccessionKind::DeclaredValue;
        request.subject=assertion;request.factors={3,role};
        const auto pin=data_->source.access(request);
        const auto* order=std::get_if<composite::DeclaredValue::Repeated>(&pin.declared_value()->data);
        require(order && order->elements.size()==retained.size(),"role order must cover its full retained occurrence fiber");
        for(const auto& value:order->elements) {
            const auto* n=std::get_if<numeric::Integer>(&value.data);
            require(n && *n>0 && composite::declaration_detail::integer_bits(*n)<=64,"invalid ordered occurrence identity");
            std::uint64_t key=0;std::size_t words=0;
            mpz_export(&key,&words,-1,sizeof(key),0,0,n->get_mpz_t());
            const auto found=retained.find(key);
            require(found!=retained.end(),"role order contains a duplicate, stale, foreign-owner or wrong-role occurrence");
            if(!active_only || found->second.aspect("exists"))out.push_back(found->second);
            retained.erase(found);
        }
        require(retained.empty(),"role order omitted a retained occurrence");
        return out;
    }
    // Explicit boundary/provenance operation, not the native query path.
    LogicalSnapshot export_snapshot() const {
        using namespace interpretation_detail;
        const auto index=validate_lineage(data_->ledger,data_->plan,data_->limits);LogicalSnapshot out;out.revision=revision();
        const auto& source=endpoint()->source;
        const auto& vertices=source.grade(core::Grade{0});
        for(std::size_t i=0;i<vertices.cell_count();++i)out.subjects.push_back(index.subject_origins.at(storage::stable_cell_key(source,{core::Grade{0},static_cast<std::uint32_t>(i)}).key));
        for(const auto& type:data_->plan.schema.assertions)for(const auto key:assertions(type.key)) {
            ExternalAssertion a;a.origin=index.assertion_origins.at(key);a.payload=*payload(key).declared_value();
            require(a.origin.kind==type.key,"assertion lineage/type mismatch");
            for(const auto& role:type.roles) {
                ExternalRoleValue value;value.role=role.key;value.state=reference_state(key,role.key);
                for(const auto& occurrence:participants(key,role.key)) {
                    const auto& original=index.occurrence_origins.at(occurrence.key().key.value);
                    require(original.assertion==a.origin,"occurrence origin moved to another assertion");
                    value.occurrences.push_back({original.lineage,index.subject_origins.at(occurrence.participant().key),occurrence.aspect("exists")});
                }
                a.roles.push_back(std::move(value));
            }
            out.assertions.push_back(std::move(a));
        }
        std::sort(out.subjects.begin(),out.subjects.end());
        std::sort(out.assertions.begin(),out.assertions.end(),[](const auto& a,const auto& b){return a.origin<b.origin;});return out;
    }
    InterpretationUpdate difference(const InterpretedSnapshot& target) const {
        using namespace interpretation_detail;
        require(same_schema(target),"F5 does not infer external schema migration");
        require(lineage_extends(data_->ledger,target.data_->ledger),"lineage deletion/reassignment in target");
        return {schema().source_namespace,schema().binding_namespace,schema().version,revision(),target.revision(),data_->ledger,target.data_->ledger,
                storage::make_temporal_segment(*endpoint(),*target.endpoint())};
    }
    InterpretedSnapshot apply(const InterpretationUpdate& update) const {
        using namespace interpretation_detail;
        require(update.source_namespace==schema().source_namespace && update.binding_namespace==schema().binding_namespace && update.version==schema().version,
                "foreign interpretation update context");
        require(update.base_revision==revision() && update.target_revision!=revision(),"stale/reused external snapshot revision");
        require(update.base==data_->ledger && lineage_extends(update.base,update.target),"stale or reassigned external lineage");
        (void)validate_lineage(update.target,data_->plan,data_->limits);
        auto target=storage::replay_temporal_segment(*endpoint(),update.segment);
        return restore(schema(),update.target,target,update.target_revision,data_->limits);
    }
private:
    struct Data {
        interpretation_detail::CompiledInterpretation plan;
        InterpretationLineage ledger;
        InterpretationLimits limits;
        std::string revision;
        storage::NativeAtlasSource source;
        std::map<std::uint64_t,std::vector<std::uint64_t>> assertions;
        std::map<std::uint32_t,std::uint64_t> type_kinds;
        Data(interpretation_detail::CompiledInterpretation p,InterpretationLineage l,const storage::TemporalEndpoint& endpoint,std::string producer_revision,InterpretationLimits bounds)
            :plan(std::move(p)),ledger(std::move(l)),limits(bounds),revision(std::move(producer_revision)),source(1,plan.schema.source_namespace,endpoint,bounds.atlas.accession) {
            for(const auto& type:plan.schema.assertions){assertions.emplace(type.key,std::vector<std::uint64_t>{});type_kinds.emplace(plan.types.at(type.key),type.key);}
            const auto& arena=endpoint.source.grade(core::Grade{1});
            for(std::size_t i=0;i<arena.cell_count();++i) {
                const auto kind=type_kinds.at(arena.relation_types.at(i));
                assertions.at(kind).push_back(storage::stable_cell_key(endpoint.source,{core::Grade{1},static_cast<std::uint32_t>(i)}).key);
            }
        }
    };
    std::shared_ptr<const Data> data_;
    InterpretedSnapshot(interpretation_detail::CompiledInterpretation p,InterpretationLineage l,storage::TemporalEndpoint endpoint,std::string revision,InterpretationLimits limits)
        :data_(std::make_shared<const Data>(std::move(p),std::move(l),endpoint,std::move(revision),limits)) {}
    static bool lineage_extends(const InterpretationLineage& a,const InterpretationLineage& b) {
        const auto includes=[](const auto& old,const auto& next) {
            for(const auto& x:old) {
                const auto it=std::lower_bound(next.begin(),next.end(),x.origin,[](const auto& e,const auto& o){return e.origin<o;});
                if(it==next.end() || !(*it==x))return false;
            }
            return true;
        };
        return includes(a.subjects,b.subjects) && includes(a.assertions,b.assertions) && includes(a.occurrences,b.occurrences);
    }
};
} // namespace rfc::database
