#pragma once

#include "rfc/storage/native_accession_types.hpp"
#include "rfc/storage/relational_code_observations.hpp"

namespace rfc::storage {
namespace native_accession_detail {
template<class T> using KeyIndex=std::unordered_map<std::uint64_t,const T*>;
template<class T> KeyIndex<T> keyed(const std::vector<T>& xs) {
    KeyIndex<T> out; out.reserve(xs.size());
    for(const auto& x:xs) require(out.emplace(x.key,&x).second,"duplicate accession subject");
    return out;
}
template<class T> const T* subject(const KeyIndex<T>& index,std::uint64_t key) {
    const auto it=index.find(key);
    if(it==index.end()) throw NativeSubjectMissing("accession subject absent");
    return it->second;
}
struct CellHash {
    std::size_t operator()(StableCellKey key) const noexcept {
        return std::hash<std::uint64_t>{}(key.key) ^ (static_cast<std::size_t>(key.grade.value)*0x9e3779b9u);
    }
};
struct OccurrenceHash {
    std::size_t operator()(StableOccurrenceKey key) const noexcept {return CellHash{}({key.upper_grade,key.key.value});}
};
template<class T> using CellContents=std::unordered_map<StableCellKey,std::vector<const T*>,CellHash>;
template<class T> using OccurrenceContents=std::unordered_map<StableOccurrenceKey,std::vector<const T*>,OccurrenceHash>;
struct Snapshot {
    std::shared_ptr<const ExecutionAuthority> authority;
    std::shared_ptr<const TemporalEndpoint> endpoint;
    std::shared_ptr<const Binding> binding;
    std::unique_ptr<StableSourceIndex> source_index;
    KeyIndex<TemporalTensor> tensors;
    KeyIndex<TemporalCarrier> carriers;
    KeyIndex<TemporalDeclaredValue> values;
    KeyIndex<TemporalSourceAspectLayout> layouts;
    KeyIndex<TemporalSourceLayoutProgram> programs;
    KeyIndex<RelationalCodeSection> sections;
    KeyIndex<RelationalCodeType> code_types;
    std::unordered_map<std::uint64_t,const composite::Declaration*> declared_code_types;
    KeyIndex<RelationalCodeOperation> operations;
    std::unordered_map<std::uint64_t,const LocalBlockDefinition*> blocks;
    std::unordered_map<std::string,composite::AspectId> aspects;
    std::unordered_map<std::uint64_t,std::unordered_map<std::uint64_t,composite::AspectId>> factors;
    std::map<RelationalCodeIdentity,std::shared_ptr<const RelationalCodeObservation>> observations;
    CellContents<TemporalDeclaredValue> cell_values;
    OccurrenceContents<TemporalDeclaredValue> occurrence_values;
    CellContents<TemporalTensor> cell_tensors;
    OccurrenceContents<TemporalTensor> occurrence_tensors;
    std::unordered_map<StableCellKey,std::vector<std::uint64_t>,CellHash> memberships;
    std::unordered_map<std::uint16_t,std::vector<const TemporalTensor*>> grade_fields;
    NativeAccessionStats stats;
    NativeAccessionLimits limits;

