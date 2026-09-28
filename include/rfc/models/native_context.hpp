#pragma once

#include "rfc/storage/native_atlas.hpp"

namespace rfc::models {
// Requests are shared by database consumers and prospective models. Authority
// is an explicit host decision; neither a hash match nor gluing grants access.
struct NativeContextSelection {
    std::uint64_t source{};
    storage::NativeAccessionRequest accession;
};
struct NativeContextAuthorization {
    std::uint64_t source{}, source_namespace{}, sequence{};
    storage::NativeAccessionRequest accession; // canonical, complete requested view
};
using NativeContextAuthorizer=std::function<bool(const NativeContextAuthorization&)>;
struct NativeContextLimits {
    std::size_t max_items{128}, max_elements{100000}, max_bytes{16u<<20}, max_proposal_bytes{1u<<20};
    composite::DeclarationLimits declarations{};
    storage::NativeAccessionLimits accession{};
};
enum class NativeContextProvenance : std::uint8_t { ExactStored, ExactDerived };
struct NativeChainContext {
    core::Grade grade;
    storage::NativePairing pairing;
    std::vector<storage::NativeCoefficient> components; // includes supported zeros
};
struct NativeTensorContext {
    storage::TemporalTensorRole role;
    std::optional<std::uint64_t> input, output;
    storage::TemporalTensorOwner owner;
    numeric::SparseRationalTensor tensor;
};
struct NativeDeclaredContext {
    composite::DeclarationId type;
    composite::DeclarationRegistry declarations;
    composite::DeclaredValue value;
};
struct NativeAspectContext {
    composite::DeclarationId type;
    storage::StableOccurrenceKey occurrence;
    std::vector<std::pair<std::uint64_t,bool>> factors;
};
struct NativeObservationContext {
    std::vector<std::byte> canonical;
    std::optional<bool> compatible;
};
struct NativeContextItem {
    NativeContextAuthorization origin;
    NativeContextProvenance provenance;
    std::variant<NativeChainContext,NativeTensorContext,NativeDeclaredContext,NativeAspectContext,NativeObservationContext> contents;
};
enum class ProspectiveEncoding : std::uint8_t { ExactCandidate, Approximate };

class NativeConsumerContext;
// A separate type with no conversion to TemporalEndpoint/segment/exact assertion.
// ExactCandidate describes representation, NOT certification of a prediction.
class NativeModelProposal {
public:
    ProspectiveEncoding encoding() const noexcept{return encoding_;}
    const std::string& model_identity() const noexcept{return model_;}
    const std::string& interpretation() const noexcept{return interpretation_;}
    std::span<const std::byte> bytes() const noexcept{return bytes_;}
    const std::vector<NativeContextAuthorization>& provenance() const noexcept{return provenance_;}
private:
    ProspectiveEncoding encoding_;
    std::string model_,interpretation_;
    std::vector<std::byte> bytes_;
    std::vector<NativeContextAuthorization> provenance_;
    NativeModelProposal(ProspectiveEncoding e,std::string m,std::string i,std::vector<std::byte> b,std::vector<NativeContextAuthorization> p)
        :encoding_(e),model_(std::move(m)),interpretation_(std::move(i)),bytes_(std::move(b)),provenance_(std::move(p)){}
    friend class NativeConsumerContext;
};

namespace native_context_detail {
using storage::temporal_source_detail::require;
inline void add(std::size_t& n,std::size_t count,std::size_t cap,const char* message) {
    n=machine::checked_size_add(n,count);if(n>cap)throw std::length_error(message);
}
inline composite::DeclarationRegistry closure(const composite::DeclarationRegistry& registry,composite::DeclarationId root,
                                               const composite::DeclarationLimits& limits) {
    std::set<composite::DeclarationId> seen;std::vector<composite::DeclarationId> pending{root};std::vector<composite::Declaration> definitions;
    while(!pending.empty()) {
        const auto id=pending.back();pending.pop_back();if(!seen.insert(id).second)continue;
        require(seen.size()<=limits.max_definitions,"context declaration count");const auto& d=registry.at(id);
        definitions.push_back(d);for(const auto& f:d.factors)pending.push_back(f.type);if(d.element.valid())pending.push_back(d.element);
        require(pending.size()<=limits.max_components,"context declaration reference budget");
    }
    return composite::DeclarationRegistry(std::move(definitions),limits);
}
inline std::size_t rational_bytes(const numeric::Rational& value,const NativeContextLimits& limits) {
    return machine::ceil_div(composite::declaration_detail::rational_bits(value,limits.declarations),8);
}
} // namespace native_context_detail

// The published result exposes ONLY authorized, bounded contents. In particular
// it does not return F3 pins: those deliberately expose endpoint() to trusted
// native callers and would let a model bypass a per-accession read restriction.
class NativeConsumerContext {
public:
    static NativeConsumerContext build(const storage::NativeAtlas& atlas,
                                        const std::vector<NativeContextSelection>& selections,
                                        const NativeContextAuthorizer& authorize,
                                        const NativeContextLimits limits={}) {
        using namespace native_context_detail;
        require(static_cast<bool>(authorize),"context access denies by default");limits.declarations.validate();
        require(selections.size()<=limits.max_items,"context item budget");
        // Pin the publication and copy/canonicalize requests before invoking host
        // callbacks. A callback may replace its atlas or request vector alias;
        // that cannot create a mixed-publication result or invalidate iteration.
        const auto publication=atlas;const auto policy=authorize;
        std::vector<NativeContextSelection> prepared;prepared.reserve(selections.size());
        std::size_t request_bytes=0;
        for(const auto& selected:selections) {
            auto request=selected.accession;
            storage::native_accession_detail::canonicalize(request,limits.accession);
            add(request_bytes,storage::native_accession_detail::request_bytes(request,limits.accession).size(),limits.max_bytes,"context aggregate request budget");
            prepared.push_back({selected.source,std::move(request)});
        }
        auto candidate=std::make_shared<State>();candidate->limits=limits;
        std::size_t elements=0,bytes=0;auto accession_limits=limits.accession;
        accession_limits.max_selection_cells=std::min(accession_limits.max_selection_cells,limits.max_elements);
        accession_limits.max_descriptor_bytes=std::min(accession_limits.max_descriptor_bytes,limits.max_bytes);
        // Atlas source publications are immutable. Every grant is bound to the
        // exact source/sequence/request; shared identities never transfer grants.
        for(const auto& selection:prepared) {
            auto request=selection.accession;storage::native_accession_detail::canonicalize(request,accession_limits);
            const auto source=publication.source(selection.source);
            NativeContextAuthorization origin{source.key(),source.name_space(),source.sequence(),request};
            require(policy(origin),"native context access denied");
            require(!request.inline_value,"request literals are not exact stored context");
            require(request.missing==storage::NativeMissingPolicy::Reject,"consumer contexts require present subjects");
            const auto view=source.access(request);
            const auto descriptor=storage::native_accession_detail::request_bytes(request,accession_limits);
            add(bytes,descriptor.size(),limits.max_bytes,"context aggregate descriptor budget");
            const auto kind=request.kind;
            if(kind==storage::NativeAccessionKind::Chain || kind==storage::NativeAccessionKind::Cochain) {
                add(elements,view.support_count(),limits.max_elements,"context component budget");
                add(bytes,machine::checked_size_mul(view.support_count(),sizeof(storage::NativeCoefficient)),limits.max_bytes,"context chain buffer budget");
                NativeChainContext chain{request.grade,request.pairing,{}};chain.components.reserve(view.support_count());
                view.for_each_cell([&](auto key){const auto value=*view.state(key);add(bytes,rational_bytes(value,limits),limits.max_bytes,"context coefficient byte budget");chain.components.push_back({key,value});});
                candidate->items.push_back({std::move(origin),NativeContextProvenance::ExactStored,std::move(chain)});
            } else if(kind==storage::NativeAccessionKind::Tensor || kind==storage::NativeAccessionKind::Field || kind==storage::NativeAccessionKind::Operator) {
                const auto t=view.tensor_declaration();
                add(elements,t->tensor.support().count(),limits.max_elements,"context tensor support budget");
                add(bytes,machine::checked_size_mul(t->tensor.values().nonzeros(),sizeof(numeric::SparseRationalEntry)),limits.max_bytes,"context tensor buffer budget");
                add(bytes,machine::checked_size_mul(t->tensor.domain().rank(),sizeof(std::size_t)),limits.max_bytes,"context tensor shape budget");
                if(const auto* packed=t->tensor.support().packed_bits())add(bytes,machine::checked_size_mul(packed->words().size(),sizeof(std::uint64_t)),limits.max_bytes,"context packed support buffer budget");
                else if(const auto* sparse=t->tensor.support().sparse_indices())add(bytes,machine::checked_size_mul(sparse->size(),sizeof(std::size_t)),limits.max_bytes,"context support buffer budget");
                for(const auto& entry:t->tensor.values().entries())add(bytes,rational_bytes(entry.value,limits),limits.max_bytes,"context tensor coefficient budget");
                candidate->items.push_back({std::move(origin),NativeContextProvenance::ExactStored,NativeTensorContext{t->role,t->input,t->output,t->owner,t->tensor}});
            } else if(kind==storage::NativeAccessionKind::DeclaredValue) {
                auto declarations=closure(view.endpoint()->declarations->registry,view.declaration()->id,limits.declarations);
                auto value_limits=limits.declarations;value_limits.max_materialized_bytes=std::min(value_limits.max_materialized_bytes,limits.max_bytes-bytes);
                value_limits.max_value_nodes=std::min(value_limits.max_value_nodes,limits.max_elements-elements);
                const composite::CompiledLayout layout(declarations,view.declaration()->id,composite::BooleanLayoutProfile::Lsb0OctetsV1,value_limits);
                const auto use=layout.allocation(*view.declared_value());
                add(elements,use.value_nodes,limits.max_elements,"context declared value nodes");add(bytes,use.materialized_bytes,limits.max_bytes,"context declared value bytes");
                // Materialize the canonical support representation rather than copying
                // an arbitrarily oversized packed backing store from the source.
                const auto schema=storage::encode_declarations(layout);
                add(bytes,schema.size(),limits.max_bytes,"context declaration bytes");
                candidate->items.push_back({std::move(origin),NativeContextProvenance::ExactStored,NativeDeclaredContext{view.declaration()->id,std::move(declarations),storage::decode_declared_bits(layout,storage::encode_declared_bits(layout,*view.declared_value()))}});
            } else if(kind==storage::NativeAccessionKind::SourceAspects) {
                const auto declaration=view.declaration();
                add(elements,declaration->factors.size(),limits.max_elements,"context aspect count");
                add(bytes,machine::checked_size_mul(declaration->factors.size(),sizeof(std::pair<std::uint64_t,bool>)),limits.max_bytes,"context aspect buffer budget");
                NativeAspectContext aspects{declaration->id,*request.occurrence,{}};aspects.factors.reserve(declaration->factors.size());
                for(const auto& f:declaration->factors)aspects.factors.emplace_back(f.key,view.source_bit(f.key));
                candidate->items.push_back({std::move(origin),NativeContextProvenance::ExactStored,std::move(aspects)});
            } else if(kind==storage::NativeAccessionKind::Observation) {
                const auto observation=view.observation();add(elements,1,limits.max_elements,"context observation count");
                add(bytes,observation->bytes.size(),limits.max_bytes,"context observation bytes");
                candidate->items.push_back({std::move(origin),NativeContextProvenance::ExactDerived,NativeObservationContext{observation->bytes,observation->compatible}});
            } else throw std::invalid_argument("accession kind has no bounded consumer projection");
        }
        candidate->accounted_bytes=bytes;candidate->elements=elements;return NativeConsumerContext(std::move(candidate));
    }
    const std::vector<NativeContextItem>& items() const noexcept{return state_->items;}
    std::size_t accounted_bytes() const noexcept{return state_->accounted_bytes;}
    std::size_t elements() const noexcept{return state_->elements;}
    NativeModelProposal propose(ProspectiveEncoding encoding,std::string model,std::string interpretation,std::vector<std::byte> bytes) const {
        using namespace native_context_detail;
        require(encoding==ProspectiveEncoding::ExactCandidate || encoding==ProspectiveEncoding::Approximate,"unknown prospective encoding");
        require(!model.empty() && !interpretation.empty() && model.size()<=4096 && interpretation.size()<=4096,"model output needs bounded provenance/interpretation");
        require(bytes.size()<=state_->limits.max_proposal_bytes,"model proposal byte budget");
        std::vector<NativeContextAuthorization> origins;origins.reserve(state_->items.size());for(const auto& item:state_->items)origins.push_back(item.origin);
        return NativeModelProposal(encoding,std::move(model),std::move(interpretation),std::move(bytes),std::move(origins));
    }
private:
    struct State{NativeContextLimits limits;std::vector<NativeContextItem> items;std::size_t accounted_bytes{},elements{};};
    std::shared_ptr<const State> state_;
    explicit NativeConsumerContext(std::shared_ptr<const State> state):state_(std::move(state)){}
};
} // namespace rfc::models