    Snapshot(const RelationalCodeObservationStore& store,std::uint64_t name_space,NativeAccessionLimits policy,
             std::shared_ptr<const ExecutionAuthority> owner)
        : authority(std::move(owner)),endpoint(store.pin_snapshot()),limits(policy) {
        require(name_space!=0,"accession source namespace required");
        const auto add=[&](std::size_t n) {
            stats.indexed_records=machine::checked_size_add(stats.indexed_records,n);
            budget(stats.indexed_records,limits.max_index_entries,"accession index-entry budget");
        };
        const auto& source=endpoint->source;
        for(std::size_t k=0;k<source.grade_count();++k) {
            const auto& g=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
            add(g.cell_count()); add(g.incidence_count());
            stats.source_cells=machine::checked_size_add(stats.source_cells,g.cell_count());
            stats.source_occurrences=machine::checked_size_add(stats.source_occurrences,g.incidence_count());
        }
        add(source.signature().aspects().size()); add(endpoint->tensors.size()); add(endpoint->carriers.size()); add(endpoint->local_blocks.size());
        if(endpoint->declarations) {
            const auto& d=*endpoint->declarations;
            add(d.values.size());add(d.programs.size());add(d.source_layouts.size());
            for(const auto& l:d.source_layouts) add(l.factors.size());
        }
        if(endpoint->codes) { add(endpoint->codes->sections.size()); add(endpoint->codes->operations.size()); add(endpoint->codes->types.size()); }
        if(endpoint->declarations) {
            add(endpoint->declarations->code_types.size());
            for(const auto& v:endpoint->declarations->values) {if(v.cell)add(1);if(v.occurrence)add(1);}
        }
        for(const auto& t:endpoint->tensors) {
            if(t.owner.kind==TemporalOwnerKind::Cell || t.owner.kind==TemporalOwnerKind::Occurrence)add(1);
            if(t.role==TemporalTensorRole::Field && t.output)add(1); // conservative upper bound for grade-field reverse entries
        }
        for(const auto& b:endpoint->local_blocks)add(b.cells.size());
        add(store.size());
        // Count before allocating the indexes; the directory does not rescan
        // these collections on a known-handle lookup.
        source_index=std::make_unique<StableSourceIndex>(source);
        tensors=keyed(endpoint->tensors); carriers=keyed(endpoint->carriers);
        for(const auto& b:endpoint->local_blocks) blocks.emplace(b.block_key,&b);
        for(std::size_t i=0;i<source.signature().aspects().size();++i) {
            const auto id=static_cast<composite::AspectId>(i);
            aspects.emplace(source.signature().aspects().at(id).name,id);
        }
        if(endpoint->declarations) {
            const auto& d=*endpoint->declarations;
            values=keyed(d.values); layouts=keyed(d.source_layouts); programs=keyed(d.programs);
            for(const auto& c:d.code_types){declared_code_types.emplace(c.key,&d.registry.at(c.type));}
            for(const auto& l:d.source_layouts) {
                auto& map=factors[l.key]; map.reserve(l.factors.size());
                for(const auto& f:l.factors) map.emplace(f.factor,aspects.at(f.source_aspect));
            }
        }
        if(endpoint->codes) { sections=keyed(endpoint->codes->sections); operations=keyed(endpoint->codes->operations);code_types=keyed(endpoint->codes->types); }
        for(const auto& [key,v]:values) {
            (void)key;
            if(v->cell) {cell_values[*v->cell].push_back(v);}
            if(v->occurrence) {occurrence_values[*v->occurrence].push_back(v);}
        }
        for(const auto& t:endpoint->tensors) {
            if(t.owner.kind==TemporalOwnerKind::Cell) {cell_tensors[t.owner.cell].push_back(&t);}
            if(t.owner.kind==TemporalOwnerKind::Occurrence) {occurrence_tensors[t.owner.occurrence].push_back(&t);}
            if(t.role==TemporalTensorRole::Field && t.output) {
                const auto* c=subject(carriers,*t.output);
                if(c->kind==TemporalCarrierKind::SourceGrade) {grade_fields[c->grade.value].push_back(&t);}
            }
        }
        for(const auto& b:endpoint->local_blocks) {
            const std::set<StableCellKey> keys(b.cells.begin(),b.cells.end());
            for(auto key:keys) {memberships[key].push_back(b.block_key);}
        }
        // Content enumeration has deterministic stable-key order, independent
        // of unordered-map bucket order or endpoint vector insertion order.
        const auto sort_values=[](auto& index) {
            for(auto& [key,xs]:index) {(void)key;std::sort(xs.begin(),xs.end(),[](auto a,auto b){return a->key<b->key;});}
        };
        sort_values(cell_values);sort_values(occurrence_values);sort_values(cell_tensors);sort_values(occurrence_tensors);sort_values(grade_fields);
        for(auto& [key,blocks_for_cell]:memberships) {(void)key;std::sort(blocks_for_cell.begin(),blocks_for_cell.end());}
        for(auto id:store.identities()) observations.emplace(id,store.pin_observation(id));
        auto context=std::make_shared<Binding>();context->name_space=name_space;context->sequence=endpoint->sequence;
        context->endpoint=endpoint;
        {
            const auto image=encode_rfcx(source);
            context->source_bytes=image.bytes.size();context->source_fingerprint=rfcx_detail::fnv1a64(image.bytes);
        }
        budget(context->source_bytes,limits.max_binding_bytes,"accession source witness budget");
        {
            const auto bytes=encode_temporal_endpoint(*endpoint);
            context->endpoint_bytes=bytes.size();context->endpoint_fingerprint=rfcx_detail::fnv1a64(bytes);
        }
        auto total=machine::checked_size_add(context->source_bytes,context->endpoint_bytes);
        budget(total,limits.max_binding_bytes,"accession endpoint witness budget");
        rfcx_detail::Writer w; const auto qs=store.queries(); w.u64(qs.size());
        for(const auto& q:qs) {
            w.u64(q.key);w.u8(static_cast<std::uint8_t>(q.kind));w.u64(q.operation);
            w.u8(q.field?1:0);if(q.field)w.u64(*q.field);w.u64(q.work_budget);w.u64(q.subjects.size());
            for(auto key:q.subjects)w.u64(key);
        }
        context->queries=w.bytes();total=machine::checked_size_add(total,context->queries.size());
        budget(total,limits.max_binding_bytes,"accession query witness budget");
        stats.binding_bytes=total;binding=std::move(context);
    }
    core::CellId cell(StableCellKey key) const {
        const auto c=source_index->cell(key);
        if(!c) throw NativeSubjectMissing("accession retained cell absent");
        return *c;
    }
    std::size_t occurrence(StableOccurrenceKey key) const {
        const auto p=source_index->occurrence(key);
        if(!p) throw NativeSubjectMissing("accession retained occurrence absent");
        return *p;
    }
};
template<class T,class Map,class Key> std::vector<std::shared_ptr<const T>> pin_contents(
    const std::shared_ptr<const Snapshot>& snapshot,const Map& index,const Key& key) {
    std::vector<std::shared_ptr<const T>> out;
    const auto it=index.find(key);if(it==index.end())return out;
    budget(it->second.size(),snapshot->limits.max_action_work,"accession content enumeration budget");
    out.reserve(it->second.size());for(auto* value:it->second)out.emplace_back(snapshot,value);return out;
}
struct Entry {
    NativeAccessionRequest request;
    Bytes bytes;
    bool active{true};
    const TemporalTensor* tensor{};
    const TemporalCarrier* carrier{};
    const LocalBlockDefinition* block{};
    const TemporalDeclaredValue* value_owner{};
    const composite::DeclaredValue* value{};
    const composite::Declaration* declaration{};
    const TemporalSourceAspectLayout* layout{};
    const RelationalCodeSection* section{};
    const RelationalCodeType* code_type{};
    const RelationalCodeType* input_code_type{};
    const RelationalCodeOperation* operation{};
    std::shared_ptr<const RelationalCodeObservation> observation;
    std::size_t occurrence_slot{};
};
inline std::shared_ptr<const Entry> bind(const Snapshot& s,NativeAccessionRequest r,Bytes bytes) {
    auto e=std::make_shared<Entry>();e->request=std::move(r);e->bytes=std::move(bytes);
    const auto& q=e->request;
    try {
        if(chain_kind(q.kind)) {
            if(q.grade.value>=s.endpoint->source.grade_count()) throw NativeSubjectMissing("accession grade absent");
            if(!q.inline_value) {
                e->tensor=subject(s.tensors,q.subject);
                require(e->tensor->role==TemporalTensorRole::Field && e->tensor->output,"chain/cochain requires field carrier");
                e->carrier=subject(s.carriers,*e->tensor->output);
                require(e->carrier->kind==TemporalCarrierKind::SourceGrade && e->carrier->grade==q.grade,
                        "chain/cochain requires the declared source grade carrier");
            }
            for(auto key:q.cells) {
                const auto c=s.cell({q.grade,key});
                require(!e->tensor || e->tensor->tensor.support().contains(c.index),"selected cell is structurally absent from field");
            }
            for(const auto& c:q.coefficients) (void)s.cell({q.grade,c.cell});
        } else if(q.kind==NativeAccessionKind::Field || q.kind==NativeAccessionKind::Tensor || q.kind==NativeAccessionKind::Operator) {
            e->tensor=subject(s.tensors,q.subject);
            if(q.kind==NativeAccessionKind::Field) require(e->tensor->role==TemporalTensorRole::Field,"accession is not a field");
            if(q.kind==NativeAccessionKind::Operator) {
                require(e->tensor->input && e->tensor->output,"operator requires input/output carriers");
                require(e->tensor->role!=TemporalTensorRole::Metric && e->tensor->role!=TemporalTensorRole::CrossForm,
                        "bilinear forms are not implicitly identified with operators");
            }
            if(e->tensor->output) e->carrier=subject(s.carriers,*e->tensor->output);
        } else if(q.kind==NativeAccessionKind::LocalComplex) {
            const auto it=s.blocks.find(q.subject);
            if(it==s.blocks.end())throw NativeSubjectMissing("local accession block absent");
            e->block=it->second;
        } else if(q.kind==NativeAccessionKind::DeclaredValue) {
            e->value_owner=subject(s.values,q.subject);e->value=&e->value_owner->value;
            e->declaration=&s.endpoint->declarations->registry.at(e->value_owner->type);
            for(auto factor:q.factors) {
                require(e->declaration->kind==composite::DeclarationKind::Product,"stable factor path requires product node");
                const auto& fs=e->declaration->factors;
                const auto it=std::find_if(fs.begin(),fs.end(),[&](const auto& f){return f.key==factor;});
                if(it==fs.end())throw NativeSubjectMissing("declared product factor absent");
                const auto index=static_cast<std::size_t>(it-fs.begin());
                e->value=&std::get<composite::DeclaredValue::Product>(e->value->data).elements.at(index);
                e->declaration=&s.endpoint->declarations->registry.at(it->type);
            }
        } else if(q.kind==NativeAccessionKind::SourceAspects) {
            e->layout=subject(s.layouts,q.subject);e->occurrence_slot=s.occurrence(*q.occurrence);
            e->declaration=&s.endpoint->declarations->registry.at(e->layout->type);
        } else if(q.kind==NativeAccessionKind::CodeSection) {
            e->section=subject(s.sections,q.subject);e->code_type=subject(s.code_types,e->section->type);
            if(e->section->source_aspects)e->occurrence_slot=s.occurrence(*e->section->occurrence);
        } else if(q.kind==NativeAccessionKind::CodeOperation) {
            e->operation=subject(s.operations,q.subject);e->code_type=subject(s.code_types,e->operation->output);
            e->input_code_type=subject(s.code_types,e->operation->input);
        }
        else if(q.kind==NativeAccessionKind::Observation) {
            const auto it=s.observations.find({q.subject,q.secondary});
            if(it==s.observations.end())throw NativeSubjectMissing("subscribed code observation absent");
            e->observation=it->second;
        }
        if(e->code_type) {
            const auto it=s.declared_code_types.find(e->code_type->key);
            if(it!=s.declared_code_types.end())e->declaration=it->second;
        }
    } catch(const NativeSubjectMissing&) {
        if(q.missing!=NativeMissingPolicy::Dormant)throw;
        // Partially resolved pointers must never be usable in a dormant entry.
        auto dormant=std::make_shared<Entry>();
        dormant->request=e->request;dormant->bytes=e->bytes;dormant->active=false;e=std::move(dormant);
    }
    return e;
}
} // namespace native_accession_detail

class PinnedNativeCell;
class PinnedNativeOccurrence {
public:
    PinnedNativeOccurrence(const PinnedNativeOccurrence&) = default;
    PinnedNativeOccurrence& operator=(const PinnedNativeOccurrence&) = default;
    std::vector<std::shared_ptr<const TemporalDeclaredValue>> declared_values() const {
        return native_accession_detail::pin_contents<TemporalDeclaredValue>(snapshot_,snapshot_->occurrence_values,key());
    }
    std::vector<std::shared_ptr<const TemporalTensor>> tensors() const {
        return native_accession_detail::pin_contents<TemporalTensor>(snapshot_,snapshot_->occurrence_tensors,key());
    }
    PinnedNativeCell parent_cell() const;
    PinnedNativeCell participant_cell() const;
    StableOccurrenceKey key() const { return {grade_,snapshot_->endpoint->source.occurrence_identity(grade_,slot_)}; }
    StableCellKey parent() const {
        const auto& a=snapshot_->endpoint->source.grade(grade_);
        return stable_cell_key(snapshot_->endpoint->source,{grade_,a.upper_index.at(slot_)});
    }
    StableCellKey participant() const {
        const auto& a=snapshot_->endpoint->source.grade(grade_);
        return stable_cell_key(snapshot_->endpoint->source,{core::Grade{static_cast<std::uint16_t>(grade_.value-1)},a.lower_index.at(slot_)});
    }
    numeric::Rational coefficient() const { return snapshot_->endpoint->source.incidence_coefficient(grade_,slot_); }
    bool aspect(composite::AspectId id) const {
        native_accession_detail::require(id<snapshot_->endpoint->source.signature().aspects().size(),"unknown occurrence aspect");
        return snapshot_->endpoint->source.grade(grade_).aspects.get(id,slot_);
    }
    bool aspect(const std::string& name) const { return aspect(snapshot_->aspects.at(name)); }
    std::size_t aspect_count() const { return snapshot_->endpoint->source.signature().aspects().size(); }
    std::uint64_t sequence() const { return snapshot_->endpoint->sequence; }
private:
    std::shared_ptr<const native_accession_detail::Snapshot> snapshot_;
    core::Grade grade_{};std::size_t slot_{};
    PinnedNativeOccurrence(std::shared_ptr<const native_accession_detail::Snapshot> s,core::Grade g,std::size_t p)
        :snapshot_(std::move(s)),grade_(g),slot_(p) {}
    friend class PinnedNativeCell;friend class PinnedNativeAccession;friend class NativeAccessionDirectory;
};
class PinnedNativeCell {
public:
    PinnedNativeCell(const PinnedNativeCell&) = default;
    PinnedNativeCell& operator=(const PinnedNativeCell&) = default;
    std::vector<std::shared_ptr<const TemporalDeclaredValue>> declared_values() const {
        return native_accession_detail::pin_contents<TemporalDeclaredValue>(snapshot_,snapshot_->cell_values,key());
    }
    std::vector<std::shared_ptr<const TemporalTensor>> tensors() const {
        return native_accession_detail::pin_contents<TemporalTensor>(snapshot_,snapshot_->cell_tensors,key());
    }
    std::vector<std::shared_ptr<const TemporalTensor>> grade_fields() const {
        return native_accession_detail::pin_contents<TemporalTensor>(snapshot_,snapshot_->grade_fields,cell_.grade.value);
    }
    std::vector<std::uint64_t> local_memberships() const {
        const auto it=snapshot_->memberships.find(key());
        if(it==snapshot_->memberships.end())return {};
        native_accession_detail::budget(it->second.size(),snapshot_->limits.max_action_work,"local membership output budget");
        return it->second;
    }
    std::shared_ptr<const core::RelationSignature> source_schema() const {return {snapshot_,&snapshot_->endpoint->source.signature()};}
    StableCellKey key() const { return stable_cell_key(snapshot_->endpoint->source,cell_); }
    std::optional<core::RelationTypeId> type() const {
        if(!cell_.grade.value)return std::nullopt;
        return snapshot_->endpoint->source.grade(cell_.grade).relation_types.at(cell_.index);
    }
    std::size_t occurrence_count() const {
        if(!cell_.grade.value)return 0;
        const auto [first,last]=snapshot_->endpoint->source.grade(cell_.grade).incidence_range(cell_.index);return last-first;
    }
    PinnedNativeOccurrence occurrence(std::size_t offset) const {
        if(!cell_.grade.value)throw std::out_of_range("grade zero has no lower occurrence fiber");
        const auto [first,last]=snapshot_->endpoint->source.grade(cell_.grade).incidence_range(cell_.index);
        if(offset>=last-first)throw std::out_of_range("retained occurrence position");
        return {snapshot_,cell_.grade,first+offset};
    }
    std::uint64_t sequence() const { return snapshot_->endpoint->sequence; }
private:
    std::shared_ptr<const native_accession_detail::Snapshot> snapshot_;core::CellId cell_{};
    PinnedNativeCell(std::shared_ptr<const native_accession_detail::Snapshot> s,core::CellId c)
        :snapshot_(std::move(s)),cell_(c) {}
    friend class PinnedNativeAccession;friend class NativeAccessionDirectory;friend class PinnedNativeOccurrence;
};
inline PinnedNativeCell PinnedNativeOccurrence::parent_cell() const {return {snapshot_,snapshot_->cell(parent())};}
inline PinnedNativeCell PinnedNativeOccurrence::participant_cell() const {return {snapshot_,snapshot_->cell(participant())};}

class PinnedNativeAccession {
public:
    PinnedNativeAccession(const PinnedNativeAccession&) = default;
    PinnedNativeAccession& operator=(const PinnedNativeAccession&) = default;
    NativeAccessionKind kind() const noexcept { return entry_->request.kind; }
    std::uint64_t sequence() const noexcept { return snapshot_->endpoint->sequence; }
    std::uint64_t source_namespace() const noexcept { return snapshot_->binding->name_space; }
    NativeAccessionLimits limits() const noexcept { return snapshot_->limits; }
    NativeAccessionDescriptor descriptor() const { return {snapshot_->binding,entry_->request,entry_->bytes}; }
    std::shared_ptr<const TemporalEndpoint> endpoint() const { return snapshot_->endpoint; }
    // Execution authority is directory-local and survives publication changes.
    // Equal bytes or namespaces in independently opened directories do not merge it.
    bool same_execution_authority(const PinnedNativeAccession& other) const noexcept {
        return snapshot_->authority == other.snapshot_->authority;
    }
    // Resolve another declared view against this pinned publication, never the
    // directory's current generation. No new source copy or implicit rebind.
    PinnedNativeAccession access(const NativeAccessionRequest& request) const {
        native_accession_detail::preflight(request,snapshot_->limits);
        return derived(request);
    }

    std::shared_ptr<const numeric::SparseRationalTensor> tensor() const {
        require(!native_accession_detail::chain_kind(kind()),"selected chains expose state(), not an unselected backing tensor");
        if(entry_->tensor)return {snapshot_,&entry_->tensor->tensor};
        if(entry_->value && std::holds_alternative<numeric::SparseRationalTensor>(entry_->value->data))
            return {snapshot_,&std::get<numeric::SparseRationalTensor>(entry_->value->data)};
        throw std::invalid_argument("accession does not expose a stored rational tensor");
    }
    std::shared_ptr<const TemporalTensor> tensor_declaration() const {
        require(entry_->tensor,"accession has no temporal tensor declaration");return {snapshot_,entry_->tensor};
    }
    std::shared_ptr<const TemporalCarrier> carrier() const {
        require(entry_->carrier,"accession has no field/output carrier");return {snapshot_,entry_->carrier};
    }
    std::shared_ptr<const composite::DeclaredValue> declared_value() const {
        require(entry_->value,"accession has no stored declared value");return {snapshot_,entry_->value};
    }
    std::shared_ptr<const composite::Declaration> declaration() const {
        require(entry_->declaration,"accession has no F1 declaration binding");return {snapshot_,entry_->declaration};
    }
    std::shared_ptr<const TemporalDeclaredValue> declared_owner() const {
        require(entry_->value_owner,"accession has no declared value owner");return {snapshot_,entry_->value_owner};
    }
    std::shared_ptr<const LocalBlockDefinition> local() const {
        require(entry_->block,"accession is not a local complex");return {snapshot_,entry_->block};
    }
    std::shared_ptr<const RelationalCodeType> code_type() const {
        require(entry_->code_type,"accession has no code type/output binding");return {snapshot_,entry_->code_type};
    }
    std::shared_ptr<const RelationalCodeType> input_code_type() const {
        require(entry_->input_code_type,"accession has no code input binding");return {snapshot_,entry_->input_code_type};
    }
    std::shared_ptr<const RelationalCodeSection> code_section() const {
        require(entry_->section,"accession is not a code section");return {snapshot_,entry_->section};
    }
    std::shared_ptr<const RelationalCodeOperation> code_operation() const {
        require(entry_->operation,"accession is not a code operation");return {snapshot_,entry_->operation};
    }
    std::shared_ptr<const RelationalCodeObservation> observation() const {
        require(entry_->observation!=nullptr,"accession is not an observation");return entry_->observation;
    }
    PinnedNativeCell local_cell(std::size_t i) const {
        require(entry_->block,"accession is not a local complex");
        return {snapshot_,snapshot_->cell(entry_->block->cells.at(i))};
    }
    PinnedNativeOccurrence source_occurrence() const {
        if(entry_->section && entry_->section->source_aspects)
            return {snapshot_,entry_->section->occurrence->upper_grade,entry_->occurrence_slot};
        require(entry_->layout,"accession is not a source aspect view");
        return {snapshot_,entry_->request.occurrence->upper_grade,entry_->occurrence_slot};
    }
    bool source_bit(std::uint64_t factor) const {
        require(entry_->layout,"accession is not a source aspect view");
        return source_occurrence().aspect(snapshot_->factors.at(entry_->layout->key).at(factor));
    }
    // Explicit execution, not part of handle resolution. The existing F2
    // source-backed program path retains packed words and active-domain masking.
    machine::PackedBits execute_source_program(std::uint64_t program) const {
        require(entry_->layout,"program requires a source aspect accession");
        const auto* p=native_accession_detail::subject(snapshot_->programs,program);
        require(p->layout==entry_->layout->key,"program has different declared layout");
        return execute_declared_source_program(snapshot_->endpoint->source,*snapshot_->endpoint->declarations,
                                               program,entry_->request.occurrence->upper_grade);
    }
    // Raw coordinates are explicitly in this operator's declared input basis.
    // Use the pinned overload below when a typed field already exists.
    numeric::SparseRationalVector apply_coordinates(const numeric::SparseRationalVector& input) const {
        require(kind()==NativeAccessionKind::Operator,"application requires a declared operator accession");
        const auto& t=entry_->tensor->tensor;const auto& shape=t.domain().extents();
        require(shape.size()==2 && shape[1]==input.size(),"operator input dimension mismatch");
        native_accession_detail::budget(machine::checked_size_add(t.values().nonzeros(),input.nonzeros()),
                                        snapshot_->limits.max_action_work,"operator application work budget");
        std::vector<numeric::SparseRationalEntry> values;
        for(const auto& coefficient:t.values().entries()) {
            const auto row=coefficient.index/shape[1],column=coefficient.index%shape[1];
            const auto& xs=input.entries();
            const auto it=std::lower_bound(xs.begin(),xs.end(),column,[](const auto& x,auto i){return x.index<i;});
            if(it!=xs.end() && it->index==column)values.push_back({row,coefficient.value*it->value});
        }
        return numeric::SparseRationalVector(shape[0],std::move(values));
    }
    numeric::SparseRationalVector apply(const PinnedNativeAccession& input) const {
        require(kind()==NativeAccessionKind::Operator,"application requires an operator accession");
        require(input.kind()==NativeAccessionKind::Field || input.kind()==NativeAccessionKind::Chain,
                "operator pushforward cannot implicitly identify a cochain with a field");
        require(snapshot_->binding==input.snapshot_->binding || *snapshot_->binding==*input.snapshot_->binding,
                "operator input has a different endpoint/declaration context");
        require(input.entry_->carrier && input.entry_->tensor && entry_->tensor->input==input.entry_->carrier->key,
                "operator input carrier identity mismatch");
        const auto count=input.entry_->carrier->domain.logical_size();
        require(input.entry_->tensor->tensor.support().count()==count &&
                (input.kind()!=NativeAccessionKind::Chain || input.support_count()==count),
                "partial input requires an explicitly declared extension");
        return apply_coordinates(input.entry_->tensor->tensor.values());
    }
    std::size_t support_count() const {
        chain_required();const auto& r=entry_->request;
        if(!r.all_cells)return r.cells.size();
        if(entry_->tensor)return entry_->tensor->tensor.support().count();
        return snapshot_->endpoint->source.grade(r.grade).cell_count();
    }
    std::optional<numeric::Rational> state(std::uint64_t stable_cell) const {
        chain_required();const auto& r=entry_->request;
        const auto c=snapshot_->source_index->cell({r.grade,stable_cell});
        if(!c || (!r.all_cells && !std::binary_search(r.cells.begin(),r.cells.end(),stable_cell)))return std::nullopt;
        if(entry_->tensor)return entry_->tensor->tensor.state(c->index);
        const auto it=std::lower_bound(r.coefficients.begin(),r.coefficients.end(),stable_cell,
                                      [](const auto& x,auto key){return x.cell<key;});
        if(it==r.coefficients.end() || it->cell!=stable_cell)return numeric::Rational{0};
        return it->value;
    }
    PinnedNativeCell cell(std::uint64_t stable_cell) const {
        require(state(stable_cell).has_value(),"cell outside accession structural support");
        return {snapshot_,snapshot_->cell({entry_->request.grade,stable_cell})};
    }
    // Enumeration is explicit and bounded; full support is never expanded at
    // registration or lookup. A retained supported zero still visits its cell.
    template<class F> void for_each_cell(F&& f) const {
        chain_required();native_accession_detail::budget(support_count(),snapshot_->limits.max_action_work,"accession enumeration budget");
        const auto& r=entry_->request;
        if(!r.all_cells) { for(auto key:r.cells)f(key);return; }
        const auto visit=[&](std::size_t index) {
            f(stable_cell_key(snapshot_->endpoint->source,{r.grade,static_cast<std::uint32_t>(index)}).key);
        };
        if(entry_->tensor)entry_->tensor->tensor.support().for_each_active(visit);
        else for(std::size_t i=0;i<snapshot_->endpoint->source.grade(r.grade).cell_count();++i)visit(i);
    }
    PinnedNativeAccession boundary() const {
        require(kind()==NativeAccessionKind::Chain,"boundary requires a chain, not a cochain");
        const auto grade=entry_->request.grade;require(grade.value>0,"grade-zero boundary is outside this access path");
        std::map<std::uint64_t,numeric::Rational> values;std::size_t work=0;
        for_each_cell([&](auto key) {
            const auto c=cell(key);const auto value=*state(key);
            work=machine::checked_size_add(work,c.occurrence_count());
            native_accession_detail::budget(work,snapshot_->limits.max_action_work,"boundary occurrence work budget");
            for(std::size_t i=0;i<c.occurrence_count();++i) {
                const auto o=c.occurrence(i);auto& v=values[o.participant().key];
                if(o.aspect(snapshot_->endpoint->source.signature().existence_aspect()))v+=o.coefficient()*value;
            }
        });
        NativeAccessionRequest out=NativeAccessionRequest::sparse_chain(core::Grade{static_cast<std::uint16_t>(grade.value-1)},{},{});
        for(auto& [key,value]:values) {out.cells.push_back(key);if(value!=0)out.coefficients.push_back({key,std::move(value)});}
        return derived(std::move(out));
    }
    PinnedNativeAccession coboundary() const {
        require(kind()==NativeAccessionKind::Cochain,"coboundary requires an algebraic cochain");
        const auto grade=entry_->request.grade;const auto& source=snapshot_->endpoint->source;
        require(static_cast<std::size_t>(grade.value)+1<source.grade_count(),"upper cochain grade absent");
        require(support_count()==source.grade(grade).cell_count(),"coboundary needs a total cochain; no implicit zero extension");
        const auto upper=core::Grade{static_cast<std::uint16_t>(grade.value+1)};const auto& a=source.grade(upper);
        native_accession_detail::budget(machine::checked_size_add(a.cell_count(),a.incidence_count()),
                                        snapshot_->limits.max_action_work,"coboundary action budget");
        auto out=NativeAccessionRequest::cochain(0,upper);out.inline_value=true;
        for(std::size_t i=0;i<a.cell_count();++i) {
            numeric::Rational value=0;const auto [first,last]=a.incidence_range(i);
            for(std::size_t p=first;p<last;++p)if(a.aspects.get(source.signature().existence_aspect(),p)) {
                const auto key=stable_cell_key(source,{grade,a.lower_index[p]}).key;
                value+=source.incidence_coefficient(upper,p)*(*state(key));
            }
            if(value!=0)out.coefficients.push_back({a.relation_identities[i].value,std::move(value)});
        }
        return derived(std::move(out));
    }
    numeric::Rational pair_with(const PinnedNativeAccession& chain) const {
        require(kind()==NativeAccessionKind::Cochain && chain.kind()==NativeAccessionKind::Chain,"pairing requires cochain and chain");
        require(snapshot_->binding==chain.snapshot_->binding || *snapshot_->binding==*chain.snapshot_->binding,
                "pairing requires the same source/endpoint/declaration context");
        require(entry_->request.grade==chain.entry_->request.grade,"pairing grade mismatch");
        numeric::Rational result=0;
        chain.for_each_cell([&](auto key) {
            const auto f=state(key);require(f.has_value(),"partial cochain undefined on chain support");
            result+=*f*(*chain.state(key));
        });
        result.canonicalize();return result;
    }
private:
    std::shared_ptr<const native_accession_detail::Snapshot> snapshot_;
    std::shared_ptr<const native_accession_detail::Entry> entry_;
    static void require(bool b,const char* m) { native_accession_detail::require(b,m); }
    void chain_required() const { require(native_accession_detail::chain_kind(kind()),"operation requires chain/cochain accession"); }
    PinnedNativeAccession(std::shared_ptr<const native_accession_detail::Snapshot> s,
                         std::shared_ptr<const native_accession_detail::Entry> e):snapshot_(std::move(s)),entry_(std::move(e)) {
        if(!entry_->active)throw NativeSubjectMissing("accession subscription is dormant");
    }
    PinnedNativeAccession derived(NativeAccessionRequest r) const {
        native_accession_detail::canonicalize(r,snapshot_->limits);
        auto bytes=native_accession_detail::request_bytes(r,snapshot_->limits);
        auto e=native_accession_detail::bind(*snapshot_,std::move(r),std::move(bytes));return {snapshot_,std::move(e)};
    }
    friend class NativeAccessionDirectory;
};
} // namespace rfc::storage
